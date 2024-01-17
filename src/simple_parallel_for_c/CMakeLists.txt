aux_source_directory(. source)
add_library(simple_parallel_for_c ${source})
add_library(simple_parallel::simple_parallel_for_c ALIAS simple_parallel_for_c)

# if using clang, enable blocks extension
# maybe https://cmake.org/cmake/help/latest/command/try_compile.html is a better solution.
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(
        simple_parallel_for_c 
        PUBLIC  -fblocks)
    target_link_options(
        simple_parallel_for_c 
        PUBLIC  -lBlocksRuntime)
endif()

target_include_directories(
    simple_parallel_for_c 
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/configured_files/include>
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
            $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>)

target_link_libraries(
    simple_parallel_for_c
    PRIVATE $<BUILD_INTERFACE:simple_parallel::simple_parallel_options>
            $<BUILD_INTERFACE:simple_parallel::simple_parallel_warnings>)

target_link_system_libraries(
    simple_parallel_for_c 
    PUBLIC  $<BUILD_INTERFACE:MPI::MPI_C>
            $<BUILD_INTERFACE:mimalloc>
            $<BUILD_INTERFACE:simple_parallel>)

install(TARGETS simple_parallel_for_c
        EXPORT  simple_parallelTargets)