#include <mpi.h>
#include <omp.h>
#include <simple_parallel/c/c_binding.h>
#include <simple_parallel/c/c_dynamic_schedule_binding.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

collapse_2_gss_gen_func_res_t my_gen_func(s_p_ij_point_t current,
                                          s_p_ij_point_t final_end,
                                          uint64_t expect_count,
                                          const void *state) {
  uint64_t count = 0;
  uint64_t i = current.i;
  uint64_t j = current.j;

  goto label;

  for (; i < final_end.i; i++) {
    j = 0;
  label:
    for (; j <= i; j++) {
      if (count == expect_count) {
        collapse_2_gss_gen_func_res_t ret = {{i, j}, count};
        return ret;
      }
      count++;
    }
  }

  collapse_2_gss_gen_func_res_t ret = {{i, j}, count};
  return ret;
};

int main() {

  {
    size_t *heap_test = (size_t *)malloc(1024 * sizeof(double));

    heap_test[0] = 0;

    S_P_C_PAR_BEGIN(true, ctx, (heap_test, 1024, MPI_SUM))

    int world_rank = 0;
    int world_size = 0;

    MPI_Comm_rank(ctx.comm, &world_rank);
    MPI_Comm_size(ctx.comm, &world_size);

    printf("rank: %d, size: %d\n", world_rank, world_size);

#pragma omp parallel default(shared) reduction(+ : heap_test[ : 1024])         \
    allocate(omp_large_cap_mem_alloc : heap_test)
    {
      S_P_GSS_UINT64_T_PAR_FOR(i, 0, 102400, 4, ctx.comm) {
        printf("thread: %d, i: %zu\n", omp_get_thread_num(), i);
        heap_test[0] += i;
        usleep(40);
      }
      S_P_GSS_UINT64_T_PAR_FOR_END

      printf("thread: %d, heap_test[0]: %zu\n", omp_get_thread_num(),
             heap_test[0]);
    }

    S_P_C_PAR_END(ctx)

    printf("%zu ", heap_test[0]);

    printf("\n");

    free(heap_test);
  }

  {
    size_t count = 0;

    S_P_C_PAR_BEGIN(true, ctx, (&count, 1, MPI_SUM))

    int world_rank = 0;
    int world_size = 0;

    MPI_Comm_rank(ctx.comm, &world_rank);
    MPI_Comm_size(ctx.comm, &world_size);

    printf("rank: %d, size: %d\n", world_rank, world_size);

    S_P_GSS_UINT64_T_PAR_FOR_COLLAPSE_2(4, ctx.comm, 55, i, 0, 10, j, 0, 10,
                                        my_gen_func, NULL)
    printf("[%zu, %zu]  ", i, j);
    count++;
    S_P_GSS_UINT64_T_PAR_FOR_COLLAPSE_2_END

    S_P_C_PAR_END(ctx)

    printf("\ncount:%zu\n", count);
  }

  return 0;
}