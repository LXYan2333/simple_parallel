#include <simple_parallel/worker.h>

#include <atomic>
#include <boost/mpi.hpp>
#include <cstddef>
#include <simple_parallel/detail.h>
#include <simple_parallel/mpi_util.h>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>

namespace bmpi = boost::mpi;
using namespace simple_parallel::detail;
using namespace std::literals;

namespace simple_parallel::worker {
    auto worker() -> void {

        assert(comm.rank() != 0);

        // since clang doesn't support jthread yet, we use atomic to signal
        // the thread to exit
        std::atomic<bool> should_exit{false};

        // since master might try to mmap at any time, we need to create a new
        // thread to handle it
        std::thread mmap_thread{[&] {
            // while (!should_exit.load(std::memory_order_relaxed)) {
            while (!should_exit.load(std::memory_order_relaxed)) {

                while (auto message = mmap_comm.iprobe(0, 0)) {
                    mmap_comm.recv(0, 0);

                    void*          begin_try_ptr{};
                    size_t         length{};
                    std::ptrdiff_t increase_len{};
                    int            prot{};
                    int            flags{};
                    int            fd{};
                    off_t          offset{};

                    static_assert(bmpi::is_mpi_datatype<size_t>());

                    MPI_Bcast(
                        &begin_try_ptr, sizeof(void*), MPI_BYTE, 0, mmap_comm);
                    bmpi::broadcast(mmap_comm, length, 0);
                    bmpi::broadcast(mmap_comm, increase_len, 0);
                    bmpi::broadcast(mmap_comm, prot, 0);
                    bmpi::broadcast(mmap_comm, flags, 0);
                    bmpi::broadcast(mmap_comm, fd, 0);
                    bmpi::broadcast(mmap_comm, offset, 0);

                    detail::find_avail_virtual_space_impl(begin_try_ptr,
                                                          length,
                                                          increase_len,
                                                          prot,
                                                          flags,
                                                          fd,
                                                          offset);
                };

                std::this_thread::sleep_for(1ms);
            }
        }};


        while (true) {

            while (comm.iprobe(0, 0).has_value()) {

                mpi_util::rpc_code code{};
                comm.recv(0, code, code);
                switch (code) {
                    case mpi_util::rpc_code::init: {
                        break;
                    }
                    case mpi_util::rpc_code::finalize: {
                        should_exit.store(true, std::memory_order_relaxed);
                        goto exit_loop;
                    }
                    case mpi_util::rpc_code::run_std_function: {

                        std::vector<mem_area> mem_areas;
                        detail::sync_mem_areas(mem_areas);

                        std::move_only_function<void()>* f_ptr{};

                        MPI_Bcast(&f_ptr, sizeof(void*), MPI_BYTE, 0, comm);


                        (*f_ptr)();

                        // free all mmaped area
                        std::scoped_lock lock{mmaped_areas_lock};
                        for (const auto i : mmaped_areas) {
                            madvise(i.data(), i.size_bytes(), MADV_DONTNEED);
                        }

                        break;
                    }
                    case mpi_util::rpc_code::run_function_with_context: {
                        void (*f)(void*) = nullptr;
                        void*  context{};
                        size_t context_size{};
                        MPI_Bcast(static_cast<void*>(&f),
                                  sizeof(void*),
                                  MPI_BYTE,
                                  0,
                                  comm);
                        MPI_Bcast(
                            &context_size, sizeof(size_t), MPI_BYTE, 0, comm);

                        std::vector<std::byte> buffer;
                        buffer.reserve(context_size);
                        context = buffer.data();

                        MPI_Bcast(context,
                                  static_cast<int>(context_size),
                                  MPI_BYTE,
                                  0,
                                  comm);

                        f(context);

                        break;
                    }
                    default: {
                        std::cerr << "Invalid tag received in worker!\n";
                        std::terminate();
                    }
                }
            }

            std::this_thread::sleep_for(1ms);
        }

        // exit switch in a while loop. break is not enough
    exit_loop:

        mmap_thread.join();
    }
} // namespace simple_parallel::worker
