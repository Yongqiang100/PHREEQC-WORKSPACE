# Phase 1B Task 3: Slim the Accepted Path, Measure It, and Classify the Output Differences (phreeqc3)

## Context

Task 2 (decision B-D11) showed three things:
- Steering accepts 99.9 % of the CO₂–basalt calls and keeps those results within tolerance.
- It is nevertheless **6.6 % slower**, because each accepted call does two LU factorizations and two condition estimates.
- Five examples differ in redox-sensitive quantities, and ex3 needs 60 % more `ineq()` calls.

This task:
1. removes the redundant work on the steering path. The shadow path is untouched;
2. measures the time spent in the steering evaluation;
3. re-times the CO₂–basalt input against the stop criterion of B-D11 (g);
4. collects the complete output differences, and the ex3 iteration counts, for classification.

## Rules (apply to every task)

1. Touch only the files listed under Deliverables. **No change to `src/model.cpp` or any other PHREEQC source.**
2. Code is C++11. Functions called from PHREEQC code never throw and never abort. No PETSc header in the changed files.
3. With `INTERCEPT` unset, and in shadow mode, every result and statistics line is identical to Task 2.
4. Never change tolerances, baselines, inputs, or the helper scripts.
5. Write edits in parts of at most about 150 lines, and keep your reasoning short before each tool call. Run long commands in the background (`nohup … &`) and poll them.
6. Use Plan mode first, present the plan, and implement only after the reviewer replies `APPROVED`. Do not run `git commit`. The report is given in your final reply. You may delete `build/lapack_only/tmp_ex8dbg` (the reviewer approves `rm -r` for that one directory only).

## Deliverables

1. `src/petsc_linear_solver/lapack_newton_solver.{h,cpp}`: the fast path for the two-sided metric.
2. `src/petsc_linear_solver/newton_linear_solver_state.{h,cpp}`: configure the steering solver for the fast path; add the time accumulator and its statistics field.
3. `tests/petsc_linear_solver/test_lapack_newton_solver.cpp`: an equivalence test for the fast path.

## Specification

### LAPACK solver: the fast two-sided metric

- **New field** `bool equilibrated_condition_from_factors = false;`.
- **In `solve_newton_step_with_lapack`,** after a successful `dgetrf`, if `equilibrated_condition_from_factors` is true (and `compute_equilibrated_condition` is false):
  1. **Column maxima:** `column_max_j = max_i |scaled_matrix[j*n + i]|`, from the row-equilibrated matrix *before* factorization, which `scaled_matrix` still holds. If any `column_max_j == 0`, the value is −1.
  2. **Scaled factors:** copy `factor_matrix` into `equilibrated_matrix`, and divide every **U** entry (row index i ≤ column index j) of column j by `column_max_j`. The **L** entries (i > j) stay unchanged.
  3. **Norm:** `anorm = max_j Σ_i |scaled_matrix[j*n + i]| / column_max_j`.
  4. **Estimate:** call `dgecon_("1", &n, equilibrated_matrix.data(), &n, &anorm, &rcond, condition_work.data(), condition_iwork.data(), &info, 1)`. If `info != 0`, the value is −1 and `lapack_error_count` is incremented. Store the result in `last_equilibrated_reciprocal_condition`.
  5. **Skip the row metric:** in this mode the existing row-metric `dgecon` is **not** called, and `last_reciprocal_condition` stays −1.

  The gate then uses `last_equilibrated_reciprocal_condition`, exactly as before. Everything else (solve, backward error) is unchanged.
- **Why this is valid:** partial pivoting compares entries within one column, and column scaling multiplies a whole column by one positive factor. The pivot sequence is therefore the same, and P·A·D⁻¹ = L·(U·D⁻¹). The existing full method (a second `dgetrf` on the column-scaled matrix) stays in place for `compute_equilibrated_condition = true`, which shadow mode uses.

### Steering configuration and timing

- **In `create_newton_linear_solver_state`, for steering mode only:** set `cl1_problem_solver->compute_equilibrated_condition = false` and `cl1_problem_solver->equilibrated_condition_from_factors = (condition_metric_row_column == 1)`. Shadow mode keeps its Task 1b/2 settings.
- **Timing:** in `cl1_problem_steer`, measure the elapsed `std::chrono::steady_clock` time of everything after the exclusion checks: evaluation, plus writing the solution on accept. Accumulate it in `double cl1_problem_steer_seconds`.
- **Statistics:** append ` seconds=%.6f` to the `phreeqc_cl1_problem_steer:` line.

### Test (`test_lapack_newton_solver.cpp`, new case 14)

Compare the two methods, fast versus full, on 200 matrices generated with the Task 1b LCG (seed 777):
- sizes n = 5 to 30;
- entries `2u − 1`, with diagonal n + 1;
- column j multiplied by `10^(−12·u_j)` for a random u_j;
- in half of the cases, the last row replaced by row 0 + 1e-10 perturbations.

For each matrix:
- **Method A:** `compute_equilibrated_condition = true`.
- **Method B:** `equilibrated_condition_from_factors = true`.

Both use mode `LOG`. Require the two values to agree within a relative 1e-6 whenever both are > 1e-14. Require equal gate decisions at a minimum of 1e-8 in **every** case. Report the largest relative difference observed.

## Validation

1. **Build and test:** `lapack_only`, `petsc_lapack`, `petsc_on`, and `lapack_only_asan` with `ctest -R '^test_'`. All pass. Report the case-14 largest difference.
2. **Unchanged paths:**
   - default ex1 and ex2: identical to Task 2;
   - ex4 gate-mode shadow: `run.stderr` identical to Task 2.
3. **Steering, same decisions:** rerun the 10 steering runs of Task 2 in `build/lapack_only/1bt3_<name>_steer`, with the same environment.
   - The `phreeqc_cl1_problem_steer:` line must equal Task 2's apart from the new `seconds=` field. Report any count that differs.
   - `cmp` every `.sel` file, and compare `.out` with `compare_main_output_to_baseline.py`, against the Task 2 steering outputs (`1bt2_<name>_steer`). They should be identical; report any difference.
4. **Timing (the B-D11 (g) criterion).** Run co2 alternating `cl1` / steering, 3 each (cl1, steer, cl1, steer, cl1, steer), with the `date` pattern, in the background. hpc02 must be quiet: check `uptime` and `squeue` first, and stop if the load is above 4. Report:
   - the six wall times;
   - both medians;
   - the ratio steer / cl1;
   - the `seconds=` value of each steering run, and `seconds / accept` in µs per accepted call.
5. **Classification data.**
   - **(a)** For ex2, ex3, ex6, ex7 and ex8, run `compare_phreeqcrm_chem_output.py <1bt1_<name>_cl1/.out> <1bt3_<name>_steer/.out> --relative-tolerance 1e-6 --absolute-tolerance 1e-12 --max-reported-differences 400`, and report the **full** output of each.
   - **(b)** For ex3: run `grep -n "Iterations" ` on both `.out` files (the `cl1` reference and steering), and give a side-by-side table of the iteration count per calculation.

## Report (in your final reply)

Report pass or fail and the outputs for every step, the files changed, and any deviation with the reason for it. End with **"Reviewer decisions required"**, giving:
- the co2 medians and their ratio, and the µs per accepted call;
- whether the 10 % criterion of B-D11 (g) is met (stated as a fact);
- for each of the five inputs, the number of differing lines, and the names of every species or quantity that appears in them (from 5 (a));
- the ex3 iteration table.

Then stop.
