#pragma once

#include <cstddef>
#include <simple_parallel/detail.h>
#include <simple_parallel/mpi_util.h>

namespace simple_parallel::master {

    auto broadcast_tag(mpi_util::rpc_code code) -> void;

    // don't forget to set mem_end after call me!
    auto find_avail_virtual_space(void*          begin_try_ptr,
                                  size_t         length,
                                  std::ptrdiff_t increase_len,
                                  int            prot,
                                  int            flags,
                                  int            fd,
                                  off_t          offset) -> mem_area;

    __attribute__((noinline)) auto
    run_std_function_on_all_nodes(std::move_only_function<void()> f) -> void;

    auto run_function_with_context(void (*f)(void*),
                                   void*  context,
                                   size_t context_size) -> void;

    auto cross_node_heap_mmap(void*  addr,
                              size_t len,
                              int    prot,
                              int    flags,
                              int    fd,
                              off_t  offset) -> void*;

    auto register_heap(mi_heap_t* heap) -> void;

    auto register_munmaped_areas(void* ptr, size_t size) -> void;
} // namespace simple_parallel::master
