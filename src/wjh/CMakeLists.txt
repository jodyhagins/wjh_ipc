## ======================================================================
## Copyright 2025 Jody Hagins
## Distributed under the MIT Software License
## See accompanying file LICENSE or copy at
## https://opensource.org/licenses/MIT
## ======================================================================
add_library(wjh_ipc
    STATIC
        ProcessId.cpp
        ProcessIdLock.cpp
    )
add_library(wjh::ipc ALIAS wjh_ipc)

target_include_directories(wjh_ipc
    INTERFACE
        "${PROJECT_SOURCE_DIR}/src")

    if (WJH_IPC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
