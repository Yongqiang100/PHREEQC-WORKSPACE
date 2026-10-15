# Overlay Task O0: Audit Report

| Field | Value |
|---|---|
| Date | 2026-10-15 |
| Mode | Read-only; only this file and `~/phreeqc3/build/overlay/` were written |
| P3 | `~/upstream/phreeqc3_v390` @ `b0b3be767158ccc3322d2c816625cf470045e67e` (`git describe`: `v3.9.0`) |
| RM | `~/phreeqcrm_baseline_src` @ `90b31ed8f2080dd957af6cd75edb9ce099d0914b` (`git describe`: `v3.9.0`) |
| Our phreeqc3 | `petsc-phase1b-complete` = `b83ffd8943a5df5754cb0475f5e8a0df08d8567e` |
| Our PhreeqcRM | `newton-solver-phase2-task3` = `cf6ff4fea7c7206b9daad599d503e8e1db536916` |

Saved data: `build/overlay/o0_p3_edits.diff` (241 lines), `build/overlay/o0_rm_edits.diff` (1161 lines), plus `o0_p3_model_at_tag.cpp`, `o0_p3_Phreeqc_at_tag.cpp`, `o0_p3_Phreeqc_h_at_tag.h` (tag contents used for quoting with line numbers).

## A1. Our current edits

### A1.1 phreeqc3: `git diff b0b3be76 petsc-phase1b-complete -- src/Phreeqc.h src/model.cpp src/Phreeqc.cpp src/class_main.cpp`

```
src/Phreeqc.cpp    |   7 ++++
src/Phreeqc.h      |  11 +++++
src/class_main.cpp |  24 ++++++++++-
src/model.cpp      | 115 ++++++++++++++++++++++++++++++++++++++++++++++++++++-
4 files changed, 154 insertions(+), 3 deletions(-)
```

Hunks and classification (line numbers are in `build/overlay/o0_p3_edits.diff`):

| # | Hunk header | Classification | Content |
|---|---|---|---|
| P1 | `Phreeqc.cpp @@ -16,6 +16,10 @@` | include | `#ifdef USE_NEWTON_LINEAR_SOLVER` / `#include "newton_linear_solver_state.h"` / `#endif` |
| P2 | `Phreeqc.cpp @@ -61,6 +65,9 @@` | destructor | inside `Phreeqc::~Phreeqc(void)`: `destroy_newton_linear_solver_state(&newton_linear_solver_state);` under `#ifdef USE_NEWTON_LINEAR_SOLVER` |
| P3 | `Phreeqc.h @@ -71,6 +71,8 @@` | forward declaration | `struct NewtonLinearSolverState;` before `class Phreeqc` |
| P4 | `Phreeqc.h @@ -447,6 +449,15 @@` | members | after `int ineq(int kode);`: decl `ineq_cl1_original(int kode)`, decl `newton_solver_ineligibility_reason(int in_kode) const`, inline accessor `augmented_newton_matrix()` returning `my_array.data()`, member `NewtonLinearSolverState *newton_linear_solver_state = nullptr;` |
| P5 | `class_main.cpp @@ -14,6 +14,10 @@` | include | `#ifdef USE_PETSC` / `#include "petsc_initialization.h"` / `#endif` |
| P6 | `class_main.cpp @@ -84,8 +88,24 @@` | PETSc pin | wraps `Phreeqc phreeqc_instance;` / `return phreeqc_instance.main_method(argc, argv);` in a scope, with `acquire_petsc()` before and `release_petsc(); shutdown_petsc_if_owned_by_phreeqc();` after, all under `#ifdef USE_PETSC` |
| P7 | `model.cpp @@ -8,6 +8,10 @@` | include | same guarded include as P1 |
| P8 | `model.cpp @@ -923,9 +927,89 @@` | eligibility + dispatcher + rename | defines `Phreeqc::newton_solver_ineligibility_reason` (eligibility), defines new `Phreeqc::ineq` (dispatcher), and renames the old definition line `-ineq(int in_kode)` → `+ineq_cl1_original(int in_kode)` |
| P9 | `model.cpp @@ -1776,9 +1860,38 @@` | shadow hook + steering hook | before `cl1(`: shadow prepare block and steering block ending in `if (!newton_cl1_problem_accepted)`; after the `cl1(` statement: `cl1_problem_shadow_record(...)` |

### A1.2 PhreeqcRM: `git diff v3.9.0 newton-solver-phase2-task3 -- src/IPhreeqcPhast/IPhreeqc/phreeqcpp/`

```
src/IPhreeqcPhast/IPhreeqc/phreeqcpp/Phreeqc.cpp   |   7 +
src/IPhreeqcPhast/IPhreeqc/phreeqcpp/Phreeqc.h     |  11 +
src/IPhreeqcPhast/IPhreeqc/phreeqcpp/model.cpp     |  85 ++++-
.../newton_linear_solver/lapack_newton_solver.cpp  | 205 ++++++++++++
.../newton_linear_solver/lapack_newton_solver.h    |  40 +++
.../newton_linear_solver/linear_solve_status.h     |   6 +
.../newton_linear_solver_options.cpp               | 220 ++++++++++++
.../newton_linear_solver_options.h                 |  57 ++++
.../newton_linear_solver_state.cpp                 | 367 +++++++++++++++++++++
.../newton_linear_solver_state.h                   |  65 ++++
10 files changed, 1062 insertions(+), 1 deletion(-)
```

Hunks in the three edited upstream files (identical to the phreeqc3 hunks except where noted):

| # | Hunk header | Classification |
|---|---|---|
| R1 | `Phreeqc.cpp @@ -16,6 +16,10 @@` | include (identical to P1) |
| R2 | `Phreeqc.cpp @@ -61,6 +65,9 @@` | destructor (identical to P2) |
| R3 | `Phreeqc.h @@ -71,6 +71,8 @@` | forward declaration (identical to P3) |
| R4 | `Phreeqc.h @@ -447,6 +449,15 @@` | members (identical to P4) |
| R5 | `model.cpp @@ -8,6 +8,10 @@` | include (identical to P7) |
| R6 | `model.cpp @@ -923,9 +927,88 @@` | eligibility + dispatcher + rename (see difference below) |

The remaining eight files are new files (`@@ -0,0 +1,N @@`), not edits: the six `newton_linear_solver/` sources listed in the stat.

### A1.3 Difference between the two diffs (fact, per reviewer note)

- The phreeqc3 diff contains P5/P6 (the `class_main.cpp` PETSc pin) and P9 (shadow/steering hooks around `cl1(`); the PhreeqcRM diff has no `class_main.cpp` change and no shadow/steering hunk.
- The RM dispatcher (diff line 115) lacks the phreeqc3 condition `&& solver_state->options.intercept_mode == NEWTON_INTERCEPT_ENTRY` (diff line 157–158): RM gates only on `solver_state->backend != NEWTON_BACKEND_CL1`.
- All other upstream-file hunks are byte-identical between the two diffs (same blob hashes in the `index` lines: `52d87210..4f12c235`, `f93c228f..055e8953`; model.cpp differs: `22c289c1` vs `dc63ad92`).

## A2. Movable logic (all quotes from `~/phreeqc3` at `petsc-phase1b-complete`)

Line numbers refer to the tag contents (`git show petsc-phase1b-complete:src/model.cpp`, saved as `build/overlay/o0_p3_model_at_tag.cpp`).

### A2.1 Dispatcher `Phreeqc::ineq` — `src/model.cpp:964-1009`

```cpp
964: /* ------------------------------------------------------------------------------- */
965: int Phreeqc::ineq(int in_kode)
966: /* ------------------------------------------------------------------------------- */
967: {
968: #ifdef USE_NEWTON_LINEAR_SOLVER
969: 	if (newton_linear_solver_state == nullptr) newton_linear_solver_state = create_newton_linear_solver_state();
970: 	NewtonLinearSolverState *solver_state = newton_linear_solver_state;
971: 	if (solver_state != nullptr && solver_state->backend != NEWTON_BACKEND_CL1
972: 		&& solver_state->options.intercept_mode == NEWTON_INTERCEPT_ENTRY) {
973: 		const int ineligibility_reason = newton_solver_ineligibility_reason(in_kode);
974: 		if (ineligibility_reason != NEWTON_SOLVER_ELIGIBLE) {
975: 			count_ineligible_newton_system(solver_state, ineligibility_reason);
976: 		} else {
977: 			solver_state->candidate_newton_step.resize((size_t)count_unknowns);
978: 			/* The backend reads the augmented Newton matrix BEFORE ineq_cl1_original runs
979: 			   (cl1 may modify its inputs) and writes only to candidate_newton_step. */
980: 			LinearSolveStatus solve_status = solve_newton_step_with_backend(
981: 				solver_state, augmented_newton_matrix(), count_unknowns,
982: 				solver_state->candidate_newton_step.data());
983: 			if (solver_state->options.compare_with_cl1) {   /* cl1 steers; backend result is only logged */
984: 				int cl1_return_code = ineq_cl1_original(in_kode);
985: 				double relative_difference = 0.0, relative_sum = 0.0;
986: 				int largest_difference_index = -1;
987: 				if (solve_status == LINEAR_SOLVE_ACCEPTED)
988: 					largest_difference_index = compare_newton_steps(
989: 						solver_state->candidate_newton_step.data(), delta.data(), count_unknowns,
990: 						&relative_difference, &relative_sum);
991: 				const int largest_difference_unknown_type =
992: 					largest_difference_index >= 0 ? (int)x[largest_difference_index]->type : -1;
993: 				write_step_comparison_record(
994: 					solver_state, count_unknowns, solve_status, cl1_return_code,
995: 					(double)solver_state->last_backward_error,
996: 					relative_difference, relative_sum,
997: 					largest_difference_index, largest_difference_unknown_type);
998: 				return cl1_return_code;
999: 			}
1000: 			if (solve_status == LINEAR_SOLVE_ACCEPTED) {
1001: 				for (int i = 0; i < count_unknowns; ++i) delta[i] = solver_state->candidate_newton_step[i];
1002: 				return OK;   /* success value from Task 0 item B3 */
1003: 			}
1004: 			/* Any other status, including LINEAR_SOLVE_NON_FINITE_INPUT, falls through to cl1. */
1005: 		}
1006: 	}
1007: #endif
1008: 	return ineq_cl1_original(in_kode);
1009: }
```

Phreeqc members used: `newton_linear_solver_state`, `count_unknowns` (Phreeqc.h:1435 at tag), `x`, `delta` (Phreeqc.h:1531 `std::vector<double> my_array, delta, residual;`), `my_array` (only via the accessor `augmented_newton_matrix()`, Phreeqc.h:457), and the member functions `newton_solver_ineligibility_reason`, `ineq_cl1_original`. Its locals (`solver_state`, `ineligibility_reason`, `solve_status`, `cl1_return_code`, `relative_difference`, `relative_sum`, `largest_difference_index`, `largest_difference_unknown_type`) are locals of the dispatcher itself, not of `ineq_cl1_original`. **Could live in the glue `.cpp` as a member function: yes** — it is `Phreeqc::ineq` itself; nothing of `ineq_cl1_original`'s locals is touched.

### A2.2 `newton_solver_ineligibility_reason` — `src/model.cpp:930-963`

```cpp
930: #ifdef USE_NEWTON_LINEAR_SOLVER
931: /* ------------------------------------------------------------------------------- */
932: int Phreeqc::
933: newton_solver_ineligibility_reason(int in_kode) const
934: /* ------------------------------------------------------------------------------- */
935: {
936: 	if (in_kode != 1)
937: 		return NEWTON_SOLVER_NOT_FIRST_ATTEMPT;
938: 	if (remove_unstable_phases == TRUE)
939: 		return NEWTON_SOLVER_REMOVING_UNSTABLE_PHASES;
940: 	if (pitzer_model == TRUE || sit_model == TRUE)
941: 		return NEWTON_SOLVER_PITZER_OR_SIT_MODEL;
942: 	if (mass_water_switch == TRUE)
943: 		return NEWTON_SOLVER_MASS_WATER_SWITCH;
944: 	if (iterations < aqueous_only)
945: 		return NEWTON_SOLVER_AQUEOUS_ONLY_ITERATION;
946: 	if (numerical_deriv)   /* D17: numerical-derivative mode excluded entirely */
947: 		return NEWTON_SOLVER_NUMERICAL_DERIVATIVES;
948: 	if (count_unknowns <= 0)
949: 		return NEWTON_SOLVER_EMPTY_SYSTEM;
950: 	for (int i = 0; i < count_unknowns; i++)
951: 	{
952: 		switch (x[i]->type)
953: 		{
954: 		case MB: case ALK: case CB: case SOLUTION_PHASE_BOUNDARY:
955: 		case MU: case AH2O: case MH: case MH2O:
956: 			break;
957: 		default:
958: 			return NEWTON_SOLVER_INELIGIBLE_UNKNOWN_TYPE;
959: 		}
960: 	}
961: 	return NEWTON_SOLVER_ELIGIBLE;
962: }
963: #endif
```

Phreeqc members used: `remove_unstable_phases` (Phreeqc.h:1598), `pitzer_model, sit_model` (1764), `mass_water_switch` (1571), `iterations` (1538), `aqueous_only` (1576), `numerical_deriv` (1579), `count_unknowns` (1435), `x`. Locals of `ineq_cl1_original` used: none (its only local is its own loop index `i`). **Could live in the glue `.cpp`: yes** — it already is a `const` member function.

### A2.3 Shadow and steering blocks around `cl1(` — `src/model.cpp:1863-1894`

```cpp
1860: /*
1861:  *   Call CL1
1862:  */
1863: #ifdef USE_NEWTON_LINEAR_SOLVER
1864: 	std::vector<int> newton_unknown_types;
1865: 	const bool newton_cl1_problem_shadow = cl1_problem_shadow_active(newton_linear_solver_state);
1866: 	if (newton_cl1_problem_shadow)
1867: 	{
1868: 		newton_unknown_types.resize(count_unknowns);
1869: 		for (size_t newton_index = 0; newton_index < count_unknowns; newton_index++)
1870: 			newton_unknown_types[newton_index] = x[newton_index]->type;
1871: 		cl1_problem_shadow_prepare(newton_linear_solver_state, in_kode,
1872: 			(pitzer_model || sit_model) ? 1 : 0, mass_water_switch ? 1 : 0, numerical_deriv ? 1 : 0,
1873: 			k, l, m, n,
1874: 			((sit_model || pitzer_model) && full_pitzer == FALSE) ? n + 2 : (int) max_column_count,
1875: 			&ineq_array[0], &delta1[0], newton_unknown_types.data(), back_eq.data());
1876: 	}
1877: #endif
1878: #ifdef USE_NEWTON_LINEAR_SOLVER
1879: 	int newton_cl1_problem_accepted = 0;
1880: 	if (cl1_problem_steering_active(newton_linear_solver_state))
1881: 		newton_cl1_problem_accepted = cl1_problem_steer(newton_linear_solver_state, in_kode,
1882: 			(pitzer_model || sit_model) ? 1 : 0, mass_water_switch ? 1 : 0, numerical_deriv ? 1 : 0,
1883: 			k, l, m, n,
1884: 			((sit_model || pitzer_model) && full_pitzer == FALSE) ? n + 2 : (int) max_column_count,
1885: 			&ineq_array[0], &delta1[0], &res[0], &l_kode, &l_iter, &l_error);
1886: 	if (!newton_cl1_problem_accepted)
1887: #endif
1888: 	cl1(k, l, m, n, l_nklmd, l_n2d, &ineq_array[0],
1889: 		&l_kode, ineq_tol, &l_iter, &delta1[0], &res[0],
1890: 		&l_error, &cu[0], &iu[0], &is[0], FALSE);
1891: #ifdef USE_NEWTON_LINEAR_SOLVER
1892: 	if (newton_cl1_problem_shadow)
1893: 		cl1_problem_shadow_record(newton_linear_solver_state, l_kode, &delta1[0]);
1894: #endif
```

Phreeqc members used: `newton_linear_solver_state`, `count_unknowns`, `x`, `pitzer_model, sit_model` (1764), `full_pitzer` (1765), `mass_water_switch` (1571), `numerical_deriv` (1579), `max_column_count` (1702), `ineq_array, res, cu, delta1` (1755), `iu, is, back_eq` (1756), `ineq_tol` (1562, in the `cl1(` call only).

Locals of `ineq_cl1_original` used (declarations at `src/model.cpp` at tag): `k, l, m, n` (1028), `l_kode` (1033), `l_iter` (1030), `l_error` (1031, type `LDBLE`); `l_nklmd, l_n2d` (1029) appear only in the `cl1(` call itself, not in the hooks. The block-locals `newton_unknown_types`, `newton_cl1_problem_shadow`, `newton_cl1_problem_accepted` are ours, not upstream's.

**Could live in the glue `.cpp`: yes**, as member functions (e.g. `newton_solver_before_cl1(in_kode, k, l, m, n, ..., &l_kode, &l_iter, &l_error)` and `newton_solver_after_cl1(l_kode, &delta1[0])` as in design §3), with `k, l, m, n, l_kode, l_iter, l_error` passed as arguments because they are locals of `ineq_cl1_original`. One design note: `newton_cl1_problem_shadow` is computed before the `cl1(` call and read after it (line 1865 → 1892); with the two one-line edits of design §3 this flag cannot be a local of `ineq_cl1_original`. It can be stored inside `NewtonLinearSolverState` by the before-hook and re-read by the after-hook — feasible without a third edit, but it is a decision for the reviewer (see decisions list).

### A2.4 Destructor call — `src/Phreeqc.cpp:66-72`

```cpp
66: Phreeqc::~Phreeqc(void)
67: {
68: #ifdef USE_NEWTON_LINEAR_SOLVER
69: 	destroy_newton_linear_solver_state(&newton_linear_solver_state);
70: #endif
71:
72: 	clean_up();
```

Phreeqc members used: `newton_linear_solver_state` only. Locals of `ineq_cl1_original`: none. **Could live in the glue `.cpp`: yes** — a member function `newton_solver_destroy_state()` taking no arguments.

### A2.5 Summary

No logic in A2 is blocked from moving to `glue/newton_solver_phreeqc_glue.cpp` as `Phreeqc` member functions. The only arguments needed beyond `this` are the `ineq_cl1_original` locals `k, l, m, n, l_kode, l_iter, l_error` (and pointers to the members `ineq_array, delta1, res`, which the design's edit (4) passes explicitly). The shadow-active flag must move from a function local into `NewtonLinearSolverState`.

## A3. Anchors in the unmodified upstream files

Counted in `$P3/src/` and `$RM/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/` only. Multi-line counts by `python3 -c` (`str.count`); single-line counts also cross-checked with `grep -c`. `$` marks end-of-line and `^I` marks a tab in the `cat -A` quotes.

**Edit (1), `Phreeqc.h`, forward declaration before `class Phreeqc`.** Anchor (insert after):

```
#include "global_structures.h"

class Phreeqc
```

Exact bytes (`$P3/Phreeqc.h:72-74`, `cat -A`): `#include "global_structures.h"$`, `$`, `class Phreeqc$`.
Count: **1 in P3, 1 in RM** (P3 line 72, RM line 72).

**Edit (2), `Phreeqc.h`, members include inside the class.** Anchor (insert after):

```
\tint ineq(int kode);
```

(`$P3/Phreeqc.h:449`, `cat -A`: `^Iint ineq(int kode);$`). Count: **1 in P3, 1 in RM** (line 449 in both).

**Edit (3), `model.cpp`, rename the definition.** Anchor, exact bytes (`$P3/model.cpp:927-928`, `cat -A`):

```
int Phreeqc::$
ineq(int in_kode)$
```

No tabs on these two lines; the anchor is `int Phreeqc::\nineq(int in_kode)\n`. Count: **1 in P3, 1 in RM** (line 927 in both). Note: the fragment `int Phreeqc::` alone occurs 24 times in each `model.cpp` (`grep -c 'int Phreeqc::$'` = 24), so the `ineq(int in_kode)` line is required in the anchor.

**Edit (4), `model.cpp`, insert before the `cl1(` call.** The full call statement, `$P3/model.cpp:1779-1781` and `$RM/model.cpp:1779-1781` (identical; `cat -A`):

```
^Icl1(k, l, m, n, l_nklmd, l_n2d, &ineq_array[0],$
^I^I&l_kode, ineq_tol, &l_iter, &delta1[0], &res[0],$
^I^I&l_error, &cu[0], &iu[0], &is[0], FALSE);$
```

Anchor: the whole three-line statement (insert before it). Count: **1 in P3, 1 in RM**. `cl1(` itself occurs exactly once in each `model.cpp` (`grep -n 'cl1('` → line 1779 only), so the first line alone would also be unique.

**Edit (5), `model.cpp`, insert after that statement.** Anchor: the last line of the statement, exact bytes `^I^I&l_error, &cu[0], &iu[0], &is[0], FALSE);$` (insert after it). `grep -c '&l_error, &cu\[0\], &iu\[0\], &is\[0\], FALSE);'`: **1 in P3, 1 in RM** (line 1781 in both). The three-line statement anchor of edit (4) is equally unique and would let one edit serve both insertions; the reviewer may prefer that.

**Edit (6), `Phreeqc.cpp`, destructor.** Anchor (insert after `{`):

```
Phreeqc::~Phreeqc(void)
{
```

(`$P3/Phreeqc.cpp:62-63`, `cat -A`: `Phreeqc::~Phreeqc(void)$`, `{$`). Count: **1 in P3, 1 in RM** (line 62 in both).

All six anchors are unique in both trees. Line numbers of every anchor position are identical in P3 and RM (72/74, 449, 927, 1779-1781, 62).

## A4. File hashes

`sha256sum` in `$P3/src/` and `$RM/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/`:

| File | P3 | RM | Identical |
|---|---|---|---|
| `Phreeqc.h` | `761c4bfb4f75be786aa576bec7c1e2129a9b4768fa507edaf0f8f2df2eed462d` | same | yes |
| `model.cpp` | `0fb852f2d9fa171cf7324ba6601742a1b5380a989813cd7ffcbcace7068f607e` | same | yes |
| `Phreeqc.cpp` | `e1c03f8d19db76017b8acf9589cd65376fef95cfa7a584a71c9e829fc7488989` | same | yes |
| `class_main.cpp` | `6133d5734212c5afc05c64fd670990566298a8482d36080cb52d8f53c8bb9bcc` | same | yes |

All four files are byte-identical between the two upstream trees.

## A5. Build attachment facts

### A5.1 phreeqc3 (`$P3/CMakeLists.txt`)

- `cmake_minimum_required(VERSION 3.10)` — line 1.
- Top-level project line, line 3: `project(phreeqc)`.
- Target compiling `model.cpp`: **`phreeqc`**, created as an *executable*: `add_executable(phreeqc ${phreeqc_SOURCES})` (line 260); `src/model.cpp` is listed in the `phreeqc_SOURCES` set at line 119 (set starts line 13). There is no library target for the engine in phreeqc3 v3.9.0.
- Include directories (lines 291-293):
  ```
  target_include_directories(phreeqc PRIVATE "${PROJECT_SOURCE_DIR}/src")
  target_include_directories(phreeqc PRIVATE "${PROJECT_SOURCE_DIR}/src/common")
  target_include_directories(phreeqc PRIVATE "${PROJECT_SOURCE_DIR}/src/PhreeqcKeywords")
  ```
- Link-style precedent: **plain** (no keyword) — the only `target_link_libraries` call, line 300: `target_link_libraries(phreeqc ${GMP_LIBRARY})`.
- `target_compile_features(phreeqc PUBLIC cxx_std_11)` (line 263) confirms the C++11 baseline.

### A5.2 PhreeqcRM (`$RM/CMakeLists.txt`)

- `cmake_minimum_required(VERSION 3.22...4.1)` — line 1.
- Top-level project lines, 3-7:
  ```
  project(
    PhreeqcRM
    VERSION 3.8.0
    LANGUAGES CXX C
  )
  ```
  (fact: the declared `VERSION 3.8.0` does not match the `v3.9.0` tag).
- Target compiling `model.cpp`: **`PhreeqcRM`** — `add_library(PhreeqcRM ${PhreeqcRM_SOURCES})` (line 322), alias `PhreeqcRM::PhreeqcRM` (line 323); the source is listed at line 211 (`${CMAKE_CURRENT_SOURCE_DIR}/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/model.cpp`).
- Include directories (lines 325-335, `PUBLIC`): `$<BUILD_INTERFACE:${PhreeqcRM_BINARY_DIR}>`, `.../src`, `.../src/IPhreeqcPhast`, `.../src/IPhreeqcPhast/IPhreeqc`, `.../phreeqcpp`, `.../phreeqcpp/common`, `.../phreeqcpp/PhreeqcKeywords`, plus `$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>`.
- Link-style precedent: **keyword** — e.g. line 376 `target_link_libraries(PhreeqcRM PUBLIC yaml-cpp::yaml-cpp)`, line 407 `target_link_libraries(PhreeqcRM PRIVATE ZLIB::ZLIB)`, line 475 `target_link_libraries(PhreeqcRM PUBLIC MPI::MPI_CXX)`.

### A5.3 `project(` calls reached during PhreeqcRM's configure

`grep -rn "project(" --include=CMakeLists.txt $RM` gives six in-tree calls:

| Location | Call | Reached when |
|---|---|---|
| `CMakeLists.txt:3` | `project(PhreeqcRM ...)` | always (top level) |
| `Tests/CMakeLists.txt:5` | `project(TestInstall ...)` | `Tests` is added only if `STANDALONE_BUILD EQUAL 1` and `NOT DEFINED SKBUILD` (lines 611-617); this `project()` then runs unless `_PHREEQCRM_TEST_INSTALL` is false — note the file's `project()` at line 5 is inside `if(_PHREEQCRM_TEST_INSTALL)`; `project(TestRM CXX)` at line 105 runs unconditionally once `Tests` is added |
| `Tests/CMakeLists.txt:20` | `project(FortranAdvect Fortran)` | `PHREEQCRM_FORTRAN_TESTING` |
| `Tests/CMakeLists.txt:105` | `project(TestRM CXX)` | `Tests` added (see above) |
| `setup-fortran/CMakeLists.txt:3` | `project(hw Fortran)` | not reached: `setup-fortran` is never `add_subdirectory`ed from the top level (standalone examples) |
| `setup-fortran/openmp/CMakeLists.txt:3` | `project(openmp Fortran)` | not reached (same) |

Fetched content (not on disk in the source tree; their `project()` calls exist only inside the downloaded archives and are reached at configure time): `zlib` via `FetchContent_Declare`/`FetchContent_MakeAvailable` (lines 54-63, only when `DEFINED SKBUILD`), and `yaml-cpp` 0.8.0 via `FetchContent_Declare(... OVERRIDE_FIND_PACKAGE)` + `find_package(yaml-cpp REQUIRED)` (lines 86-93, only when `PHREEQCRM_WITH_YAML_CPP=ON`, which is forced ON under `SKBUILD`). The design's guard `CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR` in `attach.cmake` is therefore required for the nested `TestInstall`/`FortranAdvect`/`TestRM` and any fetched project.

### A5.4 CMake version

```
$ cmake --version
cmake version 4.2.3
```

Satisfies the design's CMake 3.19+ requirement (`cmake_language(DEFER)`).

## A6. The PETSc pin

### A6.1 The `class_main.cpp` edit (from A1 diff, hunks P5/P6)

```cpp
#ifdef USE_PETSC
#include "petsc_initialization.h"
#endif
```

and in `main()`:

```cpp
	int return_code;
#ifdef USE_PETSC
	const bool petsc_pinned = (acquire_petsc() == 0);
#endif
	{
		/* Scope ends before PETSc shutdown, so the Phreeqc object and its PETSc state are
		   destroyed first. */
		Phreeqc phreeqc_instance;
		return_code = phreeqc_instance.main_method(argc, argv);
	}
#ifdef USE_PETSC
	if (petsc_pinned)
	{
		release_petsc();
		(void)shutdown_petsc_if_owned_by_phreeqc();
	}
#endif
	return return_code;
```

### A6.2 The decisions behind it

D10, `docs/petsc_linear_solver/task0_reviewer_decisions.md:51`:

> **D10. `main()`.** `src/class_main.cpp:31-89` constructs one `Phreeqc` on the stack and returns `phreeqc_instance.main_method(argc, argv)` (`src/class_main.cpp:87-88`). The Task 4 change wraps exactly those two lines.

P2-D4, `docs/petsc_linear_solver/phase2_task0_reviewer_decisions.md:19`:

> **P2-D4. PETSc lifecycle in PhreeqcRM.** With `lapack_direct` as the default, PETSc is never initialised unless the PETSc backend is selected, and then only lazily, as in Phase 1. Design §4 M1 (`PETSC_COMM_WORLD = MPI_COMM_SELF`) and M2 (no finalisation after `MPI_Finalized`) are still required, but only for an MPI build with an MPI-enabled PETSc. They are deferred together with P2-D2 (c). The `MPI_Finalized` guard in the `atexit` handler is cheap and is added anyway when the PETSc backend is compiled with MPI headers.

### A6.3 Is the pin required with only the LAPACK backend?

**No — fact from the code.** Both `class_main.cpp` hunks are inside `#ifdef USE_PETSC`, and the whole of `src/petsc_linear_solver/petsc_initialization.cpp` is wrapped in `#ifdef USE_PETSC` (line 1: `#ifdef USE_PETSC`). With only the LAPACK backend (`USE_PETSC` undefined) the pin compiles to nothing, and the `atexit` handler `finalize_petsc_at_process_exit` is registered only inside `acquire_petsc()` (`petsc_initialization.cpp:44-47`), which the LAPACK path never calls. Nothing in the LAPACK path needs the pin; the design's statement (overlay_design.md:57) that the pin "is not needed for LAPACK" is confirmed.

### A6.4 Would PETSc alone, without the pin, finalise correctly through the `atexit` handler?

**AMBIGUOUS — cannot be settled from the code.** The handler (`petsc_initialization.cpp:19-26`) finalises when `phreeqc_initialized_petsc && reference_count == 0`; without the pin `reference_count` stays 0, so it would call `PetscFinalize()` after `main()` returns, i.e. after the `Phreeqc` destructor has already destroyed the PETSc objects. That ordering is consistent with the handler's comment about static-storage objects, but whether it is correct in all cases (interaction with PETSc's own exit handling, and the reason the pin comment gives — "Scope ends before PETSc shutdown") can only be established by running the PETSc build without the pin, which O0 does not do. Question for the reviewer; design §3 line 57 already flags it [VERIFY].

## Reviewer decisions required

1. **Anchors:** none non-unique. All six proposed anchors occur exactly once in both upstream trees, at identical line numbers. Optional simplification: edits (4) and (5) can share the single unique three-line `cl1(` statement anchor; please confirm the anchor forms to freeze in `edits.cmake` and `SUPPORTED_UPSTREAM.txt`.
2. **Logic that cannot move to the glue file:** none. One required change: the shadow-active flag (`newton_cl1_problem_shadow`, currently a local spanning the `cl1(` call) must be stored in `NewtonLinearSolverState` by the before-hook and re-read by the after-hook, so that design §3's two one-line edits suffice. Confirm this approach.
3. **Differences between the two upstream trees:** none in the four edited files (all four SHA-256 identical). Differences are in the build wiring only: phreeqc3 target `phreeqc` is an *executable* with plain `target_link_libraries` and `cmake_minimum_required(VERSION 3.10)`; PhreeqcRM target `PhreeqcRM` is a library (with ALIAS) with keyword linking and `cmake_minimum_required(VERSION 3.22...4.1)`. The design's §4 step 4 must therefore use `target_sources`-style attachment on an executable for phreeqc3, and the design's "CMake 3.19 or later" exceeds phreeqc3's own declared 3.10 minimum — acceptable only if the overlay raises the effective requirement; confirm. Also note PhreeqcRM v3.9.0's top-level `project()` declares `VERSION 3.8.0`.
4. **PETSc-pin question (A6.4):** whether PETSc alone, without the `class_main.cpp` pin, finalises correctly through the `atexit` handler is AMBIGUOUS from the code. With LAPACK only, the pin is provably unnecessary. Decide whether O1 drops the pin, or O2 must run a PETSc-without-pin test before deciding.
