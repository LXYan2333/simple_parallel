#include <ISO_Fortran_binding.h>
#include <boost/assert.hpp>
#include <cstdbool>
#include <cstddef>
#include <gsl/gsl>
#include <map>
#include <mpi.h>
#include <simple_parallel/cxx/simple_parallel.h>
#include <sstream>

namespace sp = simple_parallel;

struct par_ctx_fortran_binding : public sp::par_ctx_base {
  std::vector<sp::reduce_area> m_reduces;
  bool m_in_parallel = false;
  bool m_ever_entered_parallel = false;

  par_ctx_fortran_binding(const par_ctx_fortran_binding &) = delete;
  par_ctx_fortran_binding(par_ctx_fortran_binding &&) = delete;
  auto operator=(const par_ctx_fortran_binding &)
      -> par_ctx_fortran_binding & = delete;
  auto operator=(par_ctx_fortran_binding &&)
      -> par_ctx_fortran_binding & = delete;

  void add_reduce_area(MPI_Datatype type, void *begin, size_t count,
                       MPI_Op op) {
    m_reduces.emplace_back(type, begin, count, op);
    set_reduces(m_reduces);
  }

  void enter_parallel(bool enter_parallel) {
    if (m_in_parallel) {
      throw std::runtime_error(
          "Error: Recursively enter Fortran parallel context again is "
          "not supported");
    }
    if (m_ever_entered_parallel) {
      throw std::runtime_error(
          "Error: Re-enter Fortran parallel context is not supported");
    }
    m_in_parallel = true;
    m_ever_entered_parallel = true;
    par_ctx_base::do_enter_parallel(enter_parallel);
  }

  void exit_parallel() {
    if (m_in_parallel) {
      m_in_parallel = false;
      par_ctx_base::do_exit_parallel();
    }
  }

  par_ctx_fortran_binding() = default;

  ~par_ctx_fortran_binding() {
    if (m_in_parallel) {
      exit_parallel();
    }
  }
};

extern "C" auto s_p_f_new_ctx() -> gsl::owner<par_ctx_fortran_binding *> {
  return new par_ctx_fortran_binding();
}

extern "C" auto s_p_get_comm_from_ctx(par_ctx_fortran_binding *ctx)
    -> MPI_Fint {
  return MPI_Comm_c2f(ctx->get_comm());
}

extern "C" void s_p_f_del_ctx(gsl::owner<par_ctx_fortran_binding *> ctx) {
  delete ctx;
}

extern "C" void s_p_f_ctx_begin_parallel(par_ctx_fortran_binding *ctx) {
  ctx->enter_parallel(true);
}

extern "C" void s_p_f_ctx_exit_parallel(par_ctx_fortran_binding *ctx) {
  ctx->exit_parallel();
}

extern "C" void s_p_f_ctx_add_reduce_area(par_ctx_fortran_binding *ctx,
                                          CFI_cdesc_t *array, MPI_Fint op) {
  static const std::map<CFI_type_t, MPI_Datatype> cfi_type_2_mpi_type{
      {CFI_type_int, MPI_INT},         {CFI_type_int8_t, MPI_INT8_T},
      {CFI_type_int16_t, MPI_INT16_T}, {CFI_type_int32_t, MPI_INT32_T},
      {CFI_type_int64_t, MPI_INT64_T}, {CFI_type_float, MPI_FLOAT},
      {CFI_type_double, MPI_DOUBLE},   {CFI_type_Bool, MPI_C_BOOL}};

  BOOST_ASSERT(array->rank >= 1);

  auto mpi_type = cfi_type_2_mpi_type.find(array->type);
  if (mpi_type == cfi_type_2_mpi_type.end()) {
    static const std::map<CFI_type_t, const char *> all_type_names{
        {CFI_type_signed_char, "CFI_type_signed_char"},
        {CFI_type_short, "CFI_type_short"},
        {CFI_type_int, "CFI_type_int"},
        {CFI_type_long, "CFI_type_long"},
        {CFI_type_long_long, "CFI_type_long_long"},
        {CFI_type_size_t, "CFI_type_size_t"},
        {CFI_type_int8_t, "CFI_type_int8_t"},
        {CFI_type_int16_t, "CFI_type_int16_t"},
        {CFI_type_int32_t, "CFI_type_int32_t"},
        {CFI_type_int64_t, "CFI_type_int64_t"},
        {CFI_type_int_least8_t, "CFI_type_int_least8_t"},
        {CFI_type_int_least16_t, "CFI_type_int_least16_t"},
        {CFI_type_int_least32_t, "CFI_type_int_least32_t"},
        {CFI_type_int_least64_t, "CFI_type_int_least64_t"},
        {CFI_type_int_fast8_t, "CFI_type_int_fast8_t"},
        {CFI_type_int_fast16_t, "CFI_type_int_fast16_t"},
        {CFI_type_int_fast32_t, "CFI_type_int_fast32_t"},
        {CFI_type_int_fast64_t, "CFI_type_int_fast64_t"},
        {CFI_type_intmax_t, "CFI_type_intmax_t"},
        {CFI_type_intptr_t, "CFI_type_intptr_t"},
        {CFI_type_ptrdiff_t, "CFI_type_ptrdiff_t"},
        {CFI_type_float, "CFI_type_float"},
        {CFI_type_double, "CFI_type_double"},
        {CFI_type_long_double, "CFI_type_long_double"},
        {CFI_type_float_Complex, "CFI_type_float_Complex"},
        {CFI_type_double_Complex, "CFI_type_double_Complex"},
        {CFI_type_long_double_Complex, "CFI_type_long_double_Complex"},
        {CFI_type_Bool, "CFI_type_Bool"},
        {CFI_type_char, "CFI_type_char"},
        {CFI_type_cptr, "CFI_type_cptr"},
        {CFI_type_struct, "CFI_type_struct"},
        {CFI_type_other, "CFI_type_other"}};
    auto name = all_type_names.find(array->type);
    std::stringstream ss;
    if (name != all_type_names.end()) {
      ss << "Error: simple_parallel does not support Fortran type "
         << *name->second << '\n';
    } else {
      ss << "Error: Unsupported Fortran type\n";
    }
    throw std::runtime_error(ss.str());
  }

  size_t len = 1;
  for (int i = 0; i < array->rank; i++) {
    len *= array->dim[i].extent;
  }

  BOOST_ASSERT(len > 0);

  ctx->add_reduce_area(mpi_type->second, array->base_addr, len, MPI_Op_f2c(op));
}