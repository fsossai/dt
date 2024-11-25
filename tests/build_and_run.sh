#!/bin/bash

if [[ -z $bin_dir ]]; then
  bin_dir="."
fi

if [[ -z $results_dir ]]; then
  results_dir="."
fi

makebm=/nfs-scratch/fsv1684/.local/bin/makebm
cd $root
mkdir -p $results_dir
mkdir -p $bin_dir
make -j FLAGS="$flags" OUTDIR=$bin_dir ${parallel_programs[@]} ${sequential_programs[@]}


# running sequential programs -------------------------------

target_core=4

if [[ $turboboost == "off" ]]; then
  cpu0_cores=$(( $(nproc --all) / 2 ))
  for ((i=0; i < $cpu0_cores; i += 2)); do
    if [[ $i -eq $target_core ]]; then
      continue
    fi
    taskset -c $i burnP6 &
  done
fi

for program in ${sequential_programs[@]}; do
  export outfile=$results_dir/s_$program.csv
  export cmd="$bin_dir/$program.out $input_file"
  tspace=1 turboboost=on taskset -c $target_core $makebm
done

if [[ $turboboost == "off" ]]; then
  killall burnP6
fi

# running parallel programs ---------------------------------

for program in ${parallel_programs[@]}; do
  export outfile=$results_dir/p_$program.csv
  export cmd="$bin_dir/$program.out $input_file"
  $makebm
done
