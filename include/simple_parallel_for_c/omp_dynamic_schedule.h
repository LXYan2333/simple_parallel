#pragma once

#include <simple_parallel_for_c/lambda.h>
#include <simple_parallel_for_c/simple_parallel_for_c.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

    void simple_parallel_omp_generator_set(int    begin,
                                           int    end,
                                           int    grain_size,
                                           int    processor,
                                           size_t prefetch_count,
                                           bool   parallel_run);

    bool simple_parallel_omp_generator_done();

    void simple_parallel_omp_generator_next(int* begin, int* end);

#ifdef __cplusplus
}
#endif

// clang-format off
#define SIMPLE_PARALLEL_C_OMP_DYNAMIC_SCHEDULE_BEGIN(_begin,                   \
                                                     _end,                     \
                                                     _grain_size,              \
                                                     _processor,               \
                                                     _prefetch_count,          \
                                                     simple_parallel_run,      \
                                                     _begin_var,               \
                                                     _end_var)                 \
    _Pragma("omp masked")                                                      \
    {                                                                          \
        simple_parallel_omp_generator_set(_begin,                              \
                                          _end,                                \
                                          _grain_size,                         \
                                          _processor,                          \
                                          _prefetch_count,                     \
                                          simple_parallel_run);                \
    }                                                                          \
    _Pragma("omp barrier")                                                     \
    bool _s_p_should_break = false;                                            \
    while (true) {                                                             \
        _Pragma("omp critical")                                                \
        {                                                                      \
            _s_p_should_break = simple_parallel_omp_generator_done();          \
            if (!_s_p_should_break) {                                          \
                simple_parallel_omp_generator_next(&_begin_var, &_end_var);    \
            }                                                                  \
        }                                                                      \
        if (_s_p_should_break) {                                               \
            break;                                                             \
        }

#define SIMPLE_PARALLEL_C_OMP_DYNAMIC_SCHEDULE_END                             \
    }
