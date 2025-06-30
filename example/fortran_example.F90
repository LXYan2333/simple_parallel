module test_module
   integer,dimension(:),allocatable::global_array
end module

subroutine main_impl
   use,intrinsic::iso_c_binding
   use::simple_parallel
   use::mpi_f08
   use::simple_parallel_dynamic_schedule
   use::omp_lib
   use::test_module

   implicit none

   integer(kind=8) count

   count = 0

#define s_p_enter_parallel .true.
#define s_p_ctx_name ctx
#define s_p_reduce_1 count,MPI_SUM
#include <simple_parallel/fortran/parallel_begin.h>

   block
      integer::rank,size
      integer(kind=8) ii

      call MPI_Comm_rank(ctx%get_comm(),rank)
      call MPI_Comm_rank(ctx%get_comm(),size)

      print *,'rank:',rank,'thread: ',omp_get_thread_num()


#define s_p_comm ctx%get_comm()
#define s_p_begin 1_c_int64_t
#define s_p_end 100000_c_int64_t
#define s_p_grainsize 4_c_int64_t
#define s_p_do_index ii
#include <simple_parallel/fortran/gss_dynamic_schedule_begin.h>

      count = count + ii

#include <simple_parallel/fortran/gss_dynamic_schedule_end.h>

   end block


   print *,'private count: ', count
   
#include <simple_parallel/fortran/parallel_end.h>

   print *,'count: ', count


end subroutine main_impl

program simple_parallel_example

   ! some compiler will place variable declared in program block in static area,
   ! so it is highly recommend to place everything into a separate subroutine
   ! and call that subroutine in program block.
   call main_impl()

end program simple_parallel_example
