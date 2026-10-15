# Phase 2 Design: The PETSc Linear Solver in PhreeqcRM, under MPI and OpenMP

| Field | Value |
|---|---|
| Status | **Closed** (P2-D21). Revision 2 (2026-10-11) was updated by `phase2_task0_reviewer_decisions.md` (P2-D1 to P2-D21), which supersedes §4, §5 and §9 where they differ |
| Builds on | Phase 1A, complete (tag `petsc-phase1a-complete`); Phase 1B Task 0, profiling report |
| Benchmark input | Phase 1B Task 1, done: `lapack_direct` is the default backend (decision D19), which resolves §5 T2 and open decision 2 in §11 |
| Verification | Items marked **[VERIFY]** are checked in Phase 2 Task 0 before any code is written, as in Phase 1 |

## 1. What Phase 1 Established

- **Correctness.** The adapter's PETSc solves agree with `cl1()` to round-off: normwise step differences of at most 5.5 × 10⁻¹⁶. Results are identical on the validation examples, and agree to about 10⁻¹¹ on the CO₂–basalt input.
- **Lifecycle and isolation.**
  - The OFF build and the runtime-disabled ON build reproduce the baseline exactly.
  - There are no leaks (PETSc allocation tracking and ASan), apart from the known PETSc-internal block after a singular LU (D16).
  - The class layout does not depend on `USE_PETSC`.
- **Performance facts.**
  - The linear solve is 57.5 % of the CO₂–basalt run.
  - PETSc is eligible for only 0.0017 % of calls today.
  - Per call, PETSc (about 13.5 µs for n ≈ 9) and `cl1()` (about 13.6 µs average) cost about the same. The benchmark will quantify a LAPACK-direct alternative.

## 2. Where Parallelism Actually Is

PHREEQC chemistry is **cell-local**. Each cell's equilibrium problem has 10–40 unknowns and does not depend on any other cell during the chemistry step. PhreeqcRM parallelises *across cells*:

- **OpenMP mode:** one process runs `nthreads` worker `Phreeqc` instances (IPhreeqcPhast workers), each processing a block of cells.
- **MPI mode:** each rank owns one worker and a block of cells. The root process coordinates, and the other ranks wait in a worker loop. **[VERIFY]** the exact mechanism (`MpiWorker`), and whether OpenMP and MPI can be combined in the version used.

**Consequence: Phase 2 does not distribute any matrix.** Every linear system stays a sequential system on `PETSC_COMM_SELF`, solved by the rank or thread that owns the cell. Phase 2 makes the Phase 1 adapter work *correctly and safely inside* PhreeqcRM's existing parallel structure. A distributed solve would only matter for a globally implicit reactive-transport formulation, which is a different project (§10).

## 3. Code Base and Porting

**PhreeqcRM is a separate repository, and it carries its own copy of the PHREEQC sources.** The copy is expected at `src/IPhreeqcPhast/IPhreeqc/phreeqcpp/`, synchronised from phreeqc3 by git subtree **[VERIFY]**. Phase 1 changed four PHREEQC files (`Phreeqc.h`, `model.cpp`, `Phreeqc.cpp`, `class_main.cpp`) and added `src/petsc_linear_solver/`.

**P1. Port as a patch series, not by hand.**
1. Produce `git format-patch` output from `petsc-phase1a-task0..petsc-phase1a-task4`, restricted to source files.
2. Check whether PhreeqcRM's `phreeqcpp` copy of `model.cpp`, `Phreeqc.h` and `Phreeqc.cpp` matches phreeqc3 at commit `5132d269` in the regions we touched. **[VERIFY]**
3. If they match, the patches apply cleanly. If they don't, Phase 2 Task 0 records every difference before porting.

**P2. `class_main.cpp` is not part of PhreeqcRM.** PhreeqcRM is a library, so the `main()` pin and shutdown from Task 4 have no equivalent there. PETSc lifetime must be tied to PhreeqcRM objects instead (§4).

**P3. Build integration.** The `PHREEQC_USE_PETSC` option and the adapter library move into PhreeqcRM's CMake. The plain-versus-keyword `target_link_libraries` question has to be checked again for PhreeqcRM's targets. **[VERIFY]**

## 4. PETSc Initialisation under MPI

The Phase 1 initialisation is correct for a single process. Under MPI it has three defects that must be fixed **before** any MPI run.

**M1. `PetscInitialize` is collective over `PETSC_COMM_WORLD`.** By default that communicator is `MPI_COMM_WORLD`. Our initialisation is lazy: it runs on the first eligible `ineq()` call, at different times on different ranks, and never on ranks that get no eligible cell. **Ranks would deadlock.**

*Decision:* when PHREEQC initialises PETSc itself and MPI is already initialised, set `PETSC_COMM_WORLD = MPI_COMM_SELF` before `PetscInitializeNoArguments()`. Initialisation then becomes rank-local and never synchronises. This is valid because every PETSc object the adapter creates lives on `PETSC_COMM_SELF`. If a host application has already initialised PETSc, we use it unchanged. The adapter performs no collective operations, so the host's `PETSC_COMM_WORLD` is irrelevant to us.

**M2. Finalise before `MPI_Finalize`.** The Phase 1 `atexit` handler may run after the host has called `MPI_Finalize`, and `PetscFinalize` after `MPI_Finalize` is invalid.

*Decision:* add a public function, `shutdown_petsc_if_owned_by_phreeqc()` (it already exists), and call it from PhreeqcRM's destructor and from the MPI worker-exit path **[VERIFY: names]**. Change the `atexit` handler to check `MPI_Finalized()` first and do nothing if MPI is already finalised. Finalising after `MPI_Finalize` is then impossible by construction.

**M3. A matching MPI is required.** The installed `petsc/3.26.0` module is built **MPIUNI**, i.e. serial: `PETSC_HAVE_MPIUNI` is defined, per the Task 0 report, A3. MPIUNI cannot be linked into an MPI-parallel PhreeqcRM, because its MPI stubs conflict with the real library.

*Requirement:* a PETSc build configured with the same MPI implementation and version as PhreeqcRM. Check the other module (`/opt/petsc/3.20.6`) **[VERIFY: is it MPI-enabled, and with which MPI]**. Otherwise build PETSc 3.26 against the cluster's MPI, using `--with-debugging=0`, and without `-march=native` if the binaries may run on other node types. This matters because the AVX-512 build caused the valgrind failure (D14), and would fail the same way on CPUs without AVX-512.

## 5. Threads (OpenMP Mode)

**T1. PETSc is not thread-safe by default.** Logging, memory tracking, the error-handler stack and object registration are global. Two threads in `KSPSolve` at once, even on different objects, are undefined behaviour unless PETSc is configured for thread safety **[VERIFY: configure option and constraints for 3.26]**. Phase 1 deliberately locked only object creation and destruction, not the solve path (Task 3 design).

**T2. The backend policy follows from the benchmark.** The adapter gets a backend switch with the same interface, the same eligibility gate and the same comparison mode:

| Backend | Thread-safe | Where it is used |
|---|---|---|
| `lapack_direct` | Yes. LAPACK `getrf`/`getrs` are reentrant, with BLAS limited to 1 thread. | OpenMP mode, and any serial run where it is faster |
| `petsc` | Only with a thread-safe PETSc build | MPI mode with 1 thread per rank; future large systems |

If the benchmark shows `lapack_direct` is clearly faster at n ≤ 40, it becomes the default backend for chemistry solves in every mode, and PETSc stays available as `-phreeqc_linear_solver_backend petsc`. If it isn't faster, `petsc` remains the default, and OpenMP runs require either a thread-safe PETSc build or 1 thread per process. **This is the one decision the benchmark gates.**

**T3. BLAS threading.** Any BLAS behind LAPACK or PETSc must run with 1 thread (`OPENBLAS_NUM_THREADS=1`, `MKL_NUM_THREADS=1`, `OMP_NUM_THREADS` handled by PhreeqcRM), so that per-cell solves don't oversubscribe the cores.

## 6. Statistics and Diagnostics across Ranks and Threads

- Each worker `Phreeqc` object has its own `PetscSolverIntegrationState`, as now, so counting needs no locks.
- At shutdown, PhreeqcRM's root process gathers per-worker counters and prints one aggregated statistics pair plus the minimum and maximum per worker. In MPI mode this uses PhreeqcRM's existing communication pattern, not PETSc. **[VERIFY]**
- In comparison mode, the log file name gets a `_rank<r>_thread<t>` suffix, so workers never write to the same file.

## 7. Correctness Criteria

Chemistry is cell-local, so results must not depend on how cells are distributed. For each PhreeqcRM test case:

1. **OFF build:** results identical to unmodified PhreeqcRM, for `np` = 1, 2, 4 ranks and for 1, 2, 4 threads.
2. **ON build:** results within the Phase 1 tolerances (`compare_selected_output_to_baseline.py`), for the same rank and thread counts.
3. **Comparison mode:** normwise step differences at round-off level, with no flipped signs, in every worker's log.
4. **Shutdown:** clean in every mode, with PETSc finalised before `MPI_Finalize` on every rank. No deadlock when some ranks have no eligible cells. This is a specific test: a cell distribution where one rank receives only cells with pure phases.
5. **Memory:** PETSc allocation tracking per rank; ASan in OpenMP mode.

## 8. Performance Criteria

Measured, not assumed:
- strong scaling of the PhreeqcRM chemistry step for `np` = 1 to the number of cores on hpc02, OFF against ON, using the CO₂–basalt profiling input in a PhreeqcRM driver;
- the effect of the backend choice (§5) at each rank count.

Phase 2 is not expected to add speed by itself, because cell distribution already exists in PhreeqcRM. Its goal is that the adapter **works and costs nothing** in the parallel setting. Speed comes from the backend (§5) and from wider eligibility (Phase 1B).

## 9. Task Plan

| Task | Content | Gate |
|---|---|---|
| P2-T0 | Read-only verification: PhreeqcRM tree, version and parallel modes; the `phreeqcpp` copy against phreeqc3 `5132d269`; CMake targets and link style; MPI implementation; MPI-enabled PETSc availability (the 3.20.6 module); PhreeqcRM lifecycle entry points (constructor, destructor, MPI worker exit). Baseline runs of PhreeqcRM's own tests at np = 1, 2, 4 and threads = 1, 2, 4. | All [VERIFY] items answered; stop conditions as in Phase 1 |
| P2-T1 | Port the Phase 1 patch series into PhreeqcRM's `phreeqcpp` copy, and the CMake option and adapter into PhreeqcRM's build. Serial (np = 1, 1 thread) validation equal to Phase 1. | Byte-identical OFF results; Phase 1 unit tests pass in the PhreeqcRM build |
| P2-T2 | MPI-safe initialisation: `PETSC_COMM_WORLD = MPI_COMM_SELF` (M1), finalise ordering (M2), shutdown from the PhreeqcRM destructor and worker exit. New unit tests run under `mpiexec -n 2` and `-n 4`, including the "rank with no eligible cell" case. | No deadlock; clean shutdown on every rank |
| P2-T3 | Backend switch (`lapack_direct` / `petsc`, §5) behind the existing interface. Thread-safety tests with OpenMP: concurrent solves on separate workers, stress-tested under ASan. | Depends on the Phase 1B benchmark result |
| P2-T4 | Aggregated statistics (§6) and per-worker comparison logs | Counters sum exactly across workers |
| P2-T5 | Validation matrix (§7) and performance study (§8) | All criteria met; report |

## 10. Out of Scope

- **Distributed matrices** (`MATMPIAIJ`, `PCFIELDSPLIT`) and a globally implicit coupling of transport and chemistry.
- **Making more unknown types eligible.** That is Phase 1B, and it is independent of Phase 2. Both feed into the same eligibility gate.
- **GPU back-ends.**

## 11. Open Decisions for the Reviewer

1. **Which PhreeqcRM version and repository to target,** and whether hpc02 has an MPI-enabled PETSc (P2-T0 will report).
2. **The backend default,** decided by the Phase 1B benchmark (§5, T2).
3. **Whether OpenMP mode must support the PETSc backend at all,** which requires a thread-safe PETSc build, or may always use `lapack_direct`.

## 12. Revision 2: Changes after Phase 2 Task 0

Phase 2 Task 0 verified PhreeqcRM v3.9.0 and the environment (`phase2_task0_verification_report.md`), and the reviewer's resolution is `phase2_task0_reviewer_decisions.md`. In summary:

- **Porting is low-risk.** PhreeqcRM's PHREEQC copy is identical to the Phase 1 base, and the core patch applies cleanly (P2-D1). CMake in PhreeqcRM uses the keyword link form.
- **No MPI-enabled PETSc exists, and with D19 none is needed for chemistry solves.** `lapack_direct` is the default backend and the only one in MPI builds. PETSc is available in non-MPI builds, serialised under OpenMP (P2-D2). The PETSc-under-MPI items of §4 are deferred (P2-D4).
- **Revised task order (P2-D3):**

| Task | Content |
|---|---|
| P2-T1a | In phreeqc3: make the integration layer backend-neutral, with options in environment variables; no behaviour change (P2-D11) |
| P2-T1b | In phreeqc3: add the `lapack_direct` backend, buildable without PETSc; unit tests; Phase 1 validation repeated for both backends |
| P2-T2 | Port the complete patch series to PhreeqcRM, in both the OpenMP and the MPI configuration, with the redefined MPI baseline (P2-D5) |
| P2-T3 | Parallel validation: OpenMP (3 threads in `TestRM`, plus ASan) and MPI (`-n 1, 2, 4`); outputs compared under P2-D6; aggregated statistics across workers |
| P2-T4 | Performance study and report |
