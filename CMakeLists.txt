## ======================================================================
## Copyright 2025 Jody Hagins
## Distributed under the MIT Software License
## See accompanying file LICENSE or copy at
## https://opensource.org/licenses/MIT
## ======================================================================
cmake_minimum_required(VERSION 3.26)

project(wjh_ipc
    VERSION 1.0
    DESCRIPTION "A small library of IPC-related utilities."
    LANGUAGES CXX)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(Setup)

find_package(Threads REQUIRED)
include(FetchContent)
include(CTest)
include(CompileOptions)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    option(WJH_PROC_BUILD_TESTS "whether or not to build tests" ON)

    if (WJH_PROC_BUILD_TESTS)
        string(REGEX REPLACE "(^| )-g([0-9]?)( |$)" "\\1-g3\\3" tmp "${CMAKE_CXX_FLAGS_DEBUG}")
        if (NOT "${CMAKE_CXX_FLAGS_DEBUG}" STREQUAL "${tmp}")
            message(STATUS "Changing CMAKE_CXX_FLAGS_DEBUG from '${CMAKE_CXX_FLAGS_DEBUG}' to '${tmp}'")
            set(CMAKE_CXX_FLAGS_DEBUG "${tmp}")
        endif()

        message(STATUS "Processing third-party DocTest...")
        FetchContent_Declare(
            DocTest
            GIT_REPOSITORY https://github.com/jodyhagins/doctest.git
            GIT_TAG 6b7985bc2562a135964bc03d69a3ef7af12b9309 # order-by-rand
            SYSTEM
            )
        FetchContent_MakeAvailable(DocTest)

        message(STATUS "Processing third-party RapidCheck...")
        set(RC_ENABLE_DOCTEST ON)
        FetchContent_Declare(
            rapidcheck
            GIT_REPOSITORY https://github.com/jodyhagins/rapidcheck.git
            GIT_TAG wjh-master
            SYSTEM
            )
        FetchContent_MakeAvailable(rapidcheck)
    endif()
endif()

add_subdirectory(src)
