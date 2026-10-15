# Phase 1A Implementation Prompt (Superseded)

> **Superseded.** Use the task prompts in `docs/petsc_linear_solver/implementation_tasks/` (start with `README.md`). This single prompt is kept only as a scope summary and lacks the Revision 3 amendments.

You are implementing **Phase 1A** of the PETSc integration described in `petsc_serial_design.md` (Revision 2). That document is the specification. Where this prompt and the document disagree, stop and ask; do not choose.

## Step 0: Verify before writing any code

Read the PHREEQC source and report findings for design §10 items **1, 3, 4, 5, 6, 11, 12, 13**. For each item, quote the relevant lines (file and line numbers) and state whether the design's assumption holds.

In particular:

- **Item 1:** confirm the augmented Newton matrix is row-major, 0-based, with leading dimension `count_unknowns + 1` and the residual in column `count_unknowns`.
- **Item 11:** list the unknown-type constants and their names exactly as defined.
- **Item 12:** state whether `ineq()` adds, drops, or rescales any rows when every unknown is one of `MB, ALK, CB, MU, AH2O, MH, MH2O`.

**Stop after Step 0 and wait for approval** if any of the following holds:

- the layout differs from design §4.2;
- any allow-listed type is treated specially by `ineq()`;
- `ineq()` has side effects beyond `delta` that callers use.

## Scope (implement only this)

1. `src/petsc_linear_solver/petsc_initialization.h/.cpp`: `acquire_petsc()`, `release_petsc()`, `petsc_global_state_mutex()`, finalize policy (design §5.1). Use `PetscInitializeNoArguments()`; never pass `argv` to PETSc.
2. `src/petsc_linear_solver/petsc_newton_solver.h/.cpp`: context struct, 8-slot size-keyed LRU cache, slot create/destroy under `std::lock_guard` on `petsc_global_state_mutex()` (design §3.2, §5.4). Dense LU only: `MATSEQDENSE`, `KSPPREONLY`, `PCLU`, natural ordering, options prefix `phreeqc_`, `KSPSetFromOptions()` once per slot. Solve, classify (`reason > 0` plus backward error ≤ `1e-10`, design §6.1), counters.
3. `src/petsc_linear_solver/newton_matrix_transfer.h/.cpp`: `DenseMatrixLayout` descriptor and `layout_jacobian_entry()` (design §4.2); row-scaled load via per-row `MatSetValues()` with NaN/Inf and zero-row detection (design §4.3); store to a scratch buffer with a finiteness check (design §4.5).
4. In PHREEQC: rename the existing body of `ineq()` to `ineq_cl1_original()` **without modifying it**, add the Phase 1A dispatcher and `is_eligible_for_petsc_phase1a()` exactly as in design §3.4, add the per-instance context pointer and scratch buffer, and add acquire/release in the `Phreeqc` constructor and destructor (design §5.2). Copies of a `Phreeqc` object must set the context pointer to `NULL`.
5. `-phreeqc_petsc_compare_with_cl1` δ-comparison mode (design §8.3, without rcond and bound fields).
6. `-phreeqc_petsc_statistics` counter printout at instance destruction, including the eligibility-gate bypass count.
7. CMake option `PHREEQC_USE_PETSC` (default `OFF`) using pkg-config, applied to the real target names found in Step 0.
8. `tests/petsc_linear_solver/test_newton_matrix_transfer.cpp`: the asymmetric 3×3 unit test and variants (a)–(e) from design §4.3.

## Out of scope (do not implement)

AIJ/GMRES, the direct-retry KSP, inexact acceptance, `step_satisfies_cl1_bounds()`, circuit breaker, rcond estimation, the dense fast path (`MatDenseGetArrayWrite`), any pre-factorisation condition-number gate, and OpenMP thread support beyond `nthreads = 1`.

## Rules

- Adapter code is C++. Use `PetscCall()` inside functions returning `PetscErrorCode`. At the boundary with PHREEQC, convert any non-zero PETSc error code into a fall-through to `ineq_cl1_original()`. Never abort, never throw.
- In Phase 1A, every status other than `LINEAR_SOLVE_ACCEPTED` falls through to `ineq_cl1_original()`, including `LINEAR_SOLVE_NON_FINITE_INPUT`.
- PETSc reads the augmented Newton matrix **before** `ineq_cl1_original()` runs and writes only to scratch. `delta` is written only after the step is accepted.
- Do not change PETSc dense indexing. Element (i, j) of a `MATSEQDENSE` array is at `j*lda + i`.
- With `PHREEQC_USE_PETSC=OFF`, compiled behaviour must be unchanged.
- Do **not** change tolerances, baseline files, or test inputs to make a comparison pass. Report mismatches instead.

## Validation (run in this order and report each)

1. Build with the option OFF. Run ex1 and ex2. Results must match the unmodified baseline **exactly**.
2. Build with the option ON. Run the unit tests; all must pass.
3. ex1 with `-phreeqc_petsc_statistics`. Expect `accepted_count == solve_count` and zero bypasses. Compare `SELECTED_OUTPUT` (`-high_precision true`) against baseline numerically: pass if |new − ref| ≤ 1e-6·|ref| + atol, with atol = 1e-14 for amounts and molalities and 1e-6 for pH, pe, SI and log activities. Report the maximum deviation per quantity and the Newton iteration counts for both runs.
4. ex2 with `-phreeqc_petsc_statistics`. Expect bypass count > 0 (pure-phase calculations must use `cl1`). Same tolerance comparison.
5. ex1 with `-phreeqc_petsc_compare_with_cl1`. Report the distribution of max relative δ differences and any unknown type with systematic discrepancies.
6. Valgrind or ASan on ex1 and ex2 with the option ON: no leaks, no invalid accesses, no PETSc object destroyed after `PetscFinalize()`.

## Report format

For each step above, give pass/fail and the numbers, followed by a list of files changed with a one-line description each. List any deviation from the design and the reason for it.

Do not start Phase 1B work.
