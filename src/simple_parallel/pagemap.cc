#include "simple_parallel/cxx/types_fwd.h"
#include <boost/assert.hpp>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <gsl/util>
#include <iostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

#include <pagemap.h>

namespace {

using namespace simple_parallel;

class pagemap {
  int m_fd;

  // NOLINTNEXTLINE(*-vararg)
  pagemap() : m_fd(open("/proc/self/pagemap", O_RDONLY)) {
    if (m_fd == -1) {
      std::stringstream ss;
      ss << "Failed to open /proc/self/pagemap, reason: "
         // NOLINTNEXTLINE(concurrency-mt-unsafe)
         << std::strerror(errno);
      throw std::runtime_error(ss.str());
    }
  }

  ~pagemap() {
    if (close(m_fd) == -1) {
      perror("close");
    };
  }

public:
  pagemap(const pagemap &) = delete;
  pagemap(pagemap &&) = delete;
  auto operator=(const pagemap &) -> pagemap & = delete;
  auto operator=(pagemap &&) -> pagemap & = delete;

  static auto instance() -> const pagemap & {
    const static pagemap inst;
    return inst;
  }

  void read(std::span<uint64_t> res, pte_range rng) const {

    BOOST_ASSERT(res.size() == rng.count);

    // sadly we can not use ifstream here because we can not control its
    // pre-read behaviour, but Linux requires the read be a multiple of 8
    if (pread(m_fd, res.data(), res.size_bytes(),
              gsl::narrow_cast<off_t>(rng.begin * sizeof(uint64_t))) == -1) {
      std::stringstream ss;
      ss << "Failed to read /proc/self/pagemap, reason: "
         // NOLINTNEXTLINE(concurrency-mt-unsafe)
         << std::strerror(errno);
      throw std::runtime_error(ss.str());
    }
  }
};

} // namespace

namespace simple_parallel {

void clear_soft_dirty() {
  static std::ofstream clear_refs("/proc/self/clear_refs");
  if (!clear_refs) {
    throw std::runtime_error("Failed to open /proc/self/clear_refs");
  }

  // for this magic number, see
  // https://www.kernel.org/doc/html/latest/admin-guide/mm/soft-dirty.html
  clear_refs << 4;
  clear_refs.flush();
}

void get_pte(pte_range range, std::span<uint64_t> res) {
  pagemap::instance().read(res, range);
}

auto get_pte(pgnum page_num) -> pte {
  uint64_t res{};
  pagemap::instance().read({&res, 1}, {.begin = page_num, .count = 1});
  return pte{res};
}

} // namespace simple_parallel