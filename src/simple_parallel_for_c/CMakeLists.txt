aux_source_directory(. source)
add_library(simple_parallel_for_c OBJECT ${source})
add_library(simple_parallel::simple_parallel_for_c ALIAS simple_parallel_for_c)

target_sources(
    simple_parallel
    PUBLIC
    FILE_SET    HEADERS
    BASE_DIRS   ${PROJECT_SOURCE_DIR}/include
                ${PROJECT_BINARY_DIR}/configured_files/include/
    FILES       ${PROJECT_SOURCE_DIR}/include/simple_parallel_for_c/lambda.h
                ${PROJECT_SOURCE_DIR}/include/simple_parallel_for_c/main.h
                ${PROJECT_SOURCE_DIR}/include/simple_parallel_for_c/omp_dynamic_schedule.h
                ${PROJECT_SOURCE_DIR}/include/simple_parallel_for_c/simple_parallel_for_c.h
                ${PROJECT_BINARY_DIR}/configured_files/include/internal_use_only/c_omp_dynamic_schedule_buffer_size_config.h)


target_link_libraries(
    simple_parallel_for_c
    PRIVATE $<BUILD_INTERFACE:simple_parallel::simple_parallel_options>
            $<BUILD_INTERFACE:simple_parallel::simple_parallel_warnings>)

target_link_system_libraries(
    simple_parallel_for_c 
    PUBLIC  simple_parallel
            OpenMP::OpenMP_C
            MPI::MPI_C)

install(TARGETS simple_parallel_for_c
        EXPORT  simple_parallelTargets
        OBJECTS DESTINATION ${CMAKE_INSTALL_LIBDIR}
        FILE_SET HEADERS)