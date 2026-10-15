# Task 2: Legacy-Array Loader and Layout Unit Tests

## Context

This is PETSc integration into PHREEQC's Newton linear solve, Phase 1A (serial, dense LU only). This task adds the code that copies PHREEQC's Jacobian and residual from a legacy C array into a PETSc matrix and vector, and copies the solution back. It also adds unit tests that would catch a transposed or off-by-one load.

No solver objects are involved in this task, and no PHREEQC source file is modified.

**Required input:** `docs/petsc_linear_solver/task0_source_verification_report.md` (approved). The layout of the augmented Newton matrix (Task 0 item B1) must be: row-major, leading dimension `count_unknowns + 1`, RHS in column `count_unknowns`, 0-based. `LDBLE` comes from the header named in item B7.

Task 1 must be complete. The library `phreeqc_petsc_linear_solver` and the global init functions `acquire_petsc`, `release_petsc` and `shutdown_petsc_if_owned_by_phreeqc` must exist.

## Rules (apply to every task)

1. Touch only the files listed under Deliverables. If another file seems to need a change, stop and ask.
2. Before using any PETSc function, type, macro, or constant, grep `$PETSC_DIR/include` and `$PETSC_DIR/$PETSC_ARCH/include` for its declaration and use the signature you find. If it is not declared, stop and report. Never substitute a similar-looking name.
3. Code is C++. Inside functions returning `PetscErrorCode`, use `PetscFunctionBeginUser` / `PetscCall(...)` / `PetscFunctionReturn(PETSC_SUCCESS)`. Functions called from PHREEQC code never throw and never abort.
4. All PETSc code compiles only with `PHREEQC_USE_PETSC=ON`. With the option OFF, the build is unchanged.
5. Never change tolerances, baseline files, or test inputs to make a test pass. Report failures with numbers.
6. If the code or environment contradicts this prompt, stop and report. Do not adapt the design yourself.
7. Use Plan mode first, present the plan, and implement only after approval.
8. Do not run `git commit`.

## Deliverables

1. `src/petsc_linear_solver/linear_solve_status.h`, which defines the status enum.
2. `src/petsc_linear_solver/newton_matrix_transfer.h`
3. `src/petsc_linear_solver/newton_matrix_transfer.cpp`
4. `tests/petsc_linear_solver/test_newton_matrix_transfer.cpp`
5. CMake: add `newton_matrix_transfer.cpp` to `phreeqc_petsc_linear_solver`, and add a `test_newton_matrix_transfer` executable and test, using the same pattern as Task 1.

## Two Indexing Problems, Kept Separate

- **Storage offset.** This is how a logical element (row, column) maps to a memory address. It covers row-major vs column-major order, the leading dimension, and whether row or column 0 is unused padding (C code that emulates 1-based arrays). All of this is handled **only** inside `layout_jacobian_entry()` and `layout_rhs_entry()`.
- **Index-valued data.** These are arrays whose contents are indices. They are not used in this task.

Logical `row` and `column` indices are **always 0-based**, matching PETSc row and column numbers.

## Specification

### `linear_solve_status.h` (no PETSc include)

```cpp
#pragma once
enum LinearSolveStatus {
    LINEAR_SOLVE_ACCEPTED = 0,       /* PETSc step accepted                                   */
    LINEAR_SOLVE_FALL_BACK_TO_CL1,   /* use the existing cl1 path for this Newton iteration   */
    LINEAR_SOLVE_NON_FINITE_INPUT    /* NaN or Inf in the input; in Phase 1A also falls back  */
};
```

### `newton_matrix_transfer.h`

```cpp
#pragma once
#ifdef USE_PETSC
#include <petscmat.h>
#include "phrqtype.h"          /* LDBLE (use the header named in Task 0 item B7) */
#include "linear_solve_status.h"

struct DenseMatrixLayout {
    const LDBLE *data;                  /* base pointer of the legacy storage                    */
    PetscInt     leading_dimension;     /* stride in elements: between rows if row-major,
                                           between columns if column-major                       */
    PetscInt     padding_rows;          /* 1 if storage reserves row 0 (padded 1-based), else 0  */
    PetscInt     padding_columns;       /* 1 if storage reserves column 0, else 0                */
    PetscBool    is_column_major;       /* PETSC_TRUE for Fortran-ordered storage                */
    PetscInt     rhs_column;            /* 0-based logical column of the RHS inside data, or -1  */
    const LDBLE *separate_rhs;          /* separate RHS vector when rhs_column < 0, else NULL    */
    PetscInt     separate_rhs_padding;  /* 1 if the separate RHS vector is padded 1-based        */
};

/* row and column are 0-based logical indices. This is the only place where storage
   offsets (padding, stride, orientation) are applied. */
static inline PetscReal layout_jacobian_entry(const DenseMatrixLayout *layout,
                                              PetscInt row, PetscInt column)
{
    const PetscInt storage_row    = row + layout->padding_rows;
    const PetscInt storage_column = column + layout->padding_columns;
    const LDBLE value = layout->is_column_major
        ? layout->data[storage_column * layout->leading_dimension + storage_row]
        : layout->data[storage_row * layout->leading_dimension + storage_column];
    return (PetscReal)value;   /* explicit narrowing if LDBLE is long double */
}

static inline PetscReal layout_rhs_entry(const DenseMatrixLayout *layout, PetscInt row)
{
    return (layout->rhs_column >= 0)
        ? layout_jacobian_entry(layout, row, layout->rhs_column)
        : (PetscReal)layout->separate_rhs[row + layout->separate_rhs_padding];
}

/* Layout of PHREEQC's augmented Newton matrix (verified in Task 0 item B1). */
DenseMatrixLayout phreeqc_newton_matrix_layout(const LDBLE *augmented_newton_matrix,
                                               int count_unknowns);

/* Loads a square system of size system_size into an existing MATSEQDENSE jacobian_matrix
   and a Vec rhs_vector of the same size.
   Pass 1: every Jacobian and RHS entry must be finite; otherwise
           *solve_status = LINEAR_SOLVE_NON_FINITE_INPUT.
   Pass 2: for each row, row_max_abs = max over columns of |J(row, column)|.
           If row_max_abs == 0: *solve_status = LINEAR_SOLVE_FALL_BACK_TO_CL1, stop loading.
           Otherwise row_scale_factor = use_row_equilibration ? 1 / row_max_abs : 1;
           set the row of jacobian_matrix to row_scale_factor * J(row, :) with one
           MatSetValues call (INSERT_VALUES, all columns), and set
           rhs_vector(row) = row_scale_factor * rhs(row).
   Always restores the vector array and assembles jacobian_matrix (MAT_FINAL_ASSEMBLY)
   before returning PETSC_SUCCESS, including on early exit, so the matrix is always
   in an assembled state.
   column_index_buffer and row_value_buffer are caller-provided, length >= system_size. */
PetscErrorCode load_newton_system_into_petsc(Mat jacobian_matrix, Vec rhs_vector,
                                             const DenseMatrixLayout *layout,
                                             PetscInt system_size,
                                             PetscBool use_row_equilibration,
                                             PetscInt *column_index_buffer,
                                             PetscScalar *row_value_buffer,
                                             LinearSolveStatus *solve_status);

/* Copies solution_vector (length system_size) into output[row + output_padding].
   Checks that ALL entries are finite FIRST; if any is not, sets
   *solution_is_finite = PETSC_FALSE and leaves output completely unchanged. */
PetscErrorCode copy_solution_from_petsc(Vec solution_vector, LDBLE *output,
                                        PetscInt system_size, PetscInt output_padding,
                                        PetscBool *solution_is_finite);
#endif
```

### `newton_matrix_transfer.cpp`: required details

- `phreeqc_newton_matrix_layout()` returns `leading_dimension = count_unknowns + 1`, `padding_rows = padding_columns = 0`, `is_column_major = PETSC_FALSE`, `rhs_column = count_unknowns`, `separate_rhs = NULL`, and `separate_rhs_padding = 0`.
- Detect non-finite values with `PetscIsInfOrNanReal`.
- Row scaling divides each row **and** its RHS entry by the same factor. This leaves the solution unchanged.
- Use `MatSetValues` only. Do **not** use `MatDenseGetArray*` direct writes in Phase 1A.
- Use `VecGetArrayWrite`/`VecRestoreArrayWrite` for `b`, and `VecGetArrayRead`/`VecRestoreArrayRead` in `copy_solution_from_petsc`.

## Test Data

These systems are deliberately **non-symmetric**. A transposed load would solve with the transpose of the Jacobian, which gives a different answer.

```cpp
/* expected_jacobian  = [ 1 2 0 ; 0 3 4 ; 5 0 6 ]
   expected_newton_step = { 1, -1, 2 }
   expected_rhs       = expected_jacobian * expected_newton_step = { -1, 5, 17 }
   expected_row_max_abs = { 2, 4, 6 }
   Stored as PHREEQC stores the augmented Newton matrix:
   row-major, leading_dimension = 4, RHS in column 3. */
static const LDBLE augmented_newton_matrix_3x3[3 * 4] = {
    1.0, 2.0, 0.0,  -1.0,
    0.0, 3.0, 4.0,   5.0,
    5.0, 0.0, 6.0,  17.0,
};
```

## Tests (`test_newton_matrix_transfer.cpp`)

**Setup and conventions:**

- Call `acquire_petsc()` at the start. At the end, call `release_petsc()` and then `shutdown_petsc_if_owned_by_phreeqc()`.
- Create `jacobian_matrix` with `MatCreateSeqDense(PETSC_COMM_SELF, system_size, system_size, NULL, &jacobian_matrix)`, and create `solution_vector` and `rhs_vector` with `MatCreateVecs`.
- Read entries back with `MatGetValues` and `VecGetValues`.
- Use a `CHECK_OR_FAIL(condition)` macro that prints the condition and line and returns 1 from `main`. Do not use `assert`.
- Destroy all PETSc objects before release.

**Tests:**

1. **Exact load, no row equilibration.**
   - Load through `phreeqc_newton_matrix_layout(augmented_newton_matrix_3x3, 3)`.
   - All nine entries of `jacobian_matrix` must equal `expected_jacobian` exactly, and `rhs_vector` must equal `expected_rhs` exactly.
   - Check these entries explicitly as transposition detectors: entry (0,1) == 2 and entry (1,0) == 0; entry (2,0) == 5 and entry (0,2) == 0.
2. **Load with row equilibration.** Each entry (row, column) must equal `expected_jacobian(row, column) / expected_row_max_abs[row]`, and each `rhs_vector(row)` must equal `expected_rhs[row] / expected_row_max_abs[row]`, within relative tolerance 1e-15.
3. **Padded 1-based storage.**
   - Build `padded_storage` as a 4 × 5 array with `padding_rows = padding_columns = 1`, `leading_dimension = 5`, `rhs_column = 3`.
   - Store logical (row, column) at `padded_storage[(row + 1) * 5 + (column + 1)]` and the RHS at `padded_storage[(row + 1) * 5 + 4]`.
   - Fill every padding element (row 0 and column 0) with NaN.
   - The result must equal test 1, with status `LINEAR_SOLVE_ACCEPTED`. This proves padding is never read.
4. **Column-major storage with a separate RHS.**
   - Build `column_major_storage[column * 3 + row] = expected_jacobian(row, column)`, with `leading_dimension = 3`, `is_column_major = PETSC_TRUE`, `rhs_column = -1`, `separate_rhs = expected_rhs`, `separate_rhs_padding = 0`.
   - The result must equal test 1.
5. **Zero row.**
   - Set row 1 of the Jacobian to zeros, keeping its RHS non-zero.
   - Expect `LINEAR_SOLVE_FALL_BACK_TO_CL1`, and `MatAssembled(jacobian_matrix, &is_assembled)` must give `is_assembled == PETSC_TRUE`.
6. **Non-finite input.** Each of these cases gives `LINEAR_SOLVE_NON_FINITE_INPUT`, and in every case `jacobian_matrix` is assembled:
   - NaN at Jacobian entry (2,1);
   - Inf in the RHS of row 0;
   - zero row 0 combined with NaN at entry (2,2), because non-finite input takes precedence over a zero row.
7. **Copying the solution out.**
   - Set `solution_vector = {1, -1, 2}`. With `output_padding = 0`, `output` equals the solution. With `output_padding = 1`, `output[1..3]` equals the solution and `output[0]` is untouched.
   - Set `solution_vector[1] = Inf`. Expect `*solution_is_finite == PETSC_FALSE`, and `output` must be unchanged (prefill it with the sentinel 12345).
8. **Layout fields.** `phreeqc_newton_matrix_layout(any_pointer, 3)` returns `leading_dimension == 4`, `rhs_column == 3`, `padding_rows == padding_columns == 0`, and `is_column_major == PETSC_FALSE`.

## Validation

1. Build with OFF; the build must be unchanged, as in Task 1 step 1.
2. Build with ON, then run `ctest --test-dir build/petsc_on -R "test_initialization|test_newton_matrix_transfer" --output-on-failure`. Everything must pass.
3. Memory check: run `test_newton_matrix_transfer` with PETSc's own allocation tracking: `PETSC_OPTIONS="-malloc_debug -malloc_dump" build/petsc_on/test_newton_matrix_transfer`. Report the full output. Any line reporting memory that was still allocated at `PetscFinalize` is a failure: it means a PETSc object was not destroyed. Do not use valgrind: it cannot execute the installed PETSc library (AVX-512 instructions, decision D14).

## Report

Give pass or fail per test with values printed for tests 1–4, then a list of files changed. List each PETSc symbol used with its header location. List any deviation from this prompt and the reason for it.

Stop after the report. Do not start Task 3.
