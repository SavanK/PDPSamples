#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <mpi.h>

#define TAG 13

double** initialize_matrix(int size, int my_rank, int strip_size);
void divide_matrix(double **matrix, int size, int num_nodes, int strip_size);
void wait_for_matrix(double **matrix, int size, int strip_size);
double red_black_reduction(double **matrix, int size, int num_nodes, 
			   int strip_size, int my_rank, int max_iters);
void barrier_sync(double **matrix, int size, int num_nodes, int strip_size, int my_rank);

int main(int argc, char *argv[]) {
  int size;
  int max_iters;
  int num_nodes;
  double **matrix;
  int my_rank;
  int strip_size;

  double max_diff;
  int i;
  double start_time, end_time;
  
  if(argc < 3) {
    printf("Too less arguments. Require matrix size and MAX_ITERS\n");
    exit(1);
  } else {
    size = atoi(argv[1]);
    max_iters = atoi(argv[2]);
  }

  MPI_Init(&argc, &argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &num_nodes);

  // Even though a particular node works on {size/num_nodes} rows, 
  // it still needs one row below and one row above to calculate 
  // the new red and black points
  strip_size = (size/num_nodes)+2;

  // Initialize matrix
  matrix = initialize_matrix(size, my_rank, strip_size);

  if (my_rank==0) {
    // Divide the matrix among nodes
    divide_matrix(matrix, size, num_nodes, strip_size);
  } else {
    // Wait to receive my part of the matrix
    wait_for_matrix(matrix, size, strip_size);
  }

  if(my_rank==0) {
    // Start time
    start_time = MPI_Wtime();
  }

  // Reduce my part of the matrix 
  // will use the returned max_diff only in case of node 0
  max_diff = red_black_reduction(matrix, size, num_nodes, 
				 strip_size, my_rank, max_iters);

  if(my_rank==0) {
    // End time
    end_time = MPI_Wtime();
  }

  MPI_Finalize();

  if(my_rank==0) {
    printf("%d 0 %.3f %.5f\n", num_nodes, (end_time-start_time), max_diff);
  }
}

double red_black_reduction(double **matrix, int size, int num_nodes, 
			   int strip_size, int my_rank, int max_iters) {
  int iters;
  int i,j;
  double l_max_diff;
  int split_rows_even = (strip_size-2)%2==0?1:0;

  /* Following is only applicable for 8 nodes since only it can divide the matrix in odd numbers, 
     whereas both 2,4 certainly divide the matrix in even numbers */
  /* If rows are split even, then all nodes can do "red" followed by "black" reduction.
     If rows are split odd, then even nodes do "red" followed by "black" and odd nodes "black" followed by "red". */

  for(iters=0;iters<max_iters+1;iters++) {
    // Reduce first set of points
    for(i=1;i<strip_size-1;i++) {
      int jStart;
      if(split_rows_even) {
	jStart=(i%2==1)?1:2;
      } else {
	if(my_rank%2==0) {
	  jStart=(i%2==1)?1:2;
	} else {
	  jStart=(i%2==1)?2:1;
	}
      }

      for(j=jStart;j<size+1;j=j+2) {
	double new_value = (matrix[i-1][j]+matrix[i][j-1]+
			    matrix[i+1][j]+matrix[i][j+1])*0.25;
	if(iters==max_iters) {
	  // If last iteration, then calculate node's max diff
	  double my_diff = new_value-matrix[i][j];
	  if(my_diff > l_max_diff) {
	    l_max_diff = my_diff;
	  }
	}
	matrix[i][j]=new_value;
      }
    }

    // At this point, re-sync first and last row with neighbors before proceeding
    barrier_sync(matrix, size, num_nodes, strip_size, my_rank);

    // Reduce second set of points
    for(i=1;i<strip_size-1;i++) {
      int jStart;
      if(split_rows_even) {
	jStart=(i%2==1)?2:1;
      } else {
	if(my_rank%2==0) {
	  jStart=(i%2==1)?2:1;
	} else {
	  jStart=(i%2==1)?1:2;
	}
      }

      for(j=jStart;j<size+1;j=j+2) {
	double new_value = (matrix[i-1][j]+matrix[i][j-1]+
			    matrix[i+1][j]+matrix[i][j+1])*0.25;
	if(iters==max_iters) {
	  // If last iteration, then calculate node's max diff
	  double my_diff = new_value-matrix[i][j];
	  if(my_diff > l_max_diff) {
	    l_max_diff = my_diff;
	  }
	}
	matrix[i][j]=new_value;
      }
    }

    // At this point, re-sync first and last row with neighbors before proceeding
    barrier_sync(matrix, size, num_nodes, strip_size, my_rank);

    // Off to next iteration
  }

  if (my_rank == 0) {
    MPI_Request request[num_nodes-1];
    MPI_Status status[num_nodes-1];
    double node_max_diffs[num_nodes];

    node_max_diffs[0]=l_max_diff;
    
    for (i=1; i<num_nodes; i++) {
      MPI_Irecv(&node_max_diffs[i], 1, MPI_DOUBLE, i, TAG, MPI_COMM_WORLD, &request[i-1]);
    }
    // wait for all slaves to send their node_max_diff
    MPI_Waitall(num_nodes-1, request, status);

    // compare each node's max_diff and find actual max_diff
    double max_diff=node_max_diffs[0];
    for(i=1;i<num_nodes;i++) {
      if(node_max_diffs[i]>max_diff)
	max_diff=node_max_diffs[i];
    }
    
    return max_diff;
  } else {
    // send my node_max_diff to master
    MPI_Send(&l_max_diff, 1, MPI_DOUBLE, 0, TAG, MPI_COMM_WORLD);
    return l_max_diff;
  }
}

void barrier_sync(double **matrix, int size, int num_nodes, int strip_size, int my_rank) {
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

void divide_matrix(double **matrix, int size, int num_nodes, int strip_size) {
  // Send each node it's piece of matrix
  int i;
  int offset = strip_size-2;
  int num_of_elements = strip_size * (size+2);
  for (i=1; i<num_nodes; i++) {
    MPI_Send(matrix[offset], num_of_elements, MPI_DOUBLE, i, TAG, MPI_COMM_WORLD);
    offset += (strip_size-2);
  }
}

void wait_for_matrix(double **matrix, int size, int strip_size) {
  // Each node waits for it's piece
  MPI_Request request;
  MPI_Status status;
  int num_of_elements = strip_size * (size+2);
  MPI_Irecv(matrix[0], num_of_elements, MPI_DOUBLE, 0, TAG, MPI_COMM_WORLD, &request);
  MPI_Wait(&request, &status);
}

double** initialize_matrix(int size, int my_rank, int strip_size) {
  int i,j;
  double **matrix;
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

  return matrix;
}
