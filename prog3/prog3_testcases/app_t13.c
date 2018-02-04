#include <stdlib.h>
#include <stdio.h>
#include "mpi.h"
#include <unistd.h>

int main(int argc, char* argv[]) {
  int x, y, np, me, i, j, iters;
  int tag = 42;
  int unfortunate_rank = 0;
  MPI_Status status;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &np);
  MPI_Comm_rank(MPI_COMM_WORLD, &me);

  /* Check that we run on exactly two processors */
  if (np != 4) {
    printf("this test uses 2 processors\n");
    MPI_Finalize();            /* Quit if there is only one processor */
    exit(0);
  }

  for (iters = 0; iters < 4; iters++) {
    if (me == 0)  {
      for (j = 1; j < np; j++)  {
        MPI_Recv (&y, 1, MPI_INT, j, tag, MPI_COMM_WORLD, &status);
      }
      for (j = 1; j < np; j++)  {
        MPI_Send(&x, 1, MPI_INT, j, tag, MPI_COMM_WORLD);
      }
    }
    else {
      MPI_Send(&x, 1, MPI_INT, 0, tag, MPI_COMM_WORLD);
      MPI_Recv (&y, 1, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);
    }

    if (iters % 2 == 0){
    	MPI_Pcontrol(iters);
    }else{
    	MPI_Pcontrol(4+iters);
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }

  MPI_Finalize();

  printf("success\n");
  exit(0);
}

