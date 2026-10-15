# Phase 1B Design: Pure Phases, Gases and Solid Solutions, by Solving cl1's Own Problem

| Field | Value |
|---|---|
| Status | Draft for review, Revision 1 (2026-10-14) |
| Builds on | Phase 1A (serial integration), Phase 2 (backend-neutral layer, LAPACK backend, condition gate, PhreeqcRM port; closed by P2-D21) |
| Verification | Items marked **[VERIFY]** are checked against PHREEQC v3.9.0 (`b0b3be76`) in Phase 1B Task 0 before any code is written. Line numbers below come from a recent phreeqc3 checkout and may differ |

## 1. Why Phase 1B, and What It Must Achieve

**Facts so far:**
- **The linear-solve step dominates.** In the CO₂–basalt profile, `ineq()` takes 57.5 % of the run time, and `cl1()` alone 55.4 %, at about 13.6 µs per call.
- **Almost nothing is eligible today:** 0.0017 % of calls in the profile, and 0.01–0.02 % in the PhreeqcRM benchmark. Nearly all ineligible calls carry pure-phase unknowns.
- **The LAPACK path itself is ready:** it is correct, gated against ill-conditioning, deterministic across threads and ranks, and free of measurable overhead.

**Goal:** route most of those calls to the LAPACK path, **without ever changing the answer that `cl1()` would give**, and with an automatic fall back to `cl1()` whenever equivalence cannot be established for a call.

## 2. The Problem cl1 Actually Solves

`ineq()` does not hand `cl1()` the Newton system. It builds a **constrained L1 problem** in `ineq_array`, and Phase 1A intercepted *before* that construction. The structure below is taken from the source and marked [VERIFY] for Task 0. `cl1.cpp` documents the problem as:

> minimise Σ|b − A x| over k rows, subject to C x = d (l rows) and E x ≤ f (m rows), with optional sign restrictions on the variables and on the residuals of the A rows, selected with KODE = 1.

`ineq()` always calls `cl1()` with `kode = 1`. [VERIFY]

| Block | Rows | Contents, as built by `ineq()` |
|---|---|---|
| **A** (minimised), k rows | `l_count_optimize` | One row for each pure phase that is present or supersaturated; ALK; SOLUTION_PHASE_BOUNDARY; GAS_MOLES (if the gas phase is in); SS_MOLES (if the solid solution is in). For pure-phase, gas and SS rows, `res[i] = 1`, i.e. residual ≥ 0. With `in_kode ≠ 1` these flags are reset to 0, and pure-phase rows are scaled by `pp_scale`. |
| **C** (equalities), l rows | `count_equal` | MB, CB, MU, AH2O, MH, MH2O, EXCH, SURFACE, SURFACE_CB*, pure phases with `force_equality`, PITZER_GAMMA (full Pitzer). Rows for exchangers and surfaces with negligible mass are skipped. |
| **E** (≤ inequalities), m rows | the rest | For a present pure phase: δᵢ ≤ molesᵢ (cannot dissolve more than is present). For `dissolve_only`: −δᵢ ≤ initial − moles. For gas: −δᵢ ≤ molesᵢ. For SS: δᵢ ≤ 0.99·moles − MIN_TOTAL_SS. With Pitzer/SIT: an MH2O row. With `negative_concentrations`: MB rows. |
| **Variable signs** | `delta1[j]` on entry | `−1` for a pure phase with moles ≤ 0 that is in the model and not excluded (it may only precipitate). Otherwise 0, unrestricted. |
| **Excluded unknowns** | whole column set to 0 | Pure phases not in the model, or absent and undersaturated; satisfied `dissolve_only` phases; exchangers and surfaces with negligible mass; a gas phase that is not in; an SS that is not in; the mass-of-water column when `mass_water_switch` is on. |

There are also column scalings (`normal[]` for small columns, `pp_column_scale` for pure-phase and SS columns) and, for Pitzer/SIT without full Pitzer, a compacted layout (`SHRINK_ARRAY`, n = count_unknowns − |s_list|). After `cl1()`: δ = x · normal, and `res[]` is used only if `kode ≠ 0`. [VERIFY]

## 3. Key Idea: a Feasible Zero-Residual Solution Is cl1's Answer

Let J be the set of columns that are not all zero, i.e. the unknowns not excluded, and let M be the (k + l) × |J| matrix of the A and C rows restricted to J.

**Claim.** Suppose that:
1. k + l = |J| (the system is square);
2. M is nonsingular (checked by the reciprocal-condition gate);
3. x_J solves M x_J = [b; d] exactly, and x_j = 0 for the excluded columns;
4. x satisfies every E row (E x ≤ f) and every variable sign restriction.

Then x is the **unique** optimum of cl1's problem.

**Proof sketch.** Every residual of the A rows is 0, so the objective is 0, its lower bound. The residual sign restrictions (≥ 0 or ≤ 0) hold at 0, and conditions 3 and 4 make x feasible, so x is optimal. Any other optimum also has objective 0, so all of its A residuals are 0. It satisfies C exactly, so it solves the same nonsingular system on J. On the excluded columns `cl1()` returns 0. That last point is a property of the simplex on all-zero columns, and is marked [VERIFY] to be checked empirically in Task 1.

**Consequence.** Whenever the four conditions hold, the LAPACK solution is the answer `cl1()` would compute, up to `cl1()`'s own tolerance. The integration accepts the LAPACK step only then, and otherwise calls `cl1()`. When a bound is active (a mineral is being exhausted, an absent phase would dissolve, gas would go negative) the check fails and `cl1()` handles it, exactly as today. This is the standard "optimistic solve, then verify" approach of active-set methods.

**What this replaces.** Phase 1A asked "is this unknown type safe?" (D8). The new rule asks "is this cl1 problem square, well-conditioned and feasible at its zero-residual point?" The question applies to every unknown type, because the bounds are read from the rows `ineq()` built, never re-derived.

## 4. Integration Point

**H1. A hook immediately before the `cl1()` call in `ineq()`.** This is after every row, column and scaling modification, so the hook sees exactly what `cl1()` would see. It replaces the call with:

```
if (!newton_solver_try_cl1_problem(state, in_kode, k, l, m, n, stride, &ineq_array[0],
                                   &delta1[0], &res[0], &l_kode, &l_iter, &l_error))
    cl1(...unchanged...);
```

On acceptance it:
- writes `delta1` (x, in cl1's scaled variables);
- writes `res` (b − Ax, d − Cx and f − Ex);
- sets `l_kode = 0`, `l_iter = 0`, and `l_error = Σ|A residuals|`, which is ≈ 0.

The rest of `ineq()` (δ = x·normal, column rescaling, return code) runs unchanged. **This is the first edit inside the body of `ineq_cl1_original`.** It replaces Phase 1's rule "rename only" with "rename plus one guarded hook at the `cl1()` call". It is a reviewer decision (§9).

**H2. The Phase 1A entry dispatcher.** It stays in the code. A new variable selects the interception point: `PHREEQC_LINEAR_SOLVER_INTERCEPT=entry|cl1_problem`. `entry` is the default until Task 3 validates `cl1_problem`. Both are never active at once. For the aqueous-only systems of D8, both formulations solve the same square system; they differ only by `ineq()`'s small pre-modifications (an MH diagonal increment of MIN_TOTAL, and column scaling).

**H3. Initial exclusions,** kept until their own audit: `in_kode ≠ 1` (retries, different residual flags and `pp_scale`), Pitzer/SIT (compacted layout, MH2O inequality), numerical derivatives (D17), and `mass_water_switch`. Each is counted as its own ineligibility reason.

## 5. The Feasibility and Acceptance Check

The adapter receives the dense `ineq_array`, with row stride `max_column_count`, or n + 2 under `SHRINK_ARRAY`. It:

1. **Finds the columns.** J = {j < n : some row among the k + l + m has a nonzero entry in column j}. Whether this correctly identifies the excluded unknowns, without false positives from columns that happen to be zero, is checked in Task 1. A naturally zero column makes M singular anyway, and the gate then falls back to `cl1()`.
2. **Checks squareness:** if k + l ≠ |J|, fall back (reason `not_square`).
3. **Builds M and the right-hand side** from the A and C rows on J, then row-equilibrates, factors (`dgetrf`), checks the reciprocal condition (`dgecon`, gate 10⁻⁸, P2-D19) and solves (`dgetrs`). Reasons: `singular` or `ill_conditioned`.
4. **Checks residuals:** backward error ≤ 10⁻¹⁰ (reason `backward_error`).
5. **Checks feasibility:**
   - every E row must satisfy f − E·x ≥ 0 (reason `inequality_violated`, counted by the unknown type of the row's `back_eq`);
   - every sign restriction must hold, i.e. x_j ≤ 0 where `delta1[j] = −1` on entry, and x_j ≥ 0 where it is +1 (reason `sign_violated`).

   **There is no tolerance in this direction.** A violation of any size falls back to `cl1()`, because `cl1()` itself would then return a different, bound-active solution.
6. **Accepts** otherwise.

Cost per call is about k·|J| + m·n operations for copying and checking, plus the LU. At n ≈ 20 that is a few microseconds, against `cl1()`'s roughly 13.6 µs average.

## 6. Comparison and Shadow Modes

- **Shadow mode** (`PHREEQC_LINEAR_SOLVER_CL1_PROBLEM=shadow`, Task 1). The hook classifies and solves, but **never steers**: `cl1()` is always called. The hook then compares x with `cl1()`'s `delta1` (the normwise relative difference on J, and the maximum |x_j| on excluded columns) and logs one record per call. This measures, before any steering:
  - the square fraction;
  - the feasible fraction;
  - agreement wherever the check would accept, expected at round-off level, or at `cl1()`'s tolerance;
  - every case where it would accept but `cl1()` disagrees, which would falsify the claim of §3.
- **Steering mode** (`=steer`, Task 2). The hook accepts or falls back. Comparison mode keeps working as in Phase 2.

## 7. Plan

| Task | Content | Gate |
|---|---|---|
| 1B-T0 | Read-only source audit of §2 against v3.9.0: the `cl1()` interface and KODE semantics; every row, sign, residual flag and column exclusion in `ineq()` with line numbers; scalings; the `SHRINK_ARRAY` layout; how `delta1`, `res`, `l_kode`, `l_iter` and `l_error` are used after `cl1()`; the caller's use of the return code. Lists the conditions under which k + l ≠ |J| is possible. | Every [VERIFY] answered |
| 1B-T1 | Shadow-mode hook (phreeqc3), per-call log, and an analysis tool. Runs: ex1–ex8, `units.pqi`, the CO₂–basalt input. | The claim is not falsified, and the square and feasible fractions are measured |
| 1B-T2 | Steering mode, with `PHREEQC_LINEAR_SOLVER_INTERCEPT=cl1_problem`. Validation against `cl1()` runs with the P2-D19 (e) tolerances, statistics by reason, ASan. | Outputs within tolerance; no unexplained difference |
| 1B-T3 | Performance: the CO₂–basalt profile in phreeqc3, and the PhreeqcRM benchmark (Phase 2 harness). Then the port to PhreeqcRM and the Phase 2 validation set. | Measured speed-up reported; PhreeqcRM results within tolerance |
| 1B-T4 (optional) | Lifting the H3 exclusions one at a time, each with its own audit | Per exclusion |

## 8. Expected Effect (to be measured, not promised)

The gain is bounded by the share of calls the check accepts (call it p) and by the cost difference per call.

As an illustration, if p = 0.8 and an accepted call costs 4 µs instead of 13.6 µs, then in the CO₂–basalt profile (1.43 million calls in 34 s) the saving is about 0.8 × 9.6 µs × 1.43 × 10⁶ ≈ 11 s. That is roughly 1.5 times faster overall, against the Amdahl ceiling of 2.35. p is unknown until Task 1 measures it. While minerals are actively dissolving to exhaustion, bounds are active and p falls.

## 9. Open Decisions for the Reviewer

1. **The hook inside `ineq_cl1_original` (H1)** relaxes the Phase 1 rule. Is it acceptable as one guarded hook at the `cl1()` call?
2. **The default interception point** after Task 2: `cl1_problem`, keeping `entry` for regression comparison.
3. **The order of lifting H3 exclusions** (1B-T4), if at all.
4. **The feasibility tolerance:** zero, i.e. strict (§5). Should cases be observed in which `cl1()` accepts slightly infeasible points within its own `toler`, Task 1 will report them, and the reviewer decides then.
