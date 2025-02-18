#include <algorithm>
#include <boost/assert.hpp>
#include <cstddef>
#include <gsl/util>
#include <limits>
#include <mpi.h>

#include <bigmpi.h>

namespace {
auto get_mpi_type_size_fallback(MPI_Datatype datatype) -> size_t {
  int size{};
  MPI_Type_size(datatype, &size);
  return gsl::narrow_cast<size_t>(size);
}

auto get_mpi_type_size(MPI_Datatype datatype) -> size_t {
  if (datatype == MPI_BYTE) {
    BOOST_ASSERT(get_mpi_type_size_fallback(datatype) == 1);
    return 1;
  }
  if (datatype == MPI_DOUBLE) {
    BOOST_ASSERT(get_mpi_type_size_fallback(datatype) == sizeof(double));
    return sizeof(double);
  }
  if (datatype == MPI_INT) {
    BOOST_ASSERT(get_mpi_type_size_fallback(datatype) == sizeof(int));
    return sizeof(int);
  }
  if (datatype == MPI_FLOAT) {
    BOOST_ASSERT(get_mpi_type_size_fallback(datatype) == sizeof(float));
    return sizeof(float);
  }
  return get_mpi_type_size_fallback(datatype);
}
} // namespace

namespace simple_parallel::bigmpi {
auto Bcast(void *buffer, size_t count, MPI_Datatype datatype, const int root,
           MPI_Comm comm) -> int {

  const size_t elem_size = get_mpi_type_size(datatype);
  BOOST_ASSERT(elem_size > 0);

  constexpr size_t int_max = std::numeric_limits<int>::max();

  // NOLINTBEGIN(*pointer-arithmetic)
  for (size_t sent = 0; sent < count; sent += int_max) {
    char *this_iter_buffer = static_cast<char *>(buffer) + (sent * elem_size);
    int this_iter_count =
        gsl::narrow_cast<int>(std::min(int_max, count - sent));
    auto ret =
        MPI_Bcast(this_iter_buffer, this_iter_count, datatype, root, comm);
    if (ret != MPI_SUCCESS) {
      return ret;
    }
  }
  // NOLINTEND(*pointer-arithmetic)
  return MPI_SUCCESS;
}

auto AllReduce(void *recvbuf, size_t count, MPI_Datatype datatype, MPI_Op op,
               MPI_Comm comm) -> int {
  const size_t elem_size = get_mpi_type_size(datatype);
  BOOST_ASSERT(elem_size > 0);

  constexpr int int_max = std::numeric_limits<int>::max();

  // NOLINTBEGIN(*pointer-arithmetic)
  for (size_t sent = 0; sent < count; sent += int_max) {
    char *this_iter_buffer = static_cast<char *>(recvbuf) + (sent * elem_size);
    int this_iter_count =
        std::min(int_max, gsl::narrow_cast<int>(count - sent));
    auto ret = MPI_Allreduce(MPI_IN_PLACE, this_iter_buffer, this_iter_count,
                             datatype, op, comm);
    if (ret != MPI_SUCCESS) {
      return ret;
    }
  }
  // NOLINTEND(*pointer-arithmetic)
  return MPI_SUCCESS;
}
} // namespace simple_parallel::bigmpi