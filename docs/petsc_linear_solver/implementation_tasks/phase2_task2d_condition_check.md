# Phase 2 Task 2d: Reciprocal-Condition Check for the LAPACK Backend, and Threshold Calibration (phreeqc3)

## Context

Task 2c (decision P2-D18) found that the LAPACK backend accepts steps on nearly singular Jacobians:
- 63 of 362 steps differ from `cl1()` by more than 10⁻³, mostly in the redox (MH) direction;
- their backward errors are tiny, so the backward-error test cannot detect them.

Design §6.6 foresaw a remedy: estimate the reciprocal condition number of the factored matrix with LAPACK `dgecon` (O(n²) cost), and route ill-conditioned systems to `cl1()`. This task:
1. implements that check in the LAPACK backend, with three modes: `off` (the default, so nothing changes), `log`, and `gate`;
2. adds the reciprocal condition as a 10th column of the comparison log;
3. calibrates the threshold on real inputs, using a reviewer-supplied script. **The threshold itself is a reviewer decision after this task.**

**Work in `~/phreeqc3` only. No PHREEQC source file and no CMake file changes.** `dgecon` is in the same LAPACK library that is already linked.

## Rules (apply to every task)

1. Touch only the files listed under Deliverables. If another file seems to need a change, stop and ask.
2. Code is C++11. Functions called from PHREEQC code never throw and never abort. The LAPACK files include no PETSc header.
3. With the condition check off (the default), results and statistics must be identical to Task 1b.
4. Never change tolerances, baselines, test inputs, or the helper scripts.
5. Write large edits in parts of at most about 150 lines, and keep your reasoning short before each tool call.
6. Use Plan mode first, present the plan, and implement only after the reviewer replies `APPROVED`. Do not run `git commit`. The report is given in your final reply.

## Deliverables

1. `src/petsc_linear_solver/newton_linear_solver_options.h`, `.cpp`: two new options.
2. `src/petsc_linear_solver/lapack_newton_solver.h`, `.cpp`: the condition check.
3. `src/petsc_linear_solver/newton_linear_solver_state.h`, `.cpp`: pass the options through, record the last reciprocal condition, and write the 10th log column.
4. `tests/petsc_linear_solver/test_lapack_newton_solver.cpp` and `test_newton_linear_solver_options.cpp`: new cases.

## Specification

### Options (no PETSc)

Add to `newton_linear_solver_options.h`:

```cpp
enum NewtonConditionCheckMode {
	NEWTON_CONDITION_CHECK_OFF = 0,
	NEWTON_CONDITION_CHECK_LOG = 1,    /* compute and record; never changes the decision */
	NEWTON_CONDITION_CHECK_GATE = 2    /* reciprocal condition below the minimum -> cl1 */
};
```

Add these fields to `NewtonLinearSolverOptions`:
- `NewtonConditionCheckMode condition_check_mode;`
- `double min_reciprocal_condition;`
- `bool condition_options_valid;`

Read them in `read_newton_linear_solver_options()`:
- **`PHREEQC_LINEAR_SOLVER_CONDITION_CHECK`:** `off`, `log` or `gate`, matched exactly. Unset or empty means `off`. Any other value means `off` with `condition_options_valid = false`.
- **`PHREEQC_LINEAR_SOLVER_MIN_RCOND`:** a positive finite number parsed with `std::strtod`, where the whole string must be consumed. Unset or empty gives `1.0e-12`. An invalid, non-positive or non-finite value gives `1.0e-12` with `condition_options_valid = false`.

### LAPACK backend

Add to `LapackNewtonSolver`, and include `newton_linear_solver_options.h` for the enum:

```cpp
	NewtonConditionCheckMode condition_check_mode = NEWTON_CONDITION_CHECK_OFF;
	double min_reciprocal_condition = 1.0e-12;
	std::vector<double> condition_work;      /* length 4n */
	std::vector<int>    condition_iwork;     /* length n  */
	double last_reciprocal_condition = -1.0; /* -1: not computed */
	unsigned long ill_conditioned_count = 0;
```

The prototype, alongside the existing ones (the hidden `size_t` is as for `dgetrs_`):

```cpp
void dgecon_(const char *norm, const int *n, const double *a, const int *lda, const double *anorm,
             double *rcond, double *work, int *iwork, int *info, size_t norm_length);
```

Changes to the algorithm of Task 1b, by step number:

- **Step 1:** also set `last_reciprocal_condition = -1.0`.
- **After step 6** (a successful `dgetrf`), if `condition_check_mode != NEWTON_CONDITION_CHECK_OFF`:
  1. `matrix_one_norm = max_j Σ_i |scaled_matrix[j*n + i]|`, the column sums of the row-equilibrated matrix *before* factorization. `scaled_matrix` is not overwritten.
  2. Resize `condition_work` to 4n and `condition_iwork` to n, then call `dgecon_("1", &n, factor_matrix.data(), &n, &matrix_one_norm, &reciprocal_condition, condition_work.data(), condition_iwork.data(), &info, 1)`.
  3. If `info != 0`: set `reciprocal_condition = -1.0` and increment `lapack_error_count`.
  4. Set `last_reciprocal_condition = reciprocal_condition`.
  5. If the mode is `GATE`, `reciprocal_condition >= 0`, and `reciprocal_condition < min_reciprocal_condition`: increment `ill_conditioned_count` and `cl1_fallback_count`, and return `LINEAR_SOLVE_FALL_BACK_TO_CL1` with `output` unchanged. `dgetrs` is not called.
- **Statistics line:** append ` ill_conditioned=%lu` to `phreeqc_lapack_statistics:`.

### Integration state

- **New field:** `double last_reciprocal_condition = -1.0;`.
- **`create_newton_linear_solver_state`:**
  - If `!options.condition_options_valid`, print one line to stderr: `phreeqc_linear_solver: warning: invalid PHREEQC_LINEAR_SOLVER_CONDITION_CHECK or PHREEQC_LINEAR_SOLVER_MIN_RCOND; using condition check <mode> and minimum <value>`.
  - For the LAPACK backend, copy the two options into the solver.
- **`solve_newton_step_with_backend`:** reset `last_reciprocal_condition = -1.0` before switching on the backend. In the LAPACK case, copy `lapack_solver->last_reciprocal_condition` after the solve.
- **`write_step_comparison_record`:**
  - Append a 10th column: `%.6e` of `state->last_reciprocal_condition` if it is ≥ 0, otherwise `nan`.
  - The header line becomes `# call_index system_size solve_status cl1_return_code backward_error relative_difference relative_sum largest_difference_index largest_difference_unknown_type reciprocal_condition`.
  - The value is written for every status, including fallbacks, so that gated calls are visible.

The dispatcher in `src/model.cpp` already passes the state to `write_step_comparison_record`. **Do not change `src/model.cpp`.**

### Tests

`test_lapack_newton_solver.cpp`, adding to the existing cases:
- **8.** Mode `LOG`, the 3 × 3 system: accepted, with `0.001 < last_reciprocal_condition <= 1`.
- **9.** Nearly singular: J = [[1, 1], [1, 1 + 1e-14]], r = [2, 2 + 1e-14].
  - Mode `LOG`: report the status, `last_reciprocal_condition` (must be < 1e-12), and the backward error.
  - Mode `GATE` with `min_reciprocal_condition = 1e-10`: `LINEAR_SOLVE_FALL_BACK_TO_CL1`, `ill_conditioned_count == 1`, `output` unchanged, and `last_reciprocal_condition < 1e-12`.
- **10.** Mode `OFF`: after a solve, `last_reciprocal_condition == -1.0`.

`test_newton_linear_solver_options.cpp`:
- `CONDITION_CHECK` unset gives `OFF`; `log` gives `LOG`; `gate` gives `GATE`; `bogus` gives `OFF` and invalid.
- `MIN_RCOND` unset gives `1e-12`; `1e-9` gives `1e-9`; `abc`, `-1` and `0` each give `1e-12` and invalid.

## Validation

1. **Build and test all three trees:** `petsc_on` (7 tests), `lapack_only` (2), `petsc_lapack` (8). All pass.
2. **Condition check off, so behaviour must be unchanged:** in `lapack_only`, run ex1 and ex2 with `PHREEQC_LINEAR_SOLVER_STATISTICS=1`. The `.sel` files must be byte-identical to the baseline (`cmp`), and the counts equal Task 1b (ex1 8/8; ex2 3/3, 3 + 8 + 1045 ineligible). The `phreeqc_lapack_statistics:` lines must show `ill_conditioned=0`.
3. **Calibration runs.** Use `lapack_only` in log mode with comparison on, from `build/lapack_only/task2d_<name>`, with:
   ```
   PHREEQC_LINEAR_SOLVER_STATISTICS=1 PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1=1 PHREEQC_LINEAR_SOLVER_CONDITION_CHECK=log PHREEQC_LINEAR_SOLVER_COMPARE_LOG=newton_step_comparison.log
   ```
   - **ex1 to ex8:** `../../../build/lapack_only/phreeqc ../../../examples/exN exN.out ../database/phreeqc.dat exN.log` (8 directories, `task2d_ex1` to `task2d_ex8`).
   - **`units`:** first `mkdir -p build/condition_inputs && cp ~/phreeqcrm/Tests/units.pqi build/condition_inputs/`, then `../../../build/lapack_only/phreeqc ../../condition_inputs/units.pqi units.out ../database/phreeqc.dat units.log`.
   - **`co2`:** `../../../build/lapack_only/phreeqc ../../profiling_inputs/co2_basalt_profiling.pqi profiling.out ../../profiling_inputs/phreeqc.dat profiling.log`.

   For each run, report the exit status and the statistics lines, and run `summarize_petsc_cl1_step_comparison.py` on its log. Then run, once, on all 10 logs together:
   ```
   python3 tests/petsc_linear_solver/tools/calibrate_condition_threshold.py build/lapack_only/task2d_*/newton_step_comparison.log
   ```
   and report its full output.
4. **Gate smoke test (information only; the threshold is not final).**
   - In `task2d_units_gate`, run `units.pqi` with `PHREEQC_LINEAR_SOLVER_STATISTICS=1 PHREEQC_LINEAR_SOLVER_CONDITION_CHECK=gate PHREEQC_LINEAR_SOLVER_MIN_RCOND=1e-10`.
   - In `task2d_units_cl1`, run `units.pqi` with `PHREEQC_LINEAR_SOLVER=cl1`.
   - Report the statistics, including `ill_conditioned`. Compare `sel.out` with `compare_selected_output_to_baseline.py` (cl1 run as baseline), and `units.out` with `compare_main_output_to_baseline.py`.
5. **Memory:** the ASan tree `lapack_only_asan`, rebuilt, running `test_lapack_newton_solver` and the `units` calibration run with `ASAN_OPTIONS=detect_leaks=1`. No ASan report.

## Report

In your final reply, give:
- pass or fail and the outputs for every validation step;
- the files changed;
- any deviation, with the reason for it;
- a section **"Reviewer decisions required"** containing the full calibration table and every unexpected result.

Then stop.
