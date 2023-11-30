add_subdirectory(simple_parallel)
add_subdirectory(example)

if(simple_parallel_ENABLE_TESTING)
    add_subdirectory(test)
endif()