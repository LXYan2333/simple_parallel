#include <boost/assert.hpp>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <span>
#include <unistd.h>

#include <pagemap.h>

namespace {

using namespace simple_parallel;

class pagemap {
  int m_fd;

public:
  // NOLINTNEXTLINE(*-vararg)
  pagemap() : m_fd(open("/proc/self/pagemap", O_RDONLY)) {
    if (m_fd == -1) {
      perror("open");
      std::cerr << "Failed to open /proc/self/pagemap\n";
      std::terminate();
    }
  }

  pagemap(const pagemap &) = delete;
  pagemap(pagemap &&) = delete;
  auto operator=(const pagemap &) -> pagemap & = delete;
  auto operator=(pagemap &&) -> pagemap & = delete;

  void read(std::span<uint64_t> res, pte_range rng) const {

    BOOST_ASSERT(res.size() == rng.count);

    // sadly we can not use ifstream here because we can not control its
    // pre-read behaviour, but Linux requires the read be a multiple of 8
    if (pread(m_fd, res.data(), res.size_bytes(),
              static_cast<off_t>(rng.begin * sizeof(uint64_t))) == -1) {
      perror("pread");
      std::cerr << "Failed to read /proc/self/pagemap\n";
      std::terminate();
    }
  }

  ~pagemap() {
    if (close(m_fd) == -1) {
      perror("close");
    };
  }
};

// NOLINTNEXTLINE(cert-err58-cpp)
const pagemap pagemap_instance;

} // namespace

namespace simple_parallel {

void clear_soft_dirty() {
  static std::ofstream clear_refs("/proc/self/clear_refs");
  if (!clear_refs) {
    std::cerr << "Failed to open /proc/self/clear_refs\n";
    std::terminate();
  }

  // for this magic number, see
  // https://www.kernel.org/doc/html/latest/admin-guide/mm/soft-dirty.html
  clear_refs << 4;
  clear_refs.flush();
}

auto addr2pgnum(void *addr) -> pgnum {
  // NOLINTNEXTLINE(*-reinterpret-cast)
  return reinterpret_cast<size_t>(addr) / page_size;
}

auto memarea2pgrng(mem_area area) -> pte_range {
  BOOST_ASSERT(area.size() != 0);
  pgnum begin = addr2pgnum(area.data());
  pgnum end = addr2pgnum(area.end().base() - 1) + 1;
  return {.begin = begin, .count = end - begin};
}

auto pgrng2memarea(pte_range range) -> mem_area {
  // NOLINTNEXTLINE(*-reinterpret-cast,performance-no-int-to-ptr)
  char *begin = reinterpret_cast<char *>(range.begin * page_size);
  size_t size = range.count * page_size;
  return {begin, size};
}

void get_pte(pte_range range, std::span<uint64_t> res) {
  pagemap_instance.read(res, range);
}

auto get_pte(pgnum page_num) -> pte {
  uint64_t res{};
  pagemap_instance.read({&res, 1}, {.begin = page_num, .count = 1});
  return pte{res};
}

} // namespace simple_parallel