      end do
   end do
!$omp barrier

!$omp single
   call s_p_parallel_scheduler_ptr%del()
!$omp end single
end block