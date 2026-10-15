# Phase 1B Task 3b: Make Test 14 Exercise the Gate (test-only fix)

## Context

Task 3 added test 14 to `tests/petsc_linear_solver/test_lapack_newton_solver.cpp`. It compares the fast two-sided condition estimate (method B, from the existing factors) with the full method (method A, a second factorization) on 200 generated matrices. The agent reported, and decision B-D12 (a) recorded, that **no generated matrix falls below the 10⁻⁸ gate** (smallest value 1.4 × 10⁻⁷). The reason is that the near-singular last row is built *after* the column scaling, so the 1e-10 perturbation is large relative to columns scaled by up to 1e-12. The test therefore never checks that both methods *reject* the same matrices.

This task fixes the generator so that the near-singular cases really are near-singular. **Only the test file changes.**

## Rules

1. Touch only `tests/petsc_linear_solver/test_lapack_newton_solver.cpp`, test 14 only. No source, CMake, tool, input or baseline change.
2. Keep the existing generator recipe (DeterministicLcg, seed 777) and the existing draw order for the entries, diagonal and column scales.
3. Use Plan mode first, present the plan, and implement only after the reviewer replies `APPROVED`. Do not run `git commit`. The report is given in your final reply.

## Change

For the odd-indexed matrices (half of the 200), build the near-singular last row **before** applying the column scaling:
1. fill the entries with `2u − 1`, then set the diagonal to n + 1, as now;
2. for odd matrix indices, replace the last row with row 0 plus `1e-10·(2u − 1)` per entry, **now at this point**;
3. then scale column j by `10^(−12·u_j)`, as now.

Each draw stays in the LCG stream, in the order of these steps. State the final draw order in the report.

## Required Assertions in Test 14

Keep:
- relative agreement ≤ 1e-6 between methods A and B whenever both values are > 1e-14;
- equal gate decisions at a minimum of 1e-8 in every case.

Add:
- at least **80** of the 200 cases must have **both** values < 1e-8, i.e. both methods gate them;
- at least **80** cases must have both values ≥ 1e-8.

Print:
- the number of cases gated by each method;
- the smallest and largest value seen;
- the largest relative difference.

## Validation

- `ctest -R '^test_'` in `lapack_only`, `petsc_lapack`, `petsc_on` and `lapack_only_asan`: all pass.
- Report the test-14 output line.

## Report (in your final reply)

Give pass or fail and the outputs for every step, the final draw order, and any deviation with the reason for it. Then stop.
