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

  /* Check that we run on exactly two processors */
  if (np != 4) {
    printf("this test uses 4 processors, 2 leads and 2 replicas, but size should be 2, not %d\n", np);
    MPI_Finalize();	       /* Quit if there is only one processor */
    exit(0);
  }
  
  x = me;
  /* MPI_Pcontrol(0); */
  MPI_Pcontrol(1);
  /* MPI_Pcontrol(2); */
  MPI_Pcontrol(3);
  MPI_Pcontrol(4);
  //  MPI_Pcontrol(5);
  MPI_Pcontrol(6);
  //  MPI_Pcontrol(7);
  MPI_Barrier(MPI_COMM_WORLD);
  //regular send and recv
  if (me != 0) {
    x = 88;
    MPI_Send(&x, 1, MPI_INT, 0, tag, MPI_COMM_WORLD);
  } 
  else { /* me == 0 */
    MPI_Recv (&y, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
    printf( "Rank %d receives %d from rank %d.\n", me, y, status.MPI_SOURCE );
    MPI_Recv (&y, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
    printf( "Rank %d receives %d from rank %d.\n", me, y, status.MPI_SOURCE );
    MPI_Recv (&y, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
    printf( "Rank %d receives %d from rank %d.\n", me, y, status.MPI_SOURCE );
    //MPI_Recv (&y, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    //MPI_Recv (&y, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    //MPI_Recv (&y, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  }

  MPI_Finalize();
  printf("success\n");
  exit(0);
}
