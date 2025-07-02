#include <ISO_Fortran_binding.h>
#include <boost/algorithm/string.hpp>
#include <boost/assert.hpp>
#include <cstdbool>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <gsl/gsl>
#include <map>
#include <mpi.h>
#include <simple_parallel/cxx/simple_parallel.h>
#include <simple_parallel/fortran/c_cxx_manipulate_fortran_global_array.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sp = simple_parallel;

namespace simple_parallel {

namespace {

auto ftype_2_mpi_type(CFI_type_t ftype) -> MPI_Datatype {
  static const std::map<CFI_type_t, MPI_Datatype> cfi_type_2_mpi_type{
      {CFI_type_int, MPI_INT},         {CFI_type_int8_t, MPI_INT8_T},
      {CFI_type_int16_t, MPI_INT16_T}, {CFI_type_int32_t, MPI_INT32_T},
      {CFI_type_int64_t, MPI_INT64_T}, {CFI_type_float, MPI_FLOAT},
      {CFI_type_double, MPI_DOUBLE},   {CFI_type_Bool, MPI_C_BOOL}};
  auto mpi_type = cfi_type_2_mpi_type.find(ftype);

  if (mpi_type != cfi_type_2_mpi_type.end()) {
    return mpi_type->second;
  }

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
  auto name = all_type_names.find(ftype);
  std::stringstream ss;
  if (name != all_type_names.end()) {
    ss << "Error: simple_parallel does not support Fortran type "
       << *name->second << '\n';
  } else {
    ss << "Error: Unsupported Fortran type\n";
  }
  throw std::runtime_error(std::move(ss).str());
}

struct par_ctx_fortran_binding : public par_ctx_base {
  std::vector<reduce_area> m_reduces;
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
} // namespace

} // namespace simple_parallel

extern "C" auto s_p_f_new_ctx() -> gsl::owner<sp::par_ctx_fortran_binding *> {
  return new sp::par_ctx_fortran_binding();
}

extern "C" auto s_p_get_comm_from_ctx(sp::par_ctx_fortran_binding *ctx)
    -> MPI_Fint {
  return MPI_Comm_c2f(ctx->get_comm());
}

extern "C" void s_p_f_del_ctx(gsl::owner<sp::par_ctx_fortran_binding *> ctx) {
  delete ctx;
}

extern "C" void s_p_f_ctx_begin_parallel(sp::par_ctx_fortran_binding *ctx) {
  ctx->enter_parallel(true);
}

extern "C" void s_p_f_ctx_exit_parallel(sp::par_ctx_fortran_binding *ctx) {
  ctx->exit_parallel();
}

extern "C" void s_p_f_ctx_add_reduce_area(sp::par_ctx_fortran_binding *ctx,
                                          CFI_cdesc_t *array, MPI_Fint op) {

  BOOST_ASSERT(array->rank >= 0);

  size_t len = 1;
  for (int i = 0; i < array->rank; i++) {
    len *= array->dim[i].extent;
  }

  BOOST_ASSERT(len > 0);

  ctx->add_reduce_area(sp::ftype_2_mpi_type(array->type), array->base_addr, len,
                       MPI_Op_f2c(op));
}

extern "C" void set_matrix_zero_impl(CFI_cdesc_t *matrix, int64_t size) {
  memset(matrix->base_addr, 0, size);
}

extern "C" auto get_matrix_mpi_datatype(CFI_cdesc_t *matrix) -> MPI_Fint {
  return MPI_Type_c2f(sp::ftype_2_mpi_type(matrix->type));
}

namespace simple_parallel {
namespace {

auto get_gfortran_array_sym(std::string_view module,
                            std::string_view array_name) -> std::string {

  std::stringstream sym;
  sym << "__" << module << "_MOD_" << array_name;

  return std::move(sym).str();
}

auto get_ifx_array_sym(std::string_view module, std::string_view array_name)
    -> std::string {

  std::stringstream sym;
  sym << module << "_mp_" << array_name << '_';

  return std::move(sym).str();
}

auto get_flang_array_sym(std::string_view module, std::string_view array_name)
    -> std::string {

  std::stringstream sym;
  sym << "_QM" << module << "E" << array_name;

  return std::move(sym).str();
}

auto get_array_addr(std::string module_name, std::string array_name) -> void * {
  boost::to_lower(module_name);
  boost::to_lower(array_name);

  // try gfortran name convension
  {
    std::string gfort_array_sym_name =
        sp::get_gfortran_array_sym(module_name, array_name);

    std::optional<char *> gfort_array_addr =
        sp::symbol_name_2_addr(gfort_array_sym_name);

    if (gfort_array_addr.has_value()) {
      return *gfort_array_addr;
    }
  }

  // try ifx name convension
  {
    std::string ifx_array_sym_name =
        sp::get_ifx_array_sym(module_name, array_name);

    std::optional<char *> ifx_array_addr =
        sp::symbol_name_2_addr(ifx_array_sym_name);

    if (ifx_array_addr.has_value()) {
      return *ifx_array_addr;
    }
  }

  // try flang-new name convension
  {
    std::string flang_array_sym_name =
        sp::get_flang_array_sym(module_name, array_name);

    std::optional<char *> flang_array_addr =
        sp::symbol_name_2_addr(flang_array_sym_name);

    if (flang_array_addr.has_value()) {
      return *flang_array_addr;
    }
  }

  // NOLINTNEXTLINE(*-mt-unsafe)
  throw std::runtime_error(dlerror());
}

} // namespace

} // namespace simple_parallel

extern "C" auto get_set_matrix_zero_func() -> void (*)(void *);

extern "C" void fortran_global_array_set_zero(const char *module_name,
                                              const char *array_name) {

  void *matrix_addr = sp::get_array_addr(module_name, array_name);

  static void (*set_array_zero_func)(void *) = get_set_matrix_zero_func();

  set_array_zero_func(matrix_addr);
}

extern "C" auto get_reduce_matrix_func()
    -> void (*)(void *, MPI_Fint, MPI_Fint, MPI_Fint);

extern "C" void fortran_global_array_reduce(const char *module_name,
                                            const char *array_name, MPI_Op op,
                                            MPI_Comm comm, int root_rank) {
  void *matrix_addr = sp::get_array_addr(module_name, array_name);

  static void (*reduce_matrix_func)(void *, MPI_Fint, MPI_Fint, MPI_Fint) =
      get_reduce_matrix_func();

  reduce_matrix_func(matrix_addr, MPI_Op_c2f(op), MPI_Comm_c2f(comm),
                     root_rank);
}