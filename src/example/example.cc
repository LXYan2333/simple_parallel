#include <cppcoro/generator.hpp>
#include <list>
#include <mpi.h>
#include <omp.h>
#include <simple_parallel/main.h>
#include <simple_parallel/omp_dynamic_schedule.h>
#include <simple_parallel/simple_parallel.h>

/*
 * @@name:	reduction.6
 * @@type:	C
 * @@operation:	run
 * @@expect:	unspecified
 * @@version:	omp_5.1
 */
#include <stdio.h>

int main(void) {
    int a, i;

    SIMPLE_PARALLEL_BEGIN(true)
#pragma omp parallel
    {
        a = 0;

// To avoid race conditions, add a barrier here.
#pragma omp barrier

        SIMPLE_PARALLEL_OMP_DYNAMIC_SCHEDULE_GENERATOR
        []() -> cppcoro::generator<int> {
            for (int i = 0; i < 10; i++) {
                co_yield i;
            }
        }();
        SIMPLE_PARALLEL_OMP_DYNAMIC_SCHEDULE_PAYLOAD(4)
        a += simple_parallel_task;
        SIMPLE_PARALLEL_OMP_DYNAMIC_SCHEDULE_END

#pragma omp single
        printf("Sum is %d\n", a);
    }
    MPI_Allreduce(MPI_IN_PLACE, &a, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    SIMPLE_PARALLEL_END
    return 0;
}
