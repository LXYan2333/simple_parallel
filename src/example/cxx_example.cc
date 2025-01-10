#include <simple_parallel/cxx/simple_parallel.h>
#include <vector>

auto main() -> int {

  {
    std::vector<int> test2{1, 2, 3, 4};
    simple_parallel::par_ctx ctx{};

    std::cout << ctx.get_comm().rank() << '\n';

    for (auto i : test2) {
      std::cout << i << ' ';
    }

    std::cout << '\n';
  }

  {
    std::vector<int> test3{1, 2, 3, 4};
    simple_parallel::par_ctx ctx{};

    std::cout << ctx.get_comm().rank() << '\n';

    for (auto i : test3) {
      std::cout << i << ' ';
    }

    std::cout << '\n';
  }

  {
    std::vector<int> test3{1, 2, 3, 4};
    test3.reserve(1024 * 1024 * 1024);
    simple_parallel::par_ctx ctx{};

    std::cout << ctx.get_comm().rank() << '\n';

    for (auto i : test3) {
      std::cout << i << ' ';
    }

    std::cout << '\n';
  }

  return 0;
}