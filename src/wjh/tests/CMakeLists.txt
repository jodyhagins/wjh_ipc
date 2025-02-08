## ======================================================================
## Copyright 2025 Jody Hagins
## Distributed under the MIT Software License
## See accompanying file LICENSE or copy at
## https://opensource.org/licenses/MIT
## ======================================================================
add_executable(wjh_ipc_ut main.cpp
    Atomic_ut.cpp
    ProcessId_ut.cpp
    ProcessIdLock_ut.cpp
    )

target_link_libraries(wjh_ipc_ut
    PRIVATE
        wjh::ipc
        Threads::Threads
    )


add_test(
    NAME "wjh_ipc Tests"
    COMMAND wjh_ipc_ut)
set_target_properties(wjh_ipc_ut
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
target_link_libraries(wjh_ipc_ut PRIVATE rapidcheck_doctest doctest)
