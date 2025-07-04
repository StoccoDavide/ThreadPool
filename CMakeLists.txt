# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Copyright (c) 2025, Davide Stocco.                                                              #
#                                                                                                 #
# The ThreadPool project is distributed under the MIT License.                                    #
#                                                                                                 #
# Davide Stocco                    University of Trento                    davide.stocco@unitn.it #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

cmake_minimum_required(VERSION 3.14)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ThreadPoolVersion)
get_version_from_git()

project(ThreadPool
  VERSION ${THREADPOOL_VERSION}
  DESCRIPTION "A simple thread pool library in C++17"
  HOMEPAGE_URL "https://github.com/StoccoDavide/ThreadPool"
  LANGUAGES CXX
)

if(NOT THREADPOOL_VERSION_PRERELEASE)
  set(THREADPOOL_FULL_VERSION "${THREADPOOL_VERSION}")
  write_version_file()
else()
  set(THREADPOOL_FULL_VERSION "${THREADPOOL_VERSION}-${THREADPOOL_VERSION_PRERELEASE}")
endif()
message(STATUS "ThreadPool: version ${THREADPOOL_FULL_VERSION}")

set(THREADPOOL_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

if(NOT CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/lib")
  message(STATUS "ThreadPool: CMAKE_INSTALL_PREFIX not specified, defaulting to ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# Library definition
add_library(ThreadPool INTERFACE)
add_library(ThreadPool::ThreadPool ALIAS ThreadPool)

target_include_directories(ThreadPool INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# INFO: Ensure this options are only available to developers. In theory we could leave them available,
# but the call to `target_sources` breaks `fetchcontent` compatibility due to absolute paths being
# added to `INTERFACE_SOURCES`. I tried solving it, but it seems to be poorly documented, supported, etc.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
    message(STATUS "ThreadPool: CMAKE_BUILD_TYPE not specified, defaulting to ${CMAKE_BUILD_TYPE}")
  endif()

  option(THREADPOOL_BUILD_TESTS "Build tests" OFF)
  option(THREADPOOL_BUILD_BENCHMARKS "Build benchmarks" OFF)
  option(THREADPOOL_BUILD_EXAMPLES "Build examples" OFF)

  file(GLOB_RECURSE HEADER_FILES_HH "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hh")
  foreach(HEADER_FILE IN LISTS HEADER_FILES_HH)
    file(
      RELATIVE_PATH
      RELATIVE_PATH_HEADER_FILE
      ${CMAKE_CURRENT_SOURCE_DIR} ${HEADER_FILE}
    )
    list(APPEND RELATIVE_HEADER_FILES ${RELATIVE_PATH_HEADER_FILE})
  endforeach()
  target_sources(ThreadPool INTERFACE $<BUILD_INTERFACE:${RELATIVE_HEADER_FILES}>)

  if(THREADPOOL_BUILD_TESTS)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
  endif()

  if(THREADPOOL_BUILD_BENCHMARKS)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
  endif()

  if(THREADPOOL_BUILD_EXAMPLES)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples)
  endif()

  if(THREADPOOL_ENABLE_PLOTTING)
    target_compile_definitions(ThreadPool INTERFACE THREADPOOL_ENABLE_PLOTTING)
  endif()
endif()

if(MSVC)
  target_compile_options(ThreadPool INTERFACE /W4 /WX)
else()
  target_compile_options(ThreadPool INTERFACE -Wall -Wextra -pedantic -Werror)
endif()

# Installation
install(TARGETS ThreadPool EXPORT ThreadPoolTargets)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION include)

include(CMakePackageConfigHelpers)

export(EXPORT ThreadPoolTargets
  FILE ${CMAKE_CURRENT_BINARY_DIR}/ThreadPoolTargets.cmake
  NAMESPACE ThreadPool::
)

install(EXPORT ThreadPoolTargets
  FILE ThreadPoolTargets.cmake
  NAMESPACE ThreadPool::
  DESTINATION lib/cmake/ThreadPool
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ThreadPoolConfig.cmake
  INSTALL_DESTINATION lib/cmake/ThreadPool
)

write_basic_package_version_file(
  ThreadPoolConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/ThreadPoolConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/ThreadPoolConfigVersion.cmake
  DESTINATION lib/cmake/ThreadPool
)
