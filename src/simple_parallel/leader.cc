#include <algorithm>
#include <bigmpi.h>
#include <boost/assert.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/icl/interval_set.hpp>
#include <boost/icl/right_open_interval.hpp>
#include <boost/mpi.hpp>
#include <boost/thread/synchronized_value.hpp>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <exception>
#include <gsl/assert>
#include <gsl/util>
#include <init.h>
#include <initializer_list>
#include <internal_types.h>
#include <map>
#include <mimalloc.h>
#include <mimalloc/simple_parallel.h>
#include <mpi.h>
#include <mutex>
#include <page_size.h>
#include <pagemap.h>
#include <set>
#include <simple_parallel/cxx/simple_parallel.h>
#include <span>
#include <sys/mman.h>
#include <type_traits>
#include <ucontext.h>
#include <vector>
#include <worker.h>

#include <leader.h>

#ifndef hwy_please_include_me_only_once
#define hwy_please_include_me_only_once

namespace bmpi = boost::mpi;
namespace bi = boost::icl;

namespace {

using namespace simple_parallel;

static_assert(std::is_trivially_copyable_v<mem_ops_t>);

auto get_next_sym(const char *symbol) -> void * {
  // call dlerror to clear previous error
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  dlerror();

  void *next_sym = dlsym(RTLD_NEXT, symbol);

  if (next_sym == nullptr) {
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    char *error = dlerror();
    if (error != nullptr) {
      std::cerr << error << '\n';
    } else {
      std::cerr << "Unknown dlsym error\n";
    }
    std::terminate();
  }
  return next_sym;
}

template <class T> struct glibc_alloc {
  using value_type = T;

  glibc_alloc() = default;

  template <class U>
  constexpr explicit glibc_alloc(const glibc_alloc<U> & /*other*/) noexcept {}

  [[nodiscard]] auto allocate(std::size_t size) -> T * {
    if (size > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
      throw std::bad_array_new_length();
    }

    // NOLINTBEGIN(*-reinterpret-cast)
    static auto *orig_malloc =
        reinterpret_cast<void *(*)(size_t size)>(get_next_sym("malloc"));
    // NOLINTEND(*-reinterpret-cast)

    auto p = static_cast<T *>(orig_malloc(size * sizeof(T)));

    if (p == nullptr) {
      throw std::bad_alloc();
    }
    return p;
  }

  void deallocate(T *p, std::size_t /*n*/) noexcept {
    // NOLINTBEGIN(*-reinterpret-cast)
    static auto *orig_free =
        reinterpret_cast<void (*)(void *)>(get_next_sym("free"));
    // NOLINTEND(*-reinterpret-cast)
    orig_free(p);
  }
};

template <class T, class U>
auto operator==(const glibc_alloc<T> & /*lhs*/, const glibc_alloc<U> & /*rhs*/)
    -> bool {
  return true;
}

template <class T, class U>
auto operator!=(const glibc_alloc<T> & /*lhs*/, const glibc_alloc<U> & /*rhs*/)
    -> bool {
  return false;
}

// a boost interval set, using glibc to alloc memory
template <typename T>
using my_interval_set =
    bi::interval_set<T, std::less, bi::right_open_interval<T>, glibc_alloc>;

// This should be a global variable, but since it may be touched before global
// variable initialization (and cause multi initialization), it is placed into a
// function
auto leader_heaps() -> auto & {
  static boost::synchronized_value<
      std::set<mi_heap_t *, std::less<>, glibc_alloc<mi_heap_t *>>,
      std::recursive_mutex>
      leader_heaps;
  return leader_heaps;
}

struct visit_block_state {
  my_interval_set<pgnum> malloc_pages;
};

auto visit_block(const mi_heap_t * /*unused*/,
                 const mi_heap_area_t * /*unused*/, void *block,
                 size_t block_size, void *vstate) -> bool {
  if (block == nullptr) {
    return true;
  }

  pte_range pte_range = memarea2pgrng({static_cast<char *>(block), block_size});

  auto *state = static_cast<visit_block_state *>(vstate);
  state->malloc_pages.add({pte_range.begin, pte_range.begin + pte_range.count});

  return true;
};

auto mem_area_overlap(mem_area mem_area_lhs, mem_area mem_area_rhs) -> bool {
  return &mem_area_lhs.back() >= &mem_area_rhs.front() &&
         &mem_area_lhs.front() <= &mem_area_rhs.back();
}

auto pte_range_overlap(pte_range lhs, pte_range rhs) -> bool {
  const size_t lhs_end = lhs.begin + lhs.count;
  const size_t rhs_end = rhs.begin + rhs.count;
  return lhs_end > rhs.begin && lhs.begin < rhs_end;
}

auto overlap_with_reserved_heap(mem_area mem_area) -> bool {
  return mem_area_overlap(mem_area, get_reserved_heap());
}

} // namespace

namespace simple_parallel {
class reduce_area_friend {
public:
  static auto get_inner_pages(const reduce_area &reduce)
      -> const std::optional<pte_range> & {
    return reduce.m_inner_pages;
  }
};
} // namespace simple_parallel
#endif

// Generates code for every target that this compiler can support.
#undef HWY_TARGET_INCLUDE
// NOLINTNEXTLINE(*macro-usage)
#define HWY_TARGET_INCLUDE "leader.cc" // this file
#include <hwy/foreach_target.h>        // must come before highway.h
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

auto is_zeroed_pg_simd(pgnum page_num) -> bool {

  static_assert(page_size % sizeof(uint64_t) == 0);

  // NOLINTBEGIN(*-reinterpret-cast,performance-no-int-to-ptr,*-pointer-arithmetic)
  const auto *begin = reinterpret_cast<uint64_t *>(page_size * page_num);
  constexpr size_t size = page_size / sizeof(uint64_t);
  constexpr size_t half_size = size / 2;

  // fast test
  if (begin[0] != 0 or begin[half_size] != 0) {
    return false;
  }

  const hn::ScalableTag<uint64_t> tag;
  using VecT = hn::Vec<decltype(tag)>;
  using MaskT = hn::Mask<decltype(tag)>;

  VecT res = hn::Zero(tag);
  BOOST_ASSERT(size % hn::Lanes(tag) == 0);
  for (size_t i = 0; i < size; i += hn::Lanes(tag)) {
    const VecT vec = hn::Load(tag, begin + i);
    res = hn::Or(res, vec);
  }

  const MaskT is_zero = hn::Eq(res, hn::Zero(tag));
  return hn::AllTrue(tag, is_zero);
  // NOLINTEND(*-reinterpret-cast,performance-no-int-to-ptr,*-pointer-arithmetic)
}

void get_zero_and_dirty_pages(my_interval_set<pgnum> &dirty_pages,
                              my_interval_set<pgnum> &zero_pages,
                              std::span<const reduce_area> reduces) {
  // collect all blocks allocated my mimalloc
  visit_block_state state{};
  for (const mi_heap_t *heap : leader_heaps().value()) {
    mi_heap_visit_blocks(heap, true, visit_block, &state);
  }
  size_t skiped_reduce_size = 0;

  // find dirty pages and dirty zeroed pages
  for (const bi::right_open_interval<pgnum> &page_range : state.malloc_pages) {

    const size_t size = page_range.upper() - page_range.lower();
    const pte_range pgs = {.begin = page_range.lower(), .count = size};

    boost::container::small_vector<pte_range, 16, glibc_alloc<pte_range>>
        overlap_reduce_rngs;

    for (const reduce_area &reduce : reduces) {
      const std::optional<pte_range> reduce_rng =
          reduce_area_friend::get_inner_pages(reduce);
      if (reduce_rng.has_value() and pte_range_overlap(*reduce_rng, pgs)) {
        overlap_reduce_rngs.push_back(*reduce_rng);
      }
    }

    std::vector<uint64_t, glibc_alloc<uint64_t>> ptes(size);
    get_pte({.begin = page_range.lower(), .count = size}, ptes);

    for (size_t i = 0; i < size; ++i) {
      const pte pte_i = pte{ptes[i]};
      const pgnum page_num = page_range.lower() + i;

      if (!pte_i.is_present() or !pte_i.is_dirty()) {
        continue;
      }

      // skip pages in reduce areas. they will be initialized on worker.
      for (const pte_range &overlap_reduce_rng : overlap_reduce_rngs) {
        const size_t reduce_begin = overlap_reduce_rng.begin;
        const size_t reduce_end =
            overlap_reduce_rng.begin + overlap_reduce_rng.count;

        if (page_num >= reduce_begin and page_num < reduce_end) {
          skiped_reduce_size++;
          goto next_for_loop;
        }
      }

      if (is_zeroed_pg_simd(page_num)) {
        zero_pages.add(page_num);
        continue;
      }

      dirty_pages.add(page_num);

    next_for_loop:;
    }
  }

  if (debug()) {
    std::cerr << "skiped pages in reduce areas: " << skiped_reduce_size << '\n';
  }
}

} // namespace HWY_NAMESPACE
} // namespace
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace {

// NOLINTBEGIN(*-global-variables,*c-arrays)
char sync_mem_stack[1024 * 1024 * 8];
// NOLINTBEGIN(cert-err58-cpp)

// This should be a global variable, but since it may be touched before global
// variable initialization (and cause multi initialization), it is placed into a
// function
auto memory_operations() -> auto & {
  static boost::synchronized_value<
      std::vector<mem_ops_t, glibc_alloc<mem_ops_t>>, std::recursive_mutex>
      memory_operations;
  return memory_operations;
};
// NOLINTEND(cert-err58-cpp)
// NOLINTEND(*-global-variables,*c-arrays)

void send_dirty_page(my_interval_set<pgnum> &dirty_pages,
                     const bmpi::communicator &comm, int root_rank) {

  for (const bi::right_open_interval<pgnum> &page_range : dirty_pages) {
    const size_t size = page_range.upper() - page_range.lower();
    mem_area mem_area =
        pgrng2memarea({.begin = page_range.lower(), .count = size});
    char *begin = mem_area.data();
    size_t block_size = mem_area.size_bytes();
    MPI_Bcast(static_cast<void *>(&begin), sizeof(begin), MPI_BYTE, root_rank,
              comm);
    bmpi::broadcast(comm, block_size, root_rank);
    bigmpi::Bcast(begin, block_size, MPI_BYTE, root_rank, comm);
  }

  void *end = nullptr;
  MPI_Bcast(static_cast<void *>(&end), sizeof(end), MPI_BYTE, root_rank, comm);
}

void send_zeroed_page(my_interval_set<pgnum> &zero_pages,
                      const bmpi::communicator &comm, int root_rank) {
  std::vector<pte_range, glibc_alloc<pte_range>> zeroed_pages;
  for (const bi::right_open_interval<pgnum> &page_range : zero_pages) {
    zeroed_pages.emplace_back(page_range.lower(),
                              page_range.upper() - page_range.lower());
  }
  size_t zeroed_pages_size = zeroed_pages.size();
  bmpi::broadcast(comm, zeroed_pages_size, root_rank);
  bigmpi::Bcast(zeroed_pages.data(), zeroed_pages_size * sizeof(pte_range),
                MPI_BYTE, root_rank, comm);
}

HWY_EXPORT(get_zero_and_dirty_pages);

void send_heap(const bmpi::communicator &comm, int root_rank,
               std::span<const reduce_area> reduces) {

  my_interval_set<pgnum> dirty_pages;
  my_interval_set<pgnum> zero_pages;

  HWY_DYNAMIC_DISPATCH(get_zero_and_dirty_pages)(dirty_pages, zero_pages,
                                                 reduces);

  if (debug()) {
    // NOLINTBEGIN
    std::cerr << "dirty pages: " << dirty_pages.size() << '\n';
    for (const bi::right_open_interval<pgnum> &page_range : dirty_pages) {
      std::cerr << reinterpret_cast<void *>(page_range.lower() * page_size)
                << '-'
                << reinterpret_cast<void *>(page_range.upper() * page_size)
                << '-' << (page_range.upper() - page_range.lower()) * page_size
                << '\n';
    }
    std::cerr << "zero pages:" << zero_pages.size() << '\n';
    for (const bi::right_open_interval<pgnum> &page_range : zero_pages) {
      std::cerr << reinterpret_cast<void *>(page_range.lower() * page_size)
                << '-'
                << reinterpret_cast<void *>(page_range.upper() * page_size)
                << '-' << (page_range.upper() - page_range.lower()) * page_size
                << '\n';
    }
    // NOLINTEND
  }

  send_dirty_page(dirty_pages, comm, root_rank);
  send_zeroed_page(zero_pages, comm, root_rank);
}

void send_stack(const bmpi::communicator &comm, int root_rank) {
  MPI_Bcast(fake_stack.data(), gsl::narrow_cast<int>(fake_stack.size_bytes()),
            MPI_BYTE, root_rank, comm);
}

void send_mem_ops(const bmpi::communicator &comm, int root_rank) {
  // TODO: potential deadlock if any mem op is performed during the broadcast
  auto &&mem_ops = memory_operations().synchronize();
  size_t mem_ops_size = mem_ops->size();
  bmpi::broadcast(comm, mem_ops_size, root_rank);
  MPI_Bcast(mem_ops->data(),
            gsl::narrow_cast<int>(sizeof(mem_ops_t) * mem_ops->size()),
            MPI_BYTE, root_rank, comm);
  mem_ops->clear();
}

struct enter_parallel_impl_params {
  const bmpi::communicator *comm;
  const int *root_rank;
  ucontext_t *parallel_ctx;
  std::span<const reduce_area> reduces;
};

void enter_parallel_impl(enter_parallel_impl_params *params) {
  static mi_heap_t *mpi_heap = mi_heap_new();
  mi_heap_t *default_heap = mi_heap_get_default();
  mi_heap_set_default(mpi_heap);
  auto restore_heap =
      gsl::finally([&default_heap] { mi_heap_set_default(default_heap); });

  send_rpc_tag(rpc_tag::enter_parallel, *params->root_rank, *params->comm);

  send_mem_ops(*params->comm, *params->root_rank);

  send_stack(*params->comm, *params->root_rank);

  send_heap(*params->comm, *params->root_rank, params->reduces);

  MPI_Bcast(params->parallel_ctx, sizeof(ucontext_t), MPI_BYTE,
            *params->root_rank, *params->comm);

  clear_soft_dirty();
}

} // namespace

namespace simple_parallel {

std::mutex par_ctx_base::m_parallel_mutex{};

void reduce_area::init_inner_pages(std::optional<pte_range> &inner_pages) {
  inner_pages =
      memarea2innerpgrng({static_cast<char *>(m_begin),
                          m_count * gsl::narrow_cast<size_t>(m_sizeof_type)});
};

auto reduce_area::all_reduce(const bmpi::communicator &comm) const -> int {
  return bigmpi::AllReduce(m_begin, m_count, m_type, m_op, comm);
}

void reduce_area::init_reduce_area_on_worker() const {

  static const std::array zero_init_types = {
      MPI_SHORT,         MPI_INT,       MPI_LONG,     MPI_UNSIGNED_LONG,
      MPI_LONG_LONG_INT, MPI_LONG_LONG, MPI_FLOAT,    MPI_DOUBLE,
      MPI_LONG_DOUBLE,   MPI_INT8_T,    MPI_INT16_T,  MPI_INT32_T,
      MPI_INT64_T,       MPI_UINT8_T,   MPI_UINT16_T, MPI_UINT32_T,
      MPI_UINT64_T};

  static const std::map<const MPI_Op, const char *> op_2_name{
      {MPI_OP_NULL, "MPI_OP_NULL"}, {MPI_MAX, "MPI_MAX"},
      {MPI_MIN, "MPI_MIN"},         {MPI_SUM, "MPI_SUM"},
      {MPI_PROD, "MPI_PROD"},       {MPI_LAND, "MPI_LAND"},
      {MPI_BAND, "MPI_BAND"},       {MPI_LOR, "MPI_LOR"},
      {MPI_BOR, "MPI_BOR"},         {MPI_LXOR, "MPI_LXOR"},
      {MPI_BXOR, "MPI_BXOR"},       {MPI_MINLOC, "MPI_MINLOC"},
      {MPI_MAXLOC, "MPI_MAXLOC"},   {MPI_REPLACE, "MPI_REPLACE"}};

  auto not_support = [&]() {
    std::array<char, MPI_MAX_OBJECT_NAME + 1> type_name{};
    int len{};
    MPI_Type_get_name(m_type, type_name.data(), &len);

    std::cerr << "Error: Reduce " << type_name.data() << " with MPI Operation "
              << op_2_name.at(m_op)
              << " is not supported by simple_parallel. You can manually call "
                 "`MPI_Reduce` to reduce your array/value.\n";
    std::terminate();
  };

  const bool is_zero_init_when_sum = std::ranges::any_of(
      zero_init_types, [this](MPI_Datatype e) -> bool { return e == m_type; });
  if (is_zero_init_when_sum and m_op == MPI_SUM) {
    std::memset(m_begin, 0, m_count * gsl::narrow_cast<size_t>(m_sizeof_type));
    return;
  };

  if (m_type == MPI_C_BOOL) {
    BOOST_ASSERT(m_sizeof_type == 1);
    int identity{};
    if (m_op == MPI_LAND) {
      identity = 1;
    } else if (m_op == MPI_LOR) {
      identity = 0;
    } else {
      not_support();
    }
    std::memset(m_begin, identity,
                m_count * gsl::narrow_cast<size_t>(m_sizeof_type));
    return;
  }

  not_support();
};

void send_rpc_tag(rpc_tag tag, int root_rank, const MPI_Comm &comm) {
  static_assert(bmpi::is_mpi_datatype<tag_type>());
  MPI_Bcast(&tag, 1, bmpi::get_mpi_datatype<tag_type>(), root_rank, comm);
}

// NOLINTNEXTLINE(*-member-init)
par_ctx_base::par_ctx_base(std::span<const reduce_area> reduces)
    : m_comm(&s_p_comm.value()), m_reduces(reduces) {
  BOOST_ASSERT(!finished);
}

reduce_area::operator mem_area() const {
  return mem_area{static_cast<char *>(m_begin),
                  m_count * gsl::narrow_cast<size_t>(m_sizeof_type)};
};

void par_ctx_base::verify_reduces_no_overlap() const {
  for (size_t lhs = 0; lhs < m_reduces.size(); lhs++) {
    for (size_t rhs = lhs + 1; rhs < m_reduces.size(); rhs++) {
      const reduce_area lhs_reduce_area = m_reduces[lhs];
      const reduce_area rhs_reduce_area = m_reduces[rhs];

      if (mem_area_overlap(static_cast<mem_area>(lhs_reduce_area),
                           static_cast<mem_area>(rhs_reduce_area))) {
        // https://stackoverflow.com/a/56386744/18245120 NOLINTBEGIN
        auto get_number_with_ordinal_suffix = [](size_t number) {
          const char *suffix = "th";
          if (number % 100 < 11 || number % 100 > 13) {
            switch (number % 10) {
            case 1:
              suffix = "st";
              break;
            case 2:
              suffix = "nd";
              break;
            case 3:
              suffix = "rd";
              break;
            }
          }
          return std::to_string(number) + suffix;
        };
        // NOLINTEND

        std::cerr << "Error: the " << get_number_with_ordinal_suffix(lhs + 1)
                  << " reduce area and "
                  << get_number_with_ordinal_suffix(rhs + 1)
                  << " reduce area overlaps. Please check your reduce input "
                     "parameter.\n";
        std::terminate();
      }
    }
  }
}

void par_ctx_base::set_reduces(std::span<const reduce_area> reduces) {
  m_reduces = reduces;
};

void par_ctx_base::do_enter_parallel(bool enter_parallel) {
  if (!enter_parallel or m_comm->size() == 1) {
    m_comm = &s_p_comm_self.value();
    return;
  }
  if (getcontext(&m_sync_mem_ctx) == -1) {
    perror("getcontext");
    std::terminate();
  }
  m_sync_mem_ctx.uc_link = &m_parallel_ctx;
  m_sync_mem_ctx.uc_stack.ss_sp = &sync_mem_stack[0];
  m_sync_mem_ctx.uc_stack.ss_size = sizeof(sync_mem_stack);

  // there is no type check in `makecontext`, better to use a struct to pass
  // params
  enter_parallel_impl_params params{.comm = m_comm,
                                    .root_rank = &m_root_rank,
                                    .parallel_ctx = &m_parallel_ctx,
                                    .reduces = m_reduces};

  // NOLINTBEGIN(*-vararg,*-reinterpret-cast)
  makecontext(&m_sync_mem_ctx,
              reinterpret_cast<void (*)()>(enter_parallel_impl), 1, &params);
  // NOLINTEND(*-vararg,*-reinterpret-cast)

  verify_reduces_no_overlap();

  // enter parallel context when return from swapcontext
  if (swapcontext(&m_parallel_ctx, &m_sync_mem_ctx) == -1) {
    perror("swapcontext");
    std::terminate();
  }

  if (debug()) {
    Expects(m_comm->rank() == get_mpi_info_from_env().world_rank);
  }

  if (m_comm->rank() != m_root_rank) {
    for (const reduce_area &reduce : m_reduces) {
      reduce.init_reduce_area_on_worker();
    }
  }
}

void par_ctx_base::do_exit_parallel() {
  if (m_comm->size() > 1) {
    for (const reduce_area &reduce : m_reduces) {
      if (reduce.all_reduce(*m_comm) != MPI_SUCCESS) {
        std::cerr << "Failed to reduce\n";
        std::terminate();
      }
    }
  }
  if (m_comm->rank() != m_root_rank) {
    setcontext(&worker_ctx);
  }
}

void register_heap(mi_heap_t *heap) {
  if (finished) {
    return;
  }

  BOOST_ASSERT(std::ranges::none_of(
      leader_heaps().value(), [&](const mi_heap_t *registered_heap) -> bool {
        return heap == registered_heap;
      }));

  // potential deadlock if mpi implementation start another thread during sync
  leader_heaps()->insert(heap);
}

void unregister_heap(mi_heap_t *heap) {
  if (finished) {
    return;
  }
  leader_heaps()->erase(heap);
}

[[nodiscard]] auto par_ctx_base::get_comm() const
    -> const bmpi::communicator & {
  return *m_comm;
};

auto proxy_mmap(void * /*addr*/, size_t len, int prot, int flags, int file_desc,
                off_t offset) -> void * {
  // `begin` needs to be protected by a mutex
  static std::mutex mmap_mutex;
  const std::lock_guard lock{mmap_mutex};

  // currently the largest huge page size is 1GiB
  constexpr auto step = static_cast<ptrdiff_t>(1024) * 1024 * 1024;

  static mem_area reserved_heap = get_reserved_heap();
  static char *begin = reserved_heap.data();

  // NOLINTNEXTLINE(*-signed-bitwise)
  BOOST_ASSERT((flags & MAP_FIXED) == 0);
  BOOST_ASSERT(file_desc == -1);

  void *res = nullptr;
  // NOLINTBEGIN(*-pointer-arithmetic)
  while (true) {
    if (begin + len > reserved_heap.end().base()) {
      // this may called at very early stage of the program (and the stdc++ may
      // not initialized yet), so we can't use std::cerr
      // NOLINTNEXTLINE
      fprintf(stderr, "Error: mmap overflow when try to malloc, please "
                      "increase the reserved heap size.\n");
      std::terminate();
    }

    // MAP_FIXED_NOREPLACE can be added to flags but it requies a recent glibc
    // and kernel
    res = mmap(begin, len, prot, flags, file_desc, offset);

    if (res == MAP_FAILED) {
      begin += step;
      continue;
    }
    if (res != begin) {
      // kernel did not place the mmaped space in the specified place, unmap and
      // try another place
      munmap(res, len);
      begin += step;
      continue;
    }
    break;
  }

  memory_operations()->emplace_back(mmap_params{.addr = res,
                                                .len = len,
                                                .prot = prot,
                                                .flags = flags,
                                                .fd = file_desc,
                                                .offset = offset});

  // https://stackoverflow.com/a/3407254/18245120
  auto round_up = [](const size_t numToRound, const size_t multiple) -> size_t {
    if (multiple == 0) {
      return numToRound;
    }
    size_t remainder = numToRound % multiple;
    if (remainder == 0) {
      return numToRound;
    }
    return numToRound + multiple - remainder;
  };

  // round up to make sure next mmap is page aligned
  begin += round_up(len, step);
  // NOLINTEND(*-pointer-arithmetic)

  return res;
};

auto proxy_madvise(void *addr, size_t size, int advice) -> int {
  if (finished) {
    return madvise(addr, size, advice);
  }

  if (!overlap_with_reserved_heap({static_cast<char *>(addr), size})) {
#if defined(__sun)
    return madvise((caddr_t)addr, size, advice);
#else
    return madvise(addr, size, advice);
#endif
  }

  int ret = madvise(addr, size, advice);

  if (ret == 0) {
    memory_operations()->emplace_back(
        madvise_params{.addr = addr, .len = size, .advice = advice});
  }

  return ret;
};

auto proxy_munmap(void *addr, size_t size) -> int {
  if (finished) {
    return munmap(addr, size);
  }

  if (!overlap_with_reserved_heap({static_cast<char *>(addr), size})) {
    return munmap(addr, size);
  }

  int ret = munmap(addr, size);

  if (ret == 0) {
    memory_operations()->emplace_back(munmap_params{.addr = addr, .len = size});
  }

  return ret;
};

} // namespace simple_parallel

#endif // HWY_ONCE