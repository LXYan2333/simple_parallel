#pragma once

#include <cstddef>
#include <mimalloc.h>
#include <simple_parallel/worker.h>

namespace simple_parallel {

    struct stack_and_heap_info {
        size_t stack_len;
        void*  stack_bottom_ptr;
        size_t heap_len;
        void* heap_ptr;
    };

    namespace advance {

        auto broadcast_stack_and_heap() -> void;

        auto print_memory_on_worker(void* ptr, size_t len_in_byte) -> void;

        auto send_stack(void* stack_frame_ptr, void* stack_ptr) -> void;

        auto send_heap(mi_heap_t* heap) -> void;

        auto find_free_virtual_space(size_t stack_len, size_t heap_len)
            -> stack_and_heap_info;

    } // namespace advance

} // namespace simple_parallel
