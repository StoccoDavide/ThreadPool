/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * Copyright (c) 2025, Davide Stocco.                                                            *
 *                                                                                               *
 * The ThreadPool project is distributed under the MIT License.                                  *
 *                                                                                               *
 * Davide Stocco                   University of Trento                   davide.stocco@unitn.it *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef THREADPOOL_PARALLELFOR_HH
#define THREADPOOL_PARALLELFOR_HH

// Standard library includes
#include "ThreadPool.hh"

namespace ThreadPool {

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

#endif // THREADPOOL_PARALLELFOR_HH
