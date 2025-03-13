#pragma once

#include <algorithm>
#include <boost/assert.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/mpi.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/optional/optional.hpp>
#include <cstddef>
#include <gsl/assert>
#include <gsl/util>
#include <iterator>
#include <memory>
#include <ranges>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace simple_parallel {

namespace bmpi = boost::mpi;

template <typename rng>
  requires std::ranges::range<rng> &&
           std::is_trivially_copyable_v<std::ranges::range_value_t<rng>>
class dynamic_schedule
    : public std::ranges::view_interface<dynamic_schedule<rng>> {

  using iter_t = std::ranges::iterator_t<rng>;
  using sentinel_t = std::ranges::sentinel_t<rng>;

  using val_t = std::ranges::range_value_t<rng>;

  // copied from C++ 23:
  // https://en.cppreference.com/w/cpp/iterator/iter_t
  template <std::indirectly_readable T>
  using iter_const_reference_t =
      std::common_reference_t<const std::iter_value_t<T> &&,
                              std::iter_reference_t<T>>;

  // `val_t` if iter returns `val_t`, `const val_t &` if iter returns `val_t &`
  using yield_val_t = iter_const_reference_t<iter_t>;

  rng m_range;
  bmpi::communicator m_comm;
  size_t m_buffer_size;

  static constexpr size_t m_root_rank = 0;

public:
  // NOLINTNEXTLINE(*rvalue-reference-param-not-moved)
  dynamic_schedule(rng &&range, bmpi::communicator comm, size_t buffer_size = 0)
      : m_range(std::forward<rng>(range)), m_comm(std::move(comm)) {

    if (buffer_size == 0) {
      m_buffer_size = static_cast<size_t>(std::thread::hardware_concurrency());
    } else {
      m_buffer_size = buffer_size;
    }
  }

  dynamic_schedule(const dynamic_schedule &) = delete;
  auto operator=(const dynamic_schedule &) -> dynamic_schedule & = delete;
  dynamic_schedule(dynamic_schedule &&) = delete;
  auto operator=(dynamic_schedule &&) -> dynamic_schedule & = delete;

  ~dynamic_schedule() = default;

  class iterator {

    class handler {
    public:
      handler() = default;
      [[nodiscard]] virtual auto done() const noexcept -> bool = 0;
      [[nodiscard]] virtual auto get_value() const -> yield_val_t = 0;
      virtual void next() = 0;

      handler(const handler &) = delete;
      handler(handler &&) = delete;
      auto operator=(const handler &) -> handler & = delete;
      auto operator=(handler &&) -> handler & = delete;
      virtual ~handler() = default;
    };

    // NOLINTNEXTLINE(*enum-size)
    enum mpi_tag : int {
      client_request_val,
      server_send_val,
      val_gen_done,
      client_finished,
    };

    class server_handler : public handler {
      iter_t m_iter;
      sentinel_t m_end;
      bmpi::communicator m_comm;
      std::vector<boost::circular_buffer<bmpi::request>> m_requests;

      // In the beginning, each rank send a lot of requests to the server. If
      // we handle them one by one, the task may not be evenly distributed. So
      // we loop over all ranks and assign one task to each rank in each loop.
      void first_time_handle_client_request(std::vector<size_t> buffer_sizes) {
        auto handled = [](size_t buffer_size) { return buffer_size == 0; };
        while (!std::ranges::all_of(buffer_sizes, handled)) {
          for (size_t i = 0; i < gsl::narrow_cast<size_t>(m_comm.size()); i++) {
            if (i == m_root_rank) {
              continue;
            }
            if (buffer_sizes[i] == 0) {
              continue;
            }
            if (done()) {
              return;
            }

            auto &requests = m_requests[i];
            size_t &buffer_size = buffer_sizes[i];

            BOOST_ASSERT(!requests.full());
            BOOST_ASSERT(
                m_comm.iprobe(gsl::narrow_cast<int>(i), client_request_val)
                    .has_value());

            const val_t &val = *m_iter;
            // NOLINTBEGIN(*-reinterpret-cast)
            bmpi::request req = m_comm.isend(
                gsl::narrow_cast<int>(i), server_send_val,
                reinterpret_cast<const char *>(std::addressof(val)),
                sizeof(val));
            // NOLINTEND(*-reinterpret-cast)
            requests.push_back(std::move(req));
            m_iter++;
            BOOST_ASSERT(buffer_size > 0);
            buffer_size--;
          }
        }
      }

      void handle_client_request() {

        while (!done()) {
          const boost::optional<bmpi::status> &recv =
              m_comm.iprobe(bmpi::any_source, client_request_val);

          if (!recv.has_value()) {
            break;
          }

          const int source = recv->source();
          auto &requests = m_requests[gsl::narrow_cast<size_t>(source)];

          m_comm.recv(source, client_request_val);
          const val_t &val = *m_iter;
          while (!requests.empty() && requests.front().test().has_value()) {
            requests.pop_front();
          }
          BOOST_ASSERT(!requests.full());
          static_assert(bmpi::is_mpi_datatype<char>::value);
          bmpi::request req = m_comm.isend(
              recv->source(), server_send_val,
              // NOLINTNEXTLINE(*-reinterpret-cast)
              reinterpret_cast<const char *>(std::addressof(val)), sizeof(val));
          requests.push_back(std::move(req));
          m_iter++;
        }
        if (done()) {
          finish_cleanup();
          return;
        }
      }

      void finish_cleanup() {
        // send a message to all clients that the value generation is done
        std::vector<bmpi::request> end_requests;
        for (int i = 0; i < m_comm.size(); i++) {
          if (i == m_root_rank) {
            continue;
          }
          end_requests.push_back(m_comm.isend(i, val_gen_done));
        }

        for (auto &req : end_requests) {
          req.wait();
        }

        for (int i = 0; i < m_comm.size(); i++) {
          if (i == m_root_rank) {
            continue;
          }
          m_comm.recv(i, client_finished);
        }

        // consume all additional requests so MPI can free the communication
        // buffer
        while (const boost::optional<bmpi::status> &recv =
                   m_comm.iprobe(bmpi::any_source, client_request_val)) {
          m_comm.recv(recv->source(), client_request_val);
        }
      }

    public:
      server_handler() = default;
      server_handler(server_handler &&) = delete;
      auto operator=(server_handler &&) -> server_handler & = delete;
      server_handler(const server_handler &) = delete;
      auto operator=(const server_handler &) -> server_handler & = delete;

      server_handler(rng &range, bmpi::communicator comm,
                     std::vector<size_t> buffer_sizes)
          : m_iter(std::ranges::begin(range)), m_end(std::ranges::end(range)),
            m_comm(std::move(comm)) {
        for (const size_t &buffer_size : buffer_sizes) {
          m_requests.emplace_back(buffer_size);
        }
        // wait until client send requests
        m_comm.barrier();
        first_time_handle_client_request(std::move(buffer_sizes));
        handle_client_request();
      }
      ~server_handler() override {
        for (auto &requests : m_requests) {
          for (auto &req : requests) {
            req.wait();
          }
        }
      }

      [[nodiscard]] auto done() const noexcept -> bool override {
        return m_iter == m_end;
      }

      [[nodiscard]] auto get_value() const -> yield_val_t override {
        return *m_iter;
      }

      void next() override {
        m_iter++;
        handle_client_request();
      }
    };

    class client_handler : public handler {
      val_t m_value;
      bmpi::communicator m_comm;
      boost::circular_buffer<bmpi::request> m_requests;
      bool m_leader_gen_finished{false};
      bool m_totally_finished{false};

      [[nodiscard]] auto has_val_in_buffer() const -> bool {
        return m_comm.iprobe(m_root_rank, server_send_val).has_value();
      }

      auto leader_gen_finished() -> bool {
        if (m_leader_gen_finished) {
          return true;
        }
        if (const auto &finish_status =
                m_comm.iprobe(m_root_rank, val_gen_done)) {
          m_comm.recv(m_root_rank, val_gen_done);
          m_leader_gen_finished = true;
          return true;
        }
        return false;
      }

    public:
      client_handler() = default;
      client_handler(client_handler &&) = delete;
      auto operator=(client_handler &&) -> client_handler & = delete;
      client_handler(const client_handler &) = delete;
      auto operator=(const client_handler &) -> client_handler & = delete;

      client_handler(bmpi::communicator comm, size_t buffer_size)
          : m_comm(std::move(comm)), m_requests(buffer_size) {

        while (!m_requests.empty() && m_requests.front().test()) {
          m_requests.pop_front();
        }

        const size_t reserve_size = m_requests.reserve();
        for (size_t i = 0; i < reserve_size; i++) {
          m_requests.push_back(m_comm.isend(m_root_rank, client_request_val));
        }

        m_comm.barrier();
        next();
      }

      ~client_handler() override {
        BOOST_ASSERT(!has_val_in_buffer());
        for (auto &req : m_requests) {
          if (req.active()) {
            req.cancel();
          }
        }
      }

      [[nodiscard]] auto done() const noexcept -> bool override {
        return m_totally_finished;
      }

      [[nodiscard]] auto get_value() const -> yield_val_t override {
        return m_value;
      }

      void next() override {
        BOOST_ASSERT(!m_totally_finished);

        while (!has_val_in_buffer()) {
          if (leader_gen_finished()) {
            if (has_val_in_buffer()) {
              break;
            }
            m_totally_finished = true;
            m_comm.send(m_root_rank, client_finished);
            return;
          }
        }

        BOOST_ASSERT(has_val_in_buffer());
        // NOLINTBEGIN(*-reinterpret-cast)
        m_comm.recv(m_root_rank, server_send_val,
                    reinterpret_cast<char *>(std::addressof(m_value)),
                    sizeof(m_value));
        // NOLINTEND(*-reinterpret-cast)

        BOOST_ASSERT(m_requests.front().test().has_value());
        m_requests.pop_front();
        m_requests.push_back(m_comm.isend(m_root_rank, client_request_val));
      }
    };

    class single_rank_handler : public handler {
      iter_t m_iter;
      sentinel_t m_end;

    public:
      single_rank_handler() = default;
      explicit single_rank_handler(rng &range)
          : m_iter(std::ranges::begin(range)), m_end(std::ranges::end(range)) {}

      [[nodiscard]] auto done() const noexcept -> bool override {
        return m_iter == m_end;
      }

      [[nodiscard]] auto get_value() const -> yield_val_t override {
        return *m_iter;
      }

      void next() override { ++m_iter; }
    };

    std::unique_ptr<handler> m_handler;

  public:
    using difference_type = std::ptrdiff_t;
    using value_type = val_t;

    auto operator==(std::default_sentinel_t /*unused*/) const noexcept -> bool {
      return m_handler->done();
    }

    auto operator*() const -> yield_val_t { return m_handler->get_value(); }

    auto operator++() -> iterator & {
      m_handler->next();
      return *this;
    }

    void operator++(int) { ++(*this); }

    iterator(rng &range, bmpi::communicator comm, size_t buffer_size) {

      if (comm.size() == 1) {
        m_handler = std::make_unique<single_rank_handler>(range);
        return;
      }

      if (comm.rank() == m_root_rank) {
        buffer_size = 0;
      }

      std::vector<size_t> all_buffer_sizes{};
      bmpi::all_gather(comm, buffer_size, all_buffer_sizes);

      if (comm.rank() == m_root_rank) {
        m_handler = std::make_unique<server_handler>(
            range, std::move(comm), std::move(all_buffer_sizes));
      } else {
        m_handler =
            std::make_unique<client_handler>(std::move(comm), buffer_size);
      }
    }
  };

  // static_assert(std::input_iterator<iterator>);

  class sentinel {};

  [[nodiscard]] auto begin() -> iterator {
    return {m_range, m_comm, m_buffer_size};
  }

  [[nodiscard]] auto end() const noexcept -> std::default_sentinel_t {
    return {};
  }
};

template <typename rng, typename... Args>
dynamic_schedule(rng &&, Args...) -> dynamic_schedule<rng>;

// NOLINTBEGIN(*c-arrays)
// static_assert(std::ranges::range<dynamic_schedule<int[1]>>);
// static_assert(std::ranges::range<dynamic_schedule<int (&)[1]>>);
// NOLINTEND(*c-arrays)

} // namespace simple_parallel