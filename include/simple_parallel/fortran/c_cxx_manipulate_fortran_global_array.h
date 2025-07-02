#pragma once

#include <mpi.h>
#include <simple_parallel/cxx/lib_visibility.h>

#ifdef __cplusplus
extern "C" {
#endif

S_P_LIB_PUBLIC void fortran_global_array_set_zero(const char *module_name,
                                                  const char *array_name);

S_P_LIB_PUBLIC void fortran_global_array_reduce(const char *module_name,
                                                const char *array_name,
                                                MPI_Op op, MPI_Comm comm,
                                                int root_rank);

#ifdef __cplusplus
}
#endif