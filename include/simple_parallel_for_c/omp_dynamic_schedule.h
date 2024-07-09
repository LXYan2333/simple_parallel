#pragma once

#include <mpi.h>
#include <simple_parallel_for_c/lambda.h>
#include <simple_parallel_for_c/simple_parallel_for_c.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
    typedef struct dynamic_schedule_context dynamic_schedule_context;
    typedef struct generator_context        generator_context;

    size_t dynamic_schedule_context_size();

    void construct_dynamic_schedule_context(
        dynamic_schedule_context* dynamic_schedule_context_buffer_ptr,
        bool (*scheduler_func)(void* state, void* task_buffer),
        void*    state,
        size_t   prefetch_count,
        MPI_Comm comm);


    void begin_schedule(
        dynamic_schedule_context* dynamic_schedule_context_buffer_ptr);

    void destruct_dynamic_schedule_context(
        dynamic_schedule_context* dynamic_schedule_context_buffer_ptr);
    size_t thread_generator_context_buffer_size();

    void construct_thread_task_generator(
        generator_context*        generator_context_buffer_ptr,
        dynamic_schedule_context* dynamic_schedule_context_buffer_ptr,
        void**                    buffer_ptr);

    void thread_generator_next(generator_context* generator_context_buffer_ptr,
                               void**             buffer_ptr);

    bool thread_generator_end(generator_context* generator_context_buffer_ptr);

    void destruct_thread_task_generator(
        generator_context* generator_context_buffer_ptr);

    bool guided_self_scheduler(void* state, void* task_buffer);

    struct gss_state {
        size_t begin;
        size_t end;
        size_t process_count;
        size_t grain_size;
    };

    bool liner_scheduler(void* state, void* task_buffer);

    struct liner_scheduler_state {
        size_t begin;
        size_t end;
        size_t grain_size;
    };


#ifdef __cplusplus
}
#endif


// clang-format off
#define S_P_PARALLEL_C_DYNAMIC_SCHEDULE_BEGIN(                                 \
    s_p_communicator, task_buffer, scheduler_func, scheduler_state)            \
    static dynamic_schedule_context* s_p_dynamic_schedule_context;             \
    generator_context*              s_p_gen_context;                           \
    _Pragma("omp masked")                                                      \
    {                                                                          \
        s_p_dynamic_schedule_context =                                         \
            malloc(dynamic_schedule_context_size());                           \
        construct_dynamic_schedule_context(s_p_dynamic_schedule_context,       \
                                           scheduler_func,                     \
                                           scheduler_state,                    \
                                           80,                                 \
                                           s_p_communicator);                  \
        begin_schedule(s_p_dynamic_schedule_context);                          \
    }                                                                          \
    _Pragma("omp barrier")                                                     \
    s_p_gen_context = malloc(thread_generator_context_buffer_size());          \
    for (construct_thread_task_generator(                                      \
             s_p_gen_context, s_p_dynamic_schedule_context, task_buffer);      \
         !thread_generator_end(s_p_gen_context);                               \
         thread_generator_next(s_p_gen_context, task_buffer)) {

#define S_P_PARALLEL_C_DYNAMIC_SCHEDULE_END                                    \
    }                                                                          \
    destruct_thread_task_generator(s_p_gen_context);                           \
    free(s_p_gen_context);                                                     \
    _Pragma("omp barrier")                                                     \
    _Pragma("omp masked") {                                                    \
        destruct_dynamic_schedule_context(s_p_dynamic_schedule_context);       \
        free(s_p_dynamic_schedule_context);                                    \
    }
// clang-format on
