#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef _EXTERN_C_
#ifdef __cplusplus
#define _EXTERN_C_ extern "C"
#else /* __cplusplus */
#define _EXTERN_C_
#endif /* __cplusplus */
#endif /* _EXTERN_C_ */

#define PRIMARY 0
#define REPLICA 1

#define TAG_PARALLEL_COMM 9999
#define TAG_BARRIER_COMM 9998

#ifdef MPICH_HAS_C2F
_EXTERN_C_ void *MPIR_ToPointer(int);
#endif // MPICH_HAS_C2F

#ifdef PIC
/* For shared libraries, declare these weak and figure out which one was linked
   based on which init wrapper was called.  See mpi_init wrappers.  */
#pragma weak pmpi_init
#pragma weak PMPI_INIT
#pragma weak pmpi_init_
#pragma weak pmpi_init__
#endif /* PIC */

_EXTERN_C_ void pmpi_init(MPI_Fint *ierr);
_EXTERN_C_ void PMPI_INIT(MPI_Fint *ierr);
_EXTERN_C_ void pmpi_init_(MPI_Fint *ierr);
_EXTERN_C_ void pmpi_init__(MPI_Fint *ierr);

typedef struct {
  int rank;
  struct DyingRank *next;
} DyingRank;

int world_size;

int my_rank;
int **dead_rank;
DyingRank *dying_rank_list;

int get_execution_path();
int should_i_panic();
int am_i_dead();

/* ================== C Wrappers for MPI_Init ================== */
_EXTERN_C_ int PMPI_Init(int *argc, char ***argv);
_EXTERN_C_ int MPI_Init(int *argc, char ***argv) { 
  int _wrap_py_return_val = 0;

  _wrap_py_return_val = PMPI_Init(argc, argv);

  /**
   * Save world size (primary+replica) and my_rank in world context
   */
  PMPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  PMPI_Comm_size(MPI_COMM_WORLD, &world_size);

  /**
   * Create data structure to store dead ranks
   */
  dead_rank = (int **)malloc((world_size/2)*sizeof(int *));
  int i;
  for(i=0;i<world_size/2;i++) {
    dead_rank[i]= (int *)malloc(2*sizeof(int));
    dead_rank[i][0] = 0;
    dead_rank[i][1] = 0;
  }

  return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Finalize ================== */
_EXTERN_C_ int PMPI_Finalize();
_EXTERN_C_ int MPI_Finalize() { 
  int _wrap_py_return_val = 0;

  _wrap_py_return_val = PMPI_Finalize();

  return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Barrier ================== */
_EXTERN_C_ int PMPI_Barrier(MPI_Comm comm);
_EXTERN_C_ int MPI_Barrier(MPI_Comm comm) { 
  int _wrap_py_return_val = 0;

  /**
   * Get to know which ranks are dying at this barrier
   */
  while(dying_rank_list != NULL) {
    DyingRank *dying_rank = dying_rank_list;
    int killed_rank = dying_rank->rank;
  
    if(killed_rank < world_size/2) {
      // primary rank killed
      dead_rank[killed_rank][0] = 1;
    } else {
      // replica rank killed
      killed_rank-=(world_size/2);
      dead_rank[killed_rank][1] = 1;
    }

    dying_rank_list = (DyingRank *)dying_rank_list->next;
    dying_rank->next = NULL;
    free(dying_rank);
  }

  /**
   * If I'm dead or both me & my counter part is dead, then exit
   */
  if(should_i_panic() || am_i_dead()) {
    // this rank will not be used any further
    // will terminate after MPI_Finalize call
    MPI_Finalize();
    exit(0);
  }

  /**
   * Barrier implementation
   * 1. All ranks in this execution path (PRIMARY/REPLICA) tell the first node alive that they've reached the barrier
   * 2. First node alive tells all ranks that everyone has reached the barrier and they can continue
   */
  int first_rank_alive;
  int i;
  if(get_execution_path() == PRIMARY) {
    for(i=0;i<world_size/2;i++) {
      if(dead_rank[i][0] != 1) {
	first_rank_alive = i;
	break;
      }
    }
  } else {
    for(i=0;i<world_size/2;i++) {
      if(dead_rank[i][1] != 1) {
	first_rank_alive = i+(world_size/2);
	break;
      }
    }
  }

  if(my_rank == first_rank_alive) {
    int local_send_buf = 1;
    int local_recv_buf;
    MPI_Status status;

    // receive from all ranks that are alive in this execution path
    if(get_execution_path() == PRIMARY) {
      for(i=0;i<(world_size/2);i++) {
	if(dead_rank[i][0] != 1 && i != first_rank_alive) {
	  PMPI_Recv(&local_recv_buf, 1, MPI_INT, i, TAG_BARRIER_COMM, MPI_COMM_WORLD, &status);
	}
      }
    } else {
      for(i=0;i<(world_size/2);i++) {
	if(dead_rank[i][1] != 1 && i+(world_size/2) != first_rank_alive) {
	  PMPI_Recv(&local_recv_buf, 1, MPI_INT, i+(world_size/2), TAG_BARRIER_COMM, MPI_COMM_WORLD, &status);
	}
      }
    }
    // send to all ranks that are alive in this execution path
    if(get_execution_path() == PRIMARY) {
      for(i=0;i<(world_size/2);i++) {
	if(dead_rank[i][0] != 1 && i != first_rank_alive) {
	  PMPI_Send(&local_send_buf, 1, MPI_INT, i, TAG_BARRIER_COMM, MPI_COMM_WORLD);
	}
      }
    } else {
      for(i=0;i<(world_size/2);i++) {
	if(dead_rank[i][1] != 1 && i+(world_size/2) != first_rank_alive) {
	  PMPI_Send(&local_send_buf, 1, MPI_INT, i+(world_size/2), TAG_BARRIER_COMM, MPI_COMM_WORLD);
	}
      }
    }
  } else {
    int local_send_buf = 1;
    int local_recv_buf;
    MPI_Status status;

    // send to first_rank_alive in this execution path
    PMPI_Send(&local_send_buf, 1, MPI_INT, first_rank_alive, TAG_BARRIER_COMM, MPI_COMM_WORLD);

    // recv from first_rank_alive in this execution path
    PMPI_Recv(&local_recv_buf, 1, MPI_INT, first_rank_alive, TAG_BARRIER_COMM, MPI_COMM_WORLD, &status);
  }

  // _wrap_py_return_val = PMPI_Barrier(comm);
  return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Send ================== */
_EXTERN_C_ int PMPI_Send(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);
_EXTERN_C_ int MPI_Send(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm) { 
  int _wrap_py_return_val = 0;

  if(should_i_panic() || am_i_dead()) {
    return;
  }

  if(get_execution_path() == PRIMARY) {
    if(dead_rank[dest][0] != 1) {
      // if dest rank is not dead, send the message
      _wrap_py_return_val = PMPI_Send(buf, count, datatype, dest, tag, MPI_COMM_WORLD);
    }

    int my_group_rank = my_rank;
      
    if(dead_rank[my_group_rank][1] != 1) {
      // if counter-part is up,

      int local_send_buf = 1;
      int local_recv_buf;
      MPI_Status status;

      // let replica execution know that send finished
      PMPI_Send(&local_send_buf, 1, MPI_INT, (world_size/2)+my_group_rank, TAG_PARALLEL_COMM, MPI_COMM_WORLD);

      // wait until replica execution has reached this point
      PMPI_Recv(&local_recv_buf, 1, MPI_INT, (world_size/2)+my_group_rank, TAG_PARALLEL_COMM, MPI_COMM_WORLD, &status);
    } else {
      // replica execution counter-part rank is dead, so take responsibility to send to its dest
      // wait, check if dest is up and running
      if(dead_rank[dest][1] != 1) {
	PMPI_Send(buf, count, datatype, (world_size/2)+dest, tag, MPI_COMM_WORLD);
      } else {
	// nobody to send to on replica exec
	// continue primary exec
      }
    }
  } else {
    if(dead_rank[dest][1] != 1) {
      // if dest rank is not dead, send the message
      _wrap_py_return_val = PMPI_Send(buf, count, datatype, dest+(world_size/2), tag, MPI_COMM_WORLD);
    }
      
    int my_group_rank = my_rank - (world_size/2);
    
    if(dead_rank[my_group_rank][0] != 1) {
      // if counter-part is up,
      int local_send_buf = 1;
      int local_recv_buf;
      MPI_Status status;

      // wait until primary execution has reached this point
      PMPI_Recv(&local_recv_buf, 1, MPI_INT, my_group_rank, TAG_PARALLEL_COMM, MPI_COMM_WORLD, &status);

      // let primary execution know that I've finished sending
      PMPI_Send(&local_send_buf, 1, MPI_INT, my_group_rank, TAG_PARALLEL_COMM, MPI_COMM_WORLD);	
    } else {
      // primary execution counter-part rank is dead, so take responsibility to send to its dest
      // wait, check if dest is up and running
      if(dead_rank[dest][0] != 1) {
	PMPI_Send(buf, count, datatype, dest, tag, MPI_COMM_WORLD);
      } else {
	// nobody to send to on primary exec
	// continue replica exec
      }
    }
  }
  
  return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Recv ================== */
_EXTERN_C_ int PMPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status);
_EXTERN_C_ int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status) { 
  int _wrap_py_return_val = 0;

  if(should_i_panic() || am_i_dead()) {
    return;
  }

  if(source == MPI_ANY_SOURCE) {
    // recv from any source that is alive, either primary/replica
    _wrap_py_return_val = PMPI_Recv(buf, count, datatype, source, tag, MPI_COMM_WORLD, status);
  } else {
    if(get_execution_path() == PRIMARY) {
      if(dead_rank[source][0] != 1) {
	// source rank is not dead, recv the message from primary
	_wrap_py_return_val = PMPI_Recv(buf, count, datatype, source, tag, MPI_COMM_WORLD, status);
      
      } else {
	// source rank is dead, recv the message from replica
	_wrap_py_return_val = PMPI_Recv(buf, count, datatype, (world_size/2)+source, tag, MPI_COMM_WORLD, status);
      } 
    } else {
      if(dead_rank[source][1] != 1) {
	// source rank is not dead, recv the message from replica
	_wrap_py_return_val = PMPI_Recv(buf, count, datatype, source+(world_size/2), tag, MPI_COMM_WORLD, status);
      
      } else {
	// source rank is dead, recv the message from primary
	_wrap_py_return_val = PMPI_Recv(buf, count, datatype, source, tag, MPI_COMM_WORLD, status);

      }
    }
  }
  return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_rank ================== */
_EXTERN_C_ int PMPI_Comm_rank(MPI_Comm comm, int *rank);
_EXTERN_C_ int MPI_Comm_rank(MPI_Comm comm, int *rank) { 
  int _wrap_py_return_val = 0;
  
  int local_rank;
  _wrap_py_return_val = PMPI_Comm_rank(comm, &local_rank);

  if(local_rank < world_size/2) {
    *rank = local_rank;
  } else {
    *rank = local_rank - world_size/2;
  }

  return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_size ================== */
_EXTERN_C_ int PMPI_Comm_size(MPI_Comm comm, int *size);
_EXTERN_C_ int MPI_Comm_size(MPI_Comm comm, int *size) { 
  int _wrap_py_return_val = 1;

  *size = world_size/2;
  //_wrap_py_return_val = PMPI_Comm_size(comm, size);
  return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Pcontrol ================== */
_EXTERN_C_ int PMPI_Pcontrol(const int level, ...);
_EXTERN_C_ int MPI_Pcontrol(const int level, ...) { 
  int _wrap_py_return_val = 0;

  /**
   * Add the rank to dying ranks list
   */
  DyingRank *dying_rank = (DyingRank *)malloc(1*sizeof(DyingRank));
  dying_rank->rank = level;
  dying_rank->next = NULL;
  
  if(dying_rank_list == NULL) {
    dying_rank_list = dying_rank;
  } else {
    dying_rank->next = (struct DyingRank*)dying_rank_list;
    dying_rank_list = dying_rank;
  }

  //_wrap_py_return_val = PMPI_Pcontrol(level);
  return _wrap_py_return_val;
}

int get_execution_path() {
  return (my_rank < world_size/2)?PRIMARY:REPLICA;
}

/**
 * Return true if for a rank, both the rank itself and its counter part is dead
 */
int should_i_panic() {
  int panic = 0;

  int i;
  for(i=0;i<(world_size/2);i++) {
    if(dead_rank[i][0] == 1 && dead_rank[i][1] == 1) {
      // rank i has failed in both primary & replica execution
      // exit
      panic = 1;
      break;
    }
  }

  return panic;
}

/**
 * Return true if the rank is dead
 */
int am_i_dead() {
  int dead = 0;

  int my_group_rank;
  if(get_execution_path() == PRIMARY) {
    my_group_rank = my_rank;
    dead = dead_rank[my_group_rank][0];
  } else {
    my_group_rank = my_rank - (world_size/2);
    dead = dead_rank[my_group_rank][1];
  }

  return dead;
}
