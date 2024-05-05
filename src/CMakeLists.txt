# do not build shared library since the linker may link to system mimalloc at runtime
set(MI_BUILD_SHARED OFF)
set(MI_BUILD_TESTS OFF)
add_subdirectory(mimalloc_for_simple_parallel)

add_subdirectory(simple_parallel)
add_subdirectory(simple_parallel_for_c)
add_subdirectory(example)
add_subdirectory(example_for_c)

if(simple_parallel_ENABLE_TESTING)
    add_subdirectory(test)
endif()