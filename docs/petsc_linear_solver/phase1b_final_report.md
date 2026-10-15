# Phase 1B Final Report: Solving cl1's Problem by LU

| Field | Value |
|---|---|
| Status | **Closed** (decision B-D12, 2026-10-14) |
| Outcome | Steering implemented, validated and measured. **Not adopted as a default.** Kept as an opt-in experimental mode |
| Tags | `petsc-phase1b-task0-audit` … `petsc-phase1b-task3` |

## Goal

Make the Newton linear solves of systems with pure phases, gases and solid solutions eligible for the fast LAPACK path. That is where PHREEQC spends most of its linear-solve time, and the profile showed that only 0.0017 % of calls were eligible before.

## Approach

`ineq()` hands `cl1()` a constrained L1 problem: minimised rows, equality rows, inequality rows, sign restrictions, and excluded columns. Phase 1B:
1. hooks in immediately before the `cl1()` call;
2. when that problem is **square** on its non-zero columns, solves it with LU;
3. accepts the LU solution only if it passes a set of checks: two-sided conditioning, normwise and componentwise residuals, every inequality row, and every sign restriction;
4. otherwise lets `cl1()` run unchanged.

The audit (Task 0) confirmed the problem structure against PHREEQC v3.9.0.

## What Was Learned

| Task | Finding |
|---|---|
| T1, shadow | The hook changes nothing (all outputs byte-identical). Bound violations are rare. A row-only conditioning measure wrongly rejected 98 % of the calls |
| T1b | A two-sided (row and column) measure fixes that. But with it, LU and `cl1()` steps disagree in many well-conditioned CO₂–basalt calls (median 6 × 10⁻⁷) |
| T1c | The disagreement is not explained by `cl1()` being inexact. It lies in **weakly determined components**: trace species whose coefficients are tiny in every row, so the equations hardly constrain them |
| T2 | Steering accepts 99.9 % of CO₂–basalt calls, with results within tolerance there, but it is 6.6 % slower: two factorizations and two estimates per call |
| T3 | One factorization and one estimate: 8 µs per accepted call. **10.1 % faster** on CO₂–basalt. But **ex3 and ex6 converge to different redox states** (pe shifted by 12–13 units), and ex3 needs 60 % more iterations |

**The key lesson:** on square, feasible problems, `cl1()` is itself a dense elimination of the same order as LU. The speed gain is bounded by about 10 %, not the 1.5 times first estimated. Changing the solver also changes the Newton trajectory in weakly poised redox systems, where PHREEQC's convergence test does not determine pe.

## How to Use the Experimental Mode (opt-in, at your own risk)

The mode needs a build with `-DPHREEQC_USE_LAPACK_SOLVER=ON`. Run with:
```
PHREEQC_LINEAR_SOLVER_INTERCEPT=cl1_problem \
PHREEQC_LINEAR_SOLVER_CONDITION_CHECK=gate PHREEQC_LINEAR_SOLVER_CONDITION_METRIC=row_column PHREEQC_LINEAR_SOLVER_MIN_RCOND=1e-8 \
PHREEQC_LINEAR_SOLVER_STATISTICS=1 phreeqc input.pqi output.out database.dat
```

**Before relying on it for an input class,** run the same input with `PHREEQC_LINEAR_SOLVER=cl1`, then compare:
- `tests/petsc_linear_solver/tools/compare_phreeqcrm_chem_output.py <cl1.out> <steer.out> --relative-tolerance 1e-6 --absolute-tolerance 1e-12`, and `compare_selected_output_to_baseline.py` for `.sel` files. Any difference beyond the last digit of a trace species, and **any pe change**, means: do not use steering for that input;
- the `calls=` values: steering must not increase them noticeably.

## Status of the Code

- **Defaults:** without the environment variables, behaviour is identical to Phase 2. With the build options OFF, results are byte-identical to PHREEQC v3.9.0.
- **Shadow mode** (`PHREEQC_LINEAR_SOLVER_INTERCEPT=cl1_problem_shadow`) remains available as a diagnostic, together with the analysis tools in `tests/petsc_linear_solver/tools/`.
- **PhreeqcRM** keeps the Phase 2 integration (LAPACK backend for eligible aqueous systems, condition gate). Steering is not ported.

## Open Direction

A safe default would need a test that recognises weakly poised redox systems before steering, and sends them to `cl1()`. Even then, the measured headroom is about 10–15 %.
