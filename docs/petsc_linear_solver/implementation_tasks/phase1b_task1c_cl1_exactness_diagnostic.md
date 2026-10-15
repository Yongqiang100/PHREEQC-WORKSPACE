# Phase 1B Task 1c: Does cl1 Solve Its Own Problem Exactly? (shadow-mode diagnostic)

## Context

Task 1b (decision B-D9) found that, in the CO₂–basalt input, accepted LU candidates differ from `cl1()`'s steps by a median of 6.4 × 10⁻⁷, although the square systems are well-conditioned after two-sided scaling. The hypothesis (B-D9 (d)):
- `cl1()` does not pivot on entries with |value| ≤ `ineq_tol` = 10⁻¹⁵;
- entries for trace species fall below that;
- `cl1()` therefore returns a point that does not satisfy its own equations to round-off, while LU does.

This task measures exactly that. **Shadow mode only. No change to `src/model.cpp` or any other PHREEQC source.**

## Rules (apply to every task)

1. Touch only the files listed under Deliverables.
2. Code is C++11. Functions called from PHREEQC code never throw and never abort. No PETSc header.
3. With `INTERCEPT` unset, every result and every statistics line is identical to Task 1b. The `phreeqc_cl1_problem_shadow:` statistics line is unchanged.
4. Never change tolerances, baselines, inputs, or the helper scripts.
5. Write edits in parts of at most about 150 lines, and keep your reasoning short before each tool call. Run long commands in the background (`nohup … &`) and poll them.
6. Use Plan mode first, present the plan, and implement only after the reviewer replies `APPROVED`. Do not run `git commit`. The report is given in your final reply.

## Deliverables

1. `src/petsc_linear_solver/newton_linear_solver_state.{h,cpp}`: keep a copy of the problem in `prepare`, compute six diagnostics in `record`, and write 6 more log columns.
2. `src/petsc_linear_solver/cl1_problem_check.{h,cpp}`: only if needed, a helper `cl1_problem_relative_residuals(...)` that both of the above can use.

## Specification

### In `cl1_problem_shadow_prepare`, after the existing classification

If the evaluation reached the square-system step (`live_column_count == k + l`), copy rows 0 … k+l+m−1 of q into a state buffer `cl1_problem_q_copy`, packed with stride n + 1. Each row holds columns 0 … n−1 followed by the right-hand side. Also store k, l, m and n. **This copy must be made before `cl1()` runs, because `cl1()` destroys `ineq_array`.**

### In `cl1_problem_shadow_record`, before writing the record

Do the following only when the candidate is solved, the outcome is `ACCEPT` and `cl1_kode == 0`. Otherwise write `nan` and `-1` for the six new columns.

For a row r with coefficients a_r and right-hand side b_r, and a vector z, define:
```
relative_residual(r, z) = |b_r − Σ_j a_rj z_j| / (|b_r| + Σ_j |a_rj z_j|)
```
taking 0 if the denominator is 0.

| Column | Name | Definition |
|---|---|---|
| 17 | `cl1_rel_residual_a` | max over A rows of `relative_residual(r, cl1_x)` |
| 18 | `cl1_rel_residual_c` | max over C rows of `relative_residual(r, cl1_x)` |
| 19 | `cl1_rel_violation_e` | max over E rows of `max(Σ_j e_rj x_j − f_r, 0) / (|f_r| + Σ_j |e_rj x_j|)`, with x = `cl1_x` |
| 20 | `lu_rel_residual_ac` | max over A and C rows of `relative_residual(r, candidate.x)` |
| 21 | `min_abs_entry_m` | smallest nonzero \|a_rj\| over A and C rows, live columns only |
| 22 | `entries_below_1e-15` | number of nonzero entries of A and C rows on live columns with \|a_rj\| ≤ 1e-15 |

`cl1_x` is the vector `cl1()` returned, in cl1's scaled variables, exactly as passed to `record`.

Write the reals with `%.6e` and the count with `%d`. Extend the header line with the six names.

## Validation

1. **Build and test:** `lapack_only`, `petsc_lapack`, `petsc_on`, `lapack_only_asan`, with `ctest -R '^test_'`. All pass.
2. **Default unchanged:** the ex1 and ex2 default runs (statistics only) are identical to the Task 1b default runs, and the ex4 gate-mode shadow line is identical to Task 1b.
3. **Diagnostic runs.** Use exactly the Task 1b log-mode environment, in `build/lapack_only/1bt1c_<name>_shadow`, for `co2` (with `LOG_EVERY=100`), `ex2`, `ex6`, `ex8`, `ex5` and `units`. For each run:
   - check that every `.sel` file and the `.out` file are identical to the Task 1 `cl1` runs;
   - check that the `phreeqc_cl1_problem_shadow:` line is identical to Task 1b;
   - run `python3 tests/petsc_linear_solver/tools/analyze_cl1_problem_residuals.py <log>`, and report its full output.
4. **Together:** run `analyze_cl1_problem_residuals.py` on all six logs together, and report the full output.
5. **Ten examples:** from the `co2` log, the 10 records with the largest relative difference, and 10 random records from the agreeing group (`awk` plus `shuf -n 10 --random-source=<(yes)`). Show all 22 columns.

## Report (in your final reply)

Report pass or fail and the outputs for every step, and any deviation with the reason for it. End with **"Reviewer decisions required"**, stating as facts only:
- in the disagreeing group, the fraction of records where `cl1()` was inexact, and the fraction with entries at or below 1e-15;
- the same two fractions in the agreeing group;
- the largest `lu_rel_residual_ac` in any group.

Then stop.
