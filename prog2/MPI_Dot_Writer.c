#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MPI_Dot_Writer.h"

FILE *fp_graph = NULL;
char buf_graph[100000];

FILE *fp_crit_path = NULL;
char buf_crit_path[100000];

FILE *fp_stats = NULL;
char buf_stats[100000];

void append_to_graph(char* new_buf) {
  if(!fp_graph) {
    fp_graph = fopen("task_graph.dot", "wb+");
  }

  strcat(buf_graph, new_buf);
}

void flush_graph_to_disk() {
  fwrite(buf_graph, 1, strlen(buf_graph), fp_graph);
  fclose(fp_graph);
}

void append_to_crit_path(char *new_buf) {
  if(!fp_crit_path) {
    fp_crit_path = fopen("critPath.out", "wb+");
  }

  strcat(buf_crit_path, new_buf);
}

void flush_crit_path_to_disk() {
  fwrite(buf_crit_path, 1, strlen(buf_crit_path), fp_crit_path);
  fclose(fp_crit_path);
}

void append_to_stats(char *new_buf) {
  if(!fp_stats) {
    fp_stats = fopen("stats.dat", "wb+");
  }

  strcat(buf_stats, new_buf);  
}

void flush_stats_to_disk() {
  fwrite(buf_stats, 1, strlen(buf_stats), fp_stats);
  fclose(fp_stats);
}

void get_color_for_sub_graph(int rank, char *color) {
  switch(rank) {
  case COLOR_LIGHT_GREY:
    strcpy(color, "lightgrey");
    return;

  case COLOR_GREEN:
    strcpy(color, "green");
    return;

  case COLOR_BLUE:
    strcpy(color, "blue");
    return;

  case COLOR_PINK:
    strcpy(color, "pink");
    return;

  case COLOR_YELLOW:
    strcpy(color, "yellow");
    return;

  case COLOR_BROWN:
    strcpy(color, "brown");
    return;

  case COLOR_VIOLET:
    strcpy(color, "violet");
    return;

  case COLOR_ORANGE:
    strcpy(color, "orange");
    return;
  }
}
