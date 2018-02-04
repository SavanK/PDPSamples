#include <stdlib.h>
#include <stdio.h>
#include "mpi.h"

int main(int argc, char* argv[]) {
  int x, y, np, me, i;
  int tag = 42;
  MPI_Status  status;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &np);
  MPI_Comm_rank(MPI_COMM_WORLD, &me);

  if (np != 2) {
    printf("this test uses 4 processors, 2 leads and 2 replicas, but size should be 2, not %d\n", np);
    MPI_Finalize();            /* Quit if there is only one processor */
    exit(0);
  }
  
  x = me;

  if (me == 0) {
    MPI_Send(&x, 1, MPI_INT, 1, tag, MPI_COMM_WORLD);
  } 
  else { 
    MPI_Recv (&y, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
  }
MPI_Pcontrol(0);
	MPI_Barrier(MPI_COMM_WORLD);
  if (me == 0) {
    MPI_Recv (&y, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
  } 
  else { 
    MPI_Send (&x, 1, MPI_INT, 0, tag, MPI_COMM_WORLD);
  }

MPI_Pcontrol(3);
	MPI_Barrier(MPI_COMM_WORLD);



  MPI_Finalize();

  printf("success\n");
  exit(0);
}
