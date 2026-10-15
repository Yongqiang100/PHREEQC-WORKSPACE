# Phase 1A Implementation Tasks

This folder holds the six Phase 1A task specifications. Each is self-contained: the agent needs only that specification, the approved `docs/petsc_linear_solver/task0_source_verification_report.md`, the reviewer's `docs/petsc_linear_solver/task0_reviewer_decisions.md`, and the code from earlier tasks. Where a specification and the design document differ, the specification reflects design Revision 4 (see design §12).

The specifications are launched through OpenCode slash commands (`.opencode/commands/`), each bound to an agent whose permissions enforce that task's file scope (`.opencode/agents/`). Repository-wide rules are in `AGENTS.md`.

## How to Run Each Task in OpenCode

1. Start OpenCode from the repository root, in a shell where PETSc is already available (`PETSC_DIR`/`PETSC_ARCH` set, or the PETSc environment module loaded). Commands run by the agent inherit that environment.
2. Start a **new session** for each task (`/new`), so context from earlier tasks doesn't build up.
3. Run the task's command, for example `/petsc-task2-newton-matrix-transfer`. The agent presents a plan and waits.
4. Read the plan. Reply `APPROVED`, or correct it first.
5. File edits ask for approval. Review the first few diffs; once you trust the pattern, you can choose "always" for the rest of the session.
6. When the agent reports, run `/petsc-review-task <number>` for an independent read-only check, then work through the checkpoint list below yourself.
7. If the checkpoint passes, commit it yourself with a tag such as `petsc-phase1a-task2`, so a failed later task can be reverted cheaply. The agents cannot commit.
8. If the agent stops on a stop condition, decide how to proceed before re-running. Do not let it improvise.

Do not start OpenCode with `--auto`. The approval prompts are part of the safety design.

## Sequence and Reviewer Checkpoints

| Task | Prompt | Your checks before approving |
|---|---|---|
| Task 0 | `task0_source_verification_and_baseline.md` | Open every quoted `file:line` for B1, B3, B5, B6 and check that the quotes are real and say what the report claims. In particular, grep for the recorded `<UPSTREAM_NEWTON_MATRIX_NAME>` identifier followed by `[` in `residuals()`/`jacobian_sums()` yourself. Check the stop conditions. Fix any errors directly in `docs/petsc_linear_solver/task0_source_verification_report.md`; later tasks treat that file as authoritative. Check that `tests/petsc_linear_solver/inputs/*.pqi` differ from ex1/ex2 only by the added `SELECTED_OUTPUT` block. |
| Task 1 | `task1_build_integration_and_petsc_initialization.md` | `git diff --stat` shows only the new files and `CMakeLists.txt`. The adapter is linked to `phreeqc` with the **plain** form, `target_link_libraries(phreeqc phreeqc_petsc_linear_solver)` (decision D2), and `USE_PETSC` is **PUBLIC** on `phreeqc_petsc_linear_solver`. OFF build: `.sel` files identical (`cmp`) and `.out` files `IDENTICAL` (comparison script). Three tests pass. Spot-check two PETSc symbols in the report against `/opt/petsc/3.26.0/include`. |
| Task 2 | `task2_newton_matrix_transfer_and_layout_tests.md` | Test 1 shows `jacobian_matrix` entry (0,1) == 2 and entry (1,0) == 0 (no transposition). Test 3 passes with NaN padding. `load_newton_system_into_petsc` uses `MatSetValues` only. Non-finite values are checked before anything is loaded. |
| Task 3 | `task3_newton_solver_cache_and_dense_lu.md` | `KSPSetFromOptions` appears only in slot creation. `petsc_global_state_mutex()` is held in `select_solver_slot_for_size` and in `destroy_petsc_newton_solver`, but not in the solve path. The backward-error denominator has **no** added constant. `slot_creation_count` is 2 in test 6 and 18 in test 7. Read the answer to test 5. |
| Task 4 | `task4_phreeqc_ineq_integration.md` | In `git diff -U0 -- src/model.cpp`, the only changed line in the old body is line 928 (`ineq` → `ineq_cl1_original`). The eligibility function matches decision D8 line by line, including the `in_kode != 1` check. `petsc_solver_state` has the default member initializer `= nullptr` in `src/Phreeqc.h`, and no constructor, `init()`, `InternalCopy()` or `operator=` was edited (D9). The upstream Newton-matrix identifier appears only in the accessor. In `main()`, the scope block closes before `shutdown_petsc_if_owned_by_phreeqc()`. OFF build and runtime-disabled ON build match the baseline (`cmp` for `.sel`, comparison script for `.out`). |
| Task 5 | `task5_phase1a_validation.md` | All three scripts are in `tests/petsc_linear_solver/tools/`. Then read the report: whether ex1 used PETSc at all (`solves > 0`), the ineligibility reasons, comparison verdicts, gate behaviour in ex2, the sign-inversion flag, and iteration counts. |

## Helper Scripts (in `tests/petsc_linear_solver/tools/`, written and tested independently of the coding agent)

- `compare_selected_output_to_baseline.py`: numeric comparison of `SELECTED_OUTPUT` files. Default tolerances are relative 1e-6, absolute 1e-14 for amounts, and absolute 1e-6 for logarithmic columns (pH, pe, `si_*`, `la_*`, `lm_*`, `lg_*`). Text columns must match exactly. Exit code 0 means PASS and 1 means FAIL.
- `compare_main_output_to_baseline.py`: compares main output (`.out`) files line by line, ignoring only the `End of Run after … Seconds.` line and the dashed separators directly around it (decision D11). Exit code 0 means IDENTICAL.
- `summarize_petsc_cl1_step_comparison.py`: summarises the PETSc-versus-cl1 Newton step comparison log. It reports the distribution of the normwise relative difference, flags a suspected sign inversion (PETSc step ≈ −cl1 step), and groups large differences by unknown type.

The scripts are provided so that the code judging pass/fail is not written by the same model whose output is being judged.

## Naming Conventions

New files, types, functions, variables, options, tests, and result directories have descriptive names (for example `load_newton_system_into_petsc`, `SizedSolverSlot`, `-phreeqc_max_backward_error`). Three exceptions are deliberate:

- **PHREEQC's own identifiers** (`ineq`, `delta`, `count_unknowns`, `x`, `LDBLE`, the unknown-type constants, the parameter `in_kode`) are used as they are. Renaming them would modify the upstream code that must stay unchanged.
- **Loop indices `i` and `j`** follow the usual matrix row/column convention in tight loops.
- **PETSc API names** are used as PETSc defines them.

The upstream name of the augmented Newton matrix appears in exactly one line of new code: the accessor `Phreeqc::augmented_newton_matrix()`.

## Typical Failure Points for a Fast, Inexpensive Model

These are spelled out in the prompts, but they are worth checking explicitly in each report:

- inventing PETSc function names (each report must list symbols together with where they appear in the headers);
- "tidying" code inside `ineq_cl1_original`;
- changing a tolerance or baseline after a failure;
- `assert`-based tests that silently pass in release builds;
- restructuring `main()` or the CMake files beyond what was asked.

## Phase 1B, Task 0: Profiling

| Task | Prompt | Your checks before approving |
|---|---|---|
| Profiling | `phase1b_task0_profile_cl1_share.md`, run as `/petsc-profile-cl1-share <input> <database>` with agent `phreeqc-petsc-profiler` | Before running: copy the input and its database into `build/profiling_inputs/`. In the plan: every run uses the same command form from `build/<tree>/profiling`, and timing uses the `date` pattern. After the run: the report contains no input content beyond keyword counts and file sizes; `S`, `amdahl_ceiling`, `eligible_fraction`, and the cost-per-solve numbers are computed from the quoted inputs; `git status` shows only the new report. |

## Phase 1B, Task 1: Dense-Solver Benchmark

| Task | Prompt | Your checks before approving |
|---|---|---|
| Benchmark | `phase1b_task1_dense_solver_benchmark.md`, run as `/petsc-benchmark-dense-solvers` with agent `phreeqc-petsc-benchmarker` | In the plan: only `tests/petsc_linear_solver/benchmark_dense_solvers.cpp`, `CMakeLists.txt` (one `add_executable` and one `target_link_libraries`, no `add_test`), and the report change. The LAPACK calls use `LAPACKgetrf_`/`LAPACKgetrs_` from `petscblaslapack.h`, checked against the headers. All three solvers use the same row equilibration and backward-error acceptance. After the run: both CSV files appear verbatim in the report; OFF build still identical; 6 unit tests pass. |

## Phase 2

Design: `docs/petsc_linear_solver/phase2_parallel_design.md`. Target: PhreeqcRM v3.9.0, cloned to `~/phreeqcrm`.

| Task | Prompt | Your checks before approving |
|---|---|---|
| P2-T0 | `phase2_task0_phreeqcrm_verification.md`, run as `/petsc-phase2-task0-verification` with agent `phreeqc-petsc-phase2-verifier` | Before running: clone PhreeqcRM v3.9.0 to `~/phreeqcrm`. In the plan: no `git apply` without `--check`; builds only in `~/phreeqcrm/build-baseline-openmp` and `build-baseline-mpi`; copies only to `~/phreeqcrm_phase2_baseline/`. After the run: `git -C ~/phreeqcrm status --short` is empty; read "Reviewer decisions required" first. |
| P2-T1a | `phase2_task1a_backend_neutral_integration.md`, run as `/petsc-phase2-task1a-backend-neutral-integration` with agent `phreeqc-petsc-implementer` | In the plan: the two old `petsc_solver_integration.*` files are deleted and replaced; `src/model.cpp` changes only in the include, the eligibility function and the dispatcher; `class_main.cpp` untouched. After the run: `git diff petsc-phase1a-task4 -- src/model.cpp` confined to those regions; 7 unit tests pass; the statistics counts equal Phase 1 Task 4 (ex1 8/8; ex2 3/3, 3 + 8 + 1045 ineligible); `cl1` and `bogus` runs identical to the baseline. |
| P2-T1b | `phase2_task1b_lapack_backend.md`, run as `/petsc-phase2-task1b-lapack-backend` with agent `phreeqc-petsc-implementer` | In the plan: no PHREEQC source file changes; LAPACK files include no PETSc header; `scaled_matrix` written column-major (`j*n + i`); backward-error denominator has nothing added; CMake keeps the plain-form link and the D15 placement. After the run: `ldd build/lapack_only/phreeqc` shows no PETSc; tests pass in all four trees (7, 2, 8 and the ASan run); the thread test passes; ex1/ex2 statistics equal Task 1a with `backend=lapack`; comparison maximum <= 1e-15. |
| P2-T2a | `phase2_task2a_phreeqcrm_baselines.md`, run as `/petsc-phase2-task2a-phreeqcrm-baselines` with agent `phreeqc-petsc-phase2-verifier` | Before running: create the baseline worktree (`git -C ~/phreeqcrm worktree add --detach ~/phreeqcrm_baseline_src v3.9.0`). In the plan: builds only under `~/phreeqcrm_baseline_src/build-*`, copies only to `~/phreeqcrm_phase2_baseline/v390/`, each MPI case in its own copy of `Tests/`, every run under `timeout 600`. After the run: read "Reviewer decisions required" first; the rank-count invariance table decides how P2-T2b compares MPI results. |
| P2-T2b | `phase2_task2b_phreeqcrm_port.md`, run as `/petsc-phase2-task2b-phreeqcrm-port` with agent `phreeqc-petsc-implementer` | Before running: `git -C ~/phreeqcrm switch -c newton-linear-solver`. In the plan: step 0 tool check first; `git apply` only after `--check`; only the 3 PHREEQC files, the new `newton_linear_solver/` directory (7 copied files, no PETSc files), and `CMakeLists.txt` change; CMake in keyword form, definitions PRIVATE. Approve edits only under `~/phreeqcrm/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/` and `~/phreeqcrm/CMakeLists.txt`. After the run: every non-`.dmp` comparison IDENTICAL; `accepted == solves`; `git -C ~/phreeqcrm diff --stat` shows only the 4 files. |
| P2-T2c | `phase2_task2c_lapack_difference_diagnosis.md`, run as `/petsc-phase2-task2c-lapack-difference-diagnosis` with agent `phreeqc-petsc-phase2-verifier` | Before running: commit the Task 2b port in `~/phreeqcrm`. In the plan: no rebuild, no edits outside the report, one new case directory `Tests_np1_compare` only. After the run: comparison-mode outputs identical to `Tests_np1`; the step-difference table by unknown type tests the poorly-poised-redox hypothesis (P2-D17 (c)). |
| P2-T2d | `phase2_task2d_condition_check.md`, run as `/petsc-phase2-task2d-condition-check` with agent `phreeqc-petsc-implementer` | In the plan: no PHREEQC source or CMake change; `dgecon` called after a successful `dgetrf` with the 1-norm of the scaled matrix taken before factorization; gate mode returns before `dgetrs`; default mode `off`. After the run: off-mode results identical to Task 1b; all 10 calibration logs have 10 columns; read the calibration table before choosing a threshold. |
| P2-T2e | `phase2_task2e_gate_default_and_phreeqcrm_validation.md`, run as `/petsc-phase2-task2e-gate-default-and-phreeqcrm-validation` with agent `phreeqc-petsc-implementer` | Before running: commit Task 2d in phreeqc3. In the plan: only the options source and its test change in phreeqc3; PhreeqcRM changes only by copying the 7 adapter files; no reconfigure. After the run: every LAPACK-vs-cl1 comparison within tolerance, or listed; `Advect_cpp_units_utility.txt` in `Tests_gate`; the PhreeqcRM calibration table. |
| P2-T3 | `phase2_task3_statistics_and_comparison_logs.md`, run as `/petsc-phase2-task3-statistics-and-comparison-logs` with agent `phreeqc-petsc-implementer` | In the plan: no PHREEQC source change; the accumulator is mutex-guarded and prints only when the live count reaches 0; no MPI call (rank from environment variables only); PhreeqcRM changes only by copying plus the three Threads lines in its Newton-solver CMake block. After the run: summary sums equal the Task 2e totals; one log file per object with no malformed line; compare cases identical to the baselines. |
| P2-T4 | `phase2_task4_performance_study.md`, run as `/petsc-phase2-task4-performance-study` with agent `phreeqc-petsc-profiler` | Before running: commit Task 3 in both repositories. In the plan: no rebuild of the patched or baseline trees other than the allowed catch-up of `build-patched-openmp-off`; installs only under `~/phreeqcrm_installs/`; the `squeue`/load check before each series. After the run: ratios against `v390_*` at each count compared with the run-to-run spread; eligibility and gated fractions; all result comparisons pass. |

## Phase 1B (design: `docs/petsc_linear_solver/phase1b_design.md`)

| Task | Prompt | Your checks before approving |
|---|---|---|
| 1B-T0 | `phase1b_task0_cl1_problem_audit.md`, run as `/petsc-phase1b-task0-cl1-problem-audit` with agent `phreeqc-petsc-phase2-verifier` | In the plan: every quote taken from `git show petsc-phase1a-task0:src/...` (unmodified v3.9.0), no build, only the report written. After the run: read "Answers to the design's [VERIFY] items" and C9 (squareness) first. |
| 1B-T1 | `phase1b_task1_cl1_problem_shadow_mode.md`, run as `/petsc-phase1b-task1-cl1-problem-shadow-mode` with agent `phreeqc-petsc-implementer` | In the plan: `src/model.cpp` changes only in the dispatcher condition and the two `#ifdef` blocks immediately around the unchanged `cl1(...)` statement; the E-row check uses no tolerance; `prepare` reads everything before `cl1()` runs; a separate LapackNewtonSolver for the shadow. After the run: every shadow run identical to its `cl1` run; zero falsifiers; acceptance rate and ranked rejection reasons per input. |
| 1B-T1b | `phase1b_task1b_conditioning_diagnostics.md`, run as `/petsc-phase1b-task1b-conditioning-diagnostics` with agent `phreeqc-petsc-implementer` | In the plan: no PHREEQC source change; the column scaling affects only the metric, never the solution; the default metric stays `row`. After the run: remaining falsifiers under `--condition-factor 100`; both calibration tables; all shadow runs identical to the Task 1 `cl1` runs. |
| 1B-T1c | `phase1b_task1c_cl1_exactness_diagnostic.md`, run as `/petsc-phase1b-task1c-cl1-exactness-diagnostic` with agent `phreeqc-petsc-implementer` | In the plan: no PHREEQC source change; the copy of q is made in `prepare` (before `cl1()` destroys it); the six columns are computed only for accepted records with cl1 kode 0. After the run: per-group fractions of inexact `cl1()` and of entries <= 1e-15 decide whether B-D9 (d) holds. |
| 1B-T2 | `phase1b_task2_cl1_problem_steering.md`, run as `/petsc-phase1b-task2-cl1-problem-steering` with agent `phreeqc-petsc-implementer` | In the plan: `model.cpp` gains only the steering block plus one `if (!newton_cl1_problem_accepted)` line in front of the unchanged `cl1(...)` statement; nothing is written to `delta1`/`res` unless the outcome is ACCEPT; the componentwise check runs before the E-row check. After the run: the per-input table (acceptance, outputs, `.sel`, call ratio, warnings) and the CO2-basalt timing decide which inputs may steer. |
| 1B-T3 | `phase1b_task3_slim_accepted_path.md`, run as `/petsc-phase1b-task3-slim-accepted-path` with agent `phreeqc-petsc-implementer` | In the plan: no PHREEQC source change; the fast metric scales only the U part of the factors and skips the row-metric `dgecon`; shadow settings unchanged. After the run: identical steering decisions and outputs to Task 2; the co2 ratio against the 10 % stop criterion (B-D11 (g)); the complete difference lists and the ex3 iteration table. |
| 1B-T3b | `phase1b_task3b_test14_gate_coverage.md`, run as `/petsc-phase1b-task3b-test14-gate-coverage` with agent `phreeqc-petsc-implementer` | Test file only, test 14 only. After the run: at least 80 cases gated by both methods, at least 80 accepted by both, identical decisions, all four trees pass. |
