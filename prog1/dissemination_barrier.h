#include <stdio.h>
#include <stdlib.h>

int num_of_threads;
int loop_upper_limit;
volatile int *arrive;

void barrier_initialize(int n_threads);
void barrier_wait(int thread_id);
void barrier_destroy();
