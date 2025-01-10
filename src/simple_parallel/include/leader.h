#pragma once

#include <mimalloc.h>
#include <simple_parallel/cxx/simple_parallel.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <variant>

namespace simple_parallel {
// NOLINTBEGIN(*-global-variables)
extern bool in_parallel;
// NOLINTEND(*-global-variables)

enum class rpc_tag : int8_t {
  exit,
  run_function_without_context,
  enter_parallel,
};

using tag_type = std::underlying_type_t<rpc_tag>;

void send_rpc_tag(rpc_tag tag, int root_rank, const MPI_Comm &comm);

struct mmap_params {
  void *addr;
  size_t len;
  int prot;
  int flags;
  int fd;
  off_t offset;

  void operator()() const {
    // NOLINTNEXTLINE(*-signed-bitwise)
    if (mmap(addr, len, prot, flags | MAP_FIXED, fd, offset) == MAP_FAILED) {
      perror("mmap");
      std::terminate();
    };
  }
};

struct munmap_params {
  void *addr;
  size_t len;

  // occupy the unmaped area in worker process
  void operator()() const {
    int prot = PROT_NONE;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED;
    if (mmap(addr, len, prot, flags, -1, 0) == MAP_FAILED) {
      perror("mmap");
      std::cerr << "Failed to occupy the unmaped area in worker process\n";
      std::terminate();
    };
  }
};

struct madvise_params {
  void *addr;
  size_t len;
  int advice;

  void operator()() const {
    madvise(addr, len, advice);
#ifndef NDEBUG
    // only terminate in debug since madvise failure is usually harmless
    std::terminate();
#endif
  }
};

using mem_ops_t = std::variant<mmap_params, munmap_params, madvise_params>;

} // namespace simple_parallel