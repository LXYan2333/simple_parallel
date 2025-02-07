#pragma once

#include <boost/assert.hpp>
#include <cstddef>
#include <cstdint>
#include <internal_types.h>
#include <memory>
#include <optional>
#include <page_size.h>
#include <simple_parallel/cxx/types_fwd.h>
#include <span>
#include <sys/types.h>

namespace simple_parallel {

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

// clear the soft dirty bit of all pages so we can trace written pages and
// eliminate unnecessary synchronization when enter parallel context
void clear_soft_dirty();

inline auto pgnum2memarea(pgnum page_num) -> mem_area {
  // NOLINTNEXTLINE(*-reinterpret-cast,performance-no-int-to-ptr)
  return {reinterpret_cast<char *>(page_num * page_size), page_size};
}

void get_pte(pte_range range, std::span<uint64_t>);

auto get_pte(pgnum page_num) -> pte;

inline auto addr2pgnum(void *addr) -> pgnum {
  // NOLINTNEXTLINE(*-reinterpret-cast)
  return reinterpret_cast<size_t>(addr) / page_size;
}

inline auto memarea2pgrng(mem_area area) -> pte_range {
  BOOST_ASSERT(area.size() != 0);
  const pgnum begin = addr2pgnum(area.data());
  const pgnum end = addr2pgnum(&area.back()) + 1;

  // NOLINTBEGIN
  BOOST_ASSERT(begin * page_size <= reinterpret_cast<size_t>(area.data()));
  BOOST_ASSERT((begin + 1) * page_size > reinterpret_cast<size_t>(area.data()));

  BOOST_ASSERT(end * page_size >=
               reinterpret_cast<size_t>(std::to_address(area.end())));
  BOOST_ASSERT((end - 1) * page_size <
               reinterpret_cast<size_t>(std::to_address(area.end())));
  // NOLINTEND

  return {.begin = begin, .count = end - begin};
}

inline auto memarea2innerpgrng(mem_area area) -> std::optional<pte_range> {
  BOOST_ASSERT(area.size() != 0);
  const pgnum begin = addr2pgnum(area.data() - 1) + 1;
  const pgnum end = addr2pgnum(std::to_address(area.end()));

  // NOLINTBEGIN
  BOOST_ASSERT(begin * page_size >= reinterpret_cast<size_t>(area.data()));
  BOOST_ASSERT((begin - 1) * page_size < reinterpret_cast<size_t>(area.data()));

  BOOST_ASSERT(end * page_size <=
               reinterpret_cast<size_t>(std::to_address(area.end())));
  BOOST_ASSERT((end + 1) * page_size >
               reinterpret_cast<size_t>(std::to_address(area.end())));
  // NOLINTEND

  if (begin >= end) {
    return std::nullopt;
  }
  return {{.begin = begin, .count = end - begin}};
}

inline auto pgrng2memarea(pte_range range) -> mem_area {
  // NOLINTNEXTLINE(*-reinterpret-cast,performance-no-int-to-ptr)
  char *begin = reinterpret_cast<char *>(range.begin * page_size);
  size_t size = range.count * page_size;
  return {begin, size};
}

} // namespace simple_parallel