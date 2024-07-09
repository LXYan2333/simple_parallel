#include <mpi.h>
#include <simple_parallel_for_c/main.h>
#include <simple_parallel_for_c/omp_dynamic_schedule.h>
#include <simple_parallel_for_c/simple_parallel_for_c.h>
#include <stdio.h>
#include <stdlib.h>

struct state {
    int begin;
    int end;
    int current;
};

bool scheduler_func(void* state, void* task_buffer) {
    struct state* s = (struct state*)state;
    if (s->current < s->end) {
        // printf("Task: %d\n", s->current);
        *(int*)task_buffer = s->current;
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
        int all_count = 0;
#pragma omp parallel
        {
            int* buffer = NULL;

            int count = 0;

            static struct state s;
#pragma omp masked
            {
                s.begin   = 0;
                s.end     = 20;
                s.current = 0;
            }
            S_P_PARALLEL_C_DYNAMIC_SCHEDULE_BEGIN(
                s_p_comm, &buffer, scheduler_func, &s)
            printf("Rank %d: %d\n", my_rank, *buffer);
            count += *buffer;
            S_P_PARALLEL_C_DYNAMIC_SCHEDULE_END
#pragma omp barrier
#pragma omp critical
            {
                all_count += count;
            }
#pragma omp barrier
#pragma omp masked
            { printf("Rank %d: All count is %d\n", my_rank, all_count); }
        }


        SIMPLE_PARALLEL_C_END
        return 0;
}
