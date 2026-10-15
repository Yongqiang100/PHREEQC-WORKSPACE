# Task 3: Solver Context, Size-Keyed Cache, Dense LU Solve

## Context

PETSc is being integrated into PHREEQC's Newton linear solve. This is Phase 1A: serial, dense LU only. This task adds the per-instance solver context, which owns:

- a cache of PETSc objects keyed by system size (`count_unknowns` varies between cells);
- the direct solve;
- the acceptance test;
- counters.

**No PHREEQC source file is modified in this task.**

**Prerequisites:** Task 1 and Task 2 must be complete: `petsc_initialization.*`, `linear_solve_status.h` and `newton_matrix_transfer.*` must exist.

## Rules (apply to every task)

1. Touch only the files listed under Deliverables. If another file seems to need a change, stop and ask.
2. Before using any PETSc function, type, macro, or constant, grep `$PETSC_DIR/include` and `$PETSC_DIR/$PETSC_ARCH/include` for its declaration. Use the signature you find. If it is not declared, stop and report. Never substitute a similar-looking name.
3. Code is C++.
   - Inside functions returning `PetscErrorCode`, use `PetscFunctionBeginUser` / `PetscCall(...)` / `PetscFunctionReturn(PETSC_SUCCESS)`.
   - Functions called from PHREEQC code never throw and never abort.
4. All PETSc code compiles only with `PHREEQC_USE_PETSC=ON`. With the option OFF, the build must be unchanged.
5. Never change tolerances, baseline files, or test inputs to make a test pass. Report failures with numbers.
6. If the code or environment contradicts this prompt, stop and report. Do not adapt the design yourself.
7. Use Plan mode first. Present the plan, and implement only after approval.
8. Do not run `git commit`.

## Deliverables

1. `src/petsc_linear_solver/petsc_newton_solver.h`
2. `src/petsc_linear_solver/petsc_newton_solver.cpp`
3. `tests/petsc_linear_solver/test_newton_solver.cpp` and `tests/petsc_linear_solver/test_newton_solver_after_petsc_finalize.cpp`
4. CMake changes:
   - add `petsc_newton_solver.cpp` to `phreeqc_petsc_linear_solver`;
   - add `test_newton_solver` and `test_newton_solver_after_petsc_finalize` executables and tests.

## Design Decisions (do not change)

- **Solver configuration:** `MATSEQDENSE` + `KSPPREONLY` + `PCLU` with natural ordering. This is LAPACK LU with partial pivoting. Geochemical Jacobians are small, fairly dense and very ill-conditioned. PETSc's native sparse ILU/LU does not pivot, so it is not used.
- **Object lifetime:** objects are created **once per size** and reused. They are never created per solve.
- **Options parsing:** `KSPSetFromOptions` runs **once per slot**, at creation. It is never called per solve.
- **Locking:** slot creation and destruction take `petsc_global_state_mutex()`. The solve path takes no lock.
- **Memory:** host-side buffers use `std::vector`, not `PetscMalloc`, so the context can be freed safely even after PETSc has been finalized.
- **Acceptance:** a step is accepted only if the KSP converged reason is > 0 **and** the normwise backward error η ≤ `max_backward_error`.

## Specification

### `petsc_newton_solver.h`

```cpp
#pragma once
#ifdef USE_PETSC
#include <vector>
#include <petscksp.h>
#include "newton_matrix_transfer.h"
#include "linear_solve_status.h"

#define SOLVER_SLOT_COUNT 8

struct SizedSolverSlot {
    PetscInt system_size = 0;                        /* 0 = empty */
    Mat jacobian_matrix = NULL;
    Vec rhs_vector = NULL, solution_vector = NULL, residual_work_vector = NULL;
    KSP linear_solver = NULL;
    std::vector<PetscInt>    column_index_buffer;
    std::vector<PetscScalar> row_value_buffer;
    unsigned long last_used_tick = 0;
};

struct PetscNewtonSolver {
    SizedSolverSlot   slots[SOLVER_SLOT_COUNT];
    SizedSolverSlot  *active_slot = nullptr;
    bool      use_row_equilibration   = true;          /* -phreeqc_row_equilibration    */
    PetscReal max_backward_error     = 1.0e-10;       /* -phreeqc_max_backward_error      */
    bool      print_statistics = false;         /* -phreeqc_petsc_statistics  */
    unsigned long use_tick = 0;
    PetscReal last_backward_error = -1.0;             /* backward error of the last solve, -1 if none */
    unsigned long solve_count = 0, accepted_count = 0, cl1_fallback_count = 0, non_finite_input_count = 0,
                  petsc_error_count = 0, slot_creation_count = 0, ineligible_system_count = 0;
};

/* Acquires PETSc (acquire_petsc) and reads options. 0 on success, -1 on failure
   (then *solver_out == nullptr and PETSc is not held). */
int  create_petsc_newton_solver(PetscNewtonSolver **solver_out);

/* Prints statistics if enabled, destroys all PETSc objects (only if PETSc is not yet
   finalized), deletes the context, then calls release_petsc(). Sets *solver = nullptr.
   Safe on nullptr. */
void destroy_petsc_newton_solver(PetscNewtonSolver **solver);

/* Solves the system described by layout. On LINEAR_SOLVE_ACCEPTED, output[0..system_size-1] holds the solution.
   On any other status, output is unchanged. Never returns a PETSc error. */
LinearSolveStatus solve_newton_system(PetscNewtonSolver *solver, const DenseMatrixLayout *layout, PetscInt system_size, LDBLE *output);

/* Convenience wrapper: solve_newton_system(solver, phreeqc_newton_matrix_layout(augmented_newton_matrix, system_size), system_size, output). */
LinearSolveStatus solve_newton_step_with_petsc(PetscNewtonSolver *solver, const LDBLE *augmented_newton_matrix, int count_unknowns,
                            LDBLE *output);
#endif
```

### `petsc_newton_solver.cpp`: required implementation

**Slot creation and destruction** (both called only while holding `petsc_global_state_mutex()`):

```cpp
static PetscErrorCode create_solver_slot(SizedSolverSlot *slot, PetscInt system_size)
{
    PC preconditioner;
    PetscFunctionBeginUser;
    slot->system_size = system_size;
    PetscCall(MatCreateSeqDense(PETSC_COMM_SELF, system_size, system_size, NULL, &slot->jacobian_matrix));
    PetscCall(MatCreateVecs(slot->jacobian_matrix, &slot->solution_vector, &slot->rhs_vector));
    PetscCall(VecDuplicate(slot->rhs_vector, &slot->residual_work_vector));
    slot->column_index_buffer.assign((size_t)system_size, 0);
    slot->row_value_buffer.assign((size_t)system_size, 0.0);
    PetscCall(KSPCreate(PETSC_COMM_SELF, &slot->linear_solver));
    PetscCall(KSPSetOptionsPrefix(slot->linear_solver, "phreeqc_"));   /* isolates from host options */
    PetscCall(KSPSetType(slot->linear_solver, KSPPREONLY));
    PetscCall(KSPGetPC(slot->linear_solver, &preconditioner));
    PetscCall(PCSetType(preconditioner, PCLU));
    PetscCall(PCFactorSetMatOrderingType(preconditioner, MATORDERINGNATURAL));
    PetscCall(KSPSetInitialGuessNonzero(slot->linear_solver, PETSC_FALSE));
    PetscCall(KSPSetErrorIfNotConverged(slot->linear_solver, PETSC_FALSE));
    PetscCall(KSPSetFromOptions(slot->linear_solver));                 /* once per slot */
    PetscCall(KSPSetOperators(slot->linear_solver, slot->jacobian_matrix, slot->jacobian_matrix));
    PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode destroy_solver_slot(SizedSolverSlot *slot)   /* safe on a partially built slot */
{
    PetscFunctionBeginUser;
    PetscCall(KSPDestroy(&slot->linear_solver));
    PetscCall(MatDestroy(&slot->jacobian_matrix));
    PetscCall(VecDestroy(&slot->rhs_vector));
    PetscCall(VecDestroy(&slot->solution_vector));
    PetscCall(VecDestroy(&slot->residual_work_vector));
    slot->column_index_buffer.clear(); slot->row_value_buffer.clear();
    slot->system_size = 0; slot->last_used_tick = 0;
    PetscFunctionReturn(PETSC_SUCCESS);
}
```

**Size binding.** The rules are: prefer a slot with matching size; otherwise use an empty slot; otherwise evict the least recently used slot.

```cpp
static PetscErrorCode select_solver_slot_for_size(PetscNewtonSolver *solver, PetscInt system_size)
{
    PetscFunctionBeginUser;
    ++solver->use_tick;
    if (solver->active_slot && solver->active_slot->system_size == system_size) { solver->active_slot->last_used_tick = solver->use_tick; PetscFunctionReturn(PETSC_SUCCESS); }
    SizedSolverSlot *slot_to_replace = &solver->slots[0];
    for (int slot_index = 0; slot_index < SOLVER_SLOT_COUNT; ++slot_index) {
        SizedSolverSlot *slot = &solver->slots[slot_index];
        if (slot->system_size == system_size) { solver->active_slot = slot; slot->last_used_tick = solver->use_tick; PetscFunctionReturn(PETSC_SUCCESS); }
        if (slot_to_replace->system_size == 0) continue;                       /* already have an empty slot */
        if (slot->system_size == 0 || slot->last_used_tick < slot_to_replace->last_used_tick) slot_to_replace = slot;
    }
    solver->active_slot = nullptr;
    {
        std::lock_guard<std::mutex> state_lock(petsc_global_state_mutex());   /* RAII: released on every return */
        PetscCall(destroy_solver_slot(slot_to_replace));
        PetscErrorCode error_code = create_solver_slot(slot_to_replace, system_size);
        if (error_code != PETSC_SUCCESS) { (void)destroy_solver_slot(slot_to_replace); PetscCall(error_code); }
    }
    slot_to_replace->last_used_tick = solver->use_tick;
    solver->active_slot = slot_to_replace;
    ++solver->slot_creation_count;
    PetscFunctionReturn(PETSC_SUCCESS);
}
```

**Backward error.**

```cpp
/* backward_error = ||rhs - A*solution||_inf / (||A||_inf * ||solution||_inf + ||rhs||_inf).
   Do NOT add a constant such as PETSC_SMALL to the denominator: PETSC_SMALL is 1e-10 in
   double precision and would make backward_error artificially small when ||solution|| and ||rhs|| are tiny
   (near Newton convergence), accepting bad solves. */
static PetscErrorCode compute_normwise_backward_error(SizedSolverSlot *slot, PetscReal *backward_error)
{
    PetscReal residual_norm, matrix_norm, solution_norm, rhs_norm, denominator;
    PetscFunctionBeginUser;
    PetscCall(MatMult(slot->jacobian_matrix, slot->solution_vector, slot->residual_work_vector));   /* work = A * solution */
    PetscCall(VecAYPX(slot->residual_work_vector, -1.0, slot->rhs_vector));   /* work = rhs - A * solution */
    PetscCall(VecNorm(slot->residual_work_vector, NORM_INFINITY, &residual_norm));
    PetscCall(MatNorm(slot->jacobian_matrix, NORM_INFINITY, &matrix_norm));
    PetscCall(VecNorm(slot->solution_vector, NORM_INFINITY, &solution_norm));
    PetscCall(VecNorm(slot->rhs_vector, NORM_INFINITY, &rhs_norm));
    denominator = matrix_norm * solution_norm + rhs_norm;
    if (denominator > 0.0)    *backward_error = residual_norm / denominator;
    else                *backward_error = (residual_norm == 0.0) ? 0.0 : PETSC_MAX_REAL;
    PetscFunctionReturn(PETSC_SUCCESS);
}
```

**Direct solve.**

```cpp
static PetscErrorCode run_direct_lu_solve(PetscNewtonSolver *solver, LinearSolveStatus *solve_status)
{
    SizedSolverSlot *slot = solver->active_slot;
    KSPConvergedReason converged_reason;
    PetscFunctionBeginUser;
    *solve_status = LINEAR_SOLVE_FALL_BACK_TO_CL1;
    solver->last_backward_error = -1.0;
    PetscCall(KSPSetOperators(slot->linear_solver, slot->jacobian_matrix, slot->jacobian_matrix));
    PetscCall(KSPSolve(slot->linear_solver, slot->rhs_vector, slot->solution_vector));
    PetscCall(KSPGetConvergedReason(slot->linear_solver, &converged_reason));
    if (converged_reason <= 0) PetscFunctionReturn(PETSC_SUCCESS);   /* e.g. KSP_DIVERGED_PC_FAILED */
    PetscCall(compute_normwise_backward_error(slot, &solver->last_backward_error));
    if (solver->last_backward_error <= solver->max_backward_error) *solve_status = LINEAR_SOLVE_ACCEPTED;
    PetscFunctionReturn(PETSC_SUCCESS);
}
```

Depending on the PETSc version, a zero pivot in dense LU arrives in one of two ways:

- reported through `KSPGetConvergedReason` (reason ≤ 0); or
- raised as an error code from `KSPSolve`.

Both must end as `LINEAR_SOLVE_FALL_BACK_TO_CL1`. The second case is handled by the error path in `solve_newton_system`.

**Top-level solve.** This function never returns a PETSc error.

```cpp
LinearSolveStatus solve_newton_system(PetscNewtonSolver *solver, const DenseMatrixLayout *layout, PetscInt system_size, LDBLE *output)
{
    LinearSolveStatus solve_status = LINEAR_SOLVE_FALL_BACK_TO_CL1;
    PetscBool finite = PETSC_FALSE;
    ++solver->solve_count;
    if (system_size <= 0) { ++solver->cl1_fallback_count; return LINEAR_SOLVE_FALL_BACK_TO_CL1; }
    if (select_solver_slot_for_size(solver, system_size) != PETSC_SUCCESS) goto petsc_error;
    if (load_newton_system_into_petsc(solver->active_slot->jacobian_matrix, solver->active_slot->rhs_vector, layout, system_size, solver->use_row_equilibration ? PETSC_TRUE : PETSC_FALSE,
                       solver->active_slot->column_index_buffer.data(), solver->active_slot->row_value_buffer.data(), &solve_status) != PETSC_SUCCESS)
        goto petsc_error;
    if (solve_status == LINEAR_SOLVE_ACCEPTED && run_direct_lu_solve(solver, &solve_status) != PETSC_SUCCESS) goto petsc_error;
    if (solve_status == LINEAR_SOLVE_ACCEPTED) {
        if (copy_solution_from_petsc(solver->active_slot->solution_vector, output, system_size, 0, &finite) != PETSC_SUCCESS) goto petsc_error;
        if (!finite) solve_status = LINEAR_SOLVE_FALL_BACK_TO_CL1;
    }
    if      (solve_status == LINEAR_SOLVE_ACCEPTED)    ++solver->accepted_count;
    else if (solve_status == LINEAR_SOLVE_NON_FINITE_INPUT) ++solver->non_finite_input_count;
    else                         ++solver->cl1_fallback_count;
    return solve_status;
petsc_error:
    /* The slot may be in an inconsistent state (e.g. an array still checked out):
       discard it so the next call rebuilds it. */
    if (solver->active_slot) {
        std::lock_guard<std::mutex> state_lock(petsc_global_state_mutex());
        (void)destroy_solver_slot(solver->active_slot);
        solver->active_slot = nullptr;
    }
    ++solver->petsc_error_count;
    ++solver->cl1_fallback_count;
    return LINEAR_SOLVE_FALL_BACK_TO_CL1;
}
```

**Creation and destruction.**

- `create_petsc_newton_solver`:
  1. Call `acquire_petsc()`; on failure return -1.
  2. Allocate the context with `new (std::nothrow) PetscNewtonSolver()`; on failure, release and return -1.
  3. Then, holding `petsc_global_state_mutex()`, read the options with `PetscOptionsGetBool(NULL, NULL, "-phreeqc_row_equilibration", ...)`, `PetscOptionsGetReal(NULL, NULL, "-phreeqc_max_backward_error", ...)` and `PetscOptionsHasName(NULL, NULL, "-phreeqc_petsc_statistics", ...)`.
  4. Ignore option-read errors and keep the defaults.
  - **Never** hold the mutex while calling `acquire_petsc()`.
- `destroy_petsc_newton_solver`:
  1. Holding the mutex, call `PetscFinalized(&petsc_is_finalized)`.
  2. If `print_statistics` is set, print one line to `stderr` with `fprintf` (not PETSc printing):
     `phreeqc_petsc_statistics: solves=%lu accepted=%lu cl1_fallbacks=%lu non_finite_inputs=%lu petsc_errors=%lu ineligible_systems=%lu solver_slots_created=%lu`
  3. If `!petsc_is_finalized`, destroy all slots.
  4. Release the mutex, `delete` the context, then call `release_petsc()`.

## Tests (`test_newton_solver.cpp`)

**Setup and conventions:**

- Each test creates its own context with `create_petsc_newton_solver` and destroys it with `destroy_petsc_newton_solver`.
- At program end, call `shutdown_petsc_if_owned_by_phreeqc()` and check that it returns 0.
- Use a `CHECK_OR_FAIL(condition)` macro, not `assert`.
- Before each solve, prefill `output` with the sentinel 12345.0. Whenever the status is not `LINEAR_SOLVE_ACCEPTED`, check that `output` still holds only sentinel values.

**Test data:**

- **3 × 3 system:** `augmented_newton_matrix_3x3` from Task 2, with `expected_newton_step = {1, -1, 2}`.
- **4 × 4 system,** also non-symmetric:

  ```cpp
  /* expected_jacobian_4x4 = [2 0 1 0; 1 3 0 0; 0 1 4 1; 0 0 1 5]
     expected_newton_step_4x4 = {1, 2, -1, 1}, expected_rhs_4x4 = {1, 7, -1, 4}
     row-major, leading_dimension = 5, RHS in column 4 */
  static const LDBLE augmented_newton_matrix_4x4[4 * 5] = {
      2.0, 0.0, 1.0, 0.0,   1.0,
      1.0, 3.0, 0.0, 0.0,   7.0,
      0.0, 1.0, 4.0, 1.0,  -1.0,
      0.0, 0.0, 1.0, 5.0,   4.0,
  };
  ```

- **Generated systems** for `system_size` = 2 to 10:
  - Jacobian entries: (row, row) = 4; (row, row+1) = 1; (row+1, row) = 2; all others 0.
  - `expected_newton_step[row] = (row % 3) - 1`.
  - The test computes the RHS as Jacobian × `expected_newton_step` and stores the system in PHREEQC layout.

**Tests:**

1. **3 × 3 solve.** Run once with `use_row_equilibration = true` and once with `false` (set the field directly). Both runs must give `LINEAR_SOLVE_ACCEPTED`, every component within 1e-12 of `expected_newton_step`, and `last_backward_error ≤ 1e-14`. Print the solution and `last_backward_error`.
2. **Badly scaled row.** Multiply row 1, including its RHS, by 1e-12, with `use_row_equilibration = true`. Expect `LINEAR_SOLVE_ACCEPTED` and the same solution within 1e-12.
3. **Zero row.** Expect `LINEAR_SOLVE_FALL_BACK_TO_CL1`, with `output` unchanged.
4. **NaN entry.** Expect `LINEAR_SOLVE_NON_FINITE_INPUT`, `output` unchanged, and `non_finite_input_count == 1`.
5. **Exactly singular matrix.** Jacobian [[1, 1], [1, 1]] with RHS {1, 1}.
   - Expect `LINEAR_SOLVE_FALL_BACK_TO_CL1`, no crash, and `output` unchanged.
   - Report whether the zero pivot was reported through the converged reason or through the error path, i.e. whether `petsc_error_count` was incremented.
6. **Solver slot reuse.** Alternate the 3 × 3 and 4 × 4 systems, 10 times each. Every solve must be correct, and `slot_creation_count` must equal 2.
7. **Solver slot eviction.** Cycle twice through the generated systems for `system_size` = 2 to 10 (9 sizes, 8 slots). Every solve must be correct. Under least-recently-used replacement every access misses, so `slot_creation_count` must equal 18.
8. **Recovery after eviction.** After test 7, solve the 3 × 3 system again. Expect `LINEAR_SOLVE_ACCEPTED` and the correct solution.
9. **Statistics.** Set `print_statistics = true` and confirm that `destroy_petsc_newton_solver` prints exactly one line in the format above.
10. **Lifecycle (in `test_newton_solver`).** Create two solvers, solve once with each, then destroy both. `petsc_reference_count()` must be 0.
11. **After finalize** (separate executable `test_newton_solver_after_petsc_finalize.cpp`, registered as its own CMake test):
    1. Call `acquire_petsc()` as an outer pin, create a solver, and solve the 3 × 3 system once.
    2. Destroy the solver, call `release_petsc()` for the outer pin, then call `shutdown_petsc_if_owned_by_phreeqc()`, which must return 0.
    3. Call `create_petsc_newton_solver()`. It must return -1 and leave `*solver_out == nullptr`, because PETSc cannot be re-initialised.

## Validation

1. Build with OFF: unchanged, as in Task 1 step 1.
2. Build with ON in `build/petsc_on`, then run `ctest --test-dir build/petsc_on -R "test_initialization|test_newton_matrix_transfer|test_newton_solver" --output-on-failure`. All tests must pass.
3. Memory check: run `test_newton_solver` with PETSc's own allocation tracking: `PETSC_OPTIONS="-malloc_debug -malloc_dump" build/petsc_on/test_newton_solver`. Report the full output. Any line reporting memory that was still allocated at `PetscFinalize` is a failure: it means a PETSc object was not destroyed. Do not use valgrind: it cannot execute the installed PETSc library (AVX-512 instructions, decision D14). Also run `test_newton_solver_after_petsc_finalize` the same way. Exception (decision D16): one line reporting `PetscFPTrapPush()` in `src/sys/error/fp.c` is expected in `test_newton_solver`, because test 5 takes PETSc's error path; report it, but it is not a failure. Any other line is.

## Report

- Pass or fail for each test, with the solution and `last_backward_error` printed for tests 1, 2 and 6.
- The answer to the question in test 5.
- The files changed.
- Each PETSc symbol used, with its header location.
- Any deviations from this prompt, with reasons.

Stop after the report. Do not start Task 4.
