#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>

double** initialize_matrix(int size);
double red_black_reduction(double **matrix, int size, int max_iters);
double find_max_diff(double **matrix, int size);

int main(int argc, char *argv[]) {
  int size;
  int max_iters;
  double **matrix;
  double max_diff;
  struct timeval start_time, end_time;

  if(argc < 3) {
    printf("Too less arguments. Require matrix size and MAX_ITERS\n");
    exit(0);
  }
  else {
    size = atoi(argv[1]);
    max_iters = atoi(argv[2]);
  }

  // Initialize matrix
  matrix = initialize_matrix(size);

  // Start time
  gettimeofday(&start_time, NULL);

  // Do red-black reduction
  red_black_reduction(matrix, size, max_iters);

  // End time
  gettimeofday(&end_time, NULL);
  
  // Do one last iteration and find MAX_DIFF
  max_diff = find_max_diff(matrix, size);

  double time_duration = ((end_time.tv_sec * 1000000 + end_time.tv_usec)
  			- (start_time.tv_sec * 1000000 + start_time.tv_usec)) / 1000000.0f;

  printf("0 0 %.3f %.5f\n", time_duration, max_diff);

  free(matrix);
}

double red_black_reduction(double **matrix, int size, int max_iters) {
  int iters;
  int i,j;
  double max_diff=0;
  
  for(iters=0;iters<max_iters;iters++) {
    // reduce all the "RED" points
    for(i=1;i<size+1;i++) {
      int jStart=(i%2==1)?1:2;
      for(j=jStart;j<size+1;j=j+2) {
	matrix[i][j]=(matrix[i-1][j]+matrix[i][j-1]+
			    matrix[i+1][j]+matrix[i][j+1])*0.25;
      }
    }

    // reduce all the "BLACK" points
    for(i=1;i<size+1;i++) {
      int jStart=(i%2==1)?2:1;
      for(j=jStart;j<size+1;j=j+2) {
	matrix[i][j]=(matrix[i-1][j]+matrix[i][j-1]+
			    matrix[i+1][j]+matrix[i][j+1])*0.25;
      }
    }
    // off to next iteration
  }

  return max_diff;
}

double find_max_diff(double **matrix, int size) {
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

double** initialize_matrix(int size) {
  int i,j;
  double *tmp;

  // Allocate memory for the matrix
  tmp = (double *)malloc((size+2)*(size+2)*sizeof(double));
  double **matrix = (double **)malloc((size+2)*sizeof(double *));  
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

  return matrix;
}
