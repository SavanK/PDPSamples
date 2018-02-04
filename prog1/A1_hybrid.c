#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <mpi.h>
#include <pthread.h>
#include "dissemination_barrier.h"

#define TAG 13

// Shared variables (b/w threads)
int size;
int max_iters;
int num_nodes;
double **matrix;
int my_rank;
int strip_size;
int num_of_threads;
// Each thread in the node writes its max_diff in this array
// They write to different index and thus no race condition
double *l_max_diffs;
// This variable contains the node's max_diff 
// Written after all threads complete
double node_max_diff;
// Max_diff of the entire matrix after MAX_ITERS
double max_diff;

void initialize_matrix();
void divide_matrix();
void wait_for_matrix();
void red_black_reduction(int thread_id);
void *worker(void *arg);
void barrier_sync();

int main(int argc, char *argv[]) {
  int i, provided;
  double start_time, end_time;
  pthread_t *threads;
  int *params;
  
  if(argc < 3) {
    printf("Too less arguments. Require matrix size and MAX_ITERS\n");
    exit(1);
  } else {
    size = atoi(argv[1]);
    max_iters = atoi(argv[2]);
    num_of_threads = atoi(argv[3]);
  }

  MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &num_nodes);

  // Barrier initialize (Threads barrier)
  barrier_initialize(num_of_threads);

  // Even though a particular node works on {size/num_nodes} rows, 
  // it still needs one row below and one row above to calculate 
  // the new red and black points
  strip_size = (size/num_nodes)+2;

  // Initialize matrix
  initialize_matrix();

  if (my_rank==0) {
    // Divide the matrix among nodes
    divide_matrix();
  } else {
    // Wait to receive my part of the matrix
    wait_for_matrix();
  }

  // Start time
  if(my_rank==0) {
    start_time = MPI_Wtime();
  }

  // Allocate thread handles
  threads = (pthread_t *) malloc(num_of_threads * sizeof(pthread_t));
  params = (int *) malloc(num_of_threads * sizeof(int));

  // Create threads
  for (i = 0; i < num_of_threads; i++) {
    params[i] = i;
    pthread_create(&threads[i], NULL, worker, (void *)(&params[i]));
  }

  for (i = 0; i < num_of_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  // End time
  if(my_rank==0) {
    end_time = MPI_Wtime();
  }

  MPI_Finalize();

  if(my_rank==0) {
    printf("%d %d %.3f %.5f\n", num_nodes, num_of_threads, (end_time-start_time), max_diff);
  }
}

void *worker(void *arg) {
  int id = *((int *) arg);
  red_black_reduction(id);
  return NULL;
}

void red_black_reduction(int thread_id) {
  int iters;
  int i,j;

  // Compute bounds for this thread
  int start_row = (thread_id * ((strip_size-2)/num_of_threads)) + 1;
  int end_row = start_row + ((strip_size-2)/num_of_threads) - 1;

  /* Ignoring the fact that 8 nodes can divide the matrix in odd numbers. 
     Because the problem was relaxed by only permitting matrices in multiples of 64. */

  for(iters=0;iters<max_iters+1;iters++) {
    // Reduce all the "RED" points
    for(i=start_row;i<=end_row;i++) {
      int jStart;
      jStart=(i%2==1)?1:2;
      for(j=jStart;j<size+1;j=j+2) {
	double new_value = (matrix[i-1][j]+matrix[i][j-1]+
			    matrix[i+1][j]+matrix[i][j+1])*0.25;
	if(iters==max_iters) {
	  double my_diff = new_value-matrix[i][j];
	  if(my_diff > l_max_diffs[thread_id]) {
	    l_max_diffs[thread_id] = my_diff;
	  }
	}
	matrix[i][j]=new_value;
      }
    }

    // Wait for other threads in the node to finish reducing "RED" points
    barrier_wait(thread_id);
    if(thread_id==0) {
      // One of the threads, i.e., {Thread 0}, syncs node's first and last row
      barrier_sync();
    }
    // Other threads wait here until {Thread 0} finishes sync
    barrier_wait(thread_id);

    // Reduce all the "BLACK" points
    for(i=start_row;i<=end_row;i++) {
      int jStart;
      jStart=(i%2==1)?2:1;
      for(j=jStart;j<size+1;j=j+2) {
	double new_value = (matrix[i-1][j]+matrix[i][j-1]+
			    matrix[i+1][j]+matrix[i][j+1])*0.25;
	if(iters==max_iters) {
	  double my_diff = new_value-matrix[i][j];
	  if(my_diff > l_max_diffs[thread_id]) {
	    l_max_diffs[thread_id] = my_diff;
	  }
	}
	matrix[i][j]=new_value;
      }
    }

    // Wait for other threads in the node to finish reducing "BLACK" points
    barrier_wait(thread_id);
    if(thread_id==0) {
      // One of the threads, i.e., {Thread 0}, syncs node's first and last row
      barrier_sync();
    }
    // Other threads wait here until {Thread 0} finishes sync
    barrier_wait(thread_id);

    // off to next iteration
  }

  if(thread_id == 0) {
    // One of the threads, i.e., {Thread 0}, compares each thread's diff to find node_max_diff
    node_max_diff = l_max_diffs[0];
    for(i=1;i<num_of_threads;i++) {
      if(l_max_diffs[i] > node_max_diff) {
	node_max_diff = l_max_diffs[i];
      }
    }
  }

  if (my_rank == 0) {
    if(thread_id == 0) {
      MPI_Request request[num_nodes-1];
      MPI_Status status[num_nodes-1];
      double node_max_diffs[num_nodes];

      node_max_diffs[0]=node_max_diff;
    
      for (i=1; i<num_nodes; i++) {
	MPI_Irecv(&node_max_diffs[i], 1, MPI_DOUBLE, i, TAG, MPI_COMM_WORLD, &request[i-1]);
      }
      // Wait for all slaves to send their {node_max_diff}s
      MPI_Waitall(num_nodes-1, request, status);

      // Compare each node's diff and find max_diff
      max_diff=node_max_diffs[0];
      for(i=1;i<num_nodes;i++) {
	if(node_max_diffs[i]>max_diff)
	  max_diff=node_max_diffs[i];
      }
    }
  } else {
    if(thread_id == 0) {
      // Send my node's max_diff to master
      MPI_Send(&node_max_diff, 1, MPI_DOUBLE, 0, TAG, MPI_COMM_WORLD);
    }
  }
}

void barrier_sync() {
  int i;

  MPI_Request request_top_row, request_bottom_row;
  MPI_Status status_top_row, status_bottom_row;

  if(my_rank==0) {
    // recv top row of next
    MPI_Irecv(matrix[strip_size-1], (size+2), MPI_DOUBLE, my_rank+1, TAG, MPI_COMM_WORLD, &request_bottom_row);
    // send bottom row to next
    MPI_Send(matrix[strip_size-2], (size+2), MPI_DOUBLE, my_rank+1, TAG, MPI_COMM_WORLD);
    // wait for top row of next
    MPI_Wait(&request_bottom_row, &status_bottom_row);
  } else if(my_rank==(num_nodes-1)) {
    // recv bottom row of prev
    MPI_Irecv(matrix[0], (size+2), MPI_DOUBLE, my_rank-1, TAG, MPI_COMM_WORLD, &request_top_row);
    // send top row to prev
    MPI_Send(matrix[1], (size+2), MPI_DOUBLE, my_rank-1, TAG, MPI_COMM_WORLD);
    // wait for bottom row of prev
    MPI_Wait(&request_top_row, &status_top_row);
  } else {
    // recv both top and bottom rows
    MPI_Irecv(matrix[strip_size-1], (size+2), MPI_DOUBLE, my_rank+1, TAG, MPI_COMM_WORLD, &request_bottom_row);
    MPI_Irecv(matrix[0], (size+2), MPI_DOUBLE, my_rank-1, TAG, MPI_COMM_WORLD, &request_top_row);
      
    // send both top and bottom rows
    MPI_Send(matrix[strip_size-2], (size+2), MPI_DOUBLE, my_rank+1, TAG, MPI_COMM_WORLD);
    MPI_Send(matrix[1], (size+2), MPI_DOUBLE, my_rank-1, TAG, MPI_COMM_WORLD);

    // wait for both top and bottom rows
    MPI_Wait(&request_top_row, &status_top_row);
    MPI_Wait(&request_bottom_row, &status_bottom_row);
  }
}

void divide_matrix() {
  // Send each node it's piece of matrix
  int i;
  int offset = strip_size-2;
  int num_of_elements = strip_size * (size+2);
  for (i=1; i<num_nodes; i++) {
    MPI_Send(matrix[offset], num_of_elements, MPI_DOUBLE, i, TAG, MPI_COMM_WORLD);
    offset += (strip_size-2);
  }
}

void wait_for_matrix() {
  // Each node waits for it's piece
  MPI_Request request;
  MPI_Status status;
  int num_of_elements = strip_size * (size+2);
  MPI_Irecv(matrix[0], num_of_elements, MPI_DOUBLE, 0, TAG, MPI_COMM_WORLD, &request);
  MPI_Wait(&request, &status);
}

void initialize_matrix() {
  int i,j;
  double *tmp;

  // Allocate contiguous memory for the matrix
  if(my_rank==0) {
    // for full matrix
    tmp = (double *)malloc((size+2)*(size+2)*sizeof(double));
    matrix = (double **)malloc((size+2)*sizeof(double *));
    for (i=0; i<(size+2); i++)
      matrix[i] = &tmp[i*(size+2)];
  } else {
    // for part of the matrix that this node owns
    tmp = (double *)malloc(strip_size*(size+2)*sizeof(double));
    matrix = (double **)malloc(strip_size*sizeof(double *));
    for (i=0; i<strip_size; i++)
      matrix[i] = &tmp[i*(size+2)];
  }

  // allocate memory for local max diffs
  l_max_diffs = (double *)malloc(num_of_threads*sizeof(double));
  for(i=0;i<num_of_threads;i++) {
    l_max_diffs[i]=0;
  }

  if(my_rank==0) {
    // Initialize the matrix with exterior 1's and interior 0's
    for (i=0;i<(size+2);i++) {
      for (j=0;j<(size+2);j++) {
	double value=0.0f;
	if(i==0 || i==size+1) {
	  value=1.0f;
	} else if(j==0 || j==size+1) {
	  value=1.0f;
	}
	matrix[i][j] = value;
      }
    }
  }
}
