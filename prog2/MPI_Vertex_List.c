#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MPI_Vertex_List.h"

Vertex* head;
Vertex* tail;
int count;

Vertex* create_vertex(int rank, int mpi_op, int pre_mpi_time, int post_mpi_time, 
		      int num_of_bytes, int tag, int receive_from, int send_to) {
  Vertex* v = (Vertex *) malloc(sizeof(Vertex));

  v->next = NULL;
  v->rank = rank;
  v->mpi_op = mpi_op;
  v->pre_mpi_time = pre_mpi_time;
  v->post_mpi_time = post_mpi_time;
  v->tag = tag;
  v->num_of_bytes = num_of_bytes;
  v->send_to = send_to;
  v->receive_from = receive_from;

  return v;
}

VertexParcelable create_vertex_parcelable(Vertex *v) {
  VertexParcelable v_parcelable;

  v_parcelable.rank = v->rank;
  v_parcelable.mpi_op = v->mpi_op;
  v_parcelable.pre_mpi_time = v->pre_mpi_time;
  v_parcelable.post_mpi_time = v->post_mpi_time;
  v_parcelable.tag = v->tag;
  v_parcelable.num_of_bytes = v->num_of_bytes;
  v_parcelable.send_to = v->send_to;
  v_parcelable.receive_from = v->receive_from;

  return v_parcelable;
}

void add_vertex_to_list(Vertex* v) {
  if(head == NULL) {
    head = v;
    tail = v;
  } else {
    tail->next = (struct Vertex*)v;
    tail = v;
  }
  count++;
}

void free_vertices() {
  free(head);
}

int get_vertex_count() {
  return count;
}

Vertex* get_vertex_list_head() {
  return head;
}

void print_vertex_parcelable(VertexParcelable *vp) {
  printf("Vertex: rank-%d mpi_op-%d start-%d end-%d tag-%d #bytes-%d to-%d from-%d\n", 
	 vp->rank, vp->mpi_op, vp->pre_mpi_time, vp->post_mpi_time, 
	 vp->tag, vp->num_of_bytes, vp->send_to, vp->receive_from);
}

void print_vertex(Vertex* v) {
  printf("Vertex: rank-%d mpi_op-%d start-%d end-%d tag-%d #bytes-%d to-%d from-%d\n", 
	 v->rank, v->mpi_op, v->pre_mpi_time, v->post_mpi_time, 
	 v->tag, v->num_of_bytes, v->send_to, v->receive_from);
}

void get_MPI_op_string(int mpi_op, char* mpi_op_str) {
  switch(mpi_op) {
  case MPI_op_Init:
    strcpy(mpi_op_str, "MPI_Init");
    break;
  case MPI_op_Finalize:
    strcpy(mpi_op_str, "MPI_Finalize");
    break;
  case MPI_op_Send:
    strcpy(mpi_op_str, "MPI_Send");
    break;
  case MPI_op_Isend:
    strcpy(mpi_op_str, "MPI_Isend");
    break;
  case MPI_op_Recv:
    strcpy(mpi_op_str, "MPI_Recv");
    break;
  case MPI_op_Irecv: 
    strcpy(mpi_op_str, "MPI_Irecv");
    break;
  case MPI_op_Wait:
    strcpy(mpi_op_str, "MPI_Wait");
    break;
  case MPI_op_Waitall:
    strcpy(mpi_op_str, "MPI_Waitall");
    break;
  case MPI_op_Barrier:
    strcpy(mpi_op_str, "MPI_Barrier");
    break;
  case MPI_op_Alltoall:
    strcpy(mpi_op_str, "MPI_Alltoall");
    break;
  case MPI_op_Scatter:
    strcpy(mpi_op_str, "MPI_Scatter");
    break;
  case MPI_op_Gather:
    strcpy(mpi_op_str, "MPI_Gather");
    break;
  case MPI_op_Reduce:
    strcpy(mpi_op_str, "MPI_Reduce");
    break;
  case MPI_op_Allreduce:
    strcpy(mpi_op_str, "MPI_Allreduce");
    break;
  }
}
