#pragma once

#include <mpi.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct par_ctx_c_binding;

typedef struct s_p_reduce_area_s {
  MPI_Datatype type;
  void *begin;
  size_t count;
  MPI_Op op;
} s_p_reduce_area;

typedef struct s_p_par_ctx_s {
  MPI_Comm comm;
  struct par_ctx_c_binding *detail;
} s_p_par_ctx;

__attribute__((visibility("default"))) s_p_par_ctx
s_p_construct_par_ctx(bool enter_parallel, s_p_reduce_area *reduce_areas,
                      size_t reduce_areas_num);

__attribute__((visibility("default"))) void
s_p_destruct_par_ctx(s_p_par_ctx *ctx);

#ifdef __cplusplus
}
#endif