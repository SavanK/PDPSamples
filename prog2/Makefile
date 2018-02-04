all: app

app: app.c MPI_Wrapper.c MPI_Vertex_List.h MPI_Vertex_List.c MPI_Dot_Writer.h MPI_Dot_Writer.c MPI_Task_Graph.h MPI_Task_Graph.c 
	mpicc -o2 -o app app.c MPI_Wrapper.c MPI_Vertex_List.h MPI_Vertex_List.c MPI_Dot_Writer.h MPI_Dot_Writer.c MPI_Task_Graph.h MPI_Task_Graph.c

clean:
	rm -f *.o *~
	rm app
