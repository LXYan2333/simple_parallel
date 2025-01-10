#include <boost/assert.hpp>
#include <boost/mpi.hpp>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fake_main.h>
#include <fstream>
#include <internal_types.h>
#include <iostream>
#include <leader.h>
#include <mimalloc/simple_parallel.h>
#include <mpi.h>
#include <optional>
#include <pagemap.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <tuple>
#include <ucontext.h>
#include <unistd.h>
#include <worker.h>

#include <init.h>

namespace bmpi = boost::mpi;

namespace {

using namespace simple_parallel;

// The fake stack provided for rank 0's main function.
// NOLINTNEXTLINE(*-c-arrays,*non-const-global-variables)
char fake_stack_buffer[1024 * 1024 * 16];

void main_wrap(const int *argc, char **argv, char **env, int *ret) {
  *ret = simple_parallel::original_main(*argc, argv, env);
}

auto check_aslr_disabled(const bmpi::communicator &comm) -> void {
  std::ifstream filestat("/proc/self/personality");
  std::string line;
  std::getline(filestat, line);
  auto personality = std::stoull(line, nullptr, 16);

  // exit if ASLR is disabled
  bool aslr_disabled = (personality & ADDR_NO_RANDOMIZE) != 0;
  bool all_rank_aslr_disabled =
      bmpi::all_reduce(comm, aslr_disabled, std::logical_and<>());
  if (!all_rank_aslr_disabled) {
    if (comm.rank() == 0) {
      std::cerr << "ERROR: ASLR is not disabled on all ranks, please run "
                   "program with simple_parallel's wrap script. Exit.\n";
    }
    comm.barrier();
    if (aslr_disabled) {
      std::cerr << "ASLR is not disabled on rank " << comm.rank() << '\n';
    }
    comm.barrier();
    comm.abort(1);
  }
}
void check_pagesize() {
  auto actual_pagesize = static_cast<size_t>(sysconf(_SC_PAGE_SIZE));
  if (page_size != actual_pagesize) {
    std::cerr << "Error: page size mismatch, expect " << page_size
              << " but actual is " << actual_pagesize << '\n'
              << "You must recompile simple_parallel again on this machine. "
                 "CMake will detect the correct page size automatically.\n";
    std::terminate();
  }
}

} // namespace

namespace simple_parallel {

auto debug() -> bool {
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  static bool res = getenv("SIMPLE_PARALLEL_DEBUG") != nullptr;
  return res;
}

// NOLINTBEGIN(*-global-variables)
const mem_area fake_stack{fake_stack_buffer};

auto get_reserved_heap() -> mem_area {
  // NOLINTNEXTLINE(*-reinterpret-cast)
  return {reinterpret_cast<char *>(0x4000'0000'0000), 0x1000'0000'0000};
};

std::optional<bmpi::communicator> s_p_comm = std::nullopt;
std::optional<bmpi::communicator> s_p_comm_self = std::nullopt;

// indicates whether the program is finished and we should no longer enter
// parallel context or (un)register heap
bool finished = false;
// NOLINTEND(*-global-variables)

auto get_mpi_info_from_env() -> mpi_info {
  static bool initialized = false;
  static int world_size = 1;
  static int world_rank = 0;

  auto ret_single_rank = [&]() -> mpi_info {
    world_size = 1;
    world_rank = 0;
    return {.world_size = 1, .world_rank = 0};
  };

  if (!initialized) {
    initialized = true;
    // NOLINTBEGIN(concurrency-mt-unsafe)
    char *world_size_char = getenv("OMPI_COMM_WORLD_SIZE");
    char *world_rank_char = getenv("OMPI_COMM_WORLD_RANK");
    // NOLINTEND(concurrency-mt-unsafe)

    if (world_size_char == nullptr || world_rank_char == nullptr) {
      return ret_single_rank();
    }

    try {
      world_size = std::stoi(world_size_char);
    } catch (...) {
      // NOLINTNEXTLINE
      std::cerr << "WARNING: Failed to read environment variable "
                   "OMPI_COMM_WORLD_SIZE. Assume single rank.\n";
      return ret_single_rank();
    }

    try {
      world_rank = std::stoi(world_rank_char);
    } catch (...) {
      // NOLINTNEXTLINE
      std::cerr << "WARNING: Failed to read environment variable "
                   "OMPI_COMM_WORLD_RANK. Assume single rank.\n";
      return ret_single_rank();
    }
  }
  return {.world_size = world_size, .world_rank = world_rank};
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto fake_main(int argc, char **argv, char **env) -> int {

  // NOLINTBEGIN(concurrency-mt-unsafe)
  char *world_size_char = getenv("OMPI_COMM_WORLD_SIZE");
  char *world_rank_char = getenv("OMPI_COMM_WORLD_RANK");
  // NOLINTEND(concurrency-mt-unsafe)

  auto [world_size, world_rank] = get_mpi_info_from_env();

  if (debug()) {
    std::cerr << "rank: " << world_rank << ", PID: " << getpid() << '\n';
    std::ignore = std::getchar();
  }

  bmpi::environment mpi_env(argc, argv, bmpi::threading::serialized);
  bmpi::communicator world{};

  check_pagesize();

  s_p_comm = {world, bmpi::comm_duplicate};
  s_p_comm_self = {MPI_COMM_SELF, bmpi::comm_duplicate};

  if (world_size_char == nullptr || world_rank_char == nullptr) {
    if (world.size() > 1) {
      std::cerr
          << "Error: This program is linked to/started from MPI "
             "implementation other than OpenMPI, abort.\nPlease start the "
             "program with OpenMPI.\n";
      return 1;
    }
    std::cerr << "WARNING: This program is linked to simple_parallel library "
                 "but not start from OpenMPI, it will run without distribute "
                 "memory parallel support.\n";
    return original_main(argc, argv, env);
  }

  BOOST_ASSERT(world.size() == world_size);
  BOOST_ASSERT(world.rank() == world_rank);

  if (world.size() > 1) {
    check_aslr_disabled(s_p_comm.value());
  }

  if (world_rank == 0) {
    // a new heap is used for the main thread, so mem allocated in initialize is
    // not synchronized.
    mi_heap_t *main_process_heap = mi_heap_new();
    mi_heap_set_default(main_process_heap);
    register_heap(main_process_heap);

    clear_soft_dirty();

    ucontext_t fake_stack_context;
    ucontext_t fake_main_context;
    if (getcontext(&fake_stack_context) == -1) {
      perror("getcontext");
      return 1;
    };

    fake_stack_context.uc_link = &fake_main_context;
    fake_stack_context.uc_stack.ss_sp = fake_stack.data();
    fake_stack_context.uc_stack.ss_size = fake_stack.size_bytes();

    int ret{};
    // requires glibc > 2.8 to use 64bit pointers in makecontext
    // NOLINTNEXTLINE(*-vararg,*-reinterpret-cast)
    makecontext(&fake_stack_context, reinterpret_cast<void (*)()>(main_wrap), 4,
                &argc, argv, env, &ret);
    if (swapcontext(&fake_main_context, &fake_stack_context) == -1) {
      perror("swapcontext");
      return 1;
    };
    finished = true;
    send_rpc_tag(rpc_tag::exit, 0, s_p_comm.value());
    return ret;
  } else {
    return worker(s_p_comm.value(), 0);
  }

  return 0;
};
} // namespace simple_parallel