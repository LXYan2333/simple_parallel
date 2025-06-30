#ifndef S_P_MATRIX
#error "matrix to manipulate not defined!
#endif

#ifndef S_P_MATRIX_MODULE
#error "name of the module which contains the matrix to manipulate is not defined!
#endif


subroutine s_p_zero_out_/**/S_P_MATRIX_MODULE/**/_/**/S_P_MATRIX/**/(zero_out) bind(c)
    use::S_P_MATRIX_MODULE,only: S_P_MATRIX
    use,intrinsic::iso_c_binding,only:c_bool

    logical(c_bool),value::zero_out

    if (zero_out) then
        S_P_MATRIX = 0
    endif
    
end subroutine s_p_zero_out_/**/S_P_MATRIX_MODULE/**/_/**/S_P_MATRIX/**/

#undef S_P_MATRIX
#undef S_P_MATRIX_MODULE