#include <array>
#include <boost/mpi/communicator.hpp>
#include <cppcoro/generator.hpp>
#include <cstddef>
#include <list>
#include <memory>
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
#include <omp.h>
#include <stdio.h>
#include <vector>

cppcoro::generator<size_t> my_gen() {
    for (size_t i = 0; i < 100000; i++) {
        co_yield i;
    }
}

int main(void) {
    size_t a, i;

    std::vector<int> test{1, 2, 3, 4, 5};

    std::unique_ptr<
        simple_parallel::detail::dynamic_schedule<decltype(my_gen())>>
        schedule;


    SIMPLE_PARALLEL_BEGIN(true)
#pragma omp parallel
    {
        // for (int i : test) {
        //     std::cout << "rank:" << simple_parallel::detail::comm.rank()
        //               << test[0] << test[1] << test[2] << "\n";
        // }
#pragma omp masked
        {
            a = 0;
            std::cout << "num_threads on rank " << bmpi::communicator{}.rank()
                      << "is " << omp_get_num_threads() << "\n";
            bmpi::communicator{}.barrier();
        }
#pragma omp barrier

        // To avoid race conditions, add a barrier here.

#pragma omp masked
        {
            schedule = std::make_unique<
                simple_parallel::detail::dynamic_schedule<decltype(my_gen())>>(
                simple_parallel::detail::comm, my_gen(), omp_get_num_threads());
        }

#pragma omp barrier
        size_t loc = 0;
        auto   gen = schedule->gen();

        for (auto i : gen) {
            std::cout << i << " ";
            loc += i;
        }
#pragma omp critical
        a += loc;
#pragma omp barrier
#pragma omp masked
        { printf("Sum is %zu\n", a); }
    }
    MPI_Allreduce(MPI_IN_PLACE, &a, 1, MPI_INT64_T, MPI_SUM, MPI_COMM_WORLD);
    printf("Final sum is %zu\n", a);
    SIMPLE_PARALLEL_END
    return 0;
}
