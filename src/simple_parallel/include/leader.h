#pragma once

#include <mimalloc.h>
#include <simple_parallel/cxx/simple_parallel.h>
#include <sstream>
#include <stdexcept>
#include <sys/mman.h>
#include <ucontext.h>
#include <variant>

namespace simple_parallel {

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
      std::stringstream ss;
      // NOLINTNEXTLINE(concurrency-mt-unsafe)
      ss << "Failed to mmap, reason: " << std::strerror(errno);
      throw std::runtime_error(ss.str());
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
      std::stringstream ss;
      ss << "Failed to occupy the unmaped area in worker process, reason: "
         // NOLINTNEXTLINE(concurrency-mt-unsafe)
         << std::strerror(errno);
      throw std::runtime_error(ss.str());
    };
  }
};

struct madvise_params {
  void *addr;
  size_t len;
  int advice;

  void operator()() const {
#ifndef NDEBUG
    // only terminate in debug since madvise failure is usually harmless
    if (madvise(addr, len, MADV_NORMAL) == -1) {
      std::terminate();
    }
#else
    madvise(addr, len, advice);
#endif
  }
};

using mem_ops_t = std::variant<mmap_params, munmap_params, madvise_params>;

} // namespace simple_parallel