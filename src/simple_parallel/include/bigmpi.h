#pragma once

#include <mpi.h>
#include <simple_parallel/cxx/types_fwd.h>
#include <span>

// allows size_t count in MPI

namespace simple_parallel::bigmpi {
auto Bcast(void *buffer, size_t count, MPI_Datatype datatype, const int root,
           MPI_Comm comm) -> int;
auto AllReduce(void *recvbuf, size_t count, MPI_Datatype datatype, MPI_Op op,
               MPI_Comm comm) -> int;
void sync_areas(std::span<mem_area> mem_areas, int root_rank, MPI_Comm comm);
} // namespace simple_parallel::bigmpi