#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MPI_Vertex_List.h"

typedef struct {
  VertexParcelable *v;
  int cost;
  int rank_index;
  struct AdjListNode* next;
} AdjListNode;

typedef struct {
  int adj_node_index;
  struct TopologyStack* next;
} TopologyStack;

typedef struct {
  int index;
  struct PathElement* next;
} PathElement;

typedef struct {
  int rank;
  double time_spent;
  struct Stat* next;
} Stat;

void set_num_ranks_to_graph(int rank_count);

void add_vertices_to_graph(VertexParcelable* vertices, int size, int rank);

void construct_graph();

void find_critical_path();

void print_graph();

void dump_graph_to_dot();

void pop_up_graph();

void dump_crit_path();

void dump_stats();

void free_graph();
