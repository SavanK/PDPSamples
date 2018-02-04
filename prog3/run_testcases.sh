#!/bin/bash
FILES=./testcases/*
for f in $FILES
do
  echo "Running test case: $f";
  rm app.c;
  make clean;
  cp $f ./app.c;
  make app;
  mpirun -np 8 app;
done

FILES1=./prog3_testcases/*
for f in $FILES1
do
  echo "Running test case: $f";
  rm app.c;
  make clean;
  cp $f ./app.c;
  make app;
  mpirun -np 8 app;
done

FILES2=./two_rank_testcases/*
for f in $FILES2
do
  echo "Running test case: $f";
  rm app.c;
  make clean;
  cp $f ./app.c;
  make app;
  mpirun -np 4 app;
done
