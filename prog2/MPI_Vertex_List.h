#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define MPI_op_Init 1
#define MPI_op_Finalize 2
#define MPI_op_Send 3
#define MPI_op_Isend 4
#define MPI_op_Recv 5
#define MPI_op_Irecv 6
#define MPI_op_Wait 7
#define MPI_op_Waitall 8
#define MPI_op_Barrier 9
#define MPI_op_Alltoall 10
#define MPI_op_Scatter 11
#define MPI_op_Gather 12
#define MPI_op_Reduce 13
#define MPI_op_Allreduce 14

typedef struct {
  int rank;
  int mpi_op;
  int pre_mpi_time, post_mpi_time;
  int tag;
  int num_of_bytes;
  int send_to;
  int receive_from;
  struct Vertex* next;
} Vertex;

typedef struct {
  int rank;
  int mpi_op;
  int pre_mpi_time, post_mpi_time;
  int tag;
  int num_of_bytes;
  int send_to;
  int receive_from;
} VertexParcelable;

Vertex* create_vertex(int rank, int mpi_op, int pre_mpi_time, int post_mpi_time, 
		      int tag, int num_of_bytes, int send_to, int receive_from);

VertexParcelable create_vertex_parcelable(Vertex *v);

void add_vertex_to_list(Vertex* v);

void free_vertices();

int get_vertex_count();

Vertex* get_vertex_list_head();

void print_vertex_parcelable(VertexParcelable *vp);

void print_vertex(Vertex* v);

void get_MPI_op_string(int mpi_op, char* mpi_op_str);
