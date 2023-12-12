add_subdirectory(simple_parallel)
add_subdirectory(simple_parallel_for_c)
add_subdirectory(example)
add_subdirectory(example_for_c)

if(simple_parallel_ENABLE_TESTING)
    add_subdirectory(test)
endif()