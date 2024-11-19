#pragma once

#include <atomic>
#include <boost/mpi/communicator.hpp>
#include <cassert>
#include <condition_variable>
#include <cppcoro/generator.hpp>
#include <cstddef>
#include <cstdint>
#include <mimalloc.h>
#include <mutex>
#include <optional>
#include <ranges>
#include <simple_parallel/worker.h>
#include <unordered_set>


extern __thread mi_heap_t* _mi_heap_default;
extern __thread bool       s_p_should_proxy_mmap;
extern std::atomic<int>    s_p_comm_rank;

namespace simple_parallel {
    class mem_area {

        char*  m_ptr;
        size_t m_size;

      public:
        mem_area() noexcept
            : m_ptr{nullptr},
              m_size{0} {}

        mem_area(void* ptr, size_t size) noexcept
            : m_ptr{static_cast<char*>(ptr)},
              m_size{size} {}

        mem_area(void* begin, void* end) noexcept
            : m_ptr{static_cast<char*>(begin)},
              m_size{reinterpret_cast<uintptr_t>(end)
                     - reinterpret_cast<uintptr_t>(begin)} {}

        auto data() const noexcept -> char* { return m_ptr; }

        auto size_bytes() const noexcept -> size_t { return m_size; }

        auto size() const noexcept -> size_t { return m_size; }

        auto begin() const noexcept -> char* { return m_ptr; }

        auto end() const noexcept -> char* { return m_ptr + m_size; }
    };
} // namespace simple_parallel

namespace std {
    template <>
    struct hash<simple_parallel::mem_area> {
        auto operator()(const simple_parallel::mem_area& area) const noexcept
            -> size_t {
            return std::hash<void*>{}(area.data());
        }
    };

    template <>
    struct equal_to<simple_parallel::mem_area> {
        auto operator()(const simple_parallel::mem_area& lhs,
                        const simple_parallel::mem_area& rhs) const noexcept
            -> bool {
            if (lhs.data() == rhs.data()) {
                assert(lhs.size_bytes() == rhs.size_bytes());
                return true;
            }
            return false;
        }
    };
} // namespace std

namespace simple_parallel::detail {


    // the stack used for rank 0
    extern mem_area stack;

    // the heap used for rank 0. mimalloc requires each thread to have its
    // own heap, so a vector is used
    extern std::vector<mi_heap_t*> heaps;

    // the communicator used by most simple_parallel functions
    extern boost::mpi::communicator comm;

    // the communicator used to mmap memory across several nodes
    extern boost::mpi::communicator mmap_comm;

    // the end of the last `mmap`ed memory end address. program will try to
    // allocate next memory region start from this address.
    extern void* mem_end;

    // any thread try to cross node mmap should aquire this lock until the
    // mmap thread returned the result
    extern std::mutex cross_node_mmap_lock;

    extern std::mutex cross_node_mmap_send_param_lock;

    extern std::mutex cross_node_mmap_result_lock;

    extern std::condition_variable cross_node_mmap_send_param_cv;

    extern std::condition_variable cross_node_mmap_return_result_cv;

    extern std::mutex mmaped_areas_lock;

    // all the mmaped areas that is proxyed by simple_parallel
    extern std::unordered_set<mem_area> mmaped_areas;

    template <std::ranges::range T>
    auto loop_forever(T&& range)
        -> cppcoro::generator<std::ranges::range_value_t<T>> {
        while (true) {
            for (auto&& i : range) {
                co_yield i;
            }
        }
    }

    struct cross_mmap_params_t {
        bool   should_exit;
        bool   has_request;
        void*  addr;
        size_t len;
        int    prot;
        int    flags;
        int    fd;
        off_t  offset;
    };

    extern cross_mmap_params_t cross_mmap_params;

    struct cross_mmap_result_t {
        void* addr;
        bool  returned;
    };

    extern cross_mmap_result_t cross_mmap_result;

    auto get_env_var(const char* key) -> std::optional<const std::string_view>;

    auto find_avail_virtual_space_impl(void*          begin_try_ptr,
                                       size_t         length,
                                       std::ptrdiff_t increase_len,
                                       int            prot,
                                       int            flags,
                                       int            fd,
                                       off_t          offset) -> mem_area;

    auto sync_mem_areas(std::vector<mem_area>& mem_areas) -> void;

} // namespace simple_parallel::detail
