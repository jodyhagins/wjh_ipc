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
    option(WJH_IPC_BUILD_TESTS "whether or not to build tests" ON)
    if (WJH_IPC_BUILD_TESTS)
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

    # The .clang-format included with this project requires a custom fork
    # of clang-format.  You likely don't need this unless you want to make
    # a properly formatted submission.
    option(WJH_IPC_BUILD_WJH_FORMAT "Fetch and build custom clang-format" OFF)
    if (WJH_IPC_BUILD_WJH_FORMAT)
	set(WJH_IPC_FORMAT_INSTALL_DIR "${PROJECT_BINARY_DIR}/wjh_ipc_format_install")
	set(WJH_IPC_FORMAT_BIN "${WJH_IPC_FORMAT_INSTALL_DIR}/bin/clang-format")

	# Check if clang-format is already built
	if (EXISTS ${WJH_IPC_FORMAT_BIN})
	    message(STATUS "Using cached clang-format from ${WJH_IPC_FORMAT_BIN}")
	else()
	    message(STATUS "clang-format not found, fetching and building; this will take a while...")

	    # Fetch LLVM repository
	    FetchContent_Declare(
		wjh_format
		GIT_REPOSITORY https://github.com/jodyhagins/llvm-project
		GIT_TAG        2b5f27077f3c35be5370f9f5253ebd497e2901e3
	    )
	    FetchContent_MakeAvailable(wjh_format)

	    # Ensure build directory exists
	    file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/wjh_format_build)

	    # Configure LLVM
	    execute_process(
		COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}"
			-S ${wjh_format_SOURCE_DIR}/llvm
			-B ${PROJECT_BINARY_DIR}/wjh_format_build
			-DCMAKE_BUILD_TYPE=MinSizeRel
			-DCMAKE_INSTALL_PREFIX=${WJH_IPC_FORMAT_INSTALL_DIR}
			"-DLLVM_ENABLE_PROJECTS=clang;clang-tools-extra"
			-DLLVM_INCLUDE_TESTS=OFF
			-DLLVM_INCLUDE_BENCHMARKS=OFF
			-DLLVM_INCLUDE_DOCS=OFF
			-DLLVM_BUILD_TOOLS=OFF
		COMMAND_ECHO STDOUT
		RESULT_VARIABLE CLANG_FORMAT_CONFIG_RESULT
	    )

	    if (CLANG_FORMAT_CONFIG_RESULT)
		message(FATAL_ERROR "Failed to configure clang-format build")
	    endif()

	    # Build and install clang-format
	    execute_process(
		COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR}/wjh_format_build --target install
		RESULT_VARIABLE CLANG_FORMAT_BUILD_RESULT
	    )

	    if (CLANG_FORMAT_BUILD_RESULT)
		message(FATAL_ERROR "Failed to build clang-format")
	    endif()

	    # Verify installation
	    if (NOT EXISTS ${WJH_IPC_FORMAT_BIN})
		message(FATAL_ERROR "clang-format binary not found after build!")
	    endif()

	    # We are caching the binary, so remove the build artifacts
	    message("Removing llvm build artifacts: REMOVE_RECURSE ${PROJECT_BINARY_DIR}/wjh_format_build")
	    file(REMOVE_RECURSE ${PROJECT_BINARY_DIR}/wjh_format_build)
	endif()

	set(WJH_IPC_FORMAT_COMMAND ${WJH_IPC_FORMAT_BIN})
    else()
	# Check if wjh-format exists in PATH
	find_program(WJH_IPC_FORMAT_PATH "wjh-format")
	if (WJH_IPC_FORMAT_PATH)
	    message(STATUS "Using system-provided wjh-format: ${WJH_IPC_FORMAT_PATH}")
	    set(WJH_IPC_FORMAT_COMMAND ${WJH_IPC_FORMAT_PATH})
	endif()
    endif()

    if (WJH_IPC_FORMAT_COMMAND)
	add_custom_target(run-format
            COMMAND find ${PROJECT_SOURCE_DIR} -type f -regex ".*\\.[chi]pp" -exec "${WJH_IPC_FORMAT_COMMAND}" -i {} +
	    COMMENT "Reformatting all source files."
	    VERBATIM
	)
    endif()
endif()

add_subdirectory(src)
