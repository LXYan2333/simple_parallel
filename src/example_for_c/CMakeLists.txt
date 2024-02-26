add_executable(simple_parallel_example_for_c example_for_c.c)

find_package(OpenMP REQUIRED)

target_link_system_libraries(
    simple_parallel_example_for_c
    PRIVATE simple_parallel_for_c)