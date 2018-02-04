#include <stdlib.h>
#include <stdio.h>
#include "mpi.h"
#include <unistd.h>

int main(int argc, char* argv[]) {
  int x, y, np, me, i;
  int tag = 42;
  MPI_Status  status;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &np);
  MPI_Comm_rank(MPI_COMM_WORLD, &me);

  /* Check that we run on exactly two processors */
  if (np != 2) {
    if(!me)
      printf("this test uses 2 processors\n");
    MPI_Abort(MPI_COMM_WORLD, 1);	       /* Quit if there is only one processor */
  }

  MPI_Pcontrol(1);
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Pcontrol(2);
  MPI_Barrier(MPI_COMM_WORLD);
  
  x = me;
  if (me == 0) {
    MPI_Send(&x, 1, MPI_INT, 1, tag, MPI_COMM_WORLD);
  } 
  else { /* me == 1 */
    MPI_Recv (&y, 1, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);
  }
  
  MPI_Barrier(MPI_COMM_WORLD);
 
  MPI_Finalize();
  printf("success\n");
  exit(0);
}
