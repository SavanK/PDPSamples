#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "mpi.h"
#include <unistd.h>

static inline void exch(int me, int *x, int *y, const int tag, int val){
  MPI_Status status;
  if (me == 0) {
    MPI_Send(x, 1, MPI_INT, 1, tag, MPI_COMM_WORLD);
  }
  else if (me != 3) {
    MPI_Recv(y, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
    MPI_Send(x, 1, MPI_INT, me+1, tag, MPI_COMM_WORLD);
  }
  else {
    MPI_Recv(y, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
    if (*y != val) assert(0);
  }
}

int main(int argc, char* argv[]) {
  int x, y, np, me, i, tag = 2;
  MPI_Status status;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &np);
  MPI_Comm_rank(MPI_COMM_WORLD, &me);

  /* Check that we run on exactly four processors */
  if (np != 4) {
    if(!me)
      printf("this test uses 4 processors\n");
    MPI_Abort(MPI_COMM_WORLD, 1);	       /* Quit if there is only one processor */
  }

  x = 17;

  exch(me, &x, &y, tag, 17);
  x++;

  //MPI_Pcontrol(0);
  MPI_Barrier(MPI_COMM_WORLD);

  exch(me, &x, &y, tag, 18);
  x++;

  MPI_Pcontrol(0);
  MPI_Pcontrol(1);
  MPI_Pcontrol(2);
  MPI_Barrier(MPI_COMM_WORLD);

  exch(me, &x, &y, tag, 19);
  x++;

  //MPI_Pcontrol(2);
  MPI_Barrier(MPI_COMM_WORLD);

  exch(me, &x, &y, tag, 20);

  //MPI_Pcontrol(7);
  MPI_Barrier(MPI_COMM_WORLD);

  MPI_Finalize();
  printf("success\n");

  exit(0);
}
