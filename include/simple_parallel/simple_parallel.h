#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <gsl/util>
#include <mpi.h>
#include <simple_parallel/advance.h>
#include <simple_parallel/mpi_util.h>

namespace simple_parallel {

    auto
    init(int (*virtual_main)(int, char**), int argc, char** argv, bool init_mpi)
        -> void;

    template <typename T>
    auto run_lambda(T lambda, bool parallel_run = true) -> void {
        assert(MPI::COMM_WORLD.Get_rank() == 0);

        // in some cases, we want to run the lambda only on the master process
        if (MPI::COMM_WORLD.Get_size() == 1 || !parallel_run) {
            lambda();
            return;
        }

        std::function<void()> f = lambda;
        advance::broadcast_stack_and_heap();

        using function                    = std::function<void()>;
        function* pointer_to_std_function = &f;

        // tell worker processes to run the lambda
        mpi_util::broadcast_tag(mpi_util::tag_enum::run_lambda);

        MPI::COMM_WORLD.Bcast(
            &pointer_to_std_function, sizeof(function*), MPI::BYTE, 0);

        f();
    }
} // namespace simple_parallel

#define SIMPLE_PARALLEL_BEGIN(_parallel_run)                                   \
    {                                                                          \
        const bool simple_parallel_run =                                       \
            MPI::COMM_WORLD.Get_size() != 1 && (_parallel_run);                \
        simple_parallel::run_lambda([&] {                                      \
            int s_p_start_index;

#define SIMPLE_PARALLEL_END                                                    \
        }, simple_parallel_run);                                               \
    }

#define SIMPLE_PARALLEL_OMP_DYNAMIC_SCEDULE_BEGIN(                             \
    _start_index, _end_index, _grain_size)                                     \
    {                                                                          \
        int simple_parallel_start_index = _start_index;                        \
        int simple_parallel_end_index   = _end_index;                          \
        int simple_parallel_grain_size  = _grain_size;                         \
        int simple_parallel_progress    = _start_index;                        \
                                                                               \
        MPI::Win window;                                                       \
        _Pragma("omp masked")                                                  \
        if (simple_parallel_run) {                                             \
            window = MPI::Win::Create(&simple_parallel_progress,               \
                                      sizeof(int),                             \
                                      sizeof(int),                             \
                                      MPI::INFO_NULL,                          \
                                      MPI::COMM_WORLD);                        \
            window.Fence(0);                                                   \
        }                                                                      \
                                                                               \
        _Pragma("omp masked")                                                  \
        s_p_start_index = simple_parallel_start_index;                         \
        while (true) {                                                         \
            _Pragma("omp masked")                                              \
            if (simple_parallel_run) {                                         \
                window.Lock(MPI::LOCK_SHARED, 0, 0);                           \
                MPI_Fetch_and_op(&simple_parallel_grain_size,                  \
                                 &s_p_start_index,                             \
                                 MPI_INT,                                      \
                                 0,                                            \
                                 0,                                            \
                                 MPI_SUM,                                      \
                                 window);                                      \
                window.Unlock(0);                                              \
            }                                                                  \
            _Pragma("omp barrier")                                             \
            if (s_p_start_index >= simple_parallel_end_index) {                \
                break;                                                         \
            }                                                                  \
            int s_p_end_index =                                                \
                std::min(s_p_start_index + simple_parallel_grain_size,         \
                         simple_parallel_end_index);

#define SIMPLE_PARALLEL_OMP_DYNAMIC_SCEDULE_END                                \
            _Pragma("omp masked")                                              \
            if (!simple_parallel_run) {                                        \
                s_p_start_index += simple_parallel_grain_size;                 \
            }                                                                  \
            _Pragma("omp barrier")                                             \
        }                                                                      \
        _Pragma("omp masked")                                                  \
        if (simple_parallel_run) {                                             \
            window.Fence(0);                                                   \
            window.Free();                                                     \
        }                                                                      \
    }
