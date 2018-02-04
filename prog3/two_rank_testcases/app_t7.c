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
  else { /* me == 1 */
    MPI_Recv (y, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
    if (*y != val) assert(0);
  }
}


int main(int argc, char* argv[]) {
  int x, y, np, me, i;
  int tag = 42;
  MPI_Status  status;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &np);
  MPI_Comm_rank(MPI_COMM_WORLD, &me);
  printf("np = %d me = %d\n",np, me);

  /* Check that we run on exactly two processors */
  if (np != 2) {
    if(!me)
      printf("this test uses 2 processors\n");
    MPI_Abort(MPI_COMM_WORLD, 1);              /* Quit if there is only one processor */
  }

  x = 17;

  exch(me, &x, &y, tag, 17);
  x++;

  MPI_Barrier(MPI_COMM_WORLD);

  exch(me, &x, &y, tag, 18);
  x++;

  MPI_Pcontrol(0);

  MPI_Pcontrol(1);
  MPI_Barrier(MPI_COMM_WORLD);

  exch(me, &x, &y, tag, 19);

  MPI_Barrier(MPI_COMM_WORLD);

  MPI_Finalize();
  printf("success rank = %d tot_ranks = %d\n",me, np);
  exit(0);
}
