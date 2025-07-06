# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Copyright (c) 2025, Davide Stocco.                                                              #
#                                                                                                 #
# The ThreadPool project is distributed under the MIT License.                                    #
#                                                                                                 #
# Davide Stocco                    University of Trento                    davide.stocco@unitn.it #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

include(ThreadPoolGTest)

file(GLOB_RECURSE TEST_THREADPOOL "${CMAKE_CURRENT_SOURCE_DIR}/test_threadpool.cc")
add_executable(test_threadpool ${TEST_THREADPOOL})
target_link_libraries(test_threadpool PRIVATE ThreadPool GTest::gtest)
