#include <boost/mpi.hpp>
#include <boost/mpi/environment.hpp>
#include <cstddef>
#include <cstdio>
#include <mimalloc.h>
#include <mpi.h>
#include <simple_parallel/advance.h>
#include <simple_parallel/mpi_util.h>
#include <ucontext.h>

#include <simple_parallel/simple_parallel.h>

namespace bmpi = boost::mpi;

namespace simple_parallel {

    // this heap is used by my_rank = 0
    mi_heap_t* heap;

    stack_and_heap_info stack_and_heap_info;

    auto init(int (*virtual_main)(int, char**), int argc, char** argv) -> void {

        bmpi::environment env{
            argc, argv, bmpi::threading::level::multiple, true};

        int my_rank   = bmpi::communicator{}.rank();
        int num_procs = bmpi::communicator{}.size();

        if (num_procs == 1) {
            // only one process, no need to do anything
            virtual_main(argc, argv);
            return;
        }

        // find a virtual memory space that is free on all MPI processes
        stack_and_heap_info = advance::find_free_virtual_space(
            1024uz * 1024 * 1024 * 8, // 8GB
            1024uz * 1024 * 1024 * 1024 * 20 /* 20TB */);

        auto [stack_len, stack_bottom_ptr, heap_len, heap_ptr] = stack_and_heap_info;

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
            target_context.uc_link = &context_current;
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
