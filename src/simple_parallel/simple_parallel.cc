#include <boost/mpi.hpp>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mimalloc.h>
#include <mpi.h>
#include <simple_parallel/advance.h>
#include <simple_parallel/mpi_util.h>
#include <sys/personality.h>
#include <ucontext.h>

#include <simple_parallel/simple_parallel.h>

namespace bmpi = boost::mpi;

namespace simple_parallel {

    // this heap is used by my_rank = 0
    mi_heap_t* heap;

    stack_and_heap_info stack_and_heap_info;

    auto init(int (*virtual_main)(int, char**),
              int                    argc,
              char**                 argv,
              bmpi::threading::level mpi_threading_level) -> void {

        bmpi::environment  env{argc, argv, mpi_threading_level, true};
        bmpi::communicator world{};

        int my_rank   = world.rank();
        int num_procs = world.size();

        if (num_procs == 1) {
            // only one process, no need to do anything
            virtual_main(argc, argv);
            return;
        } else {
            // check whether the current process is running without ASLR
            std::ifstream filestat{"/proc/self/personality"};
            std::string   line;
            std::getline(filestat, line);
            std::stringstream ss{line};
            uint32_t          personality;
            ss >> std::hex >> personality;

            // exit if ASLR is not disabled
            bool should_exit = (personality & ADDR_NO_RANDOMIZE) == 0u;
            MPI_Allreduce(MPI_IN_PLACE,
                          &should_exit,
                          1,
                          MPI_C_BOOL,
                          MPI_LOR,
                          MPI_COMM_WORLD);
            if (should_exit) {
                if (my_rank == 0) {
                    // clang-format off
                    std::cerr << "ASLR is not disabled. Please run the program with ASLR disabled.\n";
                    std::cerr << "\n";
                    std::cerr << "Execute this program with following command:\n";
                    std::cerr << "\n";
                    std::cerr << "    mpirun <mpi argument> setarch `uname -m` -R ";
                    for (int i = 0; i < argc; i++) {
                        std::cerr << argv[i] << " ";
                    }
                    std::cerr << "\n";
                    std::cerr << "\n";
                    std::cerr << "If you do not use bash shell, please run the following command instead:\n";
                    std::cerr << "\n"; 
                    std::cerr << "   bash -c \"mpirun <mpi argument> setarch `uname -m` -R";
                    for (int i = 0; i < argc; i++) {
                        std::cerr << " " << argv[i];
                    }
                    std::cerr << "\"\n";
                    std::cerr << "\n";
                    // clang-format on
                    world.abort(1);
                }
            }
        }

        // find a virtual memory space that is free on all MPI processes
        stack_and_heap_info = advance::find_free_virtual_space(
            1024uz * 1024 * 1024 * 8, // 8GB
            1024uz * 1024 * 1024 * 1024 * 20 /* 20TB */);

        auto [stack_len, stack_bottom_ptr, heap_len, heap_ptr] =
            stack_and_heap_info;

        // print PID and wait key input
        // // this is for debug purpose. you can launch this program and attach
        // // debugger to specified PID, then press any key to continue
        // std::cout << "rank: " << my_rank << ", PID: " << getpid() << "\n";
        // if (my_rank == 0) {
        //     std::cout << "Press any key to continue.\n";
        //     std::ignore = std::getchar();
        // }

        // set my_rank = 0's stack and heap to the new location
        if (my_rank == 0) {
            mi_arena_id_t mi_id{};
            mi_manage_os_memory_ex(
                heap_ptr,
                heap_len,
                false,
                false,
                true,
                -1, // mimalloc haven't implemented this yet. see
                    // https://github.com/microsoft/mimalloc/blob/4e50d6714d471b72b2285e25a3df6c92db944593/src/arena.c#L776
                // may need to use HWLOC to find the NUMA node of the new heap
                true,
                &mi_id);
            heap = mi_heap_new_in_arena(mi_id);
            mi_heap_set_default(heap);

            ucontext_t target_context;
            ucontext_t context_current;
            getcontext(&target_context);
            target_context.uc_stack.ss_sp = reinterpret_cast<void*>(
                reinterpret_cast<size_t>(stack_bottom_ptr) - stack_len);
            target_context.uc_stack.ss_size = stack_len;
            target_context.uc_link          = &context_current;
            makecontext(&target_context,
                        reinterpret_cast<void (*)()>(virtual_main),
                        2,
                        argc,
                        argv);
            mpi_util::broadcast_tag(mpi_util::tag_enum::init);
            swapcontext(&context_current, &target_context);

            mpi_util::broadcast_tag(mpi_util::tag_enum::finalize);

        } else {
            advance::worker();
        }
    }

} // namespace simple_parallel
