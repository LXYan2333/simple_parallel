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

  constexpr size_t int_max = std::numeric_limits<int>::max();

  // NOLINTBEGIN(*pointer-arithmetic)
  for (size_t sent = 0; sent < count; sent += int_max) {
    char *this_iter_buffer = static_cast<char *>(recvbuf) + (sent * elem_size);
    int this_iter_count =
        gsl::narrow_cast<int>(std::min(int_max, count - sent));
    auto ret = MPI_Allreduce(MPI_IN_PLACE, this_iter_buffer, this_iter_count,
                             datatype, op, comm);
    if (ret != MPI_SUCCESS) {
      return ret;
    }
  }
  // NOLINTEND(*pointer-arithmetic)
  return MPI_SUCCESS;
}

void sync_areas(std::span<mem_area> mem_areas, int root_rank, MPI_Comm comm) {
  boost::container::static_vector<MPI_Request, 32> buffer{};
  // wait request with a buffer of 32 requests
  auto wait_rquest = [&buffer](MPI_Request request) {
    if (buffer.size() == buffer.static_capacity) {
      int index{};
      MPI_Waitany(gsl::narrow_cast<int>(buffer.size()), buffer.data(), &index,
                  MPI_STATUS_IGNORE);
      BOOST_ASSERT(index != MPI_UNDEFINED);
      buffer[gsl::narrow_cast<size_t>(index)] = request;
    } else {
      buffer.push_back(request);
    }
  };

  for (const mem_area mem : mem_areas) {
    size_t count = mem.size_bytes();
    char *begin = mem.data();

    constexpr size_t int_max = std::numeric_limits<int>::max();
    for (size_t sent = 0; sent < count; sent += int_max) {
      MPI_Request request{};
      // NOLINTNEXTLINE(*pointer-arithmetic)
      char *this_iter_buffer = begin + sent;
      int this_iter_count =
          gsl::narrow_cast<int>(std::min(int_max, count - sent));
      MPI_Ibcast(this_iter_buffer, this_iter_count, MPI_BYTE, root_rank, comm,
                 &request);
      wait_rquest(request);
    }
  }

  MPI_Waitall(gsl::narrow_cast<int>(buffer.size()), buffer.data(),
              MPI_STATUS_IGNORE);
}

} // namespace simple_parallel::bigmpi