cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(simple_parallel VERSION 1.0.0 LANGUAGES CXX)

# options
option(SIMPLE_PARALLEL_OVERRIDE_MAIN "Override main using ld's --wrap option" ON)
option(SIMPLE_PARALLEL_C_BINDING "Enable C binding" ON)

if(CMAKE_Fortran_COMPILER_LOADED)
    option(SIMPLE_PARALLEL_Fortran_BINDING "Enable Fortran binding" ON)
else()
    option(SIMPLE_PARALLEL_Fortran_BINDING "Enable Fortran binding" OFF)
endif()

option(SIMPLE_PARALLEL_ENABLE_EXAMPLES "Enable examples" OFF)

# advance options
option(SIMPLE_PARALLEL_WARNING "Enable warnings" OFF)
option(SIMPLE_PARALLEL_WARNING_AS_ERROR "Enable warning as error" OFF)
mark_as_advanced(SIMPLE_PARALLEL_WARNING SIMPLE_PARALLEL_WARNING_AS_ERROR)

if(SIMPLE_PARALLEL_C_BINDING)
    enable_language(C)
endif()
if(SIMPLE_PARALLEL_Fortran_BINDING)
    enable_language(Fortran)
    include(FortranCInterface)
    FortranCInterface_VERIFY(CXX)
endif()

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

find_package(MPI REQUIRED COMPONENTS C CXX)

if(SIMPLE_PARALLEL_Fortran_BINDING)
    find_package(MPI REQUIRED COMPONENTS Fortran)
    if(NOT MPI_Fortran_FOUND)
        message(FATAL_ERROR "MPI Fortran not found")
    endif()
    if(NOT MPI_Fortran_HAVE_F08_MODULE)
        message(FATAL_ERROR "MPI Fortran does not support F08 module, please use a recent version of OpenMPI, and make sure its mpi_f08.mod is compatible with your compiler")
    endif()

    if(NOT TARGET re2::re2)
        find_package(re2 REQUIRED)
    endif()

    if(NOT TARGET PkgConfig::libelf)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(libelf REQUIRED IMPORTED_TARGET libelf)
    endif()
endif()

if((NOT TARGET Boost::headers) OR (NOT TARGET Boost::mpi))
    find_package(Boost REQUIRED COMPONENTS mpi headers thread container)
endif()

get_target_property(BOOST_IS_IMPORTED Boost::headers IMPORTED)

if(NOT TARGET Microsoft.GSL::GSL)
    find_package(Microsoft.GSL REQUIRED)
endif()

if(NOT TARGET hwy)
    find_package(HWY REQUIRED)
endif()

if(SIMPLE_PARALLEL_ENABLE_EXAMPLES)
    find_package(OpenMP REQUIRED)
endif()

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

configure_file("cmake/simple_parallel_config.h.in" "${PROJECT_BINARY_DIR}/configured_files/include/internal_use_only/simple_parallel_config.h.in" ESCAPE_QUOTES)

add_subdirectory(src)

if(SIMPLE_PARALLEL_ENABLE_EXAMPLES)
    add_subdirectory(example)
endif()

if(CMAKE_SKIP_INSTALL_RULES)
    return()
endif()

install(TARGETS 
    s_p_init
    s_p_c_binding
    s_p_fortran_binding
    s_p_options
    s_p_warings
    mimalloc-obj
    EXPORT 
    simple_parallelTargets
    FILE_SET public_header
)

install(EXPORT simple_parallelTargets
    DESTINATION lib/cmake/simple_parallel
    NAMESPACE simple_parallel::
    FILE simple_parallelTargets.cmake
)

include(CMakePackageConfigHelpers)
# generate the config file that includes the exports
configure_package_config_file(cmake/simple_parallelConfig.cmake.in
    ${PROJECT_BINARY_DIR}/simple_parallelConfig.cmake
    INSTALL_DESTINATION lib/cmake/simple_parallel
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

write_basic_package_version_file(
    simple_parallelConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    ${PROJECT_BINARY_DIR}/simple_parallelConfig.cmake
    ${PROJECT_BINARY_DIR}/simple_parallelConfigVersion.cmake
    DESTINATION lib/cmake/simple_parallel
)

export(EXPORT simple_parallelTargets
    FILE ${PROJECT_BINARY_DIR}/simple_parallelTargets.cmake
    NAMESPACE simple_parallel::
)