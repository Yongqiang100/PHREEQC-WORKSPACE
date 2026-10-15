# Phase 2 Task 2a: PhreeqcRM Baselines (OpenMP and MPI), from the Unmodified Worktree

## Context

Before PhreeqcRM's sources are patched (P2-T2b), we need trustworthy baselines from the **unmodified** v3.9.0 sources. Decision P2-D13 puts these in a separate worktree, `~/phreeqcrm_baseline_src`. Decision P2-D5 redefined the MPI baseline, because ctest's direct launches hang in this environment. Decision P2-D6 requires confirming that `compare_main_output_to_baseline.py` handles PhreeqcRM's `.chem.txt` and `_utility.txt` files.

This task also answers a new question: **do PhreeqcRM's results depend on the number of MPI ranks?** It matters later, because PETSc or LAPACK runs at different rank counts will be compared against these baselines.

**This task modifies no source file in any repository.** It writes only under `~/phreeqcrm_baseline_src/build-*`, `~/phreeqcrm_phase2_baseline/v390/`, and its report.

## Rules

1. Edit only `docs/petsc_linear_solver/phase2_task2a_baseline_report.md` in `~/phreeqc3`. All builds, runs and copies go under `~/phreeqcrm_baseline_src/build-openmp`, `~/phreeqcrm_baseline_src/build-mpi`, and `~/phreeqcrm_phase2_baseline/v390/`.
2. Never run `git commit`, `checkout`, `apply`, `reset`, `stash`, `clean`, or `worktree` in any repository.
3. Report every command with its full output, or with its first 40 lines plus a line count if it is longer. Never judge a difference: list it.
4. Use Plan mode first, present the plan, and execute only after the reviewer replies `APPROVED`. Keep your reasoning short before each tool call.

## Steps

0. **Pre-checks.**
   - `git -C ~/phreeqcrm_baseline_src describe --tags` must print `v3.9.0`.
   - `git -C ~/phreeqcrm_baseline_src status --short` must be empty.
   - `command -v mpiexec` must succeed.
   - Report `nproc`.

1. **OpenMP baseline.**
   - Configure and build:
     ```
     cmake -S ~/phreeqcrm_baseline_src -B ~/phreeqcrm_baseline_src/build-openmp -DCMAKE_BUILD_TYPE=Release
     cmake --build ~/phreeqcrm_baseline_src/build-openmp -j 8
     ```
   - Run 1:
     ```
     touch ~/phreeqcrm_baseline_src/build-openmp/run1_marker
     ctest --test-dir ~/phreeqcrm_baseline_src/build-openmp --output-on-failure
     ```
   - List the outputs with `find <build>/Tests -newer <build>/run1_marker -type f` (the full list), and copy them to `~/phreeqcrm_phase2_baseline/v390/openmp_run1/`, keeping the paths relative to `Tests/`.
   - Run 2: the same, with `run2_marker`, copying to `openmp_run2/`.

2. **Comparison tools on PhreeqcRM outputs (P2-D6).** For every file present in both OpenMP runs:
   - `.dmp` and `.log.txt` files: compare with `cmp`;
   - `.chem.txt` and `_utility.txt` files: compare with `python3 ~/phreeqc3/tests/petsc_linear_solver/tools/compare_main_output_to_baseline.py <run1 file> <run2 file>`;
   - any other file: compare with `cmp`.

   Report one line per file: the file, the tool used, and the result. Every comparison must come out identical or `IDENTICAL`. If any does not, show the first 10 differing lines (`diff <run1> <run2> | head -20`).

3. **Absolute paths in outputs.** Run `grep -l "/home/" ~/phreeqcrm_phase2_baseline/v390/openmp_run1/* 2>/dev/null` and report the matching files. For each file, also report the first 3 matching lines.

4. **MPI build (P2-D5).**
   ```
   cmake -S ~/phreeqcrm_baseline_src -B ~/phreeqcrm_baseline_src/build-mpi -DCMAKE_BUILD_TYPE=Release -DPHREEQCRM_BUILD_MPI=ON -DMPIEXEC_MAX_NUMPROCS=4
   cmake --build ~/phreeqcrm_baseline_src/build-mpi -j 8
   ```
   Report `MPIEXEC_EXECUTABLE`, `MPIEXEC_MAX_NUMPROCS` and `MPIEXEC_NUMPROC_FLAG` from `CMakeCache.txt`.

5. **MPI runs, each in its own copy of the test directory.** For each case below:
   1. `cp -r ~/phreeqcrm_baseline_src/build-mpi/Tests ~/phreeqcrm_baseline_src/build-mpi/<case directory>`;
   2. `touch` a marker in it;
   3. `cd` into it and run the command with `timeout 600`;
   4. report the exit status, the last 15 lines of output, and the full list of files newer than the marker;
   5. copy those files to `~/phreeqcrm_phase2_baseline/v390/<case directory>/`.

   | Case directory | Command |
   |---|---|
   | `Tests_dtor` | `mpiexec -n 1 ./TestRMdtor; mpiexec -n 1 ./TestBMIdtor` |
   | `Tests_np1` | `mpiexec -n 1 ./TestRM` |
   | `Tests_np2` | `mpiexec -n 2 ./TestRM` |
   | `Tests_np4` | `mpiexec -n 4 ./TestRM` |
   | `Tests_np2_repeat` | `mpiexec -n 2 ./TestRM` (determinism at a fixed rank count) |

   Then, in the original `build-mpi` directory, run `ctest --test-dir ~/phreeqcrm_baseline_src/build-mpi -R TestMPI --output-on-failure --timeout 600`, and report the full summary.

6. **Rank-count invariance.** Compare the outputs pairwise with the step 2 tools:
   - `Tests_np2` against `Tests_np2_repeat`: determinism;
   - `Tests_np1` against `Tests_np2`, and `Tests_np1` against `Tests_np4`: invariance;
   - `Tests_np1` against `openmp_run1`, for files present in both: MPI against OpenMP.

   Report one line per file and comparison. Do not judge the differences.

## Output

Write `docs/petsc_linear_solver/phase2_task2a_baseline_report.md` in `~/phreeqc3` with:
- steps 0–6;
- a summary table with one row per comparison set, giving the number of files identical and the number different;
- a final section, **"Reviewer decisions required"**, listing:
  - any comparison in step 2 that was not identical;
  - any absolute path found in step 3;
  - any MPI run that failed or timed out;
  - every file that differs between rank counts in step 6.

Stop after the report.
