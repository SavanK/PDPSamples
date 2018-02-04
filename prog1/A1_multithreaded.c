#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "dissemination_barrier.h"

// Shared variables (b/w threads)
int size;
int max_iters;
double **matrix;
int num_of_threads;

void initialize_matrix();
void print_matrix();
void red_black_reduction(int thread_id);
void *worker(void *arg);
double find_max_diff();

int main(int argc, char *argv[]) {
  double max_diff;
  int *params;
  pthread_t *threads;
  struct timeval start_time, end_time;
  
  if(argc < 3) {
    printf("Too less arguments. Require matrix size and MAX_ITERS\n");
    exit(1);
  } else {
    size = atoi(argv[1]);
    max_iters = atoi(argv[2]);
    num_of_threads = atoi(argv[3]);
  }

  initialize_matrix();

  // Barrier initialize
  barrier_initialize(num_of_threads);
  
  // Allocate thread handles
  threads = (pthread_t *) malloc(num_of_threads*sizeof(pthread_t));
  params = (int *) malloc(num_of_threads*sizeof(int));

  // Start time
  gettimeofday(&start_time, NULL);
  
  // Create threads and run them for red-black reduction
  int i;
  for (i=0;i<num_of_threads;i++) {
    params[i]=i;
    pthread_create(&threads[i], NULL, worker, (void *)(&params[i]));
  }

  for (i=0;i<num_of_threads;i++) {
    pthread_join(threads[i], NULL);
  }
  
  // End time
  gettimeofday(&end_time, NULL);

  // Do one last iteration and find MAX_DIFF
  max_diff = find_max_diff();

  double time_duration = ((end_time.tv_sec * 1000000 + end_time.tv_usec)
			- (start_time.tv_sec * 1000000 + start_time.tv_usec)) / 1000000.0f;
  printf("0 %d %.3f %.5f\n", num_of_threads, time_duration, max_diff);

  free(matrix);
}

void *worker(void *arg) {
  int id = *((int *) arg);
  red_black_reduction(id);
  return NULL;
}

void red_black_reduction(int thread_id) {
  int iters;
  int i,j;

  // compute bounds for this thread
  int start_row = thread_id * (size/num_of_threads);
  int end_row = (thread_id+1) * (size/num_of_threads) - 1;
  
  for(iters=0;iters<max_iters;iters++) {
    // reduce all the "RED" points
    for(i=start_row+1;i<=end_row+1;i++) {
      int jStart;
      jStart=(i%2==1)?1:2;
      for(j=jStart;j<size+1;j=j+2) {
	matrix[i][j] = (matrix[i-1][j]+matrix[i][j-1]+
			matrix[i+1][j]+matrix[i][j+1])*0.25;
      }
    }

    // wait for other threads to finish reducing "RED" points
    barrier_wait(thread_id);

    // reduce all the "BLACK" points
    for(i=start_row+1;i<=end_row+1;i++) {
      int jStart;
      jStart=(i%2==1)?2:1;
      for(j=jStart;j<size+1;j=j+2) {
	matrix[i][j] = (matrix[i-1][j]+matrix[i][j-1]+
			matrix[i+1][j]+matrix[i][j+1])*0.25;
      }
    }

    // wait for other threads to finish reducing "BLACK" points
    barrier_wait(thread_id);

    // off to next iteration
  }
}

double find_max_diff() {
  double max_diff=0;
  int i,j;

  // Do one last iteration to count MAX_DIFF
  // reduce all the "RED" points
  for(i=1;i<size+1;i++) {
    int jStart;
    jStart=(i%2==1)?1:2;
    for(j=jStart;j<size+1;j=j+2) {
      double new_value = (matrix[i-1][j]+matrix[i][j-1]+
		      matrix[i+1][j]+matrix[i][j+1])*0.25;
      double my_diff = new_value-matrix[i][j];
      max_diff=my_diff>max_diff?my_diff:max_diff;
      matrix[i][j]=new_value;
      }
  }

  // reduce all the "BLACK" points
  for(i=1;i<size+1;i++) {
    int jStart;
    jStart=(i%2==1)?2:1;
    for(j=jStart;j<size+1;j=j+2) {
      double new_value = (matrix[i-1][j]+matrix[i][j-1]+
		      matrix[i+1][j]+matrix[i][j+1])*0.25;
      double my_diff = new_value-matrix[i][j];
      max_diff=my_diff>max_diff?my_diff:max_diff;
      matrix[i][j]=new_value;
    }
  }

  return max_diff;
}

void initialize_matrix() {
  int i,j;
  double *tmp;

  // Allocate memory for the matrix
  tmp = (double *)malloc((size+2)*(size+2)*sizeof(double));
  matrix = (double **)malloc((size+2)*sizeof(double *));  
  for (i=0; i<(size+2); i++)
    matrix[i] = &tmp[i*(size+2)];

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
