#pragma once

#include <boost/mpi.hpp>
#include <mutex>
#include <ucontext.h>

namespace simple_parallel {
namespace bmpi = boost::mpi;

class par_ctx {
  ucontext_t m_parallel_ctx;
  ucontext_t m_sync_mem_ctx;
  bmpi::communicator &m_comm;
  static constexpr int m_root_rank = 0;
  static std::mutex m_parallel_mutex;
  const std::lock_guard<std::mutex> m_one_par_ctx_lock{m_parallel_mutex};

public:
  explicit par_ctx(bool enter_parallel = true);
  ~par_ctx();

  // non-copyable and non-moveable
  par_ctx(const par_ctx &) = delete;
  par_ctx(par_ctx &&) = delete;
  auto operator=(const par_ctx &) -> par_ctx & = delete;
  auto operator=(par_ctx &&) -> par_ctx & = delete;

  [[nodiscard]] auto get_comm() const -> const bmpi::communicator &;
};

} // namespace simple_parallel