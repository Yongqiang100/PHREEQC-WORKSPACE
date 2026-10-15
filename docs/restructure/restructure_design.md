# Restructuring Design: a Flat PHREEQC/PhreeqcRM Suite

| Field | Value |
|---|---|
| Status | Draft for review, Revision 1 (2026-10-15) |
| Requirement | Keep taking USGS releases of phreeqc3 and PhreeqcRM (answered "yes", 2026-10-15) |
| Verification | Items marked **[VERIFY]** are checked in task R0 before anything moves |

## 1. Problems Being Solved

1. **Nesting.** PhreeqcRM nests four libraries as directories: `src/IPhreeqcPhast/IPhreeqc/phreeqcpp/`, five levels down to the chemistry engine.
2. **Two copies of PHREEQC.** `phreeqc3/src` and PhreeqcRM's `phreeqcpp/` are identical at v3.9.0, so every change had to be applied twice. Our adapter was copied by hand.
3. **One monolithic build target.** About 250 files are compiled into `PhreeqcRM`, so no layer can be built or tested on its own.
4. **Three build systems** (CMake, autotools, Visual Studio leftovers), plus duplicated databases.
5. **Our own sprawl:** about 15 ad-hoc build trees, and documentation and tools split across two repositories.

## 2. Target Layout: a New Repository, `~/phreeqc-suite`

```text
phreeqc-suite/
├── CMakeLists.txt               top level: options; adds src/, tests/, benchmarks/
├── CMakePresets.json            standard builds (§4)
├── src/
│   ├── phreeqc_common/          PHRQ_io, PHRQ_base, Parser, Utils, phrqtype.h      -> library phreeqc_common
│   ├── phreeqc/                 the chemistry engine (one copy) + keywords/        -> library phreeqc_core
│   │   └── app/class_main.cpp   the standalone `phreeqc` program                   -> executable phreeqc
│   ├── iphreeqc/                module interface                                   -> library iphreeqc
│   ├── iphreeqcphast/           transport extensions                               -> library iphreeqcphast
│   ├── phreeqcrm/               reaction module (PhreeqcRM.*, VarManager, …)       -> library phreeqcrm_core
│   ├── interfaces/              C, Fortran, BMI, YAML wrappers                     -> library phreeqcrm_interfaces
│   └── newton_solver/           our adapter (options, state, LAPACK/PETSc, shadow/steering)
├── python/                      SWIG bindings, pyproject (built only with an option)
├── tests/
│   ├── phreeqc/                 examples and baselines (from phreeqc3), our adapter tests
│   └── phreeqcrm/               PhreeqcRM test programs and inputs
├── benchmarks/                  CO2-basalt driver, series scripts, profiling inputs
├── data/databases/              phreeqc.dat, llnl.dat, … (one copy)
├── tools/                       comparison scripts, sync_upstream.py (§3)
├── docs/                        designs, decisions, reports (both projects), upstream docs
└── .opencode/, AGENTS.md
```

**Library dependencies:**

```text
phreeqc_common ← phreeqc_core ← iphreeqc ← iphreeqcphast ← phreeqcrm_core ← phreeqcrm_interfaces
                     ↑
               newton_solver (optional, PRIVATE to phreeqc_core)
```

An umbrella target, `PhreeqcRM::PhreeqcRM`, links all the layers. It keeps the name that downstream users and our benchmark driver already use, so existing `find_package(PhreeqcRM)` consumers keep working.

**Rules for the layout:**
1. No file content changes during the move, except include paths that the audit (R0) shows to be necessary. All other changes are separate commits.
2. Every compile definition keeps its exact per-file effect (§5).
3. Autotools and Visual Studio files are not carried over. CMake is the only build system.

## 3. Taking USGS Releases: Vendor Branch and Sync Script

**Branches in `phreeqc-suite`:**
- **`upstream`**: pure USGS content in the flat layout and nothing else. Each commit is one imported release, for example "Import phreeqc3 v3.9.0 + PhreeqcRM v3.9.0".
- **`main`**: `upstream` plus our work (the adapter, the hook in `model.cpp`, CMake, tests, docs).

**`tools/sync_upstream.py`** (written in R1):
- **Inputs:** clean checkouts of USGS phreeqc3 and PhreeqcRM at a tag.
- **The mapping table**, upstream path → flat path, is one data structure in the script. For example:
  - `phreeqcrm:src/IPhreeqcPhast/IPhreeqc/phreeqcpp/*.{cpp,h,cxx}` → `src/phreeqc/`
  - `…/phreeqcpp/common/*` → `src/phreeqc_common/`
  - `phreeqcrm:src/IPhreeqcPhast/IPhreeqc/*` → `src/iphreeqc/`
- **What it does:** copies every mapped file into an `upstream` worktree and reports files added, removed, or not mapped. Unmapped upstream files make the script **fail**, so nothing is ever silently dropped. It checks that the PHREEQC engine files in the two upstream repositories are identical, and stops if they are not.
- **Release procedure:** run the script; commit on `upstream`; `git merge upstream` into `main`. Git's three-way merge then carries our changes (the hook and the CMake edits) forward, exactly as for any branch.

## 4. Standard Builds (`CMakePresets.json`)

Inherit from PhreeqcRM's existing presets where they fit. Named configurations replace the ad-hoc trees:

| Preset | Contents |
|---|---|
| `release` | serial PHREEQC + PhreeqcRM (OpenMP), no Newton solver |
| `release-lapack` | + LAPACK backend |
| `mpi-release`, `mpi-release-lapack` | MPI configuration |
| `debug`, `asan-lapack` | diagnostics |
| `petsc` | + PETSc backend (non-MPI) |
| `profile` | `-pg` or frame pointers, for the profiling workflow |

Each preset has a matching `testPresets` entry, so `ctest --preset <name>` runs the right tests.

## 5. Compile Definitions: the Main Technical Risk

PhreeqcRM compiles everything with one target-wide set of definitions:
- PUBLIC: `USE_MPI`, and `USE_YAML` (conditional);
- PRIVATE: `USE_OPENMP`, `USE_PHRQ_ALLOC`, `SWIG_SHARED_OBJ`, `IPhreeqc_EXPORTS`, `NO_UTF8_ENCODING`, `USE_GZ`.

The standalone phreeqc build uses different ones. When the target is split, each library must receive exactly the definitions that its files received before, otherwise headers could be compiled differently in different libraries (an ODR or class-layout risk).

**Check:** in R0, list every `#if`/`#ifdef` on these macros in **headers**. After the move, compare the per-file compile commands of the old and new builds (`compile_commands.json`, normalised for paths) and require **identical** flags and definitions for every file. That comparison is a gate of R3.

## 6. Plan

| Task | Content | Gate |
|---|---|---|
| R0 | Read-only audit: full file inventory of both upstream trees at v3.9.0, with byte comparison of the duplicated PHREEQC files; the proposed mapping table; every `#include` with a path component, or crossing a layer; every macro tested in headers; every build-script reference to source paths (CMake, SWIG, pyproject, Doxygen); the definitions per file in the current builds (`compile_commands.json`) | Mapping covers every file; risks listed |
| R1 | `phreeqc-suite` repository, `sync_upstream.py` and its mapping table, and the `upstream` branch with the v3.9.0 import. The script is run twice to show it is reproducible | Script reproducible; nothing unmapped |
| R2 | Per-layer CMake, presets, umbrella target; `upstream` builds alone (no Newton solver) | All upstream tests pass; compile commands match the old builds (§5) |
| R3 | `main`: our adapter, the `model.cpp` hook, tests, tools, benchmarks, docs and agent setup, ported from `phreeqc3` (`petsc-phase1b-complete`) and `phreeqcrm` (`newton-solver-phase2-task3`) | Every baseline byte-identical (§7) |
| R4 | A rehearsal of the release procedure: import the current USGS development heads into a scratch branch with the script, merge into a copy of `main`, report the conflicts | The procedure works end to end |

## 7. Acceptance (R3)

1. **phreeqc executable:** ex1 and ex2 byte-identical to `tests/.../baseline` (`cmp` for `.sel`, the script for `.out`). With the Newton solver ON, the Phase 2 Task 3 statistics are identical.
2. **PhreeqcRM:** the v3.9.0 OpenMP and MPI baselines (`~/phreeqcrm_phase2_baseline/v390/`) compared under the P2-D16 method, including the LAPACK cases, the known exception, and the summary sums (490/368/122 and so on).
3. **Adapter tests:** all unit tests pass in every preset, ASan included.
4. **Benchmark:** the driver builds against the new installed `PhreeqcRM::PhreeqcRM`, and its concentrations table is identical to Phase 2 Task 4.
5. **Compile commands:** per-file definitions identical to the old builds (§5).

## 8. Open Decisions

1. **Which interfaces to keep** (C, Fortran, Python, BMI, YAML). The default is to keep all of them, each behind a CMake option. Dropping unused ones later is a one-line option change, not a restructuring.
2. **The repository name and location** (`~/phreeqc-suite`).
3. **What happens to the old repositories.** They are kept read-only as history, with their tags. Nothing is deleted.
