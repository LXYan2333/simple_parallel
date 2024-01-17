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

if(NOT simple_parallel_FORCE_FIND_PACKAGE)
    # suppress warning from boost
    target_include_directories(
        simple_parallel 
        SYSTEM 
        PUBLIC  $<BUILD_INTERFACE:${Boost_SOURCE_DIR}/libs/mpl/include/> 
                $<BUILD_INTERFACE:${Boost_SOURCE_DIR}/libs/mpi/include/>)
endif()

target_link_system_libraries(
    simple_parallel 
    PUBLIC  $<BUILD_INTERFACE:MPI::MPI_C>
            $<BUILD_INTERFACE:OpenMP::OpenMP_CXX>
            $<BUILD_INTERFACE:mimalloc>
            $<BUILD_INTERFACE:Microsoft.GSL::GSL>
            $<BUILD_INTERFACE:Boost::mpi>
            $<BUILD_INTERFACE:cppcoro>)
