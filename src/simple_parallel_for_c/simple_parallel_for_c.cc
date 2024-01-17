#include <cppcoro/generator.hpp>
#include <cstddef>
#include <internal_use_only/simple_parallel_config.h>
#include <simple_parallel/omp_dynamic_schedule.h>
#include <simple_parallel/simple_parallel.h>
#include <utility>

#include <simple_parallel_for_c/simple_parallel_for_c.h>

extern "C" {
    auto simple_parallel_init(int (*virtual_main)(int, char**),
                              int    argc,
                              char** argv) -> void {
        simple_parallel::init(
            virtual_main, argc, argv, boost::mpi::threading::level::serialized);
    }

    auto simple_parallel_run_lambda(void* lambda, bool parallel_run) -> void {
        simple_parallel::run_lambda(
            [&] {
#ifdef SIMPLE_PARALLEL_COMPILER_GNU
                // use gcc's nested function extension
                // see https://gcc.gnu.org/onlinedocs/gcc/Nested-Functions.html
                (*reinterpret_cast<void (*)()>(lambda))();
#else
                // use clang's block extension
                // see https://clang.llvm.org/docs/BlockLanguageSpec.html
                //     https://thephd.dev/lambdas-nested-functions-block-expressions-oh-my
                (*reinterpret_cast<void (^(*))(void)>(lambda))();
#endif
            },
            parallel_run);
    }

    auto simple_parallel_omp_dynamic_schedule_simple(int    begin,
                                                     int    end,
                                                     int    grain_size,
                                                     size_t prefetch_count,
                                                     void*  lambda) -> void {

        auto task_generator = [](int _begin, int _end, int _grain_size)
            -> cppcoro::generator<std::pair<int, int>> {
            for (int i = _begin; i < _end; i += _grain_size) {

                const int yield_end = std::min(_end, i + _grain_size);

                co_yield {i, yield_end};
            }
        };

        auto payload = [&](std::pair<int, int> task) {
            for (int i = task.first; i < task.second; i++) {
#ifdef SIMPLE_PARALLEL_COMPILER_GNU
                // use gcc's nested function extension
                // see https://gcc.gnu.org/onlinedocs/gcc/Nested-Functions.html
                (*reinterpret_cast<void (*)(int)>(lambda))(i);
#else
                // use clang's block extension
                // see https://clang.llvm.org/docs/BlockLanguageSpec.html
                //     https://thephd.dev/lambdas-nested-functions-block-expressions-oh-my
                (*reinterpret_cast<void (^(*))(int)>(lambda))(i);
#endif
            }
        };

        simple_parallel::omp_dynamic_schedule(
            prefetch_count,
            task_generator(begin, end, grain_size),
            std::move(payload));
    }
}
