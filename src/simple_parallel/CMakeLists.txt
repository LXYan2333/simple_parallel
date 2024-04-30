aux_source_directory(. source)
add_library(simple_parallel ${source})
add_library(simple_parallel::simple_parallel ALIAS simple_parallel)

try_compile(
    LIBC_SUPPORTS_MAP_FIXED_NOREPLACE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/src/try_compile/map_fixed_noreplace.c)
if(NOT LIBC_SUPPORTS_MAP_FIXED_NOREPLACE)
    message(FATAL_ERROR "Your libc does not support MAP_FIXED_NOREPLACE as mmap parameter. Please use a newer libc (e.g. glibc 2.29+) which supports this feature.")
endif()

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
            Microsoft.GSL::GSL)  

install(TARGETS mimalloc
        EXPORT  simple_parallelTargets
        ARCHIVE DESTINATION lib/mimalloc_for_simple_parallel)