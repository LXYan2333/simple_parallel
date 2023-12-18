aux_source_directory(. source)
add_library(simple_parallel ${source})
add_library(simple_parallel::simple_parallel ALIAS simple_parallel)

target_include_directories(
    simple_parallel 
    PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include"
    PUBLIC  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_link_libraries(
    simple_parallel
    PRIVATE simple_parallel::simple_parallel_options
            simple_parallel::simple_parallel_warnings)

target_link_system_libraries(
    simple_parallel 
    PUBLIC  MPI::MPI_CXX
            mimalloc
            Microsoft.GSL::GSL)

install(TARGETS simple_parallel
                simple_parallel_options
                simple_parallel_warnings
        EXPORT  simple_parallelTargets
        LIBRARY     DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE     DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME     DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/simple_parallel 
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

