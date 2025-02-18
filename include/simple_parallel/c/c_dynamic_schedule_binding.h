#pragma once

#include <mpi.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef char s_p_dyn_buffer[64];

typedef bool(s_p_task_gen_func)(void *state, s_p_dyn_buffer *buffer);

typedef struct s_p_dynamic_schedule_s s_p_dynamic_schedule;

s_p_dynamic_schedule *new_s_p_dynamic_schedule(s_p_task_gen_func *task_gen_func,
                                               void *state,
                                               MPI_Comm communicator,
                                               size_t buffer_size);

const s_p_dyn_buffer *s_p_get_buffer(s_p_dynamic_schedule *schedule);

void s_p_next(s_p_dynamic_schedule *schedule);

bool s_p_done(s_p_dynamic_schedule *schedule);

void delete_s_p_dynamic_schedule(s_p_dynamic_schedule *schedule);

// handy guided self-scheduling generator
typedef struct gss_state_s {
  uint64_t current;
  uint64_t end;
  uint64_t grain_size;
  uint64_t all_rank_process_count;
} gss_state_t;

typedef struct simple_task_s {
  uint64_t begin;
  uint64_t end;
} simple_task;

gss_state_t new_gss_state(uint64_t begin, uint64_t end, uint64_t grain_size,
                          uint64_t current_rank_process_count,
                          MPI_Comm communicator);

bool gss_generator(void *state, s_p_dyn_buffer *buffer);

#ifdef __cplusplus
}
#endif

// clang-format off
#define S_P_GSS_UINT64_T_PAR_FOR(_s_p_index_var, _s_p_begin, _s_p_end,         \
                                 _s_p_grain_size, _s_p_communicator)           \
  {                                                                            \
    gss_state_t _s_p_gss_state;                                                \
    s_p_dynamic_schedule *_s_p_scheduler = NULL;                               \
_Pragma("omp single copyprivate(_s_p_scheduler)")                              \
    {                                                                          \
      _s_p_gss_state =                                                         \
          new_gss_state(_s_p_begin, _s_p_end, _s_p_grain_size,                 \
                        omp_get_num_threads() * 4, _s_p_communicator);         \
      _s_p_scheduler = new_s_p_dynamic_schedule(                               \
          gss_generator, &_s_p_gss_state, _s_p_communicator,                   \
          omp_get_num_threads() * 4);                                          \
    }                                                                          \
    simple_task *_s_p_gss_task_buffer =                                        \
        (simple_task *)s_p_get_buffer(_s_p_scheduler);                         \
    simple_task _s_p_gss_task;                                                 \
    while (true) {                                                             \
      bool _s_p_done = false;                                                  \
_Pragma("omp critical(_s_p_scheduler_gen)")                                    \
      {                                                                        \
        if (s_p_done(_s_p_scheduler)) {                                        \
          _s_p_done = true;                                                    \
        } else {                                                               \
          _s_p_gss_task = *_s_p_gss_task_buffer;                               \
          s_p_next(_s_p_scheduler);                                            \
        }                                                                      \
      }                                                                        \
      if (_s_p_done) {                                                         \
        break;                                                                 \
      }                                                                        \
      for (uint64_t _s_p_index_var = _s_p_gss_task.begin;                      \
           (_s_p_index_var) < _s_p_gss_task.end; (_s_p_index_var)++)

#define S_P_GSS_UINT64_T_PAR_FOR_END                                           \
    }                                                                          \
_Pragma("omp barrier")                                                         \
_Pragma("omp single")                                                          \
    {                                                                          \
      delete_s_p_dynamic_schedule(_s_p_scheduler);                             \
    }                                                                          \
  }
// clang-format on