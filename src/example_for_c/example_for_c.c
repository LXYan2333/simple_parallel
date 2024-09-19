#include <assert.h>
#include <mpi.h>
#include <omp.h>
#include <simple_parallel_for_c/main.h>
#include <simple_parallel_for_c/omp_dynamic_schedule.h>
#include <simple_parallel_for_c/simple_parallel_for_c.h>
#include <stdio.h>
#include <stdlib.h>

struct state {
    size_t begin;
    size_t end;
    size_t current;
};

bool scheduler_func(void* state, void* task_buffer) {
    struct state* s = (struct state*)state;
    if (s->current < s->end) {
        // printf("Task: %d\n", s->current);
        *(size_t*)task_buffer = s->current;
        s->current++;
        return true;
    }
    return false;
}

int main() {
    int abc = 5;

    int* def = malloc(10 * sizeof(int));
    def[0]   = 50;

    SIMPLE_PARALLEL_C_BEGIN(true)

        int my_rank, comm_size;
        MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
        MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
        MPI_Bcast(&comm_size, 1, MPI_INT, 0, s_p_comm);
        size_t           all_count = 0;
        struct gss_state s;


#pragma omp parallel
        {
            struct simple_task* buffer = NULL;
            // make sure your own task buffer is not larger than
            // S_P_DEFAULT_C_TASK_BUFFER_SIZE
            // if you do need a larger buffer, you must change the value by
            // setting simple_parallel_DEFAULT_C_TASK_BUFFER_SIZE in cmake
            // configuration stage and recompile simple_parallel_for_c library
            // again
            assert(sizeof(struct simple_task)
                   <= S_P_DEFAULT_C_TASK_BUFFER_SIZE);
            size_t  count  = 0;
#pragma omp masked
            {
                s.begin   = 0;
                s.end           = 2000;
                s.grain_size    = 4;
                s.process_count = omp_get_num_threads();
                MPI_Allreduce(MPI_IN_PLACE,
                              &s.process_count,
                              1,
                              MPI_INT,
                              MPI_SUM,
                              s_p_comm);
                s.process_count *= 4;
            }
#pragma omp barrier
            S_P_PARALLEL_C_DYNAMIC_SCHEDULE_BEGIN(
                s_p_comm, (void**)&buffer, guided_self_scheduler, &s)
            printf("Rank %d: begin: %zu, end: %zu\n",
                   my_rank,
                   buffer->begin,
                   buffer->end);
            for (size_t i = buffer->begin; i < buffer->end; i++) {
                count += i;
            }
            S_P_PARALLEL_C_DYNAMIC_SCHEDULE_END
#pragma omp barrier
#pragma omp critical
            {
                all_count += count;
            }
#pragma omp barrier
#pragma omp masked
            { printf("Rank %d: All count is %zu\n", my_rank, all_count); }
        }

        MPI_Barrier(s_p_comm);
        MPI_Allreduce(
            MPI_IN_PLACE, &all_count, 1, MPI_INT64_T, MPI_SUM, s_p_comm);
        printf("All count is %zu\n", all_count);


        SIMPLE_PARALLEL_C_END return 0;
}
