#include <boost/assert.hpp>
#include <boost/mpi.hpp>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fake_main.h>
#include <fstream>
#include <gsl/gsl>
#include <internal_types.h>
#include <iostream>
#include <leader.h>
#include <mimalloc/simple_parallel.h>
#include <mpi.h>
#include <optional>
#include <page_size.h>
#include <pagemap.h>
#include <sched.h>
#include <simple_parallel/cxx/types_fwd.h>
#include <sstream>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/personality.h>
#include <thread>
#include <tuple>
#include <ucontext.h>
#include <unistd.h>
#include <worker.h>

#include <init.h>

namespace bmpi = boost::mpi;
using namespace std::literals;

namespace {

using namespace simple_parallel;

// The fake stack provided for rank 0's main function.
// NOLINTNEXTLINE(*-c-arrays,*non-const-global-variables)
char fake_stack_buffer[1024 * 1024 * 16];

struct main_wrap_params {
  const int *argc;
  char **argv;
  char **env;
  int *ret;
  std::exception_ptr exception;
};

void main_wrap(main_wrap_params *params) try {
  *params->ret =
      simple_parallel::original_main(*params->argc, params->argv, params->env);
} catch (...) {
  params->exception = std::current_exception();
  *params->ret = 1;
}

auto check_aslr_disabled(const bmpi::communicator &comm) -> void {
  std::ifstream filestat("/proc/self/personality");
  if (!filestat.is_open()) {
    throw std::runtime_error("Failed to open /proc/self/personality");
  }
  std::string line;
  std::getline(filestat, line);
  unsigned long long personality{};
  try {
    personality = std::stoull(line, nullptr, 16);
  } catch (...) {
    std::stringstream ss;
    ss << "Failed to parse /proc/self/personality, str: " << line << '\n';
    throw std::runtime_error(std::move(ss).str());
  }

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
  auto actual_pagesize = gsl::narrow_cast<size_t>(sysconf(_SC_PAGE_SIZE));
  if (page_size != actual_pagesize) {
    std::stringstream ss;
    ss << "Error: page size mismatch, expect " << page_size << " but actual is "
       << actual_pagesize << '\n'
       << "You must recompile simple_parallel again on this machine. "
          "CMake will detect the correct page size automatically.\n";
    throw std::runtime_error(std::move(ss).str());
  }
}

auto get_avail_cpu_count() -> size_t {
  cpu_set_t set;
  if (sched_getaffinity(0, sizeof(set), &set) == -1) {
    std::stringstream ss;
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    ss << "Failed to get CPU affinity, reason: " << std::strerror(errno);
    throw std::runtime_error(std::move(ss).str());
  };
  auto cpusetsize = static_cast<size_t>(CPU_COUNT(&set));
  return cpusetsize;
};

void check_cpu_binding(int rank) {
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  if (std::getenv("SIMPLE_PARALLEL_SKIP_CORE_BIND_CHECK") != nullptr) {
    return;
  }
  if (get_avail_cpu_count() <= 2 && std::thread::hardware_concurrency() > 2) {
    std::stringstream ss;
    // clang-format off
    ss << "Error: CPU avaliable to rank " << rank << " is restricted. This process will not be able to take advantage of all CPU cores.\n"
          "Usually this is caused by OpenMPI, which bind process to one core by default. see https://docs.open-mpi.org/en/v5.0.x/man-openmpi/man1/mpirun.1.html#quick-summary\n"
          "This can be easily solved by adding `--bind-to none` to mpirun argument\n"
          "If you believe this is a false positive error, please set SIMPLE_PARALLEL_SKIP_CORE_BIND_CHECK environment variable to any value.\n";
    // clang-format on
    throw std::runtime_error(std::move(ss).str());
  }
}

} // namespace

namespace simple_parallel {

auto debug() -> bool {
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  static bool res = std::getenv("SIMPLE_PARALLEL_DEBUG") != nullptr;
  return res;
}

// NOLINTBEGIN(*-global-variables)
const mem_area fake_stack{fake_stack_buffer};

auto get_reserved_heap() -> mem_area {

  BOOST_ASSERT(get_mpi_info_from_env().world_size != 1);

  static mem_area reserved_heap = [] {
    // NOLINTBEGIN(concurrency-mt-unsafe,*-reinterpret-cast,*-int-to-ptr,*-magic-numbers)
    auto *start = reinterpret_cast<char *>(0x4100'0000'0000);

    if (const char *user_set_start =
            std::getenv("SIMPLE_PARALLEL_RESERVED_HEAP_ADDR")) {
      try {
        start =
            reinterpret_cast<char *>(std::stoull(user_set_start, nullptr, 16));
      } catch (...) {
        throw std::runtime_error(
            "Failed to parse SIMPLE_PARALLEL_RESERVED_HEAP_ADDR");
      }
    }

    size_t size = 0x1000'0000'0000;

    if (const char *user_set_size =
            std::getenv("SIMPLE_PARALLEL_RESERVED_HEAP_SIZE")) {
      try {
        size = std::stoull(user_set_size, nullptr, 16);
      } catch (...) {
        throw std::runtime_error(
            "Failed to parse SIMPLE_PARALLEL_RESERVED_HEAP_SIZE");
      }
    }
    // NOLINTEND(concurrency-mt-unsafe,*-reinterpret-cast,*-int-to-ptr,*-magic-numbers)

    return mem_area{start, size};
  }();

  // NOLINTNEXTLINE(*-reinterpret-cast)
  return reserved_heap;
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
    char *world_size_char = std::getenv("OMPI_COMM_WORLD_SIZE");
    char *world_rank_char = std::getenv("OMPI_COMM_WORLD_RANK");
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
auto fake_main(int argc, char **argv, char **env) -> int try {

  // NOLINTBEGIN(concurrency-mt-unsafe)
  char *world_size_char = std::getenv("OMPI_COMM_WORLD_SIZE");
  char *world_rank_char = std::getenv("OMPI_COMM_WORLD_RANK");
  // NOLINTEND(concurrency-mt-unsafe)

  auto [world_size, world_rank] = get_mpi_info_from_env();

  if (debug()) {
    std::cerr << "rank: " << world_rank << ", PID: " << getpid() << '\n';
    std::ignore = std::getchar();
  }

  bmpi::threading::level thread_level = bmpi::threading::serialized;

  // NOLINTBEGIN(concurrency-mt-unsafe)
  const char *env_thread_level =
      std::getenv("SIMPLE_PARALLEL_MPI_THREAD_LEVEL");
  // NOLINTEND(concurrency-mt-unsafe)
  if (env_thread_level != nullptr) {
    if (env_thread_level == "single"sv) {
      thread_level = bmpi::threading::single;
    } else if (env_thread_level == "funneled"sv) {
      thread_level = bmpi::threading::funneled;
    } else if (env_thread_level == "serialized"sv) {
      thread_level = bmpi::threading::serialized;
    } else if (env_thread_level == "multiple"sv) {
      thread_level = bmpi::threading::multiple;
    } else {
      std::stringstream ss;
      ss << "Invalid SIMPLE_PARALLEL_MPI_THREAD_LEVEL: " << env_thread_level
         << '\n';
      throw std::runtime_error(std::move(ss).str());
    }
  }

  // NOLINTNEXTLINE(*-owning-memory)
  auto *mpi_env = new std::optional<bmpi::environment>{std::in_place, argc,
                                                       argv, thread_level};
  auto del_mpi_env = gsl::finally([&]() { mpi_env->reset(); });
  try {
    bmpi::communicator world{};

    // If the program calls exit(), the destructor of mpi_env will not be called
    // and MPI will complain about it. Destruct it manually in this case.
    struct exit_work_param {
      std::optional<bmpi::environment> *env;
    };
    auto exit_work = +[](int, void *param_v) {
      // NOLINTNEXTLINE(*use-auto)
      gsl::owner<exit_work_param *> param =
          static_cast<gsl::owner<exit_work_param *>>(param_v);
      if (param->env->has_value()) {
        finished = true;
        send_rpc_tag(rpc_tag::exit, 0, s_p_comm.value());
        param->env->reset();
      }
      delete param;
    };
    on_exit(exit_work,
            gsl::owner<exit_work_param *>{new exit_work_param{.env = mpi_env}});

    check_pagesize();

    s_p_comm = {world, bmpi::comm_duplicate};
    s_p_comm_self = {MPI_COMM_SELF, bmpi::comm_duplicate};

    check_cpu_binding(s_p_comm->rank());

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
      // a new heap is used for the main thread, so mem allocated in initialize
      // is not synchronized.
      mi_heap_t *main_process_heap = mi_heap_new();
      mi_heap_set_default(main_process_heap);
      register_heap(main_process_heap);

      clear_soft_dirty();

      ucontext_t fake_stack_context;
      ucontext_t fake_main_context;
      if (getcontext(&fake_stack_context) == -1) {
        std::stringstream ss;
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        ss << "Failed to getcontext, reason: " << std::strerror(errno);
        throw std::runtime_error(std::move(ss).str());
      };

      fake_stack_context.uc_link = &fake_main_context;
      fake_stack_context.uc_stack.ss_sp = fake_stack.data();
      fake_stack_context.uc_stack.ss_size = fake_stack.size_bytes();

      int ret{};
      main_wrap_params params{.argc = &argc,
                              .argv = argv,
                              .env = env,
                              .ret = &ret,
                              .exception = nullptr};
      // requires glibc > 2.8 to use 64bit pointers in makecontext
      // NOLINTNEXTLINE(*-vararg,*-reinterpret-cast)
      makecontext(&fake_stack_context, reinterpret_cast<void (*)()>(main_wrap),
                  1, &params);
      {
        auto finish = gsl::finally([]() { finished = true; });
        if (swapcontext(&fake_main_context, &fake_stack_context) == -1) {
          std::stringstream ss;
          // NOLINTNEXTLINE(concurrency-mt-unsafe)
          ss << "Failed to swapcontext, reason: " << std::strerror(errno);
          throw std::runtime_error(std::move(ss).str());
        };
      }
      if (params.exception) {
        std::rethrow_exception(params.exception);
      }
      send_rpc_tag(rpc_tag::exit, 0, s_p_comm.value());
      return ret;
    } else {
      return worker(s_p_comm.value(), 0);
    }
  } catch (std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "Error: unknown exception\n";
    return 1;
  };

  return 0;

} catch (std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
} catch (...) {
  std::cerr << "Error: unknown exception\n";
  return 1;
};

} // namespace simple_parallel