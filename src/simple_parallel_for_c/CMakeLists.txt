aux_source_directory(. source)
add_library(simple_parallel_for_c ${source})
add_library(simple_parallel::simple_parallel_for_c ALIAS simple_parallel_for_c)

# if using clang, enable blocks extension
include(CheckCCompilerFlag)
check_c_compiler_flag("-fblocks" COMPILER_SUPPORTS_FBLOCKS_FLAG)
if(COMPILER_SUPPORTS_FBLOCKS_FLAG)
    target_compile_options(
        simple_parallel_for_c 
        PUBLIC  -fblocks)
    target_link_options(
        simple_parallel_for_c 
        PUBLIC  -lBlocksRuntime)
    try_compile(
        COMPILER_SUPPORTS_BLOCKS
        SOURCES ${PROJECT_SOURCE_DIR}/src/try_compile/test_blocks.c
        LINK_OPTIONS "-lBlocksRuntime")
    if(NOT COMPILER_SUPPORTS_BLOCKS)
        message(FATAL_ERROR "\nmissing BlocksRuntime library.
please install it from your distribution's repository:
    Debian:libblocksruntime-dev
    Gentoo:sys-libs/blocksruntime
    Fedora:libblocksruntime
or build it from source: https://github.com/mackyle/blocksruntime
or use gcc instead.")
    endif()
else()
    try_compile(
        COMPILER_SUPPORTS_NESTED_FUNCTIONS
        ${CMAKE_CURRENT_BINARY_DIR}
        ${PROJECT_SOURCE_DIR}/src/try_compile/test_nested_function.c)
    if(NOT COMPILER_SUPPORTS_NESTED_FUNCTIONS)
        message(FATAL_ERROR "Compiler does not support gcc's nested functions or clang's blocks extension. Please use clang or gcc.")
    endif()
endif()

# if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
#     target_compile_options(
#         simple_parallel_for_c 
#         PUBLIC  -fblocks)
#     target_link_options(
#         simple_parallel_for_c 
#         PUBLIC  -lBlocksRuntime)
# endif()

target_include_directories(
    simple_parallel_for_c 
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/configured_files/include>
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
            $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>)

target_link_libraries(
    simple_parallel_for_c
    PRIVATE $<BUILD_INTERFACE:simple_parallel::simple_parallel_options>
            $<BUILD_INTERFACE:simple_parallel::simple_parallel_warnings>)

target_link_system_libraries(
    simple_parallel_for_c 
    PUBLIC  simple_parallel
            OpenMP::OpenMP_C)

install(TARGETS simple_parallel_for_c
        EXPORT  simple_parallelTargets
        ARCHIVE DESTINATION lib/${PROJECT_NAME}-${PROJECT_VERSION})