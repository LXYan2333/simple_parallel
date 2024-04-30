#pragma once

#include <simple_parallel/detail.h>
#include <simple_parallel/mpi_util.h>
#include <stdio.h>

extern "C" void (*simple_parallel_register_heap)(mi_heap_t* heap);

extern "C" void (*simple_parallel_register_munmaped_areas)(void*  ptr,
                                                           size_t size);

namespace simple_parallel::master {

    auto broadcast_tag(mpi_util::rpc_code code) -> void;

    // don't forget to set mem_end after call me!
    auto find_avail_virtual_space(void*  begin_try_ptr,
                                  size_t length,
                                  size_t increase_len,
                                  int    prot,
                                  int    flags,
                                  int    fd,
                                  off_t  offset) -> mem_area;

    __attribute__((noinline)) auto
    run_std_function_on_all_nodes(std::move_only_function<void()> f) -> void;

    auto cross_node_heap_mmap(void*  addr,
                              size_t len,
                              int    prot,
                              int    flags,
                              int    fd,
                              off_t  offset) -> void*;

    auto register_heap(mi_heap_t* heap) -> void;

    auto register_munmaped_areas(void* ptr, size_t size) -> void;
} // namespace simple_parallel::master