add_executable(cxx_example
    cxx_example.cc
)
target_link_libraries(cxx_example
    PRIVATE
    simple_parallel::s_p_init
    OpenMP::OpenMP_CXX
)

if(SIMPLE_PARALLEL_C_BINDING)
    add_executable(c_example
        c_example.c
    )
    target_link_libraries(c_example
        PRIVATE
        simple_parallel::s_p_c_binding
        OpenMP::OpenMP_C
    )
endif()

if(SIMPLE_PARALLEL_Fortran_BINDING)
    add_executable(fortran_example
        # .F90 suffix is required to apply the preprocessor.
        # -cpp can be used to apply preprocessor for .f90 suffix
        fortran_example.F90
    )
    target_link_libraries(fortran_example
        PRIVATE
        simple_parallel::s_p_fortran_binding
        OpenMP::OpenMP_Fortran
    )
endif()