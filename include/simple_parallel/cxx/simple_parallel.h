#pragma once

#include <array>
#include <boost/mpi.hpp>
#include <cstddef>
#include <exception>
#include <memory>
#include <mpi.h>
#include <mutex>
#include <optional>
#include <ranges>
#include <simple_parallel/cxx/types_fwd.h>
#include <span>
#include <type_traits>
#include <ucontext.h>

namespace simple_parallel {
namespace bmpi = boost::mpi;

class reduce_area {
private:
  void *m_begin;
  size_t m_count;
  MPI_Datatype m_type;
  MPI_Op m_op;
  int m_sizeof_type{};
  std::optional<pte_range> m_inner_pages;

  [[nodiscard]] auto all_reduce(const bmpi::communicator &comm) const -> int;
  void init_reduce_area_on_worker() const;
  void init_inner_pages(std::optional<pte_range> &inner_pages);

  friend par_ctx_base;
  // use this to access private members in anonymous namespace, to hide
  // implementation details
  friend reduce_area_friend;
  explicit operator mem_area() const;

public:
  template <std::ranges::range rng>
    requires bmpi::is_mpi_builtin_datatype<
                 std::ranges::range_value_t<rng>>::value &&
                 std::ranges::contiguous_range<rng> &&
                 std::ranges::sized_range<rng>
  reduce_area(rng &area, MPI_Op op)
      : m_begin(std::ranges::data(area)), m_count(std::ranges::size(area)),
        m_type(bmpi::get_mpi_datatype<std::ranges::range_value_t<rng>>()),
        m_op(op), m_sizeof_type(sizeof(std::ranges::range_value_t<rng>)) {
    init_inner_pages(m_inner_pages);
  }

  template <typename T>
    requires bmpi::is_mpi_builtin_datatype<T>::value
  reduce_area(std::span<T> area, MPI_Op op)
      : m_begin(area.data()), m_count(area.size()),
        m_type(bmpi::get_mpi_datatype<std::remove_reference_t<T>>()), m_op(op),
        m_sizeof_type(sizeof(T)) {
    init_inner_pages(m_inner_pages);
  }

  template <typename T>
    requires bmpi::is_mpi_builtin_datatype<T>::value
  reduce_area(T &value, MPI_Op op)
      : m_begin(std::addressof(value)), m_count(1),
        m_type(bmpi::get_mpi_datatype<std::remove_reference_t<T>>()), m_op(op),
        m_sizeof_type(sizeof(T)) {
    init_inner_pages(m_inner_pages);
  }

  reduce_area(MPI_Datatype type, void *begin, size_t count, MPI_Op op)
      : m_begin(begin), m_count(count), m_type(type), m_op(op) {
    MPI_Type_size(type, &m_sizeof_type);
    init_inner_pages(m_inner_pages);
  }
};
class par_ctx_base {
private:
  ucontext_t m_parallel_ctx;
  ucontext_t m_sync_mem_ctx;
  bmpi::communicator *m_comm;
  static constexpr int m_root_rank = 0;

  std::span<const reduce_area> m_reduces;

  bool entered_parallel = false;

  void verify_reduces_no_overlap() const;

protected:
  explicit par_ctx_base(std::span<const reduce_area> reduces = {});
  void set_reduces(std::span<const reduce_area> reduces);
  void do_enter_parallel(bool enter_parallel);
  void do_exit_parallel();
  ~par_ctx_base() = default;

public:
  // non-copyable and non-moveable
  par_ctx_base(const par_ctx_base &) = delete;
  par_ctx_base(par_ctx_base &&) = delete;
  auto operator=(const par_ctx_base &) -> par_ctx_base & = delete;
  auto operator=(par_ctx_base &&) -> par_ctx_base & = delete;

  [[nodiscard]] auto get_comm() const -> const bmpi::communicator &;
};

template <size_t reduces_size> class par_ctx : public par_ctx_base {
  std::array<reduce_area, reduces_size> m_reduces;

public:
  par_ctx(const par_ctx &) = delete;
  par_ctx(par_ctx &&) = delete;
  auto operator=(const par_ctx &) -> par_ctx & = delete;
  auto operator=(par_ctx &&) -> par_ctx & = delete;

  explicit par_ctx(bool enter_parallel = true) {
    do_enter_parallel(enter_parallel);
  }

  template <typename... args>
    requires(std::is_same_v<reduce_area, args> && ...)
  explicit par_ctx(bool enter_parallel, const args &...reduces)
      : par_ctx_base(m_reduces), m_reduces(std::array{reduces...}) {
    do_enter_parallel(enter_parallel);
  }

  ~par_ctx() { do_exit_parallel(); }
};

explicit par_ctx(bool enter_parallel = true) -> par_ctx<0>;

template <typename... args>
explicit par_ctx(bool enter_parallel, args... reduces)
    -> par_ctx<sizeof...(args)>;

} // namespace simple_parallel