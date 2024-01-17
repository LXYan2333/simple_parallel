#include <simple_parallel_for_c/main.h>
#include <simple_parallel_for_c/omp_dynamic_schedule.h>
#include <simple_parallel_for_c/simple_parallel_for_c.h>
#include <stdio.h>

int main(void) {
    SIMPLE_PARALLEL_C_BEGIN(true)

    int end = 90;
#pragma omp parallel
    {
        printf("hello,world!\n");
        int c = 80;
        SIMPLE_PARALLEL_C_OMP_DYNAMIC_SCHEDULE_BEGIN
#pragma omp critical
        {
            printf("task is:%d\n", _s_p_task);
        }
        SIMPLE_PARALLEL_C_OMP_DYNAMIC_SCHEDULE_END(0, end, 5, 4)
    }
    SIMPLE_PARALLEL_C_END
    return 0;
}
