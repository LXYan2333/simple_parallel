module simple_parallel
   use iso_c_binding
   use mpi_f08
   implicit none

   private
   public par_ctx

   type::par_ctx
      type(c_ptr),private::ctx = c_null_ptr
      logical,private::is_in_parallel = .false.
      logical,private::exited_parallel = .false.
   contains
      procedure::init
      procedure::add_reduce
      procedure::enter_parallel
      procedure::exit_parallel
      procedure::get_comm
      final::finalize
   end type par_ctx

   interface
      function s_p_f_new_ctx() bind(c) result(ctx)
         use,intrinsic::iso_c_binding
         type(c_ptr)::ctx
      end function s_p_f_new_ctx

      subroutine s_p_f_del_ctx(ctx) bind(c)
         use,intrinsic::iso_c_binding
         type(c_ptr),value::ctx
      end subroutine s_p_f_del_ctx

      function s_p_get_comm_from_ctx(ctx) bind(c) result(comm)
         use,intrinsic::iso_c_binding
         type(c_ptr),value::ctx
         integer(c_int)::comm
      end function s_p_get_comm_from_ctx

      subroutine s_p_f_ctx_begin_parallel(ctx) bind(c)
         use,intrinsic::iso_c_binding
         type(c_ptr),value::ctx
      end subroutine s_p_f_ctx_begin_parallel

      subroutine s_p_f_ctx_exit_parallel(ctx) bind(c)
         use,intrinsic::iso_c_binding
         type(c_ptr),value::ctx
      end subroutine s_p_f_ctx_exit_parallel

      subroutine s_p_f_ctx_add_reduce_area(ctx,reduce_array,op) bind(c)
         use,intrinsic::iso_c_binding
         type(c_ptr),value::ctx
         type(*),intent(in),dimension(..)::reduce_array
         integer(c_int),value::op
      end subroutine s_p_f_ctx_add_reduce_area

      subroutine set_matrix_zero_impl(matrix,size) bind(c)
         use,intrinsic::iso_c_binding
         type(*),dimension(..)::matrix
         integer(c_int64_t),value::size
      end subroutine set_matrix_zero_impl

      function get_matrix_mpi_datatype(matrix) result(res) bind(c)
         use,intrinsic::iso_c_binding
         type(*),dimension(..)::matrix
         integer::res
      end function get_matrix_mpi_datatype

   end interface

contains

   subroutine init(self)
      class(par_ctx)::self

      self%ctx = s_p_f_new_ctx()
   end subroutine init

   subroutine add_reduce(self,reduce_array,op)
      class(par_ctx),intent(in)::self
      type(*),intent(in),dimension(..)::reduce_array
      type(MPI_Op),intent(in)::op

      if (self%is_in_parallel) then
         print*, 'Error: cannot add reduce area while in parallel'
         error stop
      endif
      if (self%exited_parallel) then
         print*, 'Error: cannot add reduce area after exiting parallel'
         error stop
      endif

      call s_p_f_ctx_add_reduce_area(self%ctx,reduce_array,op%MPI_VAL)
   end subroutine add_reduce

   subroutine finalize(self)
      type(par_ctx),intent(inout)::self

      if (self%is_in_parallel) then
         call self%exit_parallel()
      end if
      call s_p_f_del_ctx(self%ctx)
   end subroutine finalize

   subroutine enter_parallel(self)
      class(par_ctx),intent(inout)::self

      if (self%is_in_parallel) then
         print*, 'Error: already in parallel'
         error stop
      end if
      if (self%exited_parallel) then
         print*, 'Error: cannot re-enter parallel after exiting'
         error stop
      end if
      
      self%is_in_parallel = .true.
      call s_p_f_ctx_begin_parallel(self%ctx)
   end subroutine enter_parallel

   subroutine exit_parallel(self)
      class(par_ctx),intent(inout)::self

      self%is_in_parallel = .false.
      self%exited_parallel = .true.
      call s_p_f_ctx_exit_parallel(self%ctx)
   end subroutine exit_parallel

   function get_comm(self) result(comm)
      class(par_ctx),intent(in)::self
      type(MPI_Comm)::comm

      comm%MPI_VAL = s_p_get_comm_from_ctx(self%ctx)
   end function get_comm

   ! we need to manipulate fortran global matrix from c/c++

   subroutine set_matrix_zero(matrix)
      type(*),dimension(..)::matrix

      ! due to TS29113:
      ! > This Technical Specification provides no mechanism for a Fortran procedure to
      ! > determine the actual type of an assumed-type argument.
      !
      ! so we have to do this work in C++
      call set_matrix_zero_impl(matrix,sizeof(matrix))

   end subroutine set_matrix_zero

   function get_set_matrix_zero_func() result(res) bind(c)
      type(c_funptr)::res

      res = c_funloc(set_matrix_zero)
   end function get_set_matrix_zero_func

   subroutine reduce_matrix(matrix,op_int,comm_int,root_rank)
      type(*),dimension(..)::matrix
      integer,value::op_int,comm_int,root_rank

      integer::rank

      type(MPI_Comm)::comm
      type(MPI_Datatype)::datatype
      type(MPI_Op)::op

      comm%MPI_VAL = comm_int
      datatype%MPI_VAL = get_matrix_mpi_datatype(matrix)
      op%MPI_VAL = op_int
      call MPI_Comm_Rank(comm,rank)

      if (root_rank .eq. rank) then
         call MPI_Reduce(MPI_IN_PLACE,matrix,size(matrix),datatype,op,root_rank,comm)
      else
         call MPI_Reduce(matrix,0,size(matrix),datatype,op,root_rank,comm)
      endif

   end subroutine reduce_matrix

   function get_reduce_matrix_func() result(res) bind(c)
      type(c_funptr)::res

      res = c_funloc(reduce_matrix)
   end function get_reduce_matrix_func

end module simple_parallel
