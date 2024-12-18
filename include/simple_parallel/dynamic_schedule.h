#pragma once

#include <algorithm>
#include <atomic>
#include <boost/circular_buffer.hpp>
#include <boost/circular_buffer/base.hpp>
#include <boost/mpi.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi/request.hpp>
#include <cassert>
#include <cppcoro/generator.hpp>
#include <cstddef>
#include <internal_use_only/include_concurrentqueue.h>
#include <latch>
#include <memory>
#include <mutex>
#include <ranges>
#include <semaphore>
#include <simple_parallel/detail.h>
#include <simple_parallel/simple_parallel.h>
#include <thread>
#include <type_traits>
#include <vector>

namespace bmpi = boost::mpi;

namespace simple_parallel::detail {

    template <std::ranges::range T, size_t thread_buffer_size = 4>
    // for performance, the task must be trivially copyable so MPI can send it
    // directly
        requires std::is_trivially_copyable_v<std::ranges::range_value_t<T>>
    class dynamic_schedule {
      public:
        using task_type = std::ranges::range_value_t<T>;

      private:
        bmpi::communicator communicator;
        T                  task_generator;
        int                comm_size{};
        int                comm_rank{};
        int                num_threads{};

        static constexpr bool task_t_trivially_copyable =
            std::is_trivially_copyable_v<task_type>;

        // number of threads on each rank
        std::vector<int> num_threads_on_each_rank;

        // a struct, used to track all the information of a thread on a rank
        struct thread {

            bmpi::communicator communicator;

            // used on master to save all the pending send request, thus they
            // can be `test`ed. see:
            // https://www.boost.org/doc/libs/1_84_0/doc/html/mpi/tutorial.html#mpi.tutorial.point_to_point.nonblocking
            boost::circular_buffer<bmpi::request> pending_send_request{
                thread_buffer_size};
        };

        // all the infomation of a rank. used on the master rank
        struct rank {
            int target_rank{};

            // a communicator only involving the master rank and the target rank
            bmpi::communicator comm_to_rank0;

            // all information of threads on the target rank
            // threads[0] is empty, the master rank will distribute tasks
            // through rank_tasks_buffer
            std::vector<thread> threads;
        };

        std::vector<rank> ranks;

        std::vector<bmpi::communicator> comm_to_master;
        // a mono increasing thread index (since this header is not desinged
        // specifically for OpenMP, omp_get_thread_num() is not available)
        std::atomic<size_t>             current_thread_index = 0;

        std::thread server_thread;

        moodycamel::ConcurrentQueue<task_type> rank_task_buffer;

        std::unique_ptr<moodycamel::ProducerToken> ptok;


        std::atomic<bool> finished{false};
        // used on rank != 0
        std::latch        finish_latch_1;
        std::latch        finish_latch_2;

        std::binary_semaphore one_master_thread_wait_for_server{0};

        enum mpi_tag : int {
            client_request_task,
            server_send_task,
            all_task_generated,
            client_finished,
        };

        static auto request_finished(bmpi::request& request) -> bool {
            return request.test().has_value();
        };

        static auto request_unfinished(bmpi::request& request) -> bool {
            return !request.test().has_value();
        };

        // all the logic of the server. the server will runs on rank 0 and send
        // tasks to rank 0's task queue and every thread on other ranks.
        auto server() -> cppcoro::generator<bool> {

            assert(communicator.rank() == 0);

            auto current_task  = task_generator.begin();
            auto sentinel_task = task_generator.end();

            const size_t rank0_buffer_size =
                thread_buffer_size * num_threads_on_each_rank.at(0);

            // only adds up to 2
            size_t run_times                                  = 0;
            bool   one_master_thread_wait_for_server_released = false;

            while (true) {
                for (rank& rank_ : ranks) {
                    if (rank_.target_rank == 0) {
                        while (rank_task_buffer.size_approx()
                               < rank0_buffer_size) {
                            if (current_task == sentinel_task) {
                                goto task_gen_finished_label;
                            }
                            rank_task_buffer.enqueue(*ptok,
                                                     std::move(*current_task));
                            current_task++;
                        }
                        continue;
                    }
                    for (thread& thread_ : rank_.threads) {
                        size_t request_to_del = 0;
                        bool   continue_count = true;
                        for (bmpi::request& request :
                             thread_.pending_send_request) {
                            if (request_finished(request) && continue_count) {
                                request_to_del++;
                            } else {
                                continue_count = false;
                                continue;
                            }
                        }
                        thread_.pending_send_request.erase_begin(
                            request_to_del);
                        // while there are task requests from the thread on
                        // other ranks
                        while (
                            thread_.communicator.iprobe(1, client_request_task)
                                .has_value()) {

                            // receive the task request
                            thread_.communicator.recv(1, client_request_task);
                            if (current_task == sentinel_task) {
                                goto task_gen_finished_label;
                            }
                            // send the task to the thread
                            assert(!thread_.pending_send_request.full());
                            if constexpr (task_t_trivially_copyable) {
                                thread_.pending_send_request.push_back(
                                    thread_.communicator.isend(
                                        1,
                                        server_send_task,
                                        std::bit_cast<char*>(
                                            std::addressof(*current_task)),
                                        sizeof(task_type)));
                            } else {
                                thread_.pending_send_request.push_back(
                                    thread_.communicator.isend(
                                        1, server_send_task, *current_task));
                            }
                            current_task++;
                            if (run_times == 0) {
                                break;
                            }
                        }
                    }
                }
                if (run_times == 0) {
                    run_times++;
                    continue;
                } else if (run_times == 1) {
                    run_times++;
                    one_master_thread_wait_for_server.release();
                    one_master_thread_wait_for_server_released = true;
                    continue;
                }
                co_yield false;
            }
        task_gen_finished_label:
            if (!one_master_thread_wait_for_server_released) {
                one_master_thread_wait_for_server.release();
            }


            // all tasks generated, make sure all async send request are
            // fullfilled
            for (rank& rank_ : ranks) {
                if (rank_.target_rank == 0) {
                    continue;
                }
                for (thread& thread_ : rank_.threads) {
                    while (std::ranges::any_of(thread_.pending_send_request,
                                               request_unfinished)) {
                        co_yield false;
                    }
                }
            }

            // tell all client that all tasks have beed generated
            // for rank 0, just set the finished flag to true
            finished = true;
            // for other ranks, send a message to every thread to info them
            std::vector<bmpi::request> pending_send_end_requests;
            for (rank& rank_ : ranks) {
                if (rank_.target_rank == 0) {
                    continue;
                }
                for (thread& thread_ : rank_.threads) {
                    pending_send_end_requests.push_back(
                        thread_.communicator.isend(1, all_task_generated));
                }
            }
            while (std::ranges::any_of(pending_send_end_requests,
                                       request_unfinished)) {
                co_yield false;
            }


            // make sure all client request end
            for (auto& rank_ : ranks) {
                if (rank_.target_rank == 0) {
                    continue;
                }
                for (auto& thread_ : rank_.threads) {
                    while (!thread_.communicator.iprobe(1, client_finished)
                                .has_value()) {
                        co_yield false;
                    }
                    thread_.communicator.recv(1, client_finished);
                }
            }

            // consume all additional task request so MPI can free the
            // communication buffer
            for (auto& rank_ : ranks) {
                if (rank_.target_rank == 0) {
                    continue;
                }
                for (auto& thread_ : rank_.threads) {
                    while (thread_.communicator.iprobe(1, client_request_task)
                               .has_value()) {
                        thread_.communicator.recv(1, client_request_task);
                    }
                }
            }

            co_yield true;
        }

        enum get_task_result {
            ok,
            temporarily_no_task,
            task_gen_finished,
            failed_to_lock
        };

      public:
        template <typename U>
            requires std::is_same_v<T, std::remove_reference_t<U>>
        dynamic_schedule(const bmpi::communicator& communicator_,
                         U&&                       task_generator_,
                         int                       num_threads_)
            : communicator(communicator_, bmpi::comm_duplicate),
              task_generator(std::forward<U>(task_generator_)),
              comm_size(communicator.size()),
              comm_rank(communicator.rank()),
              rank_task_buffer(thread_buffer_size * num_threads_),
              ptok(std::make_unique<moodycamel::ProducerToken>(
                  rank_task_buffer)),
              gen_comm_mutex(std::make_unique<std::mutex>()),
              finish_latch_1(num_threads_),
              finish_latch_2(num_threads_),
              num_threads(num_threads_) {


            bmpi::all_gather(
                communicator, num_threads_, num_threads_on_each_rank);

            for (int rank_index = 0; rank_index < comm_size; rank_index++) {
                if (rank_index == 0) {
                    ranks.emplace_back(
                        0,
                        bmpi::communicator{MPI_COMM_SELF,
                                           boost::mpi::comm_duplicate},
                        std::vector<thread>{});
                    continue;
                }
                bool is_master_current =
                    (comm_rank == 0 or comm_rank == rank_index);
                ranks.emplace_back(
                    rank_index,
                    communicator.split(is_master_current ? 0 : 1),
                    std::vector<thread>{});
            }

            if (comm_rank == 0) {
                server_thread = std::thread([this] {
                    s_p_this_thread_should_be_proxied = false;

                    // setup communicators with other ranks
                    for (int i = 1; i < comm_size; i++) {
                        for (int thread_num = 0;
                             thread_num < num_threads_on_each_rank.at(i);
                             thread_num++) {
                            ranks.at(i).threads.emplace_back(
                                bmpi::communicator{ranks.at(i).comm_to_rank0,
                                                   boost::mpi::comm_duplicate});
                        }
                    }

                    for (const auto& _ : server()) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(1));
                    }
                });
            } else {
                // setup the communicators with rank 0
                for (int thread_num = 0; thread_num < num_threads;
                     thread_num++) {
                    comm_to_master.emplace_back(
                        ranks.at(comm_rank).comm_to_rank0,
                        boost::mpi::comm_duplicate);
                }
            }
        }

        /**
         * @brief Construct a null dynamic schedule object
         *
         */
        dynamic_schedule() = default;

        dynamic_schedule(const dynamic_schedule&)            = delete;
        dynamic_schedule& operator=(const dynamic_schedule&) = delete;

        dynamic_schedule(dynamic_schedule&&)            = default;
        dynamic_schedule& operator=(dynamic_schedule&&) = default;

        ~dynamic_schedule() {
            if (server_thread.joinable()) {
                server_thread.join();
            }
        }

      private:
        std::unique_ptr<std::mutex> gen_comm_mutex;
        std::once_flag              flag1;

      public:
        auto gen() -> cppcoro::generator<task_type> {
            if (comm_rank == 0) {
                std::call_once(flag1, [this]() {
                    one_master_thread_wait_for_server.acquire();
                });

                while (true) {
                    task_type task;
                    if (rank_task_buffer.try_dequeue_from_producer(*ptok,
                                                                   task)) {
                        co_yield std::move(task);
                    } else {
                        if (finished) {
                            break;
                        }
                    };
                }
                while (rank_task_buffer.size_approx() != 0) {
                    task_type task;
                    if (rank_task_buffer.try_dequeue(task)) {
                        co_yield std::move(task);
                    }
                }
            } else {
                boost::circular_buffer<bmpi::request> pending_send_request{
                    thread_buffer_size};

                moodycamel::ProducerToken t_l_ptok(rank_task_buffer);

                size_t my_thread_index = current_thread_index.fetch_add(1);

                if (my_thread_index >= num_threads) {
                    std::cerr
                        << "The number of threads on this rank has exceeded "
                           "the number of threads specified in the constructor "
                           "of dynamic_schedule. If you use omp and you are "
                           "sure you have passed correct parameter, then this "
                           "problem is usually caused by mixing different omp "
                           "implementations. e.g. use clang omp in the main "
                           "program and link to MKL bundled with gcc omp.";
                    MPI_Abort(MPI_COMM_WORLD, 11);
                }

                bmpi::communicator& thread_comm =
                    comm_to_master.at(my_thread_index);


                assert(thread_comm.rank() == 1);

                for (size_t _ = 0; _ < thread_buffer_size; _++) {
                    pending_send_request.push_back(
                        thread_comm.isend(0, client_request_task));
                }

                // send the task request to the master, and yield the task
                while (!thread_comm.iprobe(0, all_task_generated)) {

                    if (!thread_comm.iprobe(0, server_send_task)) {
                        continue;
                    }

                    task_type task;
                    if constexpr (task_t_trivially_copyable) {
                        thread_comm.recv(
                            0,
                            server_send_task,
                            std::bit_cast<char*>(std::addressof(task)),
                            sizeof(task));
                    } else {
                        thread_comm.recv(0, server_send_task, task);
                    }
                    co_yield std::move(task);

                    size_t request_to_del = 0;
                    bool   continue_count = true;
                    for (bmpi::request& request : pending_send_request) {
                        if (request_finished(request) && continue_count) {
                            request_to_del++;
                        } else {
                            continue_count = false;
                            continue;
                        }
                    }
                    pending_send_request.erase_begin(request_to_del);
                    assert(!pending_send_request.full());
                    thread_comm.isend(0, client_request_task);
                }

                // receive all tasks in the mpi's buffer
                while (thread_comm.iprobe(0, server_send_task).has_value()) {
                    task_type task;
                    if constexpr (task_t_trivially_copyable) {
                        thread_comm.recv(
                            0,
                            server_send_task,
                            std::bit_cast<char*>(std::addressof(task)),
                            sizeof(task));
                    } else {
                        thread_comm.recv(0, server_send_task, task);
                    }
                    rank_task_buffer.enqueue(std::move(task));
                }

                finish_latch_1.count_down();

                // consume all tasks in the rank_task_buffer
                while (rank_task_buffer.size_approx() != 0
                       or !finish_latch_1.try_wait()) {
                    task_type task;
                    if (rank_task_buffer.try_dequeue(task)) {
                        co_yield std::move(task);
                    }
                }
                finish_latch_2.arrive_and_wait();

                // cancel all the pending send request
                for (bmpi::request& request : pending_send_request) {
                    if (request.active()) {
                        request.cancel();
                    }
                }

                // tell the master that this thread has finished
                thread_comm.send(0, client_finished);
                assert(!thread_comm.iprobe(0, server_send_task).has_value());
            }
            assert(rank_task_buffer.size_approx() == 0);
        };
    };


} // namespace simple_parallel::detail
