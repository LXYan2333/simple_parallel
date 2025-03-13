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

s_p_dynamic_schedule *s_p_new_dynamic_schedule(s_p_task_gen_func *task_gen_func,
                                               void *state,
                                               MPI_Comm communicator,
                                               size_t buffer_size);

const s_p_dyn_buffer *s_p_get_buffer(s_p_dynamic_schedule *schedule);

void s_p_next(s_p_dynamic_schedule *schedule);

bool s_p_done(s_p_dynamic_schedule *schedule);

void s_p_delete_dynamic_schedule(s_p_dynamic_schedule *schedule);

// handy guided self-scheduling generator
typedef struct s_p_gss_state_s {
  uint64_t current;
  uint64_t end;
  uint64_t grain_size;
  uint64_t all_rank_process_count;
} s_p_gss_state_t;

typedef struct s_p_simple_task_s {
  uint64_t begin;
  uint64_t end;
} s_p_simple_task;

s_p_gss_state_t s_p_new_gss_state(uint64_t begin, uint64_t end,
                                  uint64_t grain_size,
                                  uint64_t current_rank_process_count,
                                  MPI_Comm communicator);

bool s_p_gss_generator(void *state, s_p_dyn_buffer *buffer);

typedef struct s_p_ij_point_s {
  uint64_t i;
  uint64_t j;
} s_p_ij_point_t;

typedef struct s_p_collapse_2_task_s {
  s_p_ij_point_t ij_begin;
  s_p_ij_point_t ij_end;
} s_p_collapse_2_task;

typedef struct collapse_2_gss_gen_func_res_s {
  s_p_ij_point_t ij_end;
  size_t count;
} collapse_2_gss_gen_func_res_t;

typedef collapse_2_gss_gen_func_res_t(collapse_2_gss_gen_func)(
    s_p_ij_point_t current, s_p_ij_point_t final_end, uint64_t expect_count,
    const void *state);

typedef struct s_p_collapse_2_gss_state_s {
  s_p_ij_point_t current;
  s_p_ij_point_t end;
  uint64_t remaining_index;
  uint64_t grain_size;
  uint64_t all_rank_process_count;
  collapse_2_gss_gen_func *gen_func;
  const void *gen_func_state;
} s_p_collapse_2_gss_state_t;

s_p_collapse_2_gss_state_t s_p_new_collapse_2_gss_state(
    uint64_t i_begin, uint64_t i_end, uint64_t j_begin, uint64_t j_end,
    uint64_t grain_size, uint64_t all_index_count,
    collapse_2_gss_gen_func *gen_func, const void *gen_func_state,
    uint64_t current_rank_process_count, MPI_Comm communicator);

bool s_p_collapse_2_gss_generator(void *state, s_p_dyn_buffer *buffer);

#ifdef __cplusplus
}
#endif

// clang-format off
#define S_P_GSS_UINT64_T_PAR_FOR(_s_p_index_var, _s_p_begin, _s_p_end,         \
                                 _s_p_grain_size, _s_p_communicator)           \
  {                                                                            \
    s_p_gss_state_t _s_p_gss_state;                                            \
    s_p_dynamic_schedule *_s_p_scheduler = NULL;                               \
_Pragma("omp single copyprivate(_s_p_scheduler)")                              \
    {                                                                          \
      _s_p_gss_state = s_p_new_gss_state(                                      \
          _s_p_begin, _s_p_end, _s_p_grain_size,                               \
          (size_t)omp_get_num_threads() * 40, _s_p_communicator);              \
      _s_p_scheduler = s_p_new_dynamic_schedule(                               \
          s_p_gss_generator, &_s_p_gss_state, _s_p_communicator,               \
          (size_t)omp_get_num_threads() * 4);                                  \
    }                                                                          \
    s_p_simple_task *_s_p_gss_task_buffer =                                    \
        (s_p_simple_task *)s_p_get_buffer(_s_p_scheduler);                     \
    s_p_simple_task _s_p_gss_task;                                             \
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
           (_s_p_index_var) < _s_p_gss_task.end; (_s_p_index_var)++){

#define S_P_GSS_UINT64_T_PAR_FOR_END                                           \
      }                                                                        \
    }                                                                          \
    _Pragma("omp barrier")                                                     \
    _Pragma("omp single")                                                      \
    {                                                                          \
      s_p_delete_dynamic_schedule(_s_p_scheduler);                             \
    }                                                                          \
  }
// clang-format on

#define S_P_GSS_UINT64_T_PAR_FOR_COLLAPSE_2(                                   \
    _s_p_grain_size, _s_p_communicator, _s_p_all_index_count,                  \
    _s_p_i_index_var, _s_p_i_begin, _s_p_i_end, _s_p_j_index_var,              \
    _s_p_j_begin, _s_p_j_end, _s_p_collapse_2_gss_gen_func,                    \
    _s_p_collapse_2_gss_gen_func_state)                                        \
  {                                                                            \
    s_p_collapse_2_gss_state_t _s_p_collapse_2gss_state;                       \
    s_p_dynamic_schedule *_s_p_scheduler = NULL;                               \
    const s_p_ij_point_t _s_p_final_end = {_s_p_i_end, _s_p_j_end};            \
    _Pragma("omp single copyprivate(_s_p_scheduler)") {                        \
      _s_p_collapse_2gss_state = s_p_new_collapse_2_gss_state(                 \
          _s_p_i_begin, _s_p_i_end, _s_p_j_begin, _s_p_j_end, _s_p_grain_size, \
          _s_p_all_index_count, _s_p_collapse_2_gss_gen_func,                  \
          _s_p_collapse_2_gss_gen_func_state,                                  \
          (size_t)omp_get_num_threads() * 40, _s_p_communicator);              \
      _s_p_scheduler = s_p_new_dynamic_schedule(                               \
          s_p_collapse_2_gss_generator, &_s_p_collapse_2gss_state,             \
          _s_p_communicator, (size_t)omp_get_num_threads());                   \
    }                                                                          \
    s_p_collapse_2_task *_s_p_gss_collapse_2_task_buffer =                     \
        (s_p_collapse_2_task *)s_p_get_buffer(_s_p_scheduler);                 \
    s_p_collapse_2_task _s_p_collapse_2_gss_task;                              \
    while (true) {                                                             \
      bool _s_p_done = false;                                                  \
      _Pragma("omp critical(_s_p_scheduler_gen)") {                            \
        if (s_p_done(_s_p_scheduler)) {                                        \
          _s_p_done = true;                                                    \
        } else {                                                               \
          _s_p_collapse_2_gss_task = *_s_p_gss_collapse_2_task_buffer;         \
          s_p_next(_s_p_scheduler);                                            \
        }                                                                      \
      }                                                                        \
      if (_s_p_done) {                                                         \
        break;                                                                 \
      }                                                                        \
      for (s_p_ij_point_t _s_p_ij = _s_p_collapse_2_gss_task.ij_begin;         \
           _s_p_ij.i != _s_p_collapse_2_gss_task.ij_end.i ||                   \
           _s_p_ij.j != _s_p_collapse_2_gss_task.ij_end.j;                     \
           _s_p_ij = _s_p_collapse_2_gss_gen_func(                             \
                         _s_p_ij, _s_p_final_end, 1,                           \
                         _s_p_collapse_2_gss_gen_func_state)                   \
                         .ij_end) {                                            \
        uint64_t _s_p_i_index_var = _s_p_ij.i;                                 \
        uint64_t _s_p_j_index_var = _s_p_ij.j;

#define S_P_GSS_UINT64_T_PAR_FOR_COLLAPSE_2_END                                \
  }                                                                            \
  }                                                                            \
  _Pragma("omp barrier") _Pragma("omp single") {                               \
    s_p_delete_dynamic_schedule(_s_p_scheduler);                               \
  }                                                                            \
  }
// clang-format off
// clang-format on