# Phase 2 Task 1b: LAPACK Direct Backend, Buildable without PETSc

## Context

Task 1a made the integration layer backend-neutral (`newton_linear_solver_state.*`, `newton_linear_solver_options.*`). This task adds the `lapack_direct` backend chosen in decisions D19 and P2-D10:

- a solver that calls LAPACK `dgetrf`/`dgetrs` directly on PHREEQC's augmented Newton matrix;
- it uses the same row equilibration and backward-error acceptance as the PETSc adapter;
- it uses no PETSc;
- it is reentrant, with no global state, so that PhreeqcRM can later call it from several threads or MPI ranks.

**No PHREEQC source file changes in this task.** The dispatcher in `src/model.cpp` already calls `solve_newton_step_with_backend()`. Only the adapter directory, the tests, and the adapter block of `CMakeLists.txt` change.

## Rules (apply to every task)

1. Touch only the files listed under Deliverables. If another file seems to need a change, stop and ask.
2. Code is C++11. Functions called from PHREEQC code never throw and never abort.
3. The LAPACK backend files must not include any PETSc header, under any `#ifdef`.
4. With both options OFF, results are byte-identical to the baseline: `cmp` for `.sel` files, `compare_main_output_to_baseline.py` for `.out` files.
5. Never change tolerances, baselines, test inputs, or the helper scripts.
6. Write large files in parts of at most about 150 lines per edit, and keep your reasoning short before each tool call.
7. Use Plan mode first, present the plan, and implement only after the reviewer replies `APPROVED`. Do not run `git commit`.

## Deliverables

1. **New** `src/petsc_linear_solver/lapack_newton_solver.h`, `.cpp`.
2. `src/petsc_linear_solver/newton_linear_solver_state.h`, `.cpp`: additions under `#ifdef USE_LAPACK_NEWTON_SOLVER` only.
3. **New** `tests/petsc_linear_solver/test_lapack_newton_solver.cpp`.
4. `tests/petsc_linear_solver/test_newton_linear_solver_options.cpp`: make the expected default backend depend on the build (below).
5. `CMakeLists.txt`: the adapter block only (below).

## Specification

### `lapack_newton_solver.h`

```cpp
#pragma once
#ifdef USE_LAPACK_NEWTON_SOLVER
#include <vector>
#include "phrqtype.h"
#include "linear_solve_status.h"

struct LapackNewtonSolver {
	bool   use_row_equilibration = true;
	double max_backward_error = 1.0e-10;   /* same acceptance as the PETSc adapter */
	bool   print_statistics = false;
	std::vector<double> scaled_matrix;      /* column-major n*n, kept for the backward error   */
	std::vector<double> factor_matrix;      /* column-major n*n, overwritten by dgetrf         */
	std::vector<double> scaled_rhs;         /* length n                                        */
	std::vector<double> solution;           /* length n; RHS on input to dgetrs, solution after */
	std::vector<int>    pivot_indices;      /* length n                                        */
	double last_backward_error = -1.0;
	unsigned long solve_count = 0, accepted_count = 0, cl1_fallback_count = 0,
	              non_finite_input_count = 0, lapack_error_count = 0;
};

/* Returns nullptr only if allocation fails. */
LapackNewtonSolver *create_lapack_newton_solver(void);

/* If print_statistics: prints to stderr
     phreeqc_lapack_statistics: solves=%lu accepted=%lu cl1_fallbacks=%lu non_finite_inputs=%lu lapack_errors=%lu
   then deletes the solver and sets *solver = nullptr. Safe on nullptr. */
void destroy_lapack_newton_solver(LapackNewtonSolver **solver);

/* PHREEQC layout: row-major, leading dimension count_unknowns + 1, RHS in column count_unknowns.
   On LINEAR_SOLVE_ACCEPTED, output[0..n-1] holds the solution; otherwise output is unchanged. */
LinearSolveStatus solve_newton_step_with_lapack(LapackNewtonSolver *solver,
	const LDBLE *augmented_newton_matrix, int count_unknowns, LDBLE *output);
#endif
```

### `lapack_newton_solver.cpp`: required algorithm

LAPACK prototypes, declared in the `.cpp` file. The trailing `size_t` is the hidden length of the character argument, as passed by gfortran-built LAPACK. On x86-64 it is harmless for C-implemented LAPACK.

```cpp
extern "C" {
void dgetrf_(const int *m, const int *n, double *a, const int *lda, int *ipiv, int *info);
void dgetrs_(const char *trans, const int *n, const int *nrhs, const double *a, const int *lda,
             const int *ipiv, double *b, const int *ldb, int *info, size_t trans_length);
}
```

`solve_newton_step_with_lapack`, with n = `count_unknowns`, `ld = n + 1`, `J(i,j) = M[i*ld + j]` and `r(i) = M[i*ld + n]`:

1. **Count and check size.** Increment `solve_count`, and set `last_backward_error = -1.0`. If `n <= 0`, count a fallback and return `LINEAR_SOLVE_FALL_BACK_TO_CL1`.
2. **Finiteness.** If any `J(i,j)` or `r(i)` is not finite (`std::isfinite`), increment `non_finite_input_count` and return `LINEAR_SOLVE_NON_FINITE_INPUT`.
3. **Buffers.** Resize the buffers to n·n and n (`std::vector::resize`; capacity is reused between calls).
4. **Row equilibration.** For each row i, compute `row_max = max_j |J(i,j)|`. If `row_max == 0`, count a fallback and return `LINEAR_SOLVE_FALL_BACK_TO_CL1`. Otherwise:
   - `scale = use_row_equilibration ? 1.0 / row_max : 1.0`;
   - `scaled_matrix[j*n + i] = scale * J(i,j)` for all j. This is column-major: write `j*n + i`, not `i*n + j`;
   - `scaled_rhs[i] = scale * r(i)`.
5. **Copies.** Copy `factor_matrix = scaled_matrix` and `solution = scaled_rhs`.
6. **Factor.** Call `dgetrf_(&n, &n, factor_matrix.data(), &n, pivot_indices.data(), &info)`. If `info < 0`, increment `lapack_error_count`. If `info != 0`, count a fallback and return `LINEAR_SOLVE_FALL_BACK_TO_CL1`. `info > 0` means an exactly singular matrix.
7. **Solve.** Call `dgetrs_("N", &n, &one, factor_matrix.data(), &n, pivot_indices.data(), solution.data(), &n, &info, 1)`, where `one = 1`. If `info != 0`, increment `lapack_error_count`, count a fallback and return.
8. **Check the solution.** If any `solution[i]` is not finite, count a fallback and return.
9. **Backward error,** on the scaled system, using infinity norms:
   - `residual_norm = max_i |scaled_rhs[i] - Σ_j scaled_matrix[j*n + i] * solution[j]|`;
   - `matrix_norm = max_i Σ_j |scaled_matrix[j*n + i]|`;
   - `solution_norm = max_i |solution[i]|`;
   - `rhs_norm = max_i |scaled_rhs[i]|`;
   - `denominator = matrix_norm * solution_norm + rhs_norm`, **with no constant added**;
   - `backward_error = denominator > 0 ? residual_norm / denominator : (residual_norm == 0 ? 0 : DBL_MAX)`.

   Set `last_backward_error = backward_error`.
10. **Accept or fall back.** If `backward_error <= max_backward_error`, set `output[i] = solution[i]`, increment `accepted_count`, and return `LINEAR_SOLVE_ACCEPTED`. Otherwise count a fallback and return `LINEAR_SOLVE_FALL_BACK_TO_CL1`.

"Count a fallback" means incrementing `cl1_fallback_count`. The solver has no static or global variables.

### Integration: `newton_linear_solver_state.*`, additions only

- **Header:** under `#ifdef USE_LAPACK_NEWTON_SOLVER`, forward-declare `struct LapackNewtonSolver;` and add the member `LapackNewtonSolver *lapack_solver = nullptr;` to `NewtonLinearSolverState`.
- **`create_newton_linear_solver_state`:** if `backend == NEWTON_BACKEND_LAPACK`, call `create_lapack_newton_solver()`. On `nullptr`, set `backend = NEWTON_BACKEND_CL1`. Otherwise set `lapack_solver->print_statistics = options.print_statistics`.
- **`solve_newton_step_with_backend`:** add the case `NEWTON_BACKEND_LAPACK`, which calls `solve_newton_step_with_lapack(state->lapack_solver, …)` and then copies `state->lapack_solver->last_backward_error`.
- **`destroy_newton_linear_solver_state`:** after the PETSc destroy, call `destroy_lapack_newton_solver(&state->lapack_solver)`.

Every addition is guarded by `#ifdef USE_LAPACK_NEWTON_SOLVER` and includes only `lapack_newton_solver.h`.

### `CMakeLists.txt`: restructure the adapter block only

```cmake
option(PHREEQC_USE_PETSC "Build the PETSc Newton linear-solver backend" OFF)
option(PHREEQC_USE_LAPACK_SOLVER "Build the LAPACK direct Newton linear-solver backend" OFF)
if(PHREEQC_USE_PETSC OR PHREEQC_USE_LAPACK_SOLVER)
  add_library(phreeqc_newton_linear_solver STATIC
      src/petsc_linear_solver/newton_linear_solver_options.cpp
      src/petsc_linear_solver/newton_linear_solver_state.cpp)
  # existing properties: POSITION_INDEPENDENT_CODE, cxx_std_11, PUBLIC include directories
  target_compile_definitions(phreeqc_newton_linear_solver PUBLIC USE_NEWTON_LINEAR_SOLVER)
  target_link_libraries(phreeqc phreeqc_newton_linear_solver)        # plain form (decision D2)

  if(PHREEQC_USE_LAPACK_SOLVER)
    find_package(LAPACK REQUIRED)
    target_sources(phreeqc_newton_linear_solver PRIVATE src/petsc_linear_solver/lapack_newton_solver.cpp)
    target_compile_definitions(phreeqc_newton_linear_solver PUBLIC USE_LAPACK_NEWTON_SOLVER)
    target_link_libraries(phreeqc_newton_linear_solver PUBLIC LAPACK::LAPACK)
  endif()

  if(PHREEQC_USE_PETSC)
    # existing: pkg_check_modules(... PETSc), the three PETSc sources added with target_sources,
    # PUBLIC USE_PETSC, PUBLIC PkgConfig::PETSC
  endif()

  if(BUILD_TESTING)
    # test_newton_linear_solver_options: always
    # PETSc tests (the six existing ones) and benchmark_dense_solvers: only if PHREEQC_USE_PETSC
    # test_lapack_newton_solver: only if PHREEQC_USE_LAPACK_SOLVER; also link Threads::Threads
    #   (find_package(Threads REQUIRED))
  endif()
endif()
```

Keep every existing line's effect: same sources, definitions, link flags, the benchmark's OpenBLAS fallback line, and placement after `include(CTest)` (D15). Report which LAPACK library `find_package(LAPACK)` found: the `LAPACK_LIBRARIES` value from `CMakeCache.txt`, or the configure output.

### `test_newton_linear_solver_options.cpp`: build-dependent expectations

The expected default backend and validity become:

| Build | Default backend | `PHREEQC_LINEAR_SOLVER=lapack` | `PHREEQC_LINEAR_SOLVER=petsc` |
|---|---|---|---|
| `USE_LAPACK_NEWTON_SOLVER` defined | LAPACK | valid | valid only if `USE_PETSC` is also defined |
| PETSc only | PETSC | invalid, gives CL1 | valid |

Implement this with `#ifdef`. Every other case stays unchanged.

### `test_lapack_newton_solver.cpp`

Use `CHECK_OR_FAIL`, and the same test data as Tasks 2 and 3: `augmented_newton_matrix_3x3`, the 4 × 4 system, and the generated tridiagonal systems. Before each solve, prefill `output` with 12345.0. Whenever the status is not `LINEAR_SOLVE_ACCEPTED`, `output` must be unchanged.

1. **3 × 3, row equilibration on and off.** Status `LINEAR_SOLVE_ACCEPTED`, every component within 1e-12 of `expected_newton_step`, and `last_backward_error ≤ 1e-14`.
2. **Row 1 scaled by 1e-12.** Same solution within 1e-12.
3. **Zero row** gives `LINEAR_SOLVE_FALL_BACK_TO_CL1`. **A NaN entry** gives `LINEAR_SOLVE_NON_FINITE_INPUT` with `non_finite_input_count == 1`. **Singular [[1,1],[1,1]]** gives `LINEAR_SOLVE_FALL_BACK_TO_CL1`, no crash, and `lapack_error_count == 0`.
4. **Generated systems** for n = 2 to 60: all accepted, with errors within 1e-12. This also exercises buffer growth.
5. **Alternate between the 3 × 3 and 4 × 4 systems** 10 times each: all correct.
6. **Concurrency.** Run 4 `std::thread`s. Each creates its own solver and solves the generated systems for n = 5 to 30 in a loop, 2,000 times. Each thread records whether all solves were accepted and correct. The main thread joins the threads and checks every flag. This shows the solver keeps no shared state.
7. **Statistics.** With `print_statistics = true`, `destroy_lapack_newton_solver` prints exactly one `phreeqc_lapack_statistics:` line. Capture it with the same `dup2` technique as Task 3.

## Build Trees

| Directory | CMake options (all `-DCMAKE_BUILD_TYPE=Release`, built with `-j 8`) |
|---|---|
| `build/petsc_off` | `-DPHREEQC_USE_PETSC=OFF -DPHREEQC_USE_LAPACK_SOLVER=OFF` |
| `build/petsc_on` | `-DPHREEQC_USE_PETSC=ON -DPHREEQC_USE_LAPACK_SOLVER=OFF` |
| `build/lapack_only` | `-DPHREEQC_USE_PETSC=OFF -DPHREEQC_USE_LAPACK_SOLVER=ON` |
| `build/petsc_lapack` | `-DPHREEQC_USE_PETSC=ON -DPHREEQC_USE_LAPACK_SOLVER=ON` |
| `build/lapack_only_asan` | as `lapack_only`, plus `"-DCMAKE_CXX_FLAGS=-fsanitize=address -fno-omit-frame-pointer" "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address"` |

## Validation

Every PHREEQC run uses the baseline README command, changing only the executable path, from `build/<tree>/task1b_<name>`.

1. **OFF build:** ex1 and ex2 identical to the baseline (`cmp` for `.sel`, script for `.out`).
2. **`petsc_on` unchanged:**
   - 7 of 7 tests pass;
   - with `PHREEQC_LINEAR_SOLVER_STATISTICS=1`, the statistics equal Task 1a (ex1 `backend=petsc solves=8 accepted=8`; ex2 `solves=3`, with `3 + 8 + 1045` ineligible).
3. **`lapack_only`, the PETSc-free build:**
   - **No PETSc linked:** `ldd build/lapack_only/phreeqc | grep -i petsc` prints nothing, and `grep -c USE_PETSC build/lapack_only/CMakeCache.txt` is reported.
   - **Tests:** `ctest --test-dir build/lapack_only --output-on-failure` gives 2 of 2 passing (`test_newton_linear_solver_options`, `test_lapack_newton_solver`).
   - **`statistics`:** `PHREEQC_LINEAR_SOLVER_STATISTICS=1`, ex1 and ex2. Expect `backend=lapack`, ex1 `solves=8 accepted=8`, and ex2 `solves=3 accepted=3` with the same ineligibility counts. Run `compare_selected_output_to_baseline.py` on all `.sel` files, which must PASS, and also report `cmp` for each.
   - **`compare`:** `PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1=1`, ex1. `.sel` identical to the baseline. Report the full output of `summarize_petsc_cl1_step_comparison.py`; maximum ≤ 1e-15, no sign inversion.
   - **`cl1`:** `PHREEQC_LINEAR_SOLVER=cl1`, ex1 and ex2, identical to the baseline.
   - **`petsc_requested`:** `PHREEQC_LINEAR_SOLVER=petsc PHREEQC_LINEAR_SOLVER_STATISTICS=1`, ex1. One warning line, `backend=cl1 solves=0`, results identical to the baseline.
4. **`petsc_lapack`, both backends:**
   - 8 of 8 tests pass;
   - the default run (statistics on) gives `backend=lapack` with the same counts;
   - `PHREEQC_LINEAR_SOLVER=petsc` gives `backend=petsc` with the Task 1a counts and a `phreeqc_petsc_statistics:` line.
5. **CO₂–basalt input on `lapack_only`.** Run the profiling input from `build/profiling_inputs/` with the Phase 1B Task 0 command form, from `build/lapack_only/task1b_co2`, with `PHREEQC_LINEAR_SOLVER_STATISTICS=1`.
   - Report the statistics lines.
   - Compare its `.sel` file with `build/petsc_off/profiling/co2_basalt_profiling.sel` using `compare_selected_output_to_baseline.py`, and report the verdict and the 5 largest relative differences.
   - Report the wall time with the `date` pattern, together with the OFF median from the profiling report.
6. **AddressSanitizer:** in `lapack_only_asan`, run `test_lapack_newton_solver`, and ex1 and ex2 with `ASAN_OPTIONS=detect_leaks=1 PHREEQC_LINEAR_SOLVER_STATISTICS=1`. No ASan error. Report any leak report in full.

## Report

Give pass or fail and the outputs for every validation step, the LAPACK library found by CMake, the files created and changed, and any deviation from this specification with the reason for it. Stop after the report.
