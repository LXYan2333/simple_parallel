#ifndef S_P_MATRIX_MODULE
#error "name of the module which contains the matrix to manipulate is not defined!
#endif

#ifndef S_P_MATRIX
#error "matrix to manipulate not defined!
#endif

subroutine s_p_mpi_reduce_/**/S_P_MATRIX_MODULE/**/_/**/S_P_MATRIX/**/(comm_int,root_rank,datatype_int,op_int) bind(c)
    use::S_P_MATRIX_MODULE,only:S_P_MATRIX
    use::mpi_f08
    use,intrinsic::iso_c_binding

    implicit none

    integer(c_int),value::comm_int,root_rank,datatype_int,op_int
    integer::rank,matrix_size
    
    type(MPI_Comm)::comm
    type(MPI_Datatype)::datatype
    type(MPI_Op)::op

    comm%MPI_VAL = comm_int
    datatype%MPI_VAL = datatype_int
    op%MPI_VAL = op_int
    call MPI_Comm_Rank(comm,rank)
    matrix_size = size(S_P_MATRIX)

    if (.not. allocated(S_P_MATRIX)) then
        return
    endif

    if (root_rank .eq. rank) then
        call MPI_Reduce(MPI_IN_PLACE,S_P_MATRIX,matrix_size,datatype,op,root_rank,comm)
    else
        call MPI_Reduce(S_P_MATRIX,0,matrix_size,datatype,op,root_rank,comm)
    endif
    
    
end subroutine s_p_mpi_reduce_/**/S_P_MATRIX_MODULE/**/_/**/S_P_MATRIX/**/

#undef S_P_MATRIX
#undef S_P_MATRIX_MODULE