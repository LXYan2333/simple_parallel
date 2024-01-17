#include <boost/mpi.hpp>
#include <simple_parallel/mpi_util.h>
#include <simple_parallel/worker.h>

namespace simple_parallel::advance {
    auto worker() -> void {

        assert(boost::mpi::communicator{}.rank() != 0);

        while (true) {
            mpi_util::tag_enum tag{};
            MPI_Bcast(&tag, 1, MPI_INT, 0, MPI_COMM_WORLD);
            switch (tag) {
                case mpi_util::tag_enum::init: {
                    continue;
                }
                case mpi_util::tag_enum::finalize: {
                    return;
                }
                case mpi_util::tag_enum::send_stack: {
                    void* stack_frame_ptr{};

                    MPI_Bcast(&stack_frame_ptr,
                              sizeof(void*),
                              MPI_BYTE,
                              0,
                              MPI_COMM_WORLD);

                    size_t stack_len{};
                    MPI_Bcast(
                        &stack_len, sizeof(void*), MPI_BYTE, 0, MPI_COMM_WORLD);

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
                    break;
                }
                case mpi_util::tag_enum::send_heap: {
                    while (true) {
                        void* block_ptr{};
                        MPI_Bcast(&block_ptr,
                                  sizeof(void*),
                                  MPI_BYTE,
                                  0,
                                  MPI_COMM_WORLD);

                        if (block_ptr == nullptr) {
                            break;
                        }

                        size_t block_size{};
                        MPI_Bcast(&block_size,
                                  sizeof(size_t),
                                  MPI_BYTE,
                                  0,
                                  MPI_COMM_WORLD);
#ifdef simple_parallel_MPI_BIG_COUNT
                        MPI_Bcast_c(block_ptr,
                                    static_cast<MPI_Count>(block_size),
                                    MPI_BYTE,
                                    0,
                                    MPI_COMM_WORLD);
#else
                        MPI_Bcast(block_ptr,
                                  static_cast<int>(block_size),
                                  MPI_BYTE,
                                  0,
                                  MPI_COMM_WORLD);
#endif
                    }
                    break;
                }
                case mpi_util::tag_enum::run_lambda: {
                    using function = std::function<void()>;
                    function* pointer_to_std_function{};
                    MPI_Bcast(&pointer_to_std_function,
                              sizeof(function*),
                              MPI_BYTE,
                              0,
                              MPI_COMM_WORLD);
                    (*pointer_to_std_function)();

                    break;
                }
                case mpi_util::tag_enum::common: {
                    throw std::runtime_error(
                        "common tag should not be received here!");
                    break;
                }
                case mpi_util::tag_enum::print_memory: {
                    void*  ptr{};
                    size_t len_in_byte{};
                    MPI_Bcast(&ptr, sizeof(void*), MPI_BYTE, 0, MPI_COMM_WORLD);
                    MPI_Bcast(&len_in_byte,
                              sizeof(size_t),
                              MPI_BYTE,
                              0,
                              MPI_COMM_WORLD);
                    for (size_t i = 0; i < len_in_byte; i++) {
                        std::cout << std::hex
                                  << reinterpret_cast<u_int8_t*>(ptr)[i];
                    }
                    std::cout << "\n";
                    break;
                }
                default: {
                    throw std::runtime_error("Invalid tag received in worker!");
                }
            }
        }
    }
} // namespace simple_parallel::advance
