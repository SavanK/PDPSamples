all: app

app: app.c MPI_Wrapper.c
	mpicc -o2 -o app app.c MPI_Wrapper.c

clean:
	rm -f *.o *~
	rm app
