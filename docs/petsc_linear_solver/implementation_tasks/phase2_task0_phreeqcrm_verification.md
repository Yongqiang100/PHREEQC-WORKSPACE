# Phase 2 Task 0: PhreeqcRM and Parallel-Environment Verification (read-only)

## Context

Phase 2 (design: `docs/petsc_linear_solver/phase2_parallel_design.md`) carries the Phase 1 linear-solver integration into PhreeqcRM, which runs PHREEQC across cells with OpenMP threads or MPI ranks. This task verifies the facts the design depends on and records baseline test results. **It writes no source code and modifies neither repository.**

- **Target:** PhreeqcRM **v3.9.0** (`usgs-coupled/phreeqcrm`), cloned by the reviewer to `~/phreeqcrm`. This matches the PHREEQC base of the Phase 1 work, phreeqc3 v3.9.0, commit `b0b3be76` in `~/phreeqc3`.
- **Phase 1 source changes:** the difference between the tags `petsc-phase1a-task0` and `petsc-phase1a-task4` in `~/phreeqc3`.

Facts the reviewer has already seen in PhreeqcRM's current sources, to be **confirmed or refuted** against v3.9.0 with `file:line` quotes:

- the PHREEQC copy lives in `src/IPhreeqcPhast/IPhreeqc/phreeqcpp/`;
- `CMakeLists.txt` has options `PHREEQCRM_BUILD_MPI` (default OFF) and `PHREEQCRM_DISABLE_OPENMP` (default OFF);
- MPI and OpenMP are mutually exclusive (an `if(PHREEQCRM_BUILD_MPI) … else() … OpenMP …` block);
- `target_link_libraries(PhreeqcRM …)` calls use the **keyword** form;
- the `PhreeqcRM` target compiles the `phreeqcpp` sources directly;
- the `Tests/` directory registers `TestRMdtor`, `TestBMIdtor`, `TestRM`, and, with MPI, `TestMPI` launched through `${MPIEXEC}`.

## Rules

1. Edit only `docs/petsc_linear_solver/phase2_task0_verification_report.md` in `~/phreeqc3`. Build trees go under `~/phreeqcrm/build-baseline-openmp` and `~/phreeqcrm/build-baseline-mpi`, and baseline copies under `~/phreeqcrm_phase2_baseline/`. Nothing else may be created or changed.
2. Every claim about source code needs a file path, line numbers, and the quoted lines. If something is not found, write NOT FOUND and say where you looked. If something is unclear, write AMBIGUOUS and explain why. Never guess.
3. Never run `git commit`, `git checkout`, `git apply` without `--check`, `git reset`, `git stash`, or `git clean` in either repository.
4. Use Plan mode first. Present the plan, then execute only after the reviewer replies `APPROVED`.

## Part A: Environment

- **A1. PhreeqcRM tree.** Run `git -C ~/phreeqcrm describe --tags`, `git -C ~/phreeqcrm rev-parse HEAD`, and `git -C ~/phreeqcrm status --short`; the last must be empty.
- **A2. MPI.**
  - Run `module avail 2>&1 | grep -i -E "mpi|openmpi|mpich|intel"`.
  - Find the tools with `command -v mpicxx mpicc mpiexec mpirun`.
  - Report the first line of `mpicxx --version` and of `mpiexec --version`.
  - Report the underlying compiler and flags: run `mpicxx -show`, and if that fails, `mpicxx --showme`.
  - If nothing is found, write "no MPI found" and continue.
- **A3. PETSc installations.** For each directory under `/opt/petsc/`:
  - the version from `include/petscversion.h`;
  - from `include/petscconf.h`, whether `PETSC_HAVE_MPIUNI` and `PETSC_USE_64BIT_INDICES` are defined;
  - from `lib/petsc/conf/petscvariables`, the lines `CONFIGURE_OPTIONS`, `MPICXX_SHOW` or `CXX`, `MPI_INCLUDE`, `MPI_LIB`, and `BLASLAPACK_LIB`.

  Also run `module show` for each matching `petsc/*` module.
- **A4. CPU.** Report `lscpu | grep "Model name"`, and whether `/proc/cpuinfo` lists `avx512f` (`grep -m1 -o avx512f /proc/cpuinfo`).

## Part B: PhreeqcRM Source Facts (quote `file:line`)

- **B1. PHREEQC copy against the Phase 1 base.** For each of `model.cpp`, `Phreeqc.h`, `Phreeqc.cpp`, `global_structures.h`, `class_main.cpp`, run:
  ```
  git -C ~/phreeqc3 show b0b3be76:src/<file> | diff - ~/phreeqcrm/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/<file> > /dev/null && echo identical || echo DIFFERENT
  ```
  For each file that differs, report `diff --stat`-style counts and the differing hunks that fall within these ranges of the base file: `model.cpp` lines 130-180 and 927-1922; `Phreeqc.h` lines 70-80 and 440-460; `Phreeqc.cpp` lines 55-80.
- **B2. Patch applicability (dry run only).**
  ```
  mkdir -p ~/phreeqc3/build/phase2
  git -C ~/phreeqc3 diff petsc-phase1a-task0 petsc-phase1a-task4 -- src/model.cpp src/Phreeqc.h src/Phreeqc.cpp > ~/phreeqc3/build/phase2/phase1_phreeqc_core.patch
  git -C ~/phreeqcrm apply --check -p2 --directory=src/IPhreeqcPhast/IPhreeqc/phreeqcpp ~/phreeqc3/build/phase2/phase1_phreeqc_core.patch && echo APPLIES_CLEANLY
  ```
  Report the patch size in lines and the full output of `git apply --check`. **Do not apply the patch.**
- **B3. Build structure.** Quote:
  - where the `phreeqcpp` sources are listed and compiled into `PhreeqcRM`;
  - `add_library(PhreeqcRM …)`;
  - every `target_link_libraries(PhreeqcRM …)` call, and whether each uses the keyword form;
  - the C++ standard requested;
  - the MPI/OpenMP option blocks;
  - which compile definitions are PUBLIC and which PRIVATE (`USE_MPI`, `USE_OPENMP`).
- **B4. Lifecycle entry points.** Quote the definitions of:
  - every `PhreeqcRM` constructor and the destructor;
  - `MpiWorker` and `MpiWorkerBreak`;
  - the code that creates and deletes the worker objects (`IPhreeqcPhast`), and how many are created, counting any utility instance;
  - `RM_Create` and `RM_Destroy` (C interface);
  - the BMI class constructor and destructor.
- **B5. Threads.** Quote every `#pragma omp` in `src/PhreeqcRM.cpp`, and state which function runs the chemistry of a block of cells on each thread.
- **B6. MPI ownership.** Run `grep -rn "MPI_Init\|MPI_Finalize\|MPI_Initialized\|MPI_Finalized" src Tests`, then quote and state who initialises and finalises MPI: the application or PhreeqcRM.
- **B7. Which translation units include `Phreeqc.h`.** Run `grep -rln '#include "Phreeqc.h"' src`, and for each file, state whether it is compiled into `PhreeqcRM` and under which compile definitions. This checks that the Phase 1 rule "class layout does not depend on `USE_PETSC`" holds in PhreeqcRM too.

## Part C: Baseline Builds and Tests

- **C1. OpenMP configuration.**
  ```
  cmake -S ~/phreeqcrm -B ~/phreeqcrm/build-baseline-openmp -DCMAKE_BUILD_TYPE=Release
  cmake --build ~/phreeqcrm/build-baseline-openmp -j 8
  ctest --test-dir ~/phreeqcrm/build-baseline-openmp --output-on-failure
  ```
  Report:
  - whether CMake found OpenMP (quote the configure line);
  - the `ctest` summary;
  - the time of each test;
  - the list of files the tests wrote: `find ~/phreeqcrm/build-baseline-openmp/Tests -newer ~/phreeqcrm/build-baseline-openmp/CMakeCache.txt -type f -not -name "*.o" -not -name "*.cmake" | head -50`.

  Copy those output files to `~/phreeqcrm_phase2_baseline/openmp/`, keeping their relative paths.
- **C2. MPI configuration.** Run this only if A2 found MPI:
  ```
  cmake -S ~/phreeqcrm -B ~/phreeqcrm/build-baseline-mpi -DCMAKE_BUILD_TYPE=Release -DPHREEQCRM_BUILD_MPI=ON
  cmake --build ~/phreeqcrm/build-baseline-mpi -j 8
  ctest --test-dir ~/phreeqcrm/build-baseline-mpi --output-on-failure
  ```
  Report the same items as C1, plus `MPIEXEC_EXECUTABLE` and `MPIEXEC_MAX_NUMPROCS` from `build-baseline-mpi/CMakeCache.txt`. Copy the output files to `~/phreeqcrm_phase2_baseline/mpi/`.
- **C3. Determinism.** Re-run `ctest` once more in each configuration that built. For every output file, compare the two runs with `cmp`, and list every file that differs. Do not judge the differences.

## Output

Write `docs/petsc_linear_solver/phase2_task0_verification_report.md` in `~/phreeqc3` with:

- sections A, B, and C, numbered as above;
- a final section, **"Reviewer decisions required"**, with one line each for:
  - whether an MPI-enabled PETSc exists (from A3);
  - whether the Phase 1 patch applies cleanly (B2);
  - whether the baseline tests pass in each configuration (C1, C2);
  - any output file that is not reproducible (C3);
  - every NOT FOUND or AMBIGUOUS item.

Stop after the report.
