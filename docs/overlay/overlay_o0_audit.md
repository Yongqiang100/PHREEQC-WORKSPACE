# Overlay Task O0: Audit of Our Upstream Edits and Choice of Anchors (read-only)

## Context

The overlay design (`docs/overlay/overlay_design.md`) packages the whole integration as one folder, `phreeqc_newton_solver/`. It touches upstream sources through **six one-line anchored edits** in `Phreeqc.h`, `model.cpp` and `Phreeqc.cpp`, applied at configure time by CMake. This audit gives the facts needed to write those edits, for both official trees.

**Trees, all read-only:**
- `P3=~/upstream/phreeqc3_v390`: phreeqc3 at `b0b3be76` (v3.9.0);
- `RM=~/phreeqcrm_baseline_src`: PhreeqcRM v3.9.0;
- `~/phreeqc3` at tag `petsc-phase1b-complete`, our integrated phreeqc3;
- `~/phreeqcrm` at tag `newton-solver-phase2-task3`, our integrated PhreeqcRM.

## Rules

1. Read-only. Edit only `docs/overlay/overlay_o0_audit_report.md`. Data files may go under `~/phreeqc3/build/overlay/`. No builds or configures. Never run `git commit`, `checkout`, `apply`, `reset`, `stash`, or `clean`.
2. Every claim gives a file path with line numbers and the quoted lines, or a command with its output. Write NOT FOUND or AMBIGUOUS rather than guessing.
3. Use Plan mode first, present the plan, and execute only after the reviewer replies `APPROVED`. Keep your reasoning short before each tool call, and write the report in parts of at most about 150 lines per edit.

## Items

**A1. Our current edits.** Save each of these to `build/overlay/` and report its `--stat` and every hunk header:
- `git -C ~/phreeqc3 diff b0b3be76 petsc-phase1b-complete -- src/Phreeqc.h src/model.cpp src/Phreeqc.cpp src/class_main.cpp`;
- `git -C ~/phreeqcrm diff v3.9.0 newton-solver-phase2-task3 -- src/IPhreeqcPhast/IPhreeqc/phreeqcpp/`.

For each hunk, state what it does: forward declaration, member, rename, dispatcher, eligibility, shadow hook, steering hook, destructor, PETSc pin, or include.

**A2. Movable logic.** Quote the full current text, from `~/phreeqc3` at `petsc-phase1b-complete`, of:
- the dispatcher `Phreeqc::ineq`;
- `newton_solver_ineligibility_reason`;
- the shadow block and the steering block around `cl1(`;
- the destructor call.

For each, list every Phreeqc member and every local variable of `ineq_cl1_original` it uses, and state whether it could live in a separate `.cpp` file as a member function. A local variable must instead be passed as an argument.

**A3. Anchors in the upstream files** (the same text in `$P3/src/` and `$RM/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/`). For each of the six edits of design §3, propose an anchor: an exact text fragment, including line breaks, at the edit position. Then report `grep -c` (or a multi-line count with `python3 -c`) of that fragment in **both** files, which must be 1 in each. For edit (3), the definition `int Phreeqc::` followed by `ineq(int in_kode)` on the next line, quote the exact bytes, including tabs. For edits (4) and (5), quote the full `cl1(` call statement, with its first and last line numbers, and propose the anchors. If no unique anchor exists for an edit, say so and propose the nearest unique alternative.

**A4. File hashes.** Report `sha256sum` of `Phreeqc.h`, `model.cpp`, `Phreeqc.cpp` and `class_main.cpp` in both upstream trees, and state whether they are identical.

**A5. Build attachment facts.**
- For `$P3/CMakeLists.txt` and `$RM/CMakeLists.txt`, quote:
  - the top-level `project(...)` line;
  - the name of the target that compiles `model.cpp`;
  - its include directories;
  - its link-style precedent (plain or keyword);
  - `cmake_minimum_required`.
- List every `project(` call reached during PhreeqcRM's configure, i.e. in subdirectories and fetched content (`grep -rn "project(" --include=CMakeLists.txt $RM`).
- Report `cmake --version`.

**A6. The PETSc pin.** Quote the `class_main.cpp` edit, and the comments and decisions behind it (D10, P2-D4). State whether, with only the LAPACK backend, anything requires it. Whether PETSc alone (without the pin) would still finalise correctly through the `atexit` handler is a question to put to the reviewer; mark it AMBIGUOUS if it cannot be settled from the code.

## Output

Write `docs/overlay/overlay_o0_audit_report.md` with items A1–A6. End with **"Reviewer decisions required"**, listing:
- any anchor that is not unique;
- any logic that cannot move to the glue file;
- any difference between the two upstream trees;
- the open PETSc-pin question.

Then stop.
