# PHREEQC / PhreeqcRM Newton Linear Solver Project: Summary

| Phase | Goal | Result | Decisions |
|---|---|---|---|
| 1A | PETSc in PHREEQC's Newton step (serial, eligible aqueous systems) | Correct; identical results; no leaks. Tiny eligible share | D1–D18 |
| 1B T0, benchmarks | Profile and dense-solver benchmark | Linear solve = 57.5 % of run time; direct LAPACK 2.5–7× faster than PETSc at n ≤ 40 | D19 |
| 2 | PhreeqcRM: backend-neutral layer, LAPACK backend, condition gate, OpenMP/MPI, statistics, performance | Correct, deterministic across threads and ranks, no measurable overhead. Closed | P2-D1–P2-D21 |
| 1B | Solve cl1's own problem by LU with bound checks (minerals, gases, SS) | About 10 % faster on CO₂–basalt, but changes redox results in ex3/ex6. Not adopted; opt-in experimental | B-D1–B-D12 |

## What You Have Now

- **phreeqc3** (branch `petsc-phase1a`, tags `petsc-phase1a-*`, `petsc-phase2-*`, `petsc-phase1b-*`). The integration layer has backends `cl1`, `lapack` and `petsc`, a reciprocal-condition gate, comparison mode, per-object and process statistics, shadow and steering modes, and the analysis tools.
- **PhreeqcRM** (branch `newton-linear-solver`, tags `newton-solver-phase2-*`). The Phase 2 port, with the LAPACK backend, OpenMP and MPI.
- **Measurement harness.** The PhreeqcRM chemistry benchmark driver and series scripts (`tests/petsc_linear_solver/phreeqcrm_benchmark/`), plus the CO₂–basalt profiling input.

## Main Conclusions

1. For PHREEQC's cell-local chemistry, the linear systems are small and dense. **A direct LAPACK call is the right tool; PETSc is not** (benchmark, D19). PETSc would pay off only for a globally implicit transport–chemistry formulation.
2. Most of the linear-solve time is in `cl1()` calls on systems with minerals. On those, `cl1()` is already about as fast as a dense direct solve. **The remaining headroom is about 10 %**, and using it changes the Newton trajectory in weakly poised redox systems.
3. **For speed in CO₂–basalt simulations, parallelism matters far more than the linear solver.** PhreeqcRM's MPI mode gave 9.5× at 32 ranks on hpc02 and scaled better than OpenMP (Phase 2 Task 4).
