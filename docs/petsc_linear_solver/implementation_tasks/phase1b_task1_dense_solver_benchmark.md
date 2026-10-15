# Phase 1B Task 1: Dense-Solver Micro-Benchmark

## Context

The profiling report (`docs/petsc_linear_solver/phase1b_profiling_report.md`) found three things:

- The linear-solve step takes 57.5 % of the run time.
- One `cl1()`-path call averages about 13.6 µs.
- One PETSc solve through our adapter costs about 13.5 µs once every logged event is counted (`PCSetUp` + `KSPSolve` + `MatMult` + `VecNorm` + assembly), for systems of only about 9 unknowns.

At these sizes PETSc's per-call machinery, not arithmetic, probably dominates.

This task measures, on reproducible synthetic systems of the sizes PHREEQC produces, how long one Newton linear solve takes with:

- **`lapack_direct`:** LAPACK `getrf`/`getrs` called directly on a plain buffer, with the same row equilibration and backward-error acceptance as the adapter;
- **`petsc_adapter`:** the production adapter call, `solve_newton_system`;
- **`petsc_breakdown`:** the same PETSc operations timed phase by phase, to show where the PETSc time goes.

`cl1()` is not benchmarked here. It needs a full `Phreeqc` object and its inequality-row set-up. The profiling report's average of 13.6 µs per call is the reference.

**This task adds one standalone benchmark program and one CMake target. It changes no PHREEQC source file and no existing adapter source file.**

## Rules (apply to every task)

1. Touch only the files listed under Deliverables. If another file seems to need a change, stop and ask.
2. Before using any PETSc or LAPACK name provided by PETSc, grep `$PETSC_DIR/include` for its declaration and use the signature found there. If it is not declared, stop and report.
3. Code is C++11. Inside functions returning `PetscErrorCode`, use `PetscFunctionBeginUser` / `PetscCall` / `PetscFunctionReturn(PETSC_SUCCESS)`.
4. All new code compiles only with `PHREEQC_USE_PETSC=ON`. With the option OFF, the build is unchanged.
5. Report measured numbers as they come out. Do not tune, filter, or rerun selectively to improve them.
6. Use Plan mode first, present the plan, and implement only after the reviewer replies `APPROVED`.
7. Do not run `git commit`.

## Deliverables

1. `tests/petsc_linear_solver/benchmark_dense_solvers.cpp`
2. `CMakeLists.txt`: inside the existing `if(PHREEQC_USE_PETSC)` block, add:
   ```cmake
   add_executable(benchmark_dense_solvers tests/petsc_linear_solver/benchmark_dense_solvers.cpp)
   target_link_libraries(benchmark_dense_solvers PRIVATE phreeqc_petsc_linear_solver)
   ```
   This is **not** an `add_test`: the benchmark is run by hand.
3. `docs/petsc_linear_solver/phase1b_dense_solver_benchmark_report.md`

**If linking fails** with undefined references to LAPACK symbols (`dgetrf_`, `dgetrs_`, or similar), take the BLAS/LAPACK libraries from the `BLASLAPACK_LIB` line of `$PETSC_DIR/lib/petsc/conf/petscvariables` and add them to `target_link_libraries(benchmark_dense_solvers PRIVATE ...)`. Report the line you used. Make no other change.

## Benchmark Specification

### Test systems

Every system is stored exactly as PHREEQC stores the augmented Newton matrix (row-major, leading dimension n + 1, RHS in column n) and loaded through `phreeqc_newton_matrix_layout()`.

- **Sizes:** n = 5, 9, 12, 16, 20, 25, 30, 40.
- **Random numbers:** use a deterministic linear congruential generator, `state = state * 6364136223846793005 + 1442695040888963407` (64-bit unsigned). A value is `u = (state >> 11) * (1.0 / 9007199254740992.0)`, uniform in [0, 1). Seed it with `12345 + n` for each size.
- **Exact solution:** `x_true[i] = (i % 2 == 0 ? 1.0 : -1.0) * (1.0 + 0.1 * i)`.
- **Matrix classes:**
  1. **`well_conditioned`:** off-diagonal entries `2u - 1`, and diagonal entries `n + 1`.
  2. **`badly_row_scaled`:** `well_conditioned`, with row i multiplied by `10^(-4 * (i % 4))`, i.e. by 1, 1e-4, 1e-8, 1e-12 in turn.
  3. **`near_singular`:** `well_conditioned`, with the last row replaced by row 0 plus `1e-10 * (2u - 1)` on each entry.
- **Right-hand side:** `rhs = J * x_true`, computed in `long double` and then rounded to `double`.

### Solvers

All three apply the same row equilibration (divide each row and its RHS by the row's largest absolute entry) and the same acceptance test: normwise backward error at most 1e-10, with no constant added to the denominator.

1. **`lapack_direct`.** Use buffers allocated once per size (`std::vector`). Each call:
   - (a) copies the row-equilibrated matrix into a column-major buffer, and keeps a second copy for the backward error;
   - (b) copies the RHS;
   - (c) calls `LAPACKgetrf_`;
   - (d) calls `LAPACKgetrs_` with `"N"` and one RHS;
   - (e) computes the backward error from the saved copy with plain loops;
   - (f) writes the solution.

   Use the macros and `PetscBLASInt` from `petscblaslapack.h`. A non-zero `info` from `getrf` counts as not accepted.
2. **`petsc_adapter`.** One `PetscNewtonSolver` per run, created with `create_petsc_newton_solver`. Each call is `solve_newton_system(solver, &layout, n, output)`: exactly the production path, including slot reuse.
3. **`petsc_breakdown`.** Use one `MATSEQDENSE` matrix, its vectors, and one `KSP` per size, configured exactly as `create_solver_slot` does (`KSPPREONLY`, `PCLU`, natural ordering, options prefix `phreeqc_`, `KSPSetFromOptions` once). Time four phases separately on each call:
   - **`load`:** `load_newton_system_into_petsc`;
   - **`factor_solve`:** `KSPSetOperators` + `KSPSolve`;
   - **`backward_error`:** `MatMult`, `VecAYPX`, `MatNorm`, and three `VecNorm` calls;
   - **`copy_out`:** `copy_solution_from_petsc`.

### Timing

- Use `std::chrono::steady_clock`.
- For each (solver, class, n): run 200 warm-up calls, then 5 batches. Each batch runs enough calls to take at least 0.1 s; fix the call count once from the warm-up timing, and use the same count for all 5 batches.
- Report the median and the minimum of the 5 batch averages, in **nanoseconds per call**. For `petsc_breakdown`, report each phase the same way.
- Check correctness on the last call of each batch: report the maximum relative solution error `max|x - x_true| / max|x_true|`, the backward error, and whether the result was accepted.

### Output

The program prints CSV to stdout, with this header:

```
solver,matrix_class,n,phase,ns_per_call_median,ns_per_call_min,calls_per_batch,accepted,max_relative_solution_error,backward_error
```

`phase` is `total` for `lapack_direct` and `petsc_adapter`, and `load`, `factor_solve`, `backward_error`, or `copy_out` for `petsc_breakdown`.

The program calls `acquire_petsc()` at the start and `release_petsc()` then `shutdown_petsc_if_owned_by_phreeqc()` at the end. It destroys all PETSc objects before releasing, and returns 0 only if every `well_conditioned` and `badly_row_scaled` case was accepted with a relative solution error at most 1e-10.

## Validation and Runs

1. **OFF build unchanged.** Build `build/petsc_off` and run ex1 and ex2 as in Task 1 step 1. Compare with `cmp` for `.sel` files and the comparison script for `.out` files: all identical.
2. **ON build.** Build `build/petsc_on` (all targets). `ctest --test-dir build/petsc_on -R "test_initialization|test_newton_matrix_transfer|test_newton_solver"` must pass all 6 tests.
3. **Benchmark run.** Record `uptime` and `lscpu | grep "Model name"`. Then run, from the repository root:
   ```
   PETSC_OPTIONS="" build/petsc_on/benchmark_dense_solvers > build/petsc_on/benchmark_dense_solvers.csv
   ```
   Report the exit status. Run it a second time to `benchmark_dense_solvers_run2.csv` and report both.

## Report

Write `docs/petsc_linear_solver/phase1b_dense_solver_benchmark_report.md` containing:

- **Environment:** CPU model, `uptime` before each run, compiler, PETSc version, and the LAPACK link line if the fallback was used.
- **The full CSV of both runs**, verbatim.
- **Summary table**, from run 1: one row per n, `well_conditioned` class, with columns:
  - `lapack_direct` median ns;
  - `petsc_adapter` median ns;
  - the ratio `petsc_adapter / lapack_direct`;
  - the four `petsc_breakdown` phase medians.
- **The same table** for `badly_row_scaled`.
- **For `near_singular`:** acceptance, solution error, and backward error per solver and n.
- **Run-to-run variation:** the largest relative difference between run 1 and run 2 medians.
- **Observations:** statements of fact only, including how the numbers compare with the profiling report's 13.6 µs per `cl1()` call. Do not make recommendations.

Stop after the report.
