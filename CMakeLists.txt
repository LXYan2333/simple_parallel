cmake_minimum_required(VERSION 3.21.0)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 23)

cmake_policy(SET CMP0069 NEW)

project(simple_parallel VERSION 0.1.0 LANGUAGES C CXX)

include(cmake/PreventInSourceBuilds.cmake)
include(ProjectOptions.cmake)
simple_parallel_setup_options()
simple_parallel_global_options()

include(Dependencies.cmake)
simple_parallel_setup_dependencies()

simple_parallel_local_options()

target_compile_features(simple_parallel_options INTERFACE cxx_std_${CMAKE_CXX_STANDARD})

add_library(simple_parallel::simple_parallel_options ALIAS simple_parallel_options)
add_library(simple_parallel::simple_parallel_warnings ALIAS simple_parallel_warnings)

# configure files based on CMake configuration options
add_subdirectory(configured_files)

add_subdirectory(src)

if(CMAKE_SKIP_INSTALL_RULES)
    return()
endif()

configure_file(cmake/simple_parallelConfig.cmake.in simple_parallelConfig.cmake @ONLY)

write_basic_package_version_file(
    simple_parallelConfigVersion.cmake
    VERSION ${PACKAGE_VERSION}
    COMPATIBILITY AnyNewerVersion)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/simple_parallelConfigVersion.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/simple_parallelConfig.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/simple_parallel)

install(EXPORT simple_parallelTargets
        FILE simple_parallelTargets.cmake
        NAMESPACE simple_parallel::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/simple_parallel)

export(EXPORT simple_parallelTargets
       FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/simple_parallelTargets.cmake"
       NAMESPACE simple_parallel::
)