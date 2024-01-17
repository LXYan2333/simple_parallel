#include <simple_parallel/advance.h>

#include <cassert>
#include <cstddef>
#include <dlfcn.h>
#include <gsl/util>
#include <internal_use_only/simple_parallel_config.h>
#include <mimalloc.h>
#include <mpi.h>
#include <simple_parallel/mpi_util.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <type_traits>

namespace simple_parallel {

    extern mi_heap_t* heap;

    extern struct stack_and_heap_info stack_and_heap_info;

    namespace advance {

        auto find_free_virtual_space(size_t stack_len, size_t heap_len)
            -> struct stack_and_heap_info {

            // MPI::Add_error_string(111,
            //                       "Failed to find a free virtual memory
            //                       space");

            // find a free virtual memory space that is free on all MPI
            // processes for my_rank=0's stack
            size_t new_stack_ptr = 0x4000'0000'0000uz;
            while (true) {
                void* mmap_result =
                    mmap(reinterpret_cast<void*>(new_stack_ptr),
                         stack_len,
                // gcc's nested function extension requires executable stack
#ifdef SIMPLE_PARALLEL_COMPILER_GNU
                         PROT_WRITE | PROT_READ | PROT_EXEC,
#else
                         PROT_WRITE | PROT_READ,
#endif
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_STACK
                             | MAP_FIXED_NOREPLACE | MAP_NORESERVE,
                         -1,
                         0);

                bool my_result      = mmap_result != MAP_FAILED;
                bool reduced_result = false;

                static_assert(
                    std::is_convertible_v<boost::mpi::is_mpi_datatype<bool>,
                                          boost::mpl::true_>);
                // static_assert(
                //     std::is_convertible_v<
                //         boost::mpi::is_mpi_op<std::logical_and<bool>, bool>,
                //         boost::mpl::true_>);
                MPI_Allreduce(&my_result,
                              &reduced_result,
                              1,
                              MPI_C_BOOL,
                              MPI_LAND,
                              MPI_COMM_WORLD);

                if (reduced_result) {
                    // all MPI processes successfully find a free virtual
                    // memory, exit loop
                    break;
                } else {
                    // some MPI processes failed to find a free virtual memory,
                    // unmap
                    // skip the process that failed to mmap a free virtual
                    // memory
                    if (my_result) {
                        munmap(reinterpret_cast<void*>(new_stack_ptr),
                               stack_len);
                    }
                }

                new_stack_ptr += 1024uz * 1024 * 1024 * 4; // forward 4GB
                // if the pointer is too large, abort
                if (new_stack_ptr + stack_len > 0xFFFF'FFFF'FFFFuz) {
                    boost::mpi::communicator{}.abort(111);
                }
            }

            // find a free virtual memory space that is free on all MPI
            // processes for my_rank=0's heap
            size_t new_heap_ptr = (new_stack_ptr + stack_len + 0xFFFF'FFFFuz)
                                  & 0xFFFF'0000'0000uz;
            while (true) {
                void* mmap_result =
                    mmap(reinterpret_cast<void*>(new_heap_ptr),
                         heap_len,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE
                             | MAP_NORESERVE,
                         -1,
                         0);

                bool my_result      = mmap_result != MAP_FAILED;
                bool reduced_result = false;

                MPI_Allreduce(&my_result,
                              &reduced_result,
                              1,
                              MPI_C_BOOL,
                              MPI_LAND,
                              MPI_COMM_WORLD);

                if (reduced_result) {
                    // all MPI processes successfully find a free virtual
                    // memory, exit loop
                    break;
                } else {
                    // some MPI processes failed to find a free virtual memory,
                    // unmap
                    // skip the process that failed to mmap a free virtual
                    // memory
                    if (my_result) {
                        munmap(reinterpret_cast<void*>(new_heap_ptr), heap_len);
                    }
                }

                // forward 256GB
                new_heap_ptr += 1024uz * 1024 * 1024 * 256;

                // if the pointer is too large, abort
                if (new_heap_ptr + heap_len > 0xFFFF'FFFF'FFFFuz) {
                    boost::mpi::communicator{}.abort(111);
                }
            }

            struct stack_and_heap_info r {
                stack_len,
                    reinterpret_cast<void*>(new_stack_ptr + stack_len), heap_len
                    ,
                    reinterpret_cast<void*>(new_heap_ptr)
            };

            return r;

        }


        auto
        send_stack(void* stack_frame_ptr, void* stack_bottom_ptr) -> void {

            // tell all workers to receive stack
            mpi_util::broadcast_tag(mpi_util::tag_enum::send_stack);

            // send stack pointer to all workers
            MPI_Bcast(
                &stack_frame_ptr, sizeof(void*), MPI_BYTE, 0, MPI_COMM_WORLD);

            // send stack length to all workers
            size_t stack_len = reinterpret_cast<size_t>(stack_bottom_ptr)
                               - reinterpret_cast<size_t>(stack_frame_ptr);

            MPI_Bcast(&stack_len, sizeof(size_t), MPI_BYTE, 0, MPI_COMM_WORLD);
#ifdef simple_parallel_MPI_BIG_COUNT
            MPI_Bcast_c(stack_frame_ptr,
                        static_cast<MPI_Count>(stack_len),
                        MPI_BYTE,
                        0,
                        MPI_COMM_WORLD);
#else
            MPI_Bcast(stack_frame_ptr,
                      static_cast<int>(stack_len),
                      MPI_BYTE,
                      0,
                      MPI_COMM_WORLD);
#endif
        }

        namespace {
            auto simple_parallel_send_block(const mi_heap_t* /*unused*/,
                                            const mi_heap_area_t* /*unused*/,
                                            void*  block,
                                            size_t block_size,
                                            void* /*unused*/) -> bool {
                if (block == nullptr) {
                    return true;
                }
                MPI_Bcast(&block, sizeof(void*), MPI_BYTE, 0, MPI_COMM_WORLD);
                MPI_Bcast(
                    &block_size, sizeof(size_t), MPI_BYTE, 0, MPI_COMM_WORLD);
#ifdef simple_parallel_MPI_BIG_COUNT
                MPI_Bcast_c(block,
                            static_cast<MPI_Count>(block_size),
                            MPI_BYTE,
                            0,
                            MPI_COMM_WORLD);
#else
                MPI_Bcast(block,
                          static_cast<int>(block_size),
                          MPI_BYTE,
                          0,
                          MPI_COMM_WORLD);
#endif
                return true;
            }
        } // namespace

        auto send_heap(mi_heap_t* target_heap) -> void {
            // tell all workers to receive heap
            mpi_util::broadcast_tag(mpi_util::tag_enum::send_heap);
            mi_heap_t* backing_heap = mi_heap_get_backing();
            // probably MPI will malloc some memory, so temporarily switch to
            // default heap
            {
                mi_heap_set_default(backing_heap);
                gsl::final_action restore_heap{
                    [&target_heap] { mi_heap_set_default(target_heap); }};
                mi_heap_visit_blocks(
                    target_heap, true, &simple_parallel_send_block, nullptr);
            }
            // tell all workers to stop receiving heap
            void* null_ptr = nullptr;
            MPI_Bcast(&null_ptr, sizeof(void*), MPI_BYTE, 0, MPI_COMM_WORLD);
        }

        /**
         * @brief Broadcast the virtual stack and heap of the `rank = 0` process
         * to all other process.
         *
         * Warning to code maintaners:
         * This function can **NOT** be inlined, as we need to get the correct
         * frame pointer address to send the entire heap.
         * If this function is inlined, we will get an incorrect frame pointer
         * of the caller function, and the sent stack will be incomplete.
         * In my test, the `__attribute__((noinline))` works for clang++ 17.0.6
         * and g++ 12.2.0
         *
         */
        __attribute__((noinline)) auto broadcast_stack_and_heap() -> void {
            assert(boost::mpi::communicator{}.rank() == 0);

            void* frame_address = __builtin_frame_address(0);

            // send my_rank = 0's stack
            send_stack(&frame_address, stack_and_heap_info.stack_bottom_ptr);
            send_heap(heap);
        }

        auto print_memory_on_worker(void* ptr, size_t len_in_byte) -> void {
            assert(boost::mpi::communicator{}.rank() == 0);
            mpi_util::broadcast_tag(mpi_util::tag_enum::print_memory);
            MPI_Bcast(&ptr, sizeof(void*), MPI_BYTE, 0, MPI_COMM_WORLD);
            MPI_Bcast(
                &len_in_byte, sizeof(size_t), MPI_BYTE, 0, MPI_COMM_WORLD);
        }
    } // namespace advance
} // namespace simple_parallel
