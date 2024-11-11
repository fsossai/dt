#!/bin/bash

cd /nfs-scratch/fsv1684/repo/dt/tests

export KMP_AFFINITY=granularity=thread,balanced

source /nfs-scratch/fsv1684/noelle-dt/enable

make bfs_omp FLAGS="-march=native -O3 -fno-exceptions" OUTDIR=callgrind

export OMP_NUM_THREADS=1

cd callgrind

input=../inputs/kronecker_24.bin
bin=./bfs_omp.out

valgrind \
  --tool=callgrind \
  --simulate-cache=yes \
  $bin $input \
  > cg_s.txt \
  2>&1

export OMP_NUM_THREADS=8

valgrind \
  --tool=callgrind \
  --simulate-cache=yes \
  $bin $input \
  > cg_p.txt \
  2>&1
