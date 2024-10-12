#pragma once

#include <boost/mpi.hpp>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <mpi.h>
#include <simple_parallel/detail.h>
#include <type_traits>

namespace simple_parallel::mpi_util {
    enum rpc_code : int32_t {
        init,
        finalize,
        run_std_function,
        run_function_with_context,
    };

} // namespace simple_parallel::mpi_util
