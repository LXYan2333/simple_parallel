#include <fmt/core.h>
#include <fmt/ranges.h>
#include <mpi.h>
#include <simple_parallel/main.h>
#include <simple_parallel/simple_parallel.h>

auto main() -> int {

    fmt::print("=================\nstart reduce\n=================\n");
    int reduce_result{};
    simple_parallel::dynamic_schedule_reduce(
        0,
        110,
        20,
        [] { return 0; },
        [](const int& start, const int& end, const int& identity) -> int {
            fmt::println("rank{} start {} end {} identity {}",
                         MPI::COMM_WORLD.Get_rank(),
                         start,
                         end,
                         identity);
            return identity + (start + end - 1) * (end - start) / 2;
        },
        [&](const int& identity) {
            fmt::println("sum is {}", identity);
            MPI::COMM_WORLD.Reduce(
                &identity, &reduce_result, 1, MPI::INT, MPI::SUM, 0);
            return identity;
        });

    fmt::println("reduce result is {}", reduce_result);


    // original from OpenMP's example
    // https://github.com/OpenMP/Examples/blob/main/data_environment/sources/reduction.6.c

    /*
     * @@name:	    reduction.6
     * @@type:	    C
     * @@operation:	run
     * @@expect:	unspecified
     * @@version:	omp_5.1
     */


    int b = 0;

    const bool run_in_parallel = true;

    SIMPLE_PARALLEL_BEGIN(run_in_parallel)
    int a, i;

#pragma omp parallel shared(a) private(i)
    {
#pragma omp masked
        a = 0;

        // To avoid race conditions, add a barrier here.

        SIMPLE_PARALLEL_OMP_DYNAMIC_SCEDULE_BEGIN(0, 100, 100)
#pragma omp for reduction(+ : a)
        for (i = s_p_start_index; i < s_p_end_index; i++) {
            a += i;
        }
        SIMPLE_PARALLEL_OMP_DYNAMIC_SCEDULE_END

#pragma omp single
        fmt::print("Sum is {}\n", a);
    }
    SIMPLE_PARALLEL_END
    return 0;
}
