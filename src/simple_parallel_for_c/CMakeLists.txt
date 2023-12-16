aux_source_directory(. source)
add_library(simple_parallel_for_c ${source})

# if using clang, enable blocks extension
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(
        simple_parallel_for_c 
        PUBLIC  -fblocks)
    target_link_options(
        simple_parallel_for_c 
        PUBLIC  -lBlocksRuntime)
endif()

target_include_directories(
    simple_parallel_for_c 
    PUBLIC  "${CMAKE_BINARY_DIR}/configured_files/include" # make it public since this file is required in public header
            "include")

target_link_libraries(
    simple_parallel_for_c
    PRIVATE simple_parallel::simple_parallel_options
            simple_parallel::simple_parallel_warnings)

target_link_system_libraries(
    simple_parallel_for_c 
    PUBLIC  MPI::MPI_C
            mimalloc
            simple_parallel)