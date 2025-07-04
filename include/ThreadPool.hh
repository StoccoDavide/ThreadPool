/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * Copyright (c) 2025, Davide Stocco.                                                            *
 *                                                                                               *
 * The ThreadPool project is distributed under the MIT License.                                  *
 *                                                                                               *
 * Davide Stocco                   University of Trento                   davide.stocco@unitn.it *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef THREADPOOL_THREADPOOL_HH
#define THREADPOOL_THREADPOOL_HH

// Standard library includes
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <condition_variable>
#include <future>
#include <mutex>
#include <vector>
#include <queue>
#include <stdexcept>
#include <cmath>
#include <functional>
#include <thread>
#include <numeric>

// Print ThreadPool errors
#ifndef THREADPOOL_ERROR
#define THREADPOOL_ERROR(MSG)           \
  {                                     \
    std::ostringstream os;              \
    os << MSG;                          \
    throw std::runtime_error(os.str()); \
  }
#endif

// Assert for ThreadPool
#ifndef THREADPOOL_ASSERT
#define THREADPOOL_ASSERT(COND, MSG) \
  if (!(COND))                       \
  {                                  \
    THREADPOOL_ERROR(MSG);           \
  }
#endif

// Default integer type
#ifndef THREADPOOL_DEFAULT_INTEGER_TYPE
#define THREADPOOL_DEFAULT_INTEGER_TYPE int
#endif

/**
 * \brief ThreadPool namespace.
 *
 * This namespace contains the ThreadPool class and related functionality for managing parallel
 * execution of tasks.
 */
namespace ThreadPool {

  /**
  * \brief The Integer type as used for the API.
  *
  * The Integer type, \c \#define the preprocessor symbol \c THREADPOOL_DEFAULT_INTEGER_TYPE. The
  * vdefault alue is \c int.
  */
  using Integer = THREADPOOL_DEFAULT_INTEGER_TYPE;

  /**
   * \brief Option base class for parallel algorithms.
   *
   * This class allows users to specify the number of threads to be used in parallel algorithms.
   */
  class Options {
    private:
      Integer m_nthreads; /**< The number of threads to be used in parallel algorithms. */

    public:

    /** \brief Constants for specifying the number of threads.
    *
    * These constants can be used to specify the number of threads in parallel algorithms.
    */
    enum : Integer {
      NONE = 0,  /**< Disable multi-threading, executing tasks sequentially. */
      AUTO = -1, /**< Automatically determine the number of threads based on the system's hardware concurrency. */
      NICE = -2, /**< Use half as many threads as <tt>AUTO</tt> would. */
    };

    /** \brief Default constructor.
     *
     * Initializes the Options object with the default number of threads, which is determined by
     * <tt>actual_nthreads(AUTO)</tt>.
     */
    Options() : m_nthreads(actual_nthreads(AUTO)) {}

    /** \brief Helper function to compute the actual number of threads.
     *
     * This function interprets the user-specified number of threads and returns the actual number
     * of threads to be used. If the preprocessor flag <tt>THREADPOOL_SINGLE_THREADED</tt> is defined,
     * it always returns 0, indicating that multi-threading is disabled.
     * \param[in] user_num_threads The user-specified number of threads.
     * \return The actual number of threads to be used.
     */
    static Integer actual_nthreads(const Integer user_num_threads)
    {
      #ifdef THREADPOOL_SINGLE_THREADED
        return 0;
      #else
        return user_num_threads >= 0
                ? user_num_threads
                : user_num_threads == NICE
                  ? std::thread::hardware_concurrency() / 2
                  : std::thread::hardware_concurrency();
      #endif
    }

    /** \brief Set the number of threads or one of the constants <tt>AUTO</tt>, <tt>NICE</tt> and <tt>NONE</tt>.
     *
     * \note This setting is ignored if the preprocessor flag <tt>THREADPOOL_SINGLE_THREADED</tt> is
     * defined. Then, the number of threads is set to 0 and all tasks revert to sequential algorithm
     * implementations. The same can be achieved at runtime by passing <tt>n = 0</tt>. In contrast,
     * passing <tt>n = 1</tt> causes the parallel algorithm versions to be executed with a single
     * thread. Both possibilities are mainly useful for debugging.
     * \param[in] n The desired number of threads.
     * \return Reference to the Options object, allowing for method chaining.
     */
    Options & nthreads(const Integer n = AUTO)
    {
      this->m_nthreads = actual_nthreads(n);
      return *this;
    }

    /** \brief Get desired number of threads.
     *
     * \note This function may return 0, which means that multi-threading shall be switched off
     * entirely. If an algorithm receives this value, it should revert to a sequential implementation.
     * In contrast, if <tt>get_nthreads() == 1</tt>, the parallel algorithm version shall be executed
     * with a single thread.
     * \return The desired number of threads.
     */
    Integer get_nthreads() const {return this->m_nthreads;}

    /** \brief Get desired number of threads.
    * In contrast to <tt>get_nthreads()</tt>, this will always return a value <tt>>= 1</tt>.
    */
    Integer get_actual_nthreads() const {return std::max(1, m_nthreads);}

  }; // class Options

  /**
  * \brief Thread pool class to manage a set of parallel workers.
  */
  class Manager
  {
  private:
    std::vector<std::thread> m_workers; /**< Vector of worker threads. */
    std::queue<std::function<void(Integer)>> m_tasks; /** Queue of tasks to be executed by the workers. */

    // Synchronization primitives
    std::mutex m_queue_mutex; /** Mutex to protect access to the task queue. */
    std::condition_variable m_worker_condition; /** Condition variable to notify workers of new tasks. */
    std::condition_variable m_finish_condition; /** Condition variable to notify when all tasks are finished. */
    std::atomic_long m_busy; /** Atomic counter for the number of busy workers. */
    std::atomic_long m_processed; /** Atomic counter for the number of processed tasks. */
    bool m_stop; /** Flag to indicate whether the thread pool is stopping. */

    /**
     * \brief Initialize the thread pool with the specified options.
     * \param[in] options The options for the parallel execution.
     */
    inline void init(const Options & options)
    {
      this->m_busy.store(0);
      this->m_processed.store(0);

      const size_t actual_nthreads = options.get_nthreads();
      for(size_t i{0}; i < actual_nthreads; ++i)
      {
        this->m_workers.emplace_back(
          [i, this] {
            for(;;)
            {
              std::function<void(Integer)> task;
              {
                std::unique_lock<std::mutex> lock(this->m_queue_mutex);

                // will wait if : stop == false  AND queue is empty
                // if stop == true AND queue is empty thread function will return later
                //
                // so the idea of this wait, is : If where are not in the destructor
                // (which sets stop to true, we wait here for new jobs)
                this->m_worker_condition.wait(lock, [this] {return this->m_stop || !this->m_tasks.empty();});
                if (!this->m_tasks.empty())
                {
                  ++this->m_busy;
                  task = std::move(this->m_tasks.front());
                  this->m_tasks.pop();
                  lock.unlock();
                  task(i);
                  ++this->m_processed;
                  --this->m_busy;
                  this->m_finish_condition.notify_one();
                }
                else if (this->m_stop)
                {
                  return;
                }
              }
            }
          }
        );
      }
  }

  public:
    /**
     * \brief Create a thread pool from Options.
     *
     * Class constructor to launch the desired number of workers. If the number of threads is zero,
     * no workers are started, and all tasks will be executed in synchronously in the present thread.
     * \param[in] options The options for the parallel execution.
     */
    Manager(const Options & options) : m_stop(false) {this->init(options);}

    /**
     * \brief Create a thread pool with <tt>n<\tt> threads.
     * \param[in] n The number of threads to be used in parallel algorithms.
     * \note If <tt>n<\tt> is <tt>Options::AUTO</tt>, the number of threads is determined by
     * <tt>std::thread::hardware_concurrency()</tt>. <tt>Options::NICE</tt> will create half as many
     * threads. If <tt>n = 0</tt>, no workers are started, and all tasks will be executed synchronously
     * in the present thread. If the preprocessor flag <tt>THREADPOOL_SINGLE_THREADED</tt> is defined,
     * the number of threads is always set to zero (i.e. synchronous execution), regardless of the
     * value of <tt>n<\tt>. This is useful for debugging.
     */
    Manager(const Integer n) : m_stop(false) {this->init(Options().nthreads(n));}

    /**
     * \brief The destructor joins all threads.
     */
    inline ~Manager() {
      {
        std::unique_lock<std::mutex> lock(this->m_queue_mutex);
        this->m_stop = true;
      }
      this->m_worker_condition.notify_all();
      for(std::thread & worker: this->m_workers) {worker.join();}
    }

    /**
     * \brief Enqueue a task that will be executed by the thread pool.
     *
     * The task result can be obtained using the get() function of the returned future.
     * If the task throws an exception, it will be raised on the call to get().
     * \param[in] function The function to be executed by the worker threads.
     * \return A future that will hold the result of the task.
     */
    template<class Function>
    inline auto enqueue_with_return(Function && function) -> std::future<decltype(function(0))>
    {
      #define CMD "ThreadPool::enqueue_with_return(...): "

      typedef decltype(function(0)) result_type;
      typedef std::packaged_task<result_type(Integer)> PackageType;
      auto task = std::make_shared<PackageType>(function);
      auto res = task->get_future();
      if (this->m_workers.size() > 0){
        {
          std::unique_lock<std::mutex> lock(this->m_queue_mutex);
          THREADPOOL_ASSERT(!this->m_stop, CMD "enqueue on stopped thread pool.");
          this->m_tasks.emplace([task] (Integer task_id) {(*task)(std::move(task_id));});
        }
        this->m_worker_condition.notify_one();
      } else {
        (*task)(0);
      }
      return res;

      #undef CMD
    }

    /**
     * \brief Enqueue function for tasks without return value.
     *
     * This is a special case of the <tt>enqueue_with_return</tt> template function, but some compilers fail on
     * <tt>std::result_of<Function(int)>::type</tt> for void(int) functions.
     * \param[in] function The function to be executed by the worker threads.
     * \return A future that will hold the result of the task, which is void in this case.
     */
    template<class Function>
    inline std::future<void> enqueue(Function && function)
    {
      #define CMD "ThreadPool::enqueue(...): "

      typedef std::packaged_task<void(Integer)> PackageType;
      auto task = std::make_shared<PackageType>(function);
      auto res = task->get_future();
      if (this->m_workers.size() > 0){
        {
          std::unique_lock<std::mutex> lock(this->m_queue_mutex);
          THREADPOOL_ASSERT(!this->m_stop, CMD "enqueue on stopped thread pool.");
          this->m_tasks.emplace([task] (Integer tid) {(*task)(std::move(tid));});
        }
        this->m_worker_condition.notify_one();
      }
      else{
        (*task)(0);
      }
      return res;

      #undef CMD
    }

    /**
     * \brief Block until all tasks are finished.
     */
    void wait_finished()
    {
      std::unique_lock<std::mutex> lock(this->m_queue_mutex);
      this->m_finish_condition.wait(lock, [this] () {return this->m_tasks.empty() && (this->m_busy == 0);});
    }

    /**
     * \brief Return the number of worker threads.
     */
    size_t nthreads() const {return this->m_workers.size();}

  }; // class Manager

  /**
   * \brief Apply a functor to all items in a range in parallel.
   * \param[in] pool The thread pool to use for parallel execution.
   * \param[in] nitems The number of items in the range.
   * \param[in] iter The beginning of the range.
   * \param[in] end The end of the range.
   * \param[in] function The functor to apply to each item in the range.
   * \tparam Iterator The type of the iterator used to traverse the range.
   * \tparam Function The type of the functor to apply.
   * \note If <tt>nitems = 0</tt>, it will be computed using <tt>std::distance(iter, end)</tt>.
   * \note The redundancy of nitems and iter,end here is due to the fact that, for forward iterators,
   * computing the distance from iterators is costly, and, for input iterators, we might not know in
   * advance how many items there are  (e.g., stream iterators).
  */
  template<class Iterator, class Function>
  inline void parallel_foreach_impl(Manager & pool, const std::ptrdiff_t, Iterator iter, Iterator end,
    Function && function, std::random_access_iterator_tag)
  {
    std::ptrdiff_t workload = std::distance(iter, end);
    assert(workload == nitems || nitems == 0);
    const float workPerThread = float(workload) / pool.nthreads();
    const std::ptrdiff_t chunkedWorkPerThread = std::max<std::ptrdiff_t>(std::llround(workPerThread/3.0), 1);

    std::vector<std::future<void>> futures;
    for( ;iter<end; iter+=chunkedWorkPerThread)
    {
      const size_t lc = std::min(workload, chunkedWorkPerThread);
      workload -= lc;
      futures.emplace_back(
        pool.enqueue(
          [&function, iter, lc] (Integer id) {
            for(size_t i{0}; i<lc; ++i) {function(id, iter[i]);}
          }
        )
      );
    }
    for (auto & future : futures) {future.get();}
  }

  /**
  * \brief Apply a functor to all items in a range in parallel.
  * \param[in] pool The thread pool to use for parallel execution.
  * \param[in] nitems The number of items in the range.
  * \param[in] iter The beginning of the range.
  * \param[in] end The end of the range.
  * \param[in] function The functor to apply to each item in the range.
  * \tparam Iterator The type of the iterator used to traverse the range.
  * \tparam Function The type of the functor to apply.
   */
  template<class Iterator, class Function>
  inline void parallel_foreach_impl(Manager & pool, const std::ptrdiff_t nitems, Iterator iter,
    Iterator end, Function && function, std::forward_iterator_tag)
  {
      if (nitems == 0) {nitems = std::distance(iter, end);}

      std::ptrdiff_t workload = nitems;
      const float workPerThread = float(workload)/pool.nthreads();
      const std::ptrdiff_t chunkedWorkPerThread = std::max<std::ptrdiff_t>(std::llround(workPerThread/3.0), 1);

      std::vector<std::future<void>> futures;
      for(;;)
      {
        const size_t lc = std::min(chunkedWorkPerThread, workload);
        workload -= lc;
        futures.emplace_back(
            pool.enqueue(
              [&function, iter, lc] (Integer id)
              {
                auto iterCopy = iter;
                for(size_t i{0}; i<lc; ++i){
                    function(id, *iterCopy);
                    ++iterCopy;
                }
              }
            )
        );
        for (size_t i{0}; i < lc; ++i)
        {
          ++iter;
          if (iter == end)
          {
            assert(workload == 0);
            break;
          }
        }
        if (workload == 0) {break;}
      }
      for (auto & future : futures) {future.get();}
  }

  /**
   * \brief Apply a functor to all items in a range in parallel.
   * \param[in] pool The thread pool to use for parallel execution.
   * \param[in] nitems The number of items in the range.
   * \param[in] iter The beginning of the range.
   * \param[in] end The end of the range.
   * \param[in] function The functor to apply to each item in the range.
   * \tparam Iteration The type of the iterator used to traverse the range.
   * \tparam Function The type of the functor to apply.
   * \note If <tt>nitems = 0</tt>, it will be computed using <tt>std::distance(iter, end)</tt>.
   */
  template<class Iteration, class Function>
  inline void parallel_foreach_impl(Manager & pool, [[maybe_unused]] std::ptrdiff_t nitems,
      Iteration iter, Iteration end, Function && function, std::input_iterator_tag)
    {
    [[maybe_unused]] std::ptrdiff_t num_items = 0;
    std::vector<std::future<void>> futures;
    for (; iter != end; ++iter)
    {
      auto item = *iter;
      futures.emplace_back(pool.enqueue([&function, &item](Integer id){function(id, item);}));
      ++num_items;
    }
    assert(num_items == nitems || nitems == 0);
    for (auto & future : futures) {future.get();}
  }

  /**
   * \brief Apply a functor to all items in a range in parallel using a single thread.
   * \param[in] begin The beginning of the range.
   * \param[in] end The end of the range.
   * \param[in] function The functor to apply to each item in the range.
   * \param[in] nitems The number of items in the range.
   * \tparam Function The type of the functor to apply.
   * \tparam Iterator The type of the iterator used to traverse the range.
   * \note If <tt>nitems = 0</tt>, it will be computed using <tt>std::distance(begin, end)</tt>.
   * \note This function is used when the number of threads is 0, meaning that the parallel execution
   * is not enabled, and the function will be executed sequentially in the current thread.
   */
  template<class Iterator, class Function>
  inline void parallel_foreach_single_thread(Iterator begin, Iterator end, Function && function,
    [[maybe_unused]] const std::ptrdiff_t nitems = 0)
  {
    [[maybe_unused]] std::ptrdiff_t n = 0;
    for (; begin != end; ++begin)
    {
      function(0, *begin);
      ++n;
    }
    assert(n == nitems || nitems == 0);
  }

  /**
   * \brief Apply a functor to all items in a range in parallel.
   * \param[in] pool The thread pool to use for parallel execution.
   * \param[in] begin The beginning of the range.
   * \param[in] end The end of the range.
   * \param[in] function The functor to apply to each item in the range.
   * \param[in] nitems The number of items in the range.
   * \tparam Function The type of the functor to apply.
   * \tparam Iterator The type of the iterator used to traverse the range.
   * \note If <tt>nitems = 0</tt>, it will be computed using <tt>std::distance(begin, end)</tt>.
   */
  template<class Iterator, class Function>
  inline void parallel_foreach(Manager & pool, Iterator begin, Iterator end, Function && function,
    const std::ptrdiff_t nitems = 0)
  {
    if (pool.nthreads() > 1) {
      parallel_foreach_impl(pool, nitems, begin, end, function,
        typename std::iterator_traits<Iterator>::iterator_category());
    } else {
      parallel_foreach_single_thread(begin, end, function, nitems);
    }
  }

  /**
   * \brief Apply a functor to all items in a range in parallel using a specified number of threads.
   * \param[in] nthreads The number of threads to use for parallel execution.
   * \param[in] begin The beginning of the range.
   * \param[in] end The end of the range.
   * \param[in] function The functor to apply to each item in the range.
   * \param[in] nitems The number of items in the range.
   * \tparam Function The type of the functor to apply.
   * \tparam Iterator The type of the iterator used to traverse the range.
   * \note If <tt>nitems = 0</tt>, it will be computed using <tt>std::distance(begin, end)</tt>.
   */
  template<class Iterator, class Function>
  inline void parallel_foreach(int64_t nthreads, Iterator begin, Iterator end, Function && function,
    const std::ptrdiff_t nitems = 0)
  {
    Manager pool(nthreads);
    parallel_foreach(pool, begin, end, function, nitems);
  }

  /**
   * \brief Apply a functor to a range of integers in parallel.
   * \param[in] nthreads The number of threads to use for parallel execution.
   * \param[in] nitems The number of items in the range from <tt>0</tt> to <tt>nitems-1</tt>.
   * \param[in] function The functor to apply to each integer in the range.
   * \tparam Function The type of the functor to apply.
   */
  template<class Function>
  inline void parallel_foreach(int64_t nthreads, std::ptrdiff_t nitems, Function && function)
  {
    std::vector<std::ptrdiff_t> range;
    range.resize(nitems);
    std::iota(range.begin(), range.end(), std::ptrdiff_t(0));
    parallel_foreach(nthreads, range.begin(), range.end(), function, nitems);
  }

  /**
   * \brief Apply a functor to a range of integers in parallel using an existing thread pool.
   * \param[in] threadpool The thread pool to use for parallel execution.
   * \param[in] nitems The number of items in the range from <tt>0</tt> to <tt>nitems-1</tt>.
   * \param[in] function The functor to apply to each integer in the range.
   * \tparam Function The type of the functor to apply.
   */
  template<class Function>
  inline void parallel_foreach(Manager & threadpool, std::ptrdiff_t nitems, Function && function)
  {
    std::vector<std::ptrdiff_t> range;
    range.resize(nitems);
    std::iota(range.begin(), range.end(), std::ptrdiff_t(0));
    parallel_foreach(threadpool, range.begin(), range.end(), function, nitems);
  }

} // namespace ThreadPool

#endif // THREADPOOL_THREADPOOL_HH
