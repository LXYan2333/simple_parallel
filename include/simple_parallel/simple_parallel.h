#pragma once

#include <cassert>
#include <functional>
#include <gsl/util>
#include <mpi.h>
#include <simple_parallel/advance.h>
#include <simple_parallel/mpi_util.h>

namespace simple_parallel {

    auto init(int (*virtual_main)(int, char**), int argc, char** argv) -> void;

    template <typename T>
    auto run_lambda(T lambda, bool parallel_run = true) -> void {
        assert(boost::mpi::communicator{}.rank() == 0);

        // in some cases, we want to run the lambda only on the master process
        if (boost::mpi::communicator{}.size() == 1 || !parallel_run) {
            lambda();
            return;
        }

        std::function<void()> f = lambda;
        advance::broadcast_stack_and_heap();

        using function = std::function<void()>;

        function* pointer_to_std_function = &f;

        // tell worker processes to run the lambda
        mpi_util::broadcast_tag(mpi_util::tag_enum::run_lambda);

        MPI_Bcast(&pointer_to_std_function,
                  sizeof(function*),
                  MPI_BYTE,
                  0,
                  MPI_COMM_WORLD);

        f();
    }
} // namespace simple_parallel

// clang-format off
#define SIMPLE_PARALLEL_BEGIN(_parallel_run)                                   \
    {                                                                          \
        const bool simple_parallel_run =                                       \
            boost::mpi::communicator{}.size() != 1 && (_parallel_run);         \
        simple_parallel::run_lambda([&] {                                      \
            int s_p_start_index;

#define SIMPLE_PARALLEL_END                                                    \
        }, simple_parallel_run);                                               \
    }
// clang-format on
