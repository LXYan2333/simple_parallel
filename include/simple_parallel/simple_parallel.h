#pragma once

#include <boost/mpi.hpp>
#include <cassert>
#include <functional>
#include <simple_parallel/detail.h>
#include <simple_parallel/master.h>
#include <simple_parallel/mpi_util.h>
#include <type_traits>

// if you have a thread that you wish not to be proxied by simple_parallel, set
// this thread_local variable to false before you try to malloc/new anything.
extern __thread bool s_p_this_thread_should_be_proxied;

#include <simple_parallel/dynamic_schedule.h>

namespace simple_parallel {

    auto init(std::move_only_function<void()> call_after_init) -> void;

    template <typename T>
        requires std::is_invocable_v<T>
    auto run_invocable(T invocable, bool parallel_run = true) -> void {
        assert(detail::comm.rank() == 0);

        // in some cases, we want to run the lambda only on the master process
        if (detail::comm.size() == 1 || !parallel_run) {
            invocable();
            return;
        }


        master::run_std_function_on_all_nodes([&] { invocable(); });
    }
} // namespace simple_parallel

// clang-format off
#define SIMPLE_PARALLEL_BEGIN(_parallel_run)                                   \
    {                                                                          \
        const bool simple_parallel_run =                                       \
            boost::mpi::communicator{}.size() != 1 && (_parallel_run);         \
        simple_parallel::run_invocable([&] {                                   \
            int s_p_start_index;

#define SIMPLE_PARALLEL_END                                                    \
        }, simple_parallel_run);                                               \
    }
// clang-format on
