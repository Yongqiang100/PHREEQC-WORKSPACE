# Task 4: PHREEQC Integration (Dispatcher, Eligibility Gate, Lifecycle, Compare Mode)

## Context

We are integrating PETSc into PHREEQC's Newton linear solve, Phase 1A: serial, dense LU only. This task connects the adapter (Task 1–Task 3) to PHREEQC.

The key principle: **the existing `cl1()` path stays exactly as it is.** PETSc is tried first, and only for calls that pass the eligibility rule in decision D8. Every other call, and every PETSc failure, goes to the original code unchanged.

**Required input:** `docs/petsc_linear_solver/task0_source_verification_report.md` and `docs/petsc_linear_solver/task0_reviewer_decisions.md`. The decisions file wins wherever the two differ. The facts this task relies on are:

| Fact | Value | Source |
|---|---|---|
| `ineq()` declaration | `src/Phreeqc.h:449` `int ineq(int kode);` (public section) | B3, B12 |
| `ineq()` definition | `src/model.cpp:927-928`, the name `ineq` is on line 928; body ends at `src/model.cpp:1922` | B3 |
| Success return value | `OK` (`#define OK 1`, `src/global_structures.h:36`) | B3 |
| Augmented Newton matrix | `std::vector<double>` member, identifier recorded in report B1 as `<UPSTREAM_NEWTON_MATRIX_NAME>` | B1 |
| `delta` | `std::vector<double>` member (`src/Phreeqc.h:1520`) | B2 |
| Unknown type | `x[i]->type`, constants in `src/global_structures.h:82-99` | B4 |
| Gate flags | `remove_unstable_phases`, `pitzer_model`, `sit_model`, `mass_water_switch`, `aqueous_only`, `iterations`: `Phreeqc` members readable from `ineq()` | B15 |
| Destructor | `src/Phreeqc.cpp:62-73` | B9 |
| `main()` | `src/class_main.cpp:31-89`; executable lines `src/class_main.cpp:87-88` | B10 |

If the source does not match a row of this table, stop and report.

**Prerequisites:** Task 1–Task 3 complete, and all their tests passing.

## Rules (apply to every task)

1. Touch only the files listed under Deliverables. If another file seems to need a change, stop and ask.
2. Before using any PETSc function, type, macro, or constant, grep `$PETSC_DIR/include` and `$PETSC_DIR/$PETSC_ARCH/include` for its declaration. Use the signature you find there. If it is not declared, stop and report. Never substitute a similar-looking name.
3. Code is C++. Functions called from PHREEQC code never throw and never abort.
4. All PETSc code compiles only with `PHREEQC_USE_PETSC=ON`. The deliberate exception is the set of declarations in `src/Phreeqc.h` described below. With the option OFF, results must match the baseline: `.sel` files byte for byte, `.out` files apart from the run-time lines (decision D11).
5. Never change tolerances, baseline files, or test inputs to make a test pass. Report failures with numbers.
6. If the code or environment contradicts this prompt, stop and report. Do not adapt the design yourself.
7. Use Plan mode first. Present the plan, and implement only after approval.
8. Do not run `git commit`.

## Deliverables

1. `src/petsc_linear_solver/petsc_solver_integration.h`
2. `src/petsc_linear_solver/petsc_solver_integration.cpp` (add it to `phreeqc_petsc_linear_solver` in the top-level `CMakeLists.txt`)
3. `src/Phreeqc.h`: one forward declaration and five declarations inside the class (below)
4. `src/model.cpp`: the rename on line 928, the dispatcher, the eligibility function, and one `#include`
5. `src/Phreeqc.cpp`: one call at the start of the destructor, and one `#include`
6. `src/class_main.cpp`: replace lines 87-88 of `main()`, and one `#include`

No other file changes. In particular, no constructor, `init()`, `InternalCopy()`, or copy assignment operator is edited (decision D9).

## Specification

### Declarations in `src/Phreeqc.h` (unconditional)

If a member existed only under `#ifdef USE_PETSC`, translation units compiled with and without the definition would see different class layouts. That is undefined behaviour, and it shows up as random memory corruption. The declarations below are therefore **not** inside `#ifdef USE_PETSC`.

Immediately before the line `class Phreeqc` (`src/Phreeqc.h:74`), at namespace scope:

```cpp
struct PetscSolverIntegrationState;
```

Immediately after `int ineq(int kode);` (`src/Phreeqc.h:449`), inside the class:

```cpp
	/* PETSc Phase 1A integration (see docs/petsc_linear_solver/). */
	int ineq_cl1_original(int kode);   /* the original ineq() body, renamed, otherwise unmodified */
	int petsc_phase1a_ineligibility_reason(int in_kode) const;   /* defined only when USE_PETSC */
	/* Read-only view of the augmented Newton matrix [J | r]. This accessor is the ONLY place in
	   new code where the upstream identifier appears (report B1); it is a std::vector. */
	const LDBLE *augmented_newton_matrix(void) const { return <UPSTREAM_NEWTON_MATRIX_NAME>.data(); }
	/* Opaque; nullptr unless the PETSc path has been used. The default member initializer
	   gives nullptr in every constructor, including the user-defined copy constructor. */
	PetscSolverIntegrationState *petsc_solver_state = nullptr;
```

Replace `<UPSTREAM_NEWTON_MATRIX_NAME>` with the identifier from report B1. All new code, comments, and log messages refer to this array as the *augmented Newton matrix* or through `augmented_newton_matrix()`. Do not write the upstream identifier anywhere else. `LDBLE` is `double` in this build (report B7), so the accessor's return type matches the vector's element type; if it does not compile, stop and report.

### `petsc_solver_integration.h` / `.cpp`

```cpp
#pragma once
#ifdef USE_PETSC
#include <cstdio>
#include <vector>
#include "petsc_newton_solver.h"

/* Why a call to ineq() did not use PETSc (decision D8). Checked in this order. */
enum PetscPhase1aIneligibilityReason {
    PETSC_PHASE1A_ELIGIBLE = 0,
    PETSC_PHASE1A_NOT_FIRST_ATTEMPT,           /* in_kode != 1: retry after cl1 returned 2 (D6) */
    PETSC_PHASE1A_REMOVING_UNSTABLE_PHASES,    /* remove_unstable_phases set (D4)              */
    PETSC_PHASE1A_PITZER_OR_SIT_MODEL,         /* pitzer_model or sit_model (D4)               */
    PETSC_PHASE1A_MASS_WATER_SWITCH,           /* mass_water_switch set (D4)                   */
    PETSC_PHASE1A_AQUEOUS_ONLY_ITERATION,      /* iterations < aqueous_only (D4)               */
    PETSC_PHASE1A_NUMERICAL_DERIVATIVES,       /* numerical derivatives active (D7)            */
    PETSC_PHASE1A_EMPTY_SYSTEM,                /* count_unknowns <= 0                          */
    PETSC_PHASE1A_INELIGIBLE_UNKNOWN_TYPE,     /* an unknown outside the allow-list (D3)       */
    PETSC_PHASE1A_REASON_COUNT
};

struct PetscSolverIntegrationState {
    PetscNewtonSolver *newton_solver = nullptr;
    std::vector<LDBLE> candidate_newton_step;      /* PETSc writes here, never directly into delta */
    bool  petsc_enabled = false;                   /* false if PETSc is unusable or disabled       */
    bool  comparison_mode_enabled = false;         /* -phreeqc_petsc_compare_with_cl1              */
    FILE *comparison_log_file = nullptr;           /* -phreeqc_petsc_comparison_log <path>         */
    unsigned long comparison_call_count = 0;
    double largest_relative_difference = 0.0;      /* largest normwise relative difference seen    */
    unsigned long ineligible_count_by_reason[PETSC_PHASE1A_REASON_COUNT] = {};
};

/* Increments ineligible_count_by_reason[reason] and newton_solver->ineligible_system_count. */
void count_petsc_ineligible_system(PetscSolverIntegrationState *state, int reason);

/* Never returns nullptr unless allocation fails. If PETSc is unusable, returns a state
   with petsc_enabled == false, so creation is not retried on every Newton iteration. */
PetscSolverIntegrationState *create_petsc_solver_integration_state(void);

/* If newton_solver->print_statistics is set, first prints one line to stderr (fprintf):
     phreeqc_petsc_ineligible: not_first_attempt=%lu removing_unstable_phases=%lu
       pitzer_or_sit_model=%lu mass_water_switch=%lu aqueous_only_iteration=%lu
       numerical_derivatives=%lu empty_system=%lu ineligible_unknown_type=%lu
   (on one line). Then writes the comparison summary if comparison mode is on, closes the
   log, destroys the Newton solver (destroy_petsc_newton_solver), deletes the state, and
   sets *state = nullptr. Safe on nullptr. */
void destroy_petsc_solver_integration_state(PetscSolverIntegrationState **state);

/* Normwise comparison of the PETSc step with the cl1 step, both of length system_size:
     relative_difference = max |petsc_step - cl1_step| / max |cl1_step|
     relative_sum        = max |petsc_step + cl1_step| / max |cl1_step|
                           (close to 0 means the PETSc step has the opposite sign)
   If max |cl1_step| == 0, both are set to max |petsc_step|.
   Returns the index of the component with the largest |petsc_step - cl1_step|. */
int compare_newton_steps(const LDBLE *petsc_step, const LDBLE *cl1_step, int system_size,
                         double *relative_difference, double *relative_sum);

/* Appends one record to the comparison log. */
void write_step_comparison_record(PetscSolverIntegrationState *state, int system_size,
                                  LinearSolveStatus solve_status, int cl1_return_code,
                                  double backward_error, double relative_difference,
                                  double relative_sum, int largest_difference_index,
                                  int largest_difference_unknown_type);
#endif
```

`create_petsc_solver_integration_state`:

1. Create the state.
2. Call `create_petsc_newton_solver(&state->newton_solver)`. If it fails, return the state with `petsc_enabled = false`.
3. Holding `petsc_global_state_mutex()`, read these options:
   - `-phreeqc_petsc_enable` (a bool; default **true**);
   - `-phreeqc_petsc_compare_with_cl1` (whether it is present);
   - `-phreeqc_petsc_comparison_log` (a string; default `petsc_cl1_step_comparison.log`).
4. If comparison mode is on, open the log in append mode and write a header line. If opening fails, print one warning to `stderr` and turn comparison mode off.

Comparison-log format: whitespace-separated columns, with a header line starting with `#`. Use `%.6e` for floating-point values.

```
# call_index system_size solve_status cl1_return_code backward_error relative_difference relative_sum largest_difference_index largest_difference_unknown_type
```

- `call_index` counts comparison records from 1.
- `solve_status` is the integer value of `LinearSolveStatus`.
- When `solve_status` is not `LINEAR_SOLVE_ACCEPTED`, write `backward_error`, `relative_difference` and `relative_sum` as `nan`, and `largest_difference_index` and `largest_difference_unknown_type` as -1.

`summarize_petsc_cl1_step_comparison.py` reads exactly these nine columns.

### Eligibility function (in `src/model.cpp`, inside `#ifdef USE_PETSC`)

This implements decision D8 exactly, in the same order.

```cpp
#ifdef USE_PETSC
int Phreeqc::
petsc_phase1a_ineligibility_reason(int in_kode) const
{
	if (in_kode != 1)
		return PETSC_PHASE1A_NOT_FIRST_ATTEMPT;
	if (remove_unstable_phases == TRUE)
		return PETSC_PHASE1A_REMOVING_UNSTABLE_PHASES;
	if (pitzer_model == TRUE || sit_model == TRUE)
		return PETSC_PHASE1A_PITZER_OR_SIT_MODEL;
	if (mass_water_switch == TRUE)
		return PETSC_PHASE1A_MASS_WATER_SWITCH;
	if (iterations < aqueous_only)
		return PETSC_PHASE1A_AQUEOUS_ONLY_ITERATION;
	if (numerical_deriv)   /* see the check below */
		return PETSC_PHASE1A_NUMERICAL_DERIVATIVES;
	if (count_unknowns <= 0)
		return PETSC_PHASE1A_EMPTY_SYSTEM;
	for (int i = 0; i < count_unknowns; i++)
	{
		switch (x[i]->type)
		{
		case MB: case ALK: case CB: case SOLUTION_PHASE_BOUNDARY:
		case MU: case AH2O: case MH: case MH2O:
			break;
		default:
			return PETSC_PHASE1A_INELIGIBLE_UNKNOWN_TYPE;
		}
	}
	return PETSC_PHASE1A_ELIGIBLE;
}
#endif
```

**Check before writing the `numerical_deriv` line.** Read `src/model.cpp:138-150` and quote, in the plan, the condition that selects the call `numerical_jacobian()` at `src/model.cpp:144` (the branch without `jacobian_sums()`). Use exactly that condition here. If it is not a single `Phreeqc` member, stop and report.

### Rename and dispatcher

1. **Rename.** In the definition of `ineq()`, change **only the function name** on `src/model.cpp:928`, from `ineq` to `ineq_cl1_original`. Do not change a single other character of the original function: no reformatting, no comment changes, no fixes.
2. **Dispatcher.** Add a new `Phreeqc::ineq()` with the original signature, directly above `ineq_cl1_original`:

```cpp
int Phreeqc::ineq(int in_kode)
{
#ifdef USE_PETSC
    if (petsc_solver_state == nullptr) petsc_solver_state = create_petsc_solver_integration_state();
    PetscSolverIntegrationState *solver_state = petsc_solver_state;
    if (solver_state != nullptr && solver_state->petsc_enabled) {
        const int ineligibility_reason = petsc_phase1a_ineligibility_reason(in_kode);
        if (ineligibility_reason != PETSC_PHASE1A_ELIGIBLE) {
            count_petsc_ineligible_system(solver_state, ineligibility_reason);
        } else {
            solver_state->candidate_newton_step.resize((size_t)count_unknowns);
            /* PETSc reads the augmented Newton matrix BEFORE ineq_cl1_original runs
               (cl1 may modify its inputs) and writes only to candidate_newton_step. */
            LinearSolveStatus solve_status = solve_newton_step_with_petsc(
                solver_state->newton_solver, augmented_newton_matrix(), count_unknowns,
                solver_state->candidate_newton_step.data());
            if (solver_state->comparison_mode_enabled) {   /* cl1 steers; PETSc result is only logged */
                int cl1_return_code = ineq_cl1_original(in_kode);
                double relative_difference = 0.0, relative_sum = 0.0;
                int largest_difference_index = -1;
                if (solve_status == LINEAR_SOLVE_ACCEPTED)
                    largest_difference_index = compare_newton_steps(
                        solver_state->candidate_newton_step.data(), delta.data(), count_unknowns,
                        &relative_difference, &relative_sum);
                const int largest_difference_unknown_type =
                    largest_difference_index >= 0 ? (int)x[largest_difference_index]->type : -1;
                write_step_comparison_record(
                    solver_state, count_unknowns, solve_status, cl1_return_code,
                    (double)solver_state->newton_solver->last_backward_error,
                    relative_difference, relative_sum,
                    largest_difference_index, largest_difference_unknown_type);
                return cl1_return_code;
            }
            if (solve_status == LINEAR_SOLVE_ACCEPTED) {
                for (int i = 0; i < count_unknowns; ++i) delta[i] = solver_state->candidate_newton_step[i];
                return OK;   /* success value from Task 0 item B3 */
            }
            /* Any other status, including LINEAR_SOLVE_NON_FINITE_INPUT, falls through to cl1. */
        }
    }
#endif
    return ineq_cl1_original(in_kode);
}
```

Place the eligibility function and the dispatcher directly above the renamed `ineq_cl1_original` definition. Add `#include "petsc_solver_integration.h"` inside `#ifdef USE_PETSC` after the existing `#include` lines at the top of `src/model.cpp`.

### Object lifecycle (decision D9)

The default member initializer in `src/Phreeqc.h` already makes the pointer `nullptr` in every constructor. Neither the user-defined copy constructor nor the copy assignment operator copies it. **Do not edit any constructor, `init()`, `initialize()`, `InternalCopy()`, or `operator=`.**

The only lifecycle change is in the destructor `Phreeqc::~Phreeqc(void)` (`src/Phreeqc.cpp:62-73`). Insert this as its first statement, before `clean_up();`:

```cpp
#ifdef USE_PETSC
	destroy_petsc_solver_integration_state(&petsc_solver_state);
#endif
```

Add `#include "petsc_solver_integration.h"` inside `#ifdef USE_PETSC` after the existing `#include` lines at the top of `src/Phreeqc.cpp`.

### `main()` of the phreeqc executable (decision D10)

Replace exactly `src/class_main.cpp:87-88`:

```cpp
	Phreeqc phreeqc_instance;
	return phreeqc_instance.main_method(argc, argv);
```

with:

```cpp
	int return_code;
#ifdef USE_PETSC
	const bool petsc_pinned = (acquire_petsc() == 0);
#endif
	{
		/* Scope ends before PETSc shutdown, so the Phreeqc object and its PETSc state are
		   destroyed first. */
		Phreeqc phreeqc_instance;
		return_code = phreeqc_instance.main_method(argc, argv);
	}
#ifdef USE_PETSC
	if (petsc_pinned)
	{
		release_petsc();
		(void)shutdown_petsc_if_owned_by_phreeqc();
	}
#endif
	return return_code;
```

Keep the existing indentation style of the file (tabs). Add `#include "petsc_initialization.h"` inside `#ifdef USE_PETSC` after the existing `#include` lines at the top of `src/class_main.cpp`. Leave lines 31-86 unchanged.

## Validation

1. **Rename check.** Run `git diff -U0 -- src/model.cpp` and include the full output in the report. Inside the old function body (originally `src/model.cpp:928-1922`), the only changed line allowed is line 928 (`ineq` → `ineq_cl1_original`); everything else must be additions. Any modified line inside the old body is a failure: report it.
2. **OFF build.** Build in `build/petsc_off` as in Task 1 step 1. Run ex1 and ex2 with the commands from `tests/petsc_linear_solver/baseline/README.md`, using `build/petsc_off/phreeqc` and the working directory `build/petsc_off/run`. Then:
   - `cmp` each of `ex1_seawater_speciation.sel`, `ex2_gypsum_anhydrite_equilibria.sel`, `ex2.sel` against the baseline copy: all identical;
   - run `python3 tests/petsc_linear_solver/tools/compare_main_output_to_baseline.py` on both `.out` files against the baseline: both `IDENTICAL`.
3. **ON build.** Build in `build/petsc_on` as in Task 1 step 2. All earlier tests must still pass: `ctest --test-dir build/petsc_on -R "test_initialization|test_newton_matrix_transfer|test_newton_solver" --output-on-failure`.
4. **ON smoke run.** Run ex1 and ex2 with `build/petsc_on/phreeqc`, working directory `build/petsc_on/smoke`, and `PETSC_OPTIONS="-phreeqc_petsc_statistics"`. Both runs must complete with exit status 0. Each must print a `phreeqc_petsc_ineligible:` line and a `phreeqc_petsc_statistics:` line on `stderr`; report all of them. **Do not compare results with the baseline in this task; that is Task 5.**
5. **ON with PETSc disabled at runtime.** Run ex1 and ex2 with `build/petsc_on/phreeqc`, working directory `build/petsc_on/disabled`, and `PETSC_OPTIONS="-phreeqc_petsc_enable false"`. Every solve then goes through cl1, so the results must match the baseline exactly as in step 2 (`cmp` for `.sel`, the comparison script for `.out`).

## Report

Include the following, then stop. Do not start Task 5.

- Pass/fail and output for each validation step.
- The full `git diff -U0` from step 1.
- The list of files changed.
- Any deviations from this prompt, and the reason for each.
