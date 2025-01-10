#pragma once

#include <cstddef>
#include <cstdint>
#include <internal_types.h>
#include <page_size.h>
#include <span>
#include <sys/types.h>

namespace simple_parallel {

using pgnum = size_t;

// https://www.kernel.org/doc/html/latest/admin-guide/mm/pagemap.html
class pte {
  using entry_t = uint64_t;
  entry_t m_entry;

  static constexpr entry_t dirty_bit = 1ULL << 55ULL;
  static constexpr entry_t present_bit = 1ULL << 63ULL;

public:
  explicit pte(uint64_t entry) : m_entry(entry) {}
  explicit pte() : m_entry(0) {}

  // this page is written since last clear_soft_dirty
  [[nodiscard]] auto is_dirty() const -> bool {
    return (m_entry & dirty_bit) != 0;
  }

  // this page is mmaped, but not touched yet
  [[nodiscard]] auto is_present() const -> bool {
    return (m_entry & dirty_bit) != 0;
  }

  // this page is not mmaped
  [[nodiscard]] auto is_mapped() const -> bool { return m_entry != 0; }

  explicit operator entry_t() const { return m_entry; }
};

struct pte_range {
  pgnum begin;
  size_t count;
};

// clear the soft dirty bit of all pages so we can trace written pages and
// eliminate unnecessary synchronization when enter parallel context
void clear_soft_dirty();

auto addr2pgnum(void *addr) -> pgnum;

auto memarea2pgrng(mem_area area) -> pte_range;

auto pgrng2memarea(pte_range range) -> mem_area;

inline auto pgnum2memarea(pgnum page_num) -> mem_area {
  // NOLINTNEXTLINE(*-reinterpret-cast,performance-no-int-to-ptr)
  return {reinterpret_cast<char *>(page_num * page_size), page_size};
}

void get_pte(pte_range range, std::span<uint64_t>);

auto get_pte(pgnum page_num) -> pte;

// https://godbolt.org/z/e6Y4jGKvG
[[deprecated("Performance critical, reimplement in leader.cc using SIMD")]]
inline auto is_zeroed_pg(pgnum page_num) -> bool {

  static_assert(page_size % sizeof(uint64_t) == 0);

  // NOLINTNEXTLINE
  const auto *begin = reinterpret_cast<uint64_t *>(page_size * page_num);
  const size_t size = page_size / sizeof(uint64_t);

  // fast test
  // NOLINTNEXTLINE
  if (begin[0] != 0) {
    return false;
  }

  // check the full page
  // clang can vectorize this loop quite well, but gcc needs guide and correct
  // march flag (and still perform worse than clang, and even fail to vectorize
  // it after gcc14)
  bool res = true;
#pragma omp simd reduction(and : res)
  for (size_t i = 0; i < size; ++i) {
    // this seems redundant, but we need this to make gcc happy
    // NOLINTNEXTLINE
    if (begin[i] != 0) {
      // can not return here, or auto vectorization will fail
      res = false;
    }
  }
  return res;
}

} // namespace simple_parallel