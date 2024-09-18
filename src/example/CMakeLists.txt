add_executable(simple_parallel_example example.cc)

find_package(OpenMP REQUIRED)

target_link_system_libraries(simple_parallel_example
    PRIVATE mimalloc-obj simple_parallel
            OpenMP::OpenMP_CXX)