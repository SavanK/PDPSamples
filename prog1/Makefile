all: seq-rb mt-rb dist-rb

seq-rb: A1_sequential.c
	gcc -o2 -o seq-rb A1_sequential.c

mt-rb: A1_multithreaded.c dissemination_barrier.c dissemination_barrier.h
	gcc -o2 -pthread -lm -o mt-rb A1_multithreaded.c dissemination_barrier.c

dist-rb: A1_mpi.c
	mpicc -o2 -o dist-rb A1_mpi.c

hybrid-rb: A1_hybrid.c dissemination_barrier.c dissemination_barrier.h
	mpicc -o2 -pthread -lm -o hybrid-rb A1_hybrid.c dissemination_barrier.c

clean:
	rm -f *.o *~
	rm seq-rb mt-rb dist-rb
