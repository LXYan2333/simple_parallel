#pragma once
#include <simple_parallel/simple_parallel.h>

#include <array>
#include <bit>
#include <boost/circular_buffer.hpp>
#include <boost/mpi.hpp>
#include <cassert>
#include <cppcoro/generator.hpp>
#include <cstddef>
#include <memory>
#include <mpi.h>
#include <mutex>
#include <optional>
#include <ranges>
#include <simple_parallel/detail.h>
#include <thread>
#include <type_traits>
#include <vector>

namespace bmpi = boost::mpi;

namespace simple_parallel::detail {

    template <std::ranges::range T>
    class dynamic_schedule {
      public:
        using task_type = std::ranges::range_value_t<T>;

      private:
        bmpi::communicator communicator;
        T                  task_generator;
        size_t             prefetch_count{};
        int                comm_size{};

        std::thread server_thread;
        std::thread client_thread;


        std::unique_ptr<std::mutex>       rank_tasks_buffer_lock;
        boost::circular_buffer<task_type> rank_task_buffer;
        bool                              finished{false};

        enum mpi_tag : int {
            client_request_task,
            server_send_task,
            all_task_generated,
            client_finished,
        };

        auto server() -> cppcoro::generator<bool> {

            assert(communicator.rank() == 0);

            // used to `test()` async send result
            // the `test()` is necessary in boost mpi. see
            // https://www.boost.org/doc/libs/1_84_0/doc/html/mpi/tutorial.html#mpi.tutorial.point_to_point.nonblocking
            // todo: maybe use something like circular buffer?
            std::vector<bmpi::request> pending_send_requests;

            auto ranks_iota = std::views::iota(0, comm_size);
            auto ranks      = loop_forever(ranks_iota);

            // wait until all client request task, thus the tasks can be evenly
            // distributed
            while (true) {
                bool should_exit = true;
                for (const int& rank : ranks_iota) {
                    if (!communicator.iprobe(rank, client_request_task)
                             .has_value()) {
                        should_exit = false;
                        co_yield false;
                        break;
                    }
                }
                if (should_exit) {
                    break;
                }
            }

            for (auto&& i : task_generator) {

                // if any client request a task
                int tried_ranks = 0;
                for (const int& rank : ranks) {
                    // remove all fullfilled async send request
                    std::erase_if(pending_send_requests,
                                  [](bmpi::request& request) {
                                      return request.test().has_value();
                                  });

                    if (auto status =
                            communicator.iprobe(rank, client_request_task)) {


                        communicator.recv(status->source(),
                                          client_request_task);

                        // send task to it
                        if constexpr (std::is_trivially_copyable_v<task_type>) {
                            // Objects of trivially-copyable types that are
                            // not potentially-overlapping subobjects are
                            // the only C++ objects that may be safely
                            // copied with std::memcpy or serialized to/from
                            // binary files with std::ofstream::write() /
                            // std::ifstream::read().
                            //
                            // https://en.cppreference.com/w/cpp/types/is_trivially_copyable
                            static_assert(bmpi::is_mpi_datatype<char>::value);
                            pending_send_requests.push_back(communicator.isend(
                                status->source(),
                                server_send_task,
                                std::bit_cast<char*>(std::addressof(i)),
                                sizeof(i)));
                        } else {
                            pending_send_requests.push_back(communicator.isend(
                                status->source(), server_send_task, i));
                        }
                        // break the ranks for loop
                        break;
                    }
                    tried_ranks++;
                    if (tried_ranks == comm_size) {
                        tried_ranks = 0;
                        co_yield false;
                    }
                }
            }


            // all tasks generated, make sure all async send request are
            // fullfilled
            while (!pending_send_requests.empty()) {
                std::erase_if(pending_send_requests,
                              [](bmpi::request& request) {
                                  return request.test().has_value();
                              });
                co_yield false;
            }

            // tell all client that all tasks have beed generated
            std::vector<bmpi::request> pending_send_end_requests;
            pending_send_end_requests.reserve(
                static_cast<size_t>(communicator.size()));
            for (int i = 0; i < communicator.size(); i++) {
                pending_send_end_requests.emplace_back(
                    communicator.isend(i, all_task_generated));
            }
            while (!pending_send_end_requests.empty()) {
                std::erase_if(pending_send_end_requests,
                              [](bmpi::request& request) {
                                  return request.test().has_value();
                              });
                co_yield false;
            }

            // make sure all client request end
            int unfinished_rank_count = comm_size;
            while (unfinished_rank_count != 0) {
                while (auto status = communicator.iprobe(boost::mpi::any_source,
                                                         client_finished)) {
                    communicator.recv(status->source(), client_finished);
                    unfinished_rank_count--;
                }
                co_yield false;
            }

            // consume all additional task request so MPI can free the
            // communication buffer
            while (auto status = communicator.iprobe(boost::mpi::any_source,
                                                     client_request_task)) {
                communicator.recv(status->source(), client_request_task);
            }

            co_yield true;
        }

        auto client_task_gen() -> cppcoro::generator<std::optional<task_type>> {
            // used to `test()` async send result
            // the `test()` is necessary in boost mpi. see
            // https://www.boost.org/doc/libs/1_84_0/doc/html/mpi/tutorial.html#mpi.tutorial.point_to_point.nonblocking
            std::vector<bmpi::request> pending_send_requests{prefetch_count
                                                             + 3};

            // make `prefetch_count` request to the server
            for (size_t i = 0; i < prefetch_count; i++) {
                pending_send_requests.emplace_back(
                    communicator.isend(0, client_request_task));
            }

            // if any task is received and the server haven't send all task
            while (!communicator.iprobe(0, all_task_generated)
                   or communicator.iprobe(0, server_send_task).has_value()) {
                // remove all fullfilled async send request
                std::erase_if(pending_send_requests,
                              [](bmpi::request& request) -> bool {
                                  return request.test().has_value();
                              });
                // if any task is received
                if (communicator.iprobe(0, server_send_task).has_value()) {
                    task_type task;
                    if constexpr (std::is_trivially_copyable_v<task_type>) {
                        communicator.recv(
                            0,
                            server_send_task,
                            std::bit_cast<char*>(std::addressof(task)),
                            sizeof(task));
                    } else {
                        communicator.recv(0, server_send_task, task);
                    }
                    pending_send_requests.emplace_back(
                        communicator.isend(0, client_request_task));
                    // yield the task
                    co_yield std::optional{std::move(task)};
                } else {
                    // if no task is received, suspend the coroutine
                    co_yield std::nullopt;
                }
            }
            communicator.recv(0, all_task_generated);

            // cancel all pending send request
            std::erase_if(pending_send_requests, [](bmpi::request& request) {
                return request.test().has_value();
            });
            for (auto& i : pending_send_requests) {
                if (i.active()) {
                    i.cancel();
                }
            }
            // tell server that this client has finished
            auto req = communicator.isend(0, client_finished);
            while (!req.test().has_value()) {
                co_yield std::nullopt;
            }
            co_return;
        }

        enum get_task_result {
            ok,
            temporarily_no_task,
            task_gen_finished,
            failed_to_lock
        };

        /**
         * @brief try to fill the buffer. may fail with reason. the buffer may
         * be not completely filled.
         *
         * @param buffer
         * @return get_task_result
         */
        auto try_fill_buffer(boost::circular_buffer<task_type>& buffer)
            -> get_task_result {
            assert(!buffer.full());
            std::unique_lock lock(*rank_tasks_buffer_lock, std::try_to_lock);
            if (lock.owns_lock()) {
                if (rank_task_buffer.empty()) {
                    if (finished) {
                        return task_gen_finished;
                    }
                    return temporarily_no_task;
                } else {
                    size_t tasks_to_take_num =
                        std::min(rank_task_buffer.size(), buffer.reserve());
                    auto tasks_to_take =
                        rank_task_buffer
                        | std::views::take(tasks_to_take_num)
                        // use std::views::as_rvalue is better, but gcc12
                        // doesn't support it
                        | std::views::transform(
                            [](auto&& task) { return std::move(task); });
                    buffer.insert(buffer.end(),
                                  tasks_to_take.begin(),
                                  tasks_to_take.end());
                    rank_task_buffer.erase_begin(tasks_to_take_num);
                    return ok;
                }
            }
            return failed_to_lock;
        }


      public:
        auto schedule() -> void {
            if (communicator.rank() == 0) {
                server_thread = std::thread([this] {
                    s_p_this_thread_should_be_proxied = false;
                    for (const auto& _ : server()) {
                        std::this_thread::yield();
                    }
                });
            }

            client_thread = std::thread([this] {
                s_p_this_thread_should_be_proxied = false;

                auto tasks = client_task_gen();
                // todo: reduce lock time?
                for (auto task : tasks) {
                    if (task.has_value()) {
                        while (true) {
                            std::lock_guard lock(*rank_tasks_buffer_lock);
                            if (rank_task_buffer.full()) {
                                std::this_thread::yield();
                            } else {
                                rank_task_buffer.push_back(
                                    std::move(task.value()));
                                break;
                            }
                        }
                    } else {
                        std::this_thread::yield();
                    }
                }
                std::lock_guard lock(*rank_tasks_buffer_lock);
                finished = true;
            });
        }

        template <typename U>
            requires std::is_same_v<T, std::remove_reference_t<U>>
        dynamic_schedule(const bmpi::communicator& communicator_,
                         U&&                       task_generator_,
                         size_t                    prefetch_count_)
            : communicator(communicator_, bmpi::comm_duplicate),
              task_generator(std::forward<U>(task_generator_)),
              prefetch_count(prefetch_count_),
              comm_size(communicator_.size()),
              rank_task_buffer(prefetch_count_),
              rank_tasks_buffer_lock(std::make_unique<std::mutex>()) {}

        /**
         * @brief Construct a null dynamic schedule object
         *
         */
        dynamic_schedule() = default;

        dynamic_schedule(const dynamic_schedule&)            = default;
        dynamic_schedule(dynamic_schedule&&)                 = default;
        dynamic_schedule& operator=(const dynamic_schedule&) = default;
        dynamic_schedule& operator=(dynamic_schedule&&)      = default;

        ~dynamic_schedule() {
            if (server_thread.joinable()) {
                server_thread.join();
            }
            if (client_thread.joinable()) {

                client_thread.join();
            }
        }

        /**
         * @brief every thread has its own thread_local task buffer, and should
         * assign it to this vector before dynamic schedule begin
         *
         * @tparam buffer_prefetch_threshold if the tasks in the buffer is less
         * than this value, buffer will try to get more tasks from the scheduler
         */
        template <size_t buffer_prefetch_threshold = 4, size_t buffer_size = 16>
        class thread_local_task_buffer {
            boost::circular_buffer<task_type> buffer;
            bool                              finished = false;

            dynamic_schedule* scheduler = nullptr;

            static_assert(buffer_size > buffer_prefetch_threshold);

          public:
            /**
             * @brief Construct a new thread local task buffer object
             *
             * @param buffer_size the buffer size. must be greater than 2
             * @param scheduler a pointer to the dynamic schedule object
             */
            explicit thread_local_task_buffer(dynamic_schedule* scheduler_)
                : buffer(buffer_size),
                  scheduler(scheduler_) {}

            /**
             * @brief if there are not enough tasks in local buffer, try
             * to get tasks from the scheduler without block. may fail
             * with reason. should not be called when the buffer is full
             *
             * @return get_task_result
             */
            auto try_aquiring_task() -> get_task_result {
                assert(!buffer.full());
                if (buffer.size() > buffer_prefetch_threshold) {
                    return ok;
                }
                return scheduler->try_fill_buffer(buffer);
            }

            /**
             * @brief get a task from the local buffer without any sync
             * operation. return std::nullopt if the buffer is empty
             *
             * @return std::optional<task_type>
             */
            auto pop() -> std::optional<task_type> {
                if (buffer.empty()) {
                    return std::nullopt;
                }
                task_type task = std::move(buffer.front());
                buffer.pop_front();
                return {std::move(task)};
            }
        };

        template <size_t buffer_prefetch_threshold = 4, size_t buffer_size = 16>
        auto gen(dynamic_schedule& scheduler) -> cppcoro::generator<task_type> {
            thread_local_task_buffer<buffer_prefetch_threshold, buffer_size>
                buffer{&scheduler};
            buffer.try_aquiring_task();
            while (true) {
                if (auto task = buffer.pop()) {
                    buffer.try_aquiring_task();
                    co_yield std::move(task).value();
                } else {
                    if (buffer.try_aquiring_task() == task_gen_finished) {
                        assert(!buffer.pop().has_value());
                        co_return;
                    }
                    std::this_thread::yield();
                }
            }
        };
    };


} // namespace simple_parallel::detail
