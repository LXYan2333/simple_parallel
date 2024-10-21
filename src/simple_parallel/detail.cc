#include <cstdint>
#include <simple_parallel/detail.h>

#include <bit>
#include <boost/mpi.hpp>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <dlfcn.h>
#include <gsl/util>
#include <internal_use_only/simple_parallel_config.h>
#include <limits>
#include <mimalloc.h>
#include <mpi.h>
#include <mutex>
#include <optional>
#include <simple_parallel/mpi_util.h>
#include <string_view>
#include <sys/mman.h>
#include <sys/types.h>
#include <unordered_set>
#include <vector>

namespace bmpi = boost::mpi;

namespace simple_parallel::detail {


    mem_area stack{};

    std::vector<mi_heap_t*> heaps{};

    std::mutex mmaped_areas_lock;

    std::unordered_set<mem_area> mmaped_areas{};

    void* mem_end;

    bmpi::communicator comm;

    bmpi::communicator mmap_comm;

    std::mutex cross_node_mmap_lock;

    std::mutex cross_node_mmap_send_param_lock;

    std::mutex cross_node_mmap_result_lock;

    std::condition_variable cross_node_mmap_send_param_cv;

    std::condition_variable cross_node_mmap_return_result_cv;

    cross_mmap_params_t cross_mmap_params{false, false, nullptr, 0, 0, 0, 0, 0};

    cross_mmap_result_t cross_mmap_result{nullptr, false};

    auto get_env_var(const char* key) -> std::optional<const std::string_view> {
        assert(key != nullptr);
        assert(key[0] != '\0');
        const char* val = std::getenv(key);
        if (val == nullptr) {
            return std::nullopt;
        } else {
            return {val};
        }
    }

    auto find_avail_virtual_space_impl(void*          try_ptr,
                                       size_t         length,
                                       std::ptrdiff_t increase_len,
                                       int            prot,
                                       int            flags,
                                       int            fd,
                                       off_t          offset) -> mem_area {
        assert(try_ptr != nullptr);
        assert(length > 0);
        assert(increase_len > 0);


        while (true) {

            // if the memory address is higher than 0xFFFF'FFFF'FFFFuz
            // (which is the highest memory address in x86_64), abort
            size_t end = std::bit_cast<size_t>(try_ptr) + length;
            if (end > 0x7FFF'FFFF'FFFFuz) {
                comm.abort(111);
            }


            void* mmap_result = mmap(try_ptr, length, prot, flags, fd, offset);

            // whether mmap successfully on this process
            bool my_result = mmap_result == try_ptr;

            if (!my_result) {
                munmap(mmap_result, length);
            }

            // whether all process mmap successfully
            bool reduced_result = false;

            MPI_Allreduce(
                &my_result, &reduced_result, 1, MPI_C_BOOL, MPI_LAND, comm);

            if (reduced_result) {
                // all MPI processes successfully found a free virtual memory,
                // exit loop
                {
                    std::scoped_lock lock{mmaped_areas_lock};
                    mmaped_areas.insert({try_ptr, length});
                }
                return {try_ptr, length};
            } else {
                // some MPI processes found a free virtual memory while some
                // are failed, we should unmap it
                if (my_result) {
                    munmap(try_ptr, length);
                }
            }

            try_ptr = reinterpret_cast<void*>(
                reinterpret_cast<uintptr_t>(try_ptr) + increase_len);
        }
    };

    namespace {
        auto sync_mem_area_impl(mem_area area) -> void {
#ifdef simple_parallel_MPI_BIG_COUNT
            MPI_Bcast_c(mem_area.data(),
                        static_cast<u_int64_t>(mem_area.size()),
                        MPI_BYTE,
                        0,
                        comm);
#else
            size_t remaining = area.size_bytes();
            auto   data_ptr  = reinterpret_cast<uintptr_t>(area.data());

            while (remaining > std::numeric_limits<int>::max()) [[unlikely]] {
                MPI_Bcast(reinterpret_cast<void*>(data_ptr),
                          std::numeric_limits<int>::max(),
                          MPI_BYTE,
                          0,
                          comm);

                // 0x7fffffff = std::numeric_limits<int>::max()
                // use literal to suppress gcc's false positive warning
                remaining -= 0x7fff'ffff;
                data_ptr  += 0x7fff'ffff;
            }

            assert(remaining <= std::numeric_limits<int>::max());

            MPI_Bcast(reinterpret_cast<void*>(data_ptr),
                      static_cast<int>(remaining),
                      MPI_BYTE,
                      0,
                      comm);
#endif
        }
    } // namespace

    auto sync_mem_areas(std::vector<mem_area>& mem_areas) -> void {

        if (comm.rank() == 0) {
            assert(mem_areas.size() >= 1);
        } else {
            assert(mem_areas.size() == 0);
        }

        size_t mem_areas_size = mem_areas.size();

        bmpi::broadcast(comm, mem_areas_size, 0);

        if (comm.rank() != 0) {
            mem_areas.resize(mem_areas_size);
        }
        sync_mem_area_impl(
            {mem_areas.data(), mem_areas_size * sizeof(mem_area)});

        for (const auto& area : mem_areas) {
            sync_mem_area_impl(area);
        }
    }


} // namespace simple_parallel::detail
