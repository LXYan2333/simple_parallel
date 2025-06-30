#ifndef s_p_comm
#error "parameter s_p_comm not defined"
#endif

#ifndef s_p_begin
#error "parameter s_p_begin not defined."
#endif

#ifndef s_p_end
#error "parameter s_p_end not defined."
#endif

#ifndef s_p_grainsize
#error "parameter s_p_grainsize not defined."
#endif

#ifndef s_p_do_index
#error "parameter s_p_do_index not defined."
#endif

block
   use::simple_parallel_dynamic_schedule
   use::mpi_f08

   integer(c_int64_t) s_p_all_thread_count

   type(parallel_scheduler),pointer::s_p_parallel_scheduler_ptr
   type(optional_buffer)::s_p_buffer
   type(parallel_scheduler),target::s_p_parallel_scheduler
   type(gss_scheduler),target::s_p_gss_gen

!$omp single
   s_p_all_thread_count = omp_get_num_threads();
   call MPI_Allreduce(MPI_IN_PLACE,s_p_all_thread_count,1,MPI_INT,MPI_SUM,s_p_comm)
   s_p_all_thread_count = s_p_all_thread_count * 40
   call s_p_gss_gen%gss_init(s_p_begin,s_p_end,s_p_all_thread_count,s_p_grainsize)
   call s_p_parallel_scheduler%init(s_p_gss_gen,s_p_comm,omp_get_num_threads())
   s_p_parallel_scheduler_ptr => s_p_parallel_scheduler
!$omp end single copyprivate (s_p_parallel_scheduler_ptr)
   do while (.true.)
!$omp critical(s_p_gen_critical)
      s_p_buffer = s_p_parallel_scheduler_ptr%generator_next()
!$omp end critical(s_p_gen_critical)
      if (.not. s_p_buffer%has_value) then
        exit
      end if
      do s_p_do_index = s_p_buffer%buffer(1),s_p_buffer%buffer(2)

#undef s_p_comm
#undef s_p_begin
#undef s_p_end
#undef s_p_grainsize
#undef s_p_do_index
