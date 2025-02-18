#pragma once

#include <boost/preprocessor.hpp>
#include <mpi.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

#define __S_P_COMMA_SEP(r, data, i, elem)                                      \
  BOOST_PP_COMMA_IF(i) {                                                       \
    _Generic((BOOST_PP_TUPLE_ELEM(0, elem)),                                   \
        float *: MPI_FLOAT,                                                    \
        double *: MPI_DOUBLE,                                                  \
        bool *: MPI_C_BOOL,                                                    \
        int8_t *: MPI_INT8_T,                                                  \
        int16_t *: MPI_INT16_T,                                                \
        int32_t *: MPI_INT32_T,                                                \
        int64_t *: MPI_INT64_T,                                                \
        uint8_t *: MPI_UINT8_T,                                                \
        uint16_t *: MPI_UINT16_T,                                              \
        uint32_t *: MPI_UINT32_T,                                              \
        uint64_t *: MPI_UINT64_T),                                             \
        BOOST_PP_TUPLE_ENUM(elem)                                              \
  }

// __VA_OPT__ is a c23 feature but supported in gcc 8+ and clang 12+
// https://godbolt.org/z/aP7PqKKfd
#define S_P_C_PAR_BEGIN(enter_parallel, ctx_name, ...)                         \
  {                                                                            \
    s_p_reduce_area _s_p_reduces[] = {__VA_OPT__(BOOST_PP_SEQ_FOR_EACH_I(      \
        __S_P_COMMA_SEP, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)))};          \
    s_p_par_ctx ctx_name =                                                     \
        s_p_construct_par_ctx(enter_parallel, _s_p_reduces,                    \
                              sizeof(_s_p_reduces) / sizeof(s_p_reduce_area));

#define S_P_C_PAR_END(ctx_name)                                                \
    s_p_destruct_par_ctx(&(ctx_name));                                         \
  }
