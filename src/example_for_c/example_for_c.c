#include <mpi.h>
#include <simple_parallel_for_c/lambda.h>
#include <simple_parallel_for_c/main.h>

// original from OpenMP's example
// https://github.com/OpenMP/Examples/blob/main/data_environment/sources/reduction.6.c

/*
 * @@name:	reduction.6
 * @@type:	C
 * @@operation:	run
 * @@expect:	unspecified
 * @@version:	omp_5.1
 */
#include <stdio.h>

int main(void) {

    S_P_ASSIGN int mpi_reduce_result = 0;

    const bool run_in_mpi_parallel = true;

    SIMPLE_PARALLEL_C_BEGIN(run_in_mpi_parallel)

    int a, i;

#pragma omp parallel shared(a, s_p_start_index) private(i)
    {
#pragma omp masked
        a = 0;

        // To avoid race conditions, add a barrier here.
        SIMPLE_PARALLEL_OMP_DYNAMIC_SCEDULE_C_BEGIN(0, 100, 10)
#pragma omp for reduction(+ : a)
        for (i = s_p_start_index; i < s_p_end_index; i++) {
            a += i;
        }
        SIMPLE_PARALLEL_OMP_DYNAMIC_SCEDULE_C_END
#pragma omp single
        printf("Sum is %d\n", a);
    }
    if (run_in_mpi_parallel) {
        MPI_Reduce(
            &a, &mpi_reduce_result, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    } else {
        mpi_reduce_result = a;
    }
    SIMPLE_PARALLEL_C_END
    printf("Final sum is %d\n", mpi_reduce_result);
    return 0;
}
