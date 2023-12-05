#include <fmt/core.h>
#include <mpi.h>
#include <simple_parallel/advance.h>
#include <simple_parallel/simple_parallel.h>
#include <tbb/tbb.h>

auto virtual_main(int argc, char** argv) -> int {
    fmt::print("Hello from virtual main\n");
    return 0;
}

auto main(int argc, char** argv) -> int {
    // std::cin.get();
    // initialize your global variable here

    // after this line, your modification on global variables won't be synced to
    // other processes, and you can only access global variables directly (i.e.
    // you should't access global variable through pointers)
    simple_parallel::init(virtual_main, argc, argv, true);

    return 0;
}
