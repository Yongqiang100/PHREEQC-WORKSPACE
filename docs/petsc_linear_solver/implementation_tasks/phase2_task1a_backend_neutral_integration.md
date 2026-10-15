# Phase 2 Task 1a: Backend-Neutral Integration Layer (no behaviour change)

## Context

Decisions P2-D10 and P2-D11 (`docs/petsc_linear_solver/phase2_task0_reviewer_decisions.md`) make LAPACK the parallel backend. They require an integration layer that does not depend on PETSc, so that a later task can build the LAPACK backend into MPI builds of PhreeqcRM, where PETSc is unavailable.

Today the eligibility counters, comparison mode, and option handling live in `src/petsc_linear_solver/petsc_solver_integration.{h,cpp}`. That code uses PETSc's options database and PETSc types. This task:

1. moves that code into a **backend-neutral layer**, `newton_linear_solver_state.{h,cpp}`, plus `newton_linear_solver_options.{h,cpp}`. Neither includes a PETSc header outside `#ifdef USE_PETSC`;
2. reads integration-level options from **environment variables** instead of PETSc options (table below);
3. renames the PHREEQC-side names from "petsc" to "newton linear solver";
4. keeps every numerical behaviour identical: the same eligibility rule (D8, D17), the same PETSc solve, the same comparison log format.

**No new backend is added in this task.** LAPACK is Task 1b.

| Old PETSc option (Phase 1) | New environment variable |
|---|---|
| `-phreeqc_petsc_enable false` | `PHREEQC_LINEAR_SOLVER=cl1` |
| (default: PETSc on) | `PHREEQC_LINEAR_SOLVER` unset: the build's default backend, which is `petsc` in this task |
| `-phreeqc_petsc_statistics` | `PHREEQC_LINEAR_SOLVER_STATISTICS=1` |
| `-phreeqc_petsc_compare_with_cl1` | `PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1=1` |
| `-phreeqc_petsc_comparison_log <path>` | `PHREEQC_LINEAR_SOLVER_COMPARE_LOG=<path>` (default `newton_step_comparison.log`) |

PETSc-internal options stay in `PETSC_OPTIONS`: `-phreeqc_row_equilibration`, `-phreeqc_max_backward_error`, `-phreeqc_ksp_*`, `-phreeqc_pc_*`, `-log_view`, and `-malloc_debug`.

## Rules (apply to every task)

1. Touch only the files listed under Deliverables. If another file seems to need a change, stop and ask.
2. Before using any PETSc function, type, macro, or constant, grep `$PETSC_DIR/include` for its declaration and use the signature you find. If it is not declared, stop and report.
3. Code is C++11. Functions called from PHREEQC code never throw and never abort.
4. With `PHREEQC_USE_PETSC=OFF`, results are byte-identical to the baseline: `cmp` for `.sel` files, `compare_main_output_to_baseline.py` for `.out` files.
5. Never change tolerances, baselines, test inputs, or the helper scripts.
6. **Do not modify `ineq_cl1_original`**, or any line of `src/model.cpp` outside the eligibility function and the dispatcher.
7. **Write large files in parts.** Create each new file with at most about 150 lines, then extend it with further edits of at most about 150 lines each. Keep your reasoning short before each tool call.
8. Use Plan mode first, present the plan, and implement only after the reviewer replies `APPROVED`. Do not run `git commit`.

## Deliverables

1. **New** `src/petsc_linear_solver/newton_linear_solver_options.h`, `.cpp`: no PETSc include anywhere.
2. **New** `src/petsc_linear_solver/newton_linear_solver_state.h`, `.cpp`: PETSc includes only inside `#ifdef USE_PETSC`.
3. **Delete** `src/petsc_linear_solver/petsc_solver_integration.h` and `.cpp`. Their code moves into item 2.
4. **New** `tests/petsc_linear_solver/test_newton_linear_solver_options.cpp`.
5. `src/Phreeqc.h`: renames only (below).
6. `src/model.cpp`: the eligibility function and the dispatcher only (below).
7. `src/Phreeqc.cpp`: the destructor call and its `#include` only.
8. `CMakeLists.txt`: inside the existing `if(PHREEQC_USE_PETSC)` block only (below).

`src/class_main.cpp` is **not** changed. Its PETSc pin stays under `#ifdef USE_PETSC`.

## Specification

### `newton_linear_solver_options.h`

```cpp
#pragma once
#include <string>

enum NewtonLinearSolverBackend {
	NEWTON_BACKEND_CL1 = 0,     /* no replacement: every call goes to ineq_cl1_original */
	NEWTON_BACKEND_PETSC = 1,
	NEWTON_BACKEND_LAPACK = 2   /* Task 1b; never compiled in this task */
};

struct NewtonLinearSolverOptions {
	NewtonLinearSolverBackend backend;
	bool print_statistics;
	bool compare_with_cl1;
	std::string comparison_log_path;
	std::string backend_name_as_given;   /* value of PHREEQC_LINEAR_SOLVER, or empty */
	bool backend_name_valid;             /* false if the name was unknown or not built in */
};

/* Reads the environment on every call (no caching):
     PHREEQC_LINEAR_SOLVER                    cl1 | petsc | lapack; unset or empty: build default
     PHREEQC_LINEAR_SOLVER_STATISTICS         exactly "1" enables statistics
     PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1   exactly "1" enables comparison mode
     PHREEQC_LINEAR_SOLVER_COMPARE_LOG        log path; unset or empty: newton_step_comparison.log
   An unknown name, or a backend not compiled into this build, gives backend = NEWTON_BACKEND_CL1
   and backend_name_valid = false. */
NewtonLinearSolverOptions read_newton_linear_solver_options(void);

/* "cl1", "petsc", "lapack"; "unknown" for any other value. */
const char *newton_linear_solver_backend_name(NewtonLinearSolverBackend backend);
```

### `newton_linear_solver_options.cpp`: required logic

```cpp
static NewtonLinearSolverBackend default_backend_of_this_build(void)
{
#if defined(USE_LAPACK_NEWTON_SOLVER)
	return NEWTON_BACKEND_LAPACK;
#elif defined(USE_PETSC)
	return NEWTON_BACKEND_PETSC;
#else
	return NEWTON_BACKEND_CL1;
#endif
}

static bool backend_is_compiled(NewtonLinearSolverBackend backend)
{
	switch (backend)
	{
	case NEWTON_BACKEND_CL1:
		return true;
	case NEWTON_BACKEND_PETSC:
#if defined(USE_PETSC)
		return true;
#else
		return false;
#endif
	case NEWTON_BACKEND_LAPACK:
#if defined(USE_LAPACK_NEWTON_SOLVER)
		return true;
#else
		return false;
#endif
	}
	return false;
}
```

`read_newton_linear_solver_options()` works like this:

- **Defaults:** start from `default_backend_of_this_build()` and `backend_name_valid = true`.
- **Flags:** `print_statistics` and `compare_with_cl1` are true only if the variable's value is exactly `"1"`, compared with `std::strcmp`.
- **Log path:** `comparison_log_path` takes the variable's value if it is set and non-empty, otherwise `"newton_step_comparison.log"`.
- **Backend:** if `PHREEQC_LINEAR_SOLVER` is set and non-empty, store it in `backend_name_as_given`, then match it exactly against `cl1`, `petsc`, `lapack`. If it matches and `backend_is_compiled()`, use that backend. Otherwise set `backend = NEWTON_BACKEND_CL1` and `backend_name_valid = false`.
- **Headers:** use only `<cstdlib>`, `<cstring>` and `<string>`.

### `newton_linear_solver_state.h`

```cpp
#pragma once
#ifdef USE_NEWTON_LINEAR_SOLVER
#include <cstdio>
#include <vector>
#include "phrqtype.h"
#include "linear_solve_status.h"
#include "newton_linear_solver_options.h"

#ifdef USE_PETSC
struct PetscNewtonSolver;
#endif

/* Why a call to ineq() did not use the replacement solver (decisions D8, D17). Same order and
   meaning as Phase 1; only the names changed. */
enum NewtonSolverIneligibilityReason {
	NEWTON_SOLVER_ELIGIBLE = 0,
	NEWTON_SOLVER_NOT_FIRST_ATTEMPT,
	NEWTON_SOLVER_REMOVING_UNSTABLE_PHASES,
	NEWTON_SOLVER_PITZER_OR_SIT_MODEL,
	NEWTON_SOLVER_MASS_WATER_SWITCH,
	NEWTON_SOLVER_AQUEOUS_ONLY_ITERATION,
	NEWTON_SOLVER_NUMERICAL_DERIVATIVES,
	NEWTON_SOLVER_EMPTY_SYSTEM,
	NEWTON_SOLVER_INELIGIBLE_UNKNOWN_TYPE,
	NEWTON_SOLVER_REASON_COUNT
};

struct NewtonLinearSolverState {
	NewtonLinearSolverOptions options;
	NewtonLinearSolverBackend backend = NEWTON_BACKEND_CL1;   /* effective backend */
#ifdef USE_PETSC
	PetscNewtonSolver *petsc_solver = nullptr;
#endif
	std::vector<LDBLE> candidate_newton_step;
	FILE *comparison_log_file = nullptr;
	unsigned long comparison_call_count = 0;
	double largest_relative_difference = 0.0;
	unsigned long ineligible_count_by_reason[NEWTON_SOLVER_REASON_COUNT] = {};
	unsigned long ineligible_system_count = 0;
	unsigned long solve_count = 0, accepted_count = 0, cl1_fallback_count = 0,
	              non_finite_input_count = 0;
	double last_backward_error = -1.0;   /* of the last solve; -1 if none */
};

NewtonLinearSolverState *create_newton_linear_solver_state(void);
void destroy_newton_linear_solver_state(NewtonLinearSolverState **state);
LinearSolveStatus solve_newton_step_with_backend(NewtonLinearSolverState *state,
	const LDBLE *augmented_newton_matrix, int count_unknowns, LDBLE *output);
void count_ineligible_newton_system(NewtonLinearSolverState *state, int reason);
int compare_newton_steps(const LDBLE *petsc_step, const LDBLE *cl1_step, int system_size,
	double *relative_difference, double *relative_sum);
void write_step_comparison_record(NewtonLinearSolverState *state, int system_size,
	LinearSolveStatus solve_status, int cl1_return_code, double backward_error,
	double relative_difference, double relative_sum, int largest_difference_index,
	int largest_difference_unknown_type);
#endif
```

### `newton_linear_solver_state.cpp`: required behaviour

- **`compare_newton_steps` and `write_step_comparison_record`** move unchanged from `petsc_solver_integration.cpp`, except for the type name of the state argument.
- **`create_newton_linear_solver_state()`**, in this order:
  1. Allocate with `new (std::nothrow)`, and return `nullptr` on failure.
  2. Set `options = read_newton_linear_solver_options()`. If `!options.backend_name_valid`, print one line to stderr:
     `phreeqc_linear_solver: warning: PHREEQC_LINEAR_SOLVER=<value> is unknown or not built in; using cl1`.
  3. Set `backend = options.backend`.
  4. Under `#ifdef USE_PETSC`, if `backend == NEWTON_BACKEND_PETSC`: call `create_petsc_newton_solver(&petsc_solver)`. If it fails, set `backend = NEWTON_BACKEND_CL1`. Otherwise set `petsc_solver->print_statistics = options.print_statistics`.
  5. If `options.compare_with_cl1` and `backend != NEWTON_BACKEND_CL1`: open `comparison_log_path` in append mode and write the same header line as Phase 1. If the open fails, print one warning and set `options.compare_with_cl1 = false`.
- **`solve_newton_step_with_backend()`**:
  1. Set `last_backward_error = -1.0`.
  2. Switch on `backend`. Under `#ifdef USE_PETSC`, the `NEWTON_BACKEND_PETSC` case calls `solve_newton_step_with_petsc(petsc_solver, …)` and then copies `petsc_solver->last_backward_error`. Every other case returns `LINEAR_SOLVE_FALL_BACK_TO_CL1`.
  3. Increment `solve_count`, then exactly one of `accepted_count`, `non_finite_input_count` or `cl1_fallback_count`, according to the returned status.
- **`count_ineligible_newton_system()`**: if `0 < reason < NEWTON_SOLVER_REASON_COUNT`, increment `ineligible_count_by_reason[reason]`. Always increment `ineligible_system_count`. Under `#ifdef USE_PETSC`, if `petsc_solver != nullptr`, also increment `petsc_solver->ineligible_system_count`, so that the PETSc statistics line stays as it was in Phase 1.
- **`destroy_newton_linear_solver_state()`**: safe on `nullptr`. Sets `*state = nullptr`. Then, in this order:
  1. If `options.print_statistics`, print to stderr:
     ```
     phreeqc_linear_solver_ineligible: not_first_attempt=%lu removing_unstable_phases=%lu pitzer_or_sit_model=%lu mass_water_switch=%lu aqueous_only_iteration=%lu numerical_derivatives=%lu empty_system=%lu ineligible_unknown_type=%lu
     phreeqc_linear_solver_statistics: backend=%s solves=%lu accepted=%lu cl1_fallbacks=%lu non_finite_inputs=%lu ineligible_systems=%lu
     ```
  2. If a comparison log is open, write `# summary comparison_calls=%lu largest_relative_difference=%.6e` and close it.
  3. Under `#ifdef USE_PETSC`, call `destroy_petsc_newton_solver(&petsc_solver)`, which prints its own `phreeqc_petsc_statistics:` line if statistics are on.
  4. `delete` the state.

### `src/Phreeqc.h`: renames only

| Phase 1 declaration | Task 1a declaration |
|---|---|
| `struct PetscSolverIntegrationState;` (before `class Phreeqc`) | `struct NewtonLinearSolverState;` |
| `int petsc_phase1a_ineligibility_reason(int in_kode) const;` | `int newton_solver_ineligibility_reason(int in_kode) const;` |
| `PetscSolverIntegrationState *petsc_solver_state = nullptr;` | `NewtonLinearSolverState *newton_linear_solver_state = nullptr;` |

Update the comments on those lines to match. `ineq_cl1_original` and the accessor `augmented_newton_matrix()` stay unchanged.

### `src/model.cpp`: eligibility function and dispatcher only

- Change `#ifdef USE_PETSC` to `#ifdef USE_NEWTON_LINEAR_SOLVER` around the include, the eligibility function and the dispatcher body. Include `newton_linear_solver_state.h` instead of `petsc_solver_integration.h`.
- **Eligibility function:** rename it to `newton_solver_ineligibility_reason`, and its return constants from `PETSC_PHASE1A_*` to `NEWTON_SOLVER_*`. Its conditions and their order do not change.
- **Dispatcher:** same structure as Phase 1, with these substitutions:
  - `petsc_solver_state` → `newton_linear_solver_state`;
  - `create_petsc_solver_integration_state()` → `create_newton_linear_solver_state()`;
  - the gate `solver_state->petsc_enabled` → `solver_state->backend != NEWTON_BACKEND_CL1`;
  - `count_petsc_ineligible_system(…)` → `count_ineligible_newton_system(…)`;
  - `solve_newton_step_with_petsc(solver_state->newton_solver, …)` → `solve_newton_step_with_backend(solver_state, …)`;
  - `solver_state->comparison_mode_enabled` → `solver_state->options.compare_with_cl1`;
  - `solver_state->newton_solver->last_backward_error` → `solver_state->last_backward_error`.

### `src/Phreeqc.cpp`

Replace the destructor's call and its include with `#ifdef USE_NEWTON_LINEAR_SOLVER` / `destroy_newton_linear_solver_state(&newton_linear_solver_state);` / `#endif`, and include `newton_linear_solver_state.h` under the same guard.

### `CMakeLists.txt`: inside `if(PHREEQC_USE_PETSC)` only

1. Rename the library `phreeqc_petsc_linear_solver` to `phreeqc_newton_linear_solver` everywhere in the block, including the test and benchmark targets.
2. Its sources become `petsc_initialization.cpp`, `newton_matrix_transfer.cpp`, `petsc_newton_solver.cpp`, `newton_linear_solver_options.cpp`, `newton_linear_solver_state.cpp`. Remove `petsc_solver_integration.cpp`.
3. Add `target_compile_definitions(phreeqc_newton_linear_solver PUBLIC USE_NEWTON_LINEAR_SOLVER)`, keeping the existing `PUBLIC USE_PETSC`.
4. Add the test `test_newton_linear_solver_options` with the same pattern as the other tests (`add_executable`, keyword `target_link_libraries … PRIVATE`, `add_test`).
5. The plain-form line `target_link_libraries(phreeqc phreeqc_newton_linear_solver)` replaces the old one.

### `test_newton_linear_solver_options.cpp`

Use `setenv`/`unsetenv` (POSIX) and `CHECK_OR_FAIL`. Before each case, unset all four variables. The cases:

1. Nothing set: `backend == NEWTON_BACKEND_PETSC`, `backend_name_valid`, `!print_statistics`, `!compare_with_cl1`, `comparison_log_path == "newton_step_comparison.log"`.
2. `PHREEQC_LINEAR_SOLVER=cl1` gives CL1 and valid. `=petsc` gives PETSC and valid.
3. `=lapack` gives CL1 and **not** valid, because LAPACK is not built in this task. `=bogus` gives CL1, not valid, and `backend_name_as_given == "bogus"`.
4. `PHREEQC_LINEAR_SOLVER_STATISTICS=1` makes `print_statistics` true. `=yes` and `=0` make it false. `PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1=1` makes `compare_with_cl1` true.
5. `PHREEQC_LINEAR_SOLVER_COMPARE_LOG=custom.log` gives that path. An empty value gives the default.
6. `newton_linear_solver_backend_name` returns `cl1`, `petsc`, `lapack`.

## Validation

1. **Source checks** (report the outputs):
   - `grep -n "petsc\|Petsc\|PETSC" src/petsc_linear_solver/newton_linear_solver_options.*` must print nothing.
   - `grep -n "#include" src/petsc_linear_solver/newton_linear_solver_state.*` must show PETSc headers only inside `#ifdef USE_PETSC`. Quote the lines around each one.
   - `git diff petsc-phase1a-task4 -- src/model.cpp`: every changed line must be inside the eligibility function, the dispatcher, or the include line. Report the full diff.
2. **OFF build:** `build/petsc_off`, ex1 and ex2 compared with the baseline as in Phase 1 (`cmp` for `.sel`, script for `.out`). All identical.
3. **ON build:** `build/petsc_on`, all targets. `ctest --test-dir build/petsc_on -R "test_initialization|test_newton_matrix_transfer|test_newton_solver|test_newton_linear_solver_options"`: 7 of 7 pass.
4. **Behaviour unchanged.** Run each of these in `build/petsc_on/task1a_<name>` with the README commands, changing only the executable path:
   - **`statistics`:** `PHREEQC_LINEAR_SOLVER_STATISTICS=1`, ex1 and ex2. The counts must equal Phase 1 Task 4:
     - ex1: `solves=8 accepted=8`, ineligible 0;
     - ex2: `solves=3 accepted=3`, `not_first_attempt=3 removing_unstable_phases=8 ineligible_unknown_type=1045`.

     Also `cmp` ex1's `.sel` against the baseline (it was byte-identical in Phase 1). Report all statistics lines.
   - **`cl1`:** `PHREEQC_LINEAR_SOLVER=cl1`, ex1 and ex2. Results identical to the baseline (`cmp`, script). No `phreeqc_petsc_statistics` solves.
   - **`compare`:** `PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1=1 PHREEQC_LINEAR_SOLVER_COMPARE_LOG=newton_step_comparison.log`, ex1. `.sel` identical to the baseline. Report the full output of `summarize_petsc_cl1_step_comparison.py` on the log; maximum relative difference ≤ 1e-15, no sign inversion.
   - **`bogus`:** `PHREEQC_LINEAR_SOLVER=bogus PHREEQC_LINEAR_SOLVER_STATISTICS=1`, ex1. Exactly one warning line; `backend=cl1 solves=0`; results identical to the baseline.
5. **Memory:** `PHREEQC_LINEAR_SOLVER_STATISTICS=1 PETSC_OPTIONS="-malloc_debug -malloc_dump"` on ex1. No unfreed-memory line other than the D16 exception.

## Report

Give pass or fail and the outputs for every validation step, the list of files created, changed, and deleted, the PETSc symbols used with their header locations (only if new ones were added), and any deviation from this specification with the reason for it. Stop after the report.
