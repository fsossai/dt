#!/bin/bash

export machine="local"
export input_file="inputs/kronecker_24.bin"
export runs=5
export turboboost="on"
export affinity="y"
export KMP_AFFINITY="granularity=thread,balanced"
export LD_PRELOAD=libjemalloc.so
export note="turboboost $turboboost, LD_PRELOAD=$LD_PRELOAD"
export flags="-march=native -O3 -fno-exceptions"
export winchester_flags=""

source /nfs-scratch/fsv1684/repo/dt/enable
source /nfs-scratch/fsv1684/noelle-dt/enable
source /nfs-scratch/fsv1684/gino-dt/enable

_sequential_programs=(
  # bfs_frontier
)

_parallel_programs=(
  # bfs_omp
  # bfs_pthreads
  bfs_tc
  # bfs_tc_outlined
  # bfs_manual
  # bfs_manual_opt
)

export sequential_programs=${_sequential_programs[@]}
export parallel_programs=${_parallel_programs[@]}
export root=$(realpath .)

case $machine in
  allagash)
    export tspace="1 2 4 8 12 14 20 24 28"
    ;;
  boucanier)
    export tspace="1 2 4 6 8 10 12"
    ;;
  fix)
    export tspace="1 2 7 14 21 28 35 42 49 56"
    ;;
  piraat)
    export tspace="1 2 4 8 12 16 20 24"
    ;;
  tremens | maudite | guldendraak)
    export tspace="1 2 4 6 8"
    ;;
  local)
    export tspace="14"
    ;;
esac

if [[ $machine == "local" ]]; then
  ./build_and_run.sh
else
  condor_submit condor.job
fi
