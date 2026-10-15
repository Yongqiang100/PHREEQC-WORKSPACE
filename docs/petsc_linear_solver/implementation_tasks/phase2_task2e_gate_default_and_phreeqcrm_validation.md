# Phase 2 Task 2e: Condition Gate as the LAPACK Default; Validation in phreeqc3 and PhreeqcRM

## Context

Decision P2-D19 sets the threshold `PHREEQC_LINEAR_SOLVER_MIN_RCOND = 1e-8`, and makes gate mode the default for the LAPACK backend. This task:
1. changes those defaults in phreeqc3, with tests;
2. validates LAPACK in **steering** mode against `cl1()` runs, on ex1–ex8, `units.pqi` and the CO₂–basalt input;
3. copies the changed adapter files into PhreeqcRM, rebuilds, and repeats the Task 2b validation. That includes a log-mode comparison run, which records the reciprocal condition of every PhreeqcRM step.

The acceptance criterion is P2-D19 (e): numeric tolerance (relative 10⁻⁶, absolute 10⁻¹²) for main and chemistry outputs, and `compare_selected_output_to_baseline.py` for `.sel` files.

## Rules

1. Modify only:
   - in `~/phreeqc3`: `src/petsc_linear_solver/newton_linear_solver_options.cpp` and `tests/petsc_linear_solver/test_newton_linear_solver_options.cpp`;
   - in `~/phreeqcrm`: the files under `src/IPhreeqcPhast/IPhreeqc/phreeqcpp/newton_linear_solver/`, by copying only.

   No PHREEQC source, no CMake file, and nothing in `~/phreeqcrm/Tests/` may change. Run directories go under `build/` (phreeqc3) or `~/phreeqcrm/build-patched-*` (PhreeqcRM), new ones only.
2. Never run `git commit`, `checkout`, `apply`, `reset`, `stash`, `clean`, or `worktree`. Do not reconfigure build trees; only rebuild them.
3. Never change baselines, inputs, or the helper scripts. Report every comparison as it comes out.
4. Write large edits in parts of at most about 150 lines, and keep your reasoning short before each tool call.
5. Use Plan mode first, present the plan, and implement only after the reviewer replies `APPROVED`. The report is given in your final reply.

## Part A: phreeqc3

**A1. Defaults.** In `read_newton_linear_solver_options()`:
- **Mode:** an unset or empty `PHREEQC_LINEAR_SOLVER_CONDITION_CHECK` now gives `NEWTON_CONDITION_CHECK_GATE`. An invalid value gives `GATE` with `condition_options_valid = false`.
- **Threshold:** an unset or empty `PHREEQC_LINEAR_SOLVER_MIN_RCOND` now gives `1.0e-8`. An invalid value gives `1.0e-8` with `condition_options_valid = false`.

Update the comments to match. In `test_newton_linear_solver_options.cpp`, change only the expectations for these defaults (unset gives GATE and 1e-8, invalid values give GATE and 1e-8, `off` gives OFF, `log` gives LOG). The `LapackNewtonSolver` struct defaults and `test_lapack_newton_solver.cpp` stay unchanged.

**A2. Build and test:** `petsc_on` (7 tests), `lapack_only` (2), `petsc_lapack` (8), all passing.

**A3. Steering validation, `lapack_only`.** For each input below, run twice in new directories:
- `build/lapack_only/task2e_<name>_lapack`, with `PHREEQC_LINEAR_SOLVER_STATISTICS=1`, which uses the gate by default;
- `build/lapack_only/task2e_<name>_cl1`, with `PHREEQC_LINEAR_SOLVER=cl1`.

| name | command (from the run directory) |
|---|---|
| ex1 … ex8 | `../../../build/lapack_only/phreeqc ../../../examples/exN exN.out ../database/phreeqc.dat exN.log` |
| units | `../../../build/lapack_only/phreeqc ../../condition_inputs/units.pqi units.out ../database/phreeqc.dat units.log` |
| co2 | `../../../build/lapack_only/phreeqc ../../profiling_inputs/co2_basalt_profiling.pqi profiling.out ../../profiling_inputs/phreeqc.dat profiling.log` |

For each input, report:
- both exit statuses;
- the `phreeqc_linear_solver_statistics:` and `phreeqc_lapack_statistics:` lines, including `ill_conditioned`;
- the main output compared with `python3 tests/petsc_linear_solver/tools/compare_phreeqcrm_chem_output.py <cl1 .out> <lapack .out> --relative-tolerance 1e-6 --absolute-tolerance 1e-12`, with its verdict and every differing line;
- every `.sel` file present in both directories, compared with `compare_selected_output_to_baseline.py <cl1 .sel> <lapack .sel>`, with the verdict and any failing columns.

**A4. ex4 detail (P2-D19 (d)).** Also run ex4 with comparison on and the gate in effect:
```
PHREEQC_LINEAR_SOLVER_STATISTICS=1 PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1=1 PHREEQC_LINEAR_SOLVER_COMPARE_LOG=newton_step_comparison.log
```
in `task2e_ex4_gate_compare`. Report the full output of `summarize_petsc_cl1_step_comparison.py`, plus the count of records with `solve_status = 1`, which are the gated ones, and with `cl1_return_code = 0`.

## Part B: PhreeqcRM

**B1. Copy.** Copy the 7 adapter files from `~/phreeqc3/src/petsc_linear_solver/` to `~/phreeqcrm/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/newton_linear_solver/` (the same 7 names as Task 2b). Confirm each with `cmp`, then report `git -C ~/phreeqcrm status --short` and `git -C ~/phreeqcrm diff --stat`.

**B2. Rebuild** without reconfiguring: `cmake --build ~/phreeqcrm/build-patched-openmp-lapack -j 8`, the same for `build-patched-openmp-lapack-asan` and `build-patched-mpi-lapack`.

**B3. Runs.** Same method as Task 2b step 5: a fresh copy of `Tests/`, a marker touched before the run, `timeout 600`, `OPENBLAS_NUM_THREADS=1`, and explicit `mpiexec -x NAME=value`. All cases are new directories:

| Tree | Case | Environment and command | Compare with |
|---|---|---|---|
| openmp-lapack | `Tests_gate` | `PHREEQC_LINEAR_SOLVER_STATISTICS=1 ./TestRM` | `openmp_run1` |
| openmp-lapack-asan | `Tests_gate_asan` | `ASAN_OPTIONS=detect_leaks=1 PHREEQC_LINEAR_SOLVER_STATISTICS=1 ./TestRM` | `openmp_run1` |
| mpi-lapack | `Tests_gate_np1`, `Tests_gate_np2`, `Tests_gate_np4` | `mpiexec -n <k> -x OPENBLAS_NUM_THREADS=1 -x PHREEQC_LINEAR_SOLVER_STATISTICS=1 ./TestRM` | `Tests_np<k>` |
| mpi-lapack | `Tests_log_compare_np1` | `mpiexec -n 1 -x OPENBLAS_NUM_THREADS=1 -x PHREEQC_LINEAR_SOLVER_STATISTICS=1 -x PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1=1 -x PHREEQC_LINEAR_SOLVER_CONDITION_CHECK=log -x PHREEQC_LINEAR_SOLVER_COMPARE_LOG=newton_step_comparison.log ./TestRM` | `Tests_np1` (exact, because cl1 steers) |

**B4. Comparisons.** For each case, against its baseline under `~/phreeqcrm_phase2_baseline/v390/`:
- `.chem.txt` and `_utility.txt` files: `compare_phreeqcrm_chem_output.py`, run in **both** exact mode and numeric mode (`--relative-tolerance 1e-6 --absolute-tolerance 1e-12`);
- `.log.txt` files: `cmp`;
- `.dmp` files: `cmp`, reported only.

Report one line per file and mode. Show the full output for every file that is not within tolerance. State explicitly the result for `Advect_cpp_units_utility.txt` in `Tests_gate`.

**B5. Statistics.** For each case, using the Task 2b step 7 counting method, report:
- the sums from the `phreeqc_linear_solver_statistics:` lines;
- the sums from the `phreeqc_lapack_statistics:` lines, including `ill_conditioned` and `lapack_errors`;
- any ASan report.

**B6. PhreeqcRM conditioning data.** On `Tests_log_compare_np1/newton_step_comparison.log`:
- run `summarize_petsc_cl1_step_comparison.py --large-difference-threshold 1e-8`, and report its full output;
- run `calibrate_condition_threshold.py`, and report its full output;
- list the records with `relative_difference > 1e-3`, showing all 10 columns (at most 70 rows).

## Report (in your final reply)

Report pass or fail and the outputs for every step, the files changed in both repositories, and any deviation with the reason for it. End with a section **"Reviewer decisions required"** that lists:
- every comparison outside tolerance;
- every input where LAPACK was used on a call in which `cl1()` failed;
- the PhreeqcRM calibration table, especially whether 10⁻⁸ separates the PhreeqcRM disagreeing and agreeing steps;
- any ASan report or failed run.

Then stop.
