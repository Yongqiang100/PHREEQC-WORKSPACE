# Phase 1B Task 2: cl1-Problem Steering Mode, Validated by Final Results (phreeqc3)

## Context

Tasks 1 to 1c (decisions B-D8 to B-D10) established the following:
- The cl1-problem check works, and is exact on every shadow run.
- With a two-sided conditioning measure, most CO₂–basalt calls are square, feasible and well-conditioned. This task measures the exact fraction (the `accept` count).
- LU steps differ from `cl1()`'s mainly in weakly determined components. That does not matter for the rows, but must be judged by its effect on results.

This task adds **steering**: when the check accepts, the LU solution replaces the `cl1()` call. Acceptance requires the B-D10 (e) checks, including the new componentwise residual check. It then validates by final results, convergence and run time (B-D10 (f)).

## Rules (apply to every task)

1. Touch only the files listed under Deliverables. In `src/model.cpp`, only the hook block immediately before the existing `cl1(...)` statement changes, plus the single `if` line in front of that statement (B-D2). The `cl1(...)` statement itself is unchanged.
2. Code is C++11. Functions called from PHREEQC code never throw and never abort. No PETSc header in the new code.
3. With `PHREEQC_LINEAR_SOLVER_INTERCEPT` unset, every result and every statistics line is identical to Task 1c. With both build options OFF, results are byte-identical to the baseline.
4. Never change tolerances, baselines, inputs, or the helper scripts.
5. Write edits in parts of at most about 150 lines, and keep your reasoning short before each tool call. Run long commands in the background (`nohup … &`) and poll them.
6. Use Plan mode first, present the plan, and implement only after the reviewer replies `APPROVED`. Do not run `git commit`. The report is given in your final reply.

## Deliverables

1. `src/petsc_linear_solver/cl1_problem_check.{h,cpp}`: the componentwise check (new outcome 12) and an output function for steering.
2. `src/petsc_linear_solver/newton_linear_solver_options.{h,cpp}`: `INTERCEPT=cl1_problem`, and `PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_MAX_COMPONENTWISE_ERROR`.
3. `src/petsc_linear_solver/newton_linear_solver_state.{h,cpp}`: the steering entry point, counters, and the statistics line.
4. `src/model.cpp`: the hook block only.
5. Tests: `test_cl1_problem_check.cpp` and `test_newton_linear_solver_options.cpp`, new cases.

## Specification

### Options

- **`NewtonInterceptMode`** gains `NEWTON_INTERCEPT_CL1_PROBLEM = 2`, selected by `PHREEQC_LINEAR_SOLVER_INTERCEPT=cl1_problem`. It is valid only with LAPACK, and invalid values behave as in Task 1.
- **`double cl1_problem_max_componentwise_error`**, from `PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_MAX_COMPONENTWISE_ERROR`: a positive finite number, parsed with `strtod` with the whole string consumed. The default is `1e-8`. An invalid value gives `1e-8` with `intercept_mode_valid = false`.
- **In steering mode,** `create_newton_linear_solver_state` creates `cl1_problem_solver` exactly as in shadow mode (same condition options, `compute_equilibrated_condition = true`). The Phase 1A entry dispatcher is off, exactly as in shadow mode.

### `cl1_problem_check`

- **New outcome** `CL1_PROBLEM_COMPONENTWISE_BACKWARD_ERROR = 12`, so `CL1_PROBLEM_OUTCOME_COUNT` becomes 13.
- **`evaluate_cl1_problem` gains a parameter** `double max_componentwise_error`. After step 6 (the accepted solve) and **before** the E-row check (step 7), compute over the A and C rows, on the full row (all n columns, x = `candidate.x`):
  ```
  componentwise_error = max_r |b_r − Σ_j a_rj x_j| / (|b_r| + Σ_j |a_rj x_j|)
  ```
  taking 0 for a row whose denominator is 0. Store it in the candidate as `double componentwise_error`. If it exceeds `max_componentwise_error`, set the outcome to 12 and return.

  Shadow mode passes the option value too. Its Task 1c log format and statistics fields stay unchanged, but the new outcome is counted, and appended to the shadow statistics line as ` componentwise_backward_error=%lu`.
- **New function:**
  ```cpp
  /* For an ACCEPT candidate: writes the solution to x_out[0..n-1], residuals to res_out for all
     k+l+m rows (b - A x, d - C x, f - E x), and returns the sum of |A-row residuals|. */
  double write_cl1_problem_solution(const Cl1ProblemView &view, const Cl1ProblemCandidate &candidate,
                                    LDBLE *x_out, LDBLE *res_out);
  ```
  It must compute the residuals from the view **before** `cl1()` would run. Since `cl1()` will not run when steering accepts, the view is still intact.

### State: steering entry point

```cpp
bool cl1_problem_steering_active(const NewtonLinearSolverState *state);
/* Returns 1 if the LU solution was accepted and written (delta1, res, *l_kode = 0, *l_iter = 0,
   *l_error = sum |A residuals|); 0 if cl1() must run. Arguments as cl1_problem_shadow_prepare, plus
   the cl1 output locations. */
int cl1_problem_steer(NewtonLinearSolverState *state, int in_kode, int pitzer_or_sit,
	int mass_water_switch_on, int numerical_derivatives_on, int k, int l, int m, int n, int row_stride,
	const LDBLE *q, LDBLE *delta1, LDBLE *res, int *l_kode, int *l_iter, LDBLE *l_error);
```

- **`cl1_problem_steer`:**
  - applies the same exclusions as `prepare`, in the same order;
  - otherwise calls `evaluate_cl1_problem`, passing `delta1` as `variable_signs`;
  - counts the outcome in `cl1_problem_outcome_count[]`;
  - on `ACCEPT`, calls `write_cl1_problem_solution` and sets the three scalars.

  Only `delta1` is read before being written, so no copy is needed: the evaluation finishes before anything is written.
- **Statistics:** at destroy, if `print_statistics` and the mode is steering, print:
  ```
  phreeqc_cl1_problem_steer: calls=%lu accept=%lu excluded_retry=%lu excluded_pitzer_sit=%lu excluded_mass_water=%lu excluded_numerical_derivatives=%lu not_square=%lu non_finite=%lu ill_conditioned=%lu solve_failed=%lu backward_error=%lu inequality_violated=%lu sign_violated=%lu componentwise_backward_error=%lu
  ```

### `src/model.cpp`: the hook block

Directly before the existing `cl1(` statement, after the existing shadow block:

```cpp
#ifdef USE_NEWTON_LINEAR_SOLVER
	int newton_cl1_problem_accepted = 0;
	if (cl1_problem_steering_active(newton_linear_solver_state))
		newton_cl1_problem_accepted = cl1_problem_steer(newton_linear_solver_state, in_kode,
			(pitzer_model || sit_model) ? 1 : 0, mass_water_switch ? 1 : 0, numerical_deriv ? 1 : 0,
			k, l, m, n,
			((sit_model || pitzer_model) && full_pitzer == FALSE) ? n + 2 : (int) max_column_count,
			&ineq_array[0], &delta1[0], &res[0], &l_kode, &l_iter, &l_error);
	if (!newton_cl1_problem_accepted)
#endif
```

- The existing `cl1(...)` statement follows **unchanged**, so it becomes the body of that `if` when the macro is defined.
- Report the exact types of `l_kode`, `l_iter` and `l_error` in `ineq_cl1_original` (Task 0 C7). If any is not `int`, `int`, `LDBLE`, stop and ask.
- Shadow and steering are mutually exclusive by mode, so the shadow `record` block after `cl1()` is unaffected.

### Tests

**`test_cl1_problem_check.cpp`,** new cases:
- **8.** Problem 1 with an A row whose terms nearly cancel. Build a row r with `b_r` and `a_r·x` equal to within 1e-6 relative of `|b_r| + Σ|a_rj x_j|`; for example, 1e8·x0 − 1e8·x1 = b with a perturbed right-hand side. Check that `componentwise_error` is computed, and that with `max_componentwise_error = 1e-12` the outcome is 12.
- **9.** `write_cl1_problem_solution` on Problem 2 (with the satisfied E row):
  - x equals the expected solution;
  - `res` holds 0 for the A and C rows (within 1e-14) and `f − E·x` for the E row;
  - the return value is within 1e-14 of 0.
- **10.** `cl1_problem_steer` on a rejected problem (Problem 5, not square): returns 0, and `delta1`, `res`, `l_kode`, `l_iter` and `l_error` are all unchanged.

**Options tests:**
- `cl1_problem` gives mode 2, valid with LAPACK;
- `MAX_COMPONENTWISE_ERROR` unset gives 1e-8; `1e-6` gives 1e-6; `abc` gives 1e-8 and invalid.

## Validation

1. **Build and test:** `petsc_off` (ex1 and ex2 byte-identical to the baseline), `petsc_on`, `lapack_only`, `petsc_lapack`, and `lapack_only_asan` with `ctest -R '^test_'`. All pass.
2. **Default and shadow:** the ex1 and ex2 default runs are identical to Task 1c. The ex4 gate-mode shadow line is identical to Task 1c, except for the appended `componentwise_backward_error=` field.
3. **Steering runs.** For the 10 Task 1 inputs, in `build/lapack_only/1bt2_<name>_steer`, use:
   ```
   PHREEQC_LINEAR_SOLVER_STATISTICS=1 PHREEQC_LINEAR_SOLVER_INTERCEPT=cl1_problem PHREEQC_LINEAR_SOLVER_CONDITION_CHECK=gate PHREEQC_LINEAR_SOLVER_CONDITION_METRIC=row_column PHREEQC_LINEAR_SOLVER_MIN_RCOND=1e-8
   ```
   with the Task 2e A3 commands. The reference is the Task 1 `cl1` run, `build/lapack_only/1bt1_<name>_cl1`. For each input, report:
   - **(a)** the exit status, and the `phreeqc_cl1_problem_steer:` line, with acceptance = accept / calls;
   - **(b)** `.out` against the reference: `compare_phreeqcrm_chem_output.py <ref> <new> --relative-tolerance 1e-6 --absolute-tolerance 1e-12 --max-reported-differences 50`, with the verdict and the first differing lines;
   - **(c)** every `.sel` file: `compare_selected_output_to_baseline.py <ref> <new>`, with the verdict and the failing columns;
   - **(d)** `ineq()` calls with steering, i.e. `calls` from the steer line, against without, i.e. `calls` from the Task 1b log-mode shadow line of the same input, whose trajectory is `cl1()`'s. Give the ratio;
   - **(e)** `grep -c -i "warning\|error\|did not converge"` on both `.out` files, and the lines that differ (`diff <(grep -i "warning\|error\|did not converge" ref.out) <(… new.out)`).
4. **Timing.** `co2` steering against `co2` with `PHREEQC_LINEAR_SOLVER=cl1`: 3 repeats each, alternating (cl1, steer, cl1, steer, cl1, steer), with the `date` timing pattern of Phase 1B Task 0, run in the background. Report the six wall times and the two medians. Also run `ex6` and `ex8` once each way.
5. **Memory:** `lapack_only_asan`, the `units`, `ex4` and `ex7` steering runs, with `ASAN_OPTIONS=detect_leaks=1`. No ASan report.

## Report (in your final reply)

Report pass or fail and the outputs for every step, the files changed, the types checked, and any deviation with the reason for it. End with **"Reviewer decisions required"**: one row per input with the acceptance, the outputs verdict, the `.sel` verdict, the call ratio, and any new warnings or errors; then the CO₂–basalt medians and their ratio; then any ASan report. Then stop.
