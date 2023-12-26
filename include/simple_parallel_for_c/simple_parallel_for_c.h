#pragma once

#include <mpi.h>
#include <simple_parallel_for_c/lambda.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
    void simple_parallel_init(int (*virtual_main)(int, char**),
                              int    argc,
                              char** argv,
                              bool   init_mpi);

    void simple_parallel_run_lambda(void* lambda, bool parallel_run);

#ifdef __cplusplus
}
#endif

#define SIMPLE_PARALLEL_C_BEGIN(_parallel_run)                                 \
    {                                                                          \
        int _s_p_mpi_size;                                                     \
        MPI_Comm_size(MPI_COMM_WORLD, &_s_p_mpi_size);                         \
        const bool simple_parallel_run = _s_p_mpi_size != 1 && (_parallel_run);\
        SIMPLE_PARALLEL_LAMBDA(simple_parallel_lambda_tag, void) {             \
            int s_p_start_index;

#define SIMPLE_PARALLEL_C_END                                                  \
        };                                                                     \
        simple_parallel_run_lambda(&simple_parallel_lambda_tag,                \
                                   simple_parallel_run);                       \
    };

#define SIMPLE_PARALLEL_OMP_DYNAMIC_SCEDULE_C_BEGIN(                           \
    _start_index, _end_index, _grain_size)                                     \
    {                                                                          \
        int simple_parallel_start_index = _start_index;                        \
        int simple_parallel_end_index   = _end_index;                          \
        int simple_parallel_grain_size  = _grain_size;                         \
        int simple_parallel_progress    = _start_index;                        \
                                                                               \
        MPI_Win window;                                                        \
                                                                               \
        _Pragma("omp masked")                                                  \
        if (simple_parallel_run) {                                             \
            MPI_Win_create(&simple_parallel_progress,                          \
                           sizeof(int),                                        \
                           sizeof(int),                                        \
                           MPI_INFO_NULL,                                      \
                           MPI_COMM_WORLD,                                     \
                           &window);                                           \
            MPI_Win_fence(0, window);                                          \
        }                                                                      \
        _Pragma("omp masked")                                                  \
        s_p_start_index = simple_parallel_start_index;                         \
        while (true) {                                                         \
            _Pragma("omp masked")                                              \
            if (simple_parallel_run) {                                         \
                MPI_Win_lock(MPI_LOCK_SHARED, 0, 0, window);                   \
                MPI_Fetch_and_op(&simple_parallel_grain_size,                  \
                                 &s_p_start_index,                             \
                                 MPI_INT,                                      \
                                 0,                                            \
                                 0,                                            \
                                 MPI_SUM,                                      \
                                 window);                                      \
                MPI_Win_unlock(0,window);                                      \
            }                                                                  \
            _Pragma("omp barrier")                                             \
            if (s_p_start_index >= simple_parallel_end_index) {                \
                break;                                                         \
            }                                                                  \
            int s_p_end_index =                                                \
                s_p_start_index + simple_parallel_grain_size                   \
                        > simple_parallel_end_index                            \
                    ? simple_parallel_end_index                                \
                    : s_p_start_index + simple_parallel_grain_size;

#define SIMPLE_PARALLEL_OMP_DYNAMIC_SCEDULE_C_END                              \
            _Pragma("omp masked")                                              \
            if (!simple_parallel_run) {                                        \
                s_p_start_index += simple_parallel_grain_size;                 \
            }                                                                  \
            _Pragma("omp barrier")                                             \
        }                                                                      \
        if (simple_parallel_run) {                                             \
            _Pragma("omp masked")                                              \
            {                                                                  \
                MPI_Win_fence(0, window);                                      \
                MPI_Win_free(&window);                                         \
            }                                                                  \
        }                                                                      \
    }
