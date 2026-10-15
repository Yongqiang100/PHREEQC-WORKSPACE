# Restructuring Task R0: Read-Only Audit Report

Trees: `P3=~/upstream/phreeqc3_v390` (HEAD `b0b3be767158ccc3322d2c816625cf470045e67e`), `RM=~/phreeqcrm_baseline_src` (`git describe --tags` → `v3.9.0`). No repository was modified. Data files are under `~/phreeqc3/build/restructure/` (listed per item).

## U1. Origins

`git -C ~/phreeqc3 remote -v`:
```
upstream	https://github.com/phreeqc-dev/phreeqc3.git (fetch)
upstream	DISABLED (push)
```
`git -C ~/phreeqcrm remote -v`:
```
origin	https://github.com/usgs-coupled/phreeqcrm.git (fetch)
origin	https://github.com/usgs-coupled/phreeqcrm.git (push)
```
`git -C ~/phreeqc3 log --oneline -1 b0b3be76`:
```
b0b3be76 Merge commit 'a4b1596bc0d4ec479a2493541f275af37e9f7349'
```
(author of that merge commit: `41898282+github-actions[bot]@users.noreply.github.com`.)

`git -C $P3 rev-parse HEAD` → `b0b3be767158ccc3322d2c816625cf470045e67e`; `git -C $RM describe --tags` → `v3.9.0`.

Assessment: the phreeqcrm remote **is** the official USGS repository (`usgs-coupled/phreeqcrm`). The phreeqc3 remote is **not** `usgs-coupled/phreeqc3`; it is `https://github.com/phreeqc-dev/phreeqc3.git` (named `upstream`, push disabled). Whether that fork is a faithful mirror of USGS phreeqc3 cannot be established from the local repository: NOT FOUND (no usgs-coupled remote configured).

## U2. Inventory

- `build/restructure/inventory_phreeqc3.txt` — 3334 tracked files.
- `build/restructure/inventory_phreeqcrm.txt` — 506 tracked files.

phreeqc3, per top-level directory:
```
2348 mytest   545 HTMLversion   163 src   112 examples   70 database   36 test
26 (root files)  20 doc   7 .github   5 msi   1 m4   1 bin
```
phreeqc3 `src/` (163): 15 `src/common/` (10 sources + 5 CI files), 4 `src/.github/`, 1 `src/.gitlab-ci.yml`, 2 `src/PhreeqcKeywords/`, 1 `src/class_main.cpp`, 140 other files directly in `src/` (engine sources, headers, VS resources, makefiles, assets).

phreeqcrm, per top-level directory:
```
246 src   88 Tests   70 database   37 swig   26 (root files)   12 Doxygen
9 .github   7 doc   6 m4   4 setup-fortran   1 .vscode
```
phreeqcrm `src/` (246): 208 `src/IPhreeqcPhast/` (203 under `IPhreeqc/`, of which 163 under `phreeqcpp/`; 5 `IPhreeqcPhast.*` files), 33 files directly in `src/` (PhreeqcRM, VarManager, RMVARS, IrmResult, RM/BMI/YAML interfaces, bmi.hxx, BMIVariant, BMIPhreeqcRM), 1 `src/phreeqcrm/__init__.py`, 5 `src/.github/` + `src/.gitlab-ci.yml`.
`src/IPhreeqcPhast/IPhreeqc/` non-phreeqcpp (40): IPhreeqc sources/headers, fwrap*.cpp, Var.c/h, Version.h, thread.h, Debug.h, ErrorReporter.hxx, CVar.hxx, CSelectedOutput.*, 5 `.github/`, `.gitlab-ci.yml`, `Makefile.am`, `README.Fortran`.

## U3. The duplicated PHREEQC

Method: for every tracked file under `$RM/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/`, `cmp` against the same relative path under `$P3/src/`, and the reverse direction. Lists: `build/restructure/dup_identical.txt`, `dup_differ.txt`, `dup_only_rm.txt`, `dup_only_p3.txt`.

- phreeqcpp vs `$P3/src/`: **163 identical, 0 differing, 0 only in PhreeqcRM, 0 only in phreeqc3.** The two engine copies are byte-identical, including the `common/`, `PhreeqcKeywords/` and `.github/` subdirectories that phreeqcpp mirrors from `$P3/src/`.

Method for databases: same `cmp` loop, lists `db_identical.txt`, `db_differ.txt`, `db_only_rm.txt`, `db_only_p3.txt`.

- `$RM/database` vs `$P3/database`: **70 identical, 0 differing, 0 only-in-RM, 0 only-in-P3.**

`$RM/Tests/phreeqc.dat` vs `$P3/database/phreeqc.dat`: **differ** (`cmp`: "differ: byte 405, line 7"; `diff` = 3208 lines). The differences are whitespace (tabs vs spaces) and line reflow in the data tables, e.g. RM line 7 `#element	species	alk	gfw_formula	element_gfw` (tabs) vs P3 line 7 with spaces. `$RM/Tests/phreeqc.dat` matches no file in `$P3/database/`. It is a separate test input, not a copy of the released database.

## U4. Mapping proposal

`build/restructure/mapping_proposal.tsv` — 3840 rows (3334 phreeqc3 + 506 phreeqcrm), columns `repository<TAB>upstream_path<TAB>destination<TAB>reason`. Generator: `build/restructure/make_mapping_part1.py` + `make_mapping_part2.py`. Duplicate engine/database rows carry `DUPLICATE_OF phreeqc3:<path>` in the reason column and inherit the phreeqc3 destination (all duplicates are byte-identical per U3).

Counts per destination:
```
2490 tests/phreeqc/        557 docs/upstream/phreeqc3/   254 src/phreeqc/
170 NOT_CARRIED            140 data/databases/            80 tests/phreeqcrm/
  34 python/                32 src/iphreeqc/              25 src/interfaces/
  20 src/phreeqc_common/    12 AMBIGUOUS                   9 docs/upstream/phreeqcrm/
   6 src/phreeqcrm/          5 src/iphreeqcphast/          4 src/phreeqc/keywords/
   2 src/phreeqc/app/
```
(`src/phreeqc/` 254 = 127 phreeqc3 + 127 PhreeqcRM duplicates; likewise 20 = 10+10 for `src/phreeqc_common/`, 4 = 2+2 keywords, 2 = 1+1 app, 140 = 70+70 databases.)

NOT_CARRIED (170) grouped by reason:
```
28 CI configuration (GitHub/GitLab)          26 build-system file; suite re-derives it (R2)
22 CI of the upstream common repository      21 autotools
15 Visual Studio                             12 Doxygen tooling; docs build decided in R2
 6 VCS/editor metadata                       6 Jenkins
 5 Windows installer (MSI/WiX)               3 legacy hand-written makefile
 2 CI encoding-maintenance script            1 pkg-config template (autotools)
 1 CI encoding-check script
33 PhreeqcRM files whose destination is inherited from the phreeqc3 original
   (reason column: "DUPLICATE_OF phreeqc3:<path>; upstream reason: ...")
```
The 22 "CI of the upstream common repository" rows are `src/common/.github/**`, `src/common/.gitlab-ci.yml`, `src/.github/**`, `src/.gitlab-ci.yml` in phreeqc3 and their phreeqcpp mirrors, per the reviewer clarification.

AMBIGUOUS rows (12), full list:
```
phreeqc3  README.md                                 upstream README vs suite root README
phreeqc3  bin/phreeqc.orig                          shell-script launcher; tool or drop?
phreeqc3  phreeqc_version.h                         version header included by the engine
phreeqc3  src/test.xml                              committed PHAST state file (phast_state XML)
phreeqcrm CMakePresets.json                         design §4 says inherit, not copy
phreeqcrm LICENSE                                   legal file; suite root not in design §2
phreeqcrm README.md                                 upstream README vs suite root README
phreeqcrm environment.yml                           conda env file; no destination in design §2
phreeqcrm mamba.environment.yml                     conda env file; no destination in design §2
phreeqcrm uv.lock                                   uv lock file; no destination in design §2
phreeqcrm src/IPhreeqcPhast/IPhreeqc/phreeqcpp/test.xml   DUPLICATE_OF phreeqc3:src/test.xml
phreeqcrm src/phreeqcrm/__init__.py                 python package __init__ under src/; belongs to python/?
```

## U5. Include paths

The two greps from the task spec, verbatim, produced `build/restructure/includes_with_slash.txt` (7 lines) and `includes_dotdot.txt` (2 lines). Every hit:
```
$P3/src/Phreeqc.h:1607                                    #include "../../phreeqci_gui.h"
$RM/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/Phreeqc.h:1607  #include "../../phreeqci_gui.h"
$P3/src/Phreeqc.h:12 (via PHREEQCI_GUI block, lines 8-14) #include "../../resource.h"
$RM/.../phreeqcpp/Phreeqc.h:12                           #include "../../resource.h"
$RM/src/PhreeqcRM.cpp:38, YAMLPhreeqcRM.h:18, YAMLPhreeqcRM.cpp:2,
        BMIPhreeqcRM.cpp:13, BMI_methods.cpp:15          #include "yaml-cpp/yaml.h"   (external library)
```
Both `../..` includes are inside `PHREEQCI_GUI` guards, e.g. `$P3/src/Phreeqc.h:1606-1608`:
```
#if defined(PHREEQCI_GUI)
#include "../../phreeqci_gui.h"
#endif /* defined(PHREEQCI_GUI) */
```
and `Phreeqc.h:11-13`:
```
#  if defined(PHREEQCI_GUI)
#    include "../../resource.h"
#  endif
```
`phreeqci_gui.h` and `resource.h` (as `../../` targets) exist in neither tree (`find` over both trees: no results) — dead includes of the retired PhreeqcI GUI.

Cross-layer analysis (script `build/restructure/cross_layer.py`; list `build/restructure/cross_layer_includes.txt`). Layers used: `phreeqcpp`, `IPhreeqc`, `IPhreeqcPhast`, `phreeqcrm` (RM `src/` top level). Scanned 797 quoted includes in `$RM/src`: 708 same-layer, **63 cross-layer**, **0 AMBIGUOUS** (no included name resolves to more than one file under `$RM/src`), 26 not found under `$RM/src` (external or dead: `yaml-cpp/yaml.h` ×5, `mpi.h` ×5, `irm_dll_export.h` ×9 (generated at configure time), `CSelectedOutputhxx.h`, `../../resource.h`, `../../phreeqci_gui.h`, `BasicCallback.h`, `string.h`, `float.h`).

Cross-layer counts per (from layer, to layer):
```
27  phreeqcrm      -> phreeqcpp
11  IPhreeqc       -> phreeqcpp
10  phreeqcrm      -> IPhreeqcPhast
 5  IPhreeqcPhast  -> phreeqcpp
 5  IPhreeqcPhast  -> IPhreeqc
 4  phreeqcrm      -> IPhreeqc
 1  phreeqcpp      -> IPhreeqc   (Phreeqc.h:51 #include "thread.h", inside #ifdef SWIG_SHARED_OBJ)
```
Every cross-layer include is an unqualified file name resolved through `-I` directories (see U8), so none needs an include-text change if the flat build supplies equivalent include directories. The only includes with path components that cross layers are the two dead `../..` includes above.

## U6. Conditional compilation in headers

Script `build/restructure/header_macros.py`; full list (macro, count, files) in `build/restructure/header_macros.txt`, machine-readable copy `header_macros.json`. **149 distinct macros** are tested in `*.h|hxx|hpp` across both trees. Most are include guards. Where the interesting ones are defined:

| Macro | Defined in |
|---|---|
| USE_MPI | `$RM/CMakeLists.txt:474` `target_compile_definitions(PhreeqcRM PUBLIC USE_MPI)` — only when MPI on |
| USE_OPENMP | `$RM/CMakeLists.txt:481` (PRIVATE) — only when OpenMP on |
| USE_PHRQ_ALLOC | `$RM/CMakeLists.txt:347` (PRIVATE) — always in RM |
| SWIG_SHARED_OBJ | `$RM/CMakeLists.txt:346` (PRIVATE) — always in RM |
| IPhreeqc_EXPORTS | `$RM/CMakeLists.txt:348` (PRIVATE) — always in RM |
| USE_YAML | `$RM/CMakeLists.txt:374,386` (PUBLIC, conditional) — NOT set in either configured tree (yaml-cpp not found) |
| NO_UTF8_ENCODING | `$RM/CMakeLists.txt:351` — option OFF, not set |
| USE_GZ | `$RM/CMakeLists.txt:406,412` — not set in either configured tree |
| NDEBUG | not in the three CMakeLists files; comes from `-DCMAKE_BUILD_TYPE=Release` (`CMAKE_CXX_FLAGS_RELEASE:STRING=-O3 -DNDEBUG` in both CMakeCache.txt) |
| MULTICHART | `$P3/CMakeLists.txt:241,251` — only in MSVC `/clr` flag strings; not set in the GCC build |
| PHREEQCI_GUI | nowhere (no CMake, no header define) |
| PHREEQC_PARALLEL | nowhere; tested only at `$RM/src/IPhreeqcPhast/IPhreeqc/thread.h:52` |
| FC_FUNC | nowhere in the trees; supplied by Fortran name-mangling convention; used at `fwrap.h:11-14` |
| NO_NAMELESS_UNION, R_SO | nowhere |
| USE_LONG_DOUBLE | nowhere; commented out at `common/phrqtype.h:10` `/*#define USE_LONG_DOUBLE*/` |
| USE_DECIMAL128 | nowhere |
| SUNDIALS_FLOAT | hardcoded `#define SUNDIALS_FLOAT 0` at `sundialstypes.h:132` (both trees) |
| IPHREEQC_NO_FORTRAN_MODULE, WITH_PYBIND11, SWIG, SWIG_IPHREEQC, CPPUNIT, SKIP | not in the three CMake files; not set in the configured builds (SWIG_PYTHON/SWIG_IPHREEQC come from SWIG itself) |
| IRM_DLL_* (IRM_DLL_EXPORT, IRM_DLL_STATIC_DEFINE, …) | generated header `irm_dll_export.h` from `$RM/irm_dll_export.h.in` (CMake `generate_export_header`) |
| _MSC_VER, WIN32, WINVER, _WIN64, _WINDLL, DJGPP, _DEBUG | compiler/platform predefined; none on this Linux build |
| include guards (`*_H_INCLUDED`, `_INC_*`, …) | the headers themselves |

Flagged macro (changes a class/struct definition **and** set differently between the two builds) — **USE_MPI**:
`$RM/src/PhreeqcRM.h:5865-5867` adds a member to class `PhreeqcRM`:
```
#ifdef USE_MPI
	MPI_Comm phreeqcrm_comm;                                       // MPI communicator
#endif
```
and `$RM/src/RM_interface_C.h:6-10` changes a typedef used in C API declarations:
```
#ifdef USE_MPI
#include "mpi.h"
#define MP_TYPE MPI_Comm
#else
#define MP_TYPE int
#endif
```
USE_MPI is set in `cc_phreeqcrm_mpi` but not in `cc_phreeqc3` nor `cc_phreeqcrm_openmp` (U8). In the flat layout `PhreeqcRM.h` (→ `src/phreeqcrm/`) must keep receiving exactly the old definition set per configuration.

Near-miss (header content changes, not a class/struct definition; set in RM, unset in phreeqc3) — **SWIG_SHARED_OBJ**: `$P3/src/Phreeqc.h:50-52` and its RM duplicate:
```
#ifdef SWIG_SHARED_OBJ
#include "thread.h"
#endif
```
`thread.h:52-58` then *defines* vs *declares* globals (`mutex_t map_lock = MUTEX_INITIALIZER;` vs `extern mutex_t map_lock;`) depending on `PHREEQC_PARALLEL`/`_INC_PHREEQC_H` — an ODR/linkage hazard when the engine is split into a separate library.

Other class-affecting macros that are **not** set differently (unset in both configured builds, so not flagged, but they gate class bodies and must keep their exact per-file effect): PHREEQCI_GUI (`Phreeqc.h:1606-1608`, include inside the class body), MULTICHART (`Phreeqc.h:42-44` includes `ChartHandler.h`; ChartObject/CurveObject class members), NO_NAMELESS_UNION/R_SO (`$RM/src/IPhreeqcPhast/IPhreeqc/Var.h:11` `#if defined(R_SO) || defined(NO_NAMELESS_UNION)` selects named vs anonymous union members), USE_LONG_DOUBLE (`common/phrqtype.h:11`, changes `LDBLE`), USE_PHRQ_ALLOC (`phqalloc.h:4-45`, changes function declarations only, not classes).

## U7. Build-script references to source paths

Files scanned (list `build/restructure/u7_files.txt`, 51 files): `$RM/CMakeLists.txt`, `$RM/Tests/CMakeLists.txt`, `$RM/CMakePresets.json`, `$RM/pyproject.toml`, all 37 tracked files under `$RM/swig/`, the 3 Doxygen configuration files (`Doxygen/CMakeLists.txt`, `Doxyfile`, `Makefile.am`), `$RM/*.cmake.in` (2), the 2 `setup-fortran/*/CMakeLists.txt`, and `$P3/CMakeLists.txt`. A line counts if it contains a path with `/` or a token with a source/config extension. Full list: `build/restructure/build_path_references.txt` (1098 lines).

Counts per file (top):
```
241  $RM/CMakeLists.txt            141  $P3/CMakeLists.txt
 81  $RM/swig/python/test_callbacks.py   81  $RM/Tests/CMakeLists.txt
 71  $RM/Doxygen/Doxyfile          63  $RM/swig/python/PhreeqcRM_docstrings.swg
 63  $RM/swig/python/CMakeLists.txt     62  $RM/swig/python/ex11-advect/phreeqc.dat
 40  $RM/swig/PhreeqcRM.i          37  $RM/swig/python/yamlphreeqcrm.py
 35  $RM/swig/python/YAMLPhreeqcRM.swg  33  ex11-advect.ipynb
 27  swig/python/test_basic.py     20  swig/python/AdvectBmi.py
  5  $RM/CMakePresets.json          2  setup-fortran/*/CMakeLists.txt (2 files)
  1  swig/CMakeLists.txt, swig/numpy.i (6), pyproject.toml (0), PhreeqcRMConfig.cmake.in, phreeqcrm.cmake.in, Doxygen/Makefile.am (0)
```
Representative lines. `$RM/CMakeLists.txt` enumerates every source with an absolute path under the old tree, e.g. lines 141-155:
```
  ${CMAKE_CURRENT_SOURCE_DIR}/src/BMI_interface_C.cpp
  ...
  ${CMAKE_CURRENT_SOURCE_DIR}/src/IPhreeqcPhast/IPhreeqc/CSelectedOutput.cpp
```
and defines the include dirs and per-file definitions discussed in U6/U8 (lines 346-351, 472-481). `$P3/CMakeLists.txt` lists sources relative to its root, e.g. lines 14-25:
```
src/ChartHandler.cpp
src/ChartObject.cpp
...
```
`$RM/Tests/CMakeLists.txt` references `../src` headers and `Tests/Output/*` baselines; `swig/CMakeLists.txt` and `swig/python/CMakeLists.txt` reference `../` sources and the generated module; `Doxygen/Doxyfile` names `INPUT = ../src` style paths (71 matching lines).

## U8. Compile commands (configure only)

The three configure commands from the task spec ran successfully (logs `cfg_phreeqc3.log`, `cfg_rm_openmp.log`, `cfg_rm_mpi.log`; trees `build/restructure/cc_phreeqc3`, `cc_phreeqcrm_openmp`, `cc_phreeqcrm_mpi`). Per-file tables: `build/restructure/compile_flags_<tree>.tsv`; summary `compile_flags_summary.txt`.

**cc_phreeqc3 — 81 entries.** One `-D` set (81 files): `-DNDEBUG`. One `-I` set (81): `<ROOT>/src <ROOT>/src/PhreeqcKeywords <ROOT>/src/common`. Other flags: `-O3` (81). Uncompiled tracked `src/` files (82, list `p3_not_compiled.txt`): all headers, non-source files, and `src/cl1mp.cpp` (the MPI variant, excluded from the serial build).

**cc_phreeqcrm_openmp — 110 entries.** Two `-D` sets:
```
[95] -DIPhreeqc_EXPORTS -DNDEBUG -DSWIG_SHARED_OBJ -DUSE_OPENMP -DUSE_PHRQ_ALLOC   (all library sources)
[15] -DNDEBUG                                                                       (all 15 Tests/*.c|cpp)
```
One `-I` set (110): `<BUILD_DIR> <ROOT>/src <ROOT>/src/IPhreeqcPhast <ROOT>/src/IPhreeqcPhast/IPhreeqc <ROOT>/src/IPhreeqcPhast/IPhreeqc/phreeqcpp <ROOT>/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/PhreeqcKeywords <ROOT>/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/common`. Other flags: `-O3` (110), `-fopenmp` (104; the 6 without it are the C files `Var.c` and the five `Tests/*_c.c`). Uncompiled tracked sources include `fwrap*.cpp`, `pp_sys.cpp`, `class_main.cpp`, `ChartHandler/ChartObject/CurveObject.cpp`, `cl1mp.cpp`, `BMI_methods.cpp`, and the `.F90` interface files.

**cc_phreeqcrm_mpi — 110 entries.** Two `-D` sets:
```
[95] -DIPhreeqc_EXPORTS -DNDEBUG -DSWIG_SHARED_OBJ -DUSE_MPI -DUSE_PHRQ_ALLOC
[15] -DNDEBUG -DUSE_MPI   (the same 15 Tests files)
```
Same single `-I` set as openmp. Other flags: `-O3` (110), `-isystem /usr/lib/x86_64-linux-gnu/openmpi/include` and `.../include/openmpi` (110 each).

Notes: `USE_YAML`, `USE_GZ`, `NO_UTF8_ENCODING` appear in `$RM/CMakeLists.txt` but are **not** in any configured tree's flags (options off / dependency absent), contradicting design §5's list of always-on definitions. The phreeqc3 build compiles the engine with **no** `-D` beyond `NDEBUG`, while the same engine files in RM get `USE_OPENMP`/`USE_MPI` + `USE_PHRQ_ALLOC` + `SWIG_SHARED_OBJ` + `IPhreeqc_EXPORTS` — the ODR risk of design §5 is real and quantified here.

## U9. Tests

`ctest -N` results:
- `$RM/build-openmp` (exists): 3 tests — `TestRMdtor`, `TestBMIdtor`, `TestRM`.
- `$RM/build-mpi` (exists): 4 tests — `TestRMdtor`, `TestBMIdtor`, `TestRM`, `TestMPI`.
- `~/phreeqc3/build/baseline` (exists): **521 tests** — 489 named `mytest.*`, 32 named `examples.*`.

Test inputs (lists `build/restructure/test_inputs_phreeqcrm.txt`, `test_inputs_phreeqc3_examples.txt`):
- `$RM/Tests/`: 88 tracked files — C/C++/Fortran test programs (`Advect*`, `Gas*`, `SimpleAdvect*`, `Species*`, `TestAllMethods*`, `TestRMdtor`, `TestBMIdtor`, `WriteYAMLFile*`, `AdvectBMI*`, `main.cpp`), inputs (`advect.pqi`, `gas.pqi`, `units.pqi`, `all_reactants.pqi`, `phreeqc.dat`), expected outputs under `Tests/Output/` (`.chem.txt`, `.dmp`, `.yaml`, `_utility.txt`), plus `CMakeLists.txt`, autotools makefiles and `vs2012/` projects.
- `$P3/examples/`: 112 tracked files — PHREEQC input/baseline pairs (`ex1`…`ex37`, `co2.dat`, `Zn1e_4`, `.tsv` data, `.out`/`.sel` baselines), plus CI files, `CMakeLists.txt` and autotools makefiles. (The 2348-file `mytest/` tree is the source of the 489 `mytest.*` baseline tests; mapped to `tests/phreeqc/` in U4.)

Data files produced by this audit, all under `~/phreeqc3/build/restructure/`: `inventory_phreeqc3.txt`, `inventory_phreeqcrm.txt`, `dup_identical.txt`, `dup_differ.txt`, `dup_only_rm.txt`, `dup_only_p3.txt`, `db_identical.txt`, `db_differ.txt`, `db_only_rm.txt`, `db_only_p3.txt`, `mapping_proposal.tsv`, `make_mapping_part1.py`, `make_mapping_part2.py`, `includes_with_slash.txt`, `includes_dotdot.txt`, `cross_layer_includes.txt`, `cross_layer.py`, `header_macros.txt`, `header_macros.json`, `header_macros.py`, `build_path_references.txt`, `u7_files.txt`, `compile_flags_phreeqc3.tsv`, `compile_flags_phreeqcrm_openmp.tsv`, `compile_flags_phreeqcrm_mpi.tsv`, `compile_flags_summary.txt`, `parse_flags.py`, `p3_not_compiled.txt`, `test_inputs_phreeqcrm.txt`, `test_inputs_phreeqc3_examples.txt`, `cfg_*.log`, and the configure-only trees `cc_phreeqc3/`, `cc_phreeqcrm_openmp/`, `cc_phreeqcrm_mpi/`.

## Reviewer decisions required

1. **AMBIGUOUS mappings (12)** — full list in U4. The ones that block R1: `README.md` (both trees), `LICENSE`, `phreeqc_version.h`, `CMakePresets.json`, `environment.yml`/`mamba.environment.yml`/`uv.lock`, `bin/phreeqc.orig`, `src/test.xml` (+ its phreeqcpp duplicate), `src/phreeqcrm/__init__.py`.
2. **PHREEQC files differing between the trees (U3)**: none — all 163 phreeqcpp files and all 70 database files are byte-identical. Decision needed on `$RM/Tests/phreeqc.dat`: it differs from `data/databases/phreeqc.dat` (whitespace/reflow, 3208 diff lines); keep it as a separate test input under `tests/phreeqcrm/` as proposed, or reconcile it with the database copy?
3. **Cross-layer includes needing a path change (U5)**: none of the 63 cross-layer includes needs an include-text change (all unqualified names resolved via `-I`); the new build must provide include directories equivalent to the seven in U8. The only path-bearing includes are the dead `../../phreeqci_gui.h` and `../../resource.h` (PHREEQCI_GUI-guarded, files absent) — decision: delete them or keep verbatim under the "no content changes" rule.
4. **Macros flagged (U6)**: `USE_MPI` — changes class `PhreeqcRM` (member `phreeqcrm_comm`, `PhreeqcRM.h:5865-5867`) and `MP_TYPE` (`RM_interface_C.h:6-10`); set only in the MPI build. `SWIG_SHARED_OBJ` — near-miss: gates `#include "thread.h"` from `Phreeqc.h:50-52`, whose globals are defined vs declared depending on `PHREEQC_PARALLEL` (defined nowhere); set in RM builds, unset in phreeqc3. Decision: how the split libraries receive per-file definitions so each file keeps its exact U8 definition set.
5. **Contradiction with design §5 (U8)**: `USE_YAML`, `USE_GZ`, `NO_UTF8_ENCODING` are not actually set in the configured PhreeqcRM builds; the design's "always PRIVATE" list is inaccurate. Confirm the authoritative definition sets are the ones measured in U8.
6. **NOT FOUND items**: the official USGS phreeqc3 remote (`usgs-coupled/phreeqc3`) is not configured — the only remote is `https://github.com/phreeqc-dev/phreeqc3.git`; provenance of `b0b3be76` relative to USGS cannot be verified locally. `phreeqci_gui.h` and `../../resource.h` are not present in either tree. `FC_FUNC`, `PHREEQC_PARALLEL`, `PHREEQCI_GUI`, `NO_NAMELESS_UNION`, `USE_LONG_DOUBLE`, `USE_DECIMAL128` are defined nowhere in the trees or the three CMake files.
7. **U4 judgment calls to confirm**: `mytest/` (2348 files) → `tests/phreeqc/`; `test/` (35) → `tests/phreeqc/`; `HTMLversion/` + `doc/` → `docs/upstream/phreeqc3/`; Doxygen tooling (12 files) → NOT_CARRIED; `$RM/CMakeLists.txt` and other build files → NOT_CARRIED ("suite re-derives them in R2"); `src/.github/**` and `src/common/.github/**` → NOT_CARRIED as "CI of the upstream common repository" per the clarification.
