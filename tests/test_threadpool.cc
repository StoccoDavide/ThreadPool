/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * Copyright (c) 2025, Davide Stocco.                                                            *
 *                                                                                               *
 * The ThreadPool project is distributed under the MIT License.                                  *
 *                                                                                               *
 * Davide Stocco                   University of Trento                   davide.stocco@unitn.it *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Standard library includes
#include <future>

// Google Test includes
#include <gtest/gtest.h>

// The ThreadPool includes
#include "ThreadPool.hh"

using namespace ThreadPool;

// Dummy shouldEqual and shouldEqualSequence for demonstration
template<typename It1, typename It2>
void shouldEqualSequence(It1 begin1, It1 end1, It2 begin2) {
  for (; begin1 != end1; ++begin1, ++begin2) {
    ASSERT_EQ(*begin1, *begin2);
  }
}

// Replace with your actual parallel_foreach and ThreadPool includes/definitions

TEST(ThreadPoolTest, ThreadPoolBasic) {
  size_t const n = 10000;
  std::vector<int> v(n);
  Manager pool(4);
  for (size_t i = 0; i < v.size(); ++i) {
    pool.enqueue(
      [&v, i](size_t /*thread_id*/) {
        v[i] = 0;
        for (size_t k = 0; k < i+1; ++k) {
          v[i] += k;
        }
      }
    );
  }
  pool.wait_finished();

  std::vector<int> v_expected(n);
  for (size_t i = 0; i < v_expected.size(); ++i)
    v_expected[i] = i*(i+1)/2;

  shouldEqualSequence(v.begin(), v.end(), v_expected.begin());
}

TEST(ThreadPoolTest, ThreadPoolException) {
  bool caught = false;
  std::string exception_string = "the test exception";
  std::vector<int> v(10000);
  Manager pool(4);
  std::vector<std::future<void> > futures;
  for (size_t i = 0; i < v.size(); ++i) {
    futures.emplace_back(
      pool.enqueue(
        [&v, &exception_string, i](size_t /*thread_id*/) {
          v[i] = 1;
          if (i == 5000)
            throw std::runtime_error(exception_string);
        }
      )
    );
  }
  try {
    for (auto & fut : futures)
      fut.get();
  }
  catch (std::runtime_error & ex) {
    if (ex.what() == exception_string)
      caught = true;
  }
  ASSERT_TRUE(caught);
}

TEST(ThreadPoolTest, ParallelForEach) {
  size_t const n = 10000;
  std::vector<int> v_in(n);
  std::iota(v_in.begin(), v_in.end(), 0);
  std::vector<int> v_out(n);
  parallel_foreach(4, v_in.begin(), v_in.end(),
    [&v_out](size_t /*thread_id*/, int x) {
      v_out[x] = x*(x+1)/2;
    }
  );

  std::vector<int> v_expected(n);
  for (size_t i = 0; i < v_expected.size(); ++i)
    v_expected[i] = i*(i+1)/2;

  shouldEqualSequence(v_out.begin(), v_out.end(), v_expected.begin());
}

TEST(ThreadPoolTest, ParallelForEachException) {
  size_t const n = 10000;
  std::vector<int> v_in(n);
  std::iota(v_in.begin(), v_in.end(), 0);
  std::vector<int> v_out(n);
  bool caught = false;
  std::string exception_string = "the test exception";
  try {
    parallel_foreach(4, v_in.begin(), v_in.end(),
      [&v_out, &exception_string](size_t /*thread_id*/, int x) {
        if (x == 5000)
          throw std::runtime_error(exception_string);
        v_out[x] = x;
      }
    );
  }
  catch (std::runtime_error & ex) {
    if (ex.what() == exception_string)
      caught = true;
  }
  ASSERT_TRUE(caught);
}

TEST(ThreadPoolTest, ParallelForEachSum) {
  size_t const n_threads = 4;
  size_t const n = 2000;
  std::vector<size_t> input(n);
  std::iota(input.begin(), input.end(), 0);
  std::vector<size_t> results(n_threads, 0);

  parallel_foreach(n_threads, input.begin(), input.end(),
    [&results](size_t thread_id, size_t x) {
      results[thread_id] += x;
    }
  );

  size_t const sum = std::accumulate(results.begin(), results.end(), 0);
  ASSERT_EQ(sum, (n*(n-1))/2);
}

TEST(ThreadPoolTest, ParallelForEachSumSerial) {
  size_t const n = 2000;
  std::vector<size_t> input(n);
  std::iota(input.begin(), input.end(), 0);
  std::vector<size_t> results(1, 0);

  parallel_foreach(Options::NONE, input.begin(), input.end(),
    [&results](size_t thread_id, size_t x) {
      results[thread_id] += x;
    }
  );

  size_t const sum = std::accumulate(results.begin(), results.end(), 0);
  ASSERT_EQ(sum, (n*(n-1))/2);
}

TEST(ThreadPoolTest, ParallelForEachSumAuto) {
  Options opt;
  opt.nthreads(Options::AUTO);

  size_t const n = 2000;
  std::vector<size_t> input(n);
  std::iota(input.begin(), input.end(), 0);
  std::vector<size_t> results(opt.get_actual_nthreads(), 0);

  parallel_foreach(opt.get_nthreads(), input.begin(), input.end(),
    [&results](size_t thread_id, size_t x) {
      results[thread_id] += x;
    }
  );

  size_t const sum = std::accumulate(results.begin(), results.end(), 0);
  ASSERT_EQ(sum, (n*(n-1))/2);
}

// Optionally, you can keep this timing test, but consider disabling it by default due to its runtime
TEST(ThreadPoolTest, DISABLED_ParallelForEachTiming) {
  size_t const n_threads = 4;
  size_t const n = 300000000;
  std::vector<size_t> input(n);
  std::iota(input.begin(), input.end(), 0);

  std::vector<size_t> results(n_threads, 0);
  parallel_foreach(n_threads, input.begin(), input.end(),
    [&results](size_t thread_id, size_t /*x*/) {
      results[thread_id] += 1;
    }
  );

  size_t const sum = std::accumulate(results.begin(), results.end(), 0);
  ASSERT_EQ(sum, n);
}

// Run all the tests.
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
