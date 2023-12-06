#pragma once

#include <simple_parallel/mpi_util.h>
#include <cassert>
#include <cstddef>
#include <fmt/core.h>
#include <functional>

namespace simple_parallel {

    auto
    init(int (*virtual_main)(int, char**), int argc, char** argv, bool init_mpi)
        -> void;

    auto broadcast_stack_and_heap() -> bool;

    template <typename T>
    auto run_lambda(T lambda, bool run_on_master = true) -> void {
        assert(MPI::COMM_WORLD.Get_rank() == 0);

        std::function<void()> f = lambda;
        void* pointer_to_std_function = &f;
        fmt::println(stderr,
                     "master {} send lambda {:X}",
                     MPI::COMM_WORLD.Get_rank(),
                     reinterpret_cast<size_t>(pointer_to_std_function));
        broadcast_stack_and_heap();
        // tell worker processes to run the lambda
        mpi_util::tag_enum run_lambda_tag = mpi_util::tag_enum::run_lambda;
        MPI::COMM_WORLD.Bcast(&run_lambda_tag, 1, MPI::INT, 0);

        MPI::COMM_WORLD.Bcast(
            &pointer_to_std_function, sizeof(void*), MPI::BYTE, 0);
        if (run_on_master) {
            f();
        }
    }

    static auto print_memory_on_worker(void* ptr, size_t len_in_byte) -> void {
        assert(MPI::COMM_WORLD.Get_rank() == 0);
        mpi_util::tag_enum print_memory_tag = mpi_util::tag_enum::print_memory;
        MPI::COMM_WORLD.Bcast(&print_memory_tag, 1, MPI::INT, 0);
        MPI::COMM_WORLD.Bcast(&ptr, sizeof(void*), MPI::BYTE, 0);
        MPI::COMM_WORLD.Bcast(&len_in_byte, sizeof(size_t), MPI::BYTE, 0);
    }
} // namespace simple_parallel
