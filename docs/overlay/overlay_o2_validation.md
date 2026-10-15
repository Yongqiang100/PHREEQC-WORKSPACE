# Overlay Task O2: Full Validation on Fresh Official Clones

## Context

O1 (decision OV-D6) built `~/phreeqc3/overlay/phreeqc_newton_solver/` and smoke-tested it on scratch copies. O2 validates it as a user would meet it: on **fresh clones from the official USGS GitHub repositories**, in every configuration we have results for, including the MPI configuration and ASan, which O1 did not cover. It also proves reversibility with git itself.

**Every comparison is against an existing reference.** The overlay must reproduce:
- the phreeqc3 baselines;
- the PhreeqcRM v3.9.0 baselines (`~/phreeqcrm_phase2_baseline/v390/`, compared by the P2-D16 method);
- the Phase 2 Task 3 and Phase 1B Task 3 results, either from existing run directories or from fresh reference runs with the existing `~/phreeqc3/build/lapack_only/phreeqc`, which contains the same code.

## Rules

1. Write only under `~/overlay_o2/`: clones, builds, runs, scratch copies. Change nothing in `~/phreeqc3` (the overlay folder included; it is copied, never edited), `~/phreeqcrm`, `~/upstream/` or `~/phreeqcrm_baseline_src`. **If a test fails because of an overlay bug, stop and report. Do not fix it in this task.**
2. Never run `git commit`, `checkout` (except `git clone --branch`, or `git -C <clone> checkout <tag>` inside `~/overlay_o2/`), `reset`, `stash` or `clean` outside `~/overlay_o2/`.
3. Run long commands in the background (`nohup … &`) and poll them. Keep your reasoning short. Before each long step, save progress notes to `~/overlay_o2/progress.md`, so that a context compaction cannot lose the state.
4. Use Plan mode first, present the plan, and execute only after the reviewer replies `APPROVED`. The report is given in your final reply.

## Steps

**S0. Official clones (and provenance).**
- **PhreeqcRM:** `git clone --branch v3.9.0 https://github.com/usgs-coupled/phreeqcrm.git ~/overlay_o2/phreeqcrm`.
- **phreeqc3:** run `git ls-remote --tags https://github.com/usgs-coupled/phreeqc3.git | grep -i "3\.9"`. If a v3.9.0 tag exists, clone with `--branch` on it. Otherwise clone the default branch, and report whether commit `b0b3be76` is contained in it (`git -C … branch -a --contains b0b3be76`). If it is, `git -C … checkout b0b3be76` inside the clone. If it is not, report and continue with the PhreeqcRM tree only for the phreeqc3 parts, marking those steps NOT RUN.
- **Hashes:** report `sha256sum` of `Phreeqc.h`, `model.cpp` and `Phreeqc.cpp` in both clones' engine directories, and compare them with `SUPPORTED_UPSTREAM.txt`.
- Copy the overlay folder into each clone: `cp -r ~/phreeqc3/overlay/phreeqc_newton_solver <clone>/`.
- Copy `~/phreeqc3/tests/petsc_linear_solver/inputs/` into the phreeqc3 clone at `tests/petsc_linear_solver/inputs/`. Copy `~/phreeqc3/build/profiling_inputs/` to `~/overlay_o2/profiling_inputs/`, and `~/phreeqc3/build/condition_inputs/units.pqi` to `~/overlay_o2/condition_inputs/`.

**S1. Without the flag.**
- **phreeqc3:** configure and build `<clone>/build/plain` (Release). Run ex1 and ex2 with the README commands from `build/plain/run`. `.sel` files must be identical to `~/phreeqc3/tests/petsc_linear_solver/baseline/` (`cmp`), and `.out` files `IDENTICAL` by `compare_main_output_to_baseline.py`. Then `git -C <clone> status --short` must list only untracked entries (the overlay folder, `tests/`, `build/`).
- **PhreeqcRM:** configure and build `build-plain` (Release, OpenMP). Run `TestRM` in a copy of `build-plain/Tests` with a marker, and compare with `openmp_run1` by the P2-D16 method. All non-`.dmp` files must be identical.

**S2. phreeqc3 with the overlay.**
- **Build:** `<clone>/build/ov` (Release, `-DCMAKE_PROJECT_INCLUDE=<clone>/phreeqc_newton_solver/cmake/attach.cmake`), then `ctest -R newton_solver`: 4 of 4.
- **`git -C <clone> status --short`:** report it. The three engine files must show as modified, with three `.orig` files present.
- **Default mode,** with `PHREEQC_LINEAR_SOLVER_STATISTICS=1`, ex1 and ex2 with the README commands. Make a reference run of each with `~/phreeqc3/build/lapack_only/phreeqc` under `~/overlay_o2/ref/<name>`, at the same depth. The statistics lines must be identical (`diff` of the grep'd lines), and `.sel` files identical to the baseline.
- **Shadow mode, ex4:** `PHREEQC_LINEAR_SOLVER_STATISTICS=1 PHREEQC_LINEAR_SOLVER_INTERCEPT=cl1_problem_shadow PHREEQC_LINEAR_SOLVER_CONDITION_CHECK=gate`, with a reference run as above. The `phreeqc_cl1_problem_shadow:` lines must be identical, and the outputs identical to the reference (`cmp` for `.sel`, the script for `.out`).
- **Steering mode:** the 10 Phase 1B Task 3 inputs (ex1–ex8, `units`, `co2`), with the Task 3 steering environment:
  ```
  PHREEQC_LINEAR_SOLVER_STATISTICS=1 PHREEQC_LINEAR_SOLVER_INTERCEPT=cl1_problem PHREEQC_LINEAR_SOLVER_CONDITION_CHECK=gate PHREEQC_LINEAR_SOLVER_CONDITION_METRIC=row_column PHREEQC_LINEAR_SOLVER_MIN_RCOND=1e-8
  ```
  Use the Task 2e A3 commands, adapted to the clone paths and the copied inputs. Compare each with `~/phreeqc3/build/lapack_only/1bt3_<name>_steer`:
  - every `.sel`: `cmp`;
  - `.out`: `compare_main_output_to_baseline.py`;
  - the `phreeqc_cl1_problem_steer:` line: identical except the `seconds=` field.
- **ASan:** configure `<clone>/build/ov_asan` with the flag plus `"-DCMAKE_CXX_FLAGS=-fsanitize=address -fno-omit-frame-pointer" "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address"`. Run `ctest -R newton_solver`, and the `units` and `ex4` steering runs with `ASAN_OPTIONS=detect_leaks=1`. No ASan report.

**S3. PhreeqcRM with the overlay.**
- **OpenMP** (`build-ov`, Release, with the flag): `ctest -R newton_solver` 4 of 4. Then `TestRM` in a copy of `Tests/`, with `PHREEQC_LINEAR_SOLVER_STATISTICS=summary`:
  - summary sums 490/368/122/122 (`sum_linear_solver_statistics.py`);
  - outputs against `openmp_run1` in numeric mode (relative 10⁻⁶, absolute 10⁻¹²). Only the known exception of P2-D20 (c) may differ; report its pe value.
- **ASan** (`build-ov-asan`): `TestRM` as above, with `ASAN_OPTIONS=detect_leaks=1`. No ASan report.
- **MPI** (`build-ov-mpi`, `-DPHREEQCRM_BUILD_MPI=ON -DMPIEXEC_MAX_NUMPROCS=4`, with the flag): `TestRM` with `mpiexec -n <k> -x OPENBLAS_NUM_THREADS=1 -x PHREEQC_LINEAR_SOLVER_STATISTICS=summary ./TestRM` for k = 2 and 4, each in its own copy of `Tests/`. The sums must be np2 654/460/194/194 and np4 1194/856/338/338, with summary lines from every rank. Compare outputs with `Tests_np2` and `Tests_np4` in numeric mode.
- **Install and driver:** `cmake --install build-ov --prefix ~/overlay_o2/install_rm`. Build `~/phreeqc3/tests/petsc_linear_solver/phreeqcrm_benchmark` against it. Run 1,000 cells, 10 steps, 4 threads, with the Task 4 command and database. Compare `bench_concentrations.tsv` with `compare_selected_output_to_baseline.py` against `~/phreeqc3/build/phreeqcrm_benchmark_runs/lapack_gate_n4_r1/bench_concentrations.tsv`. Must PASS.

**S4. Reversibility with git.**
- Run `cmake -DTREE=<clone> -P <clone>/phreeqc_newton_solver/cmake/detach.cmake` on both clones.
- `git -C <clone> diff --stat` must be **empty** in both, and `git -C <clone> status --short` must list only untracked entries.
- Report both outputs.

## Report (in your final reply)

Give pass or fail and the outputs for every step, the provenance facts from S0, and any deviation with the reason for it. End with **"Reviewer decisions required"**, listing:
- every comparison that is not identical or not within tolerance;
- every NOT RUN step;
- any ASan report;
- any overlay bug found (with no fix attempted).

Then stop.
