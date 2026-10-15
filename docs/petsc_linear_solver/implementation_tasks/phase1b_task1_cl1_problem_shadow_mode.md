# Phase 1B Task 1: cl1-Problem Hook in Shadow Mode (phreeqc3)

## Context

The Phase 1B design (`docs/petsc_linear_solver/phase1b_design.md`) and decisions B-D1 to B-D7 (`docs/petsc_linear_solver/phase1b_reviewer_decisions.md`) define a check:
1. take the constrained L1 problem `ineq()` passes to `cl1()`;
2. restrict its A and C rows to the non-zero columns;
3. if that system is square, solve it with the LAPACK backend (with the condition gate);
4. accept the solution only if it satisfies every E row and every variable sign restriction.

This task implements that check in **shadow mode**. It classifies and solves every eligible call, `cl1()` always runs as today, and the candidate is compared with `cl1()`'s result. **Nothing steers.** A shadow run must be byte-identical to a `PHREEQC_LINEAR_SOLVER=cl1` run.

The task answers three questions:
- How many calls are square and feasible (the eventual acceptance rate)?
- Why are the others rejected?
- Does every accepted candidate agree with `cl1()`, i.e. are there no falsifiers (B-D7)?

## Rules (apply to every task)

1. Touch only the files listed under Deliverables. In `src/model.cpp`, only the two places specified below change (B-D2). `ineq_cl1_original` otherwise stays untouched.
2. Code is C++11. Functions called from PHREEQC code never throw and never abort. The new adapter files include no PETSc header.
3. With `PHREEQC_LINEAR_SOLVER_INTERCEPT` unset, every result and every statistics line is identical to Phase 2 Task 3. With both build options OFF, results are byte-identical to the baseline.
4. Never change tolerances, baselines, test inputs, or the helper scripts.
5. Write large files in parts of at most about 150 lines per edit, and keep your reasoning short before each tool call. Run any command that may take longer than 2 minutes in the background (`nohup … &`) and poll it.
6. Use Plan mode first, present the plan, and implement only after the reviewer replies `APPROVED`. Do not run `git commit`. The report is given in your final reply.

## Deliverables

1. **New** `src/petsc_linear_solver/cl1_problem_check.h`, `.cpp`: the check (LAPACK builds only).
2. `src/petsc_linear_solver/newton_linear_solver_options.{h,cpp}`: the interception mode and the shadow-log options.
3. `src/petsc_linear_solver/newton_linear_solver_state.{h,cpp}`: the shadow state, the two hook functions, and counters and statistics.
4. `src/model.cpp`: the dispatcher condition and the hook around `cl1(...)`.
5. `CMakeLists.txt` (adapter block only): the new source in LAPACK builds, and the new test.
6. **New** `tests/petsc_linear_solver/test_cl1_problem_check.cpp`, and new cases in `test_newton_linear_solver_options.cpp`.

## Specification

### Options

- **Interception mode.** Add:
  ```cpp
  enum NewtonInterceptMode { NEWTON_INTERCEPT_ENTRY = 0, NEWTON_INTERCEPT_CL1_PROBLEM_SHADOW = 1 };
  ```
  and the fields `NewtonInterceptMode intercept_mode;` and `bool intercept_mode_valid;`.

  `PHREEQC_LINEAR_SOLVER_INTERCEPT`:

  | Value | Result |
  |---|---|
  | unset or empty | `ENTRY` |
  | `entry` | `ENTRY` |
  | `cl1_problem_shadow` | `CL1_PROBLEM_SHADOW`, valid only if `USE_LAPACK_NEWTON_SOLVER` is defined |
  | anything else, or shadow without LAPACK | `ENTRY`, with `intercept_mode_valid = false` |

- **Shadow-log options:** `std::string cl1_problem_log_path;` and `unsigned long cl1_problem_log_every;`.
  - `PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG` gives the path, with the `%r`, `%o` and `%p` placeholders as in Task 3. Unset or empty means `cl1_problem_shadow.log`.
  - `PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG_EVERY` is a positive integer, parsed with `strtoul` with the whole string consumed. Unset gives 1. An invalid value gives 1, with `intercept_mode_valid = false`.

### `cl1_problem_check.h` / `.cpp` (only with `USE_LAPACK_NEWTON_SOLVER`; no PETSc)

```cpp
enum Cl1ProblemOutcome {
	CL1_PROBLEM_ACCEPT = 0, CL1_PROBLEM_EXCLUDED_RETRY = 1, CL1_PROBLEM_EXCLUDED_PITZER_SIT = 2,
	CL1_PROBLEM_EXCLUDED_MASS_WATER = 3, CL1_PROBLEM_EXCLUDED_NUMERICAL_DERIVATIVES = 4,
	CL1_PROBLEM_NOT_SQUARE = 5, CL1_PROBLEM_NON_FINITE = 6, CL1_PROBLEM_ILL_CONDITIONED = 7,
	CL1_PROBLEM_SOLVE_FAILED = 8, CL1_PROBLEM_BACKWARD_ERROR = 9,
	CL1_PROBLEM_INEQUALITY_VIOLATED = 10, CL1_PROBLEM_SIGN_VIOLATED = 11, CL1_PROBLEM_OUTCOME_COUNT = 12
};

struct Cl1ProblemView {            /* cl1's input; valid only before cl1() runs */
	int k, l, m, n;                /* A rows, C rows, E rows, columns */
	int row_stride;                /* doubles per row of q */
	const LDBLE *q;                /* rows 0..k+l+m-1; columns 0..n-1; right-hand side in column n */
	const LDBLE *variable_signs;   /* delta1 on entry: -1, 0 or +1 per column */
};

struct Cl1ProblemCandidate {
	int outcome = CL1_PROBLEM_NOT_SQUARE;
	int live_column_count = 0;
	bool solved = false;               /* true if an LU solution exists in x */
	std::vector<LDBLE> x;              /* length n; 0 on non-live columns */
	std::vector<char> live;            /* length n; 1 for live columns */
	double reciprocal_condition = -1.0, backward_error = -1.0;
	double largest_violation = 0.0;    /* of the most violated E row or sign restriction */
	int violated_row = -1;             /* row index in q of the most violated E row, or -1 */
	int violated_column = -1;          /* column of the most violated sign restriction, or -1 */
};

/* Never modifies the view. Uses `solver` (its own LapackNewtonSolver, configured from the options)
   and `augmented` as reusable workspace. */
void evaluate_cl1_problem(LapackNewtonSolver *solver, const Cl1ProblemView &view,
                          Cl1ProblemCandidate &candidate, std::vector<LDBLE> &augmented,
                          std::vector<LDBLE> &live_solution);
```

`evaluate_cl1_problem`, in order:

1. Reset the candidate. Resize `x` and `live` to n.
2. **Live columns:** column j is live if any of the rows 0 … k+l+m−1 has `q[r*row_stride + j] != 0`. Let p be the number of live columns, and J the live columns in increasing order.
3. **Squareness:** if `k + l != p`, set `NOT_SQUARE` and return.
4. **Square system:** build the p × p system in the PHREEQC augmented layout (row-major, leading dimension p + 1). Row i, for i = 0 … p−1, is q row i (A rows first, then C rows) restricted to J, with right-hand side `q[i*row_stride + n]`.
5. **Solve:** call `solve_newton_step_with_lapack(solver, augmented.data(), p, live_solution.data())`, with `live_solution` sized p. Copy `solver->last_reciprocal_condition` and `solver->last_backward_error` into the candidate.
   - `LINEAR_SOLVE_NON_FINITE_INPUT` gives `NON_FINITE`.
   - `LINEAR_SOLVE_FALL_BACK_TO_CL1` gives `ILL_CONDITIONED` if the mode is gate and 0 ≤ reciprocal condition < `solver->min_reciprocal_condition`; otherwise `BACKWARD_ERROR` if the backward error is greater than `solver->max_backward_error`; otherwise `SOLVE_FAILED`.
   - In each of these cases, return.
6. **Accepted solve:** set `solved = true`, `x[J[i]] = live_solution[i]`, and x = 0 on the non-live columns.
7. **E rows** (r = k+l … k+l+m−1): compute `slack = q[r*row_stride + n] - Σ_j q[r*row_stride + j]*x[j]`. Every row with `slack < 0` is a violation of size −slack. **No tolerance.** Record the largest violation and its row. If there is any violation, set `INEQUALITY_VIOLATED` and return.
8. **Signs:** for each column j, `variable_signs[j] < 0 && x[j] > 0` is a violation of size x[j], and `variable_signs[j] > 0 && x[j] < 0` is a violation of size −x[j]. Record the largest and its column. If there is any, set `SIGN_VIOLATED` and return.
9. Otherwise set `ACCEPT`.

### State and hooks (`newton_linear_solver_state.{h,cpp}`)

Declare these unconditionally under `USE_NEWTON_LINEAR_SOLVER`. Without `USE_LAPACK_NEWTON_SOLVER`, `cl1_problem_shadow_active` returns false and the others do nothing.

```cpp
bool cl1_problem_shadow_active(const NewtonLinearSolverState *state);
void cl1_problem_shadow_prepare(NewtonLinearSolverState *state, int in_kode, int pitzer_or_sit,
	int mass_water_switch_on, int numerical_derivatives_on, int k, int l, int m, int n, int row_stride,
	const LDBLE *q, const LDBLE *variable_signs, const int *unknown_types, const int *back_eq);
void cl1_problem_shadow_record(NewtonLinearSolverState *state, int cl1_kode, const LDBLE *cl1_x);
```

- **New state fields** (under `#ifdef USE_LAPACK_NEWTON_SOLVER`):
  - `LapackNewtonSolver *cl1_problem_solver`: a separate solver, so that the Phase 1A counters stay unchanged;
  - a candidate, workspace vectors, `pending` (bool);
  - pointers to the `unknown_types` and `back_eq` passed to `prepare`;
  - `FILE *cl1_problem_log` and a record counter;
  - `unsigned long cl1_problem_outcome_count[CL1_PROBLEM_OUTCOME_COUNT]`;
  - `accept_agree`, `accept_disagree`, `accept_cl1_failed`, `double max_accept_relative_difference`, `double max_excluded_cl1_value`.
- **`create_newton_linear_solver_state`:**
  - If `intercept_mode` is shadow, create `cl1_problem_solver` with the same condition-check options as the LAPACK backend, and with `print_statistics = false`.
  - Open the shadow log at `expand_comparison_log_path(cl1_problem_log_path, …)` and write the 15-column header given under "Log record". If the open fails, warn once and keep counting without a log.
  - If `!intercept_mode_valid`, print one warning line.
- **`cl1_problem_shadow_active`:** true if the state exists, the mode is shadow, and `cl1_problem_solver != nullptr`.
- **`cl1_problem_shadow_prepare`:** sets `pending = true` and stores the pointers. Its outcome, in order:
  1. `in_kode != 1` gives `EXCLUDED_RETRY`;
  2. `pitzer_or_sit` gives `EXCLUDED_PITZER_SIT`;
  3. `mass_water_switch_on` gives `EXCLUDED_MASS_WATER`;
  4. `numerical_derivatives_on` gives `EXCLUDED_NUMERICAL_DERIVATIVES`;
  5. otherwise `evaluate_cl1_problem`.
- **`cl1_problem_shadow_record`** (does nothing if `!pending`):
  1. Count the outcome.
  2. If `solved`, compare with `cl1_x` (length n):
     - `relative_difference = max_{j live} |x_j − cl1_x_j| / max(max_{j live} |cl1_x_j|, 1e-300)`;
     - `max_excluded = max_{j not live} |cl1_x_j|`, or 0 if there are none;
     - the largest-difference column.
  3. For `ACCEPT` records:
     - if `cl1_kode != 0`, increment `accept_cl1_failed`;
     - otherwise increment `accept_agree` if the relative difference ≤ 1e-10, else `accept_disagree`;
     - update `max_accept_relative_difference` and `max_excluded_cl1_value`.
  4. Write the log record if the record index modulo `cl1_problem_log_every` is 0, **or** the record is an accept-disagree, **or** an accept with `cl1_kode != 0`.
  5. Set `pending = false`.
- **Log record:** 15 columns, `%.6e` for reals, `nan` where not applicable:
  ```
  call_index n k l m live outcome cl1_kode reciprocal_condition backward_error relative_difference max_excluded_cl1 largest_violation violated_row_unknown_type largest_difference_unknown_type
  ```
  - `violated_row_unknown_type` is `unknown_types[back_eq[violated_row]]` for an E-row violation, `unknown_types[violated_column]` for a sign violation, and −1 otherwise.
  - `largest_difference_unknown_type` is `unknown_types[column]` when `solved`, else −1.
- **Statistics:** at destroy, if `print_statistics`, print one more line:
  ```
  phreeqc_cl1_problem_shadow: calls=%lu accept=%lu excluded_retry=%lu excluded_pitzer_sit=%lu excluded_mass_water=%lu excluded_numerical_derivatives=%lu not_square=%lu non_finite=%lu ill_conditioned=%lu solve_failed=%lu backward_error=%lu inequality_violated=%lu sign_violated=%lu accept_agree=%lu accept_disagree=%lu accept_cl1_failed=%lu max_accept_relative_difference=%.3e max_excluded_cl1_value=%.3e
  ```
  Then close the log and destroy `cl1_problem_solver`.

### `src/model.cpp`: exactly two places

1. **Dispatcher** (`Phreeqc::ineq`). Extend the existing gate `solver_state->backend != NEWTON_BACKEND_CL1` to also require `solver_state->options.intercept_mode == NEWTON_INTERCEPT_ENTRY`. Nothing else changes.
2. **Hook around `cl1(...)`** in `ineq_cl1_original` (B-D2). Immediately before the existing `cl1(` statement:
   ```cpp
   #ifdef USE_NEWTON_LINEAR_SOLVER
   	std::vector<int> newton_unknown_types;
   	const bool newton_cl1_problem_shadow = cl1_problem_shadow_active(newton_linear_solver_state);
   	if (newton_cl1_problem_shadow)
   	{
   		newton_unknown_types.resize(count_unknowns);
   		for (size_t newton_index = 0; newton_index < count_unknowns; newton_index++)
   			newton_unknown_types[newton_index] = x[newton_index]->type;
   		cl1_problem_shadow_prepare(newton_linear_solver_state, in_kode,
   			(pitzer_model || sit_model) ? 1 : 0, mass_water_switch ? 1 : 0, numerical_deriv ? 1 : 0,
   			k, l, m, n,
   			((sit_model || pitzer_model) && full_pitzer == FALSE) ? n + 2 : (int) max_column_count,
   			&ineq_array[0], &delta1[0], newton_unknown_types.data(), back_eq.data());
   	}
   #endif
   ```
   and immediately after the existing `cl1(...);` statement:
   ```cpp
   #ifdef USE_NEWTON_LINEAR_SOLVER
   	if (newton_cl1_problem_shadow)
   		cl1_problem_shadow_record(newton_linear_solver_state, l_kode, &delta1[0]);
   #endif
   ```
   The `cl1(...)` statement itself is unchanged. `cl1()` destroys `ineq_array`, so `prepare` must finish all of its reading before the call, which it does by construction.

   Verify the names `pitzer_model`, `sit_model`, `full_pitzer`, `mass_water_switch`, `numerical_deriv`, `max_column_count` and `back_eq`, and their types, in `src/Phreeqc.h`, and report them. If any differs, stop and ask.

### Tests

**`test_cl1_problem_check.cpp`** (LAPACK builds). Build each problem in a `q` buffer with the given row stride, with the right-hand side in column n.
1. **Square, equality only:** k = 1, l = 2, m = 0, n = 3, with a known solution. Expect `ACCEPT`, `solved`, and `x` within 1e-12.
2. **Problem 1 plus one E row `x0 ≤ 10`:** expect `ACCEPT`. With `x0 ≤ 0.5`, where the solution has x0 = 1: expect `INEQUALITY_VIOLATED`, `violated_row == 3`, and `largest_violation` within 1e-12 of 0.5.
3. **Problem 1 with `variable_signs[0] = -1`** and the solution x0 = 1: expect `SIGN_VIOLATED`, `violated_column == 0`. With +1 on a variable whose solution is negative: also `SIGN_VIOLATED`.
4. **Zero column:** n = 4, column 3 all zero, k + l = 3. Expect `ACCEPT`, `live_column_count == 3`, `x[3] == 0`.
5. **Not square:** k + l = 2 with 3 live columns gives `NOT_SQUARE`.
6. **Nearly singular** rows (as in Task 2d test 9), gate mode, minimum 1e-8: `ILL_CONDITIONED`.
7. **Problem 1 with `row_stride = n + 5`:** expect `ACCEPT` with the same x.

**`test_newton_linear_solver_options.cpp`,** new cases:
- `INTERCEPT` unset gives `ENTRY` and valid; `entry` gives `ENTRY`;
- `cl1_problem_shadow` gives `SHADOW` and valid in LAPACK builds, `ENTRY` and invalid otherwise;
- `bogus` gives `ENTRY` and invalid;
- `LOG_EVERY` of `100` gives 100; `0` and `x` each give 1 and invalid;
- `LOG` unset gives the default name.

## Validation

1. **Build and test:**
   - `petsc_off`: ex1 and ex2 byte-identical to the baseline;
   - `petsc_on`: all tests pass;
   - `lapack_only`: all tests pass, including `test_cl1_problem_check`;
   - `petsc_lapack`: all tests pass;
   - `lapack_only_asan`: `ctest -R '^test_'` passes.
2. **Default unchanged:** in `lapack_only`, with `PHREEQC_LINEAR_SOLVER_STATISTICS=1` and no `INTERCEPT`, ex1 and ex2 statistics and `.sel` files are identical to Task 3. There must be no `phreeqc_cl1_problem_shadow:` line.
3. **Shadow runs.** In `lapack_only`, each in `build/lapack_only/1bt1_<name>_shadow`, with:
   ```
   PHREEQC_LINEAR_SOLVER_STATISTICS=1 PHREEQC_LINEAR_SOLVER_INTERCEPT=cl1_problem_shadow PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG=cl1_problem_shadow.log
   ```
   plus a `cl1` reference run of the same input in `build/lapack_only/1bt1_<name>_cl1` with `PHREEQC_LINEAR_SOLVER=cl1`. The inputs and commands are those of Task 2e A3 (ex1–ex8, `units`, `co2`). For `co2` only, add `PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG_EVERY=100`. For each input, report:
   - both exit statuses;
   - **shadow against `cl1`:** `cmp` of every `.sel` file and `compare_main_output_to_baseline.py` on the `.out`. **All must be identical (B-D7).**
   - the `phreeqc_cl1_problem_shadow:` line;
   - the full output of `python3 tests/petsc_linear_solver/tools/summarize_cl1_problem_shadow_log.py <log>`.
4. **All inputs together:** run `summarize_cl1_problem_shadow_log.py` on all 10 logs, and report the full output and its exit status. Exit 1 means there is a falsifier; list them all.
5. **Memory:** `lapack_only_asan`, the `units` shadow run, and `ex4` shadow run, with `ASAN_OPTIONS=detect_leaks=1`. No ASan report.

## Report (in your final reply)

Report pass or fail and the outputs for every step, the files changed, the `Phreeqc.h` member names and types checked, and any deviation with the reason for it. End with a section **"Reviewer decisions required"** that lists:
- every falsifier;
- every shadow run that is not identical to its `cl1` run;
- the acceptance rate per input (`accept / calls`);
- the rejection reasons, ranked;
- any ASan report.

Then stop.
