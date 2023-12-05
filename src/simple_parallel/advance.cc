#include <bigmpi.h>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <dlfcn.h>
#include <fmt/core.h>
#include <gsl/gsl_util>
#include <mimalloc.h>
#include <mpi.h>
#include <simple_parallel/mpi_util.h>
#include <sys/mman.h>

namespace simple_parallel {

    extern mi_heap_t* heap;

    extern struct stack_and_heap_info {
        size_t stack_len;
        void* stack_ptr;
        size_t heap_len;
        void* heap_ptr;
    } stack_and_heap_info;

    namespace advance {

        auto run_main(char* executable) {
            dlopen(executable, RTLD_LAZY);
            auto* err = dlerror();
            if (err != nullptr) {
                fmt::print(stderr, "Failed to load executable: {}\n", err);
            }
        }

        auto find_free_virtual_space(size_t stack_len, size_t heap_len) {

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
                         PROT_WRITE | PROT_READ,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_STACK
                             | MAP_FIXED_NOREPLACE,
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
                         PROT_NONE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
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
                if (new_stack_ptr + heap_len > 0xFFFF'FFFF'FFFFuz) {
                    fmt::println(
                        stderr,
                        "Failed to find a free virtual memory space for stack");
                    MPI::COMM_WORLD.Abort(111);
                }
            }

            struct stack_and_heap_info r {
                stack_len, reinterpret_cast<void*>(new_stack_ptr), heap_len,
                    reinterpret_cast<void*>(new_heap_ptr)
            };

            return r;
        }

        auto worker() -> void {
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

                        MPIX_Bcast_x(stack_frame_ptr,
                                     static_cast<int64_t>(stack_len),
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
                            MPIX_Bcast_x(block_ptr,
                                         static_cast<int64_t>(block_size),
                                         MPI::BYTE,
                                         0,
                                         MPI_COMM_WORLD);
                        }
                        break;
                    }
                    case mpi_util::tag_enum::run_function: {
                        throw std::runtime_error(
                            "run_function not implemented!");
                        break;
                    }
                    case mpi_util::tag_enum::common: {
                        throw std::runtime_error(
                            "common tag should not be received here!");
                        break;
                    }
                    default: {
                        throw std::runtime_error(
                            "Invalic tag received in worker!");
                    }
                }
            }
        }

        auto send_stack(void* stack_frame_ptr, void* stack_ptr) {
            // tell all workers to receive stack
            mpi_util::tag_enum tag = mpi_util::tag_enum::send_stack;
            MPI::COMM_WORLD.Bcast(&tag, 1, MPI::INT, 0);

            // send stack length to all workers
            size_t stack_len = reinterpret_cast<size_t>(stack_ptr)
                               - reinterpret_cast<size_t>(stack_frame_ptr);

            MPI::COMM_WORLD.Bcast(&stack_len, sizeof(size_t), MPI::BYTE, 0);

            // send stack using bigmpi as vanilla mpi can not send BYTE >=
            // 2GB
            MPIX_Bcast_x(stack_frame_ptr,
                         static_cast<int64_t>(stack_len),
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
                assert(block != nullptr);
                MPI::COMM_WORLD.Bcast(&block, sizeof(void*), MPI::BYTE, 0);
                MPI::COMM_WORLD.Bcast(
                    &block_size, sizeof(size_t), MPI::BYTE, 0);
                MPIX_Bcast_x(block,
                             static_cast<int64_t>(block_size),
                             MPI::BYTE,
                             0,
                             MPI_COMM_WORLD);

                return true;
            }
        } // namespace

        auto send_heap(mi_heap_t* heap) -> void {
            // tell all workers to receive heap
            mpi_util::tag_enum tag = mpi_util::tag_enum::send_heap;
            MPI::COMM_WORLD.Bcast(&tag, 1, MPI::INT, 0);
            mi_heap_visit_blocks(
                heap, true, &simple_parallel_send_block, nullptr);
            // tell all workers to stop receiving heap
            MPI::COMM_WORLD.Bcast(nullptr, sizeof(void*), MPI::BYTE, 0);
        }

        // TODO(lxyan): Maybe send global variable too?
    } // namespace advance
} // namespace simple_parallel
