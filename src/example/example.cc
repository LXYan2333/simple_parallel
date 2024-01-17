#include <cppcoro/generator.hpp>
#include <iostream>
#include <mpi.h>
#include <omp.h>
#include <simple_parallel/main.h>
#include <simple_parallel/omp_dynamic_schedule.h>
#include <simple_parallel/simple_parallel.h>
#include <thread>

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

    int a, i;
    int b = 100;


    SIMPLE_PARALLEL_BEGIN(true)
#pragma omp parallel
    {
        SIMPLE_PARALLEL_OMP_DYNAMIC_SCHEDULE_GENERATOR
        [](int b) -> cppcoro::generator<int> {
            for (int i = 0; i < b; i++) {
                co_yield i;
            }
        }(b);
        SIMPLE_PARALLEL_OMP_DYNAMIC_SCHEDULE_PAYLOAD(20)
        std::cout << "task is " << simple_parallel_task << "on thread num"
                  << omp_get_thread_num() << "\n";
        mpi_reduce_result += simple_parallel_task;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        SIMPLE_PARALLEL_OMP_DYNAMIC_SCHEDULE_END

        // simple_parallel::omp_dynamic_schedule(
        //     3,
        //     [&] -> cppcoro::generator<int> {
        //         for (int i = 0; i < b; i++) {
        //             co_yield i;
        //         }
        //     }(),
        //     [&](const int& i) {
        //         std::cout << i << "\n";
        //         mpi_reduce_result += i;
        //     });
    }
    MPI_Allreduce(
        MPI_IN_PLACE, &mpi_reduce_result, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    SIMPLE_PARALLEL_END
    std::cout << "Sum is " << mpi_reduce_result << "\n";
    return 0;
}
