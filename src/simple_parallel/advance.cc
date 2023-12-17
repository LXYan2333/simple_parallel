#include <simple_parallel/advance.h>

#include <cassert>
#include <cstddef>
#include <dlfcn.h>
#include <fmt/core.h>
#include <functional>
#include <gsl/util>
#include <internal_use_only/simple_parallel_config.h>
#include <mimalloc.h>
#include <mpi.h>
#include <simple_parallel/mpi_util.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <sys/personality.h>

#ifndef HAVE_PERSONALITY
    #include <syscall.h>
    #define personality(pers) ((long)syscall(SYS_personality, pers))
#endif

namespace simple_parallel {

    extern mi_heap_t* heap;

    extern struct stack_and_heap_info stack_and_heap_info;

    namespace advance {

        // static

        //     auto
        //     disable_aslr() {
        //     // disable ASLR
        // }

        auto run_main(char* executable) {
            dlopen(executable, RTLD_LAZY);
            auto* err = dlerror();
            if (err != nullptr) {
                fmt::print(stderr, "Failed to load executable: {}\n", err);
            }
        }

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

                bool my_result = mmap_result != MAP_FAILED;
                bool reduced_result = false;

                MPI::COMM_WORLD.Allreduce(
                    &my_result, &reduced_result, 1, MPI::BOOL, MPI::LAND);
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
                    fmt::println(
                        stderr,
                        "Failed to find a free virtual memory space for stack");
                    MPI::COMM_WORLD.Abort(111);
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

                bool my_result = mmap_result != MAP_FAILED;
                bool reduced_result = false;

                MPI::COMM_WORLD.Allreduce(
                    &my_result, &reduced_result, 1, MPI::BOOL, MPI::LAND);

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
                    fmt::println(
                        stderr,
                        "Failed to find a free virtual memory space for heap");
                    MPI::COMM_WORLD.Abort(111);
                }
            }

            struct stack_and_heap_info r {
                stack_len,
                reinterpret_cast<void*>(new_stack_ptr + stack_len),
                heap_len,
                reinterpret_cast<void*>(new_heap_ptr)
            };

            return r;

        }

        auto
        worker() -> void {

            assert(MPI::COMM_WORLD.Get_rank() != 0);

            void* myself = dlopen(nullptr, RTLD_LAZY);
            gsl::final_action dlclose_guard{[&myself]() { dlclose(myself); }};

            while (true) {
                mpi_util::tag_enum tag{};
                MPI::COMM_WORLD.Bcast(&tag, 1, MPI::INT, 0);
                switch (tag) {
                    case mpi_util::tag_enum::init: {
                        continue;
                    }
                    case mpi_util::tag_enum::finalize: {
                        return;
                    }
                    case mpi_util::tag_enum::send_stack: {
                        void* stack_frame_ptr{};
                        MPI::COMM_WORLD.Bcast(
                            &stack_frame_ptr, sizeof(size_t), MPI::BYTE, 0);

                        size_t stack_len{};
                        MPI::COMM_WORLD.Bcast(
                            &stack_len, sizeof(size_t), MPI::BYTE, 0);

                        MPI_Bcast_c(stack_frame_ptr,
                                    static_cast<MPI_Count>(stack_len),
                                    MPI::BYTE,
                                    0,
                                    MPI_COMM_WORLD);
                        break;
                    }
                    case mpi_util::tag_enum::send_heap: {
                        while (true) {
                            void* block_ptr{};
                            MPI::COMM_WORLD.Bcast(
                                &block_ptr, sizeof(void*), MPI::BYTE, 0);

                            if (block_ptr == nullptr) {
                                break;
                            }

                            size_t block_size{};
                            MPI::COMM_WORLD.Bcast(
                                &block_size, sizeof(size_t), MPI::BYTE, 0);
                            MPI_Bcast_c(block_ptr,
                                        static_cast<MPI_Count>(block_size),
                                        MPI::BYTE,
                                        0,
                                        MPI_COMM_WORLD);
                            fmt::println(stderr,
                                         "worker {} received block {:X}",
                                         MPI::COMM_WORLD.Get_rank(),
                                         *reinterpret_cast<size_t*>(block_ptr));
                        }
                        break;
                    }
                    case mpi_util::tag_enum::run_lambda: {
                        using function = std::function<void()>;
                        function* pointer_to_std_function{};
                        MPI::COMM_WORLD.Bcast(&pointer_to_std_function,
                                              sizeof(function*),
                                              MPI::BYTE,
                                              0);
                        fmt::println(
                            stderr,
                            "worker {} received lambda {:X}",
                            MPI::COMM_WORLD.Get_rank(),
                            reinterpret_cast<size_t>(pointer_to_std_function));
                        (*pointer_to_std_function)();

                        break;
                    }
                    case mpi_util::tag_enum::common: {
                        throw std::runtime_error(
                            "common tag should not be received here!");
                        break;
                    }
                    case mpi_util::tag_enum::print_memory: {
                        void* ptr{};
                        size_t len_in_byte{};
                        MPI::COMM_WORLD.Bcast(
                            &ptr, sizeof(void*), MPI::BYTE, 0);
                        MPI::COMM_WORLD.Bcast(
                            &len_in_byte, sizeof(size_t), MPI::BYTE, 0);
                        for (size_t i = 0; i < len_in_byte; i++) {
                            fmt::print(stderr,
                                       "{:X}",
                                       reinterpret_cast<u_int8_t*>(ptr)[i]);
                        }
                        fmt::print(stderr, "\n");
                        break;
                    }
                    default: {
                        throw std::runtime_error(
                            "Invalic tag received in worker!");
                    }
                }
            }
        }

        auto send_stack(void* stack_frame_ptr, void* stack_bottom_ptr) -> void {
            // tell all workers to receive stack
            mpi_util::tag_enum tag = mpi_util::tag_enum::send_stack;
            MPI::COMM_WORLD.Bcast(&tag, 1, MPI::INT, 0);

            // send stack pointer to all workers
            MPI::COMM_WORLD.Bcast(
                &stack_frame_ptr, sizeof(void*), MPI::BYTE, 0);

            // send stack length to all workers
            size_t stack_len = reinterpret_cast<size_t>(stack_bottom_ptr)
                               - reinterpret_cast<size_t>(stack_frame_ptr);

            MPI::COMM_WORLD.Bcast(&stack_len, sizeof(size_t), MPI::BYTE, 0);

            MPI_Bcast_c(stack_frame_ptr,
                        static_cast<MPI_Count>(stack_len),
                        MPI_BYTE,
                        0,
                        MPI_COMM_WORLD);
        }

        namespace {
            auto simple_parallel_send_block(const mi_heap_t* /*unused*/,
                                            const mi_heap_area_t* /*unused*/,
                                            void* block,
                                            size_t block_size,
                                            void* /*unused*/) -> bool {
                if (block == nullptr) {
                    return true;
                }
                MPI::COMM_WORLD.Bcast(&block, sizeof(void*), MPI::BYTE, 0);
                MPI::COMM_WORLD.Bcast(
                    &block_size, sizeof(size_t), MPI::BYTE, 0);
                MPI_Bcast_c(block,
                            static_cast<MPI_Count>(block_size),
                            MPI::BYTE,
                            0,
                            MPI_COMM_WORLD);

                return true;
            }
        } // namespace

        auto send_heap(mi_heap_t* target_heap) -> void {
            // tell all workers to receive heap
            mpi_util::tag_enum tag = mpi_util::tag_enum::send_heap;
            MPI::COMM_WORLD.Bcast(&tag, 1, MPI::INT, 0);
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
            MPI::COMM_WORLD.Bcast(&null_ptr, sizeof(void*), MPI::BYTE, 0);
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
            assert(MPI::COMM_WORLD.Get_rank() == 0);

            void* frame_address = __builtin_frame_address(0);

            // send my_rank = 0's stack
            send_stack(&frame_address, stack_and_heap_info.stack_bottom_ptr);
            send_heap(heap);
        }

        auto print_memory_on_worker(void* ptr, size_t len_in_byte) -> void {
            assert(MPI::COMM_WORLD.Get_rank() == 0);
            mpi_util::broadcast_tag(mpi_util::tag_enum::print_memory);
            MPI::COMM_WORLD.Bcast(&ptr, sizeof(void*), MPI::BYTE, 0);
            MPI::COMM_WORLD.Bcast(&len_in_byte, sizeof(size_t), MPI::BYTE, 0);
        }
    } // namespace advance
} // namespace simple_parallel
