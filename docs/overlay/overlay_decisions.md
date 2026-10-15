# Drop-in Overlay: Reviewer Decisions

These decisions were written by the reviewer. Agents treat them as authoritative and must not edit this file. They resolve `overlay_o0_audit_report.md`.

**OV-D1. The edits are frozen: five anchored replacements in three files.** All anchors are unique in both official trees, at identical line numbers. The SHA-256 values of `Phreeqc.h`, `model.cpp` and `Phreeqc.cpp` are identical in phreeqc3 `b0b3be76` and PhreeqcRM v3.9.0. Edits (4) and (5) of the design are merged into one replacement of the unique three-line `cl1(` statement. The edits are:

| # | File | Anchor (exact bytes) | Replacement |
|---|---|---|---|
| E1 | `Phreeqc.h` | `#include "global_structures.h"\n\nclass Phreeqc` (the three-line anchor counted in audit A3) | `#include "global_structures.h"\n#include "newton_solver_phreeqc_forward.h" /* phreeqc_newton_solver edit 1 */\n\nclass Phreeqc` |
| E2 | `Phreeqc.h` | `\tint ineq(int kode);\n` | the anchor + `#include "newton_solver_phreeqc_members.h" /* phreeqc_newton_solver edit 2 */\n` |
| E3 | `model.cpp` | `int Phreeqc::\nineq(int in_kode)\n` | `int Phreeqc::\nineq_cl1_original(int in_kode) /* phreeqc_newton_solver edit 3 */\n` |
| E4 | `model.cpp` | `\tcl1(k, l, m, n, l_nklmd, l_n2d, &ineq_array[0],\n\t\t&l_kode, ineq_tol, &l_iter, &delta1[0], &res[0],\n\t\t&l_error, &cu[0], &iu[0], &is[0], FALSE);\n` | `\tif (!newton_solver_before_cl1(in_kode, k, l, m, n, &ineq_array[0], &delta1[0], &res[0], &l_kode, &l_iter, &l_error)) /* phreeqc_newton_solver edit 4 */\n` + the anchor + `\tnewton_solver_after_cl1(l_kode, &delta1[0]); /* phreeqc_newton_solver edit 4 */\n` |
| E5 | `Phreeqc.cpp` | `Phreeqc::~Phreeqc(void)\n{\n` | the anchor + `\tnewton_solver_destroy_state(); /* phreeqc_newton_solver edit 5 */\n` |

E1 must precede `class Phreeqc` (audit A3: lines 72–74). E2 sits inside the class, directly after the public declaration of `ineq`. `newton_solver_phreeqc_members.h` therefore contains **no access specifier and no `#include`**, so the declarations that follow it keep their original access.

**OV-D2. All logic moves to the glue file** (`glue/newton_solver_phreeqc_glue.cpp`, `Phreeqc` member functions):
- the dispatcher `Phreeqc::ineq`;
- `newton_solver_ineligibility_reason`;
- the accessor `augmented_newton_matrix`;
- `newton_solver_before_cl1`;
- `newton_solver_after_cl1`;
- `newton_solver_destroy_state`.

The before-hook runs the shadow `prepare` or the steering decision, in the respective mode. It computes the stride and the exclusion flags from members, and returns 1 only when steering accepted. The after-hook calls the shadow `record`, which already does nothing unless `pending` is set. So the local shadow flag of the current code is no longer needed, as the audit proposed.

**OV-D3. CMake: version requirement and attachment.**
- **Version:** `attach.cmake` requires CMake 3.19 or later (`cmake_language(DEFER)`), and fails with a clear message otherwise. phreeqc3's own `cmake_minimum_required(3.10)` is untouched, and the higher requirement applies only when the overlay is used.
- **Attachment:** use `target_sources`, `target_include_directories` and `target_compile_definitions` (all PRIVATE) on the detected target: `phreeqc` (an executable, plain link style) or `PhreeqcRM` (a library, keyword link style).
- **Guard:** `CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR` skips the nested `project()` calls of PhreeqcRM's `Tests/` and of any fetched content.

**OV-D4. PhreeqcRM install and export.** A PhreeqcRM consumer that uses `find_package(PhreeqcRM)`, such as our benchmark driver, must still configure and link. O1 establishes, and reports with quotes:
- where and when PhreeqcRM generates `PhreeqcRMConfig.cmake` from `exportPackageDependencies`;
- whether a deferred attachment is too late to add `find_dependency(LAPACK)` and `find_dependency(Threads)`.

If it is too late, the dependency lines are added at project-include time, provided PhreeqcRM's `CMakeLists.txt` does not reset that variable afterwards. Otherwise, choose the smallest alternative that works, and report it. O2 tests installation plus the driver build.

**OV-D5. Overlay version 1 ships the LAPACK backend only.**
- PETSc is not recommended for this chemistry (D19, P2-D10).
- The PETSc pin is provably unnecessary without PETSc (audit A6.3).
- Leaving PETSc out avoids its discovery, MPIUNI and finalisation questions (A6.4).

The PETSc backend remains in our phreeqc3 branch. `glue` and `core` are compiled without `USE_PETSC`, and `src/petsc/` is not part of version 1.

**OV-D6. Results of O1: accepted.**
- **Files:** the folder is complete. The three CMake scripts were written in this session, and the two OV-D2 changes in `src/core/newton_linear_solver_state.{h,cpp}` are the only differences from the phreeqc3 adapter.
- **Smoke tests, all passed:**
  - phreeqc3: 4 of 4 tests; ex1 statistics identical to a fresh reference run (`solves=8 accepted=8`); `.sel` identical to the baseline;
  - re-configure is a no-op, and `detach` restores byte-identical upstream files;
  - PhreeqcRM: 4 of 4 tests, `TestRM` sums exactly 490/368/122;
  - OV-D4: the installed config lists `find_dependency(LAPACK)` and `find_dependency(Threads)` before the targets include, once each, and the benchmark driver builds against the installed library;
  - an unsupported version fails cleanly, with nothing written.
- **Deviation accepted:** the five edits are stored as separate variables instead of one CMake list, because the E2 anchor contains `;`, which CMake lists split on. The edit strings are byte-identical to OV-D1. **This is a lesson for O3's documentation:** never pass the anchors through CMake lists.
- **Deviation accepted:** `tests/petsc_linear_solver/inputs/` was copied into the scratch tree. Official trees have no such directory, and O2 handles that the same way.

**OV-D7. Results of O2, accepted; the release policy for overlay 1.0.0.**
- **(a) O2 passed completely, on fresh official clones.**
  - **Provenance:** the official phreeqc3 tag v3.9.0 is commit `b0b3be76`, the base of all our work, and PhreeqcRM v3.9.0 is `90b31ed8`. The three engine files match `SUPPORTED_UPSTREAM.txt` in both clones.
  - **Without the flag:** byte-identical to upstream.
  - **With the overlay:** default, shadow and steering modes are identical to the reference integration on all 10 inputs. The PhreeqcRM sums are exact under OpenMP and MPI at 2 and 4 ranks. ASan is clean on both trees. The installed library is usable by the benchmark driver, with identical concentrations.
  - **Detach** leaves `git diff` empty on both clones.
  - **Only difference:** the known pe −0.601 against −0.602 exception. No overlay bug was found.
- **(b) Release policy: the overlay's default backend is `cl1`.** In our development trees the default backend is `lapack`, in `entry` mode. For distribution to others, an overlay build with no environment variables must reproduce upstream PHREEQC **exactly**, so that no-one gets a silently different answer. Even `entry` mode moves pe in unpoised solutions, as in the known exception. Users opt in with `PHREEQC_LINEAR_SOLVER=lapack`, or at build time with `-DNEWTON_SOLVER_DEFAULT_BACKEND=lapack`. That correction to the design's §1 wording ("without variables, behaviour is exactly upstream") is implemented in O3.
- **(c) Documentation is written by the reviewer** (`README.md`, `docs/RESULTS.md`, `docs/VALIDATION.md`) and installed by O3 verbatim. The `LICENSE` text is chosen by the user and placed in the folder before O3.

**OV-D8. Results of O3, accepted; overlay 1.0.0 released.**
- **Every step passed on fresh official clones.**
  - **Default (`cl1`):** phreeqc3 ex1/ex2 identical to upstream; PhreeqcRM `TestRM` **23 of 23 non-`.dmp` files identical** to unmodified v3.9.0, including `Advect_cpp_units_utility.txt`. So the release default is exactly upstream.
  - **`lapack` at run time:** reproduces O2 (statistics byte-identical; 490/368/122/122).
  - **`lapack` as build default:** reproduces the run-time choice.
  - **An invalid option** is refused, with nothing written.
  - **Detach** leaves `git diff` empty.
  - **The consumer test,** run from the tarball following the README, passed for both trees.
- **The agent's three README findings are accepted and fixed:**
  1. a separate build directory per tree (`build-phreeqcrm`, `build-phreeqc3`), with both trees spelled out;
  2. extracting the tarball as the first step;
  3. `git -C <tree> diff --stat` after removal.

  The README also documents `-DNEWTON_SOLVER_DEFAULT_BACKEND`. Because the README is inside the tarball, the tarball is rebuilt and its SHA-256 recorded again. Only documentation changed, so the O3 validation stands.
- **Restart handling.** After the SSH drop, the agent reconstructed its state from disk (steps 1–2 verified byte for byte), recreated `progress.md`, and continued. Accepted. **Operational rule from now on: run OpenCode inside tmux.**
