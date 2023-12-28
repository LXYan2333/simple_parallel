#include <iostream>
#include <mpi.h>
#include <simple_parallel/main.h>
#include <simple_parallel/simple_parallel.h>

auto main() -> int {

    // original from OpenMP's example
    // https://github.com/OpenMP/Examples/blob/main/data_environment/sources/reduction.6.c

    /*
     * @@name:	    reduction.6
     * @@type:	    C
     * @@operation:	run
     * @@expect:	unspecified
     * @@version:	omp_5.1
     */


    int mpi_reduce_result = 0;

    int a, i;

#pragma omp parallel shared(a) private(i)
    {
#pragma omp masked
        a = 0;

        // To avoid race conditions, add a barrier here.

#pragma omp for reduction(+ : a)
        for (i = 0; i < 100; i++) {
            a += i;
        }
    }
    std::cout << "Sum is " << mpi_reduce_result << "\n";
    return 0;
}
