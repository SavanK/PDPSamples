#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MPI_Task_Graph.h"

int num_ranks;
VertexParcelable **rank_vertices;
int *rank_vertices_size;
int *rank_collective_index;
int *rank_cross_sweep_start_index;
int *rank_fan_in_index;
int *rank_fan_in_cost;

AdjListNode *graph;
int unique_num_nodes;
int graph_curr_insertion_pos;
int graph_curr_sweep_start_index;
AdjListNode *curr_collective_node;

int *visited;
int *distance;
TopologyStack *topo_stack;
PathElement *paths;

void set_num_ranks_to_graph(int rank_count) {
  num_ranks = rank_count;
  rank_vertices = (VertexParcelable **)malloc(rank_count*sizeof(VertexParcelable *));
  rank_vertices_size = (int *)malloc(rank_count*sizeof(int));
  rank_collective_index = (int *)malloc(rank_count*sizeof(int));
  rank_cross_sweep_start_index = (int *)malloc(rank_count*sizeof(int));
  rank_fan_in_index = (int *)malloc(rank_count*sizeof(int));
  rank_fan_in_cost = (int *)malloc(rank_count*sizeof(int));
}

void add_vertices_to_graph(VertexParcelable *vertices, int size, int rank) {
  rank_vertices[rank] = vertices;
  rank_vertices_size[rank] = size;
  rank_collective_index[rank] = 0;
  rank_cross_sweep_start_index[rank] = 0;
  rank_fan_in_index[rank] = 0;
  rank_fan_in_cost[rank] = 0;
}

int is_collective_op(VertexParcelable *v) {
  char mpi_op_str[50];
  get_MPI_op_string(v->mpi_op, mpi_op_str);
  return (v->mpi_op == MPI_op_Init || v->mpi_op == MPI_op_Finalize || 
	  v->mpi_op == MPI_op_Barrier || v->mpi_op == MPI_op_Alltoall || 
	  v->mpi_op == MPI_op_Scatter || v->mpi_op == MPI_op_Gather || 
	  v->mpi_op == MPI_op_Reduce || v->mpi_op == MPI_op_Allreduce) ? 1:0;
}

int is_collective_op_i(int mpi_op) {
  return (mpi_op == MPI_op_Init || mpi_op == MPI_op_Finalize || 
	  mpi_op == MPI_op_Barrier || mpi_op == MPI_op_Alltoall || 
	  mpi_op == MPI_op_Scatter || mpi_op == MPI_op_Gather || 
	  mpi_op == MPI_op_Reduce || mpi_op == MPI_op_Allreduce) ? 1:0;
}

int is_blocking_op(VertexParcelable *v) {
  char mpi_op_str[50];
  get_MPI_op_string(v->mpi_op, mpi_op_str);
  return (v->mpi_op == MPI_op_Recv || v->mpi_op == MPI_op_Wait) ? 1:0;
}

void initialize_graph() {
  unique_num_nodes = 0;
  int num_collective_ops=0;
  int i;
  // count number of collective operations by iterating over Rank 0's vertices
  for(i=0;i<rank_vertices_size[0];i++) {
    if(is_collective_op(&rank_vertices[0][i])) {
      num_collective_ops++;
    }
  }

  for(i=0;i<num_ranks;i++) {
    if(i==0) {
      unique_num_nodes += rank_vertices_size[i];
    } else {
      unique_num_nodes += rank_vertices_size[i]-num_collective_ops;
    }
  }

  graph = (AdjListNode *)malloc((unique_num_nodes)*sizeof(AdjListNode));
  visited = (int *)malloc(unique_num_nodes*sizeof(int));
  distance = (int *)malloc(unique_num_nodes*sizeof(int));
  paths = (PathElement *)malloc(unique_num_nodes*sizeof(PathElement));

  for(i=0;i<unique_num_nodes;i++) {
    visited[i]=0;
    distance[i]=-1;
    paths->index = -1;
    paths->next = NULL;
  }

  graph_curr_insertion_pos = 0;
  curr_collective_node = NULL;
  graph_curr_sweep_start_index = 0;
}

AdjListNode* create_node(VertexParcelable *v, int cost, int rank_index) {
  AdjListNode *node = (AdjListNode *)malloc(1*sizeof(AdjListNode));
  node->v = v;
  node->rank_index = rank_index;
  node->cost = cost;
  node->next = NULL;
  return node;
}

void linear_sweep(int rank) {
  int i=rank_collective_index[rank]+1;

  // add fan-in edges 
  if(i-1 > 0) {
    // if not first collective, i.e., !MPI_Init
    AdjListNode *tail = &graph[rank_fan_in_index[rank]];
    int cost = rank_fan_in_cost[rank];
    while(tail->next != NULL) {
      tail = (AdjListNode *)tail->next;
    }
    AdjListNode *node = create_node((VertexParcelable *)curr_collective_node->v, 
				    cost, curr_collective_node->rank_index);
    tail->next = (struct AdjListNode *)node;
  }
    
  // until you encounter next collective do -
  int first_node_skipped = 0;
  int i_before = i;
  while(!is_collective_op(&rank_vertices[rank][i])) {
    // insert nodes
    graph[graph_curr_insertion_pos].v = &rank_vertices[rank][i];
    graph[graph_curr_insertion_pos].rank_index = i;
    graph[graph_curr_insertion_pos].cost = 0;
    graph[graph_curr_insertion_pos].next = NULL;

    if(first_node_skipped) {
      // add linear edge
      int cost = (rank_vertices[rank][i].post_mpi_time - 
		  rank_vertices[rank][i].pre_mpi_time);
      AdjListNode *node = create_node(&rank_vertices[rank][i], cost, i);
      graph[graph_curr_insertion_pos-1].next = (struct AdjListNode *)node;
    } else {
      first_node_skipped = 1;
    }

    i++;
    graph_curr_insertion_pos++;
  }

  if(i_before == i) {
    // moving from one collective directly to another collective
    // don't add fan-out edges, fan-ins should take care of it
  } else {
    // add fan-out edges
    AdjListNode *tail = curr_collective_node;
    while(tail->next != NULL) {
      tail = (AdjListNode *)tail->next;
    }
    int cost = (rank_vertices[rank][rank_collective_index[rank]+1].post_mpi_time - 
		rank_vertices[rank][rank_collective_index[rank]+1].pre_mpi_time);
    AdjListNode *node = create_node(&rank_vertices[rank][rank_collective_index[rank]+1], 
				    cost, rank_collective_index[rank]+1);
    tail->next = (struct AdjListNode *)node;
  }

  rank_fan_in_cost[rank] = (rank_vertices[rank][i].post_mpi_time - 
			    rank_vertices[rank][i].pre_mpi_time);
  rank_collective_index[rank] = i;
  rank_fan_in_index[rank] = graph_curr_insertion_pos-1;
}

void cross_sweep(int rank) {
  // MPI_Send (&) MPI_Isend are non-blocking
  // MPI_Recv (&) MPI_Wait are blocking
  // So when you encounter MPI_Recv (or) MPI_Wait, 
  // find matching MPI_Send (or) MPI_Isend in the corresponding rank
  int i=rank_cross_sweep_start_index[rank];

  int rank_send_index[num_ranks];
  int k;
  for(k=0;k<num_ranks;k++) {
    rank_send_index[k] = graph_curr_sweep_start_index;
  }

  while(i<rank_collective_index[rank]) {
    VertexParcelable *v = &rank_vertices[rank][i];
    if(is_blocking_op(v)) {
      // Find mathcing MPI_Send (or) MPI_Isend
      int sender_rank = v->receive_from;
      int j=rank_send_index[sender_rank];
      while(j<graph_curr_insertion_pos) {
	VertexParcelable *sender_v = graph[j].v;
        if((sender_v->mpi_op==MPI_op_Send || sender_v->mpi_op==MPI_op_Isend) 
	   && sender_v->send_to == rank && sender_v->tag == v->tag && sender_v->rank == sender_rank) {
	  // found a matching send op
	  // add cross edge
	  AdjListNode *tail = &graph[j];
	  while(tail->next != NULL) {
	    tail = (AdjListNode *)tail->next;
	  }
	  int cost = get_time_to_send(v->num_of_bytes);
	  AdjListNode *node = create_node(v, cost, i);
	  tail->next = (struct AdjListNode *)node;
	  break;
	}
	j++;
      }
      rank_send_index[sender_rank] = j+1;
    }
    i++;
  }
}

void construct_graph() {
  initialize_graph();

  int collective_count=0;

  while(rank_collective_index[0] < rank_vertices_size[0]-1) {
    // add collective node (single instance)
    graph[graph_curr_insertion_pos].v = &rank_vertices[0][rank_collective_index[0]];
    graph[graph_curr_insertion_pos].v->rank = 9999;
    graph[graph_curr_insertion_pos].rank_index = collective_count;
    graph[graph_curr_insertion_pos].next = NULL;
    curr_collective_node = &graph[graph_curr_insertion_pos];
    graph_curr_insertion_pos++;

    graph_curr_sweep_start_index = graph_curr_insertion_pos;

    // do linear sweep to add nodes & linear edges to graph
    int i;
    for(i=0;i<num_ranks;i++) {
      // before collective index moves to next barrier by end of linear sweep,
      // initialize cross sweep start index
      rank_cross_sweep_start_index[i] = rank_collective_index[i];
      linear_sweep(i);
    }

    // do cross sweep to add cross edges to graph
    for(i=0;i<num_ranks;i++) {
      cross_sweep(i);
    }

    collective_count++;
  }

  // add finalize node in the end (single instance)
  graph[graph_curr_insertion_pos].v = &rank_vertices[0][rank_collective_index[0]];
  graph[graph_curr_insertion_pos].v->rank = 9999;
  graph[graph_curr_insertion_pos].rank_index = collective_count;
  graph[graph_curr_insertion_pos].next = NULL;
  curr_collective_node = &graph[graph_curr_insertion_pos];
  graph_curr_insertion_pos++;

  // add fan-in edges for finalize node
  int i;
  for(i=0;i<num_ranks;i++) {
    AdjListNode *tail = &graph[rank_fan_in_index[i]];
    if(tail != NULL) {
      int cost = rank_fan_in_cost[i];
      while(tail->next != NULL) {
	tail = (AdjListNode *)tail->next;
      }
      AdjListNode *node = create_node((VertexParcelable *)curr_collective_node->v, 
				      cost, curr_collective_node->rank_index);
      tail->next = (struct AdjListNode *)node;
    }
  }
}

void print_graph() {
  int i;
  for(i=0;i<unique_num_nodes;i++) {
    AdjListNode *node = &graph[i];
    if(node != NULL) {
      char mpi_op_str[50];
      get_MPI_op_string(node->v->mpi_op, mpi_op_str);
      printf("Node:%d %s_%d_%d\n", i, mpi_op_str, node->v->rank, node->rank_index);
      node = (AdjListNode *)node->next;
      int j=0;
      while(node != NULL) {
	get_MPI_op_string(node->v->mpi_op, mpi_op_str);
	printf("\tNode:%d %s_%d_%d\n", j, mpi_op_str, node->v->rank, node->rank_index);
	node = (AdjListNode *)node->next;
	j++;
      }
    }
  }
}

int is_on_critical_path(int i1, int i2) {
  PathElement *critical_path = &paths[unique_num_nodes-1];  
  int on_critical_path = 0;
  
  PathElement *curr = critical_path;
  PathElement *next = (PathElement *)critical_path->next;
  while(next != NULL) {
    if(curr->index == i1 && next->index == i2) {
      on_critical_path = 1;
      break;
    }
    curr = next;
    next = (PathElement *)next->next;
  }

  return on_critical_path;
}

void dump_graph_to_dot() {
  /*header*/
  char buf[1000];
  append_to_graph("digraph G {\n");
  
  /*do a linear sweep*/
  int i;
  for(i=0;i<num_ranks;i++) {
    /*sub-graphs according to ranks*/
    sprintf(buf, "\tsubgraph cluster_%d {\n", i);
    append_to_graph(buf);
    char color[100];
    get_color_for_sub_graph(i, color);
    sprintf(buf, "\t\tstyle=filled;\n\t\tcolor=%s;\n\t\tnode [style=filled, color=white];\n", color);
    append_to_graph(buf);

    int j;
    for(j=0;j<rank_vertices_size[i];j++) {
      if(!is_collective_op(&rank_vertices[i][j])) {
	char mpi_op_str[50];
	get_MPI_op_string(rank_vertices[i][j].mpi_op, mpi_op_str);
	sprintf(buf, "\t\t%s_%d_%d;\n", mpi_op_str, i, j);
	append_to_graph(buf);	
      }
    }

    for(j=0;j<unique_num_nodes;j++) {
      AdjListNode *curr = &graph[j];
      AdjListNode *forward = (AdjListNode *)curr->next;
      if(curr->v->rank == i) {
	if(forward != NULL && forward->v->rank == i) {
	  char mpi_op_str_1[50];
	  char mpi_op_str_2[50];
	  get_MPI_op_string(curr->v->mpi_op, mpi_op_str_1);
	  get_MPI_op_string(forward->v->mpi_op, mpi_op_str_2);
	  int forward_index = get_node_index_in_graph(forward);
	  if(is_on_critical_path(j, forward_index)) {
	    sprintf(buf, "\t\t%s_%d_%d->%s_%d_%d [xlabel=\"%d\" color=red]\n", 
		    mpi_op_str_1, i, curr->rank_index, 
		    mpi_op_str_2, i, forward->rank_index, 
		    forward->cost);
	  } else {
	    sprintf(buf, "\t\t%s_%d_%d->%s_%d_%d [xlabel=\"%d\"]\n", 
		    mpi_op_str_1, i, curr->rank_index, 
		    mpi_op_str_2, i, forward->rank_index, 
		    forward->cost);
	  }
    	  append_to_graph(buf);
	}
      }
    }

    sprintf(buf, "\t\tlabel = \"Rank %d\";\n", i);
    append_to_graph(buf);
    append_to_graph("\t}\n");
  }

  /*do a cross sweep*/
  for(i=0;i<num_ranks;i++) {    
    int j;
    for(j=0;j<unique_num_nodes;j++) {
      AdjListNode *curr = &graph[j];
      AdjListNode *forward = (AdjListNode *)curr->next;
      if(curr->v->rank == i) {
	while(forward != NULL) {
	  if(forward->v->rank != curr->v->rank) {
	    char mpi_op_str_1[50];
	    char mpi_op_str_2[50];
	    get_MPI_op_string(curr->v->mpi_op, mpi_op_str_1);
	    get_MPI_op_string(forward->v->mpi_op, mpi_op_str_2);
	    if(forward->v->rank != 9999) {
	      int forward_index = get_node_index_in_graph(forward);
	      if(is_on_critical_path(j, forward_index)) {
		sprintf(buf, "\t%s_%d_%d->%s_%d_%d [xlabel=\"%d (%d)\" color=red]\n", 
			mpi_op_str_1, i, curr->rank_index, 
			mpi_op_str_2, forward->v->rank, forward->rank_index, 
			forward->cost, forward->v->num_of_bytes);
	      } else {
		sprintf(buf, "\t%s_%d_%d->%s_%d_%d [xlabel=\"%d (%d)\"]\n", 
			mpi_op_str_1, i, curr->rank_index, 
			mpi_op_str_2, forward->v->rank, forward->rank_index, 
			forward->cost, forward->v->num_of_bytes);
	      }
	    } else {
	      int forward_index = get_node_index_in_graph(forward);
	      if(is_on_critical_path(j, forward_index)) {
		sprintf(buf, "\t%s_%d_%d->%s_%d_%d [xlabel=\"%d\" color=red]\n", 
			mpi_op_str_1, i, curr->rank_index, 
			mpi_op_str_2, forward->v->rank, forward->rank_index, 
			forward->cost);
	      } else {
		sprintf(buf, "\t%s_%d_%d->%s_%d_%d [xlabel=\"%d\"]\n", 
			mpi_op_str_1, i, curr->rank_index, 
			mpi_op_str_2, forward->v->rank, forward->rank_index, 
			forward->cost);
	      }
	    }
	    append_to_graph(buf);
	  }

	  forward = (AdjListNode *)forward->next;
	}
      } else if(curr->v->rank == 9999) {
	// a collective, draw fan out edges
	while(forward != NULL) {
	  if(forward->v->rank == i) {
	    char mpi_op_str_1[50];
	    char mpi_op_str_2[50];
	    get_MPI_op_string(curr->v->mpi_op, mpi_op_str_1);
	    get_MPI_op_string(forward->v->mpi_op, mpi_op_str_2);
	      int forward_index = get_node_index_in_graph(forward);
	      if(is_on_critical_path(j, forward_index)) {
		sprintf(buf, "\t%s_%d_%d->%s_%d_%d [xlabel=\"%d\" color=red]\n", 
			mpi_op_str_1, 9999, curr->rank_index, 
			mpi_op_str_2, forward->v->rank, forward->rank_index, 
			forward->cost);
	      } else {
		sprintf(buf, "\t%s_%d_%d->%s_%d_%d [xlabel=\"%d\"]\n", 
			mpi_op_str_1, 9999, curr->rank_index, 
			mpi_op_str_2, forward->v->rank, forward->rank_index, 
			forward->cost);
	      }
	    append_to_graph(buf);
	  }

	  forward = (AdjListNode *)forward->next;
	}
      }
    }
  }

  /**do a collective sweep**/
  // To draw collective to collective direct edges
  for(i=0;i<unique_num_nodes;i++) {
    AdjListNode *curr = &graph[i];
    AdjListNode *forward = (AdjListNode *)curr->next;
    
    if(curr->v->rank == 9999) {
      int max_edge_index = 0;
      int max_edge_cost = 0;
      AdjListNode *tmp = forward;
      
      int j=0;
      while(forward != NULL) {
	if(forward->v->rank == 9999) {
	  int forward_index = get_node_index_in_graph(forward);
	  if(is_on_critical_path(i, forward_index)) {
	    if(max_edge_cost < forward->cost) {
	      max_edge_cost = forward->cost;
	      max_edge_index = j;
	    }
	  }
	}

	forward = (AdjListNode *)forward->next;
	j++;
      }

      j=0;
      forward = tmp;
      while(forward != NULL) {
	if(forward->v->rank == 9999) {
	  char mpi_op_str_1[50];
	  char mpi_op_str_2[50];
	  get_MPI_op_string(curr->v->mpi_op, mpi_op_str_1);
	  get_MPI_op_string(forward->v->mpi_op, mpi_op_str_2);
	  int forward_index = get_node_index_in_graph(forward);
	  if(j == max_edge_index) {
	    sprintf(buf, "\t%s_%d_%d->%s_%d_%d [xlabel=\"%d\" color=red]\n", 
		    mpi_op_str_1, 9999, curr->rank_index, 
		    mpi_op_str_2, 9999, forward->rank_index, 
		    forward->cost);
	  } else {
	    sprintf(buf, "\t%s_%d_%d->%s_%d_%d [xlabel=\"%d\"]\n", 
		    mpi_op_str_1, 9999, curr->rank_index, 
		    mpi_op_str_2, 9999, forward->rank_index, 
		    forward->cost);
	  }
	  append_to_graph(buf);
	}

	forward = (AdjListNode *)forward->next;
	j++;
      }
    }
  }

  /**footer**/
  append_to_graph("\n}\n");

  /**write to disk**/
  flush_graph_to_disk();
}

void pop_up_graph() {
  system("dot -Tpng -o task_graph.png task_graph.dot && eog task_graph.png");
}

int get_time_to_send(int num_bytes) {
  // TODO apply linear regression formula here
  return (int)(1.223e-7*num_bytes+9.782e-4);
}

void free_graph() {
  int i;
  for(i=0;i<num_ranks;i++) {
    free(rank_vertices[i]);
  }
  free(rank_vertices_size);
  free(rank_collective_index);
  free(rank_fan_in_index);
  free(graph);
  free(curr_collective_node);
}

int get_node_index_in_graph(AdjListNode *node) {
  int index = -1;
  int i;
  for(i=0;i<unique_num_nodes;i++) {
    if(graph[i].rank_index == node->rank_index && 
       graph[i].v->rank == node->v->rank &&
       graph[i].v->mpi_op == node->v->mpi_op &&
       graph[i].v->tag == node->v->tag && 
       graph[i].v->num_of_bytes == node->v->num_of_bytes &&
       graph[i].v->send_to == node->v->send_to &&
       graph[i].v->receive_from == node->v->receive_from) {
      index = i;
      break;
    }	
  }

  return index;
}

int are_nodes_equal(AdjListNode *n1, AdjListNode *n2) {
  int equal = 0;
  if(n1->rank_index == n2->rank_index && 
     n1->v->rank == n2->v->rank &&
     n1->v->mpi_op == n2->v->mpi_op &&
     n1->v->tag == n2->v->tag && 
     n1->v->num_of_bytes == n2->v->num_of_bytes &&
     n1->v->send_to == n2->v->send_to &&
     n1->v->receive_from == n2->v->receive_from) {
    equal = 1;
  }

  return equal;
}

void dump_crit_path() {
  PathElement *critical_path = &paths[unique_num_nodes-1];
  int on_critical_path = 0;
  
  append_to_crit_path("MPI_Init -1\n");

  PathElement *curr = critical_path;
  PathElement *next = (PathElement *)critical_path->next;

  while(next != NULL) {
    AdjListNode *curr_node = &graph[curr->index]; 
    AdjListNode *next_node = &graph[next->index];

    // finding cost
    AdjListNode *tail = (AdjListNode *)curr_node->next;
    AdjListNode *max_collective_edge = NULL;
    while(tail != NULL) {
      if(are_nodes_equal(next_node, tail)) {
	if((curr_node->v->mpi_op == MPI_op_Send || curr_node->v->mpi_op == MPI_op_Isend) && 
	   (tail->v->mpi_op == MPI_op_Irecv || tail->v->mpi_op == MPI_op_Wait) && 
	   curr_node->v->rank != tail->v->rank) {
	  // cross edge
	  char buf[100];
	  sprintf(buf, "%d\n", tail->v->num_of_bytes);
	  append_to_crit_path(buf);
	  char mpi_op_str[50];
	  get_MPI_op_string(tail->v->mpi_op, mpi_op_str);
	  sprintf(buf, "%s %d\n", mpi_op_str, tail->v->rank);
	  append_to_crit_path(buf);
	} else if(is_collective_op(curr_node->v) && is_collective_op(next_node->v)) {
	  // edge between two collectives
	  // find the edge with max. cost
	  if(max_collective_edge == NULL) {
	    max_collective_edge = tail;
	  } else if(max_collective_edge->cost < tail->cost) {
	    max_collective_edge = tail;
	  }
	} else {
	  // linear edge
	  char buf[100];
	  sprintf(buf, "%d\n", tail->cost);
	  append_to_crit_path(buf);
	  char mpi_op_str[50];
	  get_MPI_op_string(tail->v->mpi_op, mpi_op_str);
	  sprintf(buf, "%s %d\n", mpi_op_str, (tail->v->rank==9999?-1:tail->v->rank));
	  append_to_crit_path(buf);
	}
	if(is_collective_op(curr_node->v) && is_collective_op(next_node->v)) {
	  // do no break
	} else {
	  break;
	}
      }
      tail = (AdjListNode *)tail->next;
    }

    if(max_collective_edge != NULL) {
      // collective to collective direct edge
      char buf[100];
      sprintf(buf, "%d\n", max_collective_edge->cost);
      append_to_crit_path(buf);
      char mpi_op_str[50];
      get_MPI_op_string(max_collective_edge->v->mpi_op, mpi_op_str);
      sprintf(buf, "%s %d\n", mpi_op_str, -1/*max_collective_edge->v->rank*/);
      append_to_crit_path(buf);
    }

    curr = next;
    next = (PathElement *)next->next;
  }

  flush_crit_path_to_disk();
}

void dump_stats_internal(Stat *stat, int mpi_op) {
  int invocations_on_r0 = 0;
  int total_invocations = 0;
  double mean_time = 0;
  double sum_time;
  double min_time = -1;
  double median_time = 0;
  double max_time = 0;

  if(stat != NULL) {
    Stat *tail = stat;
    while(tail != NULL) {
      total_invocations++;
      sum_time=sum_time+tail->time_spent;
      if(min_time == -1 || tail->time_spent<min_time) {
	min_time=tail->time_spent;
      }
      if(tail->time_spent>max_time) {
	max_time=tail->time_spent;
      }
      if(tail->rank == 0) {
	invocations_on_r0++;
      }
      tail = (Stat *)tail->next;
    }

    if(is_collective_op_i(mpi_op)) {
      invocations_on_r0 = 1;
    }
    mean_time = sum_time/total_invocations;

    int k=total_invocations%2;
    int mid=total_invocations/2;
    tail = stat;
    while(mid>0) {
      tail = (Stat *)tail->next;
      mid--;
    }
  
    if(k%2==0) {
      if(tail->next != NULL) {
	Stat *next = (Stat *)tail->next;
	median_time = (tail->time_spent + next->time_spent)/2.0f;
	//median_time = tail->time_spent/2.0f;
      } else {
	median_time = tail->time_spent/2.0f;
      }
    } else {
      median_time = tail->time_spent/2.0f;
    }

    char buf[300];

    char mpi_op_str[50];
    get_MPI_op_string(mpi_op, mpi_op_str);
    sprintf(buf, "%s\t%d\t%f\t%f\t%f\t%f\n", mpi_op_str, invocations_on_r0, mean_time, min_time, median_time, max_time);
    append_to_stats(buf);
  }
}

void dump_stats() {
  append_to_stats("Functions\tInvocations\tMean\tMin\tMedian\tMax\n");

  int i;

  int init_present = 0;
  int send_present = 0;
  int isend_present = 0;
  int recv_present = 0;
  int irecv_present = 0;
  int wait_present = 0;
  int barrier_present = 0;
  int alltoall_present = 0;
  int scatter_present = 0;
  int gather_present = 0;
  int reduce_present = 0;
  int allreduce_present = 0;

  Stat *init_stats;
  Stat *send_stats;
  Stat *isend_stats;
  Stat *recv_stats;
  Stat *irecv_stats;
  Stat *wait_stats;
  Stat *barrier_stats;
  Stat *alltoall_stats;
  Stat *scatter_stats;
  Stat *gather_stats;
  Stat *reduce_stats;
  Stat *allreduce_stats;

  for(i=0;i<num_ranks;i++) {
    int j;
    for(j=0;j<rank_vertices_size[i];j++) {
      Stat* element;
      element = (Stat *)malloc(1*sizeof(Stat));
      element->rank = rank_vertices[i][j].rank;
      element->time_spent = rank_vertices[i][j].post_mpi_time-rank_vertices[i][j].pre_mpi_time;
      element->next = NULL;
      switch(rank_vertices[i][j].mpi_op) {
      case MPI_op_Init:
	if(init_stats != NULL) {
	  init_stats = element;
	} else {
	  Stat *tail = init_stats;
	  while(tail->next != NULL) {
	    tail = (Stat *)tail->next;
	  }
	  tail->next=(struct Stat *)element;
	}
	init_present = 1;
	break;

      case MPI_op_Send:
	if(send_stats != NULL) {
	  send_stats = element;
	} else {
	  Stat *tail = send_stats;
	  while(tail->next != NULL) {
	    tail = (Stat *)tail->next;
	  }
	  tail->next=(struct Stat *)element;
	}
	send_present = 1;
	break;

      case MPI_op_Isend:
	if(isend_stats != NULL) {
	  isend_stats = element;
	} else {
	  Stat *tail = isend_stats;
	  while(tail->next != NULL) {
	    tail = (Stat *)tail->next;
	  }
	  tail->next=(struct Stat *)element;
	}
	isend_present = 1;
	break;

      case MPI_op_Recv:
	if(recv_stats != NULL) {
	  recv_stats = element;
	} else {
	  Stat *tail = recv_stats;
	  while(tail->next != NULL) {
	    tail = (Stat *)tail->next;
	  }
	  tail->next=(struct Stat *)element;
	}
	recv_present = 1;
	break;

      case MPI_op_Irecv:
	if(irecv_stats != NULL) {
	  irecv_stats = element;
	} else {
	  Stat *tail = irecv_stats;
	  while(tail->next != NULL) {
	    tail = (Stat *)tail->next;
	  }
	  tail->next=(struct Stat *)element;
	}
	irecv_present = 1;
	break;

      case MPI_op_Wait:
	if(wait_stats != NULL) {
	  wait_stats = element;
	} else {
	  Stat *tail = wait_stats;
	  while(tail->next != NULL) {
	    tail = (Stat *)tail->next;
	  }
	  tail->next=(struct Stat *)element;
	}
	wait_present = 1;
	break;

      case MPI_op_Barrier:
	if(barrier_stats != NULL) {
	  barrier_stats = element;
	} else {
	  Stat *tail = barrier_stats;
	  while(tail->next != NULL) {
	    tail = (Stat *)tail->next;
	  }
	  tail->next=(struct Stat *)element;
	}
	barrier_present = 1;
	break;

      case MPI_op_Alltoall:
	if(alltoall_stats != NULL) {
	  alltoall_stats = element;
	} else {
	  Stat *tail = alltoall_stats;
	  while(tail->next != NULL) {
	    tail = (Stat *)tail->next;
	  }
	  tail->next=(struct Stat *)element;
	}
	alltoall_present = 1;
	break;

      case MPI_op_Scatter:
	if(scatter_stats != NULL) {
	  scatter_stats = element;
	} else {
	  Stat *tail = scatter_stats;
	  while(tail->next != NULL) {
	    tail = (Stat *)tail->next;
	  }
	  tail->next=(struct Stat *)element;
	}
	scatter_present = 1;
	break;

      case MPI_op_Gather:
	if(gather_stats != NULL) {
	  gather_stats = element;
	} else {
	  Stat *tail = gather_stats;
	  while(tail->next != NULL) {
	    tail = (Stat *)tail->next;
	  }
	  tail->next=(struct Stat *)element;
	}
	gather_present = 1;
	break;

      case MPI_op_Reduce:
	if(reduce_stats != NULL) {
	  reduce_stats = element;
	} else {
	  Stat *tail = reduce_stats;
	  while(tail->next != NULL) {
	    tail = (Stat *)tail->next;
	  }
	  tail->next=(struct Stat *)element;
	}
	reduce_present = 1;
	break;

      case MPI_op_Allreduce:
	if(allreduce_stats != NULL) {
	  allreduce_stats = element;
	} else {
	  Stat *tail = allreduce_stats;
	  while(tail->next != NULL) {
	    tail = (Stat *)tail->next;
	  }
	  tail->next=(struct Stat *)element;
	}
	allreduce_present = 1;
	break;

      default:
	break;
      }
    }    
  }

  if(init_present)
    dump_stats_internal(init_stats, MPI_op_Init);
  if(send_present)
    dump_stats_internal(send_stats, MPI_op_Send);
  if(isend_present)
    dump_stats_internal(isend_stats, MPI_op_Isend);
  if(recv_present)
    dump_stats_internal(recv_stats, MPI_op_Irecv);
  if(wait_present)
    dump_stats_internal(wait_stats, MPI_op_Wait);
  if(barrier_present)
    dump_stats_internal(barrier_stats, MPI_op_Barrier);
  if(alltoall_present)
    dump_stats_internal(alltoall_stats, MPI_op_Alltoall);
  if(scatter_present)
    dump_stats_internal(scatter_stats, MPI_op_Scatter);
  if(gather_present)
    dump_stats_internal(gather_stats, MPI_op_Gather);
  if(reduce_present)
    dump_stats_internal(reduce_stats, MPI_op_Reduce);
  if(allreduce_present)
    dump_stats_internal(allreduce_stats, MPI_op_Allreduce);

  flush_stats_to_disk();
}

void topological_sort(AdjListNode *node, int index) {
  visited[index] = 1;

  AdjListNode *neighbor = (AdjListNode *)node->next;
  while(neighbor != NULL) {
    int neighbor_index = get_node_index_in_graph(neighbor);
    if(!visited[neighbor_index]) {
      topological_sort(&graph[neighbor_index], neighbor_index);
    }
    neighbor = (AdjListNode *)neighbor->next;
  }

  TopologyStack *entry = (TopologyStack *)malloc(1*sizeof(TopologyStack));
  entry->adj_node_index = index;
  entry->next = (struct TopologyStack *)topo_stack;
  topo_stack = entry;
}

void find_critical_path() {
  int i;
  for(i=0;i<unique_num_nodes;i++) {
    if(!visited[i]) {
      topological_sort(&graph[i], i);
    }
  }

  distance[0]=0;
  paths[0].index=0;
  
  while(topo_stack != NULL) {
    TopologyStack *entry = topo_stack;
    topo_stack = (TopologyStack *)topo_stack->next;
    
    if(distance[entry->adj_node_index]!=-1) {
      AdjListNode *node = &graph[entry->adj_node_index];
      AdjListNode *neighbor = (AdjListNode *)node->next;
      while(neighbor != NULL) {
	int neighbor_index = get_node_index_in_graph(neighbor);
	if(distance[neighbor_index] < 
	   (distance[entry->adj_node_index] + neighbor->cost)) {
	  distance[neighbor_index] = 
	    distance[entry->adj_node_index] + neighbor->cost;
	  // copy same path and add your index to it
	  PathElement *tail = &paths[entry->adj_node_index];
	  PathElement *neighbor_tail = &paths[neighbor_index];
	  neighbor_tail->index = tail->index;
	  tail = (PathElement *)tail->next;

	  while(tail != NULL) {
	    if(neighbor_tail->next == NULL) {
	      PathElement *element = (PathElement *)malloc(1*sizeof(PathElement));
	      element->index = tail->index;
	      element->next = NULL;
	      neighbor_tail->next = (struct PathElement *)element;
	      neighbor_tail = element;
	    } else {
	      neighbor_tail = (PathElement *)neighbor_tail->next;
	      neighbor_tail->index = tail->index;
	    }
	    tail = (PathElement *)tail->next;
	  }

	  if(neighbor_tail->next == NULL) {
	    PathElement *element = (PathElement *)malloc(1*sizeof(PathElement));
	    element->index = neighbor_index;
	    element->next = NULL;
	    neighbor_tail->next = (struct PathElement *)element;
	    neighbor_tail = element;
	  } else {
	    neighbor_tail = (PathElement *)neighbor_tail->next;
	    neighbor_tail->index = neighbor_index;
	    neighbor_tail->next = NULL;
	  }
	}
	neighbor = (AdjListNode *)neighbor->next;
      }
    }
  }

  // This is the one problematic printf statement
  // Looks like I'm either not handling pointers right leading to memory corruption
  // or on the off chance a buffer overflow (??, not sure)
  // Removing this will cause the program to seg fault more often for a set of inputs
  printf(" \n");

}
