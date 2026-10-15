# Phase 2 Task 0: Reviewer Decisions

This file resolves the "Reviewer decisions required" section of `phase2_task0_verification_report.md` (PhreeqcRM v3.9.0, commit `90b31ed8`). It was written by the reviewer, not by an agent. Agents treat it as authoritative and must not edit it.

**P2-D1. Porting.** PhreeqcRM's copies of `model.cpp`, `Phreeqc.h`, `Phreeqc.cpp`, `global_structures.h` and `class_main.cpp` are byte-identical to the Phase 1 base `b0b3be76` (B1), and the 159-line core patch applies cleanly (B2). The port is therefore a patch series:
- the core patch, applied with `-p2 --directory=src/IPhreeqcPhast/IPhreeqc/phreeqcpp`;
- the adapter directory, copied as files;
- the CMake integration, rewritten in PhreeqcRM's **keyword** `target_link_libraries` style (B3). This is the opposite of phreeqc3, where the plain form was required.

`class_main.cpp` is not compiled into PhreeqcRM (B7), so the Task 4 `main()` pin does not apply there.

**P2-D2. No MPI-enabled PETSc, so backend policy follows D19.** The only PETSc installed is 3.26.0, built with `--with-mpi=0` (MPIUNI). `/opt/petsc/3.20.6` is empty (A3). MPIUNI headers and libraries cannot be combined with Open MPI 5.0.10 in an MPI build of PhreeqcRM. The policy is therefore:
- **(a)** `lapack_direct` (decision D19) is the default backend, and the only backend in `PHREEQCRM_BUILD_MPI=ON` builds. CMake must not compile the PETSc backend into an MPI build when PETSc reports `PETSC_HAVE_MPIUNI`; it prints a configure message saying so.
- **(b)** In builds without MPI, the PETSc backend may be selected at runtime. Under OpenMP, every PETSc solve is serialised by a process-wide mutex, because PETSc is not thread-safe (design §5 T1). This is correct but slow, and is meant for comparison and debugging only.
- **(c)** Building an MPI-enabled PETSc against Open MPI 5.0.10 is optional future work. It is needed only if PETSc solves are wanted inside MPI runs.

**P2-D3. Order of work.** The `lapack_direct` backend and the backend switch are developed and validated in **phreeqc3 first**, where the Phase 1 validation infrastructure exists (unit tests, the baseline, comparison mode, the CO₂–basalt profiling input). The complete patch series is then ported to PhreeqcRM once. This replaces the order of design §9, where the backend came after the port (P2-T3).

**P2-D4. PETSc lifecycle in PhreeqcRM.** With `lapack_direct` as the default, PETSc is never initialised unless the PETSc backend is selected, and then only lazily, as in Phase 1. Design §4 M1 (`PETSC_COMM_WORLD = MPI_COMM_SELF`) and M2 (no finalisation after `MPI_Finalized`) are still required, but only for an MPI build with an MPI-enabled PETSc. They are deferred together with P2-D2 (c). The `MPI_Finalized` guard in the `atexit` handler is cheap and is added anyway when the PETSc backend is compiled with MPI headers.

**P2-D5. MPI baseline redefined.** In this environment, `MPI_Init` without `mpiexec` hangs (Open MPI 5 singleton start). That makes ctest's direct launches of `TestRMdtor`, `TestBMIdtor` and `TestRM` in the MPI build unusable. In addition, `TestMPI` uses `MPIEXEC_MAX_NUMPROCS=64`, which exceeds the 20 cells of `SimpleAdvect_cpp`. The MPI baseline is therefore:
- configure `build-baseline-mpi` with `-DMPIEXEC_MAX_NUMPROCS=4`;
- run `TestRMdtor` and `TestBMIdtor` with `mpiexec -n 1`;
- run `TestRM` with `mpiexec -n 1`, `-n 2` and `-n 4`, each in its own copy of the test directory, keeping the outputs of each;
- run `ctest -R TestMPI`.

The `--timeout 120` deviation in the Task 0 report is accepted as the record of the old definition.

**P2-D6. Output comparison.** In the PhreeqcRM tests, `.dmp` and `.log.txt` files are byte-identical between runs, and `.chem.txt` and `_utility.txt` files differ only in `End of Run after … Seconds.` lines (C3). Comparisons use `cmp` for `.dmp` and `.log.txt`, and `compare_main_output_to_baseline.py` for `.chem.txt` and `_utility.txt`. The first task that compares PhreeqcRM outputs must confirm that the script reports `IDENTICAL` for two baseline runs, as was done for phreeqc3.

**P2-D7. Worker instances.** Each PhreeqcRM object creates `nthreads + 2` `IPhreeqcPhast` objects (B4), each containing its own `Phreeqc` object, and therefore its own lazily created solver state. Which extra instance is the "utility" and which is for "well pH" (reported as AMBIGUOUS) does not affect the integration. The OpenMP test uses `nthreads = 3`, so it exercises concurrent solves on three workers.

**P2-D8. Expected performance.** With today's eligibility (D8), almost no Newton solves in realistic inputs are eligible: 0.0017 % in the CO₂–basalt profile. Phase 2 therefore delivers parallel correctness, not speed. A speed-up requires Phase 1B, which makes pure-phase and gas unknowns eligible with bound validation. Phase 2 and Phase 1B remain independent, and both feed the same eligibility gate.

**P2-D9. Housekeeping.** `~/phreeqcrm/build-baseline-*` show up as untracked files. The reviewer adds `/build-*/` to `~/phreeqcrm/.git/info/exclude`.

**P2-D10. Parallel backend: LAPACK; PETSc-under-MPI deferred** (the reviewer decided on the user's behalf). Phase 2 uses `lapack_direct` as the backend in OpenMP and MPI builds of PhreeqcRM. No MPI-enabled PETSc is built now. The reasons:
- the systems are cell-local and small, so a distributed or MPI-aware solver has nothing to distribute;
- the benchmark (D19) shows LAPACK is 2.5 to 7 times faster than PETSc at these sizes;
- LAPACK is reentrant, while PETSc is not thread-safe;
- the decision is reversible: building an MPI PETSc later adds a backend without changing the design.

**P2-D11. Backend-neutral integration first.** The Phase 1 integration layer (`petsc_solver_integration.*`) depends on PETSc for its option handling and types, so an MPI build without PETSc could not use it. P2-T1 is therefore split:
- **P2-T1a** makes the integration layer backend-neutral, with no change in behaviour. Integration-level options move from the PETSc options database to environment variables:

  | Old PETSc option | New environment variable |
  |---|---|
  | `-phreeqc_petsc_enable false` | `PHREEQC_LINEAR_SOLVER=cl1` |
  | `-phreeqc_petsc_statistics` | `PHREEQC_LINEAR_SOLVER_STATISTICS=1` |
  | `-phreeqc_petsc_compare_with_cl1` | `PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1=1` |
  | `-phreeqc_petsc_comparison_log <path>` | `PHREEQC_LINEAR_SOLVER_COMPARE_LOG=<path>` |

  PETSc-internal options (`-phreeqc_row_equilibration`, `-phreeqc_max_backward_error`, `-phreeqc_ksp_*`) stay in `PETSC_OPTIONS`.
- **P2-T1b** adds the `lapack_direct` backend and a CMake option to build it without PETSc.

**P2-D12. What is ported to PhreeqcRM first.** The PhreeqcRM port (P2-T2b) carries the backend-neutral integration layer and the **LAPACK backend only**. The PETSc backend in PhreeqcRM needs the serialising mutex of P2-D2 (b), which is not implemented yet. It is deferred to an optional later task. The CMake option is `PHREEQCRM_USE_LAPACK_SOLVER`, default OFF. Its compile definitions (`USE_NEWTON_LINEAR_SOLVER`, `USE_LAPACK_NEWTON_SOLVER`) are PRIVATE to the `PhreeqcRM` target. The class layout does not depend on them (D9 and Task 1a), and no translation unit outside the target includes `Phreeqc.h` (Task 0, B7). `LAPACK::LAPACK` is linked PUBLIC, because `PhreeqcRM` is a static library.

**P2-D13. Baselines come from a separate worktree.** Applying the patch changes PhreeqcRM's sources, and every build tree in `~/phreeqcrm` would then compile the patched code. Baselines are therefore built from a git worktree of the unmodified tag, `~/phreeqcrm_baseline_src` at v3.9.0, which the reviewer creates with `git -C ~/phreeqcrm worktree add --detach ~/phreeqcrm_baseline_src v3.9.0`. P2-T2a records the OpenMP and MPI baselines there, before any PhreeqcRM source is modified.

**P2-D14. Results of Phase 2 Task 1b, accepted.**
- The LAPACK backend builds and runs without PETSc (`ldd` shows no PETSc).
- `find_package(LAPACK)` found OpenBLAS, the same library PETSc uses.
- Statistics, results and comparison mode match the PETSc backend exactly: maximum step difference 5.5 × 10⁻¹⁶.
- The 4-thread test passed, and ASan reported no errors or leaks.
- The single timed CO₂–basalt run (36.4 s against the 33.9 s OFF median) lies within run-to-run variation, because only 25 of 1.43 million calls are eligible (P2-D8). Timing is measured properly in P2-T4.
- Running ctest with `-R` filters is the correct way to select the adapter tests: the phreeqc3 tree also registers its example programs as tests.

**P2-D15. Results of Phase 2 Task 2a.** Every OpenMP and MPI run of unmodified PhreeqcRM v3.9.0 succeeded, `TestMPI` passed at 4 ranks, and no output contains an absolute path. The outputs are **not byte-reproducible**, even between two identical runs. The differing lines are of four kinds, none of which involves a printed chemical quantity:
- **(a) Load-balancing table rows** (`<worker> <first cell> <last cell>`, in `Advect_cpp.chem.txt`). They follow PhreeqcRM's run-time rebalancing, which is based on measured timings.
- **(b) Solution and exchange descriptions** after `Using solution N.` / `Using exchange N.`. They depend on which worker last held the cell.
- **(c) Configuration headers:** `Number of MPI processes:` and `Number of threads:`.
- **(d) `.dmp` files:** `SOLUTION_RAW` blocks are reordered or missing, and in one block `-viscos_0` reads `1` instead of `1.0015681605689`. Dump content depends on worker ownership. This is upstream PhreeqcRM behaviour and is not investigated here.

`.log.txt` and `_utility.txt` files were identical in every comparison, including across rank counts and between MPI and OpenMP. **After discounting (a)–(c), PhreeqcRM's chemistry results do not depend on the number of ranks or threads.**

**P2-D16. Comparison method for PhreeqcRM outputs**, from P2-T2b on:
- `.log.txt` files: `cmp`;
- `_utility.txt` files: `compare_main_output_to_baseline.py`;
- `.chem.txt` files: the new `compare_phreeqcrm_chem_output.py`. It removes run-time lines, configuration headers and lines of exactly three integers, and strips the descriptions after `Using <keyword> N.`. Every other line must match exactly;
- `.dmp` files: reported only, never pass/fail (P2-D15 (d)).

Before its first use, the tool must report `IDENTICAL` for every `.chem.txt` pair of the four Task 2a comparison sets. Results at a given rank count are compared with the baseline at the same rank count (`Tests_np1`, `Tests_np2`, `Tests_np4`), and OpenMP results with `openmp_run1`.

**P2-D17. Results of Phase 2 Task 2b.**
- **(a) Accepted as a port.**
  - The core patch applied cleanly, and the three PHREEQC files are byte-identical to phreeqc3.
  - Exactly 7 adapter files were copied, and the CMake block went in verbatim. `git diff --stat` shows 4 files.
  - With the option OFF, and with `PHREEQC_LINEAR_SOLVER=cl1` under OpenMP and under MPI at 2 ranks, every non-`.dmp` output matches the Task 2a baselines.
  - All runs exited 0, ASan reported nothing, `accepted == solves` everywhere, and `lapack_errors = 0`.
  - Because `PHREEQCRM_USE_LAPACK_SOLVER` defaults to OFF, committing the port changes nothing for default builds.
- **(b) LAPACK results are rank- and thread-invariant.** The 12 differing files have byte-identical diff bodies in all five LAPACK cases (OpenMP, ASan, MPI at 1, 2 and 4 ranks).
- **(c) LAPACK results are not yet accepted as equivalent to `cl1`.** Two kinds of difference appear:
  - *Round-off-scale quantities near zero:* `Total alkalinity` and `Electrical balance`, from 10⁻²⁰ to 10⁻¹³ eq. These are residual-level values, and a different solver legitimately changes them.
  - *A redox difference in one solution* in `Advect_cpp_units_utility.txt`: `pe = -0.602` against `-0.558`, with H(0), H2, O2(g) and CH4 shifted by up to about 0.09 log units. This looks like a poorly poised redox state, where pe is fixed only by trace H2/O2 (about 10⁻¹⁶ mol), so the converged pe is not well determined within PHREEQC's convergence tolerance. **That hypothesis must be verified (P2-T2c) before the LAPACK backend is used for production results.** If it holds, the remedy is the reciprocal-condition check of design §6.6, which routes such ill-conditioned systems to `cl1()`, rather than accepting the difference.
- **(d) The three reported deviations are accepted:** the `timeout` placement in the first `Tests_np1` run, the empty destructor-test stderr files, and zero statistics lines in the `cl1` cases. In those cases the statistics variable was not set, so the specification's expectation of `backend=cl1` lines was wrong.
- **(e) Tool support.** `compare_phreeqcrm_chem_output.py` gains an optional numeric mode (`--relative-tolerance`, `--absolute-tolerance`), applicable to `.chem.txt` and `_utility.txt` files. The exact mode stays the default.

**P2-D18. Results of Phase 2 Task 2c.**
- **(a) The output difference is explained.** The only file outside a relative tolerance of 10⁻⁶ and an absolute tolerance of 10⁻¹² is `Advect_cpp_units_utility.txt`: 9 lines, all in solution 2 of `Tests/units.pqi`. That solution is Na and Cl at 1 mol/L with `pH 7 charge`, and no pe, redox couple, O or C. Its pe is fixed only by trace H(0)/O(0) at about 10⁻¹⁶ mol, so it is not well determined, and both converged answers (−0.602 and −0.558) satisfy PHREEQC's convergence criterion in the same 9 iterations. pH, ionic strength, density, total H and total O are identical. **This is not a defect of the port.**
- **(b) The step-level evidence shows a real conditioning issue.** In comparison mode, 82 of 362 accepted LAPACK steps differ from the `cl1()` step by more than 10⁻⁸, and 63 by more than 10⁻³. In each such step the largest difference is on the hydrogen-balance unknown MH (76) or a mass-balance unknown MB (6). Their backward errors are 10⁻⁴⁰ to 10⁻¹⁷, so LU solves these systems accurately, yet returns steps up to 5 × 10⁴ times larger than `cl1()`'s. The Jacobians are therefore nearly singular in the redox direction, where `cl1()`'s L1 formulation stays bounded. PHREEQC's step limiting absorbed this in these tests, but nothing guarantees that in general. **The LAPACK backend stays off for production results until a conditioning gate exists.**
- **(c) Six records with a relative difference of 3.5 × 10¹⁶ (unknown type MB) are calls in which `cl1()` itself returned `ERROR` (`cl1_return_code = 0`).** `cl1()`'s step is not a meaningful reference there. These records are kept separate in calibration.
- **(d) Comparison mode reproduced the baseline exactly,** including the `.dmp` files at 1 rank. The integration does not disturb the `cl1()` path.
- **(e) Next: the reciprocal-condition check of design §6.6, for the LAPACK backend (P2-T2d).** `dgecon` runs after `dgetrf` at O(n²) cost. There are three modes, `off`, `log` and `gate`, selected with `PHREEQC_LINEAR_SOLVER_CONDITION_CHECK` and `PHREEQC_LINEAR_SOLVER_MIN_RCOND`. The reciprocal condition is logged as a 10th column of the comparison log. The threshold is calibrated from data with `calibrate_condition_threshold.py`, never chosen in advance. Calibration runs in phreeqc3 on examples ex1–ex8, on `units.pqi` copied from PhreeqcRM, and on the CO₂–basalt input. The gate is ported to PhreeqcRM only after the reviewer has chosen the threshold.
- **(f) Tool fixes:** `summarize_petsc_cl1_step_comparison.py` now says "backend" instead of "PETSc", and reads the optional 10th column. Lines of the form `phreeqc_lapack_statistics:` are the backend's own counters, and are counted separately from the integration's statistics lines.

**P2-D19. Results of Phase 2 Task 2d, and the threshold.**
- **(a) Accepted.**
  - The condition check is implemented as specified: the 1-norm comes from the scaled matrix before factorization, and gate mode returns before `dgetrs`.
  - With the check off, results and counts equal Task 1b, with `ill_conditioned=0`.
  - All tests pass in the three trees (7, 2, 8), and ASan is clean.
  - The extra `rcond >= 0` assertion in test 9 is accepted.
- **(b) Calibration.** There were 191 accepted steps from ex1–ex8, `units.pqi` and the CO₂–basalt input. The two groups separate cleanly:
  - every **disagreeing** step (relative difference above 10⁻⁶, `cl1()` OK) has a reciprocal condition of at most 5.6 × 10⁻¹⁵;
  - every **agreeing** step (at most 10⁻¹²) has a reciprocal condition of at least 2.4 × 10⁻⁷.

  The conditioning problem is not specific to PhreeqcRM: the plain phreeqc3 examples ex3 (12 disagreeing steps, all MH) and ex4 (7 disagreeing, relative difference up to 2.3 × 10¹⁰, reciprocal condition down to 6 × 10⁻²⁹) show it as well.
- **(c) Threshold: `PHREEQC_LINEAR_SOLVER_MIN_RCOND = 1e-8`, and gate mode becomes the default for the LAPACK backend.** Falling back to `cl1()` is always the safe direction: a false positive costs only speed, never correctness. The threshold is therefore placed toward the agreeing side of the gap, more than 6 decades above the largest disagreeing value and 1.4 decades below the smallest agreeing one. In the calibration data it gates 12 of 12 disagreeing steps and 0 of 159 agreeing ones. `off` and `log` remain selectable. The PETSc backend ignores the setting.
- **(d) Steps where `cl1()` failed.** ex4 contributes 13 records in which `cl1()` itself returned `ERROR`. At 10⁻⁸, 7 of them are gated, so they behave exactly as the baseline does. The other 6 use the LAPACK step where `cl1()` would have failed. Results of such inputs may therefore differ from the baseline, possibly for the better. They are reported per input in P2-T2e, never hidden.
- **(e) Acceptance criterion for LAPACK results against `cl1()`.** Byte identity is not required, because residual-level quantities near zero legitimately differ. Main and chemistry outputs (`.out`, `.chem.txt`, `_utility.txt`) are compared with `compare_phreeqcrm_chem_output.py` in numeric mode (relative 10⁻⁶, absolute 10⁻¹²), and `.sel` files with `compare_selected_output_to_baseline.py`. Any line outside tolerance is reported for review.
- **(f) Next: P2-T2e.** Make the gate the default in phreeqc3, and validate in steering mode against `cl1()` runs. Then port the changed adapter files into PhreeqcRM, and repeat the Task 2b validation, including a log-mode comparison run that records the reciprocal condition of the PhreeqcRM steps, among them the MH disagreements of Task 2c.

**P2-D20. Results of Phase 2 Task 2e, and Phase 2 status.**
- **(a) phreeqc3, accepted.**
  - The gate is now the default (mode `gate`, threshold 10⁻⁸). Unit tests pass (7, 2, 8).
  - In steering mode, all 10 inputs are within tolerance of their `cl1()` runs (relative 10⁻⁶, absolute 10⁻¹²), and all `.sel` comparisons pass.
  - The gate fired exactly where calibration predicted: 13 times in ex3, 9 times in ex4, and never elsewhere.
- **(b) ex4, accepted.** In 6 calls `cl1()` returned `ERROR`, while the LAPACK step had a reciprocal condition between 1.4 × 10⁻⁶ and 4.0 × 10⁻⁵ and agreed with `cl1()`'s step to about 10⁻¹³. Using LAPACK there is correct, and ex4's outputs are within tolerance.
- **(c) PhreeqcRM, accepted with one documented exception.**
  - All six cases exited 0: OpenMP, OpenMP with ASan, MPI at 1, 2 and 4 ranks, and the log-mode comparison run. ASan is clean.
  - Gate counts are identical across thread and rank layouts at a given test set (122 per full suite at 1 rank and with OpenMP).
  - In comparison mode, all outputs are identical to the baseline.
  - Every output is within tolerance except the redox block of solution 2 in `Advect_cpp_units_utility.txt`: pe −0.601 against −0.602, and H2, O2 and O2(g) shifted by about 0.003 log units. This is the unpoised solution of P2-D18 (a), whose pe is not determined by its composition. Even step differences at round-off level move its converged pe along that undetermined direction. Without the gate the gap was 0.044 pe units; with the gate it is 0.001.
  - **This block is recorded as a known exception.** The global tolerance is not loosened for it. Future comparisons report it separately and flag any change from pe = −0.601 ± 0.005.
- **(d) The threshold stays at 10⁻⁸.** PhreeqcRM's data:
  - disagreeing steps: at most 2.6 × 10⁻¹⁴;
  - agreeing steps: at least 1.9 × 10⁻⁹;
  - all 63 steps with a relative difference above 10⁻³ are below 3.6 × 10⁻¹⁸.

  A threshold of 10⁻¹⁰ would separate the groups perfectly, while 10⁻⁸ also gates 12 of 244 agreeing steps. Those are false positives that fall back to `cl1()`, i.e. to baseline behaviour, so they cost only speed. Across all calibration data (phreeqc3 and PhreeqcRM) the largest disagreeing value is 2.6 × 10⁻¹⁴. 10⁻⁸ keeps 5.6 decades of margin to it, against 3.6 decades for 10⁻¹⁰, and with only about 550 calibration records the larger margin is preferred. The threshold is revisited if a performance study shows the false-positive fallbacks matter.
- **(e) Correction to the Task 2e report.** The PhreeqcRM tree was clean at the start of Task 2e (the agent's own pre-check). The diff against the Task 2b commit consists of the Task 2d condition-check code plus the Task 2e defaults, both made in phreeqc3 and copied in this task. Nothing was left uncommitted before.
- **(f) Phase 2 status.** Done:
  - P2-T1a and P2-T1b: backend-neutral layer and LAPACK backend;
  - P2-T2a to P2-T2e: baselines, port, diagnosis, condition gate, and validation in both repositories;
  - most of the parallel validation planned as P2-T3: OpenMP with 3 threads, ASan, MPI at 1, 2 and 4 ranks, and results invariant to rank and thread counts.

  Remaining: aggregated statistics and per-worker comparison logs (P2-T3), and the performance study (P2-T4). The performance study has little to measure until Phase 1B widens eligibility, because 0.0017 % of calls are eligible in the CO₂–basalt profile.

**P2-D21. Results of Phase 2 Task 3 (accepted) and Task 4; Phase 2 closed.**
- **(a) Task 3.**
  - The PhreeqcRM summary sums equal the Task 2e totals exactly (490/368/122/122, 654/460/194/194 and 1194/856/338/338), with summary lines from every rank.
  - Every per-object comparison log is well formed: 43 logs for 43 objects under OpenMP, 49 for 49 under MPI at 2 ranks, record counts equal to solve counts, and no line without 10 columns.
  - Comparison-mode outputs are identical to the baselines under OpenMP, ASan and MPI, and ASan is clean.
- **(b) Task 4: no measurable overhead.** The workload was 1,000 heterogeneous CO₂–basalt cells (4 equilibrium phases and 3 kinetic minerals each) for 10 steps, with 3 repeats at 1, 4, 16 and 32 threads or ranks.
  - The three MPI configurations (unmodified v3.9.0, patched with the LAPACK gate, patched with `cl1`) differ in median by at most 1.9 % at every count, inside the run-to-run spread.
  - The three patched OpenMP configurations (option OFF, LAPACK gate, `cl1`) differ from one another only inside the spread.
- **(c) The one outlier is attributed to measurement order, not to code.** Unmodified v3.9.0 under OpenMP at 1 thread (median 21.7 s) is about 10 % slower than every other configuration at 1 thread or rank (18.7 to 20.4 s), outside the spread. The same unmodified code under MPI at 1 rank took 19.7 s, and this series was the first one run, so it is attributed to measurement order. At 4, 16 and 32 threads, unmodified OpenMP is not slower than the patched builds. An interleaved re-run (A B A B of `v390_openmp` and `patched_openmp_off` at 1 thread) may confirm this, but it is not required for acceptance.
- **(d) Scaling is a PhreeqcRM property, and the integration does not change it.** MPI reaches 9.5–9.6 times at 32 ranks; OpenMP reaches 7.4–8.9 times at 32 threads. MPI is faster than OpenMP at 16 and 32 for this workload, for both unmodified and patched code, outside the spread.
- **(e) Eligibility in this workload: 0.0098 % (OpenMP, 4 threads) and 0.020 % (MPI, 4 ranks), with no gated steps and no fallbacks.**
  - 99 % of calls are ineligible by unknown type (pure phases).
  - The solve counts differ between OpenMP (99) and MPI (204) because every `IPhreeqcPhast` object runs the input file's initial solutions: OpenMP has 4 workers + 2 = 6 objects, MPI has 4 × (1 + 2) = 12. The 195 extra systems under MPI come from the same source. The cell chemistry itself is identical.
- **(f) Results:** the final concentrations of all seven configurations are identical to unmodified v3.9.0 at 1 thread, at the printed precision (largest difference 0 in all 11 columns).
- **(g) Phase 2 is closed.** The parallel integration is correct, deterministic across thread and rank counts, protected by the condition gate, and free of measurable overhead. Speed now depends entirely on eligibility, which means Phase 1B: pure-phase and gas-phase unknowns with bound validation. The benchmark driver and series scripts (`tests/petsc_linear_solver/phreeqcrm_benchmark/`) are the measurement harness for Phase 1B.
