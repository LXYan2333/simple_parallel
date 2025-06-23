add_library(s_p_fortran_binding
    fortran_binding.cc
    fortran_dynamic_schedule_binding.cc
    fortran_binding.f90
    fortran_dynamic_schedule_binding.f90
)

add_library(simple_parallel::s_p_fortran_binding ALIAS s_p_fortran_binding)

target_link_libraries(s_p_fortran_binding
    PUBLIC
    MPI::MPI_Fortran
    s_p_init
)

include(GNUInstallDirs)

target_include_directories(s_p_fortran_binding
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/mods/simple_parallel>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/simple_parallel/fortran>
)

target_sources(s_p_fortran_binding
    PUBLIC
    FILE_SET public_header
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
    ${PROJECT_SOURCE_DIR}/include/simple_parallel/fortran/gss_dynamic_schedule_begin.h
    ${PROJECT_SOURCE_DIR}/include/simple_parallel/fortran/gss_dynamic_schedule_end.h
    ${PROJECT_SOURCE_DIR}/include/simple_parallel/fortran/parallel_begin.h
    ${PROJECT_SOURCE_DIR}/include/simple_parallel/fortran/parallel_end.h
)

if(SIMPLE_PARALLEL_ENABLE_FLANG_ASSUMED_RANK)
    target_compile_options(s_p_fortran_binding
        PUBLIC
        # https://github.com/llvm/llvm-project/issues/95990#issuecomment-2179468321
        $<$<COMPILE_LANGUAGE:Fortran>:-mmlir -allow-assumed-rank>
    )
endif()

set_target_properties(s_p_fortran_binding
    PROPERTIES
    Fortran_MODULE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/mods/simple_parallel/
    INTERFACE_LINK_LIBRARIES_DIRECT $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:simple_parallel::mimalloc-obj>
)

# enable preprocessor
target_compile_options(s_p_fortran_binding INTERFACE $<$<COMPILE_LANGUAGE:Fortran>:-cpp>)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/mods/simple_parallel/simple_parallel.mod
    ${CMAKE_CURRENT_BINARY_DIR}/mods/simple_parallel/simple_parallel_dynamic_schedule.mod
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/simple_parallel/fortran
)

