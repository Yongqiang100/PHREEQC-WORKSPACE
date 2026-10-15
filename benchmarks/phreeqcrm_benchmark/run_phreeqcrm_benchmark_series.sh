#!/usr/bin/env bash
# Runs the PhreeqcRM chemistry benchmark for a list of thread counts (OpenMP) or rank counts (MPI),
# several repeats each, and appends one CSV row per run (Phase 2 Task 4).
#
# usage: run_phreeqcrm_benchmark_series.sh <driver> <label> <openmp|mpi> <repeats> <nxyz> <nsteps>
#                                          <input.pqi> <database> <output_dir> <count> [<count> ...]
# The linear-solver environment variables (PHREEQC_LINEAR_SOLVER, PHREEQC_LINEAR_SOLVER_STATISTICS,
# PHREEQC_LINEAR_SOLVER_CONDITION_CHECK, PHREEQC_LINEAR_SOLVER_MIN_RCOND) are taken from the caller's
# environment and forwarded to every MPI rank with mpiexec -x NAME=value when they are set.
# Results: <output_dir>/benchmark_runs.csv and one directory per run with stdout.txt and stderr.txt.
set -u
if [ "$#" -lt 10 ]; then
    echo "usage: $0 <driver> <label> <openmp|mpi> <repeats> <nxyz> <nsteps> <input.pqi> <database> <output_dir> <count>..." >&2
    exit 2
fi
driver=$(readlink -f "$1"); label=$2; mode=$3; repeats=$4; nxyz=$5; nsteps=$6
input=$(readlink -f "$7"); database=$(readlink -f "$8"); output_dir=$9
shift 9
mkdir -p "$output_dir"; output_dir=$(readlink -f "$output_dir")
csv="$output_dir/benchmark_runs.csv"
if [ ! -f "$csv" ]; then
    echo "label,mode,count,repeat,exit_status,run_cells_seconds_total,run_cells_seconds_median,setup_seconds,initial_run_cells_seconds,load_average_1min" > "$csv"
fi
forwarded=()
for name in PHREEQC_LINEAR_SOLVER PHREEQC_LINEAR_SOLVER_STATISTICS PHREEQC_LINEAR_SOLVER_CONDITION_CHECK PHREEQC_LINEAR_SOLVER_MIN_RCOND; do
    if [ -n "${!name+x}" ]; then forwarded+=(-x "$name=${!name}"); fi
done
for count in "$@"; do
    for repeat in $(seq 1 "$repeats"); do
        run_dir="$output_dir/${label}_n${count}_r${repeat}"
        mkdir -p "$run_dir"
        load_average=$(cut -d' ' -f1 /proc/loadavg)
        if [ "$mode" = "openmp" ]; then
            ( cd "$run_dir" && OPENBLAS_NUM_THREADS=1 timeout 3600 "$driver" "$nxyz" "$count" "$nsteps" 1 \
                "$input" "$database" bench > stdout.txt 2> stderr.txt )
        elif [ "$mode" = "mpi" ]; then
            ( cd "$run_dir" && timeout 3600 mpiexec -n "$count" -x OPENBLAS_NUM_THREADS=1 "${forwarded[@]}" \
                "$driver" "$nxyz" 1 "$nsteps" 1 "$input" "$database" bench > stdout.txt 2> stderr.txt )
        else
            echo "mode must be openmp or mpi" >&2; exit 2
        fi
        status=$?
        line=$(grep "^benchmark:" "$run_dir/stdout.txt" | head -1)
        value() { echo "$line" | tr ' ' '\n' | grep "^$1=" | cut -d= -f2; }
        echo "$label,$mode,$count,$repeat,$status,$(value run_cells_seconds_total),$(value run_cells_seconds_median),$(value setup_seconds),$(value initial_run_cells_seconds),$load_average" >> "$csv"
        echo "$label $mode n=$count repeat=$repeat exit=$status $(value run_cells_seconds_total) s"
    done
done
