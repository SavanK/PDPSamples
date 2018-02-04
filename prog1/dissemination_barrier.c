#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "dissemination_barrier.h"

#define DEBUG_BARRIER 0

void barrier_initialize(int n_threads) {
  num_of_threads = n_threads;
  loop_upper_limit = ceil(log2((double)num_of_threads));
  arrive = (int *) malloc(num_of_threads * sizeof(int));

  int i;
  for(i=0;i<num_of_threads;i++) {
    *(arrive+i) = 0;
  }
  if(DEBUG_BARRIER) {
    printf("B:initialize, n_threads: %d, loop_upper_limit: %d\n",
	   num_of_threads, loop_upper_limit);
  }
}

void barrier_wait(int thread_id) {
  int i = thread_id;
  int j;

  for(j=1;j<=loop_upper_limit;j++) {
    while(*(arrive+i)!=0);
    *(arrive+i) = j;
    int look_at = ((int)(i + pow(2,(j-1)))) % num_of_threads;

    if(DEBUG_BARRIER) {
      printf("B:wait, round: %d thread_id: %d look_at: %d\n",
	     j, thread_id, look_at);
    }
    
    while(*(arrive+look_at)!=j);
    *(arrive+look_at) = 0;
  }
}

void barrier_destroy() {
  free(arrive);
  if(DEBUG_BARRIER) {
    printf("B:destroy\n");
  }
}
