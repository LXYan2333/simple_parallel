#pragma once

#include <boost/mpi.hpp>
#include <boost/mpi/communicator.hpp>
#include <chrono>
#include <cppcoro/generator.hpp>
#include <cstddef>
#include <cstdint>
#include <mpi.h>
#include <optional>
#include <ratio>
#include <simple_parallel/async_util.h>
#include <simple_parallel/detail.h>
#include <thread>
#include <type_traits>

namespace bmpi = boost::mpi;

namespace simple_parallel::detail {
    enum mpi_tag : int {
        client_request_task,
        server_send_task,
        all_task_generated,
        client_finished,
    };

    template <std::ranges::range T>
    auto dynamic_schedule_server(bmpi::communicator& communicator,
                                 T                   task_generator)
        -> simple_parallel::async_util::server_coroutine_handler {

        // server runs on rank 0
        assert(communicator.rank() == 0);

        // used to `test()` async send result
        // the `test()` is necessary in boost mpi. see
        // https://www.boost.org/doc/libs/1_84_0/doc/html/mpi/tutorial.html#mpi.tutorial.point_to_point.nonblocking
        std::vector<bmpi::request> pending_send_requests;

        // for all value in the task generator
        for (const auto& task : task_generator) {
            while (true) {
                // remove all fullfilled async send request
                std::erase_if(pending_send_requests,
                              [](bmpi::request& request) {
                                  return request.test().has_value();
                              });
                // if any client request a task
                if (auto status = communicator.iprobe(boost::mpi::any_source,
                                                      client_request_task)) {
                    communicator.recv(status->source(), client_request_task);
                    // send task to it
                    pending_send_requests.emplace_back(communicator.isend(
                        status->source(), server_send_task, std::move(task)));
                    // break the while loop
                    break;
                }
                // if no client request task, suspend the coroutine and try
                // again after resume
                co_await std::suspend_always();
            }
        }

        // all tasks generated, make sure all async send request are fullfilled
        while (!pending_send_requests.empty()) {
            std::erase_if(pending_send_requests, [](bmpi::request& request) {
                return request.test().has_value();
            });
            co_await std::suspend_always();
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
            co_await std::suspend_always();
        }

        // make sure all client request end
        int unfinished_rank_count = communicator.size();
        while (unfinished_rank_count != 0) {
            while (auto status = communicator.iprobe(boost::mpi::any_source,
                                                     client_finished)) {
                communicator.recv(status->source(), client_finished);
                unfinished_rank_count--;
            }
            co_await std::suspend_always();
        }

        // consume all additional task request so MPI can free the communication
        // buffer
        while (auto status = communicator.iprobe(boost::mpi::any_source,
                                                 client_request_task)) {
            communicator.recv(status->source(), client_request_task);
        }

        // all job done
        co_return;
    }

    template <typename T>
    auto dynamic_schedule_client(size_t              prefetch_count,
                                 bmpi::communicator& communicator)
        -> simple_parallel::async_util::client_coroutine_handler<T> {

        // used to `test()` async send result
        // the `test()` is necessary in boost mpi. see
        // https://www.boost.org/doc/libs/1_84_0/doc/html/mpi/tutorial.html#mpi.tutorial.point_to_point.nonblocking
        std::vector<bmpi::request> pending_send_requests{prefetch_count + 3};

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
                T task;
                communicator.recv(0, server_send_task, task);
                pending_send_requests.emplace_back(
                    communicator.isend(0, client_request_task));
                // yield the task
                co_yield std::optional{std::move(task)};
            } else {
                // if no task is received, suspend the coroutine
                co_await std::suspend_always();
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

    /**
     * @brief generate task from a generator and load balance them to all
     * avaliable thread in a distributed memory environment
     *
     * @tparam U the type of task to generate. e.g. `int`, `std::pair<int,int>`,
     * etc. the task must be a MPI datatype or be able to be serialized by
     * `Boost.Serialization` library. the easiest way to fulfill this
     * requirement is to use a std container. for more detail about boost
     * serialization requirements, see
     * https://www.boost.org/doc/libs/1_84_0/doc/html/mpi/tutorial.html#mpi.tutorial.user_data_types
     * @tparam T a range of task type that can be range-based for. e.g.
     * `std::vector<int>`, `cppcoro::generator<int>`, etc.
     * @param prefetch_count the number of task to prefetch from the server. the
     * server is not always active, so we prefetch somes task to each MPI
     * process, so we can hide one task's comminication latency using another
     * task's calculation time.
     * @param generator a generator that generate all tasks, or a container that
     * can be range-based for.
     * @param server_first_start_delay optional parameter. the server is delayed
     * several milliseconds before it starts to generate task, to make sure all
     * MPI processes have send their request to the server.
     * @return cppcoro::generator<T> the generator that generate tasks in
     * distributed memory environment. it is not thread safe and must guarded by
     * some kind of lock. e.g. `std::mutex`, `#pragma omp critical`, etc.
     * depends on the MPI threading level, this lock may also be used to guard
     * other MPI calls in your application. see
     * https://www.boost.org/doc/libs/1_84_0/doc/html/mpi/tutorial.html#mpi.tutorial.threading
     */
    template <typename U, std::ranges::range T>
        requires requires(T t) {
            { *t.begin() } -> std::convertible_to<U>;
        }
    auto dynamic_schedule(
        size_t                                     prefetch_count,
        T                                          generator,
        std::chrono::duration<int64_t, std::milli> server_first_start_delay =
            std::chrono::milliseconds{10})
        // returns a generator that can generate values across several nodes.
        -> cppcoro::generator<U> {

        using task_type = std::remove_reference_t<decltype(*generator.begin())>;

        static_assert(std::is_same_v<task_type, U>);

        // a new communicator is created, dedicated for this generator
        bmpi::communicator generator_comm{comm, bmpi::comm_duplicate};

        // the client runs on all ranks, the server only runs on rank 0
        auto client =
            dynamic_schedule_client<task_type>(prefetch_count, generator_comm);
        auto server =
            dynamic_schedule_server(generator_comm, std::move(generator));

        bool first_run = true;

        while (true) {
            // update the client. if the task is not nullopt
            if (auto task = client.next_task()) {
                // yield value
                co_yield task.value();
            }
            // on rank 0
            if (generator_comm.rank() == 0) {
                // update the server
                if (!server.handle.done()) {
                    if (first_run) [[unlikely]] {
                        first_run = false;
                        std::this_thread::sleep_for(server_first_start_delay);
                    }
                    server.handle.resume();
                }
                // if the client and server is done on rank 0, break the loop
                if (client.handle.done() and server.handle.done()) {
                    break;
                }
            } else {
                // on other ranks, if the client is done, break the loop
                if (client.handle.done()) {
                    break;
                }
            }
        }
    }
} // namespace simple_parallel::detail
