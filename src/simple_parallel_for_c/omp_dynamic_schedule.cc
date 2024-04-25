#include <cassert>
#include <cppcoro/generator.hpp>
#include <cstddef>
#include <simple_parallel/dynamic_schedule.h>

#include <simple_parallel_for_c/omp_dynamic_schedule.h>

namespace {
    cppcoro::generator<std::pair<int, int>> simple_generator_for_c;

    decltype(simple_generator_for_c.begin()) iter_begin;
    decltype(simple_generator_for_c.end())   iter_end;
} // namespace

extern "C" {

    auto simple_parallel_omp_generator_set(int    begin,
                                           int    end,
                                           int    grain_size,
                                           int    processor,
                                           size_t prefetch_count,
                                           bool   parallel_run) -> void {
        assert(grain_size > 0);

        auto gen = [](int _begin, int _end, int _grain_size, int _processor)
            -> cppcoro::generator<std::pair<int, int>> {
            int unused_iter = _end - _begin;
            while (unused_iter > 0) {
                int next_schedule = (unused_iter - 1) / _processor + 1;

                // next_schedule should not be less than grain_size
                next_schedule = std::max(next_schedule, _grain_size);

                // next_schedule should not be greater than unused_iter
                next_schedule = std::min(next_schedule, unused_iter);

                co_yield std::make_pair(_begin, _begin + next_schedule);
                _begin      += next_schedule;
                unused_iter -= next_schedule;
            }
        }(begin, end, grain_size, processor);

        if (parallel_run) {
            simple_generator_for_c =
                simple_parallel::detail::dynamic_schedule<std::pair<int, int>>(
                    prefetch_count, std::move(gen));
        } else {
            simple_generator_for_c = std::move(gen);
        }

        iter_begin = simple_generator_for_c.begin();
        iter_end   = simple_generator_for_c.end();
    }

    auto simple_parallel_omp_generator_done() -> bool {
        return iter_begin == iter_end;
    }

    auto simple_parallel_omp_generator_next(int* begin, int* end) -> void {
        *begin = iter_begin->first;
        *end   = iter_begin->second;

        ++iter_begin;
    }
}
