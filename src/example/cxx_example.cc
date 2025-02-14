#include <cstddef>
#include <mpi.h>
#include <numeric>
#include <omp.h>
#include <ranges>
#include <simple_parallel/cxx/dynamic_schedule.h>
#include <simple_parallel/cxx/simple_parallel.h>
#include <span>
#include <vector>

using reduce = simple_parallel::reduce_area;
using namespace std::literals;

template <typename T> void vector_sum(std::vector<T> &in, std::vector<T> &out) {
  BOOST_ASSERT(in.size() == out.size());
  for (int i = 0; i < in.size(); i++) {
    out.at(i) += in.at(i);
  }
}

#pragma omp declare reduction(                                                 \
        + : std::vector<int> : vector_sum<int>(omp_in, omp_out))               \
    initializer(omp_priv = std::vector<int>(omp_orig.size(), 0))

auto main() -> int {

  {
    std::cout << "test iota dynamic schedule reduce\n";
    size_t count = 0;
    {
      simple_parallel::par_ctx ctx{true, reduce{count, MPI_SUM}};
      for (const auto &i : simple_parallel::dynamic_schedule(
               std::ranges::views::iota(0, 100), ctx.get_comm(), 2)) {
        count += i;
      }
      std::cout << "rank: " << ctx.get_comm().rank() << ", count: " << count
                << '\n';
    }
    std::cout << "count: " << count << '\n';
  }

  {
    std::cout << "\ntest vector dynamic schedule reduce\n";
    size_t count = 0;
    std::vector<int> vec(100);
    for (int i = 0; i < 100; i++) {
      vec.at(i) = i;
    }

    {
      simple_parallel::par_ctx ctx{true, reduce{count, MPI_SUM}};
      for (const auto &i :
           simple_parallel::dynamic_schedule(vec, ctx.get_comm())) {
        count += i;
      }
      std::cout << "rank: " << ctx.get_comm().rank() << ", count: " << count
                << '\n';
    }
    std::cout << "count: " << count << '\n';
  }

  {
    std::cout << "\ntest c_array dynamic schedule reduce\n";
    size_t count = 0;
    int c_arr[100];
    for (int i = 0; i < 100; i++) {
      c_arr[i] = i;
    }

    {
      simple_parallel::par_ctx ctx{true, reduce{count, MPI_SUM}};
      for (const auto &i :
           simple_parallel::dynamic_schedule(c_arr, ctx.get_comm())) {
        count += i;
      }
      std::cout << "rank: " << ctx.get_comm().rank() << ", count: " << count
                << '\n';
    }
    std::cout << "count: " << count << '\n';
  }

  {
    std::cout << "\ntest array dynamic schedule reduce\n";
    size_t count = 0;
    std::array<int, 100> array;
    for (int i = 0; i < 100; i++) {
      array.at(i) = i;
    }

    {
      simple_parallel::par_ctx ctx{true, reduce{count, MPI_SUM}};
      for (const auto &i :
           simple_parallel::dynamic_schedule(array, ctx.get_comm())) {
        count += i;
      }
      std::cout << "rank: " << ctx.get_comm().rank() << ", count: " << count
                << '\n';
    }
    std::cout << "count: " << count << '\n';
  }

  {
    std::cout << "\ntest span dynamic schedule reduce\n";
    size_t count = 0;
    std::array<int, 100> array{};
    for (int i = 0; i < 100; i++) {
      array.at(i) = i;
    }

    {
      std::span<int> span{array};
      simple_parallel::par_ctx ctx{true, reduce{count, MPI_SUM}};
      for (const auto &i :
           simple_parallel::dynamic_schedule(span, ctx.get_comm())) {
        count += i;
      }
      std::cout << "rank: " << ctx.get_comm().rank() << ", count: " << count
                << '\n';
    }
    std::cout << "count: " << count << '\n';
  }

  {
    std::cout << "\ntest span reduce\n";
    std::array<int, 100> array;
    for (int i = 0; i < 100; i++) {
      array.at(i) = i;
    }
    {
      std::span<int> span{array};
      simple_parallel::par_ctx ctx{false, reduce{span, MPI_SUM}};
      BOOST_ASSERT(ctx.get_comm().size() == 1);
      for (int i = 0; i < 100; i++) {
        array.at(i)++;
      }
    }
    size_t count = std::accumulate(array.begin(), array.end(), 0);
    std::cout << "count: " << count << '\n';
  }

  {
    std::cout << "\ntest OpenMP iota dynamic schedule task reduce\n";
#ifndef _OPENMP
#error "OpenMP is not enabled"
#endif
    size_t count = 0;
    {
      simple_parallel::par_ctx ctx{true, reduce{count, MPI_SUM}};

      // https: // www.openmp.org/wp-content/uploads/openmp-examples-5-0-1.pdf
      // page 325 USE CASE 1 explicit-task reduction + parallel reduction clause
#pragma omp parallel reduction(task, + : count) default(shared)
#pragma omp single
      for (int i : simple_parallel::dynamic_schedule(
               std::ranges::views::iota(0, 1000), ctx.get_comm(), 2)) {
#pragma omp task in_reduction(+ : count) untied
        {
          std::cerr << "rank: " << ctx.get_comm().rank()
                    << ", thread_num: " << omp_get_thread_num() << ", i: " << i
                    << '\n';
          std::this_thread::sleep_for(10ms);
          count += i;
        }
      }

      std::cout << "rank: " << ctx.get_comm().rank() << ", count: " << count
                << '\n';
    }
    std::cout << "count: " << count << '\n';
  }

  {
    std::cout << "\ntest span task reduce\n";
    std::vector<int> vec(100, 0);
    {
      simple_parallel::par_ctx ctx{true, reduce{std::span<int>{vec}, MPI_SUM}};
#pragma omp parallel reduction(task, + : vec) default(shared)
#pragma omp single
      for (int i : simple_parallel::dynamic_schedule(
               std::ranges::views::iota(0, 100), ctx.get_comm(), 2)) {
#pragma omp task in_reduction(+ : vec) untied
        {
          std::cerr << "rank: " << ctx.get_comm().rank()
                    << ", thread_num: " << omp_get_thread_num() << ", i: " << i
                    << '\n';
          vec.at(i)++;
        }
      }
    }
    size_t count = std::accumulate(vec.begin(), vec.end(), 0);
    std::cout << "count: " << count << '\n';
  }

  return 0;
}