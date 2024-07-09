#include <array>
#include <cppcoro/generator.hpp>
#include <list>
#include <mpi.h>
#include <optional>
#include <simple_parallel/detail.h>
#include <simple_parallel/dynamic_schedule.h>
#include <simple_parallel/main.h>
// #include <simple_parallel/omp_dynamic_schedule.h>
#include <simple_parallel/simple_parallel.h>

/*
 * @@name:	reduction.6
 * @@type:	C
 * @@operation:	run
 * @@expect:	unspecified
 * @@version:	omp_5.1
 */
#include <span>
#include <stdio.h>
#include <thread>
#include <utility>
#include <vector>

cppcoro::generator<int> my_gen() {
    for (int i = 0; i < 1000; i++) {
        co_yield i;
    }
}

int main(void) {
    int a, i;

    std::vector<int> test{1, 2, 3, 4, 5};

    SIMPLE_PARALLEL_BEGIN(true)
#pragma omp parallel
    {
        // for (int i : test) {
        //     std::cout << "rank:" << simple_parallel::detail::comm.rank()
        //               << test[0] << test[1] << test[2] << "\n";
        // }
#pragma omp masked
        a = 0;
#pragma omp barrier

        // To avoid race conditions, add a barrier here.

        static simple_parallel::detail::dynamic_schedule<decltype(my_gen())>
            schedule;


        // auto                                      generator = my_gen();

#pragma omp masked
        {
            schedule = {simple_parallel::detail::comm, my_gen(), 10};
            schedule.schedule();
        }
#pragma omp barrier
        int  loc = 0;
        auto gen = schedule.gen(schedule);

        for (auto i : gen) {
            std::cout << i << "\n";
            loc += i;
        }
#pragma omp critical
        a += loc;
#pragma omp barrier
#pragma omp masked
        { printf("Sum is %d\n", a); }
    }
    MPI_Allreduce(MPI_IN_PLACE, &a, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    SIMPLE_PARALLEL_END
    return 0;
}
