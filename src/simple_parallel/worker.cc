#include <bigmpi.h>
#include <boost/assert.hpp>
#include <boost/mpi.hpp>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <gsl/util>
#include <init.h>
#include <internal_types.h>
#include <leader.h>
#include <mpi.h>
#include <pagemap.h>
#include <sstream>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/ucontext.h>
#include <ucontext.h>
#include <variant>
#include <vector>

#include <worker.h>

namespace bmpi = boost::mpi;

namespace {

using namespace simple_parallel;

// reserve heap on worker process's address space to prevent other library using
// it.
void reserve_heap_area() {
  int prot = PROT_NONE;
  int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
  void *ptr = nullptr;
  size_t loop_count = 0;
  mem_area reserved_heap = get_reserved_heap();

  // try at most 16 times.
  while (ptr != reserved_heap.data()) {
    loop_count++;
    ptr = mmap(reserved_heap.data(), reserved_heap.size_bytes(), prot, flags,
               -1, 0);
    if (ptr != reserved_heap.data()) {
      if (ptr == MAP_FAILED) {
        if (loop_count > 16) {
          std::stringstream ss;
          ss << "Error: mmap reserved heap failed after 16 times, exit. "
                "Reason: "
             // NOLINTNEXTLINE(concurrency-mt-unsafe)
             << std::strerror(errno);
          throw std::runtime_error(std::move(ss).str());
        }
        continue;
      } else {
        if (loop_count > 16) {
          std::stringstream ss;
          ss << "Error: mmap reserved heap failed after 16 times, exit. "
                "Reason: reserved heap is not at expected address. The memory "
                "address may have beed used by other library. Set "
                "SIMPLE_PARALLEL_RESERVED_HEAP_ADDR environment variable to "
                "another location can solve this problem\n"
                "Tip: set SIMPLE_PARALLEL_RESERVED_HEAP_ADDR environment "
                "variable to "
             << ptr
             << "or set SIMPLE_PARALLEL_RESERVED_HEAP_SIZE environment "
                "variable (default 0x100000000000) to a smaller value might "
                "solve this problem.";
          throw std::runtime_error(std::move(ss).str());
        }
        munmap(ptr, reserved_heap.size_bytes());
      }
    }
  }
}

void recv_and_perform_mem_ops(const bmpi::communicator &comm, int root_rank) {
  size_t mem_ops_size{};
  bmpi::broadcast(comm, mem_ops_size, root_rank);
  std::vector<mem_ops_t> mem_ops{mem_ops_size};
  size_t byte_count = sizeof(mem_ops_t) * mem_ops.size();
  BOOST_ASSERT(byte_count < std::numeric_limits<int>::max());
  MPI_Bcast(mem_ops.data(), gsl::narrow_cast<int>(byte_count), MPI_BYTE,
            root_rank, comm);
  for (const auto &i : mem_ops) {
    std::visit([](auto &&func) { func(); }, i);
  }
}

void recv_stack(const bmpi::communicator &comm, int root_rank) {
  MPI_Bcast(fake_stack.data(), gsl::narrow_cast<int>(fake_stack.size_bytes()),
            MPI_BYTE, root_rank, comm);
}

void recv_dirty_page(const bmpi::communicator &comm, int root_rank) {
  std::vector<mem_area> mem_areas;
  size_t mem_areas_count{};
  bmpi::broadcast(comm, mem_areas_count, root_rank);
  mem_areas.resize(mem_areas_count);
  bigmpi::Bcast(mem_areas.data(),
                mem_areas_count * sizeof(decltype(mem_areas)::value_type),
                MPI_BYTE, root_rank, comm);

  bigmpi::sync_areas(mem_areas, root_rank, comm);
}

void recv_zero_page(const bmpi::communicator &comm, int root_rank) {
  std::vector<pte_range> zeroed_pages;
  size_t zeroed_pages_size{};
  bmpi::broadcast(comm, zeroed_pages_size, root_rank);
  zeroed_pages.resize(zeroed_pages_size);
  bigmpi::Bcast(zeroed_pages.data(), zeroed_pages_size * sizeof(pte_range),
                MPI_BYTE, root_rank, comm);

  for (const pte_range &i : zeroed_pages) {
    mem_area mem = pgrng2memarea(i);
    // use madvise to set memory to zero for large pages is faster than memset
    int advice = MADV_DONTNEED;
    if (madvise(mem.data(), mem.size_bytes(), advice) == -1) {
      if (debug()) {
        perror("madvise");
      }
      // if failed to madvice, fallback to memset
      memset(mem.data(), 0, mem.size_bytes());
    }
  }
}

void recv_heap(const bmpi::communicator &comm, int root_rank) {
  recv_dirty_page(comm, root_rank);
  recv_zero_page(comm, root_rank);
}

void enter_parallel_impl(const bmpi::communicator &comm, int root_rank) {

  recv_and_perform_mem_ops(comm, root_rank);

  recv_stack(comm, root_rank);

  recv_heap(comm, root_rank);

  ucontext_t *parallel_ctx = nullptr;
  MPI_Bcast(static_cast<void *>(&parallel_ctx), sizeof(ucontext_t *), MPI_BYTE,
            root_rank, comm);
  swapcontext(&worker_ctx, parallel_ctx);
}

} // namespace

namespace simple_parallel {

// NOLINTBEGIN(*-global-variables)
ucontext_t worker_ctx;
// NOLINTEND(*-global-variables)

auto worker(const boost::mpi::communicator &comm, const int root_rank) -> int {

  reserve_heap_area();

  while (true) {

    rpc_tag tag{};
    MPI_Bcast(&tag, 1, bmpi::get_mpi_datatype<tag_type>(), root_rank, comm);

    switch (tag) {
    case rpc_tag::exit: {
      return 0;
    }
    case rpc_tag::run_function_without_context: {
      throw std::runtime_error("Not implemented.");
      break;
    }
    case rpc_tag::enter_parallel: {
      enter_parallel_impl(comm, root_rank);
      break;
    }
    default: {
      throw std::runtime_error("Unknown tag.");
    }
    }
  }
  return 0;
}

} // namespace simple_parallel