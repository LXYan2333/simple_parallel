aux_source_directory(. source)

add_executable(simple_parallel_example ${source})

target_link_libraries(
    simple_parallel_example
    PRIVATE simple_parallel)

target_link_system_libraries(simple_parallel_example
    PRIVATE fmt::fmt)