#!/bin/bash

cd /nfs-scratch/fsv1684/repo/dt/tests

export KMP_AFFINITY=granularity=thread,balanced

source /nfs-scratch/fsv1684/repo/dt/enable
source /nfs-scratch/fsv1684/noelle-dt/enable
source /nfs-scratch/fsv1684/gino-dt/enable

export flags="-O3"
make bfs_tc FLAGS=$flags OUTDIR=callgrind
make bfs_manual FLAGS=$flags OUTDIR=callgrind

export OMP_NUM_THREADS=8

cd callgrind

input=../inputs/kronecker_24.bin
# bin=./bfs_tc.out

# echo $bin $input

valgrind \
  --tool=callgrind \
  --simulate-cache=yes \
  --callgrind-out-file=cg_${jobid}_s.out \
  ./bfs_tc.out $input \
  > cg_${jobid}_tc.txt \
  2>&1

export OMP_NUM_THREADS=8

valgrind \
  --tool=callgrind \
  --simulate-cache=yes \
  --callgrind-out-file=cg_${jobid}_p.out \
  ./bfs_manual.out $input \
  > cg_${jobid}_manual.txt \
  2>&1
