# Phase 2 Task 3: Process-Wide Statistics and Per-Object Comparison Logs

## Context

PhreeqcRM creates `nthreads + 2` `Phreeqc` objects per reaction module, and its tests create several modules. Each object owns one linear-solver state, which causes two problems:

1. **Too many statistics lines.** Statistics are printed per object, which gave 43 to 95 pairs of lines per run in Task 2e and had to be summed by hand.
2. **Unusable comparison logs under threads.** In comparison mode every object appends to the same log file, so concurrent OpenMP threads interleave their lines. That is why comparison mode has only been used at 1 MPI rank so far.

This task fixes both **inside the adapter, with no MPI or PETSc dependency**:
- A process-wide accumulator, guarded by a mutex, collects every object's counters when the object is destroyed. When the last live object of the process is destroyed, one **process-summary line** is printed. The next group of objects starts a new "segment".
- Summary lines of different MPI ranks, and of different segments, are summed offline by a reviewer-supplied tool.
- The comparison-log path may contain placeholders for the MPI rank, the process id and the object index, so that every object writes its own file.

The design is in `docs/petsc_linear_solver/phase2_parallel_design.md`, §6 and §9 (P2-T3).

## Rules

1. Modify only:
   - in `~/phreeqc3`: `src/petsc_linear_solver/newton_linear_solver_options.{h,cpp}`, `src/petsc_linear_solver/newton_linear_solver_state.{h,cpp}`, `tests/petsc_linear_solver/test_newton_linear_solver_options.cpp`, the **new** `tests/petsc_linear_solver/test_newton_linear_solver_state.cpp`, and the adapter block of `CMakeLists.txt`;
   - in `~/phreeqcrm`: the adapter files under `src/IPhreeqcPhast/IPhreeqc/phreeqcpp/newton_linear_solver/` (by copying only), and the Newton-solver block of `CMakeLists.txt` (the Threads lines below).

   No PHREEQC source file (`model.cpp`, `Phreeqc.*`, `class_main.cpp`) and nothing in `~/phreeqcrm/Tests/` may change.
2. Never run `git commit`, `checkout`, `apply`, `reset`, `stash`, `clean`, or `worktree`. New run directories only.
3. Never change baselines, inputs, or the helper scripts.
4. Write large edits in parts of at most about 150 lines, and keep your reasoning short before each tool call.
5. Use Plan mode first, present the plan, and implement only after the reviewer replies `APPROVED`. The report is given in your final reply.

## Specification

### Options (`newton_linear_solver_options.{h,cpp}`, no PETSc, no MPI)

- **New field** `bool print_summary;` in `NewtonLinearSolverOptions`.
- **`PHREEQC_LINEAR_SOLVER_STATISTICS`** is matched exactly:

  | Value | `print_statistics` (per object) | `print_summary` |
  |---|---|---|
  | `1` | true | false |
  | `summary` | false | true |
  | `all` | true | true |
  | anything else, unset or empty | false | false |

  The behaviour of `1` is unchanged.
- **Two new functions:**

  ```cpp
  /* Rank label from the first of OMPI_COMM_WORLD_RANK, PMIX_RANK, PMI_RANK that is set and
     non-empty; "0" if none is set. Reads the environment only; never calls MPI. */
  std::string current_process_rank_text(void);

  /* Expands placeholders in a comparison-log path pattern:
       %r -> rank_text      %p -> process_id      %o -> object_index      %% -> %
     Any other character after %, and a trailing %, are copied literally. */
  std::string expand_comparison_log_path(const std::string &pattern, const std::string &rank_text,
                                         unsigned long object_index, long process_id);
  ```

### State (`newton_linear_solver_state.{h,cpp}`)

- **New field** in `NewtonLinearSolverState`: `unsigned long object_index = 0;`.
- **Process-wide accumulator** (file scope in the `.cpp`, all guarded by one `std::mutex`):
  - `live_state_count`;
  - `next_object_index`;
  - `summary_segment`;
  - a totals record with these fields: `objects`, `backend_lapack_objects`, `backend_petsc_objects`, `backend_cl1_objects`, `solves`, `accepted`, `cl1_fallbacks`, `non_finite_inputs`, `ineligible_systems`, `ill_conditioned`, `lapack_errors`, and the 8 ineligibility reasons.
- **`create_newton_linear_solver_state`:**
  1. Under the mutex: `object_index = next_object_index++`, then `++live_state_count`.
  2. If comparison mode is on, open `expand_comparison_log_path(options.comparison_log_path, current_process_rank_text(), object_index, (long)getpid())` instead of the raw path. Use `<unistd.h>` for `getpid`.

  A path without placeholders expands to itself, so phreeqc3's default log name is unchanged.
- **`destroy_newton_linear_solver_state`,** in this order:
  1. The existing per-object printing, unchanged, if `print_statistics`.
  2. The existing comparison-log summary and close.
  3. Under the mutex:
     - add this object's counters to the totals. `ill_conditioned` and `lapack_errors` come from `lapack_solver` if it exists, under `#ifdef USE_LAPACK_NEWTON_SOLVER`. The backend counted is the effective `backend`;
     - `--live_state_count`;
     - if `live_state_count == 0` and this state's `options.print_summary` is set, print exactly one line to stderr, then reset the totals to zero and increment `summary_segment`:
       ```
       phreeqc_linear_solver_process_summary: rank=%s segment=%lu objects=%lu backend_lapack_objects=%lu backend_petsc_objects=%lu backend_cl1_objects=%lu solves=%lu accepted=%lu cl1_fallbacks=%lu non_finite_inputs=%lu ineligible_systems=%lu ill_conditioned=%lu lapack_errors=%lu not_first_attempt=%lu removing_unstable_phases=%lu pitzer_or_sit_model=%lu mass_water_switch=%lu aqueous_only_iteration=%lu numerical_derivatives=%lu empty_system=%lu ineligible_unknown_type=%lu
       ```
  4. Destroy the backends and `delete` the state, as before.

  Per-object counters are written only by the thread that owns the object. Only the totals are shared.

### CMake

- **phreeqc3**, inside the adapter block:
  - `find_package(Threads REQUIRED)` and `target_link_libraries(phreeqc_newton_linear_solver PUBLIC Threads::Threads)`;
  - the new test `test_newton_linear_solver_state`, built only if `PHREEQC_USE_LAPACK_SOLVER` (same pattern as `test_lapack_newton_solver`).
- **PhreeqcRM**, inside the `if(PHREEQCRM_USE_LAPACK_SOLVER)` block only:
  - `find_package(Threads REQUIRED)`;
  - `target_link_libraries(PhreeqcRM PUBLIC Threads::Threads)`;
  - `string(APPEND exportPackageDependencies "find_dependency(Threads)\n")`.

### Tests

**`test_newton_linear_solver_options.cpp`,** new cases:
- `STATISTICS`:
  - `1` gives per-object only;
  - `summary` gives summary only;
  - `all` gives both;
  - `yes` and unset give neither.
- `expand_comparison_log_path("a_%r_%o_%p_%%.log", "3", 7, 42)` returns `"a_3_7_42_%.log"`. `"plain.log"` is unchanged. `"x%"` returns `"x%"`. `"y%q"` returns `"y%q"`.
- `current_process_rank_text()`:
  - with all three variables unset, returns `"0"`;
  - with `PMIX_RANK=4` only, returns `"4"`;
  - with `OMPI_COMM_WORLD_RANK=5` and `PMIX_RANK=4`, returns `"5"`.

  Unset all three at the end.

**`test_newton_linear_solver_state.cpp`,** new, LAPACK builds only. Use `setenv` with `PHREEQC_LINEAR_SOLVER=lapack` and `PHREEQC_LINEAR_SOLVER_STATISTICS=summary`, and capture stderr with the `dup2` technique of Task 3.
1. Create 3 states. Solve the 3 × 3 system of Task 2 once with each, through `solve_newton_step_with_backend`. Destroy all 3. Exactly one summary line, with `segment=0 objects=3 backend_lapack_objects=3 solves=3 accepted=3`.
2. Create 1 state, solve twice, destroy. Exactly one more line, with `segment=1 objects=1 solves=2 accepted=2`.
3. Comparison-log path: with `PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1=1` and `PHREEQC_LINEAR_SOLVER_COMPARE_LOG=<temporary directory>/cmp_%o.log`, create 2 states and destroy them. Two files must exist whose names contain the two object indices, and each must start with the header line. Remove them afterwards.
4. **Threads:** 4 `std::thread`s, each creating its own state, solving the generated systems for n = 5 to 20 one hundred times, and destroying it. Afterwards exactly one summary line (the next segment), with `objects=4` and `solves = accepted = 4 × 100 × 16`.

## Validation

### phreeqc3

1. **Build and test:** `petsc_on` (7 tests), `lapack_only` (3), `petsc_lapack` (9), plus a rebuild of `lapack_only_asan` with `ctest -R '^test_'`. All pass.
2. **Unchanged behaviour:** in `lapack_only`, run ex1 and ex2 with `PHREEQC_LINEAR_SOLVER_STATISTICS=1`. The per-object lines must equal Task 2e, and `.sel` files must be identical to the baseline.
3. **`all` mode:** ex2 with `PHREEQC_LINEAR_SOLVER_STATISTICS=all`. The single summary line must match the sum of the per-object lines. Check with `python3 tests/petsc_linear_solver/tools/sum_linear_solver_statistics.py <stderr>`.
4. **Default comparison-log name:** ex1 with `PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1=1`. `newton_step_comparison.log` must exist under exactly that name.

### PhreeqcRM

5. **Copy, build, test.** Copy the 7 adapter files (`cmp` each). Edit the CMake block. **Reconfigure and rebuild** `build-patched-openmp-lapack`, `build-patched-openmp-lapack-asan` and `build-patched-mpi-lapack`, because the CMake file changed. Use the same `cmake -S … -B … <options>` lines as Task 2b step 4.
6. **Runs,** with the Task 2b method: a copy of the build tree's `Tests/`, a marker before the run, `timeout 600`, `OPENBLAS_NUM_THREADS=1`, and `mpiexec -x NAME=value`.

   | Tree | Case | Environment and command |
   |---|---|---|
   | openmp-lapack | `Tests_t3_summary` | `PHREEQC_LINEAR_SOLVER_STATISTICS=summary ./TestRM` |
   | openmp-lapack | `Tests_t3_compare` | `PHREEQC_LINEAR_SOLVER_STATISTICS=summary PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1=1 PHREEQC_LINEAR_SOLVER_COMPARE_LOG=newton_step_comparison_r%r_o%o.log ./TestRM` |
   | openmp-lapack-asan | `Tests_t3_compare_asan` | as `Tests_t3_compare`, plus `ASAN_OPTIONS=detect_leaks=1` |
   | mpi-lapack | `Tests_t3_summary_np2`, `Tests_t3_summary_np4` | `mpiexec -n <k> -x OPENBLAS_NUM_THREADS=1 -x PHREEQC_LINEAR_SOLVER_STATISTICS=summary ./TestRM` |
   | mpi-lapack | `Tests_t3_compare_np2` | `mpiexec -n 2 -x OPENBLAS_NUM_THREADS=1 -x PHREEQC_LINEAR_SOLVER_STATISTICS=summary -x PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1=1 -x "PHREEQC_LINEAR_SOLVER_COMPARE_LOG=newton_step_comparison_r%r_o%o.log" ./TestRM` |

7. **Checks per case:**
   - Exit status.
   - The full output of `sum_linear_solver_statistics.py` on the case's stderr.
   - **Summary sums against Task 2e:** `Tests_t3_summary` must give `solves=490 accepted=368 cl1_fallbacks=122 ill_conditioned=122`; `_np2` must give `654/460/194/194`; `_np4` must give `1194/856/338/338`. The `np` cases must show summary lines from every rank (0 to k−1).
   - **Compare cases:**
     - the number of `newton_step_comparison_r*_o*.log` files, against the summed `objects`;
     - that every log line has 10 columns: `awk 'NF!=10 && !/^#/' <logs> | wc -l` must be 0;
     - the full output of `summarize_petsc_cl1_step_comparison.py` with all of the case's logs as arguments, where the total record count must equal the summed `solves`;
     - and, because `cl1()` steers, all outputs compared with the baseline using the P2-D16 exact method: `openmp_run1` for the OpenMP cases and `Tests_np2` for `_np2`. All must be identical; `.dmp` files are reported only.
   - **Summary case:** outputs compared with the baseline in numeric mode (relative 10⁻⁶, absolute 10⁻¹²). Only the known exception of P2-D20 (c) may differ; report its pe value.
   - Any ASan report.

## Report (in your final reply)

Report pass or fail and the outputs for every step, the files changed in both repositories, and any deviation with the reason for it. End with a section **"Reviewer decisions required"** that lists every mismatch against the Task 2e sums, any interleaved or malformed log line, any comparison that is not identical in a compare case, and any ASan report. Then stop.
