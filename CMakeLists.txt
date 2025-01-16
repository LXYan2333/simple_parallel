cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(simple_parallel VERSION 1.0.0 LANGUAGES CXX)

# options
option(SIMPLE_PARALLEL_OVERRIDE_MAIN "Override main using ld's --wrap option" ON)
option(SIMPLE_PARALLEL_ENABLE_EXAMPLES "Enable examples" OFF)

# advance options
option(SIMPLE_PARALLEL_WARNING "Enable warnings" OFF)
option(SIMPLE_PARALLEL_WARNING_AS_ERROR "Enable warning as error" OFF)
mark_as_advanced(SIMPLE_PARALLEL_WARNING SIMPLE_PARALLEL_WARNING_AS_ERROR)

add_library(s_p_warings INTERFACE)
add_library(simple_parallel::s_p_warings ALIAS s_p_warings)
add_library(s_p_options INTERFACE)
add_library(simple_parallel::s_p_options ALIAS s_p_options)
if(SIMPLE_PARALLEL_WARNING)
    message(STATUS "Enable warnings")
    target_compile_options(s_p_warings INTERFACE -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wpedantic -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough -Wstrict-overflow $<$<CXX_COMPILER_ID:GNU>:-Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast -Wsuggest-override>)
endif()
if(SIMPLE_PARALLEL_WARNING_AS_ERROR)
    message(STATUS "Enable warning as error")
    target_compile_options(s_p_warings INTERFACE -Werror)
endif()
target_compile_features(s_p_options INTERFACE cxx_std_${CMAKE_CXX_STANDARD})

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
    set(CMAKE_BUILD_TYPE
        RelWithDebInfo
        CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui, ccmake
    set_property(
        CACHE CMAKE_BUILD_TYPE
        PROPERTY STRINGS
                "Debug"
                "Release"
                "MinSizeRel"
                "RelWithDebInfo")
endif()

set(MPI_CXX_SKIP_MPICXX ON)
find_package(MPI REQUIRED)
find_package(Boost REQUIRED COMPONENTS mpi headers thread)
find_package(Microsoft.GSL REQUIRED)
find_package(HWY REQUIRED)

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

add_subdirectory(src)