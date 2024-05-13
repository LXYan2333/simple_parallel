cmake_minimum_required(VERSION 3.21.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 23)

cmake_policy(SET CMP0069 NEW)

project(simple_parallel VERSION 1.0.0 LANGUAGES C CXX)

include(cmake/PreventInSourceBuilds.cmake)
include(ProjectOptions.cmake)
simple_parallel_setup_options()
simple_parallel_global_options()

# clone git submodules if they are not present
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
# Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/src/mimalloc_for_simple_parallel/CMakeLists.txt")
    message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()

include(Dependencies.cmake)
simple_parallel_setup_dependencies()

simple_parallel_local_options()

target_compile_features(simple_parallel_options INTERFACE cxx_std_${CMAKE_CXX_STANDARD})

add_library(simple_parallel::simple_parallel_options ALIAS simple_parallel_options)
add_library(simple_parallel::simple_parallel_warnings ALIAS simple_parallel_warnings)

add_subdirectory(src)

# configure files based on CMake configuration options
add_subdirectory(configured_files)

if(CMAKE_SKIP_INSTALL_RULES)
    return()
endif()


packageProject(
    # the name of the target to export
    NAME simple_parallel
    # the version of the target to export
    VERSION ${PROJECT_VERSION}
    # a temporary directory to create the config files
    BINARY_DIR ${PROJECT_BINARY_DIR}
    # location of the target's public headers
    INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include
    # should match the target's INSTALL_INTERFACE include directory
    INCLUDE_DESTINATION include/${PROJECT_NAME}-${PROJECT_VERSION}
    # (optional) option to install only header files with matching pattern
    INCLUDE_HEADER_PATTERN "*.h"
    # semicolon separated list of the project's dependencies
    DEPENDENCIES "MPI REQUIRED COMPONENTS C; Boost REQUIRED COMPONENTS mpi container; OpenMP REQUIRED; cppcoro REQUIRED; Microsoft.GSL REQUIRED"
    # (optional) install your library with a namespace (Note: do NOT add extra '::')
    NAMESPACE simple_parallel
    # (optional) define the project's version compatibility, defaults to `AnyNewerVersion`
    # supported values: `AnyNewerVersion|SameMajorVersion|SameMinorVersion|ExactVersion`
    COMPATIBILITY SameMajorVersion
    # (optional) option to generate CPack variables
    CPACK YES)

install(DIRECTORY   ${PROJECT_BINARY_DIR}/configured_files/include/internal_use_only
        DESTINATION include/${PROJECT_NAME}-${PROJECT_VERSION})