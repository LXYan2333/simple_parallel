subroutine main_impl
   use,intrinsic::iso_c_binding
   use::simple_parallel
   use::mpi_f08
   implicit none

   integer(c_int),dimension(:),allocatable::array1
   integer::i
   type(par_ctx)::ctx
   type(MPI_Comm)::comm
   integer::rank,world_size

   allocate(array1(10))
   array1 = [(i,i=1,10)]

   call ctx%init()
   call ctx%add_reduce(array1,MPI_SUM)
   call ctx%enter_parallel()
   comm = ctx%get_comm()
   call MPI_Comm_rank(comm,rank)
   call MPI_Comm_size(comm,world_size)
   print*, 'Hello from rank ', rank, ' of ', world_size
   call printmatrix(array1)
   do i = 1,size(array1)
      array1(i) = array1(i) + 1
   end do
   print*, 'exited parallel'
   call ctx%exit_parallel()
   call printmatrix(array1)

contains
   subroutine printmatrix(matrix)
      use,intrinsic::iso_c_binding
      implicit none
      integer(c_int),dimension(:),intent(in)::matrix
      integer::i

      do i = 1,size(matrix)
         print*, matrix(i)
      end do
   end subroutine printmatrix
end subroutine main_impl

program simple_parallel_example

   ! some compiler will place variable declared in program block in static area, 
   ! so it is highly recommend to place everything into a separate subroutine 
   ! and call that subroutine in program block.
   call main_impl()

end program simple_parallel_example
