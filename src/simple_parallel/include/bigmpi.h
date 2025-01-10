#pragma once

#include <mpi.h>

// allows size_t count in MPI

namespace simple_parallel::bigmpi {
auto Bcast(void *buffer, size_t count, MPI_Datatype datatype, const int root,
           MPI_Comm comm) -> int;
} // namespace simple_parallel::bigmpi