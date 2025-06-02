#include <cstdint>
#include <gsl/pointers>
#include <iterator>
#include <mpi.h>
#include <ranges>
#include <simple_parallel/cxx/dynamic_schedule.h>

using namespace simple_parallel;
namespace bmpi = boost::mpi;

struct optional_buffer {
  bool has_value;
  // NOLINTNEXTLINE(*avoid-c-arrays)
  int64_t buffer[8];
};

extern "C" auto s_p_scheduler_get_next(void *generator) -> optional_buffer;

namespace {

class fortran_task_generator {
  void *m_fortran_parallel_scheduler;

public:
  class iterator {
    void *m_fortran_parallel_scheduler;
    optional_buffer m_buffer{};

  public:
    using difference_type = std::ptrdiff_t;
    using value_type = optional_buffer;

    explicit iterator(void *fortran_parallel_scheduler)
        : m_fortran_parallel_scheduler(fortran_parallel_scheduler),
          m_buffer(s_p_scheduler_get_next(fortran_parallel_scheduler)) {}

    auto operator==(std::default_sentinel_t /*unused*/) const noexcept -> bool {
      return !m_buffer.has_value;
    }

    auto operator*() const -> const optional_buffer & { return m_buffer; }

    auto operator++() -> iterator & {
      m_buffer = s_p_scheduler_get_next(m_fortran_parallel_scheduler);
      return *this;
    }

    void operator++(int) { ++(*this); }
  };

  explicit fortran_task_generator(void *fortran_parallel_scheduler)
      : m_fortran_parallel_scheduler(fortran_parallel_scheduler) {}

  auto begin() -> iterator { return iterator{m_fortran_parallel_scheduler}; }

  static auto end() -> std::default_sentinel_t { return {}; }

  static_assert(std::input_iterator<iterator>);
};

static_assert(std::ranges::range<fortran_task_generator>);

} // namespace

using generator_t = dynamic_schedule<fortran_task_generator>;

struct s_p_f_dynamic_schedule {
  std::unique_ptr<generator_t> m_schedule;
  std::ranges::iterator_t<generator_t> m_iter;
  std::ranges::sentinel_t<generator_t> m_end;

  explicit s_p_f_dynamic_schedule(std::unique_ptr<generator_t> schedule)
      : m_schedule(std::move(schedule)), m_iter(m_schedule->begin()),
        m_end(m_schedule->end()) {}
};

extern "C" auto s_p_f_get_detail(void *fortran_parallel_scheduler,
                                 int fortran_communicator, int buffer_size)
    -> gsl::owner<void *> {
  auto schedule = std::make_unique<generator_t>(
      fortran_task_generator{fortran_parallel_scheduler},
      bmpi::communicator{MPI_Comm_f2c(fortran_communicator), bmpi::comm_attach},
      buffer_size);
  return new s_p_f_dynamic_schedule{std::move(schedule)};
};

extern "C" void s_p_f_delete_detail(gsl::owner<void *> schedule) {
  delete static_cast<s_p_f_dynamic_schedule *>(schedule);
};

extern "C" auto s_p_f_get_buffer(void *detail) -> const optional_buffer * {
  return &(*(static_cast<s_p_f_dynamic_schedule *>(detail)->m_iter));
};

extern "C" auto s_p_f_done(void *detail) -> bool {
  auto *scheduler = static_cast<s_p_f_dynamic_schedule *>(detail);
  return scheduler->m_iter == scheduler->m_end;
}

extern "C" void s_p_f_parallel_scheduler_advance(void *detail) {
  static_cast<s_p_f_dynamic_schedule *>(detail)->m_iter++;
}