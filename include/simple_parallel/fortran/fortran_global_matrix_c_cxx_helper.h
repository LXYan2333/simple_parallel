#pragma once

#ifdef __cplusplus
#define S_P_C_LINKAGE extern "C"
#else
#define S_P_C_LINKAGE extern
#endif

#define s_p_zero_out(module, matrix)                                           \
  S_P_C_LINKAGE void s_p_zero_out_##module##_##matrix(bool do_zero_out);

#define s_p_mpi_reduce(module, matrix)                                         \
  S_P_C_LINKAGE void s_p_mpi_reduce_##module##_##matrix(                       \
      int fcomm, int root_rank, int fdatatype, int fop);
