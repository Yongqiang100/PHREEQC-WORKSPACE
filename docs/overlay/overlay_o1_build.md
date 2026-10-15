# Overlay Task O1: Build the `phreeqc_newton_solver/` Folder

## Context

The overlay design (`docs/overlay/overlay_design.md`) and decisions OV-D1 to OV-D5 (`docs/overlay/overlay_decisions.md`) define a single folder. Copied into an official phreeqc3 or PhreeqcRM tree, and enabled with `-DCMAKE_PROJECT_INCLUDE=<folder>/cmake/attach.cmake`, it adds the Newton linear-solver integration (LAPACK backend, condition gate, comparison, shadow and steering modes, statistics) through five anchored edits. This task builds that folder in `~/phreeqc3/overlay/phreeqc_newton_solver/` and smoke-tests it on scratch copies of the two upstream trees. Full validation is O2.

## Rules

1. Create files only under `~/phreeqc3/overlay/phreeqc_newton_solver/`, and scratch trees under `~/overlay_scratch/`. Change nothing else in `~/phreeqc3`, nothing in `~/phreeqcrm`, and nothing in `~/upstream/` or `~/phreeqcrm_baseline_src` (read-only).
2. The overlay's adapter sources are **copies** of the current ones in `~/phreeqc3/src/petsc_linear_solver/` (tag `petsc-phase1b-complete`). Change them only where OV-D2 requires it (moving logic into the glue), and list every such change.
3. Code is C++11. Functions called from PHREEQC code never throw and never abort. Nothing depends on `USE_MPI`, `USE_OPENMP`, `SWIG_SHARED_OBJ` or `USE_PETSC`.
4. Write files in parts of at most about 150 lines per edit, and keep your reasoning short before each tool call. Run long builds in the background (`nohup … &`) and poll them.
5. Use Plan mode first, present the plan, and implement only after the reviewer replies `APPROVED`. Do not run `git commit`. The report is given in your final reply.

## Folder Contents

```text
phreeqc_newton_solver/
├── README.md                 short for now (O3 completes it): usage, the one flag, the run-time variables
├── VERSION                   1.0.0
├── SUPPORTED_UPSTREAM.txt    the three SHA-256 values of audit A4, with the releases they come from
├── cmake/attach.cmake        entry point (OV-D3)
├── cmake/edits.cmake         newton_solver_apply_edits(<engine_dir>) (OV-D1)
├── cmake/detach.cmake        script mode: cmake -DTREE=<tree> -P detach.cmake
├── src/glue/                 newton_solver_phreeqc_forward.h, _members.h, _glue.cpp (OV-D2)
├── src/core/                 linear_solve_status.h, newton_linear_solver_options.*, newton_linear_solver_state.*
├── src/lapack/               lapack_newton_solver.*, cl1_problem_check.*
├── tests/                    test_newton_linear_solver_options.cpp, test_lapack_newton_solver.cpp,
│                             test_cl1_problem_check.cpp, test_newton_linear_solver_state.cpp (copies)
└── tools/                    the comparison and analysis scripts of tests/petsc_linear_solver/tools/ (copies)
```

## Specification

### `edits.cmake`: `newton_solver_apply_edits(engine_dir)`

1. **Already edited:** if `Phreeqc.h` contains `phreeqc_newton_solver edit`, the tree is already edited. In that case:
   - check that every `<file>.phreeqc_newton_solver.orig` exists and has a supported SHA-256;
   - re-derive the edited text from each `.orig` with the five replacements, and require it to equal the current file, byte for byte;
   - on any mismatch: `FATAL_ERROR` naming the file. Otherwise print `phreeqc_newton_solver: upstream edits already present` and return.
2. **Supported version:** otherwise, compute `file(SHA256 …)` of the three files and compare them with `SUPPORTED_UPSTREAM.txt`. On a mismatch: `FATAL_ERROR` naming the file, its hash, and the supported releases.
3. **Dry run:** for each of E1–E5, `file(READ)`, then require the anchor to occur exactly once: `string(FIND)` from the front and with `REVERSE` must give the same, non-negative position. Otherwise `FATAL_ERROR` naming the edit. **Nothing is written unless all five pass.**
4. **Apply:** copy each file to `<file>.phreeqc_newton_solver.orig` (only if that does not exist yet), then write the replaced text with `file(WRITE)`. Print one line per edit.

Write the anchors and replacements exactly as in OV-D1, using CMake string escapes `\t` and `\n`. Store them in one list, so that `detach.cmake` and the re-check of step 1 share it.

### `detach.cmake`

`cmake -DTREE=<tree root> -P detach.cmake`:
1. find the engine directory;
2. for each `.orig`, check that its hash is supported;
3. copy it back over the edited file;
4. delete the `.orig`;
5. confirm with `file(SHA256)`.

Fail without writing if any check fails.

### `attach.cmake`

1. **Guards:**
   - return if `NOT CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR`;
   - `include_guard(GLOBAL)`;
   - `FATAL_ERROR` if `CMAKE_VERSION VERSION_LESS 3.19`.
2. **Tree detection:**
   - PhreeqcRM: `${CMAKE_SOURCE_DIR}/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/model.cpp` exists; target `PhreeqcRM`, link keyword `PUBLIC`;
   - phreeqc3: `${CMAKE_SOURCE_DIR}/src/model.cpp` exists; target `phreeqc`, plain link;
   - otherwise `FATAL_ERROR`.
3. **Edits:** `include(edits.cmake)`, then `newton_solver_apply_edits(<engine dir>)`.
4. **Deferred attachment:** `cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL newton_solver_attach_target <target> <keyword>)`. That function:
   - adds the glue, core and LAPACK sources with `target_sources(PRIVATE)`;
   - adds the three overlay source directories with `target_include_directories(PRIVATE)`. The engine directory is already on the target's include path, so `Phreeqc.h` finds the glue headers through the PRIVATE glue directory, and every engine file compiles with it. Verify this in the smoke test;
   - adds `target_compile_definitions(PRIVATE USE_NEWTON_LINEAR_SOLVER USE_LAPACK_NEWTON_SOLVER)`;
   - calls `find_package(LAPACK REQUIRED)` and `find_package(Threads REQUIRED)`, then links `LAPACK::LAPACK` and `Threads::Threads` with the tree's keyword (none for phreeqc3);
   - if `NEWTON_SOLVER_BUILD_TESTS` (default ON), adds the four test executables. Each test compiles the core and LAPACK sources itself (no glue), links LAPACK and Threads, and is registered with `add_test` under the names `newton_solver.<test>`. Call `enable_testing()` if it has not been called.
5. **Install and export (OV-D4).** Before choosing a mechanism, read and quote PhreeqcRM's `CMakeLists.txt` lines where `exportPackageDependencies` is set, appended and used, and where `PhreeqcRMConfig.cmake` is generated. Then implement the smallest approach that makes the installed config contain `find_dependency(LAPACK)` and `find_dependency(Threads)`. State which approach you chose, and why.

### Glue (OV-D2)

- **`newton_solver_phreeqc_forward.h`:** include guard; `struct NewtonLinearSolverState;`. Nothing else.
- **`newton_solver_phreeqc_members.h`:** no include guard (it is included once, inside the class), no `#include`, no access specifier. It declares:
  ```cpp
  NewtonLinearSolverState *newton_linear_solver_state = nullptr;
  int ineq_cl1_original(int in_kode);
  int newton_solver_ineligibility_reason(int in_kode) const;
  const LDBLE *augmented_newton_matrix(void) const;
  int newton_solver_before_cl1(int in_kode, int k, int l, int m, int n, LDBLE *ineq_array_data,
                               LDBLE *delta1_data, LDBLE *res_data, int *l_kode, int *l_iter, LDBLE *l_error);
  void newton_solver_after_cl1(int l_kode, LDBLE *delta1_data);
  void newton_solver_destroy_state(void);
  ```
  Verify in the upstream `Phreeqc.h` that none of these names already exists. Also verify that `LDBLE` is known at line 449, meaning `phrqtype.h` is included before the class.
- **`newton_solver_phreeqc_glue.cpp`:** `#include "Phreeqc.h"`, the state, options and cl1-problem headers, and the definitions:
  - **`Phreeqc::ineq`, the eligibility function and the accessor:** moved verbatim from `~/phreeqc3/src/model.cpp` at `petsc-phase1b-complete` (audit A2.1 and A2.2), apart from removing the `#ifdef USE_NEWTON_LINEAR_SOLVER` wrappers;
  - **`newton_solver_before_cl1`:** in shadow mode, the body of the current shadow block (building the unknown types, calling `cl1_problem_shadow_prepare` with the stride and flags computed from the members, as in A2.3), returning 0. In steering mode, the current steering call, returning its result. Otherwise 0;
  - **`newton_solver_after_cl1`:** calls `cl1_problem_shadow_record(newton_linear_solver_state, l_kode, delta1_data)` if `cl1_problem_shadow_active(...)`;
  - **`newton_solver_destroy_state`:** `destroy_newton_linear_solver_state(&newton_linear_solver_state);`.

  The shadow mode's unknown-types vector must live until `record`. Keep it in the state, as a `std::vector<int>` field; do not make it a local.

## Smoke Test (O2 does the full validation)

1. **Scratch trees:** `rsync -a --exclude .git ~/upstream/phreeqc3_v390/ ~/overlay_scratch/phreeqc3/`, and the same from `~/phreeqcrm_baseline_src/` to `~/overlay_scratch/phreeqcrm/`. Copy the folder into each.
2. **phreeqc3:** configure with `-DCMAKE_BUILD_TYPE=Release -DCMAKE_PROJECT_INCLUDE=<…>/attach.cmake`, build, then:
   - `ctest -R newton_solver`: 4 of 4 pass;
   - run ex1 with `PHREEQC_LINEAR_SOLVER_STATISTICS=1`: `.sel` identical to `~/phreeqc3/tests/petsc_linear_solver/baseline/`, statistics equal to Task 3 (8 of 8).
3. **Reconfigure** the same tree: the output must say the edits are already present, and nothing may change (compare the `sha256sum` of the three files before and after).
4. **Detach,** then compare the three files with `~/upstream/phreeqc3_v390/src/` using `cmp`: identical.
5. **PhreeqcRM:**
   - configure with the flag (OpenMP, Release), build, `ctest -R newton_solver`;
   - run `TestRM` once in a copy of the build's `Tests/` directory, with `PHREEQC_LINEAR_SOLVER_STATISTICS=summary`, and compare the summary sums with Phase 2 Task 3 (490/368/122);
   - `cmake --install <build> --prefix ~/overlay_scratch/install_rm`, then build the benchmark driver against it (`-DCMAKE_PREFIX_PATH=…`).
6. **Unsupported version:** in a third scratch copy of phreeqc3, append one byte to `model.cpp`, configure with the flag, and confirm a `FATAL_ERROR` that names `model.cpp`, with none of the three files changed.

## Report (in your final reply)

Report:
- the file list with line counts;
- every change to the copied adapter sources (Rule 2);
- the OV-D4 quotes, the approach chosen and why;
- pass or fail and the outputs for every smoke-test step;
- any deviation, with the reason for it.

Then stop.
