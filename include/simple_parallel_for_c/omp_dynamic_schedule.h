#pragma once

#include <simple_parallel_for_c/lambda.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

    void simple_parallel_omp_dynamic_schedule_simple(int    begin,
                                                     int    end,
                                                     int    grain_size,
                                                     size_t prefetch_count,
                                                     void*  lambda);

#ifdef __cplusplus
}
#endif

// clang-format off
#define SIMPLE_PARALLEL_C_OMP_DYNAMIC_SCHEDULE_BEGIN                           \
    SIMPLE_PARALLEL_LAMBDA(simple_parallel_payload_tag,void, int _s_p_task) {

#define SIMPLE_PARALLEL_C_OMP_DYNAMIC_SCHEDULE_END(                            \
    _begin, _end, _grain_size, _prefetch_count)                                \
    };                                                                         \
                                                                               \
    simple_parallel_omp_dynamic_schedule_simple(_begin,                        \
                                                _end,                          \
                                                _grain_size,                   \
                                                _prefetch_count,               \
                                                &simple_parallel_payload_tag);
// clang-format on
