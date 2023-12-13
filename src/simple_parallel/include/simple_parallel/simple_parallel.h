#pragma once

#include <cassert>
#include <cstddef>
#include <fmt/core.h>
#include <functional>
#include <gsl/util>
#include <mpi.h>
#include <simple_parallel/mpi_util.h>

namespace simple_parallel {

    auto
    init(int (*virtual_main)(int, char**), int argc, char** argv, bool init_mpi)
        -> void;

    auto broadcast_stack_and_heap() -> bool;

    template <typename T>
    auto run_lambda(T lambda, bool parallel_run = true) -> void {
        assert(MPI::COMM_WORLD.Get_rank() == 0);

        // in some cases, we want to run the lambda only on the master process
        if (!parallel_run) {
            lambda();
            return;
        }

        std::function<void()> f = lambda;
        broadcast_stack_and_heap();

        using function                    = std::function<void()>;
        function* pointer_to_std_function = &f;
        fmt::println(stderr,
                     "master {} send lambda {:X}",
                     MPI::COMM_WORLD.Get_rank(),
                     reinterpret_cast<size_t>(pointer_to_std_function));
        // tell worker processes to run the lambda
        mpi_util::broadcast_tag(mpi_util::tag_enum::run_lambda);

        MPI::COMM_WORLD.Bcast(
            &pointer_to_std_function, sizeof(function*), MPI::BYTE, 0);

        f();
    }

    template <typename T, typename U, typename V>
    static auto dynamic_schedule_reduce(
        int start_index,
        int end_index,
        int grain_size,
        T   identity_lambda, // should return something like TLS
        U   real_body,
        V   reduce) -> void {
        assert(MPI::COMM_WORLD.Get_rank() == 0);

        using function = std::function<void(const MPI::Win&)>;

        function f = [&](const MPI::Win& window) {
            int  my_start_index{};
            auto identity = identity_lambda();

            while (true) {
                // atomically add up the progress
                MPI_Fetch_and_op(&grain_size,
                                 &my_start_index,
                                 MPI::INT,
                                 0,
                                 0,
                                 MPI::SUM,
                                 window);

                if (my_start_index < end_index) {

                    int my_end_index =
                        std::min(my_start_index + grain_size, end_index);

                    identity = real_body(
                        my_start_index, my_end_index, std::move(identity));

                } else {
                    reduce(std::move(identity));
                    break;
                }
            }
        };

        broadcast_stack_and_heap();

        // tell worker processes to reduce
        mpi_util::broadcast_tag(mpi_util::tag_enum::dynamic_schedule_reduce);

        // MPI window to store the reduce progress
        int       reduce_progress = start_index;
        MPI::Info info            = MPI::Info::Create();
        MPI::Win  window          = MPI::Win::Create(
            &reduce_progress, sizeof(int), sizeof(int), info, MPI::COMM_WORLD);
        gsl::final_action window_final_action{[&] {
            MPI::COMM_WORLD.Barrier();
            window.Free();
        }};
        window.Fence(0);

        function* pointer_to_std_function = &f;
        MPI::COMM_WORLD.Bcast(
            &pointer_to_std_function, sizeof(function*), MPI::BYTE, 0);
        f(window);
    }

    static auto print_memory_on_worker(void* ptr, size_t len_in_byte) -> void {
        assert(MPI::COMM_WORLD.Get_rank() == 0);
        mpi_util::broadcast_tag(mpi_util::tag_enum::print_memory);
        MPI::COMM_WORLD.Bcast(&ptr, sizeof(void*), MPI::BYTE, 0);
        MPI::COMM_WORLD.Bcast(&len_in_byte, sizeof(size_t), MPI::BYTE, 0);
    }
} // namespace simple_parallel
