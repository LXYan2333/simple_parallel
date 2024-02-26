aux_source_directory(. source)
add_library(simple_parallel ${source})
add_library(simple_parallel::simple_parallel ALIAS simple_parallel)

target_include_directories(
    simple_parallel 
    PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include"
    PUBLIC  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
            $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>)

target_link_libraries(
    simple_parallel
    PRIVATE $<BUILD_INTERFACE:simple_parallel::simple_parallel_options>
            $<BUILD_INTERFACE:simple_parallel::simple_parallel_warnings>)


target_link_system_libraries(
    simple_parallel 
    PUBLIC  OpenMP::OpenMP_CXX
            mimalloc
            MPI::MPI_C
            Boost::mpi
            cppcoro::cppcoro
    PRIVATE Microsoft.GSL::GSL)
