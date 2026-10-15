# Task 0: Source Verification Report

Status: Part A complete. Part B: B1-B15 complete (B9-B15 added in follow-up sessions on 2026-10-11; B13-B15 defined by build/task0_audit_items.txt, item 0 of that file verified B11 covers every B3 member). Part C complete (C1, C2 including the determinism check, C3 — every claim independently re-verified in a follow-up session on 2026-10-11: input diffs, sha256 hashes, determinism diffs, iteration-count extraction, README contents, and all source citations re-checked against the tree). Stop conditions: **S3 YES and S4 YES — STOP** (S1 NO, S2 NO). Generated 2026-10-11. All claims verified against the working tree at commit `5132d269f5ea43ec69603c7757864f70d29163dc`. Verification corrections: three citations of the pre-existing ex2 `SELECTED_OUTPUT` block read `examples/ex2:11-13`; the block actually spans `examples/ex2:11-14` (`-si anhydrite  gypsum` at line 14) — corrected in this report and in the baseline README.

# Part A: Environment

## A1. Tree layout

- **phreeqc executable: PRESENT.** Defined by the only target in the top-level `CMakeLists.txt:260`:
  ```
  add_executable(phreeqc ${phreeqc_SOURCES})
  ```
  Core sources live in `src/` (source list `CMakeLists.txt:13-148`).
- **IPhreeqc: NOT FOUND.** Looked for: `src/ipreeqc/` (does not exist), `find . -maxdepth 2 -type d -iname "*ipreeqc*"` (no match), `grep -rn "ipreeqc" CMakeLists.txt configure.ac Makefile.am` (no match), `grep -rn "add_library" --include=CMakeLists.txt .` (no match anywhere).
- **PhreeqcRM: NOT FOUND.** Same searches as above with `phreeqcrm`; no match. Autotools subdirs confirm the scope: `Makefile.am:1` `SUBDIRS = database doc examples src test`.
- **`git status --short`:** empty output (working tree clean).
- **`git rev-parse HEAD`:** `5132d269f5ea43ec69603c7757864f70d29163dc`.

## A2. Build system

- **CMake project(s):** exactly one, `CMakeLists.txt:3`:
  ```
  project(phreeqc)
  ```
  Subdirectories added: `database`, `doc`, `examples`, optional `mytest` (`CMakeLists.txt:315-320`); none defines a target that compiles core sources (`grep -rn "add_library\|add_executable" --include=CMakeLists.txt .` returns only `./CMakeLists.txt:260`).
- **Targets that compile PHREEQC core sources** (the file defining `Phreeqc::ineq` is `src/model.cpp:927-928`, see B3; `src/model.cpp` is in `phreeqc_SOURCES` at `CMakeLists.txt:140`): exactly one target, **`phreeqc`**.
- **phreeqc executable target:** `phreeqc` (`CMakeLists.txt:260`). Core sources are **compiled directly into the executable** — no static or shared library target exists (`add_executable(phreeqc ${phreeqc_SOURCES})`, all of `phreeqc_SOURCES` passed to it).
- **IPhreeqc / PhreeqcRM targets:** NOT FOUND (see A1).
- **`target_link_libraries` form:** the only such call for `phreeqc` is `CMakeLists.txt:300`:
  ```
      target_link_libraries(phreeqc ${GMP_LIBRARY})
  ```
  This is the **plain (no-keyword) form**, and it is conditional on `PHRQC_USE_GMP` being ON and `GMP_LIBRARY` being found (`CMakeLists.txt:294-302`). No keyword-form calls exist for these targets.

## A3. PETSc

Two PETSc installations are visible in this environment. **Choosing between them is a decision for the reviewer.**

### A3a. Environment variables as currently set

- `PETSC_DIR=/home/chen/petsc`, `PETSC_ARCH=arch-linux-c-opt` (from `echo $PETSC_DIR $PETSC_ARCH` in this shell).
- **`/home/chen/petsc` does not exist** (`ls: cannot access '/home/chen/petsc': No such file or directory`). Therefore `petscversion.h`, `petscconf.h`, and any pkg-config file there: **NOT FOUND**. Looked in: `/home/chen/petsc`, `/home/chen/petsc/include`, `/home/chen/petsc/arch-linux-c-opt/include`. The environment variables point at a nonexistent installation.

### A3b. Module `petsc/3.26.0` (installation `/opt/petsc/3.26.0`)

- Module name: `petsc/3.26.0`, module file `/etc/lmod/modules/petsc/3.26.0.lua`. `module show petsc/3.26.0` sets:
  ```
  setenv("PETSC_DIR","/opt/petsc/3.26.0")
  setenv("PETSC_ARCH","")
  prepend_path("LD_LIBRARY_PATH","/opt/petsc/3.26.0/lib")
  prepend_path("LIBRARY_PATH","/opt/petsc/3.26.0/lib")
  prepend_path("CPATH","/opt/petsc/3.26.0/include")
  prepend_path("CPLUS_INCLUDE_PATH","/opt/petsc/3.26.0/include")
  prepend_path("PKG_CONFIG_PATH","/opt/petsc/3.26.0/lib/pkgconfig")
  prepend_path("PATH","/opt/petsc/3.26.0/bin")
  ```
  (Module help text: "PETSc 3.26.0 (Serial, Double Precision, Real Scalars). Installed at: /opt/petsc/3.26.0".)
- **Version** (`/opt/petsc/3.26.0/include/petscversion.h:8-10`):
  ```
  #define PETSC_VERSION_MAJOR      3
  #define PETSC_VERSION_MINOR      26
  #define PETSC_VERSION_SUBMINOR   0
  ```
  → 3.26.0.
- **`petscconf.h` answers** (`/opt/petsc/3.26.0/include/petscconf.h`):
  | Macro | Defined? | Evidence |
  |---|---|---|
  | `PETSC_USE_COMPLEX` | NO | no `#define PETSC_USE_COMPLEX` in petscconf.h (`grep -nE "define PETSC_USE_COMPLEX"` no match) |
  | `PETSC_USE_REAL_DOUBLE` | YES | line 171: `#define PETSC_USE_REAL_DOUBLE 1` |
  | `PETSC_HAVE_MPIUNI` | YES | line 75: `#define PETSC_HAVE_MPIUNI 1` |
  | `PETSC_USE_64BIT_INDICES` | NO | no match for `define PETSC_USE_64BIT_INDICES` |
  | `PETSC_USE_DEBUG` | NO | no match for `define PETSC_USE_DEBUG$` (only `PETSC_USE_DEBUGGER "gdb"` at line 163, a different macro) |
- **pkg-config files:** both exist: `/opt/petsc/3.26.0/lib/pkgconfig/PETSc.pc` and `/opt/petsc/3.26.0/lib/pkgconfig/petsc.pc` (both listed by `ls`; `petsc.pc:18` `Version: 3.26.0`).

## A4. Toolchain

- **C++ compiler:** default `c++` / `g++` is GCC: `c++ --version` → `c++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0`. Binaries: `/usr/bin/g++`, `/usr/bin/g++-15`, `/usr/bin/x86_64-linux-gnu-g++`.
- **C++ standard used by the project:** C++11, requested at `CMakeLists.txt:263`:
  ```
  target_compile_features(phreeqc PUBLIC cxx_std_11)
  ```

# Part B: Source facts

## B1. Augmented Newton matrix

- **Upstream identifier: `my_array`. CONFIRMED.** Declared in `class Phreeqc` (`src/Phreeqc.h:74` `class Phreeqc`, `protected:` at `src/Phreeqc.h:1494`), at `src/Phreeqc.h:1520`:
  ```
  std::vector<double> my_array, delta, residual;
  ```
- **Allocation:** `src/prep.cpp:89-91`:
  ```
  my_array.resize((max_unknowns + 1) * max_unknowns);
  delta.resize(max_unknowns);
  residual.resize(max_unknowns);
  ```
- **Layout: row-major, leading dimension `count_unknowns + 1`. CONFIRMED.** Every access has the form `my_array[(size_t)row * (count_unknowns + 1) + col]`, e.g. the row-copy in `jacobian_sums()` (`src/model.cpp:1927`), at `src/model.cpp:1943-1946`:
  ```
  for (i = 1; i < count_unknowns; i++)
  {
      memcpy((void *) &(my_array[(size_t)i * (count_unknowns + 1)]),
             (void *) &(my_array[0]), (size_t) count_unknowns * sizeof(LDBLE));
  }
  ```
- **Residuals occupy column `count_unknowns` (augmented column).** `src/model.cpp:4531-4534`:
  ```
  /*
   *   Store residuals in array
   */
      my_array[((size_t)i + 1) * (count_unknowns + 1) - 1] = residual[i];
  ```
  (`((i+1)*(count_unknowns+1))-1` equals `i*(count_unknowns+1) + count_unknowns`.)
- **Jacobian entries are accumulated into `my_array` through `store_jacob0`/`store_jacob1`**, e.g. `src/prep.cpp:697-698`:
  ```
  store_jacob0((int)x[i]->number, (int)master_ptr->unknown->number,
      rxn_ptr->coef);
  ```
  constant terms are added in `jacobian_sums()` at `src/model.cpp:1951-1953`:
  ```
  for (k = 0; k < (int)sum_jacob0.size(); k++)
  {
      *sum_jacob0[k].target += sum_jacob0[k].coef;
  }
  ```
- **0-based indexing. CONFIRMED.** Unknown numbers are assigned 0-based, `src/prep.cpp:4443` `ph_unknown->number = count_unknowns;` (before `count_unknowns++`); all loops use `for (i = 0; i < count_unknowns; i++)`.

## B2. `delta`

- **Declared** `src/Phreeqc.h:1520` (same declaration line as `my_array`); **allocated** `src/prep.cpp:90` `delta.resize(max_unknowns);`.
- **Written by `ineq()`** from the `cl1()` solution vector `delta1`, undoing column scaling, `src/model.cpp:1851-1854`:
  ```
  memcpy((void *) &(delta[0]), (void *) &(delta1[0]),
         (size_t) n * sizeof(LDBLE));
  for (i = 0; i < n; i++)
      delta[i] *= normal[i];
  ```
  plus the MB special case at `src/model.cpp:1879-1881` (see B3) and the PP early-return path at `src/model.cpp:991`/`995`.
- **Read by `reset()`**, which applies it to unknowns, e.g. `src/model.cpp:3343` `d = delta[i] / LOG_10;` then `src/model.cpp:3400` `x[i]->master[0]->s->la += d;`; and `src/model.cpp:3674` `x[i]->moles += delta[i];`.
- **0-based indexing. CONFIRMED** (same `for (i = 0; ...)` loops; `ineq()` fills `delta[i]` for `i` in `[0, n)`).

## B3. `ineq()`

- **Declaration** `src/Phreeqc.h:449`: `int ineq(int kode);`. **Definition** `src/model.cpp:927-928`:
  ```
  int Phreeqc::
  ineq(int in_kode)
  ```
  Body spans `src/model.cpp:928`-`src/model.cpp:1922` (`return (return_code);`).
- **No recursion. CONFIRMED.** `grep -rn "\bineq(" src/` (excluding `ineq_array`/`ineq_tol`) matches only the declaration, the definition, and these call sites: `src/model.cpp:157` `return_kode = ineq(l_kode);`, `src/model.cpp:174` `ineq(0);`, `src/pitzer.cpp:2306`, `src/pitzer.cpp:2323`, `src/sit.cpp:1295`, `src/sit.cpp:1312`. None is inside the body of `ineq()`.
- **Return values** use `src/global_structures.h:36-37`:
  ```
  #define OK 1
  #define ERROR 0
  ```
  Early success path `return (OK);` at `src/model.cpp:1005`. `cl1()`'s `l_kode` is mapped at `src/model.cpp:1783-1800`: `l_kode==1` → `return_code = ERROR`; `l_kode==2` → `return_code = 2`; `l_kode==3` → `ERROR` then `2` with `warning_msg("Too many iterations in Cl1. Should not have done this.")`; else `return_code = OK`. Final `return (return_code);` at `src/model.cpp:1922`.
- **Members written besides `delta`:** `my_array` (zeroing `src/model.cpp:1021`/`1025`, MH diagonal `src/model.cpp:1082`, column scaling `src/model.cpp:1116`, rescaling `src/model.cpp:1864`), `normal` (`src/model.cpp:1033`, `1118`, `1686`), `ineq_array` (`resize` `src/model.cpp:1127`; row copies `src/model.cpp:1176`, `1211`, `1322`), `back_eq` (`src/model.cpp:1128`, `1179`, `1214`, `1325`), `zero` (`src/model.cpp:1129-1130`), `res` (`src/model.cpp:1131`, `1183`, `1201`), `delta1` (`src/model.cpp:1133`; `cl1()` output argument `src/model.cpp:1780`), `cu`/`iu`/`is` (`src/model.cpp:1751-1753`), `x[i]->iteration` (`src/model.cpp:1157`), `remove_unstable_phases` (`src/model.cpp:1004`), `max_row_count`/`max_column_count` (`src/model.cpp:1125-1126`).
- **Caller reads `remove_unstable_phases`:** `src/model.cpp:105-106`:
  ```
  while ((r = residuals()) != CONVERGED
             || remove_unstable_phases == TRUE)
  ```
  (`remove_unstable_phases = FALSE` at `src/model.cpp:99`; set TRUE at `src/model.cpp:424`.)
- **`x[i]->iteration` is read back inside `ineq()`** at `src/model.cpp:1165`: `&& iterations >= x[i]->iteration + equi_delay`.
- **`ineq()` never reads `delta` before writing it. CONFIRMED.** In the body, `delta[` appears only at: writes `src/model.cpp:991`/`995` (PP early-return path), debug read `src/model.cpp:1000` (after that write), `memset` `src/model.cpp:1848`, main-path write `src/model.cpp:1851-1854`, and the MB adjustment `src/model.cpp:1879-1881` (after the write).
- **`ineq()` modifies `my_array` in place** (column scaling `src/model.cpp:1116`, rescaling `src/model.cpp:1864`), so the caller's Jacobian is not preserved unchanged after `ineq()` returns. CONFIRMED.

## B4. Unknown-type constants

- Defined at `src/global_structures.h:82-99`:
  ```
  #define MB 10
  #define ALK 11
  #define CB 12
  #define SOLUTION_PHASE_BOUNDARY 13
  #define MU 14
  #define AH2O 15
  #define MH 16
  #define MH2O 17
  #define PP 18
  #define EXCH 19
  #define SURFACE 20
  #define SURFACE_CB 21
  #define SURFACE_CB1 22
  #define SURFACE_CB2 23
  #define GAS_MOLES 24
  #define SS_MOLES 25
  #define PITZER_GAMMA 26
  #define SLACK 28
  ```
  Value 27 is unused.
- **Type is read via `x[i]->type`.** `x` declared `src/Phreeqc.h:1423`: `std::vector<class unknown*> x;`. `class unknown` at `src/global_structures.h:1022`; field `int type;` at `src/global_structures.h:1068`.
- **Type assignment examples** (`src/prep.cpp`): `4441` `ph_unknown->type = CB;`, `4454` `mass_hydrogen_unknown->type = MH;`, `4470` `mass_oxygen_unknown->type = MH2O;`.

## B5. Equality status of each type inside `ineq()`

- **Mechanism.** `ineq()` copies rows of `my_array` into `ineq_array` in three stages:
  1. "Optimization" rows (become `k` rows in A): loop `src/model.cpp:1139-1252`, count at `src/model.cpp:1252` `l_count_optimize = l_count_rows;`.
  2. Equality rows (become `l` rows in C): loop `src/model.cpp:1256-1346`, count at `src/model.cpp:1346` `count_equal = l_count_rows - l_count_optimize;`.
  3. Inequality rows (become `m` rows in E): from `src/model.cpp:1350` `if (pure_phase_unknown != NULL)` (`pure_phase_unknown = NULL` at `src/prep.cpp:1770`).
  Split at `src/model.cpp:1699-1701`:
  ```
  k = l_count_optimize;			/* rows in A */
  l = count_equal;			/* rows in C */
  m = l_count_rows - l - k;		/* rows in E */
  ```
  `cl1()` treats rows `[k, k+l)` as equalities: `src/cl1.cpp:782-784`:
  ```
  for (i = k; i < k + l; ++i)
  {
      if (fabs(l_res[i]) > check_toler)
  ```
- **MB (10):** no branch in the first loop → not an optimization row. Passes the second-loop filter (`src/model.cpp:1275-1280` excludes only SOLUTION_PHASE_BOUNDARY, ALK, GAS_MOLES, SS_MOLES, PITZER_GAMMA) → **equality row**. Column included in the normalization scan at `src/model.cpp:1040`. Failed-row special case at `src/model.cpp:1879-1881`: `if (x[j]->type == MB && delta[j] == 0.0 && fabs(res[i]) > ineq_tol)` → `delta[j] = res[i]/fabs(res[i]) * 1;`.
- **ALK (11):** **optimization row** via `else if (x[i]->type == ALK || x[i]->type == SOLUTION_PHASE_BOUNDARY)` at `src/model.cpp:1207` (row copy `1211-1213`, `l_count_rows++` `1215`). The whole first loop is gated by `src/model.cpp:1141` `if (iterations < aqueous_only) continue;`; `aqueous_only` defaults to 0 (`src/Phreeqc.cpp:869`) and is set to 5 only at `src/kinetics.cpp:1327`. Excluded from the equality block by the filter at `src/model.cpp:1276`.
- **CB (12), MU (14), AH2O (15):** **equality rows** (pass the second-loop filter). AH2O row and column are zeroed at `src/model.cpp:1010-1027` when `(pitzer_model || sit_model) && full_pitzer == FALSE` (condition `src/model.cpp:1014`; zeroing `src/model.cpp:1021`/`1025`).
- **MH (16):** **equality row**, except skipped when `pitzer_model == TRUE && pitzer_pe == FALSE` (`src/model.cpp:1284-1285`; row/column also zeroed via `src/model.cpp:1015`). Row modified when `mass_water_switch == TRUE && x[i] == mass_hydrogen_unknown` at `src/model.cpp:1326-1334` (subtracts 2× the `mass_oxygen_unknown` row). Diagonal bump at `src/model.cpp:1076-1085`.
- **MH2O (17):** **equality row**, except skipped when `mass_water_switch == TRUE && x[i] == mass_oxygen_unknown` (`src/model.cpp:1287-1288`); in that case its column is zeroed in `ineq_array` at `src/model.cpp:1665-1673` (`ineq_array[(size_t)j * max_column_count + (size_t)k] = 0;` at `1670`).
- **Systems whose unknowns are only B5 types (MB, ALK, CB, MU, AH2O, MH, MH2O):** one row per unknown minus the conditional exclusions above (MH under pitzer-without-pe; MH2O under `mass_water_switch`); `m = 0` because the inequality block requires `pure_phase_unknown != NULL` (`src/model.cpp:1350`); `n = count_unknowns` (`src/model.cpp:1729`; the `SHRINK_ARRAY` reduction at `src/model.cpp:1711` requires `sit_model || pitzer_model`). No extra rows or columns beyond the fixed augmented layout. **CONFIRMED for this subset.**

## B6. Residual sign convention and Newton relation

- `residuals()` (`src/model.cpp:3773`) defines the residual as target minus current function value, e.g. `src/model.cpp:3802-3804`:
  ```
  if (x[i]->type == MB)
  {
      residual[i] = x[i]->moles - x[i]->f;
  ```
- The augmented matrix's RHS column is filled with `residual[i]` at `src/model.cpp:4534` (see B1).
- Jacobian entries are the (positive) derivatives of `f`: `store_jacob0` is called with reaction coefficients, e.g. `src/prep.cpp:697-698` `store_jacob0((int)x[i]->number, (int)master_ptr->unknown->number, rxn_ptr->coef);`.
- `cl1()` solves the assembled system; the solution is extracted at `src/cl1.cpp:722` `l_x[ii - 1] = sn * q2[i * q_dim + n].dval;`, returned through `delta1` (`cl1()` call at `src/model.cpp:1779-1781`), and copied into `delta` at `src/model.cpp:1851`.
- `reset()` applies `delta` in the direction that increases `f` toward `moles`: `src/model.cpp:3343` `d = delta[i] / LOG_10;` then `src/model.cpp:3400` `x[i]->master[0]->s->la += d;`; and `src/model.cpp:3674` `x[i]->moles += delta[i];`.
- **Newton relation: `J·δ = r`, with `r = residual` (target − current) and `J` the Jacobian of `f`. CONFIRMED** from the assembly code (J from ∂f, RHS column = `moles − f`) and the application direction in `reset()`.

## B7. `LDBLE`

- `src/common/phrqtype.h:10-16`:
  ```
  /*#define USE_LONG_DOUBLE*/
  #ifdef USE_LONG_DOUBLE
  #define LDBLE long double
  #define SCANFORMAT "%Lf"
  #else
  #define LDBLE double
  #define SCANFORMAT "%lf"
  ```
- `USE_LONG_DOUBLE` is commented out at `src/common/phrqtype.h:10` and is defined in no build file (`grep -rn "USE_LONG_DOUBLE" --include=CMakeLists.txt .` → no matches). **Effective type: `double`. CONFIRMED.**

## B8. `diagonal_scale`

- Declared `src/Phreeqc.h:1559`: `int diagonal_scale;	/* 0 not used, 1 used */`.
- Default FALSE at `src/Phreeqc.cpp:863` `diagonal_scale = FALSE;`.
- Read inside `ineq()` at `src/model.cpp:1061` `if (diagonal_scale == TRUE)` — it changes the max-diagonal scan used for column scaling (`src/model.cpp:1105-1118`: `my_array[(size_t)j * (count_unknowns + 1) + (size_t)i] *= min_value / max;` at `1116`, `normal[i] = min_value / max;` at `1118`).
- Set/toggled only in kinetics code: `src/kinetics.cpp:1158` `old_diag = diagonal_scale;`, `src/kinetics.cpp:1196` `diagonal_scale = TRUE;`, toggled at `src/kinetics.cpp:1243-1292`, restored at `src/kinetics.cpp:1424` `diagonal_scale = old_diag;`.
- **The matrix is not pre-scaled before `ineq()`; scaling is applied inside `ineq()` itself. CONFIRMED** (the only writes to `normal` are inside `ineq()` at `src/model.cpp:1033`, `1118`, `1686`).

## B9. `Phreeqc` lifecycle

All lifecycle members are declared in `src/Phreeqc.h:77-81` and defined only in `src/Phreeqc.cpp` (`grep -rn "Phreeqc::Phreeqc\|Phreeqc::~Phreeqc\|Phreeqc::InternalCopy\|Phreeqc::operator=" src/` matches `src/Phreeqc.cpp` only).

- **Default constructor** — declared `src/Phreeqc.h:77` `Phreeqc(PHRQ_io* io = NULL);`, defined `src/Phreeqc.cpp:437`:
  ```
  Phreeqc::Phreeqc(PHRQ_io *io)
  ```
- **Copy constructor** — declared `src/Phreeqc.h:78` `Phreeqc(const Phreeqc& src);`, defined `src/Phreeqc.cpp:1155-1167`:
  ```
  Phreeqc::Phreeqc(const Phreeqc &src)
  {
      user_print = NULL;
      sformatf_buffer = NULL;
      basic_interpreter = NULL;
      count_elts = 0;
      aphi = NULL;
      //this->phrq_io = src.phrq_io;
      this->phrq_io = &this->ioInstance;
      this->init();
      this->initialize();
      InternalCopy(&src);
  }
  ```
- **`InternalCopy`** — declared `src/Phreeqc.h:79` `void InternalCopy(const Phreeqc* pSrc);`, defined `src/Phreeqc.cpp:1168-1169`:
  ```
  void
  Phreeqc::InternalCopy(const Phreeqc* pSrc)
  ```
- **Assignment operator** — declared `src/Phreeqc.h:80` `Phreeqc& operator=(const Phreeqc& rhs);`, defined `src/Phreeqc.cpp:2020-2046`; it self-assign-guards (`2022-2023`), calls `clean_up()` (`2026`) and `PHRQ_free_all()` (`2028`), then `init()`, `initialize()`, `InternalCopy(&rhs)` (`2042-2044`).
- **Destructor** — declared `src/Phreeqc.h:81` `~Phreeqc(void);`, defined `src/Phreeqc.cpp:62-73`:
  ```
  Phreeqc::~Phreeqc(void)
  {

      clean_up();
      
      PHRQ_free_all();
      if (phrq_io == &ioInstance)
      {
          this->phrq_io->clear_istream();
          this->phrq_io->close_ostreams();
      }
  }
  ```
- No other constructors exist: the only constructor declarations in the class are `src/Phreeqc.h:77-78` (the `PHRQ_io*` constructor has a default argument, so `Phreeqc()` also resolves to it).

## B10. `main()`

- **File:** `src/class_main.cpp`, which is in `phreeqc_SOURCES` (`CMakeLists.txt:89` `src/class_main.cpp`) and therefore compiled into the `phreeqc` executable target (`CMakeLists.txt:260`). It is the only definition of `main` in the tree (`grep -rn "main(int argc" src/` matches only `src/class_main.cpp:32`; the other `main` hits are `Phreeqc::main_method`, `src/Phreeqc.h:1079`, defined at `src/class_main.cpp:93` and `src/class_main.cpp:190` under alternative `#ifdef` branches, and stale references in legacy makefiles `src/phreeqc.Makefile`, `src/Makefile.old`).
- **Full body**, `src/class_main.cpp:31-89`, quoted verbatim:
  ```
  int
  main(int argc, char *argv[])
  /*
   *   Main program for PHREEQC
   */
  {

    // check for floating point exceptions on Linux
    // feenableexcept(FE_DIVBYZERO|FE_INVALID|FE_OVERFLOW|FE_UNDERFLOW);
  //#define WIN32_MEMORY_DEBUG
  #if defined(WIN32_MEMORY_DEBUG)
      int tmpDbgFlag;

      /*
       * Set the debug-heap flag to keep freed blocks in the
       * heap's linked list - This will allow us to catch any
       * inadvertent use of freed memory
       */
  #ifdef SKIP
      // Send messages (leaks) to stderr
          _CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
          _CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDERR );
          _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE );
          _CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDERR );
          _CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
          _CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );
  #endif
      tmpDbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
      //tmpDbgFlag |= _CRTDBG_DELAY_FREE_MEM_DF;
      tmpDbgFlag |= _CRTDBG_LEAK_CHECK_DF;
      ///tmpDbgFlag |= _CRTDBG_CHECK_ALWAYS_DF;
      _CrtSetDbgFlag(tmpDbgFlag);
      //_crtBreakAlloc = 31195;
  #endif
  #ifdef SKIP
  //Set the x86 floating-point control word according to what
  //exceptions you want to trap.
  _clearfp(); //Always call _clearfp before setting the control
                //word
  //Because the second parameter in the following call is 0, it
  //only returns the floating-point control word
  unsigned int cw = _controlfp(0, 0); //Get the default control
                                      //word
  //Set the exception masks off for exceptions that you want to
  //trap.  When a mask bit is set, the corresponding floating-point
  //exception is //blocked from being generating.
  cw &=~(EM_OVERFLOW|EM_UNDERFLOW|EM_ZERODIVIDE|
         EM_DENORMAL|EM_INVALID);
  //For any bit in the second parameter (mask) that is 1, the
  //corresponding bit in the first parameter is used to update
  //the control word.
  unsigned int cwOriginal = _controlfp(cw, MCW_EM); //Set it.
                              //MCW_EM is defined in float.h.
                              //Restore the original value when done:
                              //_controlfp(cwOriginal, MCW_EM);
  #endif
      Phreeqc phreeqc_instance;
      return phreeqc_instance.main_method(argc, argv);
  }
  ```
- With the default configuration (`WIN32_MEMORY_DEBUG` undefined, `SKIP` undefined), the only executable code is `Phreeqc phreeqc_instance;` (`src/class_main.cpp:87`) and `return phreeqc_instance.main_method(argc, argv);` (`src/class_main.cpp:88`). `main()` constructs one `Phreeqc` object on the stack and delegates entirely to `Phreeqc::main_method`.

## B11. Readers of members written by `ineq()`

Window definition used below: "after `ineq()` returns and before the next call to `ineq()` or `jacobian_sums()`". A read inside `jacobian_sums()` itself executes *during* the next `jacobian_sums()` call, not before it, so it is marked NO. Control flow basis: the Newton loop in `Phreeqc::model()` (`src/model.cpp:21`, loop at `100-254`) calls, per iteration: `residuals()` (loop condition `src/model.cpp:105`), then `numerical_jacobian()` (`144`) or `jacobian_sums(); numerical_jacobian();` (`148-149`), then `ineq(l_kode)` (`157`, and `ineq(0)` at `174`), then `reset()` (`176`), `gammas()` (`178`), `molalities()` (`179`), `mb_sums()`/`mb_gases()`/`mb_ss()` (`188-190`), optionally `switch_bases()` + `reprep()` (`195-198`); after the inner loop: `check_residuals()` (`230`), reads at `235`/`244`. Alternative drivers: `Phreeqc::model_pz()` (`src/pitzer.cpp:2191`, called from `src/model.cpp:72`) and `Phreeqc::model_sit()` (`src/sit.cpp:1182`, called from `src/model.cpp:79`) use the same order: `jacobian_sums()` (`src/pitzer.cpp:2299`, `src/sit.cpp:1288`) then `jacobian_pz()`/`jacobian_sit()` (`src/pitzer.cpp:2300`, `src/sit.cpp:1289`) then `ineq()` (`src/pitzer.cpp:2306`/`2323`, `src/sit.cpp:1295`/`1312`).

Declarations of the members: `my_array` `src/Phreeqc.h:1520`; `remove_unstable_phases` `src/Phreeqc.h:1587`; `max_row_count`, `max_column_count` `src/Phreeqc.h:1691`; `normal, ineq_array, res, cu, zero, delta1` `src/Phreeqc.h:1744`; `iu, is, back_eq` `src/Phreeqc.h:1745`; `iteration` is a field of `class unknown`, `src/global_structures.h:1105` `int iteration;`.

- **`my_array` — reads outside `ineq()` exist.**
  - Inside `jacobian_sums()` (`src/model.cpp:1927-2102`): value reads (compound assignments, memcpy source, or via stored `sum_jacob0/1/2` target pointers) at `src/model.cpp:1945-1946` (memcpy source), `1952`, `1960`, `1967` (`*target += ...`), `1980`, `1982-1983`, `1990-1991`, `2010`, `2013-2014`, `2019`, `2028`, `2030-2031`, `2034-2035`, `2063`, `2066`, `2070`, `2090`, `2093`. **NO** — these execute during the next `jacobian_sums()` call, not before it.
  - Inside `numerical_jacobian()`: the compiled definition is the second one, `src/model.cpp:5552-5553`; the first (`5311-5312`) is inside `#ifdef ORIGINAL` (`src/model.cpp:5309`) whose define is commented out at `src/model.cpp:5308` `//#define ORIGINAL`, so it is not compiled. Value reads in the compiled body: `src/model.cpp:5715` (`*=`), `5733` (`if (my_array[...] == 0)`). **AMBIGUOUS per call site: YES via `src/model.cpp:144`** (the `numerical_deriv` branch, which runs after the previous `ineq()` returned and before any `jacobian_sums()`/`ineq()` call), **NO via `src/model.cpp:149`** (immediately after `jacobian_sums()` at `148`). (In the non-compiled `ORIGINAL` variant the reads would be `5342-5343`, `5458`, `5472`, `5479`, `5480`, `5504`.)
  - Inside `prep()` (`src/prep.cpp:23-134`): `src/prep.cpp:65` `if (same_model == FALSE || my_array.size() == 0)` — reads the vector's size. **YES** — `prep()` is called at the start of each forward calculation (`src/mainsubs.cpp:398`, `540`, `626`; `src/kinetics.cpp:1576`) and from inside the Newton loop window (`src/model.cpp:2624` inside `calc_gas_pressures`, reached from `molalities()` at `src/model.cpp:2537`, which the loop calls at `src/model.cpp:179` after `ineq()`), all before the next `jacobian_sums()`/`ineq()` call.
  - Inside inverse-modeling code (`src/inverse.cpp`; the inverse problem never calls `ineq()` or `jacobian_sums()` — no such call exists in `src/inverse.cpp`): value reads at `src/inverse.cpp:513` (`/=`), `718-719`, `769`, `947` (`*=`), `970` (all in `setup_inverse`, `137-998`); `2377` (memcpy source, `range`, `2305-2495`); `2820` (memcpy source, `check_solns`, `2767-2940`); `2961`, `2964`, `2970`, `2981`, `2984`, `2989` (`post_mortem`, `2942-3013`); `3038`, `3041`, `3046`, `3059`, `3062`, `3069` (`test_cl1_solution`, `3015-3104`); `3313`, `3338`, `3366` (`+=`, `isotope_balance_equation`, `3247-3398`); `4243` (`array_save = my_array;`, full-vector read, `dump_netpath_pat`, `4215-5017`). **YES, possible** — in a run that contains both forward and inverse calculations, these execute after the last `ineq()` of the forward calculation returns and before any later `ineq()`/`jacobian_sums()` call. Caveat: `setup_inverse` resizes `my_array` (`src/inverse.cpp:250`) and refills it (`286` onward) before these reads, so the values read are the inverse code's own, not those left by `ineq()`.
  - Address-of-only uses outside `ineq()` (take an element address; no value read): `src/prep.cpp:533`, `546`, `623` (`build_gas_phase`), `716`, `720`, `733`, `738` (`build_ss_assemblage`), `891`, `911`, `950`, `979`, `1007`, `1025` (`build_jacobian_sums`), `4700`, `4716`, `4746` (`store_dn`), `4798` (`store_jacob0`); `src/gases.cpp:230`, `243`, `321` (`build_fixed_volume_gas`); `src/inverse.cpp:286` (memcpy destination), `1366` (`shrink(inv_ptr, &my_array[0], ...)` — `shrink` then reads through the alias `array_in`). These run during `prep()`/`reprep()` (`src/prep.cpp:100`, `2566` via `build_model`, `src/prep.cpp:1084-1338`), i.e. **YES** they can execute in the window, but they read no stored value.
  - Writes only (not reads): `src/model.cpp:4534` (`residuals()`), `src/model.cpp:5163` (`my_array.clear()` in `free_model_allocs`, `5149`), `src/prep.cpp:89` (resize), `src/inverse.cpp:4683` (restore). Comment: `src/Phreeqc.cpp:1567`.
- **`normal` — NO reads outside `ineq()`.** Every use of the member is inside `src/model.cpp:928-1922` (`1033`, `1034`, `1118`, `1686`, `1854`, `1860`, `1864`). Other `normal` matches are unrelated: commented declaration `src/Phreeqc.cpp:1854`, prose in `src/cvode.cpp:3566-3669`.
- **`ineq_array` — NO reads outside `ineq()`.** All uses are `src/model.cpp:1127-1779` (inside the body); `src/Phreeqc.cpp:1854` is a comment.
- **`back_eq` — NO reads outside `ineq()`.** All uses are `src/model.cpp:1128-1915`; `src/Phreeqc.cpp:1855` is a comment.
- **`zero` — NO reads outside `ineq()`.** Member uses outside the body appear only at `src/inverse.cpp:2679` and `src/inverse.cpp:2722`, and both are inside `/* ... */` comment blocks (verified by reading `src/inverse.cpp:2676-2684` and `2714-2730`). Inside `ineq()` the only read is `src/model.cpp:1773` (memcpy source). Other `zero` matches are local variables (`src/cl1mp.cpp:63`), a method name (`cxxSolution::zero`, `src/Solution.cxx:1396`), or prose.
- **`res` — NO reads outside `ineq()`.** All member uses are `src/model.cpp:1131-1915`; `&res[0]` is passed to `cl1()` at `src/model.cpp:1780`, inside `ineq()`. `l_res` in `cl1.cpp` and `inv_res` in `inverse.cpp` are different variables.
- **`delta1` — NO reads outside `ineq()`.** All member uses are `src/model.cpp:1133-1851`. `inv_delta1`, `slnq_delta1`, `delta2`, `delta3` are separate members.
- **`cu`, `iu`, `is` — NO reads outside `ineq()`.** Member uses: `src/model.cpp:1751-1753` (resize), `src/model.cpp:1781` (`&cu[0], &iu[0], &is[0]` passed to `cl1()`), all inside `ineq()`. In `cl1.cpp`/`cl1mp.cpp` these names are `cl1`'s own parameters (`src/cl1mp.cpp:31`) or locals (`src/cl1mp.cpp:59`). `int is = s_ptr->number;` at `src/model.cpp:2505`, `2519`, `2529`, `4376` are local declarations shadowing the member, not member reads; `std::istringstream is` at `src/kinetics.cpp:1402` is a local.
- **`x[i]->iteration` — NO reads outside `ineq()`.** The only read anywhere is `src/model.cpp:1165`, inside `ineq()`. Writes: `src/model.cpp:1157` (inside `ineq()`) and `src/structures.cpp:2480` `unknown_ptr->iteration = 0;` (inside `unknown_alloc`, `src/structures.cpp:2430` — initialization, a write).
- **`remove_unstable_phases` — reads outside `ineq()` exist.**
  - `src/model.cpp:106` (loop condition in `model()`). **YES** — evaluated after the previous iteration's `ineq()` returned (via `reset()`/`gammas()`/`molalities()`/... and `residuals()` at `105`, which is neither `ineq` nor `jacobian_sums`) and before the next `jacobian_sums()` (`148`) or `ineq()` (`157`).
  - `src/model.cpp:155`. **NO** — always executed after `jacobian_sums()` at `148`/`149` of the same iteration.
  - `src/model.cpp:235`, `244` (after the inner loop). **YES** — run after the last `ineq()` of the inner loop returned; if the outer loop continues (`242`) the next `jacobian_sums()`/`ineq()` come later.
  - `src/model.cpp:541` (inside `check_residuals`, `271-272`). **YES** — `check_residuals()` is called at `src/model.cpp:230` (after the inner loop, i.e. after the last `ineq()` returned), `src/mainsubs.cpp:437`, `544`, `630`, `src/kinetics.cpp:1466`, `src/pitzer.cpp:2381`, `src/sit.cpp:1370`.
  - `src/pitzer.cpp:2263` (`model_pz` loop condition). **YES.** `src/pitzer.cpp:2304`. **NO** (after `jacobian_sums()` at `2299`). `src/pitzer.cpp:2387`, `2410`. **YES.**
  - `src/sit.cpp:1252` (`model_sit` loop condition). **YES.** `src/sit.cpp:1293`. **NO** (after `jacobian_sums()` at `1288`). `src/sit.cpp:1376`, `1399`. **YES.**
  - Writes outside `ineq()` (not reads): `src/model.cpp:99`, `424`; `src/Phreeqc.cpp:879`, `1651`; `src/pitzer.cpp:2245`; `src/sit.cpp:1236`.
- **`max_row_count`, `max_column_count` — reads outside `ineq()` exist.**
  - `src/Phreeqc.cpp:1771-1772` (`max_row_count = pSrc->max_row_count;` etc., inside `InternalCopy`, `src/Phreeqc.cpp:1169`). **NO in this tree** — the only copy-construction call site found is `src/class_main.cpp:159` `Phreeqc MyCopy = *this;`, which runs after `read_database()` and before `MyCopy.run_simulations()` (`src/class_main.cpp:168`), i.e. before any `ineq()` call on either object; no `operator=` call site was found (`grep -rn "= \*this\|Phreeqc [A-Za-z_]* = " src/`).
  - `src/inverse.cpp`: written by `setup_inverse` at `196` (`max_column_count = ...`) and `214` (`max_row_count = ...`); read at `205`, `250-261`, `263`, `265`, `269`, `279-287`, `380`, `384`, `411`, `457`, `461`, `506`, `509-539`, `601-624`, `646-672`, `719`, `731`, `743-763`, `769`, `796-855`, `887-902`, `947`, `970`, `1366`, `1437`, `1442`, `2377`, `2438`, `2442`, `2497-2765` (`shrink`), `2820`, `2909`, `2942-3069`, `3313-3390`, `3730-3776` (all inside the inverse functions listed above). **YES, possible** (inverse code runs between forward calculations), but the values read are the ones `setup_inverse` itself wrote, not those left by `ineq()`.
  - `src/cl1mp.cpp:166`, `169`, `1100` (inside `cl1`, GMP variant): compiled only when `INVERSE_CL1MP` is defined — `src/cl1mp.cpp:1` `#ifdef INVERSE_CL1MP`, defined only under `PHRQC_USE_GMP` + `GMP_INCLUDE_PATH` (`CMakeLists.txt:294-298`); the baseline build has `PHRQC_USE_GMP:BOOL=OFF` (`build/baseline/CMakeCache.txt:314`), so this file is not compiled in the baseline configuration. If compiled: `cl1()` is called from `ineq()` (`src/model.cpp:1779`) and from inverse code (`src/inverse.cpp:1437`, `1442`, `2438`, `2442`, `2909`) — the inverse call sites are **YES, possible**, again reading the inverse code's own values.
  - Writes outside `ineq()` (not reads): `src/Phreeqc.cpp:959-960` (`initialize()`).

## B12. `in_kode`

Definition: `src/model.cpp:927-928`
```
int Phreeqc::
ineq(int in_kode)
```
Declaration: `src/Phreeqc.h:449` `int ineq(int kode);` — public member (`public:` at `src/Phreeqc.h:402`). The declaration names the parameter `kode`; the definition names it `in_kode`.

Every use of `in_kode` inside `ineq()` (`grep -n "in_kode" src/model.cpp` returns exactly these five lines; the other four are uses):

1. `src/model.cpp:1200-1203` (pure-phase row construction; `res[l_count_rows]` was set to `1.0` at `src/model.cpp:1187` when the component has no add-formula and is not dissolve-only, otherwise it is 0.0 from the value-initialized resize at `src/model.cpp:1131`):
```
1200: 				if (in_kode != 1)
1201: 				{
1202: 					res[l_count_rows] = 0.0;
1203: 				}
```
2. `src/model.cpp:1229-1232` (same pattern for `PP_MOLES`-type rows; `res[l_count_rows] = 1.0;` at `1228`):
```
1229: 			if (in_kode != 1)
1230: 			{
1231: 				res[l_count_rows] = 0.0;
1232: 			}
```
3. `src/model.cpp:1245-1248` (same pattern for `SS_MOLES && x[i]->ss_in == TRUE` rows; `res[l_count_rows] = 1.0;` at `1244`):
```
1245: 			if (in_kode != 1)
1246: 			{
1247: 				res[l_count_rows] = 0.0;
1248: 			}
```
4. `src/model.cpp:1741-1746` (before the `cl1()` call; the assignment at 1745 is redundant because `l_kode = 1;` is already set at 1741):
```
1737: /*
1738:  *   Retain constraints on mineral mass transfers, even if infeasible on
1739:  *   first attempt.
1740:  */
1741: 	l_kode = 1;
1742: 
1743: 	if (in_kode == 2)
1744: 	{
1745: 		l_kode = 1;
1746: 	}
```
Effect summary: `in_kode == 1` keeps the pure-phase/SS inequality right-hand sides at 1.0; `in_kode != 1` (i.e. 0, the only other value any caller passes) sets them to 0.0. `in_kode == 2` only affects the redundant assignment at 1745.

Call sites — exhaustive in this tree (`grep -rn "ineq(" src/` filtered to calls returns exactly six): `src/model.cpp:157`, `src/model.cpp:174`, `src/pitzer.cpp:2306`, `src/pitzer.cpp:2323`, `src/sit.cpp:1295`, `src/sit.cpp:1312`.

`src/model.cpp:157` with 10 lines of context (`src/model.cpp:147-167`):
```
147: 			{
148: 				jacobian_sums();
149: 				numerical_jacobian();
150: 
151: 			}
152: /*
153:  *   Full matrix with pure phases
154:  */
155: 			if (r == OK || remove_unstable_phases == TRUE)
156: 			{
157: 				return_kode = ineq(l_kode);
158: 				if (return_kode != OK)
159: 				{
160: 					if (debug_model == TRUE)
161: 					{
162: 						output_msg(sformatf(
163: 								   "Ineq had infeasible solution, "
164: 								   "kode %d, iteration %d\n", return_kode,
165: 								   iterations));
166: 					}
167: 					log_msg(sformatf("Ineq had infeasible solution, "
```
Situation: every inner Newton iteration that passes the gate at `155`. The argument `l_kode` is set to `1` at `src/model.cpp:104` (`l_kode = 1;`) at the top of each outer time-step iteration; the only other writes to `l_kode` in `model()` are `src/model.cpp:72` and `79` (`l_kode = model_pz();` / `l_kode = model_sit();`), which return at `74`/`81` and never reach line 157. Therefore this call always passes `in_kode == 1`.

`src/model.cpp:174` with 10 lines of context (`src/model.cpp:164-184`):
```
164: 								   "kode %d, iteration %d\n", return_kode,
165: 								   iterations));
166: 					}
167: 					log_msg(sformatf("Ineq had infeasible solution, "
168: 							   "kode %d, iteration %d\n", return_kode,
169: 							   iterations));
170: 					count_infeasible++;
171: 				}
172: 				if (return_kode == 2)
173: 				{
174: 					ineq(0);
175: 				}
176: 				reset();
177: 			}
178: 			gammas(mu_x);
179: 			if (molalities(FALSE) == ERROR)
180: 			{
181: 				revise_guesses();
182: /*				adjust_step_size(); */
183: 			}
184: 			if (use.Get_surface_ptr() != NULL &&
```
Situation: a retry of the same solve, executed only when the `ineq(1)` at `157` returned `2`. `ineq()` returns `2` when `cl1()` sets `*l_kode == 2` (`src/model.cpp:1787-1789`; set at `src/cl1.cpp:464`, reached from `src/cl1.cpp:459-465` with the comment `/* no positive value found in L340 or bypass intermediate vertices */`) or when `cl1()` sets `*l_kode == 3` (`src/model.cpp:1791-1795`, which also emits `warning_msg("Too many iterations in Cl1. Should not have done this.")`). The retry passes `in_kode == 0`, which zeroes the pure-phase/SS inequality right-hand sides (uses 1-3 above).

The `pitzer.cpp` and `sit.cpp` drivers repeat the same pattern: `l_kode = 1;` at `src/pitzer.cpp:2261` / `src/sit.cpp:1250`; `return_kode = ineq(l_kode);` at `src/pitzer.cpp:2306` / `src/sit.cpp:1295`; `if (return_kode == 2) { ineq(0); }` at `src/pitzer.cpp:2321-2324` / `src/sit.cpp:1310-1313`.

No call site in this tree passes `2`, so the `in_kode == 2` branch at `src/model.cpp:1743` is unreachable from any caller in this tree. `ineq` is a public member (`src/Phreeqc.h:402`, `449`), so callers outside this tree could pass any value, but no such callers exist here (per A1, IPhreeqc and PhreeqcRM are not present).

## B13. Early-return path

`src/model.cpp:960-1006`, quoted as required:
```
960:  */
961: /*
962:  *   Special case for removing unstable phases
963:  */
964: 	cxxPPassemblage * pp_assemblage_ptr = use.Get_pp_assemblage_ptr();
965: 	cxxPPassemblageComp * comp_ptr;
966: 
967: 	if (remove_unstable_phases == TRUE)
968: 	{
969: 		if (debug_model == TRUE)
970: 		{
971: 			output_msg(sformatf(
972: 					   "\nSolution vector for removing unstable phases:\n"));
973: 		}
974: 		for (i = 0; i < count_unknowns; i++)
975: 		{
976: 
977: 			if (x[i]->type == PP)
978: 			{
979: 				//std::map<std::string, cxxPPassemblageComp>::iterator it;
980: 				//it =  pp_assemblage_ptr->Get_pp_assemblage_comps().find(x[i]->pp_assemblage_comp_name);
981: 				//assert(it != pp_assemblage_ptr->Get_pp_assemblage_comps().end());
982: 				cxxPPassemblageComp * comp_ptr = (cxxPPassemblageComp *) x[i]->pp_assemblage_comp_ptr;
983: 				if (residual[i] > 0e-8 && x[i]->moles > 0 &&
984: 					//it->second.Get_add_formula().size() == 0
985: 					comp_ptr->Get_add_formula().size() == 0
986: 					&& x[i]->dissolve_only == FALSE)
987: 				{
988: 					/*
989: 					*   Set mass transfer to all of phase
990: 					*/
991: 					delta[i] = x[i]->moles;
992: 				}
993: 				else
994: 				{
995: 					delta[i] = 0.0;
996: 				}
997: 				if (debug_model == TRUE)
998: 				{
999: 					output_msg(sformatf( "%6d  %-12.12s %10.2e\n", i,
1000: 						x[i]->description, (double) delta[i]));
1001: 				}
1002: 			}
1003: 		}
1004: 		remove_unstable_phases = FALSE;
1005: 		return (OK);
1006: 	}
```

Condition for the early return: `remove_unstable_phases == TRUE` at entry to `ineq()` (`src/model.cpp:967`). The return is `return (OK);` at `src/model.cpp:1005`, after writing `delta[i]` for PP unknowns (`991`, `995`) and clearing the flag (`1004`). No linear solve (`cl1()`) is performed on this path.

Can it execute when no unknown has type PP? **NO in this tree.** Evidence:
- The only write of `TRUE` to `remove_unstable_phases` anywhere is `src/model.cpp:424` (`remove_unstable_phases = TRUE;`), inside `check_residuals()` (`src/model.cpp:271-272`), guarded by `else if (x[i]->type == PP)` (`src/model.cpp:400`) and `if ((residual[i] >= epsilon * 100 && x[i]->moles > 0.0))` (`src/model.cpp:421-422`). The flag can become TRUE only when an unconverged PP unknown exists.
- All writes of `FALSE`: `src/model.cpp:99` (start of `model()`, before the outer loop), `src/model.cpp:1004` (the early return itself), `src/Phreeqc.cpp:879` (inside `void Phreeqc::init(void)`, `src/Phreeqc.cpp:462`), `src/Phreeqc.cpp:1651` (inside `Phreeqc::InternalCopy`, `src/Phreeqc.cpp:1169`), `src/pitzer.cpp:2245` (start of `model_pz()`), `src/sit.cpp:1236` (start of `model_sit()`). None of these can execute between `src/model.cpp:424` and the next `ineq()` call.
- The path from the TRUE write to the next `ineq()` call is: `check_residuals()` at `src/model.cpp:230` → outer-loop body `src/model.cpp:231-254` (stop check `231-234`; water-switch handling `235-242`; `if (remove_unstable_phases == FALSE) break;` at `244-245`, so the loop continues only while the flag is TRUE; logging `246-253`) → top of outer loop `100` → inner loop → `ineq(l_kode)` at `157`. Neither `src/model.cpp:231-254` nor `src/model.cpp:100-157` contains any call to `prep()`, `reprep()`, `build_model()`, or `free_model_allocs()` (verified by scanning that range), so the unknown set is not rebuilt in that window and the PP unknown that caused the flag to be set is still present when `ineq()` is entered.
- The PP unknown is not removed by the mechanism itself: `reset()` (`src/model.cpp:176`) applies `delta` but does not delete unknowns; the only code that erases a pp_assemblage entry is `src/ReadClass.cxx:312` (`Rxn_pp_assemblage_map.erase(*it);`), which is reaction-definition input processing, not part of the Newton loop.

Note: the `ineq(0)` retry at `src/model.cpp:174` cannot take this path, because a prior `ineq()` that saw the flag TRUE returned `OK` via `1005` (not `2`), so `return_kode == 2` at `172` implies the flag was already FALSE.

## B14. Context of `x[i]->iteration`

`src/model.cpp:1139-1170`, quoted as required:
```
1139: 	for (i = 0; i < count_unknowns; i++)
1140: 	{
1141: 		if (iterations < aqueous_only)
1142: 			continue;
1143: /*
1144:  *   Pure phases
1145:  */
1146: 		if (x[i]->type == PP)
1147: 		{
1148: 			//std::map<std::string, cxxPPassemblageComp>::iterator it;
1149: 			//it =  pp_assemblage_ptr->Get_pp_assemblage_comps().find(x[i]->pp_assemblage_comp_name);
1150: 			
1151: 			/* not in model, ignore */
1152: 			if (x[i]->phase->in == FALSE)
1153: 				continue;		
1154: 			// delay removing phase
1155: 			if (x[i]->moles > 0.0 || x[i]->f <= 0.0 || iterations == 0 || equi_delay == 0)
1156: 			{
1157: 				x[i]->iteration = iterations;
1158: 			}
1159: 			cxxPPassemblageComp * comp_ptr = (cxxPPassemblageComp *) x[i]->pp_assemblage_comp_ptr;
1160: 			//if (it->second.Get_force_equality())
1161: 			if (comp_ptr->Get_force_equality())
1162: 				continue;
1163: 			/*   Undersaturated and no mass, ignore */
1164: 			if (x[i]->f > 0e-8 && x[i]->moles <= 0
1165: 				&& iterations >= x[i]->iteration + equi_delay
1166: 				&& comp_ptr->Get_add_formula().size() == 0)
1167: 			{
1168: 				continue;
1169: 			}
1170: 			else if (x[i]->f < 0e-8 && x[i]->dissolve_only == TRUE
```

Unknown types that reach line 1157: **only `PP`**. Line 1157 is lexically inside the `if (x[i]->type == PP)` block opened at `src/model.cpp:1146`; no other branch of the loop at `1139` contains it. Within `PP`, reaching 1157 additionally requires: `iterations >= aqueous_only` (guard at `1141`), `x[i]->phase->in == TRUE` (guard at `1152`), and the disjunction at `1155` (`x[i]->moles > 0.0 || x[i]->f <= 0.0 || iterations == 0 || equi_delay == 0`). The only other write to the `iteration` field anywhere is the initialization `unknown_ptr->iteration = 0;` at `src/structures.cpp:2480` (`unknown_alloc`), and the only read is `src/model.cpp:1165`, inside the same `PP` branch (see B11).

## B15. Gate flags

Declarations (all are members of `class Phreeqc`; all are plain `int`):

| Flag | Declaration | Access section |
|---|---|---|
| `pitzer_model` | `src/Phreeqc.h:1753` `int pitzer_model, sit_model, pitzer_pe;` | `protected:` at `src/Phreeqc.h:1737` |
| `sit_model` | `src/Phreeqc.h:1753` (same line) | `protected:` at `src/Phreeqc.h:1737` |
| `mass_water_switch` | `src/Phreeqc.h:1560` `int mass_water_switch;` | `protected:` at `src/Phreeqc.h:1494` |
| `aqueous_only` | `src/Phreeqc.h:1565` `int aqueous_only;` | `protected:` at `src/Phreeqc.h:1494` |
| `remove_unstable_phases` | `src/Phreeqc.h:1587` `int remove_unstable_phases;` | `protected:` at `src/Phreeqc.h:1494` |

Is each a `Phreeqc` member that `Phreeqc::ineq()` can read? **Yes for all five.** Each is declared in `class Phreeqc`; `ineq()` is a member of the same class (`src/Phreeqc.h:449`, `src/model.cpp:927-928`), so `protected` access is not a barrier. No local variable shadows any of these names inside `ineq()` (`grep -n` over `src/model.cpp:928-1922` finds no declaration of these names in that range). Each is in fact read inside `ineq()`:

- `pitzer_model` — read at `src/model.cpp:1010`, `1015`, `1076`, `1284`, `1408`, `1711`.
- `sit_model` — read at `src/model.cpp:1010`, `1408`, `1711`.
- `mass_water_switch` — read at `src/model.cpp:1287`, `1326`, `1666`.
- `aqueous_only` — read at `src/model.cpp:1141` (`if (iterations < aqueous_only)`, see B14).
- `remove_unstable_phases` — read at `src/model.cpp:967`, written at `src/model.cpp:1004` (the early-return path, see B13).

# Part C: Baseline

## C1. Instrumented inputs

Two files created, both byte-for-byte copies of the CMake-registered example inputs plus one inserted block:

| Created | Copied from |
| --- | --- |
| `tests/petsc_linear_solver/inputs/ex1_seawater_speciation.pqi` | `examples/ex1` |
| `tests/petsc_linear_solver/inputs/ex2_gypsum_anhydrite_equilibria.pqi` | `examples/ex2` |

`diff examples/ex1 tests/petsc_linear_solver/inputs/ex1_seawater_speciation.pqi` output is insertion-only (`1a2,12`, the 11-line block below). `diff examples/ex2 tests/petsc_linear_solver/inputs/ex2_gypsum_anhydrite_equilibria.pqi` output is insertion-only (`2a3,14`). Nothing else in either copy was changed. (An initial edit had added one leading space to line 2 of the ex2 copy; it was restored byte-exactly from `examples/ex2` before any run, and the diffs above are after that fix.)

Inserted into `ex1_seawater_speciation.pqi` after the `TITLE` line, before `SOLUTION 1`:

```
SELECTED_OUTPUT 101
        -file                  ex1_seawater_speciation.sel
        -high_precision        true
        -pH
        -pe
        -ionic_strength
        -water
        -temp
        -totals                H C O N Na K Mg Ca Fe Mn Si Cl S U
        -molalities            H+ OH- Na+ K+ Ca+2 Mg+2 Cl- SO4-2 HCO3- CO3-2 CO2 NaSO4- CaSO4 CaCO3 MgCO3 MgSO4 CaHCO3+ HSO4- UO2+2
        -saturation_indices    Uraninite
```

Inserted into `ex2_gypsum_anhydrite_equilibria.pqi` after the `TITLE` continuation line, before `SOLUTION 1`:

```
SELECTED_OUTPUT 101
        -file                  ex2_gypsum_anhydrite_equilibria.sel
        -high_precision        true
        -pH
        -pe
        -ionic_strength
        -water
        -temp
        -totals                H
        -molalities            H+ OH- Ca+2 SO4-2 HSO4- CaSO4 CaHSO4+
        -saturation_indices    anhydrite gypsum
        -equilibrium_phases    Gypsum Anhydrite
```

`examples/ex2` has its own unnumbered `SELECTED_OUTPUT` block (`examples/ex2:11-14`, `-file ex2.sel`, `-temperature`, `-si anhydrite gypsum` at line 14). It was left untouched, so the ex2 run produces two selected-output files: `ex2.sel` (block 1) and `ex2_gypsum_anhydrite_equilibria.sel` (block 101).

### Keyword and option identifiers (all confirmed in the source that reads the block)

Keyword dispatch: `src/PhreeqcKeywords/Keywords.cpp:66` `std::map<const std::string, Keywords::KEYWORDS>::value_type("selected_output", Keywords::KEY_SELECTED_OUTPUT),`; handled at `src/read.cpp:214-216`:

```
214:		case Keywords::KEY_SELECTED_OUTPUT:
215:			read_selected_output();
216:			break;
```

`read_selected_output()` is defined at `src/read.cpp:4759`. Its option list is `src/read.cpp:4768-4820` (`int count_opt_list = 51;` at `src/read.cpp:4821`). Every identifier used above is in that list, with its index comment:

```
4769:		"file",					/* 0 */
4770:		"totals",				/* 1 */
4771:		"molalities",			/* 2 */
4775:		"saturation_indices",	/* 6 */
4777:		"equilibrium_phases",	/* 8 */
4798:		"temperature",			/* 29 */
4799:		"temp",					/* 30 */
4800:		"ph",					/* 31 */
4801:		"pe",					/* 32 */
4804:		"ionic_strength",		/* 35 */
4806:		"water",				/* 37 */
4807:		"high_precision",		/* 38 */
```

Option matching is case-insensitive and prefix-based: `src/read.cpp:8582` calls `find_option(&(stdoption.c_str()[1]), &opt, opt_list, count_opt_list, FALSE)` (the `-` is skipped by the `[1]` offset); `find_option` lowercases the token at `src/read.cpp:8502` `Utilities::str_tolower(stdtoken);` and, for `exact != TRUE`, matches at `src/read.cpp:8515` `if (strstr(list[i], stdtoken.c_str()) == list[i])`, first match in list order wins. So `-pH` resolves to index 31 and `-temp` to index 29 (`"temperature"` precedes `"temp"`).

### Numbered `SELECTED_OUTPUT` blocks (amendment item 2)

`src/read.cpp:4829-4836`:

```
4829:	SelectedOutput temp_selected_output;
4830:	cptr = line;
4831:	temp_selected_output.read_number_description(cptr);
4832:	n_user = temp_selected_output.Get_n_user();
4833:	temp_selected_output.Set_new_def(false);
4834:	temp_selected_output.Set_file_name(n_user);
4835:	// find if it exists
4836:	std::map< int, SelectedOutput >::iterator so = SelectedOutput_map.find(n_user);
```

`SelectedOutput` derives from `cxxNumKeyword` (`src/SelectedOutput.h:8` `class SelectedOutput:public cxxNumKeyword`, default number at `src/SelectedOutput.h:11` `SelectedOutput(int n=1, PHRQ_io *io=NULL);`). The number is read in `src/NumKeyword.cxx:151-171`:

```
151:	if (CParser::copy_token(token, b, e) == CParser::TT_DIGIT)
...
163:		int j = sscanf(token.c_str(), "%d%d", &this->n_user, &this->n_user_end);
...
168:		else if (j == 1)
169:		{
170:			this->n_user_end = this->n_user;
171:		}
```

so `SELECTED_OUTPUT 101` gives `n_user = n_user_end = 101`. Because `n_user != 1`, the branch at `src/read.cpp:4869-4878` applies:

```
4870:	{
4871:		// n_user != 1 then reset false
4872:
4873:		temp_selected_output.Reset(false);
4874:		if (so == SelectedOutput_map.end())
4875:		{
4876:			temp_selected_output.Set_new_def(true);
4877:		}
4878:	}
```

101 does not occur in either input (`grep -n "SELECTED_OUTPUT" tests/petsc_linear_solver/inputs/*.pqi` shows one block per file, numbered 101; in the ex2 copy the second block is the unnumbered one inherited from `examples/ex2:11`, which takes `n_user = 1` by `src/NumKeyword.cxx:179` `this->n_user = this->n_user_end = 1;`).

### Names used in `-totals`, `-molalities`, `-saturation_indices`, `-equilibrium_phases`

- ex1 elements set in `SOLUTION 1` (`examples/ex1:2-22`): `Ca Mg Na K Fe Mn Si Cl S N U O` (written as `S(6)`, `N(5)`, `N(-3)`, `U`, `O(0)`), `H` from `pH` (`examples/ex1:4`), and `C` from `Alkalinity 141.682 as HCO3` (`examples/ex1:17`). The `-totals` list is exactly that set, `H C O N Na K Mg Ca Fe Mn Si Cl S U`.
- Species names were taken from `database/phreeqc.dat`, not guessed: master species `H+` (`database/phreeqc.dat:62` `H+ = H+`), `e-` (`:76`), `Na+` (`:85`), `K+` (`:92`), `Mg+2` (`:97`), `Ca+2` (`:102`), `Cl-` (`:132`), `CO3-2` (`:138`), `SO4-2` (`:143`); complexes `OH-` (`:202` `H2O = OH- + H+`), `CO2` (`:232` `CO3-2 + 2 H+ = CO2 + H2O`), `HCO3-` (`:225`), `HSO4-` (`:251`), `CaHCO3+` (`:365`), `CaSO4` (`:371`), `CaHSO4+` (`:377`), `MgCO3` (`:399`), `MgSO4` (`:412`), `NaSO4-` (one match for `= NaSO4-`). `UO2+2` is defined by the input itself (`examples/ex1:27`, `examples/ex1:44`). `NaCO3-` is NOT in `database/phreeqc.dat` (`grep -nE "=[[:space:]]*NaCO3-([[:space:]]|$)" database/phreeqc.dat` → no match) and was not used; the database names for the aqueous complexes are `CaSO4`, `CaCO3`, `MgCO3`, `MgSO4`, `CO2` (no `(aq)` suffix).
- Phases: `Gypsum` (`database/phreeqc.dat:1003`), `Anhydrite` (`database/phreeqc.dat:1008`), `Uraninite` is defined by the input (`examples/ex1:65-66`, `PHASES` / `Uraninite`).
- `e-` cannot be requested through `-molalities`. The token must begin with an upper-case letter, `(`, or `[` (`src/read.cpp:4944-4949`, inside `case 2: /* molalities */` at `src/read.cpp:4939`):

  ```
  4944:				if (i != UPPER && token[0] != '(' && (token[0] != '['))
  4945:				{
  4946:					error_string = sformatf( "Expected species name to"
  4947:							" begin with upper case letter.");
  4948:					warning_msg(error_string);
  4949:				}
  ```

  A first version of the inputs listed `e-`; the trial run echoed the option line and then printed `WARNING: Expected species name to begin with upper case letter.` (`ex1_seawater_speciation.out:35` of that trial run) and produced no `m_e-` column. `e-` was removed from both inputs before the baseline was recorded. The ex1 list has 19 species and the ex2 list 7, both within the "up to 20" limit.
- ex2 `-totals H`: the `SOLUTION 1` block of `examples/ex2:3-5` sets only `pH` and `temp`, so `H` is the only element defined in it. `Ca` and `S` enter the system only by dissolving `EQUILIBRIUM_PHASES` (`examples/ex2:6-8`). This is the literal reading of the specification; if totals of `Ca` and `S` are wanted, the reviewer must say so.
- Why `C` counts as an element defined in the ex1 `SOLUTION` block: `Alkalinity` is read as the master species for C(4), `src/prep.cpp:4402-4404`:

  ```
  4402:				error_msg
  4403:					("Error in definition of Alkalinity in SOLUTION_MASTER_SPECIES and SOLUTION_SPECIES.\n\tAlkalinity master species should be same as master species for C(4).",
  4404:					 CONTINUE);
  ```

  that is, an `Alkalinity` definition puts inorganic carbon into the solution as element C.

## C2. Baseline run

Full record (commands, hashes, counts): `tests/petsc_linear_solver/baseline/README.md`. Summary and evidence below.

**Build (unmodified code, as currently configured).** Target name confirmed first: `CMakeLists.txt:3` `project(phreeqc)`, `CMakeLists.txt:260` `add_executable(phreeqc ${phreeqc_SOURCES})` — the only executable target. Commands (plan amendment item 3):

```
cmake -S . -B build/baseline -DCMAKE_BUILD_TYPE=Release
cmake --build build/baseline --target phreeqc -j 8
```

CMake 4.2.3. From `build/baseline/CMakeCache.txt`:

```
28:CMAKE_BUILD_TYPE:STRING=Release
34:CMAKE_CXX_COMPILER:FILEPATH=/usr/bin/c++
45:CMAKE_CXX_FLAGS:STRING=
54:CMAKE_CXX_FLAGS_RELEASE:STRING=-O3 -DNDEBUG
```

`/usr/bin/c++ --version` first line: `c++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0`. `NDEBUG` is set by `CMAKE_CXX_FLAGS_RELEASE`, which is why the task rules forbid `assert` in new tests.

**Runs.** Working directories `build/baseline/run` (baseline) and `build/baseline/second_run` (determinism check); command lines byte-identical in both:

```
../../../build/baseline/phreeqc ../../../tests/petsc_linear_solver/inputs/ex1_seawater_speciation.pqi ex1_seawater_speciation.out ../database/phreeqc.dat ex1_seawater_speciation.log
../../../build/baseline/phreeqc ../../../tests/petsc_linear_solver/inputs/ex2_gypsum_anhydrite_equilibria.pqi ex2_gypsum_anhydrite_equilibria.out ../database/phreeqc.dat ex2_gypsum_anhydrite_equilibria.log
```

All four invocations exited 0. `../database/phreeqc.dat` is the CMake-copied database (`build/baseline/database/phreeqc.dat`), byte-identical to `database/phreeqc.dat` (`cmp` reports no difference); it is the database the CMake example tests use (`examples/CMakeLists.txt:136`, `examples/CMakeLists.txt:142`).

Command-line argument meaning, from the source: `argv[2]` output file `src/class_main.cpp:407-414`; `argv[3]` database `src/class_main.cpp:481-486`; `argv[4]` error-stream file, `src/class_main.cpp:361-363` `if (!phrq_io->error_open(argv[4]))`. A file literally named `phreeqc.log` is always opened in the working directory by the command-line program (`src/class_main.cpp:418-420`, with `log` hard-coded `TRUE` at the call sites `src/class_main.cpp:122` and `src/class_main.cpp:220`); it was 0 bytes in both runs. The `*.log` files named in the commands are error-stream files, not the log file.

**Saved to `tests/petsc_linear_solver/baseline/`:** `ex1_seawater_speciation.out`, `ex1_seawater_speciation.sel`, `ex2_gypsum_anhydrite_equilibria.out`, `ex2_gypsum_anhydrite_equilibria.sel`, and `ex2.sel` (written by the `SELECTED_OUTPUT` block that already existed in `examples/ex2:11-14`; it is an output of the frozen input, so it is included). `README.md` written as required. The error-stream `.log` files and `phreeqc.log` were not copied.

**Determinism check (amendment item 4).** `sha256sum`, baseline copy vs `build/baseline/second_run` copy:

| File | Result |
| --- | --- |
| `ex1_seawater_speciation.sel` | identical `980301a1fafd91d28fb8df2f1c0b4398ada90a71fc8f90f59f21afdbbdde592b` |
| `ex2_gypsum_anhydrite_equilibria.sel` | identical `7d6037e8d2b80998906fb09474651861829e2b34380d6f685213913dd2255ab8` |
| `ex2.sel` | identical `e4ff162c8957509d6d4195a1dceb24ba1f933bbe11c1c0dc4ae08170b8bce926` |
| `ex1_seawater_speciation.out` | differs: 1 line |
| `ex2_gypsum_anhydrite_equilibria.out` | differs: 3 lines |

Every differing line, quoted with its line number (identical numbering in both copies):

`ex1_seawater_speciation.out:356`

```
run:        End of Run after 0.012774 Seconds.
second_run: End of Run after 0.011799 Seconds.
```

`ex2_gypsum_anhydrite_equilibria.out:4369-4371` (the separator width follows the length of the timing value):

```
run         4369: ----------------------------------
second_run  4369: ---------------------------------
run         4370: End of Run after 0.020343 Seconds.
second_run  4370: End of Run after 0.02032 Seconds.
run         4371: ----------------------------------
second_run  4371: ---------------------------------
```

No other line differs in any compared file. The error-stream files differ only at their line 22, with the same `End of Run after ...` text (outside the required comparison set). Conclusion: the numerical content of the baseline is reproducible bit-for-bit; the only nondeterministic output is the wall-clock line. No output file was edited or filtered.

## C3. Iteration counts

**The main output does report Newton iteration counts, one line per calculation.** Printed in `print.cpp` at `src/print.cpp:2417-2420`:

```
2417:	if (iterations == overall_iterations)
2418:		output_msg(sformatf("%45s%3d\n", "Iterations  = ", iterations));
2419:	else
2420:		output_msg(sformatf("%45s%3d (%d overall)\n", "Iterations  = ", iterations, overall_iterations));
```

Extraction command (from `tests/petsc_linear_solver/baseline/`):

```
grep -oE "Iterations  = +[0-9]+( \([0-9]+ overall\))?" ex1_seawater_speciation.out
grep -oE "Iterations  = +[0-9]+( \([0-9]+ overall\))?" ex2_gypsum_anhydrite_equilibria.out
```

Baseline counts (also in the README). `ex1_seawater_speciation.out`, 1 calculation:

```
8
```

`ex2_gypsum_anhydrite_equilibria.out`, 52 calculations (initial solution + 51 `REACTION_TEMPERATURE` steps, `examples/ex2:9-10`), in file order:

```
3 16 16 22 17 18 16 19 26 55 22 21 25 20 19 21 22 20 23 22 22 21 22 16 28 23 28 19 20 20 19 19 19 20 19 19 19 20 20 19 27 20 19 19 20 16 16 19 16 17 16 16
```

No line matched the `(N overall)` form (`src/print.cpp:2420`) in either file. The extracted sequences are identical between the baseline and the second run.

# Stop conditions

**S3 and S4 are YES. Per the task specification, work stops here and waits for instructions.**

- **S1. NO.** The only installed PETSc is 3.26.0 (`/opt/petsc/3.26.0/include/petscversion.h:8-10`), which is not below 3.19; `PETSC_USE_COMPLEX` is not defined in `/opt/petsc/3.26.0/include/petscconf.h`; `PETSC_USE_REAL_DOUBLE` is defined there (line 171), so precision is double. Caveat, already flagged in A3: the environment's `PETSC_DIR=/home/chen/petsc` with `PETSC_ARCH=arch-linux-c-opt` names a directory that does not exist, so there is nothing there to evaluate; which installation to use is a reviewer decision.
- **S2. NO.** B1 confirmed all four properties of the augmented Newton matrix (`my_array`): row-major (`src/model.cpp:1943-1946`), leading dimension `count_unknowns + 1` (`src/prep.cpp:89-91`), residual in column `count_unknowns` (`src/model.cpp:4531-4534`), 0-based indexing (`src/prep.cpp:4443`). The layout does not differ from the assumed one.
- **S3. YES.** Several B5 types are treated specially by `ineq()`: ALK is routed to the optimization rows (`src/model.cpp:1207`); MH is skipped when `pitzer_model == TRUE && pitzer_pe == FALSE` (`src/model.cpp:1284-1285`) and its row is rewritten when `mass_water_switch == TRUE` (`src/model.cpp:1326-1334`); MH2O is skipped when `mass_water_switch == TRUE` (`src/model.cpp:1287-1288`); MB has a failed-row special case (`src/model.cpp:1879-1881`). The second clause is NO — for systems made only of B5 types `ineq()` adds no extra rows or columns (B5, last bullet) — but the first clause alone makes this YES.
- **S4. YES.** `ineq()` writes members other than `delta` that its caller reads. Example: `remove_unstable_phases` is written inside `ineq()` at `src/model.cpp:1004` (`remove_unstable_phases = FALSE;`, on the early `return (OK);` path at `src/model.cpp:1005`) and read by the caller in the Newton-loop condition at `src/model.cpp:105-106` (`while ((r = residuals()) != CONVERGED` / `|| remove_unstable_phases == TRUE)`) and again at `src/model.cpp:155`, `235`, `244`, `541`; `ineq()` itself reads it at `src/model.cpp:967`. The `TRUE` value is set in `check_residuals()` (`src/model.cpp:424`, function defined at `src/model.cpp:271-272`), which the caller invokes at `src/model.cpp:230` — outside `ineq()`. B3 lists the other members `ineq()` writes. The other two clauses are NO: `ineq()` does not read `delta` before writing it, and it does not call itself.

If any answer is YES or AMBIGUOUS the task says to stop; two are YES.
