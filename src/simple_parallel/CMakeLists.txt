aux_source_directory(. source)
add_library(simple_parallel ${source})

target_include_directories(
    simple_parallel 
    PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include"
    PUBLIC  "include")

target_link_libraries(
    simple_parallel
    PRIVATE simple_parallel::simple_parallel_options
            simple_parallel::simple_parallel_warnings)

target_link_system_libraries(
    simple_parallel 
    PUBLIC  MPI::MPI_CXX
            mimalloc
            fmt::fmt
            Microsoft.GSL::GSL
            bigmpi)