#include <stdlib.h>
#include <stdio.h>
#include "mpi.h"
#include <unistd.h>

int main(int argc, char* argv[]) {
  int x, y, np, me, i;
  int tag = 42;
  MPI_Status  status;
  MPI_Request request, request2;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &np);
  MPI_Comm_rank(MPI_COMM_WORLD, &me);

  /* Check that we run on exactly two processors */
  if (np != 2) {
    printf("this test uses 2 processors\n");
    MPI_Finalize();            /* Quit if there is only one processor */
    exit(0);
  }

  x = me;
  int j;
  for (j = 0; j < 2; j++){
	if (me == 0) {
        	for(i=0;i<1;i++) {
                	MPI_Send(&x, 1, MPI_INT, 1, tag, MPI_COMM_WORLD);
        	}
  	} else { /* me == 1 */
        	for(i=0;i<1;i++) {
                	MPI_Recv (&y, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
        	}
  	}
        MPI_Pcontrol(j);
        MPI_Barrier(MPI_COMM_WORLD);
  }

  MPI_Finalize();
  printf("success\n");
  exit(0);
}
