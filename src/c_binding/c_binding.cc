#include <boost/assert.hpp>
#include <cstddef>
#include <simple_parallel/c/c_binding.h>
#include <simple_parallel/cxx/simple_parallel.h>
#include <utility>
#include <vector>

namespace sp = simple_parallel;

struct par_ctx_c_binding : public sp::par_ctx_base {
  std::vector<sp::reduce_area> m_reduces;

  par_ctx_c_binding(const par_ctx_c_binding &) = delete;
  par_ctx_c_binding(par_ctx_c_binding &&) = delete;
  auto operator=(const par_ctx_c_binding &) -> par_ctx_c_binding & = delete;
  auto operator=(par_ctx_c_binding &&) -> par_ctx_c_binding & = delete;

  par_ctx_c_binding(bool enter_parallel, std::vector<sp::reduce_area> reduces)
      : par_ctx_base(std::span<sp::reduce_area>{}),
        m_reduces(std::move(reduces)) {
    set_reduces(m_reduces);
    do_enter_parallel(enter_parallel);
  }

  ~par_ctx_c_binding() { do_exit_parallel(); }
};

auto s_p_construct_par_ctx(bool enter_parallel, s_p_reduce_area *reduce_areas,
                           size_t reduce_areas_num) -> s_p_par_ctx {
  std::vector<sp::reduce_area> reduces;
  std::span<s_p_reduce_area> params{reduce_areas, reduce_areas_num};
  reduces.reserve(reduce_areas_num);
  for (const auto &i : params) {
    reduces.emplace_back(i.type, i.begin, i.count, i.op);
  }
  auto *detail = new par_ctx_c_binding(enter_parallel, std::move(reduces));
  // once par_ctx is constructed(with enter_parallel==true), code is executed on
  // all processes and reduce areas is initialized to identity on worker
  // processes

  return {detail->get_comm(), detail};
};

void s_p_destruct_par_ctx(s_p_par_ctx *ctx) {
  BOOST_ASSERT(ctx->detail != nullptr);
  delete ctx->detail;

  // from now on, code is only executed on leader process
  ctx->comm = MPI_COMM_NULL;
  ctx->detail = nullptr;
};