#pragma once

#include <mpi.h>
#include <simple_parallel_for_c/lambda.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
    int simple_parallel_init(int (*virtual_main)(int, char**),
                             int    argc,
                             char** argv);

    void simple_parallel_run_invocable(void* invocable, bool parallel_run);

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
        simple_parallel_run_invocable(&simple_parallel_lambda_tag,             \
                                   simple_parallel_run);                       \
    };
