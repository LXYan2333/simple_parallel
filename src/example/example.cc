#include "simple_parallel/simple_parallel.h"
#include <cstddef>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <mpi.h>
#include <simple_parallel/main.h>
#include <vector>

auto main() -> int {
    fmt::print("Hello from virtual main\n");
    size_t i = 4;
    std::vector<size_t> a = {1, 2, 3, 4};
    simple_parallel::broadcast_stack_and_heap();

    fmt::print("Hello{} from {}\n", i, MPI::COMM_WORLD.Get_rank());
    fmt::print("a is {}\n", a);

    if (MPI::COMM_WORLD.Get_rank() != 0) {
        return 0;
    }

    fmt::print("=================\nstart reduce\n=================\n");
    int reduce_result{};
    simple_parallel::dynamic_schedule_reduce(
        0,
        110,
        20,
        [] { return 0; },
        [](const int& start, const int& end, const int& identity) -> int {
            fmt::println("rank{} start {} end {} identity {}",
                         MPI::COMM_WORLD.Get_rank(),
                         start,
                         end,
                         identity);
            return identity + (start + end - 1) * (end - start) / 2;
        },
        [&](const int& identity) {
            fmt::println("sum is {}", identity);
            MPI::COMM_WORLD.Reduce(
                &identity, &reduce_result, 1, MPI::INT, MPI::SUM, 0);
            return identity;
        });

    fmt::println("reduce result is {}", reduce_result);
    return 0;
}
