# Phase 1B Task 1b: Conditioning Diagnostics for the cl1-Problem Path (shadow mode)

## Context

Task 1 (decision B-D8) showed that the cl1-problem check rejects 98.4 % of the CO₂–basalt calls as `ill_conditioned`, while genuine bound violations are rare. The likely cause is that the reciprocal condition is measured after **row** equilibration only, and cl1-problem systems mix columns of very different scale. This task:

1. adds a second metric to the LAPACK solver: the reciprocal condition after row **and** column equilibration. It also adds an option to gate on that metric; the default is unchanged;
2. records both metrics in the shadow log, as a 16th column;
3. reruns the shadow on the same 10 inputs **in log mode**, so that every square, feasible call is solved and compared with `cl1()`;
4. reports the calibration tables for both metrics.

**Shadow mode only. Nothing steers.** Choosing the metric and threshold is a reviewer decision after this task.

## Rules (apply to every task)

1. Touch only the files listed under Deliverables. **No change to `src/model.cpp` or any other PHREEQC source.**
2. Code is C++11. Functions called from PHREEQC code never throw and never abort. The LAPACK files include no PETSc header.
3. With the new options unset, every result and every statistics line is identical to Task 1.
4. Never change tolerances, baselines, test inputs, or the helper scripts.
5. Write large edits in parts of at most about 150 lines, and keep your reasoning short before each tool call. Run any command that may take longer than 2 minutes in the background (`nohup … &`) and poll it.
6. Use Plan mode first, present the plan, and implement only after the reviewer replies `APPROVED`. Do not run `git commit`. The report is given in your final reply.

## Deliverables

1. `src/petsc_linear_solver/lapack_newton_solver.{h,cpp}`: the equilibrated metric and the metric choice for the gate.
2. `src/petsc_linear_solver/newton_linear_solver_options.{h,cpp}`: `PHREEQC_LINEAR_SOLVER_CONDITION_METRIC`.
3. `src/petsc_linear_solver/newton_linear_solver_state.cpp`: pass the metric option to both LAPACK solvers; enable the equilibrated metric on the shadow solver; write the 16th log column.
4. `tests/petsc_linear_solver/test_lapack_newton_solver.cpp` and `test_newton_linear_solver_options.cpp`: new cases.

## Specification

### LAPACK solver

- **New fields:**

  ```cpp
  bool   compute_equilibrated_condition = false;      /* diagnostics; shadow solver only */
  int    gate_on_equilibrated_condition = 0;          /* 0: gate on the row metric (unchanged); 1: on row_column */
  double last_equilibrated_reciprocal_condition = -1.0;
  std::vector<double> equilibrated_matrix;            /* n*n, column-major */
  std::vector<int>    equilibrated_pivots;            /* n */
  ```
- **In `solve_newton_step_with_lapack`:**
  - In step 1, also reset `last_equilibrated_reciprocal_condition = -1.0`.
  - **Immediately after a successful `dgetrf` and before the existing condition check,** if `compute_equilibrated_condition || gate_on_equilibrated_condition`:
    1. For each column j, `column_max_j = max_i |scaled_matrix[j*n + i]|`, using the row-equilibrated matrix kept for the backward error. If any `column_max_j == 0`, set the value to −1 and skip the steps below.
    2. `equilibrated_matrix[j*n + i] = scaled_matrix[j*n + i] / column_max_j`.
    3. Take the 1-norm of `equilibrated_matrix`, call `dgetrf_` on it with `equilibrated_pivots`, and if that succeeds call `dgecon_("1", …)`. If either `info != 0`, the value is −1, and `lapack_error_count` is incremented.
    4. Set `last_equilibrated_reciprocal_condition` to the result.
  - **The existing gate** (mode `GATE`) compares `gate_on_equilibrated_condition ? last_equilibrated_reciprocal_condition : last_reciprocal_condition` against `min_reciprocal_condition`. The row metric is still computed and recorded, as before, whenever the mode is not `OFF`.
  - The solution is still computed from `factor_matrix`, exactly as before. **The column scaling affects only the metric.**

### Options

- Add `int condition_metric_row_column;` (0 or 1).
- `PHREEQC_LINEAR_SOLVER_CONDITION_METRIC`, matched exactly:
  - `row`, unset or empty gives 0;
  - `row_column` gives 1;
  - anything else gives 0, with `condition_options_valid = false`.
- `create_newton_linear_solver_state` copies the option into `gate_on_equilibrated_condition` of the LAPACK backend solver **and** of `cl1_problem_solver`, and sets `cl1_problem_solver->compute_equilibrated_condition = true`.

### Shadow log

- Append a 16th column, `reciprocal_condition_equilibrated` (`%.6e`, or `nan` if −1), taken from `cl1_problem_solver->last_equilibrated_reciprocal_condition` after the evaluation. Store it in the candidate as `double equilibrated_reciprocal_condition`.
- Extend the header line accordingly.
- The `phreeqc_cl1_problem_shadow:` statistics line is unchanged.

### Tests

**`test_lapack_newton_solver.cpp`,** new cases:
- **11.** The 3 × 3 system of Task 2 with column 2 multiplied by 1e-12. Expect:
  - accepted in `LOG` mode;
  - `last_reciprocal_condition < 1e-8`;
  - `last_equilibrated_reciprocal_condition > 1e-3`;
  - the solution is the original one with column scaling undone: `y[2] * 1e-12` equals the original `x[2]` within 1e-12 relative (so `y[2]` itself is 1e12 times larger), and `y[0]`, `y[1]` equal the original components within 1e-12.
- **12.** Same system, `GATE` mode, minimum 1e-8:
  - with the row metric: `LINEAR_SOLVE_FALL_BACK_TO_CL1` and `ill_conditioned_count == 1`;
  - with `gate_on_equilibrated_condition = 1`: accepted.
- **13.** The nearly singular system of test 9, with `compute_equilibrated_condition = true`: `last_equilibrated_reciprocal_condition < 1e-12`. A genuinely nearly singular matrix stays nearly singular after column scaling.

**`test_newton_linear_solver_options.cpp`,** new cases:
- `CONDITION_METRIC` unset gives 0; `row_column` gives 1; `row` gives 0; `bogus` gives 0 and invalid.

## Validation

1. **Build and test:** `lapack_only`, `petsc_lapack`, `petsc_on`, `lapack_only_asan` (`ctest -R '^test_'`). All pass.
2. **Default unchanged:**
   - ex1 and ex2 in `lapack_only` with statistics only must equal the Task 1 default runs;
   - one shadow run (`ex4`, same environment as Task 1) must give the same `phreeqc_cl1_problem_shadow:` line as Task 1.
3. **Re-evaluate Task 1 (B-D8 (b)):** run `python3 tests/petsc_linear_solver/tools/summarize_cl1_problem_shadow_log.py build/lapack_only/1bt1_*_shadow/cl1_problem_shadow.log --condition-factor 100`. Report the full output and the exit status, and list every remaining falsifier.
4. **Log-mode shadow runs:** for the 10 inputs of Task 1, in `build/lapack_only/1bt1b_<name>_shadow`, use:
   ```
   PHREEQC_LINEAR_SOLVER_STATISTICS=1 PHREEQC_LINEAR_SOLVER_INTERCEPT=cl1_problem_shadow PHREEQC_LINEAR_SOLVER_CONDITION_CHECK=log PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG=cl1_problem_shadow.log
   ```
   with `PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG_EVERY=100` for `co2` only. For each input:
   - check that every `.sel` file and the `.out` file are identical to the Task 1 `cl1` run (`build/lapack_only/1bt1_<name>_cl1`);
   - report the `phreeqc_cl1_problem_shadow:` line;
   - report the output of `summarize_cl1_problem_shadow_log.py <log> --condition-factor 100`.
5. **Calibration:** run `python3 tests/petsc_linear_solver/tools/calibrate_cl1_problem_condition.py build/lapack_only/1bt1b_*_shadow/cl1_problem_shadow.log`. Report the full output. Also run it separately on the `co2` log alone, and on `ex2`, `ex6` and `ex8` together.
6. **Memory:** in `lapack_only_asan`, the `units` and `ex4` log-mode shadow runs, with `ASAN_OPTIONS=detect_leaks=1`. No ASan report.

## Report (in your final reply)

Report pass or fail and the outputs for every step, the files changed, and any deviation with the reason for it. End with a section **"Reviewer decisions required"** that lists:
- any falsifier remaining under the condition-scaled criterion, in Task 1 or Task 1b logs;
- the two calibration tables;
- any shadow run not identical to its `cl1` run;
- any ASan report.

Then stop.
