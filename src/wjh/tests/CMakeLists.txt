## ======================================================================
## Copyright 2025 Jody Hagins
## Distributed under the MIT Software License
## See accompanying file LICENSE or copy at
## https://opensource.org/licenses/MIT
## ======================================================================
add_executable(procid_ut main.cpp
    ProcessId_ut.cpp
    ProcessIdLock_ut.cpp
    )
target_link_libraries(procid_ut
    PRIVATE
        wjh::ipc
        Threads::Threads
        rapidcheck_doctest
        doctest
    )
set_target_properties(procid_ut
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
add_test(
    NAME "ProcessId Tests"
    COMMAND procid_ut)

add_executable(atomic_ut main.cpp
    Atomic_ut.cpp
    )
target_link_libraries(atomic_ut
    PRIVATE
        wjh::ipc
        Threads::Threads
        rapidcheck_doctest
        doctest
    )
set_target_properties(atomic_ut
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
add_test(
    NAME "Atomic Tests"
    COMMAND atomic_ut)
