![Build Status](https://github.com/jodyhagins/wjh_ipc/actions/workflows/cmake-multi-platform.yml/badge.svg)

## Summary

This is a relatively small library with some utilities that are useful when
writing IPC programs.

### Supported Environments

I test and build on a few different Linux and MacOS environments, with clang++
and g++.
The code may work with other environments, but I've not tested them.
Some assumptions are made that may be unique to those operating systems.
Feel free to experiment with other environments, and I'm open to pull requests.

## Using the Library

The library contains some header-only utilities (like Atomic/ipc_atomic).
However, the usual method is to just build the library.

Builds support cmake, and you should be able to simply grab the code and run cmake.
If not specified, `CMAKE_BUILD_TYPE` will default to `Debug` and
`CMAKE_CXX_STANDARD` will default to `20`.

You should also be able to use FetchContent.

Here is how I do it.

```cmake
FetchContent_Declare(
    wjh_ipc
    GIT_REPOSITORY https://github.com/jodyhagins/wjh_ipc.git
    GIT_TAG main # or better, a specific tag/SHA
    SYSTEM
    )
FetchContent_MakeAvailable(wjh_ipc)

# Then, when referencing it
target_link_libraries(my_target PRIVATE wjh::ipc)
```

### Third-party Dependencies

The only dependencies at this time are DocTest and RapidCheck, but those are
only needed if building the tests, which doesn't happen by default if you use it
via FetchContent.

## Tests

The top-level cmake file is setup to allow building and running the tests
when `CMAKE_CURRENT_SOURCE_DIR` and `CMAKE_SOURCE_DIR` are the same.
You could also set `WJH_PROC_BUILD_TESTS` to have the tests built.
Any test dependencies are handled automatically by FetchContent.
