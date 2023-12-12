#pragma once

#include <simple_parallel_for_c/lambda.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
    void simple_parallel_init(int (*virtual_main)(int, char**),
                              int    argc,
                              char** argv,
                              bool   init_mpi);

    void simple_parallel_run_lambda(void* lambda, bool run_on_master);

#ifdef __cplusplus
}
#endif

#define SIMPLE_PARALLEL_BEGIN                                                  \
    {                                                                          \
        SIMPLE_PARALLEL_LAMBDA(simple_parallel_lambda_tag, void) {             \
            int s_p_start_index;                                               \
            int s_p_end_index;

#define SIMPLE_PARALLEL_END                                                    \
    }                                                                          \
    ;                                                                          \
    simple_parallel_run_lambda(&simple_parallel_lambda_tag, true);             \
    }                                                                          \
    ;

#define SIMPLE_PARALLEL_OMP_DYNAMIC_SCEDULE_BEGIN(                             \
    _start_index, _end_index, _grain_size)                                     \
    {                                                                          \
        int     simple_parallel_start_index = _start_index;                    \
        int     simple_parallel_end_index   = _end_index;                      \
        int     simple_parallel_grain_size  = _grain_size;                     \
        int     simple_parallel_progress    = _start_index;                    \
        MPI_Win win;                                                           \
        _Pragma("omp masked") {                                                \
            MPI_Win_create(&simple_parallel_progress,                          \
                           sizeof(int),                                        \
                           sizeof(int),                                        \
                           MPI_INFO_NULL,                                      \
                           MPI_COMM_WORLD,                                     \
                           &win);                                              \
            MPI_Win_fence(0, win);                                             \
        }                                                                      \
        while (true) {                                                         \
            _Pragma("omp masked")                                              \
                MPI_Fetch_and_op(&simple_parallel_grain_size,                  \
                                 &s_p_start_index,                             \
                                 MPI_INT,                                      \
                                 0,                                            \
                                 0,                                            \
                                 MPI_SUM,                                      \
                                 win);                                         \
            _Pragma("omp barrier") if (s_p_start_index                         \
                                       >= simple_parallel_end_index) {         \
                break;                                                         \
            }                                                                  \
            s_p_end_index =                                                    \
                s_p_start_index + simple_parallel_grain_size                   \
                        > simple_parallel_end_index                            \
                    ? simple_parallel_end_index                                \
                    : s_p_start_index + simple_parallel_grain_size;

#define SIMPLE_PARALLEL_OMP_DYNAMIC_SCEDULE_END                                \
    }                                                                          \
    _Pragma("omp masked") {                                                    \
        MPI_Barrier(MPI_COMM_WORLD);                                           \
        MPI_Win_free(&win);                                                    \
    }                                                                          \
    }
