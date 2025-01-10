#pragma once

#include <boost/mpi.hpp>
#include <ucontext.h>

namespace simple_parallel {
// NOLINTBEGIN(*-global-variables)
extern ucontext_t worker_ctx;
// NOLINTEND(*-global-variables)

auto worker(const boost::mpi::communicator &comm, const int root_rank) -> int;
} // namespace simple_parallel