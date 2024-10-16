if(NOT TARGET concurrentqueue::concurrentqueue)
    add_library(concurrentqueue::concurrentqueue ALIAS concurrentqueue)
endif()

# after cmake 3.29: Alias targets to imported libraries are also supported in try_compile.
# add these code to compatible with cmake<3.29
get_property(aliased_target TARGET concurrentqueue::concurrentqueue PROPERTY ALIASED_TARGET)
if(aliased_target)
    set(concurrentqueue_link_name concurrentqueue)
else()
    set(concurrentqueue_link_name concurrentqueue::concurrentqueue)
endif()

try_compile(
    include_moodycamel_concurrentqueue
    SOURCES ${PROJECT_SOURCE_DIR}/src/try_compile/test_include_moodycamel_concurrentqueue.cc
    LINK_LIBRARIES ${concurrentqueue_link_name})

# since `try_compile` only works for imported target, if the target is added by `add_library` or `add_subdirectory`,
# the `try_compile` will always fail. since the `concurrentqueue` target always exists, we just assume the header 
# is included as `#include <concurrentqueue.h>`
if(NOT include_moodycamel_concurrentqueue)
    set(include_concurrentqueue true)
endif()

configure_file("include_concurrentqueue.h.in" "${CMAKE_BINARY_DIR}/configured_files/include/internal_use_only/include_concurrentqueue.h" ESCAPE_QUOTES)

if(simple_parallel_C_BINDING)
    # if using clang, enable blocks extension
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-fblocks" COMPILER_SUPPORTS_FBLOCKS_FLAG)
    if(COMPILER_SUPPORTS_FBLOCKS_FLAG)
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
and make sure it is in the library path.
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
    configure_file("c_omp_dynamic_schedule_buffer_size_config.h.in" "${CMAKE_BINARY_DIR}/configured_files/include/internal_use_only/c_omp_dynamic_schedule_buffer_size_config.h" ESCAPE_QUOTES)
endif()

configure_file("simple_parallel_config.h.in" "${CMAKE_BINARY_DIR}/configured_files/include/internal_use_only/simple_parallel_config.h" ESCAPE_QUOTES)
