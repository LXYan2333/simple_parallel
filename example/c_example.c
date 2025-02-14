#include <mpi.h>
#include <simple_parallel/c/c_binding.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
  double stack_test[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  double *heap_test = (double *)malloc(10 * sizeof(double));
  for (size_t i = 0; i < 10; i++) {
    heap_test[i] = 10 - i;
  }

  S_P_PAR_BEGIN(true, ctx, (stack_test, 10, MPI_SUM), (heap_test, 10, MPI_SUM))

  int my_rank = 0;
  int world_size = 0;
  MPI_Comm_rank(ctx.comm, &my_rank);
  MPI_Comm_size(ctx.comm, &world_size);

  MPI_Barrier(ctx.comm);

  for (size_t i = 0; i < world_size; i++) {
    if (my_rank == i) {
      printf("Hello from rank %d\n", my_rank);
      printf("stack_test of rank: %d: ", my_rank);
      for (size_t j = 0; j < 10; j++) {
        printf("%f ", stack_test[j]);
      }
      printf("\n");
      printf("heap_test of rank: %d: ", my_rank);
      for (size_t j = 0; j < 10; j++) {
        printf("%f ", heap_test[j]);
      }
      printf("\n");
    }
    MPI_Barrier(ctx.comm);
  }

  for (size_t i = 0; i < 10; i++) {
    stack_test[i] += 2;
    heap_test[i] += 3;
  }

  S_P_PAR_END(ctx)

  printf("Exited parallel context\n");
  printf("stack_test: ");
  for (size_t i = 0; i < 10; i++) {
    printf("%f ", stack_test[i]);
  }
  printf("\n");
  printf("heap_test: ");
  for (size_t i = 0; i < 10; i++) {
    printf("%f ", heap_test[i]);
  }
  printf("\n");

  return 0;
}