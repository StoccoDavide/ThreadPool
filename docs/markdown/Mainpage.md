# ThreadPool

`ThreadPool` is a simple thread pool library written in C++17 that allows you to run tasks concurrently using a pool of worker threads. It is designed to be easy to use, efficient, and flexible, making it suitable for a wide range of applications. The library provides a simple interface for submitting tasks and managing the thread pool, while handling the complexities of thread management internally.

## Installation

### Quick and dirty

`ThreadPool` is a header-only library with no dependencies, so the quick and dirty way of installing it is by simply copying the `include` directory to your project. Alternatively, you can do things properly and use `CMake` (version >= 3.14).

### CMake

If you are using CMake, you can add the library as a subdirectory in your project.

```cmake
add_subdirectory(path/to/ThreadPool)
target_link_libraries(your_target PRIVATE ThreadPool::ThreadPool)
```

You can use `FetchContent` to download the library from GitHub.

```cmake
include(FetchContent)

# Optionally specify a custom path to fetch content to
set(FETCHCONTENT_BASE_DIR "path/to/your/dependencies")
fetchcontent_declare(
  ThreadPool
  GIT_REPOSITORY https://github.com/StoccoDavide/ThreadPool.git
  GIT_TAG        main
)
fetchcontent_makeavailable(ThreadPool)
target_link_libraries(your_target PRIVATE ThreadPool::ThreadPool)
```

If you already have `ThreadPool` somewhere on your system, you can use `find_pacakge` directly.

```cmake
# Optionally specify a custom path to find content from
list(APPEND CMAKE_PREFIX_PATH "path/to/your/dependencies")
find_package(
  ThreadPool
  ${YOUR_DESIRED_ThreadPool_VERSION}
  NO_MODULE
)

target_link_libraries(your_target PRIVATE ThreadPool::ThreadPool)
```

Since we are nice people, we also show you how to conditionally use `FetchContent` based if you already have the library or not.

```cmake
# Optionally specify a custom path to find content from
list(APPEND CMAKE_PREFIX_PATH "path/to/your/dependencies")
find_package(
  ThreadPool
  ${YOUR_DESIRED_ThreadPool_VERSION}
  NO_MODULE
)

if(NOT TARGET ThreadPool::ThreadPool)
  include(FetchContent)

  # Optionally specify a custom path to fetch content to
  set(FETCHCONTENT_BASE_DIR "path/to/your/dependencies")
  fetchcontent_declare(
    ThreadPool
    GIT_REPOSITORY https://github.com/StoccoDavide/ThreadPool.git
    GIT_TAG        main
  )

  fetchcontent_makeavailable(ThreadPool)
endif()

target_link_libraries(your_target PRIVATE ThreadPool::ThreadPool)
```

## Authors

- Davide Stocco <br>
  University of Trento <br>
  Department of Industrial Engineering <br>
  email: davide.stocco@unitn.it

Aka...

```
▗▄▄▄  ▄   ▄  ▐▌
▐▌  █ █   █  ▐▌
▐▌  █  ▀▄▀▗▞▀▜▌
▐▙▄▄▀     ▝▚▄▟▌

```

## License

The `ThreadPool` project is distributed under the MIT License - see the [LICENSE](https://StoccoDavide.github.io/ThreadPool/LICENSE) file for details.

Here's what the license entails:

1. Anyone can copy, modify and distribute this software.
2. The software is provided "as is", without warranty of any kind.
3. The above copyright notice and this permission notice must be included in all copies or substantial portions of the software.
4. The software can be used for both commercial and non-commercial purposes, meaning you can use it in your projects without any limitations.
