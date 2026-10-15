# Phase 2 Task 4: PhreeqcRM Performance Study (measurement only)

## Context

Phase 2 is functionally complete: the LAPACK backend with the condition gate runs correctly inside PhreeqcRM under OpenMP and MPI (decisions P2-D17 to P2-D20, and Task 3). This task measures **what the integration costs or gains in PhreeqcRM's chemistry step**, on a workload heavier than PhreeqcRM's 0.3-second tests.

The workload is a reviewer-supplied chemistry-only driver, `tests/petsc_linear_solver/phreeqcrm_benchmark/phreeqcrm_chemistry_benchmark.cpp`:
- 1,000 cells, each a mixture of a basalt pore water and a CO₂-charged brine, with the mixing fraction varying from cell to cell;
- 4 equilibrium phases and 3 kinetic minerals in every cell, at 50 °C and 100 atm;
- 10 daily `RunCells` steps, and no transport.

Its reaction input is `co2_basalt_phreeqcrm.pqi` in the same directory. It prints one `benchmark:` line with the timings, and writes the final concentrations as a tab-separated table.

Expectation, stated in advance (P2-D8): in this chemistry almost every Newton call carries pure-phase unknowns, which are ineligible. So **no speed-up is expected**. The questions are:
1. Does the integration add measurable overhead?
2. Does it change how PhreeqcRM scales with threads and ranks?
3. What fraction of calls is eligible, and how many are gated, in this workload?

**No source file in either repository changes.** The task installs existing build trees, builds the driver against each installation, runs, and reports.

## Rules

1. Edit only `docs/petsc_linear_solver/phase2_task4_performance_report.md` in `~/phreeqc3`. Everything else goes under these paths:
   - `~/phreeqcrm_installs/<name>/` (installations);
   - `~/phreeqc3/build/phreeqcrm_benchmark/<name>/` (driver builds);
   - `~/phreeqc3/build/phreeqcrm_benchmark_runs/` (runs).
2. Never rebuild or reconfigure the PhreeqcRM build trees, apart from `cmake --install`. Never run `git commit`, `checkout`, `apply`, `reset`, `stash`, `clean`, or `worktree`.
3. Report measured numbers as they come out. Do not rerun selectively to change them, and do not drop runs.
4. Before every series, record `uptime` and `squeue`. If `squeue` lists any running job, or the 1-minute load average exceeds 4, stop and report rather than measure.
5. Use Plan mode first, present the plan, and execute only after the reviewer replies `APPROVED`. Keep your reasoning short before each tool call.

## Steps

0. **Pre-checks.**
   - `git status --short` (in `~/phreeqc3`) and `git -C ~/phreeqcrm status --short` must both be empty.
   - Report `nproc`, `lscpu | grep -E "Model name|^Socket|Core\(s\) per socket|NUMA node\(s\)"`, `uptime`, and `squeue`.

1. **Install five PhreeqcRM builds:** `cmake --install <tree> --prefix ~/phreeqcrm_installs/<name>`.

   | name | tree | what it is |
   |---|---|---|
   | `v390_openmp` | `~/phreeqcrm_baseline_src/build-openmp` | unmodified PhreeqcRM v3.9.0, OpenMP |
   | `patched_openmp_off` | `~/phreeqcrm/build-patched-openmp-off` | patched sources, LAPACK option OFF |
   | `patched_openmp_lapack` | `~/phreeqcrm/build-patched-openmp-lapack` | patched, LAPACK ON (gate default) |
   | `v390_mpi` | `~/phreeqcrm_baseline_src/build-mpi` | unmodified v3.9.0, MPI |
   | `patched_mpi_lapack` | `~/phreeqcrm/build-patched-mpi-lapack` | patched, MPI, LAPACK ON |

   If `build-patched-openmp-off` was built before Task 3, run `cmake --build ~/phreeqcrm/build-patched-openmp-off -j 8` first. That is allowed, because it only brings the tree to the committed sources, and the agent must report whether it rebuilt anything. Report each `--install` exit status.

2. **Build the driver once per installation:**
   ```
   cmake -S ~/phreeqc3/tests/petsc_linear_solver/phreeqcrm_benchmark -B ~/phreeqc3/build/phreeqcrm_benchmark/<name> -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$HOME/phreeqcrm_installs/<name>
   cmake --build ~/phreeqc3/build/phreeqcrm_benchmark/<name> -j 8
   ```
   For the two MPI installations, report the `Found MPI` line.

3. **Smoke run.** For each of the 5 drivers, run once with 50 cells and 2 steps (OpenMP with 2 threads; MPI with `mpiexec -n 2`), with `PHREEQC_LINEAR_SOLVER_STATISTICS=summary`. Use the database `~/phreeqc3/database/phreeqc.dat`. Report the `benchmark:` line, the exit status, and the full stderr.

4. **Timing series.** Use `bash ~/phreeqc3/tests/petsc_linear_solver/phreeqcrm_benchmark/run_phreeqcrm_benchmark_series.sh`, with 3 repeats, 1,000 cells and 10 steps. The output directory is `~/phreeqc3/build/phreeqcrm_benchmark_runs/` for every series, so that all rows go into one CSV.
   - **OpenMP,** counts `1 4 16 32`:
     - label `v390_openmp`, driver `v390_openmp`;
     - label `patched_openmp_off`, driver `patched_openmp_off`;
     - label `lapack_gate`, driver `patched_openmp_lapack`, with no linear-solver variables set;
     - label `lapack_cl1`, driver `patched_openmp_lapack`, with `PHREEQC_LINEAR_SOLVER=cl1`.
   - **MPI,** counts `1 4 16 32`:
     - label `v390_mpi`, driver `v390_mpi`;
     - label `mpi_lapack_gate`, driver `patched_mpi_lapack`;
     - label `mpi_lapack_cl1`, driver `patched_mpi_lapack`, with `PHREEQC_LINEAR_SOLVER=cl1`.

   Report each series' console output.

5. **Summary.**
   ```
   python3 ~/phreeqc3/tests/petsc_linear_solver/phreeqcrm_benchmark/summarize_benchmark_series.py ~/phreeqc3/build/phreeqcrm_benchmark_runs/benchmark_runs.csv --reference v390_openmp
   python3 ~/phreeqc3/tests/petsc_linear_solver/phreeqcrm_benchmark/summarize_benchmark_series.py ~/phreeqc3/build/phreeqcrm_benchmark_runs/benchmark_runs.csv --reference v390_mpi
   ```
   Report both outputs in full.

6. **Eligibility in this workload.**
   - Run `patched_openmp_lapack` once with 1,000 cells, 10 steps and 4 threads, and `patched_mpi_lapack` once with `mpiexec -n 4`, both with `PHREEQC_LINEAR_SOLVER_STATISTICS=summary`, in new directories under the runs directory.
   - Report the output of `python3 ~/phreeqc3/tests/petsc_linear_solver/tools/sum_linear_solver_statistics.py` on each stderr.
   - Report `eligible_fraction = solves / (solves + ineligible_systems)` and `gated_fraction = ill_conditioned / solves`.

7. **Results agree.** Compare `bench_concentrations.tsv` of repeat 1 at count 4 of every label with that of `v390_openmp` at count 1, using `python3 ~/phreeqc3/tests/petsc_linear_solver/tools/compare_selected_output_to_baseline.py <reference> <file>`. Report each verdict and any failing columns.

## Output

Write `docs/petsc_linear_solver/phase2_task4_performance_report.md` with:
- the environment (step 0);
- the installations and driver builds;
- the smoke runs;
- the full CSV (`cat` of `benchmark_runs.csv`);
- the two summary outputs;
- the eligibility numbers;
- the result comparisons;
- **"Observations"**, stating facts only: ratios against the reference at every count, speed-ups, run-to-run spread, and whether any ratio lies outside the run-to-run spread.

Stop after the report.
