#pragma once

#include <simple_parallel/dynamic_schedule.h>

namespace simple_parallel {
    template <std::ranges::range T, typename U>
        requires requires(T t, U u) { u(std::move(*t.begin())); }
    auto omp_dynamic_schedule(size_t prefetch_count, T all_task, U payload) {

        using task_type = std::remove_reference_t<decltype(*all_task.begin())>;
        task_type task;

        static cppcoro::generator<task_type> client_generator;

        static decltype(client_generator.begin()) begin;
        static decltype(client_generator.end())   end;

#pragma omp masked
        {
            client_generator = detail::dynamic_schedule<task_type>(
                prefetch_count, std::move(all_task));
            begin = client_generator.begin();
            end   = client_generator.end();
        }
        bool should_break = false;
#pragma omp barrier
        while (true) {
#pragma omp critical
            {
                if (begin == end) {
                    should_break = true;
                } else {
                    task = *begin;
                    begin++;
                }
            }

            if (should_break) [[unlikely]] {
                break;
            }

            payload(std::move(task));
        }
    }
} // namespace simple_parallel

// clang-format off

#define SIMPLE_PARALLEL_OMP_DYNAMIC_SCHEDULE_GENERATOR                         \
    {                                                                          \
        auto _simple_parallel_all_task_generator =

#define SIMPLE_PARALLEL_OMP_DYNAMIC_SCHEDULE_PAYLOAD(_s_p_prefetch_count)      \
                                                                               \
        using task_type = std::remove_reference_t<                             \
            decltype(*_simple_parallel_all_task_generator.begin())>;           \
        task_type simple_parallel_task;                                        \
                                                                               \
        static cppcoro::generator<task_type> simple_parallel_client_generator; \
        static decltype(simple_parallel_client_generator.begin()) _s_p_begin;  \
        static decltype(simple_parallel_client_generator.end())   _s_p_end;    \
        _Pragma("omp masked")                                                  \
        {                                                                      \
            simple_parallel_client_generator =                                 \
                simple_parallel::detail::dynamic_schedule<task_type>(         \
                    _s_p_prefetch_count,                                       \
                    std::move(_simple_parallel_all_task_generator));           \
            _s_p_begin = simple_parallel_client_generator.begin();             \
            _s_p_end   = simple_parallel_client_generator.end();               \
        }                                                                      \
        _Pragma("omp barrier")                                                 \
        while (true) {                                                         \
            bool _s_p_should_break = false;                                    \
            _Pragma("omp critical")                                            \
            {                                                                  \
                if (_s_p_begin == _s_p_end) {                                  \
                    _s_p_should_break = true;                                  \
                } else {                                                       \
                    simple_parallel_task = *_s_p_begin;                        \
                    ++_s_p_begin;                                              \
                }                                                              \
            }                                                                  \
            if (_s_p_should_break) {                                           \
                break;                                                         \
            }

#define SIMPLE_PARALLEL_OMP_DYNAMIC_SCHEDULE_END                               \
        }                                                                      \
    }
// clang-format on
