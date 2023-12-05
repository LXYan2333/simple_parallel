#pragma once

namespace simple_parallel {

    auto
    init(int (*virtual_main)(int, char**), int argc, char** argv, bool init_mpi)
        -> void;

    auto broadcast_stack_and_heap() -> bool;

} // namespace simple_parallel
