cmake_minimum_required(VERSION 3.23)

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

# configure files based on CMake configuration options
add_subdirectory(configured_files)

add_subdirectory(src)

install(EXPORT  simple_parallelTargets
        FILE    simple_parallelTargets.cmake
        DESTINATION lib/cmake/simple_parallel)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    simple_parallelConfigVersion.cmake
    VERSION         ${PROJECT_VERSION}
    COMPATIBILITY   AnyNewerVersion)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/simple_parallelConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/simple_parallelConfig.cmake"
    INSTALL_DESTINATION "lib/cmake/simple_parallel"
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO)

install(FILES       ${CMAKE_CURRENT_BINARY_DIR}/simple_parallelConfig.cmake
        DESTINATION lib/cmake/simple_parallel)

if(simple_parallel_C_BINDING)
    export(TARGETS simple_parallel
                   simple_parallel_for_c
                   simple_parallel_options 
                   simple_parallel_warnings
                   mimalloc-obj
        NAMESPACE simple_parallel::
        FILE simple_parallelTargets.cmake)
else()
    export(TARGETS simple_parallel
                   simple_parallel_options 
                   simple_parallel_warnings
                   mimalloc-obj
        NAMESPACE simple_parallel::
        FILE simple_parallelTargets.cmake)
endif()

export(PACKAGE simple_parallel)

set(CPACK_PACKAGE_VENDOR "LXYan2333")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "a cross node parallel library")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_IGNORE_FILES
    /.git
    /build
    /.*build.*
    /\\\\.DS_Store)
include(CPack)
