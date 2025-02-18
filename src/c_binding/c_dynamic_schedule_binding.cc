#include <boost/mpi.hpp>
#include <cstddef>
#include <cstdint>
#include <gsl/pointers>
#include <iterator>
#include <memory>
#include <mpi.h>
#include <ranges>
#include <simple_parallel/c/c_dynamic_schedule_binding.h>
#include <simple_parallel/cxx/dynamic_schedule.h>

using namespace simple_parallel;
namespace bmpi = boost::mpi;

namespace {

class c_task_generator {

  s_p_task_gen_func *m_task_gen_func;
  void *m_state;

public:
  class iterator {
    s_p_dyn_buffer m_buffer{};
    s_p_task_gen_func *m_task_gen_func;
    void *m_state;
    bool done;

  public:
    using difference_type = std::ptrdiff_t;
    using value_type = s_p_dyn_buffer;

    iterator(s_p_task_gen_func *task_gen_func, void *state)
        : m_task_gen_func(task_gen_func), m_state(state),
          done(m_task_gen_func(m_state, &m_buffer)) {}

    auto operator==(std::default_sentinel_t /*unused*/) const noexcept -> bool {
      return done;
    }

    auto operator*() const -> const s_p_dyn_buffer & { return m_buffer; }

    auto operator++() -> iterator & {
      done = m_task_gen_func(m_state, &m_buffer);
      return *this;
    }

    void operator++(int) { ++(*this); }
  };

  c_task_generator(s_p_task_gen_func *task_gen_func, void *state)
      : m_task_gen_func(task_gen_func), m_state(state) {}

  auto begin() -> iterator { return {m_task_gen_func, m_state}; }

  static auto end() -> std::default_sentinel_t { return {}; }

  static_assert(std::input_iterator<iterator>);
};

static_assert(std::ranges::range<c_task_generator>);

} // namespace

using generator_t = dynamic_schedule<c_task_generator>;

struct s_p_dynamic_schedule_s {
  std::unique_ptr<generator_t> m_schedule;
  std::ranges::iterator_t<generator_t> m_iter;
  std::ranges::sentinel_t<generator_t> m_end;

  explicit s_p_dynamic_schedule_s(std::unique_ptr<generator_t> schedule)
      : m_schedule(std::move(schedule)), m_iter(m_schedule->begin()),
        m_end(m_schedule->end()) {}
};

auto new_s_p_dynamic_schedule(s_p_task_gen_func *task_gen_func, void *state,
                              MPI_Comm communicator, size_t buffer_size)
    -> gsl::owner<s_p_dynamic_schedule *> {
  auto schedule = std::make_unique<generator_t>(
      c_task_generator{task_gen_func, state},
      bmpi::communicator{communicator, bmpi::comm_attach}, buffer_size);
  return new s_p_dynamic_schedule{std::move(schedule)};
};

auto s_p_get_buffer(s_p_dynamic_schedule *schedule) -> const s_p_dyn_buffer * {
  // this should be in the same place as the iterator always returns its
  // m_buffer
  return &*schedule->m_iter;
};

extern "C" void s_p_next(s_p_dynamic_schedule *schedule) {
  ++schedule->m_iter;
};

auto s_p_done(s_p_dynamic_schedule *schedule) -> bool {
  return schedule->m_iter == schedule->m_end;
};

void delete_s_p_dynamic_schedule(gsl::owner<s_p_dynamic_schedule *> schedule) {
  delete schedule;
};

// NOLINTNEXTLINE(*easily-swappable-parameters)
auto new_gss_state(uint64_t begin, uint64_t end, uint64_t grain_size,
                   uint64_t current_rank_process_count, MPI_Comm communicator)
    -> gss_state_t {
  size_t all_rank_process_count =
      bmpi::all_reduce({communicator, bmpi::comm_attach},
                       current_rank_process_count, std::plus<size_t>{});
  return {begin, end, grain_size, all_rank_process_count};
};

auto gss_generator(void *state, s_p_dyn_buffer *buffer) -> bool {
  auto *gss_state = static_cast<gss_state_t *>(state);
  static_assert(sizeof(simple_task) <= sizeof(s_p_dyn_buffer));
  // NOLINTNEXTLINE(*reinterpret-cast)
  auto *res = reinterpret_cast<simple_task *>(buffer);

  size_t remaining = gss_state->end - gss_state->current;
  if (remaining == 0) {
    return true;
  }

  size_t next = ((remaining - 1) / gss_state->all_rank_process_count) + 1;
  next = std::max(next, gss_state->grain_size);
  next = std::min(next, remaining);

  res->begin = gss_state->current;
  res->end = gss_state->current + next;

  gss_state->current += next;
  return false;
};