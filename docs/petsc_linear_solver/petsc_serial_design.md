# PETSc Integration into PHREEQC / PhreeqcRM: Phase 1 (Serial) Design

| Field | Value |
|---|---|
| Scope | Replace the per-iteration linear solve inside PHREEQC's Newton-Raphson loop with PETSc. Serial only (`PETSC_COMM_SELF`). No MPI-distributed objects. |
| Targets | `phreeqc` executable, IPhreeqc library, PhreeqcRM (C/C++/Fortran APIs) |
| PETSc version | ≥ 3.19 (uses `PetscCall`, `PETSC_SUCCESS`, `KSPGetConvergedReasonString`, `PCGetFailedReason`) |
| Status | Revision 5 (2026-10-11). Items marked **[VERIFY]** must be confirmed against your PHREEQC source tree before coding. |

### Revision History

| Rev | Changes |
|---|---|
| 1 | Initial design. |
| 2 | (a) §4.3: clarified that the dense fast-path index `Ad[j*lda + i]` is already correct column-major indexing (`i + j*lda` is the same expression); added a mandatory asymmetric 3×3 unit test. (b) §6.6: added a *post*-factorisation reciprocal-condition check, log-only until calibrated; a *pre*-factorisation condition gate is explicitly rejected. (c) §5.4: slot creation and destruction now take the global mutex in code, not only in prose; adapter sources are C++ (`.cpp`). (d) §3.4: new Phase 1A eligibility gate restricting PETSc to systems with no bounded unknowns. (e) §8: validation uses relative tolerances instead of exact matches; example selection corrected (ex1 speciation, ex2 gate check, ex6 phase boundaries, ex8 surface complexation). (f) §11: phased rollout 1A/1B/1C. Appendix B: Phase 1A implementation prompt. |
| 3 | Implementation amendments found while splitting Phase 1A into tasks (§12): opaque state pointer so `Phreeqc` class layout never depends on `USE_PETSC`; PETSc acquired per solver context and never finalized on last release (explicit shutdown in `main()`, `atexit` for library hosts); loader decoupled from the cache with a finiteness pre-pass; **bug fix** in the backward-error denominator (§6.3); normwise δ comparison with sign-inversion check (§8.3). Phase 1A is implemented through the task specifications in `docs/petsc_linear_solver/implementation_tasks/`, launched by the OpenCode commands in `.opencode/commands/`. |
| 4 | Naming: all new files, types, functions, variables, options, tests, and result directories renamed to self-explanatory names (§12.6). No behavioural change. |
| 5 | Task 0 findings applied (§13): this tree has only the `phreeqc` executable; eligibility rule extended (ALK and SOLUTION_PHASE_BOUNDARY eligible; retry calls, Pitzer/SIT, `mass_water_switch`, aqueous-only iterations, unstable-phase removal and numerical derivatives excluded); state pointer via default member initializer; `.out` comparison ignores run-time lines. |

---

## 0. Executive Summary

Three findings change the plan as originally framed. Each is justified in detail below.

1. **PHREEQC does not solve a plain linear system.** The Newton step is computed in `ineq()`, which assembles an inequality-constrained problem and solves it with `cl1()` (a Barrodale–Roberts L1-minimisation simplex). The inequality rows enforce physical bounds (a mineral cannot dissolve more than is present, an absent phase may only precipitate if supersaturated, etc.), and the L1 formulation tolerates rank-deficient Jacobians. A Krylov or LU solve cannot reproduce either property. **Phase 1 should therefore replace the equality-constrained sub-solve with PETSc and keep `cl1()` as a mandatory fallback**, not delete it.

2. **The proposed default (SEQAIJ + GMRES + ILU) is the wrong default for this problem class.** Geochemical Jacobians are small (typically 10–100 unknowns, occasionally a few hundred), moderately dense, and extremely ill-conditioned. PETSc's native AIJ ILU/LU performs no numerical pivoting, so zero or tiny diagonals (common in charge-balance, alkalinity, and phase rows) cause breakdown. **Recommended default: `MATSEQDENSE` + `KSPPREONLY` + `PCLU`** (LAPACK `getrf`, partial pivoting). SEQAIJ + GMRES + ILU remains available at runtime through the options database.

3. **Create once, reuse always.** Objects are created lazily once per PHREEQC instance and reused across Newton iterations *and* across cells, with a small size-keyed cache because `count_unknowns` varies between cells. Per-iteration create/destroy would plausibly make solver overhead exceed the cost of the factorisation itself.

A fourth note for planning: chemistry is cell-local, so even in Phase 2 (MPI) the chemistry solver objects should stay on `PETSC_COMM_SELF`. MPI in PhreeqcRM distributes *cells*, not matrices (see §9).

Implementation is staged (§11). Phase 1A uses PETSc only for systems whose unknowns are all pure equalities (§3.4), so the bound logic of `cl1()` cannot be bypassed before it has been audited and reproduced.

Before writing integration code, profile a representative PhreeqcRM run and record the fraction of wall time spent in `cl1()`. In many PHREEQC workloads the dominant cost is species distribution, activity coefficients, and `jacobian_sums()`, not the linear solve; that fraction bounds the achievable speed-up (Amdahl).

---

## 1. Background: How PHREEQC Computes the Newton Step

The relevant call chain in PHREEQC 3.x (`model.cpp`) is, in simplified form:

```
model()
  └─ loop over Newton iterations
       ├─ molalities() / mb_sums() / ...       species distribution
       ├─ residuals()                          residual vector r, stored in RHS column
       ├─ jacobian_sums()                      Jacobian J and residual r, stored in the augmented Newton matrix
       ├─ ineq(kode)                           builds constrained problem → cl1() → delta[]
       └─ reset()                              step limiting, applies delta to unknowns
```

Key data structures:

| Symbol | Meaning |
|---|---|
| `count_unknowns` (n) | Number of unknowns in the current system. Varies by cell and can change when phases/unknowns are added or removed and the model is re-set-up. |
| Augmented Newton matrix [J \| r] | `LDBLE*`, dense, **row-major, 0-based**, dimension `n × (n+1)`. Element J(i,j) at offset `i*(n+1) + j`; residual r(i) in column `n` at offset `i*(n+1) + n`. Its upstream identifier is recorded during source verification (Task 0 item B1) and is accessed in new code only through `Phreeqc::augmented_newton_matrix()` (§12.1). **[VERIFY]** in `residuals()` / `jacobian_sums()`. |
| `delta` | `LDBLE*`, length n, 0-based. Output of `ineq()`, consumed by `reset()`. |
| `x[i]` | Unknown descriptors (type: mass balance, charge balance, mu, ah2o, pure phase, surface, gas, solid solution, ...). Determines which rows become constraints. |
| `LDBLE` | `double` by default; `long double` if built with `USE_LONG_DOUBLE`. **[VERIFY]** for your build. |

Unknowns are mostly natural logarithms of master-species activities, which already gives column scaling of sorts. Rows, however, mix mole-unit mass balances (spanning ~10⁻¹⁵ to 10¹ mol), the charge balance, the ionic-strength equation, and saturation-index equations. Row magnitudes therefore span many orders, and 2-norm condition numbers of 10¹⁰–10²⁰ are routine near convergence or for trace components.

Implications for the design:

- The PETSc solve must reproduce exactly the **equality subsystem** that `ineq()` would hand to `cl1()`, with the same sign convention, and then check that the bound constraints `cl1()` would have enforced are satisfied.
- Any outputs of `ineq()` other than `delta` that downstream code consumes must also be produced on the PETSc path. **[VERIFY]**
- Rank deficiency (e.g. a phase-rule violation, redundant constraints, an element with zero total) is a normal event for PHREEQC and must route to `cl1()`, not abort.

---

## 2. PETSc Object Selection

### 2.1 Problem Characteristics

| Property | Typical value | Consequence |
|---|---|---|
| Dimension n | 10–100, rarely > 300 | Dense direct factorisation costs O(n³) ≈ 10³–10⁶ flops: microseconds. Krylov machinery overhead is comparable to or larger than the work. |
| Density | Often a large fraction of n² (charge balance, mu, ah2o, and mass-water rows/columns couple to nearly everything) | Sparse storage saves little; ILU(0) on a near-dense pattern is close to full LU anyway. |
| Conditioning | κ ≈ 10¹⁰–10²⁰ | Iterative methods hit an attainable-accuracy floor of roughly ε·κ relative to ‖b‖; LU with partial pivoting is backward-stable. |
| Diagonal | Zero or tiny diagonals occur (phase rows, charge balance) | Non-pivoting factorisations (PETSc native AIJ ILU/LU) break down with zero pivots. |
| Rank | Can be deficient | Needs a fallback (cl1 L1-minimisation, or SVD for diagnostics). |
| Repetition | 10⁷–10⁹ solves per PhreeqcRM run | Per-solve fixed overhead dominates; object reuse is mandatory. |

### 2.2 Evaluation of the Proposed SEQAIJ + GMRES + ILU

The proposal is a sensible default for large, sparse, discretised PDE systems. For PHREEQC Jacobians it has four concrete weaknesses:

1. **No pivoting.** PETSc's built-in SeqAIJ ILU and LU factorisations do not perform numerical row pivoting (only shift strategies and diagonal-nonzero reordering). A zero pivot yields `KSP_DIVERGED_PC_FAILED`; a tiny pivot yields a numerically useless preconditioner.
2. **Accuracy floor.** With κ ≈ 10¹² and double precision, the true relative residual GMRES can reliably reach is around 10⁻⁴ in bad cases. Tight `rtol` settings then produce `KSP_DIVERGED_ITS` rather than a better answer.
3. **Overhead.** For n = 30, one dense LU and triangular solve is roughly 2×10⁴ flops. GMRES with orthogonalisation, ILU setup, and norm computations costs more than that, before counting PETSc's per-call bookkeeping.
4. **Preconditioned-norm trap.** GMRES defaults to left preconditioning and monitors the *preconditioned* residual, which for a poor ILU can look converged while the true residual is not.

### 2.3 Recommended Configurations

| # | Mat / KSP / PC | Role | Rationale |
|---|---|---|---|
| **A (default)** | `MATSEQDENSE` / `KSPPREONLY` / `PCLU` (natural ordering) | Production default for all n up to a few hundred | LAPACK `getrf` with partial pivoting; backward-stable; deterministic; no tolerance tuning; lowest overhead. |
| B | `MATSEQDENSE` / `KSPRICHARDSON` (max_it 2–3) / `PCLU` | Escalation when the backward-error check fails | Iterative refinement. Corrects factorisation instability (pivot growth). It improves forward accuracy only if the residual is computed in higher precision (a possible later use of `long double`), so treat it as a stability fix, not a conditioning fix. |
| C | `MATSEQAIJ` / `KSPGMRES` (right PC, restart ≥ n) / `PCILU` (levels 1–2, `MAT_SHIFT_NONZERO`) | Runtime-selectable alternative, research/benchmarking | Your original proposal, hardened. Use right preconditioning so the monitored norm is the true residual; full (non-restarted) GMRES since n is small; shift to survive zero pivots. |
| D | `MATSEQAIJ` / `KSPPREONLY` / `PCLU` with `-pc_factor_mat_solver_type umfpack` (or `superlu`, `mumps`) | Only if n grows into the many hundreds with genuine sparsity | Sparse direct with numerical pivoting. Requires PETSc built with the external package. |
| E | `MATSEQDENSE` / `KSPPREONLY` / `PCSVD` | Debug/diagnostics only | Pseudo-inverse via SVD; reports singular values. Useful to characterise rank-deficient cells; too expensive for production. |

**Decision: implement A as default and C as a runtime alternative from day one** (the code paths differ only in the Mat type and how zeros are inserted). B, D, and E are options-database changes and require no extra code beyond what is specified here.

### 2.4 Runtime Selectability

All PHREEQC solver objects use the options prefix `phreeqc_`. This isolates PHREEQC from a host application that itself uses PETSc (for example a PETSc-based transport code) and whose `-ksp_type` must not leak into the chemistry solver.

```
# Default (A): nothing needed
# Alternative (C):
-phreeqc_mat_type seqaij -phreeqc_ksp_type gmres -phreeqc_pc_type ilu
-phreeqc_pc_factor_levels 1 -phreeqc_pc_factor_shift_type nonzero
-phreeqc_ksp_pc_side right -phreeqc_ksp_norm_type unpreconditioned
# Diagnostics:
-phreeqc_ksp_converged_reason -phreeqc_ksp_view -log_view
```

`-phreeqc_mat_type` is a PHREEQC-defined option read with `PetscOptionsGetString()` (Mat type is chosen by our code, since we create the matrix).

---

## 3. Integration Architecture

### 3.1 Hook Point and Solve Strategy

The PETSc path is inserted **inside `ineq()`**, before the existing `cl1()` path, and it is wrapped so that the existing code remains unchanged and reachable:

```cpp
int Phreeqc::ineq(int in_kode)
{
#ifdef USE_PETSC
    if (petsc_enabled && !petsc_disabled_for_current_model)
    {
        LinearSolveStatus solve_status = solve_newton_step_with_petsc();          /* §4, §6 */
        if (solve_status == LINEAR_SOLVE_ACCEPTED || solve_status == LINEAR_SOLVE_ACCEPTED_INEXACT)
        {
            if (step_satisfies_cl1_bounds(delta))           /* see below */
                return OK;                              /* PETSc step accepted */
            petsc_statistics.bound_violation_fallbacks++;
        }
        if (++petsc_failures_in_current_model >= PETSC_FAILURE_LIMIT_PER_MODEL)  /* circuit breaker §6.5 */
            petsc_disabled_for_current_model = true;
    }
#endif
    /* ---- existing ineq()/cl1() code, unmodified ---- */
}
```

The strategy is **optimistic solve with validation**: solve the square n×n system J·δ = r, then check the constraints that `cl1()` would have enforced. Concretely, `step_satisfies_cl1_bounds()` must reject a step when, for example:

- a pure-phase or solid-solution unknown's δ would remove more moles than are present;
- an absent phase (zero moles) is forced to precipitate although it is undersaturated, which the square system does implicitly by forcing SI = 0;
- any other row that `ineq()` treats as an inequality, or drops, would be violated or would not have been part of the system.

The exact list of rows that `ineq()` moves into the inequality block or removes is **[VERIFY]** and must be derived by reading `ineq()` line by line. If the analysis shows that the square system differs from `ineq()`'s equality block in more than bound checks (rows removed or rescaled), the loader in §4 must apply the same row selection, building the PETSc system from `ineq()`'s equality rows rather than directly from the augmented Newton matrix.

Implementation note: rename the existing body of `ineq()` to `ineq_cl1_original()` without modifying it, and make `ineq()` a thin dispatcher. This keeps the original path byte-for-byte intact and makes the comparison mode (§3.4, §8.3) trivial. The hook above shows the Phase 1B form; Phase 1A uses the restricted form in §3.4.

Track `bound_violation_fallbacks / total_steps` as a first-class metric. If it is high for typical problems (systems dominated by minerals near their bounds), the Phase 1 performance benefit will be small and that is important to know early.

### 3.2 Solver Context

One context per `Phreeqc` instance (never shared between instances or threads):

```c
#include <petscksp.h>

#define SOLVER_SLOT_COUNT 8

typedef struct {
    PetscInt      system_size;               /* size this slot is bound to; 0 = empty           */
    Mat           jacobian_matrix;
    Vec           rhs_vector;
    Vec           solution_vector;
    Vec           residual_work_vector;      /* holds rhs - A * solution for the error check    */
    KSP           linear_solver;             /* primary solver (configured from options)        */
    KSP           direct_lu_solver;          /* direct LU retry; NULL if primary is already LU  */
    PetscInt     *column_index_buffer;       /* length system_size, reused                      */
    PetscScalar  *row_value_buffer;          /* length system_size, reused (conversion, scaling) */
    unsigned long last_used_tick;            /* for least-recently-used replacement             */
} SizedSolverSlot;

typedef struct {
    SizedSolverSlot  slots[SOLVER_SLOT_COUNT];
    SizedSolverSlot *active_slot;            /* slot bound for the current solve                */
    PetscBool        uses_sparse_aij;        /* from -phreeqc_mat_type                          */
    PetscBool        primary_solver_is_direct;
    PetscBool        use_row_equilibration;  /* default PETSC_TRUE                              */
    unsigned long    use_tick;
    /* statistics */
    unsigned long    solve_count, accepted_count, inexact_accepted_count,
                     direct_retry_count, cl1_fallback_count, slot_creation_count;
} PetscNewtonSolver;

typedef enum {
    LINEAR_SOLVE_ACCEPTED = 0,         /* converged, backward error acceptable          */
    LINEAR_SOLVE_ACCEPTED_INEXACT,     /* iterative path, partially converged, usable   */
    LINEAR_SOLVE_FALL_BACK_TO_CL1,     /* use the existing cl1 path for this iteration  */
    LINEAR_SOLVE_NON_FINITE_INPUT      /* NaN/Inf in Jacobian or residual: skip cl1 too and
                                          return failure to PHREEQC's existing recovery */
} LinearSolveStatus;
```

### 3.3 Build Integration

```cmake
option(PHREEQC_USE_PETSC "Use PETSc for the Newton linear solve" OFF)
if(PHREEQC_USE_PETSC)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(PETSC REQUIRED IMPORTED_TARGET PETSc)
  target_link_libraries(phreeqc_core PRIVATE PkgConfig::PETSC)
  target_compile_definitions(phreeqc_core PRIVATE USE_PETSC)
endif()
```

`phreeqc_core` is a placeholder: the PHREEQC executable, IPhreeqc, and PhreeqcRM each have their own CMake projects and target names in your tree **[VERIFY]**. With `PHREEQC_USE_PETSC=OFF` the build must be identical to the unmodified code, and results must match the unmodified baseline exactly (this is the one place where an exact comparison is appropriate, since no numerical code changes).

Adapter sources are C++, matching PHREEQC, so `std::mutex`, RAII lock guards, and `LDBLE` can be used directly:

```
src/petsc_linear_solver/petsc_initialization.h/.cpp      PETSc acquire/release/shutdown, global mutex   (§5.1, §12.2)
src/petsc_linear_solver/linear_solve_status.h             LinearSolveStatus enum                          (§3.2)
src/petsc_linear_solver/newton_matrix_transfer.h/.cpp     layout descriptor, load into PETSc, copy out   (§4)
src/petsc_linear_solver/petsc_newton_solver.h/.cpp        solver slots, dense LU solve, acceptance test  (§3.2, §5.4, §6)
src/petsc_linear_solver/petsc_solver_integration.h/.cpp   PHREEQC-side state, comparison log             (§3.4, §8.3)
tests/petsc_linear_solver/test_*.cpp                      unit tests                                     (§4.3)
```

Compile-time guards in the PETSc adapter source:

```c
#if defined(PETSC_USE_COMPLEX)
#  error "PHREEQC PETSc adapter requires a real-scalar PETSc build"
#endif
#if !defined(PETSC_USE_REAL_DOUBLE)
#  error "PHREEQC PETSc adapter assumes double-precision PETSc"
#endif
/* LDBLE may be long double: always copy through row_value_buffer with explicit casts.
   PETSc does not support long double as PetscScalar. */
```

Suggested PETSc configure for Phase 1 standalone and OpenMP builds:

```
./configure --with-debugging=0 --with-mpi=0 --with-scalar-type=real \
            --with-precision=double --with-64-bit-indices=0 \
            COPTFLAGS=-O3 CXXOPTFLAGS=-O3 FOPTFLAGS=-O3
# Optional for config D: --download-suitesparse (UMFPACK) or --download-superlu
```

If PhreeqcRM is built with MPI or is linked into an MPI host, use a PETSc built against the **same** MPI as the host, and link the host's PETSc library rather than a second copy. Set optimized BLAS/LAPACK to a single thread (`OPENBLAS_NUM_THREADS=1`, `MKL_NUM_THREADS=1`) to avoid oversubscription with PhreeqcRM's OpenMP workers.

### 3.4 Phase 1A Eligibility Gate (Unbounded Systems Only)

Bound validation (`step_satisfies_cl1_bounds()`) does not exist until Phase 1B. Accepting a PETSc step for a system with bounded unknowns before then could silently give wrong chemistry: a mineral dissolving past zero moles, or an undersaturated absent phase being forced to precipitate. Phase 1A therefore attempts PETSc only when **every** unknown has a type on a conservative allow-list of aqueous-solution equalities. Everything else goes to `ineq_cl1_original()` unchanged.

```cpp
/* Phase 1A allow-list. Type constants from global_structures.h [VERIFY names].
   A type may be on this list only if ineq() never moves its row into the
   inequality block, drops it, or rescales it (§10 items 11-12).            */
bool Phreeqc::is_eligible_for_petsc_phase1a(void) const
{
    for (int i = 0; i < count_unknowns; ++i) {
        switch (x[i]->type) {
        case MB: case ALK: case CB: case MU: case AH2O: case MH: case MH2O:
            break;                          /* aqueous-solution equalities      */
        default:
            return false;                   /* PP, SS_MOLES, GAS_MOLES, EXCH,
                                               SURFACE*, SOLUTION_PHASE_BOUNDARY,
                                               PITZER_GAMMA, SLACK, ... → cl1() */
        }
    }
    return true;
}
```

Exchange and surface unknowns are excluded in 1A not because they are known to be bounded, but because they have not yet been audited in `ineq()`. They move onto the list in 1B once the audit shows they are pure equalities. If the audit shows that `ineq()` adds constraint rows even for solution-only systems, remove the affected types from the list; if every system gets such rows, Phase 1A cannot proceed without the 1B bound logic, and that finding should stop the work.

Phase 1A dispatcher:

```cpp
int Phreeqc::ineq(int in_kode)
{
#ifdef USE_PETSC
    if (petsc_enabled && petsc_solver_available() && is_eligible_for_petsc_phase1a())
    {
        reserve_candidate_newton_step(count_unknowns);       /* per-instance LDBLE buffer */
        LinearSolveStatus solve_status = solve_newton_step_with_petsc(petsc_solver, augmented_newton_matrix(), count_unknowns,
                                         candidate_newton_step);   /* reads the Newton matrix only */
        if (comparison_mode_enabled)                 /* §8.3: cl1 steers, difference is logged */
        {
            int cl1_return_code = ineq_cl1_original(in_kode);
            write_step_comparison_record(petsc_solver, candidate_newton_step, delta, count_unknowns,
                                         solve_status, cl1_return_code);
            return cl1_return_code;
        }
        if (solve_status == LINEAR_SOLVE_ACCEPTED)
        {
            for (int i = 0; i < count_unknowns; ++i) delta[i] = candidate_newton_step[i];
            return OK;                               /* [VERIFY] §10 item 4 */
        }
        /* Any other status, including LINEAR_SOLVE_NON_FINITE_INPUT, falls through: in 1A the
           cl1 path behaves exactly as it did before integration.            */
    }
#endif
    return ineq_cl1_original(in_kode);                        /* original body, renamed */
}
```

Phase 1A deliberately differs from the 1B design in three ways: there is no inexact acceptance (the dense LU path never produces it), no circuit breaker, and `LINEAR_SOLVE_NON_FINITE_INPUT` falls through to `cl1()` instead of short-circuiting, so baseline failure behaviour is preserved exactly. PETSc reads the augmented Newton matrix before `ineq_cl1_original()` runs, because the `cl1()` path may modify its inputs, and PETSc writes only to `candidate_newton_step` so a rejected step can never leak into `delta`.

---

## 4. Data Mapping: Legacy Array → PETSc

### 4.1 Two Distinct Indexing Problems

"0-based vs 1-based" covers two separate issues that are handled in different places. Conflating them is the most common source of off-by-one bugs in this kind of port.

| Issue | What it is | Where it is handled |
|---|---|---|
| **Storage offset** | How a logical element (i, j) maps to a memory address: row-major vs column-major, leading dimension, and whether row/column 0 is padding (C code emulating 1-based arrays by allocating n+1 and ignoring index 0). | Address formula in `layout_jacobian_entry()` (`padding_rows`, `padding_columns`, `leading_dimension`, `is_column_major`). |
| **Index-valued data** | Arrays whose *contents* are indices: CSR `ia`/`ja`, unknown-number lists, permutation vectors, Fortran API cell numbers. | Subtract `index_base` on read; add it back on write. |

A genuine Fortran array `A(LD,*)` passed to C needs **no** address offset: element `A(I,J)` lives at `a[(I-1) + (J-1)*LD]`, which for 0-based logical (i, j) is `data[i + j*leading_dimension]`. Only the stride orientation changes. Offsets apply to *padded* C storage and to index-valued data.

**Rule for all code below:** logical indices `i`, `j` are 0-based everywhere, matching PETSc's `PetscInt` row/column indices. Conversions happen only inside `layout_jacobian_entry()` and when reading index arrays.

PHREEQC's augmented Newton matrix (3.x, **[VERIFY]**): row-major, `leading_dimension = n+1`, `padding_rows = padding_columns = 0`, RHS at logical column `n`.

### 4.2 Layout Descriptor

```c
typedef struct {
    const LDBLE *data;        /* base pointer of legacy storage                        */
    PetscInt     leading_dimension;       /* leading dimension: row stride (row-major) or
                              column stride (column-major), in elements            */
    PetscInt     padding_rows;  /* 1 if storage reserves row 0 (padded 1-based C), else 0 */
    PetscInt     padding_columns;  /* 1 if storage reserves col 0 (padded 1-based C), else 0 */
    PetscBool    is_column_major; /* PETSC_TRUE for Fortran-ordered storage                */
    PetscInt     rhs_column;  /* LOGICAL 0-based column of RHS inside 'data', or -1     */
    const LDBLE *separate_rhs;      /* separate RHS vector if rhs_column < 0 (else NULL)        */
    PetscInt     separate_rhs_padding;  /* 1 if separate RHS is padded 1-based                   */
} DenseMatrixLayout;

/* i, j: 0-based logical indices. All offset handling lives here and nowhere else. */
static inline PetscReal layout_jacobian_entry(const DenseMatrixLayout *layout, PetscInt i, PetscInt j)
{
    const PetscInt storage_row = i + layout->padding_rows;
    const PetscInt storage_column = j + layout->padding_columns;
    const LDBLE value = layout->is_column_major ? layout->data[storage_column * layout->leading_dimension + storage_row]
                                : layout->data[storage_row * layout->leading_dimension + storage_column];
    return (PetscReal)value;   /* explicit narrowing if LDBLE is long double */
}

static inline PetscReal layout_rhs_entry(const DenseMatrixLayout *layout, PetscInt i)
{
    return (layout->rhs_column >= 0) ? layout_jacobian_entry(layout, i, layout->rhs_column)
                             : (PetscReal)layout->separate_rhs[i + layout->separate_rhs_padding];
}

/* PHREEQC 3.x instance ([VERIFY] layout): */
static inline DenseMatrixLayout phreeqc_newton_matrix_layout(const LDBLE *augmented_newton_matrix, int count_unknowns)
{
    DenseMatrixLayout layout;
    layout.data        = augmented_newton_matrix;
    layout.leading_dimension       = (PetscInt)count_unknowns + 1;  /* system_size Jacobian columns + 1 RHS column */
    layout.padding_rows  = 0;
    layout.padding_columns  = 0;
    layout.is_column_major = PETSC_FALSE;
    layout.rhs_column  = (PetscInt)count_unknowns;      /* logical column system_size */
    layout.separate_rhs      = NULL;
    layout.separate_rhs_padding  = 0;
    return layout;
}
```

### 4.3 Loading the Matrix and RHS

Design points embodied in the code:

- **Row equilibration** (default on): each row and its RHS entry are divided by the row's max-abs entry. This leaves the solution unchanged (left scaling: D·J·δ = D·r has the same δ) and makes absolute tolerances and the backward-error test meaningful. If PHREEQC's own `diagonal_scale` knob is active, decide whether it is applied before the augmented Newton matrix is final, to avoid scaling twice. **[VERIFY]**
- **Dense path:** insert every entry. The pattern is trivially fixed.
- **AIJ path:** call `MatZeroEntries()` first (keeps the nonzero pattern, clears stale values), then insert only nonzeros. The pattern can only grow; preallocation of n per row guarantees no mallocs. PETSc tracks the nonzero state, so `PCSetUp` redoes symbolic factorisation automatically when the pattern grows, and only then.
- **Non-finite detection** during the copy, so NaN/Inf never reach PETSc.
- `MatSetValues()` is called one row at a time with `INSERT_VALUES`. Rows of the augmented Newton matrix are contiguous, but the buffer is still required for type conversion and scaling.

```c
/* Returns PETSC_SUCCESS on no PETSc error. *solve_status reports numerical admissibility. */
static PetscErrorCode load_newton_system_into_petsc(PetscNewtonSolver *solver, const DenseMatrixLayout *layout, PetscInt system_size,
                               LinearSolveStatus *solve_status)
{
    SizedSolverSlot *slot;
    PetscScalar *rhs_values;
    PetscBool    found_non_finite = PETSC_FALSE, found_zero_row = PETSC_FALSE;

    PetscFunctionBeginUser;
    *solve_status = LINEAR_SOLVE_ACCEPTED;
    PetscCall(select_solver_slot_for_size(solver, system_size));           /* §5.4: get/create slot for size system_size */
    slot = solver->active_slot;

    if (solver->uses_sparse_aij) PetscCall(MatZeroEntries(slot->jacobian_matrix));
    PetscCall(VecGetArrayWrite(slot->rhs_vector, &rhs_values));

    for (PetscInt i = 0; i < system_size && !found_non_finite && !found_zero_row; ++i) {
        PetscReal row_max_abs = 0.0;
        for (PetscInt j = 0; j < system_size; ++j) {
            const PetscReal value = layout_jacobian_entry(layout, i, j);
            if (PetscIsInfOrNanReal(value)) { found_non_finite = PETSC_TRUE; break; }
            row_max_abs = PetscMax(row_max_abs, PetscAbsReal(value));
        }
        if (found_non_finite) break;
        if (row_max_abs == 0.0) { found_zero_row = PETSC_TRUE; break; }   /* structurally singular */

        const PetscReal row_scale_factor = solver->use_row_equilibration ? 1.0 / row_max_abs : 1.0;
        PetscInt entry_count = 0;
        for (PetscInt j = 0; j < system_size; ++j) {
            const PetscReal value = layout_jacobian_entry(layout, i, j);
            if (solver->uses_sparse_aij && value == 0.0) continue;             /* dense: keep all */
            slot->column_index_buffer[entry_count] = j;                               /* 0-based column  */
            slot->row_value_buffer[entry_count] = (PetscScalar)(row_scale_factor * value);
            ++entry_count;
        }
        PetscInt row = i;                                    /* 0-based row     */
        PetscCall(MatSetValues(slot->jacobian_matrix, 1, &row, entry_count, slot->column_index_buffer, slot->row_value_buffer, INSERT_VALUES));

        const PetscReal rhs_value = layout_rhs_entry(layout, i);
        if (PetscIsInfOrNanReal(rhs_value)) { found_non_finite = PETSC_TRUE; break; }
        rhs_values[i] = (PetscScalar)(row_scale_factor * rhs_value);
    }

    PetscCall(VecRestoreArrayWrite(slot->rhs_vector, &rhs_values));
    /* Always assemble, even on early exit, so the Mat is in a valid state
       for MatZeroEntries() on the next call. */
    PetscCall(MatAssemblyBegin(slot->jacobian_matrix, MAT_FINAL_ASSEMBLY));
    PetscCall(MatAssemblyEnd(slot->jacobian_matrix, MAT_FINAL_ASSEMBLY));

    if (found_non_finite)      *solve_status = LINEAR_SOLVE_NON_FINITE_INPUT;          /* cl1 cannot fix NaN either */
    else if (found_zero_row)  *solve_status = LINEAR_SOLVE_FALL_BACK_TO_CL1;   /* rank deficient → L1       */
    PetscFunctionReturn(PETSC_SUCCESS);
}
```

**Optional dense fast path.** `MatDenseGetArrayWrite()` with direct writes avoids per-row `MatSetValues()` calls. PETSc dense storage is **column-major**: element (i, j) lives at offset `j*lda + i`. The code below is correct as written. Note that `Ad[j*lda + i]` and `Ad[i + j*lda]` are the same expression; the *wrong* (row-major) form would be `Ad[i*lda + j]`. Do not "correct" this line:

```c
PetscScalar *dense_values; PetscInt leading_dimension_of_values;
PetscCall(MatDenseGetLDA(slot->jacobian_matrix, &leading_dimension_of_values));
PetscCall(MatDenseGetArrayWrite(slot->jacobian_matrix, &dense_values));
for (PetscInt i = 0; i < system_size; ++i)
    for (PetscInt j = 0; j < system_size; ++j)
        dense_values[j * leading_dimension_of_values + i] = (PetscScalar)(row_scale_factor_of_row[i] * layout_jacobian_entry(layout, i, j));  /* note j*lda+i */
PetscCall(MatDenseRestoreArrayWrite(slot->jacobian_matrix, &dense_values));
```

Adopt this only if profiling shows `MatSetValues` overhead matters (unlikely at these sizes). In Phase 1A use the `MatSetValues()` path only.

**Mandatory layout unit test.** A transposition error produces plausible-looking but wrong results, and symmetric test matrices cannot detect it. Every load path (row-wise `MatSetValues`, dense fast path, CSR) must pass this test before any chemistry run:

```cpp
/* J is deliberately non-symmetric; J^T would give a different solution.
   expected_jacobian = [1 2 0; 0 3 4; 5 0 6], expected_newton_step = {1, -1, 2},
   expected_rhs = expected_jacobian * expected_newton_step = {-1, 5, 17}.
   Stored exactly as PHREEQC stores the augmented Newton matrix: row-major, leading_dimension = system_size + 1, RHS in column system_size. */
static const LDBLE augmented_newton_matrix_3x3[3 * 4] = {
    1.0, 2.0, 0.0,  -1.0,
    0.0, 3.0, 4.0,   5.0,
    5.0, 0.0, 6.0,  17.0,
};
/* Expected: solve_newton_step_with_petsc(...) == LINEAR_SOLVE_ACCEPTED and |delta[i] - delta_true[i]| <= 1e-12.
   (A transposed load would solve with the transposed Jacobian and fail this check.)

   Required variants:
   (a) row scaling on and off (identical delta expected);
   (b) row 1 multiplied by 1e-12 including its RHS entry (same delta expected:
       tests that row scaling leaves the solution unchanged);
   (c) a padded 1-based copy (padding_rows = padding_columns = 1) through layout_jacobian_entry();
   (d) a zero row → LINEAR_SOLVE_FALL_BACK_TO_CL1; a NaN entry → LINEAR_SOLVE_NON_FINITE_INPUT;
   (e) cache: alternate system_size = 3 and a 4x4 system ten times; slot_creation_count must be 2. */
```

### 4.4 Variant: Legacy Sparse (CSR) Input, Possibly 1-Based

If some PHREEQC variant or a coupled code provides CSR arrays (`ia[0..n]`, `ja[]`, `va[]`), possibly in Fortran 1-based convention (`ia[0] == 1`):

```c
/* index_base = 1 for Fortran/Harwell-Boeing style CSR, 0 for C style.
   Row i (0-based) occupies entries row_offsets[i]-index_base .. row_offsets[i+1]-index_base-1. */
static PetscErrorCode load_csr_system_into_petsc(Mat jacobian_matrix, PetscInt system_size, const PetscInt *row_offsets,
                                   const PetscInt *column_indices, const LDBLE *values,
                                   PetscInt index_base, PetscInt *column_index_buffer,
                                   PetscScalar *row_value_buffer)
{
    PetscFunctionBeginUser;
    PetscCall(MatZeroEntries(jacobian_matrix));
    for (PetscInt i = 0; i < system_size; ++i) {
        const PetscInt row_start = row_offsets[i]     - index_base;   /* offsets are index-valued data */
        const PetscInt row_end = row_offsets[i + 1] - index_base;
        PetscInt entry_count = 0;
        for (PetscInt entry = row_start; entry < row_end; ++entry) {
            column_index_buffer[entry_count] = column_indices[entry] - index_base;           /* column ids are index-valued */
            row_value_buffer[entry_count] = (PetscScalar)values[entry];
            ++entry_count;
        }
        PetscInt row = i;
        PetscCall(MatSetValues(jacobian_matrix, 1, &row, entry_count, column_index_buffer, row_value_buffer, INSERT_VALUES));
    }
    PetscCall(MatAssemblyBegin(jacobian_matrix, MAT_FINAL_ASSEMBLY));
    PetscCall(MatAssemblyEnd(jacobian_matrix, MAT_FINAL_ASSEMBLY));
    PetscFunctionReturn(PETSC_SUCCESS);
}
```

Do **not** use `MatCreateSeqAIJWithArrays()` with legacy arrays unless they are already 0-based, have sorted column indices, use `PetscInt`/`PetscScalar` types, and outlive the Mat. The copy above is safer and its cost is negligible at these sizes.

### 4.5 Writing the Solution Back

```c
/* output_padding = 1 if PHREEQC's delta were a padded 1-based array; 0 for PHREEQC 3.x */
static PetscErrorCode copy_solution_from_petsc(const SizedSolverSlot *slot, LDBLE *delta, PetscInt output_padding,
                                      PetscBool *solution_is_finite)
{
    const PetscScalar *solution_values;
    PetscFunctionBeginUser;
    *solution_is_finite = PETSC_TRUE;
    PetscCall(VecGetArrayRead(slot->solution_vector, &solution_values));
    for (PetscInt i = 0; i < slot->system_size; ++i) {
        const PetscReal value = PetscRealPart(solution_values[i]);
        if (PetscIsInfOrNanReal(value)) { *solution_is_finite = PETSC_FALSE; break; }
        delta[i + output_padding] = (LDBLE)value;
    }
    PetscCall(VecRestoreArrayRead(slot->solution_vector, &solution_values));
    PetscFunctionReturn(PETSC_SUCCESS);
}
```

Write into `delta` only after the whole vector is verified finite; otherwise a partial write could leak into the `cl1()` fallback if that code reads `delta` as an initial value. Write to the candidate Newton step buffer first if `cl1()`'s path reads `delta`. **[VERIFY]**

**Sign convention.** The PETSc path must produce the same δ that `cl1()` produces for the equality system: solve J·δ = r with r taken exactly as stored in the RHS column. Do not "fix" signs by reasoning; confirm with the A/B harness in §8, which compares δ from both solvers on identical inputs.

---

## 5. Lifecycle Management

### 5.1 Global PETSc Initialisation: Ownership Rules

PHREEQC is usually a library. A library must not assume it owns PETSc:

1. If PETSc is already initialised (host application uses PETSc), **use it and never finalise it**.
2. If not, initialise it and record that PHREEQC owns it.
3. Reference-count users so finalisation happens exactly once, after the last PETSc object is destroyed.
4. Do not rely on re-initialising PETSc after `PetscFinalize()`. It is not robust in all configurations; with MPI it is impossible because `MPI_Init` cannot be called twice.
5. Never pass PHREEQC's `argv` to `PetscInitialize()`. The `phreeqc` command line is positional (input, output, database), so PETSc options there would be misparsed as file names. Provide PETSc options via the `PETSC_OPTIONS` environment variable or an options file (`PETSC_OPTIONS="-options_file phreeqc_petsc.opts"`).

```c
/* petsc_initialization.cpp */
#include <petscsys.h>
#include <mutex>

static std::mutex global_state_mutex;
static int        reference_count      = 0;
static PetscBool  phreeqc_initialized_petsc  = PETSC_FALSE;
static PetscBool  finalize_on_last_release = PETSC_TRUE;  /* see 5.2 */

int acquire_petsc(void)
{
    std::lock_guard<std::mutex> state_lock(global_state_mutex);
    if (reference_count == 0) {
        PetscBool petsc_is_initialized = PETSC_FALSE, petsc_is_finalized = PETSC_FALSE;
        PetscInitialized(&petsc_is_initialized);
        PetscFinalized(&petsc_is_finalized);
        if (petsc_is_finalized) return -1;                    /* cannot re-init: disable PETSc path */
        if (!petsc_is_initialized) {
            if (PetscInitializeNoArguments() != PETSC_SUCCESS) return -1;
            phreeqc_initialized_petsc = PETSC_TRUE;
            /* We own PETSc: make PETSc errors return codes quietly instead of
               printing tracebacks; failures are handled by fallback logic.     */
            PetscPushErrorHandler(PetscReturnErrorHandler, NULL);
        }
    }
    ++reference_count;
    return 0;
}

void release_petsc(void)
{
    std::lock_guard<std::mutex> state_lock(global_state_mutex);
    if (reference_count <= 0) return;
    if (--reference_count == 0 && phreeqc_initialized_petsc && finalize_on_last_release) {
        PetscPopErrorHandler();
        PetscFinalize();
        phreeqc_initialized_petsc = PETSC_FALSE;
    }
}

/* Serialises every operation that touches PETSc global state outside the
   hot path: slot create/destroy (KSPSetFromOptions reads the options database),
   context destruction, statistics printing. Not taken in load/solve.          */
std::mutex &petsc_global_state_mutex(void) { return global_state_mutex; }

/* Explicit API for hosts that construct/destroy PHREEQC objects repeatedly:
   call set_petsc_finalize_policy(PETSC_FALSE) once, then shutdown_petsc_if_owned_by_phreeqc()
   at program end. */
void set_petsc_finalize_policy(PetscBool on_last_release);
void shutdown_petsc_if_owned_by_phreeqc(void);
```

### 5.2 Placement in PHREEQC Init/Cleanup Routines

| Entry point | Acquire (init) | Release (cleanup) | Notes |
|---|---|---|---|
| **`Phreeqc` class** (all builds) | `Phreeqc::Phreeqc(...)` constructor: call `acquire_petsc()`; set `petsc_solver = NULL`. Do **not** create solver objects here. | `Phreeqc::~Phreeqc()`: `destroy_petsc_newton_solver(&petsc_solver)` **then** `release_petsc()`. | Solver objects are created lazily on the first `ineq()` call (§5.3), so instances that never run a model (PhreeqcRM utility instances, database-only loads) never allocate PETSc objects. Put destruction in the destructor, not `clean_up()`, because `clean_up()` can run on re-initialisation while the instance lives on. **[VERIFY]** that a `Phreeqc` copy constructor / `InternalCopy()` sets `petsc_solver = NULL` in the copy instead of copying the pointer. |
| **`phreeqc` executable** (`class_main.cpp`, `main()`) | `acquire_petsc()` as the first statement of `main()` (pins PETSc for the whole run). | `release_petsc()` as the last statement, **after** the `Phreeqc` object has left scope. | See the code below: the stack-allocated `Phreeqc` must be destroyed before finalisation. |
| **IPhreeqc** (`CreateIPhreeqc` / `DestroyIPhreeqc`, `IPhreeqc::IPhreeqc` / `~IPhreeqc`) | Inherited through the `Phreeqc` constructor. | Inherited through the `Phreeqc` destructor. | For hosts that create and destroy IPhreeqc instances repeatedly, set the finalise policy to "explicit" and call `shutdown_petsc_if_owned_by_phreeqc()` at host exit, to avoid finalise/re-init cycles. |
| **PhreeqcRM** (`PhreeqcRM::PhreeqcRM`, `~PhreeqcRM`; `RM_Create` / `RM_Destroy`) | Optional pin: `acquire_petsc()` at the top of the constructor, before workers are created. | `release_petsc()` at the end of the destructor, after all worker instances are deleted. | Workers' own acquire/release calls happen inside. With MPI hosts, PETSc must be initialised **after** `MPI_Init`; PETSc then will not call `MPI_Finalize` itself, so the host's `MPI_Finalize` must come after the last release. |

Executable `main()`:

```cpp
int main(int argc, char *argv[])
{
    int return_code;
#ifdef USE_PETSC
    if (acquire_petsc() != 0) {
        /* PETSc unavailable: run with the cl1 path only */
    }
#endif
    {   /* explicit scope: Phreeqc destructor (and its PETSc objects) runs here */
        Phreeqc phreeqc_instance;
        return_code = phreeqc_instance.main_method(argc, argv);
    }
#ifdef USE_PETSC
    release_petsc();     /* refcount → 0 → PetscFinalize() if we own PETSc */
#endif
    return cl1_return_code;
}
```

Without the inner scope, `PetscFinalize()` would run before the destructor of `phreeqc_instance`, and the subsequent `KSPDestroy()`/`MatDestroy()` calls would be undefined behaviour.

### 5.3 Create Once vs. Create Every Newton Iteration

**Decision: create once per `Phreeqc` instance (lazily), reuse across all Newton iterations and all cells. Never create/destroy per iteration.**

Work performed per call under each policy:

| Operation | Per-iteration create/destroy | Create once, reuse |
|---|---|---|
| `KSPCreate` / `PCCreate`, function-table setup | every iteration | once |
| `KSPSetFromOptions` (dozens of options-database lookups, string comparisons) | every iteration | once |
| `MatCreate` + preallocation (n² for dense) + `VecCreate`×3 | every iteration (several heap allocations) | once per distinct n (cached) |
| Symbolic factorisation (AIJ) | every iteration | only when the nonzero pattern grows |
| `MatSetValues` + assembly | every iteration | every iteration |
| Numeric factorisation + triangular solves | every iteration | every iteration |
| `*Destroy` (frees) | every iteration | at instance destruction |

For n = 30, the dense LU and solve are roughly 2×10⁴ flops, a few microseconds. Object creation, options parsing, and allocation plausibly cost tens of microseconds, so per-iteration creation could inflate solver time several-fold. These are order-of-magnitude estimates; measure with `-log_view` on your hardware.

Scale illustration: 10⁵ cells × 10³ time steps × ~8 Newton iterations ≈ 8×10⁸ solves. Each 10 µs of avoidable overhead per solve is ≈ 8000 CPU-seconds. Repeated small allocations also fragment the heap and inflate memory high-water marks in long runs, and allocation-heavy loops scale poorly under OpenMP because the allocator becomes a contention point.

Optional optimisation for later (Phase 1b): `PCSetReusePreconditioner(pc, PETSC_TRUE)` keeps the LU factors across several Newton iterations (a chord / modified-Newton scheme). This trades extra Newton iterations for skipped factorisations. Evaluate only after the baseline is validated, and refactor whenever the Newton residual stops decreasing.

### 5.4 Handling Varying System Size (Size-Keyed Cache)

`count_unknowns` differs between cells (different mineral assemblages, surfaces, exchangers) and can change when PHREEQC re-sets-up the model. With a single object set, heterogeneous cells would force a resize on almost every cell. A small LRU cache keyed by n avoids that.

```c
static PetscErrorCode create_solver_slot(PetscNewtonSolver *solver, SizedSolverSlot *slot, PetscInt system_size)
{
    PC preconditioner;
    PetscFunctionBeginUser;
    slot->system_size = system_size;
    if (solver->uses_sparse_aij) PetscCall(MatCreateSeqAIJ(PETSC_COMM_SELF, system_size, system_size, system_size, NULL, &slot->jacobian_matrix));
    else           PetscCall(MatCreateSeqDense(PETSC_COMM_SELF, system_size, system_size, NULL, &slot->jacobian_matrix));
    PetscCall(MatSetOptionsPrefix(slot->jacobian_matrix, "phreeqc_"));
    PetscCall(MatCreateVecs(slot->jacobian_matrix, &slot->solution_vector, &slot->rhs_vector));
    PetscCall(VecDuplicate(slot->rhs_vector, &slot->residual_work_vector));
    PetscCall(PetscMalloc2(system_size, &slot->column_index_buffer, system_size, &slot->row_value_buffer));

    /* Primary solver: defaults = config A; options may override */
    PetscCall(KSPCreate(PETSC_COMM_SELF, &slot->linear_solver));
    PetscCall(KSPSetOptionsPrefix(slot->linear_solver, "phreeqc_"));
    PetscCall(KSPSetType(slot->linear_solver, KSPPREONLY));
    PetscCall(KSPGetPC(slot->linear_solver, &preconditioner));
    PetscCall(PCSetType(preconditioner, PCLU));
    PetscCall(PCFactorSetMatOrderingType(preconditioner, MATORDERINGNATURAL));
    PetscCall(KSPSetInitialGuessNonzero(slot->linear_solver, PETSC_FALSE));
    PetscCall(KSPSetErrorIfNotConverged(slot->linear_solver, PETSC_FALSE));   /* we handle failures */
    PetscCall(set_iterative_solver_defaults(slot->linear_solver, system_size));            /* §6.2, no-op if direct */
    PetscCall(KSPSetFromOptions(slot->linear_solver));                        /* ONCE per slot */
    PetscCall(KSPSetOperators(slot->linear_solver, slot->jacobian_matrix, slot->jacobian_matrix));

    /* Direct retry solver only if the primary is iterative */
    {
        KSPType solver_type; PCType preconditioner_type;
        PetscCall(KSPGetType(slot->linear_solver, &solver_type));
        PetscCall(PCGetType(preconditioner, &preconditioner_type));
        solver->primary_solver_is_direct = (PetscBool)(!strcmp(solver_type, KSPPREONLY) &&
                               (!strcmp(preconditioner_type, PCLU) || !strcmp(preconditioner_type, PCSVD)));
    }
    slot->direct_lu_solver = NULL;
    if (!solver->primary_solver_is_direct) {
        PC direct_preconditioner;
        PetscCall(KSPCreate(PETSC_COMM_SELF, &slot->direct_lu_solver));
        PetscCall(KSPSetType(slot->direct_lu_solver, KSPPREONLY));
        PetscCall(KSPGetPC(slot->direct_lu_solver, &direct_preconditioner));
        PetscCall(PCSetType(direct_preconditioner, PCLU));
        /* Native AIJ LU does not pivot; a nonzero shift avoids hard zero-pivot failure.
           Prefer an external pivoting solver if PETSc was built with one. */
        if (solver->uses_sparse_aij) PetscCall(PCFactorSetShiftType(direct_preconditioner, MAT_SHIFT_NONZERO));
        PetscCall(KSPSetErrorIfNotConverged(slot->direct_lu_solver, PETSC_FALSE));
        PetscCall(KSPSetOperators(slot->direct_lu_solver, slot->jacobian_matrix, slot->jacobian_matrix));
    }
    PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode destroy_solver_slot(SizedSolverSlot *slot)
{
    PetscFunctionBeginUser;
    if (slot->system_size == 0) PetscFunctionReturn(PETSC_SUCCESS);
    PetscCall(KSPDestroy(&slot->linear_solver));
    PetscCall(KSPDestroy(&slot->direct_lu_solver));   /* safe on NULL */
    PetscCall(MatDestroy(&slot->jacobian_matrix));
    PetscCall(VecDestroy(&slot->rhs_vector));
    PetscCall(VecDestroy(&slot->solution_vector));
    PetscCall(VecDestroy(&slot->residual_work_vector));
    PetscCall(PetscFree2(slot->column_index_buffer, slot->row_value_buffer));
    slot->system_size = 0;
    PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode select_solver_slot_for_size(PetscNewtonSolver *solver, PetscInt system_size)
{
    SizedSolverSlot *slot_to_replace = &solver->slots[0];
    PetscFunctionBeginUser;
    ++solver->use_tick;
    if (solver->active_slot && solver->active_slot->system_size == system_size) { solver->active_slot->last_used_tick = solver->use_tick; PetscFunctionReturn(PETSC_SUCCESS); }
    for (int slot_index = 0; slot_index < SOLVER_SLOT_COUNT; ++slot_index) {
        SizedSolverSlot *slot = &solver->slots[slot_index];
        if (slot->system_size == system_size) { solver->active_slot = slot; slot->last_used_tick = solver->use_tick; PetscFunctionReturn(PETSC_SUCCESS); }
        if (slot->system_size == 0 || slot->last_used_tick < slot_to_replace->last_used_tick) slot_to_replace = slot;  /* empty or LRU */
    }
    {
        /* RAII guard: released on every exit path, including early returns
           generated by PetscCall() on error. Never call acquire_petsc()
           or release_petsc() while holding this lock (same mutex).      */
        std::lock_guard<std::mutex> state_lock(petsc_global_state_mutex());
        PetscCall(destroy_solver_slot(slot_to_replace));   /* evict */
        PetscCall(create_solver_slot(solver, slot_to_replace, system_size));
    }
    slot_to_replace->last_used_tick = solver->use_tick;
    solver->active_slot = slot_to_replace;
    ++solver->slot_creation_count;
    PetscFunctionReturn(PETSC_SUCCESS);
}
```

Design notes:

- `destroy_petsc_newton_solver()` (called from `~Phreeqc`) takes the same lock while destroying all slots, then releases it **before** calling `release_petsc()`, which locks the mutex itself.
- Each slot owns its own KSP, so a size change never requires `KSPReset()`. If you implement a single-slot variant instead, PETSc requires `KSPReset()` before `KSPSetOperators()` with operators of a different size.
- Memory per dense slot is about 2n² doubles (matrix plus factor), around 160 KB at n = 100. Eight slots per instance is negligible.
- `KSPSetOperators()` is called at slot creation. Since the same `Mat` object is refilled in place, PETSc detects the changed values through the matrix state counter and refactors on the next `KSPSolve()`. Calling `KSPSetOperators()` again before each solve is harmless and is done in §6.3 for clarity.

### 5.5 Threading (PhreeqcRM with OpenMP)

PhreeqcRM runs worker `Phreeqc` instances in an OpenMP parallel loop. Each worker has its own context and objects, so no PETSc object is shared. PETSc global state (logging, memory tracking, the error-handler stack, the options database) is still touched from several threads.

Phase 1 policy:

- **Certified configuration:** PhreeqcRM with `nthreads = 1`, or MPI-only parallelism with one thread per rank.
- `nthreads > 1` is gated behind a build/runtime flag and requires a PETSc built for thread safety. Check the `--with-threadsafety` configure option for your PETSc version; it typically requires disabling logging. Run with `-malloc_debug` off.
- Slot creation and destruction (which call `KSPSetFromOptions` and touch PETSc's global registries) run under the global mutex (§5.4 code), so options-database access is serialised even in threaded runs. This is cheap: it happens once per slot, never per solve. The hot path (load, solve, store) takes no lock, which is why `nthreads > 1` still needs a thread-safe PETSc build.
- Set BLAS/LAPACK to one thread (§3.3).

---

## 6. Convergence Tolerances and Failure Handling

### 6.1 Direct Path (Default): Acceptance Test

With `KSPPREONLY` + `PCLU`, `KSPSetTolerances()` has no effect. Acceptance is decided by:

1. `KSPGetConvergedReason()` > 0;
2. the solution is finite;
3. the normwise backward error of the (row-scaled) system is small:

  η = ‖b − A·x‖∞ / (‖A‖∞·‖x‖∞ + ‖b‖∞) ≤ η_max, with **η_max = 10⁻¹⁰** by default.

LU with partial pivoting normally achieves η ≈ 10⁻¹⁵–10⁻¹⁴. A value above 10⁻¹⁰ indicates pivot growth or near-singularity, so escalate (refinement, config B) or fall back to `cl1()`. The check costs one `MatMult`, trivial at these sizes, and is the most valuable safety net in the design.

### 6.2 Iterative Path (Config C): Default Tolerances

Defaults below assume row equilibration is on, so rows are O(1).

| Parameter | Default | Rationale |
|---|---|---|
| `rtol` | 1×10⁻⁸ | Tighter values are often unattainable for κ ≥ 10¹⁰ (accuracy floor ≈ ε·κ) and convert good-enough steps into `DIVERGED_ITS`. Newton tolerates inexact steps. |
| `abstol` | 1×10⁻¹⁴ | Prevents over-solving when the nonlinear residual (hence ‖b‖) is already tiny near convergence. Meaningful only because rows are equilibrated. |
| `dtol` | 1×10⁴ | Early detection of divergence (PETSc default is 10⁵). |
| `maxits` | n + 10 | With restart ≥ n, full GMRES terminates in ≤ n steps in exact arithmetic; hitting the limit signals breakdown, not slow convergence. |
| GMRES restart | max(n, 30) | Non-restarted GMRES for small n avoids stagnation. |
| PC side / norm | `PC_RIGHT`, `KSP_NORM_UNPRECONDITIONED` | Tolerances apply to the true residual, not the ILU-preconditioned one. |
| ILU | levels 1 (try 2), `MAT_SHIFT_NONZERO` | ILU(0) on a dense-ish pattern is weak; shift survives zero pivots. |
| Inexact-accept threshold | ‖r‖/‖b‖ ≤ 10⁻³ | If `DIVERGED_ITS` but the residual was reduced by three orders, accept as an inexact Newton step (counted separately). |

```c
static PetscErrorCode set_iterative_solver_defaults(KSP linear_solver, PetscInt system_size)
{
    PetscFunctionBeginUser;
    /* Applied before KSPSetFromOptions; harmless if the type stays PREONLY. */
    PetscCall(KSPSetTolerances(linear_solver, 1.0e-8, 1.0e-14, 1.0e4, system_size + 10));
    PetscCall(KSPGMRESSetRestart(linear_solver, PetscMax(system_size, 30)));
    PetscCall(KSPSetPCSide(linear_solver, PC_RIGHT));
    PetscCall(KSPSetNormType(linear_solver, KSP_NORM_UNPRECONDITIONED));
    PetscFunctionReturn(PETSC_SUCCESS);
}
```

Order of calls matters: defaults first, then `KSPSetFromOptions()`, so that runtime options override defaults. If you switch the type to GMRES only via options, `KSPGMRESSetRestart` on a PREONLY object is silently ignored, so restart must also be given at runtime (`-phreeqc_ksp_gmres_restart`) or re-applied after `KSPSetFromOptions()` when the resulting type is GMRES.

**Relation to PHREEQC's own convergence criterion.** PHREEQC declares Newton convergence based on scaled residuals against `convergence_tolerance` (KNOBS, default on the order of 10⁻⁸; **[VERIFY]**). The linear rtol must not be the limiting factor near the end of Newton. An Eisenstat–Walker forcing term is the principled choice for a Phase 1b refinement:

  ηₖ = min(η_max, γ·(‖Fₖ‖/‖Fₖ₋₁‖)^α), with γ = 0.9, α = (1+√5)/2, η_max = 10⁻², and a floor of 10⁻¹⁰,

  then `KSPSetTolerances(linear_solver, eta_k, ...)` before each solve. This saves iterations early in Newton and tightens automatically near convergence. Not needed for config A.

### 6.3 Solve, Classify, Retry

```c
static PetscErrorCode compute_normwise_backward_error(SizedSolverSlot *slot, PetscReal *backward_error)
{
    PetscReal residual_norm, matrix_norm, solution_norm, rhs_norm;
    PetscFunctionBeginUser;
    PetscCall(MatMult(slot->jacobian_matrix, slot->solution_vector, slot->residual_work_vector));   /* work = A * solution */
    PetscCall(VecAYPX(slot->residual_work_vector, -1.0, slot->rhs_vector));   /* work = rhs - A * solution */
    PetscCall(VecNorm(slot->residual_work_vector, NORM_INFINITY, &residual_norm));
    PetscCall(MatNorm(slot->jacobian_matrix, NORM_INFINITY, &matrix_norm));
    PetscCall(VecNorm(slot->solution_vector, NORM_INFINITY, &solution_norm));
    PetscCall(VecNorm(slot->rhs_vector, NORM_INFINITY, &rhs_norm));
    /* Rev 3 fix: no additive constant. PETSC_SMALL is 1e-10 in double precision and
       would make the backward error artificially small when ||solution|| and ||rhs|| are tiny
       near convergence. */
    {
        const PetscReal denominator = matrix_norm * solution_norm + rhs_norm;
        *backward_error = (denominator > 0.0) ? residual_norm / denominator : ((residual_norm == 0.0) ? 0.0 : PETSC_MAX_REAL);
    }
    PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode run_linear_solve(SizedSolverSlot *slot, KSP linear_solver, PetscBool solver_is_iterative,
                                  LinearSolveStatus *solve_status)
{
    KSPConvergedReason converged_reason;
    PetscReal backward_error;
    PetscFunctionBeginUser;
    PetscCall(KSPSetOperators(linear_solver, slot->jacobian_matrix, slot->jacobian_matrix));
    PetscCall(KSPSolve(linear_solver, slot->rhs_vector, slot->solution_vector));
    PetscCall(KSPGetConvergedReason(linear_solver, &converged_reason));

    if (converged_reason > 0) {
        PetscCall(compute_normwise_backward_error(slot, &backward_error));
        *solve_status = (backward_error <= MAX_BACKWARD_ERROR) ? LINEAR_SOLVE_ACCEPTED : LINEAR_SOLVE_FALL_BACK_TO_CL1;
        PetscFunctionReturn(PETSC_SUCCESS);
    }

    switch (converged_reason) {
    case KSP_DIVERGED_ITS: {                 /* iterative only: stagnation */
        PetscReal krylov_residual_norm, rhs_norm;
        PetscCall(KSPGetResidualNorm(linear_solver, &krylov_residual_norm));
        PetscCall(VecNorm(slot->rhs_vector, NORM_2, &rhs_norm));
        *solve_status = (solver_is_iterative && krylov_residual_norm <= INEXACT_ACCEPTANCE_RATIO * rhs_norm)
                  ? LINEAR_SOLVE_ACCEPTED_INEXACT : LINEAR_SOLVE_FALL_BACK_TO_CL1;
        break;
    }
    case KSP_DIVERGED_NANORINF:              /* overflow inside the solve */
    case KSP_DIVERGED_PC_FAILED:             /* zero pivot / factor failure */
    case KSP_DIVERGED_DTOL:
    case KSP_DIVERGED_BREAKDOWN:
    default:
        *solve_status = LINEAR_SOLVE_FALL_BACK_TO_CL1;           /* caller may retry direct first */
        break;
    }
    PetscFunctionReturn(PETSC_SUCCESS);
}

/* Top-level adapter called from Phreeqc::ineq(). Never throws, never aborts.
   Any PETSc error code is converted into a cl1 fallback. */
LinearSolveStatus solve_newton_step_with_petsc(PetscNewtonSolver *solver, const LDBLE *augmented_newton_matrix, int count_unknowns,
                            LDBLE *delta)
{
    LinearSolveStatus solve_status = LINEAR_SOLVE_FALL_BACK_TO_CL1;
    DenseMatrixLayout layout  = phreeqc_newton_matrix_layout(augmented_newton_matrix, count_unknowns);
    PetscBool solution_is_finite;

    ++solver->solve_count;
    if (load_newton_system_into_petsc(solver, &layout, (PetscInt)count_unknowns, &solve_status) != PETSC_SUCCESS) goto petsc_error;
    if (solve_status != LINEAR_SOLVE_ACCEPTED) goto done;                 /* non-finite input or zero row */

    if (run_linear_solve(solver->active_slot, solver->active_slot->linear_solver, (PetscBool)!solver->primary_solver_is_direct, &solve_status)
        != PETSC_SUCCESS) goto petsc_error;

    if (solve_status == LINEAR_SOLVE_FALL_BACK_TO_CL1 && solver->active_slot->direct_lu_solver) {   /* iterative failed: try LU */
        ++solver->direct_retry_count;
        if (run_linear_solve(solver->active_slot, solver->active_slot->direct_lu_solver, PETSC_FALSE, &solve_status)
            != PETSC_SUCCESS) goto petsc_error;
    }

    if (solve_status == LINEAR_SOLVE_ACCEPTED || solve_status == LINEAR_SOLVE_ACCEPTED_INEXACT) {
        if (copy_solution_from_petsc(solver->active_slot, delta, 0, &solution_is_finite) != PETSC_SUCCESS) goto petsc_error;
        if (!solution_is_finite) solve_status = LINEAR_SOLVE_FALL_BACK_TO_CL1;
    }
done:
    if (solve_status == LINEAR_SOLVE_ACCEPTED)          ++solver->accepted_count;
    else if (solve_status == LINEAR_SOLVE_ACCEPTED_INEXACT) ++solver->inexact_accepted_count;
    else if (solve_status == LINEAR_SOLVE_FALL_BACK_TO_CL1) ++solver->cl1_fallback_count;
    return solve_status;
petsc_error:
    ++solver->cl1_fallback_count;
    return LINEAR_SOLVE_FALL_BACK_TO_CL1;
}
```

Constants: `MAX_BACKWARD_ERROR = 1e-10`, `INEXACT_ACCEPTANCE_RATIO = 1e-3`, both overridable via `-phreeqc_max_backward_error` and `-phreeqc_inexact_acceptance_ratio`.

### 6.4 Divergence Reasons: Causes and Actions

| `KSPConvergedReason` | Likely geochemical cause | Action |
|---|---|---|
| `KSP_DIVERGED_PC_FAILED` (check `PCGetFailedReason`: e.g. `PC_FACTOR_NUMERIC_ZEROPIVOT`) | Rank-deficient system: phase-rule violation (too many phases fixing the same component), redundant constraints, element with zero total, absent phase row | Iterative primary: retry direct. Direct: fall back to `cl1()`, whose L1 formulation handles rank deficiency. In debug runs, optionally re-solve with `PCSVD` and log the smallest singular values and cell ID. |
| `KSP_DIVERGED_NANORINF` | Overflow in activities (exp of large ln-activity), ln(0), corrupt Jacobian | Normally pre-empted by the loader's finiteness check (`LINEAR_SOLVE_NON_FINITE_INPUT`). If it occurs within the solve: fall back to `cl1()`; if that also fails, PHREEQC's existing recovery applies. |
| `KSP_DIVERGED_ITS` | Stagnation from ill-conditioning or weak ILU | Accept as inexact if ‖r‖/‖b‖ ≤ 10⁻³, else retry direct, then `cl1()`. |
| `KSP_DIVERGED_DTOL` | ILU with tiny pivots producing an unstable preconditioner | Retry direct, then `cl1()`. |
| `KSP_DIVERGED_BREAKDOWN` | Arnoldi breakdown (numerically singular or degenerate operator) | Retry direct, then `cl1()`. |
| Converged but η > 10⁻¹⁰ | Pivot growth, near-singular J | Fall back to `cl1()` (or config B refinement if enabled). |
| Converged, bounds violated (`step_satisfies_cl1_bounds` false) | Step would dissolve more mineral than present, or precipitate an undersaturated absent phase | Fall back to `cl1()` for this iteration. |

**Fallback ladder** (in order): primary KSP → direct LU retry (only if primary is iterative) → `cl1()` (existing code) → PHREEQC's existing failure recovery (`set_and_run_wrapper()` retries the model with modified step sizes, diagonal scaling, and other knobs; **[VERIFY]** the exact retry sequence in your version). PETSc never terminates the run on its own.

`LINEAR_SOLVE_NON_FINITE_INPUT` (non-finite Jacobian or residual) skips both PETSc and `cl1()`, since neither can produce a meaningful step from NaN input, and returns the same error code `ineq()` would return on a `cl1()` failure, so that PHREEQC's existing recovery engages.

### 6.5 Circuit Breaker and Diagnostics

- **Per-model circuit breaker:** after `PETSC_FAILURE_LIMIT_PER_MODEL` (default 3) fallbacks within one `model()` call, disable the PETSc path for the rest of that call. Reset at the next `model()` entry. This bounds the cost of pathological cells to a few wasted solves.
- **No per-iteration printing by default.** Expose `-phreeqc_petsc_statistics` to print, per instance at destruction: `solve_count`, `accepted_count`, `inexact_accepted_count`, `direct_retry_count`, `cl1_fallback_count`, `bound_violation_fallbacks`, `slot_creation_count`. In PhreeqcRM, aggregate across workers.
- `-phreeqc_ksp_converged_reason` and `-phreeqc_ksp_view` work through standard PETSc machinery for ad-hoc debugging.
- When PHREEQC does not own PETSc (host initialised it), do not push a global error handler. PETSc may then print tracebacks for handled failures; this is cosmetic, and the return-code handling above is unaffected.

### 6.6 Post-Factorisation Conditioning Check (Phase 1B)

**Why there is no pre-solve condition gate.** LAPACK `dgecon` estimates the reciprocal condition number *from the LU factors*, so a condition estimate costs a factorisation; there is nothing to save by computing it "before" the solve. A fixed threshold such as κ > 10¹⁵ would also reject many valid steps: geochemical Jacobians routinely have κ = 10¹⁰–10²⁰ and LU with partial pivoting still yields a usable Newton step. The backward-error test (§6.1) remains the primary acceptance criterion.

**The gap it addresses.** A nearly singular system with no exactly zero pivot can pass the backward-error test while returning a very large δ dominated by a near-null-space direction. `cl1()` would treat such a system differently (L1-minimal residual). PHREEQC's step limiting in `reset()` partially protects against this; the comparison data from §8.3 will show how much.

**Check.** After a successful dense LU, estimate rcond on the factors already computed. The cost is O(n²), negligible next to the O(n³) factorisation.

```cpp
/* Dense path only. [VERIFY] for your PETSc version that MATSEQDENSE LU stores
   the factor in LAPACK getrf layout in the matrix returned by PCFactorGetMatrix(). */
static PetscErrorCode estimate_reciprocal_condition_number(SizedSolverSlot *slot, KSP factored_solver,
                                                            PetscReal *reciprocal_condition)
{
    PC                 preconditioner;
    Mat                factor_matrix;
    const PetscScalar *factor_values;
    PetscInt           factor_leading_dimension;
    PetscReal          matrix_one_norm;
    PetscFunctionBeginUser;
    *reciprocal_condition = -1.0;                                    /* -1 = not available */
    PetscCall(MatNorm(slot->jacobian_matrix, NORM_1, &matrix_one_norm));       /* the Jacobian itself is not overwritten */
    PetscCall(KSPGetPC(factored_solver, &preconditioner));
    PetscCall(PCFactorGetMatrix(preconditioner, &factor_matrix));
    PetscCall(MatDenseGetLDA(factor_matrix, &factor_leading_dimension));
    PetscCall(MatDenseGetArrayRead(factor_matrix, &factor_values));
    /* LAPACKE_dgecon(LAPACK_COL_MAJOR, '1', system_size, factor_values, factor_leading_dimension, matrix_one_norm, reciprocal_condition)
       or the Fortran dgecon_ with 4*system_size work and system_size iwork entries. If calling dgecon_
       directly, account for the hidden character-length argument that
       gfortran passes; LAPACKE avoids the issue. Link the same LAPACK as PETSc. */
    PetscCall(MatDenseRestoreArrayRead(factor_matrix, &factor_values));
    PetscFunctionReturn(PETSC_SUCCESS);
}
```

**Policy.** `-phreeqc_rcond_mode {off,log,gate}` with `-phreeqc_rcond_min`.

| Mode | Behaviour | When |
|---|---|---|
| `off` | Not computed | Phase 1A |
| `log` | Computed and logged with the §8.3 δ-comparison record; never changes the decision | Phase 1B default |
| `gate` | rcond below `-phreeqc_rcond_min` → `LINEAR_SOLVE_FALL_BACK_TO_CL1` | Only after calibration |

Calibrate the threshold from data: plot the logged rcond (of the row-scaled matrix) against the PETSc-vs-`cl1()` δ discrepancy and set `rcond_min` where discrepancies begin to matter for converged results. Do not pick it in advance; because rows are equilibrated, the value will not correspond to the raw-κ figures quoted for unscaled Jacobians.

---

## 7. Configuration Surface

| Control | Mechanism | Default |
|---|---|---|
| Enable PETSc path | Build: `PHREEQC_USE_PETSC`; runtime: `-phreeqc_petsc_enable` or a new `KNOBS` identifier (e.g. `-petsc true`) | Build OFF; runtime ON when built |
| Mat type | `-phreeqc_mat_type {seqdense,seqaij}` | `seqdense` |
| KSP/PC | standard `-phreeqc_ksp_*`, `-phreeqc_pc_*` | PREONLY + LU |
| Row scaling | `-phreeqc_row_equilibration {0,1}` | 1 |
| Backward-error limit | `-phreeqc_max_backward_error` | 1e-10 |
| Inexact acceptance | `-phreeqc_inexact_acceptance_ratio` | 1e-3 |
| Circuit breaker | `-phreeqc_failure_limit_per_model` | 3 |
| A/B comparison mode | `-phreeqc_petsc_compare_with_cl1` (§8) | off |
| Conditioning check | `-phreeqc_rcond_mode {off,log,gate}`, `-phreeqc_rcond_min` (§6.6) | `off` in 1A, `log` in 1B |
| Statistics | `-phreeqc_petsc_statistics` (§6.5) | off |
| Options source | `PETSC_OPTIONS` env var or `-options_file` inside it | none |

---

## 8. Verification and Benchmarking Plan

### 8.1 Comparison Criteria (All Phases)

Exact equality of results is **not** an acceptance criterion once PETSc is active. A different linear solver changes rounding and can change Newton iteration counts; the converged state agrees with the baseline only to within PHREEQC's convergence tolerance. (The one exception is a build with `PHREEQC_USE_PETSC=OFF`, which must reproduce the baseline exactly.)

- **Data source:** `SELECTED_OUTPUT` with `-high_precision true`, compared numerically. Do not diff the main `.out` file, whose formatting and diagnostic lines legitimately vary.
- **Quantities:** element totals and species molalities, pH, pe, ionic strength, mass of water, saturation indices, phase amounts, and exchange/surface composition where present.
- **Tolerance:** pass if |v_new − v_ref| ≤ rtol·|v_ref| + atol, with starting values rtol = 10⁻⁶; atol = 10⁻¹⁴ mol (or mol/kgw) for amounts and molalities; atol = 10⁻⁶ for logarithmic quantities (pH, pe, SI, log activities). Tighten only if the observed distribution supports it; never to bitwise.
- **Reported, not asserted:** Newton iterations per calculation for both solvers, and the PETSc counters (`solve_count`, `accepted_count`, `cl1_fallback_count`, `slot_creation_count`, and bypass count from the eligibility gate).
- **Governance:** a coding agent must not change tolerances, edit baseline files, or alter test inputs to make a comparison pass. Any mismatch is triaged first with the δ-comparison log (§8.3); tolerance changes require human sign-off.

### 8.2 Test Matrix by Phase

| Input | Chemistry | Phase | Expected PETSc behaviour |
|---|---|---|---|
| Unit tests (§4.3) | Asymmetric 3×3, scaling, padding, cache | 1A | All pass before any chemistry run |
| ex1 | Seawater speciation, no equilibrium phases | 1A | All calculations eligible; `accepted_count == solve_count`; results within §8.1 tolerance. If any calculation is bypassed, report which unknown type triggered it. |
| ex2 | Equilibrium with pure phases (gypsum/anhydrite vs. temperature) | 1A | **Gate test:** solution-only calculations may use PETSc; every calculation with pure-phase unknowns must bypass PETSc (bypass count > 0). Results within tolerance. |
| ex6 | Reaction path across phase boundaries (K-feldspar weathering) | 1B | Frequent bound fallbacks near phase transitions are expected. Measure and report the rate; it is a property of the chemistry, not a pass/fail threshold. |
| ex8 | Surface complexation | 1B | After surface unknowns are audited and added to the allow-list. Conditioning stress test; log rcond (§6.6). |
| ex11 | Transport with cation exchange | 1B | Exchange unknowns; many cells exercise the size cache. |
| ex22 | Gas solubility (CO₂ at high pressure) | 1B | Gas-phase unknowns (bounded). |
| PhreeqcRM test suite | Coupled cells | 1C | First `nthreads = 1`, then `nthreads = 4` with a thread-safe PETSc build: no crashes, results within tolerance, per-thread counters consistent. |

Stress cases for 1B/1C, beyond the examples: high-ionic-strength brines with Pitzer or SIT; extreme redox; pure phases near zero moles; solid solutions; trace elements near 10⁻¹² mol. Track fallback rates per category.

### 8.3 δ-Comparison Mode (`-phreeqc_petsc_compare_with_cl1`, from Phase 1A)

In every eligible `ineq()` call, solve with PETSc into scratch, then run `ineq_cl1_original()`, which steers the simulation. Log per call: n, PETSc status, `cl1` return code, backward error η, the normwise difference rel = ‖δ_PETSc − δ_cl1‖∞ / ‖δ_cl1‖∞, the sign check relative_sum = ‖δ_PETSc + δ_cl1‖∞ / ‖δ_cl1‖∞, and the index and `x[i]->type` of the largest difference; from 1B also rcond and whether bounds were active. (Rev 3: a componentwise ratio with a 10⁻³⁰ floor, as in Rev 2, flags harmless noise in near-zero components; the normwise form does not.)

This mode is included in Phase 1A even though the full harness is a 1B item, because it is the only check of the residual sign convention and of the equality-row selection before PETSc steers anything. It is roughly 40 lines of code. relative_sum ≈ 0 (equivalently rel ≈ 2) means the sign convention is inverted; discrepancies confined to one unknown type point to a row that `ineq()` treats specially.

### 8.4 Performance and Lifecycle

- **Baseline profile first:** measure the fraction of wall time spent in `cl1()` on representative inputs, which bounds the achievable speed-up.
- **Performance:** compare wall time and per-solve latency (`-log_view` events `KSPSolve`, `PCSetUp`, `MatAssemblyEnd`) against `cl1()`, at `nthreads = 1` first. Report the bypass and fallback fractions alongside timing.
- **Memory/lifecycle:** Valgrind or ASan, plus `-malloc_dump` when PHREEQC owns PETSc, to confirm all objects are destroyed before `PetscFinalize()` in all three entry points, including repeated IPhreeqc create/destroy cycles.

Acceptance for enabling PETSc by default (end of Phase 1): all §8.2 rows within tolerance; no increase in convergence failures; fallback and bypass rates measured and documented; non-negative performance impact on profiled workloads.

---

## 9. Forward Compatibility (Phase 2 and Beyond)

- **Chemistry stays sequential.** Each cell's equilibrium problem is independent and small. PhreeqcRM's MPI mode distributes cells across ranks; each rank's workers still solve local n×n systems. Keep `PETSC_COMM_SELF` for these objects permanently. Distributing an n = 40 matrix over ranks would only add communication latency.
- **What Phase 2 actually changes:** PETSc must be initialised after `MPI_Init` and use the host's MPI communicator for `PETSC_COMM_WORLD`, which is relevant only for logging and the options database. The ownership and refcount rules in §5.1 already cover this.
- **Where distributed PETSc becomes relevant:** only for a globally implicit reactive-transport formulation, where transport and chemistry are solved as one coupled system. That is a different solver architecture (MPIAIJ or block matrices with `PCFIELDSPLIT`, chemistry blocks as local preconditioners) and should be scoped as a separate project rather than an extension of this adapter.
- Keep the adapter (`petsc_linear_solver`) as a self-contained module with a narrow interface (`acquire`, `release`, `ctx_create`, `ctx_destroy`, `newton_step`). That isolation is what makes later replacement of internals cheap.

---

## 10. Open Items to Verify in Source Before Coding

| # | Item | Where to look | Why it matters |
|---|---|---|---|
| 1 | Layout of the augmented Newton matrix (row-major, `leading_dimension = n+1`, RHS in column n, 0-based) | `residuals()`, `jacobian_sums()` in `model.cpp` | Drives `phreeqc_newton_matrix_layout()` |
| 2 | Exact equality subsystem passed to `cl1()`: which rows become inequalities, which are dropped or rescaled | `ineq()` | Defines what PETSc must solve and what `step_satisfies_cl1_bounds()` checks |
| 3 | Sign convention of RHS vs `delta` | `ineq()`, `reset()` | Correctness; confirm with A/B harness |
| 4 | Outputs of `ineq()` besides `delta` (status codes, residual info) | `ineq()` and callers | PETSc path must reproduce them |
| 5 | Whether the `cl1()` path reads `delta` on entry | `ineq()` | Determines scratch-buffer need in §4.5 |
| 6 | `LDBLE` type in your build | `phrqtype.h` | Conversion path |
| 7 | Where `diagonal_scale` is applied | `ineq()` / KNOBS handling | Avoid double scaling |
| 8 | `Phreeqc` copy semantics (`InternalCopy`, copy constructor) | `Phreeqc.cpp` | `petsc_solver` must not be shallow-copied |
| 9 | Failure-recovery sequence on `ineq()` failure | `set_and_run_wrapper()` | Fallback ladder's final rung |
| 10 | PhreeqcRM threading model in your build (OpenMP vs MPI, thread count) | PhreeqcRM build config | Thread-safety gating in §5.5 |
| 11 | Names of the unknown-type constants, and which types `ineq()` treats as pure equalities | `global_structures.h`, `ineq()` | Defines the Phase 1A allow-list (§3.4) |
| 12 | Whether `ineq()` adds constraint rows even for solution-only systems (e.g. for mass of water) | `ineq()` | Would shrink or invalidate the allow-list |
| 13 | CMake project and target names for phreeqc, IPhreeqc, PhreeqcRM | `CMakeLists.txt` files | Build integration (§3.3) |

Items 1, 3–6, and 11–13 gate Phase 1A. Items 2, 7, and 9 gate Phase 1B. Items 8 and 10 gate Phase 1C.

---

## 11. Phased Rollout

Each phase ends with a human review of the reported results before the next begins.

| Phase | Scope | Not in scope | Exit criteria |
|---|---|---|---|
| **1A** | Global acquire/release with refcount and mutex (§5.1); context and size-keyed cache (§3.2, §5.4); dense LU only (config A); loader with row scaling and finiteness checks (§4.2–4.3, `MatSetValues` path); store to scratch (§4.5); backward-error check (§6.1); `ineq()` → `ineq_cl1_original()` rename and dispatcher; eligibility gate (§3.4); δ-comparison mode (§8.3); counters; CMake option. | AIJ/GMRES, direct-retry KSP, inexact acceptance, `step_satisfies_cl1_bounds()`, circuit breaker, rcond, dense fast path, threading beyond `nthreads = 1`. | Build with option OFF reproduces baseline exactly; unit tests (§4.3) pass; ex1 and ex2 meet §8.2; δ-comparison on ex1 shows no systematic discrepancy; Valgrind/ASan clean. |
| **1B** | `ineq()` audit; `step_satisfies_cl1_bounds()`; extend allow-list (exchange, surface, then bounded types with bound checks); circuit breaker (§6.5); rcond in log mode (§6.6); `LINEAR_SOLVE_NON_FINITE_INPUT` short-circuit per §6.4. | Iterative path, threading. | ex6, ex8, ex11, ex22 meet §8.2; fallback rates documented; rcond threshold calibrated (or gate left off). |
| **1C** | Config C (AIJ + GMRES + ILU) with direct retry (§6.2–6.3); iterative tolerances; thread-safe PETSc build and `nthreads > 1`; PhreeqcRM suite; performance study. | MPI (Phase 2). | PhreeqcRM row of §8.2; performance report (§8.4). |

The backward-error check is in 1A rather than 1B because it is a few lines of code and can only move steps from PETSc to `cl1()`, never the reverse, so it makes 1A strictly safer.

---

---

## 12. Implementation Amendments (Revision 3)

These amendments supersede the corresponding earlier text where they differ. They are already reflected in the Phase 1A task prompts.

**12.1 Class layout independent of `USE_PETSC`.** `Phreeqc.h` is compiled by several targets. A member present only under `#ifdef USE_PETSC` would give translation units built with and without the definition different class layouts, which is undefined behaviour and appears as memory corruption. `Phreeqc` therefore has a single unconditional opaque member, `PetscSolverIntegrationState *petsc_solver_state` (forward-declared struct, `nullptr` by default), plus unconditional declarations of `ineq_cl1_original()` and `is_eligible_for_petsc_phase1a()`, and an inline accessor `augmented_newton_matrix()` that returns the augmented Newton matrix. The accessor is the only place in new code where PHREEQC's internal identifier for that array appears. All PETSc state (context, scratch buffer, flags, compare log) lives inside `PetscSolverIntegrationState`. In addition, `USE_PETSC` is a PUBLIC compile definition of the adapter library, which every core target links. Copies of a `Phreeqc` object always start with `petsc_solver_state = nullptr`.

**12.2 Acquisition and finalization.** This replaces the refcount-to-zero finalization in §5.1–5.2. `acquire_petsc()` is called when a solver context is created (lazily, on the first `ineq()` call), and `release_petsc()` when it is destroyed. Release **never** finalizes PETSc, because re-initialisation is not reliable: a host that creates and destroys IPhreeqc instances repeatedly would otherwise silently lose PETSc after the first instance. Finalization happens only in two places. The phreeqc executable's `main()` pins PETSc and calls `shutdown_petsc_if_owned_by_phreeqc()` after the `Phreeqc` object's scope closes. For library hosts, an `atexit` handler registered at initialisation finalizes PETSc if PHREEQC owns it and no references remain. Context destruction checks `PetscFinalized()` and skips PETSc destroy calls if PETSc is already gone, which makes objects with static storage duration safe. Host buffers use `std::vector` rather than `PetscMalloc`, so they can be freed after finalization.

**12.3 Loader decoupled from the cache.** `load_newton_system_into_petsc()` takes the `Mat`, the `Vec`, and caller-provided buffers rather than the context, so it can be unit-tested without solver objects. A first pass checks that every Jacobian and RHS entry is finite. Non-finite input is therefore reported as `LINEAR_SOLVE_NON_FINITE_INPUT` even when a zero row appears earlier, and `copy_solution_from_petsc()` checks the whole vector before writing anything.

**12.4 Backward-error denominator.** Revision 2 added `PETSC_SMALL` to the denominator. `PETSC_SMALL` is 10⁻¹⁰ in double precision. Once ‖x‖ and ‖b‖ shrink near Newton convergence on a row-scaled system, ‖A‖‖x‖ + ‖b‖ can become comparable to or smaller than this constant. η is then understated, and bad solves can be accepted exactly where accuracy matters most. Fixed in §6.3.

**12.5 Phase 1A `ineq()` structure.** The original body is renamed `ineq_cl1_original()` with only the name changed. The new `ineq()` dispatcher creates the state lazily, counts gate bypasses, and in compare mode runs PETSc first (it reads the augmented Newton matrix before `cl1` can modify it) and then lets `ineq_cl1_original()` steer.

**12.6 Naming conventions (Revision 4).** All new files, types, functions, variables, options, tests, and result directories use descriptive names. The main correspondences are:

| Role | Name |
|---|---|
| Source directory | `petsc_linear_solver/` |
| PETSc initialisation and finalisation | `petsc_initialization.h/.cpp`: `acquire_petsc()`, `release_petsc()`, `shutdown_petsc_if_owned_by_phreeqc()`, `petsc_global_state_mutex()` |
| Solve status | `linear_solve_status.h`: `LinearSolveStatus` with `LINEAR_SOLVE_ACCEPTED`, `LINEAR_SOLVE_FALL_BACK_TO_CL1`, `LINEAR_SOLVE_NON_FINITE_INPUT` |
| Legacy-array transfer | `newton_matrix_transfer.h/.cpp`: `DenseMatrixLayout`, `layout_jacobian_entry()`, `load_newton_system_into_petsc()`, `copy_solution_from_petsc()` |
| Solver and cache | `petsc_newton_solver.h/.cpp`: `PetscNewtonSolver`, `SizedSolverSlot`, `solve_newton_step_with_petsc()` |
| PHREEQC-side state | `petsc_solver_integration.h/.cpp`: `PetscSolverIntegrationState` (member `Phreeqc::petsc_solver_state`) |
| Library target | `phreeqc_petsc_linear_solver` |
| Renamed original `ineq()` body | `ineq_cl1_original()` |
| Accessor for the Jacobian and residual array | `Phreeqc::augmented_newton_matrix()` |

PHREEQC's own identifiers (`ineq`, `delta`, `count_unknowns`, `x`, `LDBLE`, the unknown-type constants) and PETSc API names are used unchanged. Loop indices `i` and `j` keep the usual row/column meaning.

## 13. Task 0 Findings and Reviewer Decisions (Revision 5)

The source verification (`task0_source_verification_report.md`) and the reviewer's resolution (`task0_reviewer_decisions.md`, decisions D1–D13) supersede earlier sections where they differ. In summary:

- **Build scope (D2).** The tree builds a single executable, `phreeqc`, with every source compiled into it; there is no IPhreeqc or PhreeqcRM here. The adapter library is linked with the plain `target_link_libraries` form to match the existing call. §5.2's IPhreeqc and PhreeqcRM rows apply only when this work is carried into those code bases.
- **Equality status (D3).** `ineq()` routes ALK and SOLUTION_PHASE_BOUNDARY rows to `cl1()`'s optimization block. With no inequality rows, the L1 problem has the exact solution of the square system as its unique zero-objective minimizer whenever the Jacobian is nonsingular, so both types are eligible. This supersedes the Phase 1A allow-list in §3.4.
- **Modified systems (D4–D7).** PETSc is not used for retry calls (`in_kode != 1`, which may see a column-scaled matrix), Pitzer/SIT models, `mass_water_switch`, iterations below `aqueous_only`, the unstable-phase early return, or numerical-derivative mode. The full eligibility rule is D8; each failed condition is counted separately.
- **State audit (B11).** Of the members `ineq()` writes, only `remove_unstable_phases`, and the augmented Newton matrix via `numerical_jacobian()`, can be read before they are rewritten. Both cases are excluded by D8.
- **Lifecycle (D9).** The state pointer uses a default member initializer, so PHREEQC's user-defined copy constructor and copy assignment need no edits. `main_method` copies the `Phreeqc` object (`src/class_main.cpp:159`), and each copy creates its own solver state lazily.
- **Output comparison (D11).** Main output contains a wall-clock line; comparisons ignore exactly that line and its separators. Selected output is compared byte for byte.
- **Residual sign (B6).** Confirmed as J·δ = r with r = target − current, which matches the loader as specified.

---

## Appendix A: Options Cheat Sheet

```
# Default: dense LU (no options required)
PETSC_OPTIONS="-phreeqc_petsc_statistics"

# Iterative alternative (config C)
PETSC_OPTIONS="-phreeqc_mat_type seqaij -phreeqc_ksp_type gmres \
  -phreeqc_ksp_gmres_restart 100 -phreeqc_pc_type ilu -phreeqc_pc_factor_levels 1 \
  -phreeqc_pc_factor_shift_type nonzero -phreeqc_ksp_pc_side right \
  -phreeqc_ksp_norm_type unpreconditioned -phreeqc_ksp_rtol 1e-8"

# Iterative refinement escalation (config B)
PETSC_OPTIONS="-phreeqc_ksp_type richardson -phreeqc_ksp_max_it 3 -phreeqc_pc_type lu"

# Sparse direct with pivoting (config D; PETSc built with SuiteSparse)
PETSC_OPTIONS="-phreeqc_mat_type seqaij -phreeqc_ksp_type preonly \
  -phreeqc_pc_type lu -phreeqc_pc_factor_mat_solver_type umfpack"

# Rank diagnostics on difficult cells (config E, debug only)
PETSC_OPTIONS="-phreeqc_pc_type svd -phreeqc_pc_svd_monitor"

# Validation and profiling
PETSC_OPTIONS="-phreeqc_petsc_compare_with_cl1 -phreeqc_ksp_converged_reason -log_view"
```

---

## Appendix B: Phase 1A Implementation Prompt

The same text is provided as a separate file, `phase1a_scope_summary_superseded.md`. **Superseded for implementation by the task prompts in `docs/petsc_linear_solver/implementation_tasks/` (Revision 3)**, which split this work into six reviewed steps and include the §12 amendments. It is kept as a summary of Phase 1A scope.


You are implementing **Phase 1A** of the PETSc integration described in `petsc_serial_design.md` (Revision 2). That document is the specification. Where this prompt and the document disagree, stop and ask; do not choose.

### Step 0: Verify before writing any code

Read the PHREEQC source and report findings for design §10 items **1, 3, 4, 5, 6, 11, 12, 13**. For each item, quote the relevant lines (file and line numbers) and state whether the design's assumption holds.

In particular:

- **Item 1:** confirm the augmented Newton matrix is row-major, 0-based, with leading dimension `count_unknowns + 1` and the residual in column `count_unknowns`.
- **Item 11:** list the unknown-type constants and their names exactly as defined.
- **Item 12:** state whether `ineq()` adds, drops, or rescales any rows when every unknown is one of `MB, ALK, CB, MU, AH2O, MH, MH2O`.

**Stop after Step 0 and wait for approval** if any of the following holds:

- the layout differs from design §4.2;
- any allow-listed type is treated specially by `ineq()`;
- `ineq()` has side effects beyond `delta` that callers use.

### Scope (implement only this)

1. `src/petsc_linear_solver/petsc_initialization.h/.cpp`: `acquire_petsc()`, `release_petsc()`, `petsc_global_state_mutex()`, finalize policy (design §5.1). Use `PetscInitializeNoArguments()`; never pass `argv` to PETSc.
2. `src/petsc_linear_solver/petsc_newton_solver.h/.cpp`: context struct, 8-slot size-keyed LRU cache, slot create/destroy under `std::lock_guard` on `petsc_global_state_mutex()` (design §3.2, §5.4). Dense LU only: `MATSEQDENSE`, `KSPPREONLY`, `PCLU`, natural ordering, options prefix `phreeqc_`, `KSPSetFromOptions()` once per slot. Solve, classify (`reason > 0` plus backward error ≤ `1e-10`, design §6.1), counters.
3. `src/petsc_linear_solver/newton_matrix_transfer.h/.cpp`: `DenseMatrixLayout` descriptor and `layout_jacobian_entry()` (design §4.2); row-scaled load via per-row `MatSetValues()` with NaN/Inf and zero-row detection (design §4.3); store to a scratch buffer with a finiteness check (design §4.5).
4. In PHREEQC: rename the existing body of `ineq()` to `ineq_cl1_original()` **without modifying it**, add the Phase 1A dispatcher and `is_eligible_for_petsc_phase1a()` exactly as in design §3.4, add the per-instance context pointer and scratch buffer, and add acquire/release in the `Phreeqc` constructor and destructor (design §5.2). Copies of a `Phreeqc` object must set the context pointer to `NULL`.
5. `-phreeqc_petsc_compare_with_cl1` δ-comparison mode (design §8.3, without rcond and bound fields).
6. `-phreeqc_petsc_statistics` counter printout at instance destruction, including the eligibility-gate bypass count.
7. CMake option `PHREEQC_USE_PETSC` (default `OFF`) using pkg-config, applied to the real target names found in Step 0.
8. `tests/petsc_linear_solver/test_newton_matrix_transfer.cpp`: the asymmetric 3×3 unit test and variants (a)–(e) from design §4.3.

### Out of scope (do not implement)

AIJ/GMRES, the direct-retry KSP, inexact acceptance, `step_satisfies_cl1_bounds()`, circuit breaker, rcond estimation, the dense fast path (`MatDenseGetArrayWrite`), any pre-factorisation condition-number gate, and OpenMP thread support beyond `nthreads = 1`.

### Rules

- Adapter code is C++. Use `PetscCall()` inside functions returning `PetscErrorCode`. At the boundary with PHREEQC, convert any non-zero PETSc error code into a fall-through to `ineq_cl1_original()`. Never abort, never throw.
- In Phase 1A, every status other than `LINEAR_SOLVE_ACCEPTED` falls through to `ineq_cl1_original()`, including `LINEAR_SOLVE_NON_FINITE_INPUT`.
- PETSc reads the augmented Newton matrix **before** `ineq_cl1_original()` runs and writes only to scratch. `delta` is written only after the step is accepted.
- Do not change PETSc dense indexing. Element (i, j) of a `MATSEQDENSE` array is at `j*lda + i`.
- With `PHREEQC_USE_PETSC=OFF`, compiled behaviour must be unchanged.
- Do **not** change tolerances, baseline files, or test inputs to make a comparison pass. Report mismatches instead.

### Validation (run in this order and report each)

1. Build with the option OFF. Run ex1 and ex2. Results must match the unmodified baseline **exactly**.
2. Build with the option ON. Run the unit tests; all must pass.
3. ex1 with `-phreeqc_petsc_statistics`. Expect `accepted_count == solve_count` and zero bypasses. Compare `SELECTED_OUTPUT` (`-high_precision true`) against baseline numerically: pass if |new − ref| ≤ 1e-6·|ref| + atol, with atol = 1e-14 for amounts and molalities and 1e-6 for pH, pe, SI and log activities. Report the maximum deviation per quantity and the Newton iteration counts for both runs.
4. ex2 with `-phreeqc_petsc_statistics`. Expect bypass count > 0 (pure-phase calculations must use `cl1`). Same tolerance comparison.
5. ex1 with `-phreeqc_petsc_compare_with_cl1`. Report the distribution of max relative δ differences and any unknown type with systematic discrepancies.
6. Valgrind or ASan on ex1 and ex2 with the option ON: no leaks, no invalid accesses, no PETSc object destroyed after `PetscFinalize()`.

### Report format

For each step above, give pass/fail and the numbers, followed by a list of files changed with a one-line description each. List any deviation from the design and the reason for it.

Do not start Phase 1B work.
