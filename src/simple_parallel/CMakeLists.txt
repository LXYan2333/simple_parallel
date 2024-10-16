aux_source_directory(. source)
add_library(simple_parallel OBJECT ${source})
add_library(simple_parallel::simple_parallel ALIAS simple_parallel)

target_sources(
    simple_parallel
    PUBLIC
    FILE_SET    HEADERS
    BASE_DIRS   ${PROJECT_SOURCE_DIR}/include
                ${CMAKE_BINARY_DIR}/configured_files/include/
    FILES       ${PROJECT_SOURCE_DIR}/include/simple_parallel/detail.h
                ${PROJECT_SOURCE_DIR}/include/simple_parallel/dynamic_schedule.h
                ${PROJECT_SOURCE_DIR}/include/simple_parallel/main.h
                ${PROJECT_SOURCE_DIR}/include/simple_parallel/master.h
                ${PROJECT_SOURCE_DIR}/include/simple_parallel/mpi_util.h
                ${PROJECT_SOURCE_DIR}/include/simple_parallel/simple_parallel.h
                ${PROJECT_SOURCE_DIR}/include/simple_parallel/worker.h
                ${CMAKE_BINARY_DIR}/configured_files/include/internal_use_only/simple_parallel_config.h
                ${CMAKE_BINARY_DIR}/configured_files/include/internal_use_only/include_concurrentqueue.h)


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
            MPI::MPI_CXX
            Boost::mpi
            cppcoro::cppcoro
            Microsoft.GSL::GSL
            Boost::container
            concurrentqueue::concurrentqueue)  

# install(TARGETS mimalloc-obj
#         EXPORT  simple_parallelTargets
#         OBJECTS DESTINATION lib/mimalloc_for_simple_parallel)

install(TARGETS simple_parallel
                mimalloc-obj
        EXPORT  simple_parallelTargets
        OBJECTS DESTINATION ${CMAKE_INSTALL_LIBDIR}
        FILE_SET HEADERS)