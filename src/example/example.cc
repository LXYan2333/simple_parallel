#include <fmt/core.h>
#include <fmt/ranges.h>
#include <mpi.h>
#include <simple_parallel/main.h>
#include <simple_parallel/simple_parallel.h>

auto main() -> int {

    // original from OpenMP's example
    // https://github.com/OpenMP/Examples/blob/main/data_environment/sources/reduction.6.c

    /*
     * @@name:	    reduction.6
     * @@type:	    C
     * @@operation:	run
     * @@expect:	unspecified
     * @@version:	omp_5.1
     */


    int mpi_reduce_result = 0;

    const bool run_in_mpi_parallel = true;

    SIMPLE_PARALLEL_BEGIN(run_in_mpi_parallel)
    int a, i;

#pragma omp parallel shared(a, s_p_start_index) private(i)
    {
#pragma omp masked
        a = 0;

        // To avoid race conditions, add a barrier here.

        SIMPLE_PARALLEL_OMP_DYNAMIC_SCEDULE_BEGIN(0, 100, 10)
#pragma omp for reduction(+ : a)
        for (i = s_p_start_index; i < s_p_end_index; i++) {
            a += i;
        }
        SIMPLE_PARALLEL_OMP_DYNAMIC_SCEDULE_END

#pragma omp single
        MPI::COMM_WORLD.Reduce(
            &a, &mpi_reduce_result, 1, MPI::INT, MPI::SUM, 0);
    }
    SIMPLE_PARALLEL_END
    fmt::print("Sum is {}\n", mpi_reduce_result);
    return 0;
}
