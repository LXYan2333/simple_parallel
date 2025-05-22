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

end module simple_parallel
