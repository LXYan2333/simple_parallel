#include <algorithm>
#include <boost/assert.hpp>
#include <cstddef>
#include <limits>
#include <mpi.h>

#include <bigmpi.h>

namespace simple_parallel::bigmpi {
auto Bcast(void *buffer, size_t count, MPI_Datatype datatype, const int root,
           MPI_Comm comm) -> int {

  MPI_Aint lower_bound{};
  MPI_Aint extent{};
  if (datatype == MPI_BYTE) {
    extent = 1;
  } else if (datatype == MPI_DOUBLE) {
    extent = sizeof(double);
  } else if (auto ret = MPI_Type_get_extent(datatype, &lower_bound, &extent);
             ret != MPI_SUCCESS) {
    return ret;
  };

  BOOST_ASSERT(extent > 0);

  constexpr int int_max = std::numeric_limits<int>::max();

  // NOLINTBEGIN(*pointer-arithmetic)
  for (size_t sent = 0; sent < count; sent += int_max) {
    char *this_iter_buffer =
        static_cast<char *>(buffer) + (sent * static_cast<size_t>(extent));
    int this_iter_count = std::min(int_max, static_cast<int>(count - sent));
    auto ret =
        MPI_Bcast(this_iter_buffer, this_iter_count, datatype, root, comm);
    if (ret != MPI_SUCCESS) {
      return ret;
    }
  }
  // NOLINTEND(*pointer-arithmetic)
  return MPI_SUCCESS;
}
} // namespace simple_parallel::bigmpi