#include <simple_parallel/simple_parallel.h>

#include <bit>
#include <boost/mpi.hpp>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <internal_use_only/simple_parallel_config.h>
#include <iostream>
#include <mimalloc.h>
#include <mpi.h>
#include <mutex>
#include <simple_parallel/detail.h>
#include <simple_parallel/master.h>
#include <simple_parallel/mpi_util.h>
#include <simple_parallel/worker.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <thread>
#include <threads.h>
#include <ucontext.h>


namespace bmpi = boost::mpi;
using namespace simple_parallel::detail;

#ifndef NDEBUG
static bool initialized = false;
#endif

namespace simple_parallel {

    namespace {
        // since passing 64bit parameter to `makecontext` is not part of the
        // standard, we use global variable to pass this function.
        // see
        // https://www.man7.org/linux/man-pages/man3/swapcontext.3.html#NOTES
        std::move_only_function<void()> func_to_call_after_init;
    } // namespace

    auto init(std::move_only_function<void()> call_after_init) -> void {

#ifndef NDEBUG
        if (initialized) {
            std::cerr << "simple_parallel::init() is called more than once.\n";
            std::terminate();
        }
        initialized = true;
#endif

        bmpi::environment env{bmpi::threading::level::multiple, true};
        if (env.thread_level() != bmpi::threading::level::multiple) {
            if (comm.rank() == 0) {
                // clang-format off
                std::cerr << "Error: the MPI implementation doesn't support MPI_THREAD_MULTIPLE!\n";
                std::cerr << "Please use a MPI implementation that supports it. e.g. OpenMPI\n";
                std::cerr << "https://docs.open-mpi.org/en/main/tuning-apps/multithreaded.html\n";
                // clang-format on
            }
            env.abort(1);
        }
        comm      = {MPI_COMM_WORLD, bmpi::comm_duplicate};
        mmap_comm = {MPI_COMM_WORLD, bmpi::comm_duplicate};

        if (comm.size() == 1) {
            if (!get_env_var("SIMPLE_PARALLEL_DEBUG").has_value()) {
                // only one process, no need to do anything
                call_after_init();
                return;
            }
        }

        // check whether the current process is running with ASLR disabled
        {
            std::ifstream filestat{"/proc/self/personality"};
            std::string   line;
            std::getline(filestat, line);
            std::stringstream ss{line};
            uint32_t          personality;
            ss >> std::hex >> personality;

            // exit if ASLR is not disabled
            bool should_exit = (personality & ADDR_NO_RANDOMIZE) == 0u;
            MPI_Allreduce(
                MPI_IN_PLACE, &should_exit, 1, MPI_C_BOOL, MPI_LOR, comm);
            if (should_exit) {
                if (comm.rank() == 0) {
                    // clang-format off
                    std::cerr << "ASLR is not disabled. Please run the program with ASLR disabled.\n";
                    std::cerr << "\n";
                    std::cerr << "Execute this program with following command:\n";
                    std::cerr << "\n";
                    std::cerr << "    mpirun <mpi argument> setarch `uname -m` -R <program> <args>\n";
                    std::cerr << "\n";
                    std::cerr << "If you do not use bash shell, please run the following command instead:\n";
                    std::cerr << "\n"; 
                    std::cerr << "   bash -c \"mpirun <mpi argument> setarch `uname -m` -R <program> <args>\"\n";
                    std::cerr << "\n";
                    // clang-format on
                    comm.abort(1);
                }
            }
        }

        // if environment variable SIMPLE_PARALLEL_DEBUG is set to 1, print PID
        // and wait Enter.
        // this is for debug purpose. you can launch this program and attach
        // debugger to specified PID, then press Enter key to continue.
        if (get_env_var("SIMPLE_PARALLEL_DEBUG").has_value()) {
            std::cout << "rank: " << comm.rank() << ", PID: " << getpid()
                      << "\n";
            comm.barrier();
            if (comm.rank() == 0) {
                std::cout << "Press Enter to continue.\n";
                std::ignore = std::getchar();
            }
        }


        // find a virtual memory space that is free on all MPI processes, which
        // will be used as stack later.
        {
            mem_end       = std::bit_cast<void*>(0x1000'0000'0000uz);
            int map_flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE
                            | MAP_GROWSDOWN | MAP_STACK;

            int prot_flags = PROT_WRITE | PROT_READ;
            // gcc's nested function extension requires executable stack
#ifdef COMPILER_SUPPORTS_NESTED_FUNCTIONS
            prot_flags |= PROT_EXEC;
#endif

            stack = find_avail_virtual_space_impl(
                mem_end,
                1024uz * 1024 * 1024 * 2, // 1024uz * 1024 * 1024 * 2 = 2GB
                1024uz * 1024 * 1024,
                prot_flags,
                map_flags,
                -1,
                0);
            mem_end = &*stack.end();
        }

        if (comm.rank() == 0) {
            func_to_call_after_init = std::move(call_after_init);
            auto* run_std_function  = +[] { func_to_call_after_init(); };

            // now we set the rank 0's stack to the specified address
            ucontext_t target_context;
            ucontext_t current_context;
            getcontext(&target_context);
            target_context.uc_stack.ss_sp   = stack.data();
            target_context.uc_stack.ss_size = stack.size_bytes();
            target_context.uc_link          = &current_context;
            makecontext(&target_context, run_std_function, 0);
            master::broadcast_tag(mpi_util::rpc_code::init);

            // a new thread is created to handle cross mmap calls.
            // we can't let each thread to handle cross mmap calls by
            // themselves since we need to communicate with other MPI rank,
            // but most MPI implementations will malloc in the thread local
            // heaps, which will try to mmap new memory area and cause
            // infinitly recursive calls.
            std::thread cross_node_mmap_thread{[] {
                s_p_this_thread_should_be_proxied = false;

                while (true) {
                    std::unique_lock lock{cross_node_mmap_send_param_lock};
                    cross_node_mmap_send_param_cv.wait(
                        lock, [] { return cross_mmap_params.has_request; });
                    if (cross_mmap_params.should_exit) {
                        return;
                    }
                    auto mem =
                        simple_parallel::master::find_avail_virtual_space(
                            mem_end,
                            cross_mmap_params.len,
                            0x4'000'000'000uz, // 4GiB
                            cross_mmap_params.prot,
                            cross_mmap_params.flags,
                            cross_mmap_params.fd,
                            cross_mmap_params.offset);

                    mem_end = &*mem.end();

                    cross_mmap_params.has_request = false;

                    {
                        std::lock_guard lock2{cross_node_mmap_result_lock};
                        cross_mmap_result.addr     = mem.data();
                        cross_mmap_result.returned = true;
                    }
                    cross_node_mmap_return_result_cv.notify_one();
                }
            }};


            s_p_comm_rank = comm.rank();
            // from now on, all threads which is spawned on rank0 will be
            // proxyed by simple_parallel
            //
            // check
            // src/mimalloc_for_simple_parallel/src/init.c:_mi_heap_init
            // line 334

            // a new thread is created as the "main" thread.
            // for most openmp implementations, this makes sure the
            // (potential) old (thread private) openmp thread pool is not
            // used. this also makes sure all malloc operation of this
            // thread is proxied. (some segments and pages is mmaped earlier
            // (for initialize purpose) and is stored in mimalloc's tld. they
            // will be reused and cause issue)
            std::thread t{
                [&] { swapcontext(&current_context, &target_context); }};

            t.join();

            {
                std::lock_guard lock{cross_node_mmap_lock};
                {
                    std::lock_guard lock2{cross_node_mmap_send_param_lock};
                    cross_mmap_params.should_exit = true;
                    cross_mmap_params.has_request = true;
                }
                cross_node_mmap_send_param_cv.notify_one();
            }
            cross_node_mmap_thread.join();

            master::broadcast_tag(mpi_util::rpc_code::finalize);

        } else {
            worker::worker();
        }
    }

} // namespace simple_parallel
