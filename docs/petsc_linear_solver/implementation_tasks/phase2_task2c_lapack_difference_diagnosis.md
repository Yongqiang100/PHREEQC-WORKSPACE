# Phase 2 Task 2c: Diagnose the LAPACK-versus-cl1 Differences in PhreeqcRM (read-only)

## Context

Phase 2 Task 2b ported the LAPACK backend into PhreeqcRM. With LAPACK enabled, 12 output files differ from the `cl1()` baselines, identically at every rank and thread count (decision P2-D17). Most differences are round-off-scale values near zero. One solution in `Advect_cpp_units_utility.txt` has a different redox state (`pe = -0.602` against `-0.558`). The working hypothesis is a **poorly poised redox system**, in which pe is not well determined within PHREEQC's convergence tolerance. This task tests that hypothesis with facts. **It changes no source file and no build configuration.**

All builds already exist from Task 2b: `~/phreeqcrm/build-patched-openmp-lapack` and `~/phreeqcrm/build-patched-mpi-lapack`. The case directories from Task 2b stay untouched; this task creates new ones only.

## Rules

1. Edit only `docs/petsc_linear_solver/phase2_task2c_lapack_difference_report.md` in `~/phreeqc3`. New run directories go under `~/phreeqcrm/build-patched-mpi-lapack/` only. Do not modify any existing case directory, baseline, or source file.
2. Never run `git commit`, `checkout`, `apply`, `reset`, `stash`, `clean`, or `worktree`. Never rebuild or reconfigure a build tree.
3. Report every command with its full output, or with its first 60 lines plus a line count if it is longer. Never judge a difference: state facts.
4. Use Plan mode first, present the plan, and execute only after the reviewer replies `APPROVED`. Keep your reasoning short before each tool call.

The tool is `T=~/phreeqc3/tests/petsc_linear_solver/tools/compare_phreeqcrm_chem_output.py`. The baseline directory is `B=~/phreeqcrm_phase2_baseline/v390`.

## Steps

0. **Pre-checks.**
   - `git -C ~/phreeqcrm status --short` must be empty. The reviewer commits Task 2b first.
   - `ls ~/phreeqcrm/build-patched-mpi-lapack/Tests/TestRM ~/phreeqcrm/build-patched-openmp-lapack/Tests_lapack` must succeed.
   - `python3 $T --help | head -5` must mention `--relative-tolerance`.

1. **Tolerance comparison.** For each of the 12 differing files of Task 2b (the 7 `.chem.txt` and 5 `_utility.txt` files that were not identical), run:
   ```
   python3 $T $B/openmp_run1/<file> ~/phreeqcrm/build-patched-openmp-lapack/Tests_lapack/<file> --relative-tolerance 1e-6 --absolute-tolerance 1e-12 --max-reported-differences 200
   ```
   Report the verdict line, and every differing line pair, for each file.

2. **Context of the redox difference.**
   - In both `$B/openmp_run1/Advect_cpp_units_utility.txt` and `.../Tests_lapack/Advect_cpp_units_utility.txt`, find the line numbers containing `Adjusted to redox equilibrium` (`grep -n`).
   - For the first such line where the two files differ, print lines from 40 before to 50 after it, from both files (`sed -n`).
   - Report which solution or cell number the block belongs to (the nearest preceding `Initial solution`, `Using solution`, or `Beginning of` line).
   - Then, read-only: `grep -n -i "SOLUTION\|pe \|redox\|O(0)\|H(0)\|C(-4)" ~/phreeqcrm/Tests/units.pqi`, and quote every `SOLUTION` block of that file in full (`sed -n`).

3. **Comparison-mode run (MPI, 1 rank, so no concurrent writers).**
   - Create the case: `cp -r ~/phreeqcrm/build-patched-mpi-lapack/Tests ~/phreeqcrm/build-patched-mpi-lapack/Tests_np1_compare`, then `touch` a marker in it before the run.
   - In it, run:
     ```
     timeout 600 mpiexec -n 1 -x OPENBLAS_NUM_THREADS=1 -x PHREEQC_LINEAR_SOLVER_STATISTICS=1 -x PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1=1 -x PHREEQC_LINEAR_SOLVER_COMPARE_LOG=newton_step_comparison.log ./TestRM 2> stderr.txt
     ```
     Report the exit status.
   - In comparison mode `cl1()` steers, so the outputs must match the baseline exactly. Compare every output file in the marker list with `$B/Tests_np1/<file>` using the Task 2b method (P2-D16, exact mode), one line per file.
   - Report the sums of the statistics lines, counted the same way as Task 2b step 7.

4. **Step differences.**
   - Run `python3 ~/phreeqc3/tests/petsc_linear_solver/tools/summarize_petsc_cl1_step_comparison.py newton_step_comparison.log --large-difference-threshold 1e-8` in the case directory, and report the full output.
   - Then list every record with `relative_difference > 1e-8`, sorted by `relative_difference` in descending order, showing all nine columns:
     ```
     grep -v "^#" newton_step_comparison.log | awk '$3 == 0 && $6 > 1e-8' | sort -g -k6 -r
     ```
     Show at most the first 60 rows, plus the total count.
   - Map the unknown-type codes in the last column to names using `src/global_structures.h:82-99` in `~/phreeqc3` (Task 0 report, B4): MB 10, ALK 11, CB 12, SOLUTION_PHASE_BOUNDARY 13, MU 14, AH2O 15, MH 16, MH2O 17.
   - Report the count of records per type, for `relative_difference > 1e-8` and for `relative_difference > 1e-3`.

## Output

Write `docs/petsc_linear_solver/phase2_task2c_lapack_difference_report.md` in `~/phreeqc3` with:
- steps 0–4;
- a summary stating, as facts:
  - how many lines in the 12 files exceed the tolerance of step 1, and in which files;
  - the redox and solution context of step 2;
  - whether comparison mode reproduced the baseline exactly (step 3);
  - the distribution of step differences by unknown type (step 4);
- a final section, **"Reviewer decisions required"**, listing any failure or anything not found.

Stop after the report.
