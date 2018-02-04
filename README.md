# PDPSamples
Parallel And Distributed Programming Samples - (Multithreading, MPI, Hybrid)
## Prog 1
Laplace's equation, a particular 2D partial differential equation (pde), solved using Gauss-Seidel Red/Black Successive Over-Relaxation (SOR) employing following schemes:
1. Sequential computation
2. Parallel computation using multithreading
3. Distributed computation using MPI
4. Distributed and Parallel Computation using MPI and multithreading
We measure & compare the time taken to solve the pde using each of the above strategies.
## Prog 2
Create a task graph to find out the critical path in a distributed computing system to identify the node(s) that might be slowing down the entire system.
## Prog 3
Create redundant execution nodes at each rank so that failure of one or more nodes at each rank doesn't stop the execution. Execution fails only when all nodes at a particular rank fail.
