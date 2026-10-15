# Phase 2 Task 2b: Port the LAPACK Newton Solver into PhreeqcRM v3.9.0

## Context

Phase 2 Tasks 1a and 1b produced, in phreeqc3:
- a backend-neutral integration layer;
- a LAPACK backend that needs no PETSc;
- a dispatcher in `Phreeqc::ineq()` that leaves the original `cl1()` path untouched.

This task carries exactly that into PhreeqcRM v3.9.0 (decisions P2-D1, P2-D12), so that PhreeqcRM can use the LAPACK solver in its OpenMP and MPI configurations. Results are compared against the Task 2a baselines with the method of P2-D16.

- **Repository to modify:** `~/phreeqcrm`, on the branch `newton-linear-solver`, which the reviewer creates from tag v3.9.0.
- **Source of the changes:** `~/phreeqc3`, tag `petsc-phase2-task1b`.
- **Baselines:** `~/phreeqcrm_phase2_baseline/v390/` (Task 2a).

## Rules

1. Modify only:
   - the three PHREEQC files listed in step 1, under `~/phreeqcrm/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/`;
   - the new directory `~/phreeqcrm/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/newton_linear_solver/`;
   - `~/phreeqcrm/CMakeLists.txt`.

   Build trees go under `~/phreeqcrm/build-*`. Nothing in `~/phreeqc3` changes except `~/phreeqc3/build/phase2/`.
2. `git -C ~/phreeqcrm apply` is allowed **only** for the step 1 patch, and only after `--check` succeeds. Never run `git commit`, `checkout`, `reset`, `stash`, `clean`, or `worktree`.
3. Never change baselines, test inputs, the helper scripts, or anything in `~/phreeqcrm/Tests/`.
4. Write large edits in parts of at most about 150 lines, and keep your reasoning short before each tool call.
5. Use Plan mode first, present the plan, and implement only after the reviewer replies `APPROVED`. The report is given in your final reply, not in a file.

## Steps

0. **Pre-checks.**
   - `git -C ~/phreeqcrm status --short` must be empty, and `git -C ~/phreeqcrm branch --show-current` must print `newton-linear-solver`.
   - Then confirm the comparison tool (P2-D16): for every `.chem.txt` file in the four Task 2a pairs (`Tests_np2` vs `Tests_np2_repeat`, `Tests_np1` vs `Tests_np2`, `Tests_np1` vs `Tests_np4`, `Tests_np1` vs `openmp_run1`, all under `~/phreeqcrm_phase2_baseline/v390/`), run `python3 ~/phreeqc3/tests/petsc_linear_solver/tools/compare_phreeqcrm_chem_output.py`. Every result must be `IDENTICAL`. If any is not, stop and report.

1. **Core patch** (`model.cpp`, `Phreeqc.h`, `Phreeqc.cpp`):
   ```
   git -C ~/phreeqc3 diff petsc-phase1a-task0 petsc-phase2-task1b -- src/model.cpp src/Phreeqc.h src/Phreeqc.cpp > ~/phreeqc3/build/phase2/core_through_task1b.patch
   git -C ~/phreeqcrm apply --check -p2 --directory=src/IPhreeqcPhast/IPhreeqc/phreeqcpp ~/phreeqc3/build/phase2/core_through_task1b.patch
   git -C ~/phreeqcrm apply -p2 --directory=src/IPhreeqcPhast/IPhreeqc/phreeqcpp ~/phreeqc3/build/phase2/core_through_task1b.patch
   git -C ~/phreeqcrm diff --stat
   ```
   The last command must list exactly those three files. Then confirm the result: for each of the three files, `diff ~/phreeqc3/src/<file> ~/phreeqcrm/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/<file>` must print nothing.

2. **Adapter files.** Copy these files, unchanged, from `~/phreeqc3/src/petsc_linear_solver/` to `~/phreeqcrm/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/newton_linear_solver/`:
   - `linear_solve_status.h`;
   - `newton_linear_solver_options.h` and `.cpp`;
   - `newton_linear_solver_state.h` and `.cpp`;
   - `lapack_newton_solver.h` and `.cpp`.

   **Do not copy** any PETSc file (P2-D12). Confirm each copy with `cmp`.

3. **CMake.**
   - Find where PhreeqcRM adds include directories for `phreeqcpp` and `phreeqcpp/common`, where `phrqtype.h` lives, and quote those lines.
   - Directly after the MPI/OpenMP block (the `if(PHREEQCRM_BUILD_MPI) … endif()` around `CMakeLists.txt:471-489`; quote the actual lines), add, in PhreeqcRM's **keyword** style:
     ```cmake
     # Newton linear solver for PHREEQC (Phase 2; decisions P2-D10, P2-D12)
     option(PHREEQCRM_USE_LAPACK_SOLVER "Use the LAPACK direct Newton linear solver in PHREEQC" OFF)
     if(PHREEQCRM_USE_LAPACK_SOLVER)
       find_package(LAPACK REQUIRED)
       set(newton_solver_dir ${CMAKE_CURRENT_SOURCE_DIR}/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/newton_linear_solver)
       target_sources(PhreeqcRM PRIVATE
         ${newton_solver_dir}/newton_linear_solver_options.cpp
         ${newton_solver_dir}/newton_linear_solver_state.cpp
         ${newton_solver_dir}/lapack_newton_solver.cpp)
       target_include_directories(PhreeqcRM PRIVATE ${newton_solver_dir})
       target_compile_definitions(PhreeqcRM PRIVATE USE_NEWTON_LINEAR_SOLVER USE_LAPACK_NEWTON_SOLVER)
       target_link_libraries(PhreeqcRM PUBLIC LAPACK::LAPACK)
       string(APPEND exportPackageDependencies "find_dependency(LAPACK)\n")
     endif()
     ```
   - If `phreeqcpp/common` is not already an include directory of `PhreeqcRM`, stop and report: `newton_linear_solver_state.h` needs `phrqtype.h`.

4. **Build trees.** Use `-DCMAKE_BUILD_TYPE=Release` and `-j 8` for all of them:

   | Directory | Additional options |
   |---|---|
   | `build-patched-openmp-off` | (none) |
   | `build-patched-openmp-lapack` | `-DPHREEQCRM_USE_LAPACK_SOLVER=ON` |
   | `build-patched-openmp-lapack-asan` | `-DPHREEQCRM_USE_LAPACK_SOLVER=ON "-DCMAKE_CXX_FLAGS=-fsanitize=address -fno-omit-frame-pointer" "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address"` |
   | `build-patched-mpi-lapack` | `-DPHREEQCRM_USE_LAPACK_SOLVER=ON -DPHREEQCRM_BUILD_MPI=ON -DMPIEXEC_MAX_NUMPROCS=4` |

   Report the `Found LAPACK` configure line for one tree.

5. **Runs.** Every run uses its own copy of the tree's `Tests/` directory (`cp -r <build>/Tests <build>/<case>`), a `touch`ed marker, and `timeout 600`. Each runs with `OPENBLAS_NUM_THREADS=1`. For MPI, pass the environment with `mpiexec -x OPENBLAS_NUM_THREADS -x PHREEQC_LINEAR_SOLVER -x PHREEQC_LINEAR_SOLVER_STATISTICS …`. Capture stderr to `stderr.txt` in the case directory.

   | Tree | Case | Command | Compare with |
   |---|---|---|---|
   | openmp-off | `Tests_off` | `./TestRM` | `openmp_run1` |
   | openmp-lapack | `Tests_lapack` | `PHREEQC_LINEAR_SOLVER_STATISTICS=1 ./TestRM` | `openmp_run1` |
   | openmp-lapack | `Tests_cl1` | `PHREEQC_LINEAR_SOLVER=cl1 ./TestRM` | `openmp_run1` |
   | openmp-lapack-asan | `Tests_asan` | `ASAN_OPTIONS=detect_leaks=1 PHREEQC_LINEAR_SOLVER_STATISTICS=1 ./TestRM` | `openmp_run1` |
   | mpi-lapack | `Tests_np1` / `Tests_np2` / `Tests_np4` | `PHREEQC_LINEAR_SOLVER_STATISTICS=1`, `mpiexec -n <k> ./TestRM` | `Tests_np<k>` |
   | mpi-lapack | `Tests_np2_cl1` | `PHREEQC_LINEAR_SOLVER=cl1`, `mpiexec -n 2 ./TestRM` | `Tests_np2` |

   Also run `TestRMdtor` and `TestBMIdtor` once in `Tests_lapack` (OpenMP), and with `mpiexec -n 1` in `Tests_np1` (MPI). Each must exit 0.

6. **Comparisons (P2-D16).** For every case, compare each output file with the same-named file in the baseline directory from the table:
   - `.chem.txt`: `compare_phreeqcrm_chem_output.py`;
   - `_utility.txt`: `compare_main_output_to_baseline.py`;
   - `.log.txt`: `cmp`;
   - `.dmp`: `cmp`, reported only.

   Report one line per file in the form `<case> | <file> | <tool> | <result>`. For anything other than `.dmp` that is not identical, show the tool's full output.

7. **Statistics.** In each case directory, count in `stderr.txt`:
   - the number of `phreeqc_linear_solver_statistics:` lines, and how many show `backend=lapack` (or `backend=cl1` in the `cl1` cases);
   - the sums of `solves`, `accepted`, `cl1_fallbacks`, `non_finite_inputs` and `ineligible_systems` over all lines;
   - the sum of each reason in the `phreeqc_linear_solver_ineligible:` lines;
   - any other stderr line, and any ASan report.

   Show the counting command you used.

## Report (in your final reply)

Give pass or fail and the outputs for every step, the full `git -C ~/phreeqcrm diff --stat` and `git -C ~/phreeqcrm status --short` at the end, any deviation from this specification with the reason for it, and a section **"Reviewer decisions required"** that lists every non-identical comparison other than `.dmp`, any case with `accepted < solves` or `cl1_fallbacks > 0`, any ASan report, and any run that failed. Then stop.
