# Task 0: Reviewer Decisions

This file resolves the stop conditions raised in `task0_source_verification_report.md`. It was written by the reviewer, not by an agent. Agents treat it as authoritative and must not edit it. Where it conflicts with an earlier task specification, this file wins.

Report reviewed: `task0_source_verification_report.md`, all items A1–C3 and B9–B15, against commit `5132d269`.

## Stop Conditions

| Condition | Report | Decision |
|---|---|---|
| S1 (PETSc version, scalar type, precision) | NO | Accepted. See D1 for the installation to use. |
| S2 (layout of the augmented Newton matrix) | NO | Accepted. |
| S3 (special treatment of allow-listed types) | YES | **Resolved by D3–D5.** The special cases are either harmless for a square, nonsingular system or are excluded by the eligibility gate. |
| S4 (members written by `ineq()` and read elsewhere) | YES | **Resolved by D6–D7.** The B11 reader audit shows that only two state paths matter, and the gate excludes both. |

Work may proceed to Task 1.

## Decisions

**D1. PETSc installation.** Use the module `petsc/3.26.0` (`/opt/petsc/3.26.0`, `PETSC_ARCH` empty): real double scalars, MPIUNI, 32-bit indices, non-debug build. The environment setting `PETSC_DIR=/home/chen/petsc` points at a directory that does not exist; the reviewer removes it from `~/.bashrc`. The pkg-config module name is `PETSc`.

**D2. Build scope.** This tree contains only the `phreeqc` executable target (`CMakeLists.txt:260`). All core sources are compiled directly into it, and there is no IPhreeqc or PhreeqcRM target. The only existing `target_link_libraries` call on `phreeqc` uses the plain (no-keyword) form (`CMakeLists.txt:300`). The adapter library is therefore linked with the plain form, `target_link_libraries(phreeqc phreeqc_petsc_linear_solver)`, because CMake rejects mixing the two forms on one target. The adapter's PUBLIC usage requirements (compile definition `USE_PETSC`, include directories) still propagate with the plain form. The project requests C++11 (`CMakeLists.txt:263`), so the adapter must also compile as C++11.

**D3. ALK and SOLUTION_PHASE_BOUNDARY are eligible.** `ineq()` places these rows in the optimization block, not the equality block (`src/model.cpp:1207`). Without inequality rows (`m = 0` when there are no pure phases, B5), `cl1()` minimizes the L1 norm of the optimization-row residuals while satisfying the equality rows exactly. If the full square Jacobian is nonsingular, the exact solution of all rows is feasible and has zero objective. Any other feasible point with zero objective would also satisfy all rows, so this point is the unique minimizer, and `cl1()` returns the same δ as an LU solve, up to round-off. Near-singular systems are caught by the backward-error check, and the comparison mode tests this argument directly. `SOLUTION_PHASE_BOUNDARY` is required for Phase 1A testing: ex1's `O(0) 1.0 O2(g) -0.7` creates one, and without it ex1 would never use PETSc.

**D4. Model-level exclusions.** `ineq()` builds a modified system, not the plain Newton system, in four situations. PETSc is not used in any of them:
- `pitzer_model || sit_model`: MH and AH2O rows/columns are skipped or zeroed (`src/model.cpp:1010-1027`, `1284-1285`), and these models use separate drivers (`model_pz`, `model_sit`).
- `mass_water_switch`: the MH row is rewritten and the MH2O row skipped (`src/model.cpp:1287-1288`, `1326-1334`, `1665-1673`).
- `iterations < aqueous_only`: the whole optimization block is skipped (`src/model.cpp:1141`), so ALK and SOLUTION_PHASE_BOUNDARY rows are missing and the system is not square.
- `remove_unstable_phases`: `ineq()` takes the early-return path (`src/model.cpp:967-1005`) and performs no solve. B13 shows this flag can only be TRUE when a PP unknown exists, so the type allow-list already excludes it; the explicit check is kept as a safeguard.

**D5. The MB failed-row adjustment needs no gate.** It acts only on rows that `cl1()` leaves unsatisfied (`src/model.cpp:1879-1881`). An accepted PETSc step satisfies every row to the backward-error tolerance.

**D6. Retry calls are never sent to PETSc.** `ineq(0)` is called only as a retry after `ineq(1)` returned 2 (`src/model.cpp:172-175`, B12). The first call may have column-scaled the augmented Newton matrix in place (`src/model.cpp:1116`, B3), so a PETSc solve of the retry would solve the scaled system and return a δ in scaled variables. PETSc is therefore attempted only when `in_kode == 1`.

**D7. Numerical-derivative mode is excluded.** The audit (B11) found exactly one read of the augmented Newton matrix outside `ineq()` that can see state left behind by `ineq()`: `numerical_jacobian()` reached directly at `src/model.cpp:144`, without a preceding `jacobian_sums()`. On the `cl1()` path that state is column-scaled; on the PETSc path it is not. PETSc is therefore not used when numerical derivatives are active (the condition guarding `src/model.cpp:144`). All other members written by `ineq()` are either read only inside `ineq()`, read only by inverse modelling after it writes its own values, or written before they are read (B11).

**D8. Eligibility rule for Phase 1A.** PETSc is attempted only if all of these hold, checked in this order:
1. `in_kode == 1` (D6);
2. `remove_unstable_phases` is FALSE (D4);
3. neither `pitzer_model` nor `sit_model` is TRUE (D4);
4. `mass_water_switch` is FALSE (D4);
5. `iterations >= aqueous_only` (D4);
6. numerical derivatives are not active (D7);
7. `count_unknowns > 0`, and every unknown has type MB, ALK, CB, SOLUTION_PHASE_BOUNDARY, MU, AH2O, MH, or MH2O (D3).

Each failed condition is counted separately, so the validation can show why a calculation bypassed PETSc.

**D9. Object lifecycle.** `Phreeqc` has a user-defined copy constructor (`src/Phreeqc.cpp:1155`) and copy assignment (`src/Phreeqc.cpp:2020`), and `main_method` copies the object (`src/class_main.cpp:159`, `Phreeqc MyCopy = *this;`). The state pointer is declared with a default member initializer, `PetscSolverIntegrationState *petsc_solver_state = nullptr;`. Every constructor, including the user-defined copy constructor, then starts with `nullptr`, and neither copy path copies the pointer. The copy assignment leaves the target's own state in place. The destructor releases it. No constructor or `init()` needs editing.

**D10. `main()`.** `src/class_main.cpp:31-89` constructs one `Phreeqc` on the stack and returns `phreeqc_instance.main_method(argc, argv)` (`src/class_main.cpp:87-88`). The Task 4 change wraps exactly those two lines.

**D11. Comparing main output files.** Two runs of the same binary differ only in the `End of Run after … Seconds.` line and the dashed separators around it (C2). Main output (`.out`) is compared with `tests/petsc_linear_solver/tools/compare_main_output_to_baseline.py`, which ignores exactly those lines. Selected output (`.sel`) is still compared byte for byte with `cmp`.

**D12. Build directories.** Baseline: `build/baseline` (existing). New builds: `build/petsc_off` (`-DCMAKE_BUILD_TYPE=Release -DPHREEQC_USE_PETSC=OFF`) and `build/petsc_on` (`-DCMAKE_BUILD_TYPE=Release -DPHREEQC_USE_PETSC=ON`), each built with `-j 8`. Runs use the commands in `tests/petsc_linear_solver/baseline/README.md`, with the executable path replaced by the build directory in use.

**D13. ex2 totals.** The literal reading (`-totals H`) is accepted. Calcium and sulfate are covered by the molalities, saturation indices, and equilibrium-phase columns. The instrumented inputs and baseline are frozen as recorded.

**D14. Memory checking without valgrind** (added after Task 1). The installed `libpetsc.so.3.26.0` contains AVX-512 instructions, and Valgrind 3.26.0 stops with SIGILL in `PetscGetHostName` during `PetscInitialize`. Memory checks therefore use PETSc's allocation tracking (`-malloc_debug -malloc_dump`), which reports PETSc objects never destroyed, and, in Task 5, an AddressSanitizer build in `build/petsc_on_asan`. Task 1 is accepted without its valgrind step.

**D15. Task 1 CMake placement** (added after Task 1). The adapter block goes directly after `include(CTest)` (`CMakeLists.txt:312`), because `BUILD_TESTING` is defined there. The Task 1 specification has been corrected to say so.

**D16. PETSc's internal leak after a singular dense LU** (added after Task 3). With PETSc 3.26.0, an exactly singular dense LU raises an error from `KSPSolve` (Task 3, test 5). On that error path PETSc skips its own `PetscFPTrapPop()`, and `-malloc_dump` then reports one 16-byte block, `PetscFPTrapPush() at .../src/sys/error/fp.c:60`. A diagnostic program confirmed that the block appears only when a singular solve occurs and never otherwise. It is PETSc-internal bookkeeping, not a matrix, vector, or solver created by the adapter. The memory-check rule is therefore refined: a `-malloc_dump` line whose location is `PetscFPTrapPush()` in `src/sys/error/fp.c` is reported but is **not** a failure, provided the same run recorded at least one PETSc error (test 5, or `petsc_errors > 0` in the statistics line). Any other line still fails. Each singular solve also discards and rebuilds its solver slot (Task 3 error path). Both costs are small but scale with the number of singular systems; Phase 1B should measure how often singular systems occur in real runs.

**D17. Numerical-derivative condition** (added during Task 4). The call to `numerical_jacobian()` without `jacobian_sums()` is guarded by `if (state >= REACTION && numerical_deriv)` (`src/model.cpp:141`), not by a single member. The eligibility function uses `numerical_deriv` alone, which excludes numerical-derivative mode entirely, overriding the narrower wording of D7 and D8 item 6. Reason: only line 144 was audited, and Phase 1A excludes everything not fully audited. The mode is off by default, so the cost is negligible.

**D18. Task 4 smoke-run accounting** (recorded after Task 4). With `-phreeqc_petsc_statistics`, ex1 made 8 PETSc solves, all accepted. That equals its baseline Newton iteration count, so every iteration of ex1 went through PETSc. ex2 made 3 accepted PETSc solves (the initial solution, 3 iterations) and 1,056 ineligible calls: 1,045 for pure-phase unknowns, 8 while removing unstable phases, and 3 retries with `in_kode != 1`. The 1,053 first-attempt calls plus the 3 PETSc solves equal the baseline total of 1,056 iterations, and the 3 extra calls are the `ineq(0)` retries. Every call to `ineq()` is therefore accounted for.

**D19. Solver backend for chemistry-sized systems** (after Phase 1B Task 1, `phase1b_dense_solver_benchmark_report.md`). In steady state, with matrices of 5 to 40 unknowns, LAPACK `getrf`/`getrs` called directly (`lapack_direct`) is 2.5 to 6.9 times faster than the production PETSc adapter at every size and in both required classes. At n = 9 it takes 0.85 µs against 3.7 µs, and at n = 20 3.1 µs against 9.4 µs. Both use the same row equilibration and the same backward-error acceptance, and both give identical accuracy. Single-threaded BLAS (`OPENBLAS_NUM_THREADS=1`) changes neither solver consistently, so OpenBLAS threading is not engaged at these sizes. From n = 16 upward, the largest PETSc cost is the loader (`MatSetValues` per row), not the factorization. Decisions:
- **(a)** `lapack_direct` becomes the default backend for the Phase 2 backend switch (Phase 2 design §5, T2), in every mode. PETSc stays selectable.
- **(b)** If the PETSc backend is kept for production use, its loader should write the dense array directly (design §4.3, fast path), which removes most of the load phase.
- **(c)** Micro-benchmark caches are warm. Inside PHREEQC, solves are interleaved with species calculations, so absolute per-call costs there are higher; the profile measured about 13.5 µs per PETSc solve at n ≈ 9. The relative ordering is expected to hold, and Phase 2 Task 5 measures it in place.
- **(d)** The near-singular class was accepted by both backends, with relative solution errors of 10⁻⁶ to 10⁻⁵ and backward errors below 10⁻¹⁵. The backward-error test does not detect ill-conditioning by design. The reciprocal-condition check (design §6.6) remains a Phase 1B item.
- **(e)** The benchmark target links BLAS/LAPACK through a hard-coded line taken from `petscvariables` (`-lopenblas` in `/usr/lib/x86_64-linux-gnu`). A production LAPACK backend must find BLAS/LAPACK portably (CMake `find_package(LAPACK)`, or PETSc's own `BLASLAPACK_LIB`), not by a fixed path.
