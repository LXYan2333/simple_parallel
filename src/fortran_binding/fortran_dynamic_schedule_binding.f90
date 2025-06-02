module simple_parallel_dynamic_schedule
   use iso_c_binding
   use mpi_f08
   implicit none

   private
   public optional_buffer,generator_base,parallel_scheduler,gss_scheduler

   type,bind(c)::optional_buffer
      logical(c_bool)::has_value
      integer(c_int64_t)::buffer(8)
   end type optional_buffer

   type,abstract::generator_base
   contains
      procedure(next),deferred::generator_next
   end type generator_base

   type,extends(generator_base)::parallel_scheduler
      type(c_ptr),private::detail = c_null_ptr
      type(optional_buffer),pointer,private::buffer_loc
      class(generator_base),allocatable,public::generator
   contains
      procedure::generator_next => parallel_scheduler_next
      procedure::init => parallel_scheduler_init
      procedure::del => parallel_scheduler_del
   end type parallel_scheduler

   type,extends(generator_base)::gss_scheduler
      integer(c_int64_t) current,end,thread_count,grainsize
   contains
      procedure::gss_init
      procedure::generator_next => gss_next
   end type gss_scheduler

   abstract interface
      function next(self) result(res)
         import generator_base
         import optional_buffer
         class(generator_base),intent(inout)::self
         type(optional_buffer)::res
      end function next
   end interface

   interface
      function s_p_f_get_detail(scheduler,comm,buffer_size) bind(c) result(res)
         use,intrinsic::iso_c_binding
         type(c_ptr),intent(in),value::scheduler
         integer(c_int),value::comm
         integer(c_int),value::buffer_size
         type(c_ptr)::res
      end function s_p_f_get_detail

      subroutine s_p_f_delete_detail(detail) bind(c)
         use,intrinsic::iso_c_binding
         type(c_ptr),intent(in),value::detail
      end subroutine s_p_f_delete_detail

      function s_p_f_get_buffer(detail) bind(c) result(res)
         use,intrinsic::iso_c_binding
         type(c_ptr),intent(in),value::detail
         type(c_ptr)::res
      end function s_p_f_get_buffer

      function s_p_f_done(detail) bind(c) result(res)
         use,intrinsic::iso_c_binding
         type(c_ptr),intent(in),value::detail
         logical(c_bool)::res
      end function s_p_f_done

      subroutine s_p_f_parallel_scheduler_advance(detail) bind(c)
         use,intrinsic::iso_c_binding
         type(c_ptr),intent(in),value::detail
      end subroutine s_p_f_parallel_scheduler_advance
   end interface

contains

   function parallel_scheduler_next(self) result(res)
      class(parallel_scheduler),intent(inout)::self
      type(optional_buffer)::res


      res = self%buffer_loc
      if (s_p_f_done(self%detail)) then
         res%has_value = .false.
      else
         call s_p_f_parallel_scheduler_advance(self%detail)
      end if
   end function parallel_scheduler_next

   subroutine parallel_scheduler_init(self,origin_generator,comm,buffer_size)
      class(parallel_scheduler),intent(inout),target::self
      class(generator_base),intent(in)::origin_generator
      type(MPI_Comm),intent(in)::comm
      integer::buffer_size

      select type(self)
       type is(parallel_scheduler)
         self%generator = origin_generator
         self%detail = s_p_f_get_detail(c_loc(self),comm%MPI_VAL,buffer_size)
         call c_f_pointer(s_p_f_get_buffer(self%detail),self%buffer_loc)
       class default
         print*, 'Error: unknown type'
         error stop
      end select
   end subroutine parallel_scheduler_init

   subroutine parallel_scheduler_del(self)
      class(parallel_scheduler),intent(inout),target::self

      select type(self)
       type is(parallel_scheduler)
         call s_p_f_delete_detail(self%detail)
       class default
         print*, 'Error: unknown type'
         error stop
      end select

   end subroutine parallel_scheduler_del

   function s_p_scheduler_get_next(generator) bind(c) result(res)
      use,intrinsic::iso_c_binding
      type(c_ptr),value::generator
      type(optional_buffer)::res
      type(parallel_scheduler),pointer::gen

      call c_f_pointer(generator,gen)
      res = gen%generator%generator_next()
   end function s_p_scheduler_get_next

   function gss_next(self) result(res)
      class(gss_scheduler),intent(inout)::self
      type(optional_buffer)::res
      integer(c_int64_t) remaining,next_count

      remaining = self%end - self%current
      if (remaining == 0) then
         res%has_value = .false.
         return
      end if

      next_count = ((remaining - 1) / self%thread_count) + 1
      next_count = max(next_count,self%grainsize)
      next_count = min(next_count,remaining)

      res%has_value = .true.
      res%buffer(1) = self%current
      res%buffer(2) = self%current + next_count

      self%current = self%current + next_count
   end function gss_next

   subroutine gss_init(self,begin,end,thread_count,grainsize)
      class(gss_scheduler)::self
      integer(c_int64_t) begin,end,thread_count,grainsize

      self%current = begin
      self%end = end
      self%thread_count = thread_count
      self%grainsize = grainsize
   end subroutine gss_init

end module simple_parallel_dynamic_schedule
