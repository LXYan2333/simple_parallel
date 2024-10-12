#pragma once

#include <mpi.h>
#include <simple_parallel_for_c/lambda.h>
#include <stdbool.h>
#ifdef _OPENMP
    #include <omp.h>
#endif

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

// clang-format off
#ifdef _OPENMP
#define SIMPLE_PARALLEL_C_BEGIN(_parallel_run)                                 \
    {                                                                          \
        int _s_p_mpi_size;                                                     \
        MPI_Comm_size(MPI_COMM_WORLD, &_s_p_mpi_size);                         \
        const bool simple_parallel_run = _s_p_mpi_size != 1 && (_parallel_run);\
        SIMPLE_PARALLEL_LAMBDA(simple_parallel_lambda_tag, void) {             \
            MPI_Comm s_p_comm_to_dup = simple_parallel_run ? MPI_COMM_WORLD    \
                                                           : MPI_COMM_SELF;    \
            MPI_Comm s_p_comm;                                                 \
            MPI_Comm_dup(s_p_comm_to_dup, &s_p_comm);                          \
            int s_p_omp_num_threads = omp_get_max_threads();                   \
            MPI_Bcast(&s_p_omp_num_threads, 1, MPI_INT, 0, s_p_comm);          \
            omp_set_num_threads(s_p_omp_num_threads);                              
#else
#define SIMPLE_PARALLEL_C_BEGIN(_parallel_run)                                 \
    {                                                                          \
        int _s_p_mpi_size;                                                     \
        MPI_Comm_size(MPI_COMM_WORLD, &_s_p_mpi_size);                         \
        const bool simple_parallel_run = _s_p_mpi_size != 1 && (_parallel_run);\
        SIMPLE_PARALLEL_LAMBDA(simple_parallel_lambda_tag, void) {             \
            MPI_Comm s_p_comm_to_dup = simple_parallel_run ? MPI_COMM_WORLD    \
                                                           : MPI_COMM_SELF;    \
            MPI_Comm s_p_comm;                                                 \
            MPI_Comm_dup(s_p_comm_to_dup, &s_p_comm);                          
#endif

#define SIMPLE_PARALLEL_C_END                                                  \
            MPI_Comm_free(&s_p_comm);                                          \
        };                                                                     \
        simple_parallel_run_invocable(&simple_parallel_lambda_tag,             \
                                   simple_parallel_run);                       \
    };
// clang-format on
