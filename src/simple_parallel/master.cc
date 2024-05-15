#include <simple_parallel/master.h>

#include <bit>
#include <boost/mpi.hpp>
#include <cassert>
#include <cstddef>
#include <functional>
#include <gsl/util>
#include <iostream>
#include <mimalloc.h>
#include <mimalloc/types.h>
#include <mpi.h>
#include <mutex>
#include <simple_parallel/detail.h>
#include <simple_parallel/mpi_util.h>
#include <stack>
#include <sys/mman.h>
#include <vector>

namespace bmpi = boost::mpi;

using namespace simple_parallel::detail;

namespace simple_parallel::master {

    auto broadcast_tag(mpi_util::rpc_code code) -> void {
        assert(detail::comm.rank() == 0);
        for (int i = 1; i < detail::comm.size(); i++) {
            detail::comm.send(i, 0, static_cast<int>(code));
        }
    }

    std::mutex rpc;

    // all the mem_areas that is freed by mi_malloc.
    // this variable is not actually used yet, since sync with all MPI rank
    // without issue is kind of difficult, and if any race condition happens, it
    // is extremely hard to replicate and debug. considering the virtual memory
    // address is usually more than enough, this might be ok for most cases.
    std::stack<mem_area> munmaped_areas{};

    auto find_avail_virtual_space(void*  begin_try_ptr,
                                  size_t length,
                                  size_t increase_len,
                                  int    prot,
                                  int    flags,
                                  int    fd,
                                  off_t  offset) -> mem_area {
        assert(comm.rank() == 0);

        static_assert(bmpi::is_mpi_datatype<size_t>());

        for (int i = 1; i < mmap_comm.size(); i++) {
            mmap_comm.send(i, 0);
        }

        MPI_Bcast(&begin_try_ptr, sizeof(void*), MPI_BYTE, 0, mmap_comm);
        bmpi::broadcast(mmap_comm, length, 0);
        bmpi::broadcast(mmap_comm, increase_len, 0);
        bmpi::broadcast(mmap_comm, prot, 0);
        bmpi::broadcast(mmap_comm, flags, 0);
        bmpi::broadcast(mmap_comm, fd, 0);
        bmpi::broadcast(mmap_comm, offset, 0);

        return detail::find_avail_virtual_space_impl(
            begin_try_ptr, length, increase_len, prot, flags, fd, offset);
    };


    namespace {

        auto visit_block(const mi_heap_t* /*unused*/,
                         const mi_heap_area_t* /*unused*/,
                         void*  block,
                         size_t block_size,
                         void*  mem_areas_to_send_ptr) -> bool {
            if (block == nullptr) {
                return true;
            }

            auto& mem_areas_to_send =
                *static_cast<std::vector<mem_area>*>(mem_areas_to_send_ptr);


            mem_areas_to_send.emplace_back(static_cast<std::byte*>(block),
                                           block_size);
            return true;
        };

        __attribute__((noinline)) auto get_frame_address() -> void* {

            assert(comm.rank() == 0);


            return __builtin_frame_address(0);
        }

        __thread mi_heap_t* mpi_heap = nullptr;
    } // namespace

    __attribute__((noinline)) auto
    run_std_function_on_all_nodes(std::move_only_function<void()> f) -> void {

        assert(comm.size() > 1);

        std::scoped_lock lock{rpc};


        assert(comm.rank() == 0);

        broadcast_tag(mpi_util::rpc_code::run_std_function);

        void* f_ptr = &f;

        // broadcast the heap and stack on master to all wokers
        {
            if (mpi_heap == nullptr) {
                mpi_heap = mi_heap_new();
            }
            mi_heap_t* backing_heap = mi_heap_get_backing();
            mi_heap_set_default(mpi_heap);
            gsl::final_action restore_heap{
                [&backing_heap] { mi_heap_set_default(backing_heap); }};

            std::vector<mem_area> mem_areas_to_send{};
            // reservation size is just a guess. hope it's enough
            mem_areas_to_send.reserve(heaps.size() * 100);

            // iterate over all thread-private heaps
            for (auto* heap : heaps) {
                // save all malloced blocks' pointer and size to
                // `mem_areas_to_send`
                mi_heap_visit_blocks(
                    heap, true, &visit_block, &mem_areas_to_send);
            }
            // save the stack to `mem_areas_to_send`
            mem_areas_to_send.emplace_back(
                static_cast<std::byte*>(get_frame_address()), &*stack.end());

            detail::sync_mem_areas(mem_areas_to_send);
        }

        MPI_Bcast(&f_ptr, sizeof(void*), MPI_BYTE, 0, comm);


        f();
    }

    auto cross_node_heap_mmap(void*  addr,
                              size_t len,
                              int    prot,
                              int    flags,
                              int    fd,
                              off_t  offset) -> void* {
        std::lock_guard lock{cross_node_mmap_lock};

        if (cross_mmap_params.should_exit) {
            if (get_env_var("SIMPLE_PARALLEL_DEBUG").has_value()) {
                // clang-format off
                std::cerr << "Warning: the cross node mmap thread has exited, a local mmap will performed.\n";
                std::cerr << "rank:" << comm.rank() << ", address:" << addr << ", len:" << len << "\n";
                std::cerr << "if this message occurs at the end of the program, usually this is harmless.\n";
                // clang-format on
            }
            return mmap(addr, len, prot, flags, fd, offset);
        }

        cross_mmap_result.returned = false;
        {
            std::lock_guard lock2{cross_node_mmap_send_param_lock};
            cross_mmap_params.addr   = addr;
            cross_mmap_params.len    = len;
            cross_mmap_params.prot   = prot;
            cross_mmap_params.flags  = flags;
            cross_mmap_params.fd     = fd;
            cross_mmap_params.offset = offset;

            cross_mmap_params.has_request = true;
        }
        cross_node_mmap_send_param_cv.notify_one();

        std::unique_lock lock3{cross_node_mmap_result_lock};
        cross_node_mmap_return_result_cv.wait(
            lock3, [] { return cross_mmap_result.returned; });


        return cross_mmap_result.addr;
    };

    std::mutex register_heap_lock;

    // call to this function is thread safe
    auto register_heap(mi_heap_t* heap) -> void {
        assert(_mi_heap_default == _mi_heap_default->tld->heap_backing);
        std::lock_guard lock{register_heap_lock};
        heaps.push_back(heap);
    }

    std::mutex register_munmaped_area_lock;

    auto register_munmaped_areas(void* ptr, size_t size) -> void {
        std::lock_guard lock{register_munmaped_area_lock};
        munmaped_areas.emplace(static_cast<std::byte*>(ptr), size);
    }

} // namespace simple_parallel::master
