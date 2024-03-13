#include <mpi.h>
#include <simple_parallel_for_c/main.h>
#include <simple_parallel_for_c/omp_dynamic_schedule.h>
#include <simple_parallel_for_c/simple_parallel_for_c.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    int abc = 5;

    int* def = malloc(10 * sizeof(int));
    def[0]   = 50;
    SIMPLE_PARALLEL_C_BEGIN(true)

#pragma omp parallel
    {

        int begin, end;
        int my_rank, comm_size;
        MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
        MPI_Comm_size(MPI_COMM_WORLD, &comm_size);

        int count = 0;
        // #pragma omp masked
        //         { simple_parallel_omp_generator_set(0, 20, 2, 1,
        //         simple_parallel_run); }

        //         bool should_break = false;

        //         while (true) {
        // #pragma omp critical
        //             {
        //                 should_break = simple_parallel_omp_generator_done();
        //                 if (!should_break) {
        //                     simple_parallel_omp_generator_next(&begin, &end);
        //                 }
        //             }
        //             if (should_break) {
        //                 break;
        //             }
        SIMPLE_PARALLEL_C_OMP_DYNAMIC_SCHEDULE_BEGIN(
            0, 200000000, 1, 8, 2, simple_parallel_run, begin, end)
        printf("begin: %d, end: %d, rank:%d, size:%d \n",
               begin,
               end,
               my_rank,
               comm_size);
        count += abc;
        count += def[0];
        SIMPLE_PARALLEL_C_OMP_DYNAMIC_SCHEDULE_END
    }
    SIMPLE_PARALLEL_C_END
}
