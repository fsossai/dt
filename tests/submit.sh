#!/bin/bash

export machine="custom"
export input_file="inputs/kronecker_20.bin"
export runs=10
export turboboost="on"
export KMP_AFFINITY="granularity=thread,balanced"
# export LD_PRELOAD=libjemalloc.so
# export note="using jemalloc, turboboost $turboboost"
export note="turboboost $turboboost"
export flags="-O3"

_sequential_programs=(
  # bfs_frontier
)

_parallel_programs=(
  bfs_omp
  # bfs_manual
  bfs_manual_opt
)

export sequential_programs=${_sequential_programs[@]}
export parallel_programs=${_parallel_programs[@]}
# export programs=${program_selection[@]}
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
  custom)
    export tspace="16"
    ;;
esac

./run.sh # for local testing
# condor_submit condor.job
