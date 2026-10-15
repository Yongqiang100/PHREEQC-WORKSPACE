# Drop-in Overlay Design: `phreeqc_newton_solver/`

| Field | Value |
|---|---|
| Status | Draft for review, Revision 1 (2026-10-15) |
| Goal | Anyone downloads official phreeqc3 or PhreeqcRM, copies **one folder** into it, and rebuilds with **one extra CMake flag**. No manual editing |
| Relation to the restructuring | Independent. The overlay is for distribution to others; the restructuring was for maintaining our own fork. If the overlay is adopted, the restructuring can stop after R0 |

## 1. What Users Do

```bash
git clone https://github.com/usgs-coupled/phreeqcrm.git        # or an official release tarball
cp -r /path/to/phreeqc_newton_solver phreeqcrm/
cmake -S phreeqcrm -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PROJECT_INCLUDE=$PWD/phreeqcrm/phreeqc_newton_solver/cmake/attach.cmake \
      -DNEWTON_SOLVER_LAPACK=ON
cmake --build build -j 8
```

It works the same way for phreeqc3. At run time, the environment variables of decision P2-D11 apply (`PHREEQC_LINEAR_SOLVER`, `…_STATISTICS`, `…_INTERCEPT` and so on). **Without those variables, behaviour is exactly that of upstream PHREEQC (cl1)**, as validated throughout Phases 1–2. Without the `-DCMAKE_PROJECT_INCLUDE` flag, the folder does nothing at all.

## 2. Folder Layout (everything in one place)

```text
phreeqc_newton_solver/
├── README.md                    usage, run-time options, validation advice, limitations
├── LICENSE
├── VERSION                      overlay version
├── SUPPORTED_UPSTREAM.txt       supported releases + SHA-256 of each upstream file the overlay edits
├── cmake/
│   ├── attach.cmake             entry point (CMAKE_PROJECT_INCLUDE): detects the tree, edits, attaches
│   ├── edits.cmake              the anchored source edits (§4), dry-run check then apply
│   └── detach.cmake             `cmake -P detach.cmake <tree>`: removes every edit exactly
├── src/
│   ├── glue/                    Phreeqc member functions moved out of model.cpp (§3)
│   │   ├── newton_solver_phreeqc_members.h       declarations, included inside class Phreeqc
│   │   ├── newton_solver_phreeqc_forward.h       forward declarations, included before class Phreeqc
│   │   └── newton_solver_phreeqc_glue.cpp        Phreeqc::ineq (dispatcher), eligibility, cl1 hooks, destructor hook
│   ├── core/                    backend-neutral layer: options, state, statistics, comparison
│   ├── lapack/                  LAPACK backend, cl1-problem check (shadow and steering)
│   └── petsc/                   optional PETSc backend
├── tests/                       unit tests (from Phases 1–2 and 1B), registered with CTest when enabled
├── tools/                       comparison and analysis scripts, the benchmark driver
└── docs/                        condensed design, decisions and limitations (redox caveat, opt-in steering)
```

## 3. Minimal Footprint in Upstream Sources

Today our integration edits four upstream files with sizeable blocks. The overlay reduces this to **six one-line, anchored edits in three files**, by moving all of our logic into member functions defined in `glue/newton_solver_phreeqc_glue.cpp`. They are member functions, so they keep access to `my_array`, `x`, `count_unknowns` and so on.

| File | Edit |
|---|---|
| `Phreeqc.h` | (1) `#include "newton_solver_phreeqc_forward.h"` before `class Phreeqc`; (2) `#include "newton_solver_phreeqc_members.h"` inside the class body (declares the state pointer, `ineq_cl1_original`, and the hook members) |
| `model.cpp` | (3) rename the definition `ineq(int in_kode)` to `ineq_cl1_original(int in_kode)`. `Phreeqc::ineq` (the dispatcher) is defined in the glue file. (4) Insert one line directly before the `cl1(` call: `if (!newton_solver_before_cl1(in_kode, k, l, m, n, &ineq_array[0], &delta1[0], &res[0], &l_kode, &l_iter, &l_error))`. (5) Insert one line directly after that statement: `newton_solver_after_cl1(l_kode, &delta1[0]);` |
| `Phreeqc.cpp` | (6) one line in the destructor: `newton_solver_destroy_state();` |

The PETSc `main()` pin in `class_main.cpp` is not needed for LAPACK. For the optional PETSc backend, the existing `atexit` finalisation suffices. Task O0 confirms or refutes this [VERIFY].

**Each edit is defined by an anchor,** an exact upstream text fragment that must occur **exactly once**, plus the text to insert. `edits.cmake`:
1. checks the SHA-256 of each file against `SUPPORTED_UPSTREAM.txt`;
2. checks that every anchor is found exactly once (the dry run);
3. writes the edited file, keeping a `.orig` copy next to it, and a marker line (`/* phreeqc_newton_solver vX.Y edit N */`) so that a second configure recognises the edit and does nothing.

If a file has an unknown hash, an anchor is missing, or an anchor occurs more than once, configuration stops with `FATAL_ERROR` and a message naming the file, the anchor and the supported versions. Nothing is written in that case. `detach.cmake` restores the `.orig` files after checking that they match the recorded hashes.

## 4. Build Attachment

`attach.cmake` runs at the top-level `project()` call:
1. It returns at once for nested `project()` calls, such as yaml-cpp inside PhreeqcRM (guard: `CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR`).
2. It detects the tree. PhreeqcRM has the engine in `src/IPhreeqcPhast/IPhreeqc/phreeqcpp/`, target `PhreeqcRM`; phreeqc3 has it in `src/`, target `phreeqc` [VERIFY names].
3. It runs `edits.cmake`.
4. With `cmake_language(DEFER CALL …)`, after the top-level `CMakeLists.txt` has been processed, it calls `newton_solver_attach(<target>)`. That function:
   - adds the overlay sources;
   - adds `PRIVATE` include directories;
   - adds the definitions `USE_NEWTON_LINEAR_SOLVER` and `USE_LAPACK_NEWTON_SOLVER` (PRIVATE, per P2-D12) and `USE_PETSC` (only with `NEWTON_SOLVER_PETSC=ON`);
   - links `LAPACK::LAPACK` and `Threads::Threads`, and PETSc when it is selected;
   - registers the overlay's tests when `BUILD_TESTING` is on.

   The link style matches the target: plain for phreeqc3 (D2), keyword for PhreeqcRM.

**Requirements:** CMake 3.19 or later (for `DEFER`), LAPACK, and a C++11 compiler. No Python, git or `patch` is needed at build time.

## 5. Validation (Task O2)

On fresh official clones of phreeqc3 v3.9.0 and PhreeqcRM v3.9.0:
1. **Without the flag,** builds and outputs are byte-identical to the unmodified clones.
2. **With the flag and no run-time variables,** results are identical to the Phase 2 Task 3 `lapack_only` and PhreeqcRM runs, with identical statistics.
3. **All overlay tests pass,** ASan included.
4. **Shadow and steering modes** reproduce the Phase 1B Task 3 decisions and outputs.
5. **The edits are reversible:** `detach.cmake` restores byte-identical upstream files.
6. **A second configure is a no-op,** and an unsupported upstream version, simulated by changing one byte, fails cleanly with nothing written.

## 6. Plan

| Task | Content |
|---|---|
| O0 | Audit: the exact current edits in the four upstream files, against v3.9.0 (`git diff` from the tags); choice and uniqueness of the six anchors in both trees; the target names, include directories and link style per tree; whether the `class_main.cpp` pin can go |
| O1 | Build the overlay folder: move the dispatcher, eligibility, hooks and destructor call into the glue file; write `attach`/`edits`/`detach.cmake` and `SUPPORTED_UPSTREAM.txt`; copy the adapter, tests and tools in |
| O2 | Validation per §5 on fresh official clones |
| O3 | README, condensed documentation of limitations and results, and a packaged tarball |

## 7. Honest Scope for Users

The README states plainly, from Phases 1–2 and 1B, what the overlay does and does not deliver:
- default behaviour is unchanged;
- the LAPACK backend covers aqueous-only Newton systems, which are few in mineral-rich chemistry;
- cl1-problem steering is **experimental and opt-in**: about 10 % faster on the CO₂–basalt test, but it can change the redox state in weakly poised systems, so validate against a cl1 run first;
- PETSc is not recommended for cell-local chemistry.

The overlay's main value is a clean, reversible, testable platform for solver research on PHREEQC, plus the diagnostics: comparison mode, shadow mode and statistics.
