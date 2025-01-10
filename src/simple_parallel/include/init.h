#pragma once

#include <boost/mpi.hpp>
#include <internal_types.h>
#include <optional>

namespace simple_parallel {

namespace bmpi = boost::mpi;

// NOLINTBEGIN(*-global-variables)
extern const mem_area fake_stack;
extern std::optional<bmpi::communicator> s_p_comm;
extern std::optional<bmpi::communicator> s_p_comm_self;
extern bool finished;
// NOLINTEND(*-global-variables)

auto get_reserved_heap() -> mem_area;

auto debug() -> bool;

} // namespace simple_parallel