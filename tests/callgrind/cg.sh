#!/bin/bash

cd /nfs-scratch/fsv1684/repo/dt/tests

export KMP_AFFINITY=granularity=thread,balanced

source /nfs-scratch/fsv1684/repo/dt/enable
source /nfs-scratch/fsv1684/noelle-dt/enable
source /nfs-scratch/fsv1684/gino-dt/enable

make bfs_tc FLAGS="-march=native -O3 -fno-exceptions" OUTDIR=callgrind

export OMP_NUM_THREADS=1

cd callgrind

input=../inputs/kronecker_24.bin
bin=./bfs_tc.out

echo $bin $input

valgrind \
  --tool=callgrind \
  --simulate-cache=yes \
  --callgrind-out-file=cg_${jobid}_s.out \
  $bin $input \
  > cg_${jobid}_s.txt \
  2>&1

export OMP_NUM_THREADS=8

valgrind \
  --tool=callgrind \
  --simulate-cache=yes \
  --callgrind-out-file=cg_${jobid}_p.out \
  $bin $input \
  > cg_${jobid}_p.txt \
  2>&1
