#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COLOR_YELLOW 0
#define COLOR_GREEN 1
#define COLOR_BLUE 2
#define COLOR_PINK 3
#define COLOR_LIGHT_GREY 4
#define COLOR_BROWN 5
#define COLOR_VIOLET 6
#define COLOR_ORANGE 7

void append_to_graph(char* new_buf);

void get_color_for_sub_graph(int rank, char *color);

void flush_graph_to__disk();

void append_to_crit_path(char *new_buf);

void flush_crit_path_to_disk();

void append_to_stats(char *new_buf);

void flush_stats_to_disk();
