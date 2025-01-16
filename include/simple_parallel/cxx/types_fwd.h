#pragma once

#include <cstddef>
#include <span>

namespace simple_parallel {
using pgnum = size_t;
struct pte_range {
  pgnum begin;
  size_t count;
};
class par_ctx_base;

class reduce_area_friend;

using mem_area = std::span<char>;

} // namespace simple_parallel
