aux_source_directory(. source)
add_library(simple_parallel OBJECT ${source})
add_library(simple_parallel::simple_parallel ALIAS simple_parallel)

target_include_directories(
    simple_parallel 
    PUBLIC  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/configured_files/include>
            $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
            $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>)

target_link_libraries(
    simple_parallel
    PRIVATE $<BUILD_INTERFACE:simple_parallel::simple_parallel_options>
            $<BUILD_INTERFACE:simple_parallel::simple_parallel_warnings>)

if(NOT TARGET concurrentqueue::concurrentqueue)
    add_library(concurrentqueue::concurrentqueue ALIAS concurrentqueue)
endif()

if(NOT TARGET cppcoro::cppcoro)
    add_library(cppcoro::cppcoro ALIAS cppcoro)
endif()

target_link_system_libraries(
    simple_parallel 
    PUBLIC  OpenMP::OpenMP_CXX
            mimalloc-obj
            MPI::MPI_C
            Boost::mpi
            cppcoro::cppcoro
            Microsoft.GSL::GSL
            Boost::container
            concurrentqueue::concurrentqueue)  

install(TARGETS mimalloc-obj
        EXPORT  simple_parallelTargets
        ARCHIVE DESTINATION lib/mimalloc_for_simple_parallel)