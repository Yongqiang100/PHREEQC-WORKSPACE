# Task 5: Phase 1A Validation Report

Generated 2026-10-11. This task modified no source file, build file, test input, or baseline file. It created the run directories under `build/`, the copies under `tests/petsc_linear_solver/results/`, the ASAN build tree `build/petsc_on_asan/`, and this report.

## Environment at run time

- `echo "$PETSC_DIR"` printed `/opt/petsc/3.26.0`; `PETSC_ARCH` was empty (matches decision D1).
- `git status --short` was empty; HEAD was `f0c1599e` ("Decisions D17 (numerical_deriv gate) and D18 (Task 4 call accounting)").
- `build/baseline`, `build/petsc_off`, `build/petsc_on` already existed. No file under `src/`, `tests/petsc_linear_solver/`, or `CMakeLists.txt` was newer than `build/petsc_on/phreeqc` or `build/petsc_off/phreeqc`, so both binaries matched the current tree before any step ran.
- All runs used the commands in `tests/petsc_linear_solver/baseline/README.md` exactly, with only the executable path changed, from directories at the same depth as `build/baseline/run`. PETSc options were passed only through `PETSC_OPTIONS`.
- All comparison scripts were run with default tolerances (relative 1e-6; absolute 1e-14 amounts, 1e-6 logarithmic columns). No baseline, input, or script was edited.

## Summary table

| Step | Result | Key numbers |
|---|---|---|
| 1 Unit tests | PASS | `ctest`: 100% tests passed, 0 failed out of 6 |
| 2 ex1, PETSc steering | PASS | `solves=8 accepted=8 cl1_fallbacks=0 non_finite_inputs=0 petsc_errors=0 ineligible_systems=0 solver_slots_created=1`; all ineligibility counters 0; `.sel` comparison PASS, 1 row x 39 columns, 0 failures, every largest diff 0.000e+00; iterations 8 (PETSc) vs 8 (baseline) |
| 3 ex2, eligibility gate | PASS | `ineligible_unknown_type=1045`, `not_first_attempt=3`, `removing_unstable_phases=8`; `solves=3 accepted=3 cl1_fallbacks=0 non_finite_inputs=0 petsc_errors=0 ineligible_systems=1056`; both `.sel` comparisons PASS, 52 rows, 0 failures, every largest diff 0.000e+00; iteration sequences identical (52 values) |
| 4 ex1, comparison mode | PASS | 8 comparison records, all `LINEAR_SOLVE_ACCEPTED`; relative difference median 2.131e-16, max 5.468e-16; no sign-inversion flag, no large-difference flag; `cmp` `.sel` identical (exit 0); main output `IDENTICAL` (355 compared lines) |
| 5 ex2, comparison mode | PASS | 3 comparison records, all `LINEAR_SOLVE_ACCEPTED`; relative difference median 0.000e+00, max 8.427e-19; no flags; `cmp` both `.sel` files identical (exit 0); main output `IDENTICAL` (4369 compared lines) |
| 6a PETSc memory (`-malloc_debug -malloc_dump`) | PASS | ex1 and ex2 exit 0; `-malloc_dump` printed no memory-still-allocated line at `PetscFinalize`; D16 exception not needed (`petsc_errors=0` in both runs) |
| 6b AddressSanitizer | PASS | build exit 0; ex1 and ex2 exit 0; no AddressSanitizer error and no LeakSanitizer report in either run |
| 7 OFF build regression | PASS | `cmp` all three `.sel` files identical (exit 0); both main outputs `IDENTICAL` (355 and 4369 compared lines) |

## Step 1 — Unit tests

Command (from the repository root):

```
ctest --test-dir build/petsc_on -R "test_initialization|test_newton_matrix_transfer|test_newton_solver" --output-on-failure
```

Full output:

```
Test project /home/chen/phreeqc3/build/petsc_on
    Start 1: test_initialization_phreeqc_owns_petsc
1/6 Test #1: test_initialization_phreeqc_owns_petsc ....   Passed    0.01 sec
    Start 2: test_initialization_host_owns_petsc
2/6 Test #2: test_initialization_host_owns_petsc .......   Passed    0.01 sec
    Start 3: test_initialization_explicit_shutdown
3/6 Test #3: test_initialization_explicit_shutdown .....   Passed    0.01 sec
    Start 4: test_newton_matrix_transfer
4/6 Test #4: test_newton_matrix_transfer ...............   Passed    0.01 sec
    Start 5: test_newton_solver
5/6 Test #5: test_newton_solver ........................   Passed    0.01 sec
    Start 6: test_newton_solver_after_petsc_finalize
6/6 Test #6: test_newton_solver_after_petsc_finalize ...   Passed    0.01 sec

100% tests passed, 0 tests failed out of 6

Total Test time (real) =   0.07 sec
```

## Step 2 — ex1, PETSc steering

Run (working directory `build/petsc_on/step2_ex1_petsc_steering`), exit status 0:

```
PETSC_OPTIONS="-phreeqc_petsc_statistics" ../../../build/petsc_on/phreeqc ../../../tests/petsc_linear_solver/inputs/ex1_seawater_speciation.pqi ex1_seawater_speciation.out ../database/phreeqc.dat ex1_seawater_speciation.log 2> stderr.txt
```

Captured `stderr` (`tests/petsc_linear_solver/results/step2_ex1_petsc_steering/stderr.txt`):

```
phreeqc_petsc_ineligible: not_first_attempt=0 removing_unstable_phases=0 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=0
phreeqc_petsc_statistics: solves=8 accepted=8 cl1_fallbacks=0 non_finite_inputs=0 petsc_errors=0 ineligible_systems=0 solver_slots_created=1
```

Expected values: `solves > 0` (it is 8), `accepted == solves` (8 == 8), `cl1_fallbacks == 0`, `non_finite_inputs == 0`, `petsc_errors == 0`. All hold. `ineligible_systems = 0`; every reason on the `phreeqc_petsc_ineligible:` line is 0 (full line above).

Comparison command:

```
python3 tests/petsc_linear_solver/tools/compare_selected_output_to_baseline.py tests/petsc_linear_solver/baseline/ex1_seawater_speciation.sel tests/petsc_linear_solver/results/step2_ex1_petsc_steering/ex1_seawater_speciation.sel --json-report tests/petsc_linear_solver/results/step2_ex1_petsc_steering/selected_output_comparison.json
```

Full output (exit status 0):

```
PASS  rows=1 columns=39 relative_tolerance=1e-06 absolute_tolerance_amount=1e-14 absolute_tolerance_log=1e-06
column                      kind      largest_abs_diff  largest_rel_diff  failures   row
pH                          log              0.000e+00         0.000e+00         0     -
pe                          log              0.000e+00         0.000e+00         0     -
temp                        amount           0.000e+00         0.000e+00         0     -
mu                          amount           0.000e+00         0.000e+00         0     -
mass_H2O                    amount           0.000e+00         0.000e+00         0     -
H                           amount           0.000e+00         0.000e+00         0     -
C                           amount           0.000e+00         0.000e+00         0     -
O                           amount           0.000e+00         0.000e+00         0     -
N                           amount           0.000e+00         0.000e+00         0     -
Na                          amount           0.000e+00         0.000e+00         0     -
K                           amount           0.000e+00         0.000e+00         0     -
Mg                          amount           0.000e+00         0.000e+00         0     -
Ca                          amount           0.000e+00         0.000e+00         0     -
Fe                          amount           0.000e+00         0.000e+00         0     -
Mn                          amount           0.000e+00         0.000e+00         0     -
Si                          amount           0.000e+00         0.000e+00         0     -
Cl                          amount           0.000e+00         0.000e+00         0     -
S                           amount           0.000e+00         0.000e+00         0     -
U                           amount           0.000e+00         0.000e+00         0     -
m_H+                        amount           0.000e+00         0.000e+00         0     -
m_OH-                       amount           0.000e+00         0.000e+00         0     -
m_Na+                       amount           0.000e+00         0.000e+00         0     -
m_K+                        amount           0.000e+00         0.000e+00         0     -
m_Ca+2                      amount           0.000e+00         0.000e+00         0     -
m_Mg+2                      amount           0.000e+00         0.000e+00         0     -
m_Cl-                       amount           0.000e+00         0.000e+00         0     -
m_SO4-2                     amount           0.000e+00         0.000e+00         0     -
m_HCO3-                     amount           0.000e+00         0.000e+00         0     -
m_CO3-2                     amount           0.000e+00         0.000e+00         0     -
m_CO2                       amount           0.000e+00         0.000e+00         0     -
m_NaSO4-                    amount           0.000e+00         0.000e+00         0     -
m_CaSO4                     amount           0.000e+00         0.000e+00         0     -
m_CaCO3                     amount           0.000e+00         0.000e+00         0     -
m_MgCO3                     amount           0.000e+00         0.000e+00         0     -
m_MgSO4                     amount           0.000e+00         0.000e+00         0     -
m_CaHCO3+                   amount           0.000e+00         0.000e+00         0     -
m_HSO4-                     amount           0.000e+00         0.000e+00         0     -
m_UO2+2                     amount           0.000e+00         0.000e+00         0     -
si_Uraninite                log              0.000e+00         0.000e+00         0     -
```

Iteration-count extraction (the command defined in `tests/petsc_linear_solver/baseline/README.md`, run on both `.out` files):

```
grep -oE "Iterations  = +[0-9]+( \([0-9]+ overall\))?" <file>
```

| File | Extracted counts |
|---|---|
| `tests/petsc_linear_solver/baseline/ex1_seawater_speciation.out` | `Iterations  =   8` |
| `tests/petsc_linear_solver/results/step2_ex1_petsc_steering/ex1_seawater_speciation.out` | `Iterations  =   8` |

## Step 3 — ex2, eligibility gate

Run (working directory `build/petsc_on/step3_ex2_eligibility_gate`), exit status 0:

```
PETSC_OPTIONS="-phreeqc_petsc_statistics" ../../../build/petsc_on/phreeqc ../../../tests/petsc_linear_solver/inputs/ex2_gypsum_anhydrite_equilibria.pqi ex2_gypsum_anhydrite_equilibria.out ../database/phreeqc.dat ex2_gypsum_anhydrite_equilibria.log 2> stderr.txt
```

Captured `stderr`, both statistics lines in full (`tests/petsc_linear_solver/results/step3_ex2_eligibility_gate/stderr.txt`):

```
phreeqc_petsc_ineligible: not_first_attempt=3 removing_unstable_phases=8 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=1045
phreeqc_petsc_statistics: solves=3 accepted=3 cl1_fallbacks=0 non_finite_inputs=0 petsc_errors=0 ineligible_systems=1056 solver_slots_created=1
```

Expected: `ineligible_unknown_type > 0` (it is 1045). The counts match the Task 4 accounting recorded in decision D18 (3 accepted solves; 1045 pure-phase, 8 unstable-phase-removal, 3 retry calls; 1056 ineligible total).

Comparison 1 (exit status 0):

```
python3 tests/petsc_linear_solver/tools/compare_selected_output_to_baseline.py tests/petsc_linear_solver/baseline/ex2_gypsum_anhydrite_equilibria.sel tests/petsc_linear_solver/results/step3_ex2_eligibility_gate/ex2_gypsum_anhydrite_equilibria.sel --json-report tests/petsc_linear_solver/results/step3_ex2_eligibility_gate/selected_output_comparison_ex2_gypsum_anhydrite_equilibria.json
```

```
PASS  rows=52 columns=19 relative_tolerance=1e-06 absolute_tolerance_amount=1e-14 absolute_tolerance_log=1e-06
column                      kind      largest_abs_diff  largest_rel_diff  failures   row
pH                          log              0.000e+00         0.000e+00         0     -
pe                          log              0.000e+00         0.000e+00         0     -
temp                        amount           0.000e+00         0.000e+00         0     -
mu                          amount           0.000e+00         0.000e+00         0     -
mass_H2O                    amount           0.000e+00         0.000e+00         0     -
H                           amount           0.000e+00         0.000e+00         0     -
m_H+                        amount           0.000e+00         0.000e+00         0     -
m_OH-                       amount           0.000e+00         0.000e+00         0     -
m_Ca+2                      amount           0.000e+00         0.000e+00         0     -
m_SO4-2                     amount           0.000e+00         0.000e+00         0     -
m_HSO4-                     amount           0.000e+00         0.000e+00         0     -
m_CaSO4                     amount           0.000e+00         0.000e+00         0     -
m_CaHSO4+                   amount           0.000e+00         0.000e+00         0     -
Gypsum                      amount           0.000e+00         0.000e+00         0     -
d_Gypsum                    amount           0.000e+00         0.000e+00         0     -
Anhydrite                   amount           0.000e+00         0.000e+00         0     -
d_Anhydrite                 amount           0.000e+00         0.000e+00         0     -
si_anhydrite                log              0.000e+00         0.000e+00         0     -
si_gypsum                   log              0.000e+00         0.000e+00         0     -
```

Comparison 2 (exit status 0):

```
python3 tests/petsc_linear_solver/tools/compare_selected_output_to_baseline.py tests/petsc_linear_solver/baseline/ex2.sel tests/petsc_linear_solver/results/step3_ex2_eligibility_gate/ex2.sel --json-report tests/petsc_linear_solver/results/step3_ex2_eligibility_gate/selected_output_comparison_ex2.json
```

```
PASS  rows=52 columns=11 relative_tolerance=1e-06 absolute_tolerance_amount=1e-14 absolute_tolerance_log=1e-06
column                      kind      largest_abs_diff  largest_rel_diff  failures   row
sim                         amount           0.000e+00         0.000e+00         0     -
state                       text             0.000e+00         0.000e+00         0     -
soln                        amount           0.000e+00         0.000e+00         0     -
dist_x                      amount           0.000e+00         0.000e+00         0     -
time                        amount           0.000e+00         0.000e+00         0     -
step                        amount           0.000e+00         0.000e+00         0     -
pH                          log              0.000e+00         0.000e+00         0     -
pe                          log              0.000e+00         0.000e+00         0     -
temp                        amount           0.000e+00         0.000e+00         0     -
si_anhydrite                log              0.000e+00         0.000e+00         0     -
si_gypsum                   log              0.000e+00         0.000e+00         0     -
```

Iteration-count extraction, same command as step 2. Baseline (`ex2_gypsum_anhydrite_equilibria.out`, 52 calculations) and step 3 result, side by side; the two extracted sequences are identical:

```
baseline: 3 16 16 22 17 18 16 19 26 55 22 21 25 20 19 21 22 20 23 22 22 21 22 16 28 23 28 19 20 20 19 19 19 20 19 19 19 20 20 19 27 20 19 19 20 16 16 19 16 17 16 16
step3:    3 16 16 22 17 18 16 19 26 55 22 21 25 20 19 21 22 20 23 22 22 21 22 16 28 23 28 19 20 20 19 19 19 20 19 19 19 20 20 19 27 20 19 19 20 16 16 19 16 17 16 16
```

No line matched the `(N overall)` form in either file.

## Step 4 — ex1, comparison mode

Run (working directory `build/petsc_on/step4_ex1_comparison_mode`), exit status 0:

```
PETSC_OPTIONS="-phreeqc_petsc_statistics -phreeqc_petsc_compare_with_cl1 -phreeqc_petsc_comparison_log petsc_cl1_step_comparison.log" ../../../build/petsc_on/phreeqc ../../../tests/petsc_linear_solver/inputs/ex1_seawater_speciation.pqi ex1_seawater_speciation.out ../database/phreeqc.dat ex1_seawater_speciation.log 2> stderr.txt
```

Captured `stderr` (`tests/petsc_linear_solver/results/step4_ex1_comparison_mode/stderr.txt`):

```
phreeqc_petsc_ineligible: not_first_attempt=0 removing_unstable_phases=0 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=0
phreeqc_petsc_statistics: solves=8 accepted=8 cl1_fallbacks=0 non_finite_inputs=0 petsc_errors=0 ineligible_systems=0 solver_slots_created=1
```

Summary command:

```
python3 tests/petsc_linear_solver/tools/summarize_petsc_cl1_step_comparison.py tests/petsc_linear_solver/results/step4_ex1_comparison_mode/petsc_cl1_step_comparison.log
```

Full output (exit status 0):

```
comparison records: 8
  LINEAR_SOLVE_ACCEPTED: 8
  cl1 return codes: {1: 8}
  system sizes: {16: 8}

accepted PETSc steps: normwise relative difference from the cl1 step
  median=2.131e-16  90th percentile=2.484e-16  99th percentile=2.484e-16  maximum=5.468e-16
  backward error: median=4.086e-17  maximum=6.005e-17
```

No sign-inversion flag and no large-difference group appears in the output.

Byte comparison (exit status 0; `cmp` prints nothing for identical files):

```
cmp tests/petsc_linear_solver/baseline/ex1_seawater_speciation.sel tests/petsc_linear_solver/results/step4_ex1_comparison_mode/ex1_seawater_speciation.sel
```

Main-output comparison (exit status 0):

```
python3 tests/petsc_linear_solver/tools/compare_main_output_to_baseline.py tests/petsc_linear_solver/baseline/ex1_seawater_speciation.out tests/petsc_linear_solver/results/step4_ex1_comparison_mode/ex1_seawater_speciation.out
```

```
IDENTICAL  compared_lines=355 ignored_run_time_lines baseline=3 new=3
```

## Step 5 — ex2, comparison mode

The ex2 run writes two `.sel` files (`ex2_gypsum_anhydrite_equilibria.sel` and `ex2.sel`); both were compared (reviewer confirmed the specification's "three" was an error).

Run (working directory `build/petsc_on/step5_ex2_comparison_mode`), exit status 0:

```
PETSC_OPTIONS="-phreeqc_petsc_statistics -phreeqc_petsc_compare_with_cl1 -phreeqc_petsc_comparison_log petsc_cl1_step_comparison.log" ../../../build/petsc_on/phreeqc ../../../tests/petsc_linear_solver/inputs/ex2_gypsum_anhydrite_equilibria.pqi ex2_gypsum_anhydrite_equilibria.out ../database/phreeqc.dat ex2_gypsum_anhydrite_equilibria.log 2> stderr.txt
```

Captured `stderr` (`tests/petsc_linear_solver/results/step5_ex2_comparison_mode/stderr.txt`):

```
phreeqc_petsc_ineligible: not_first_attempt=3 removing_unstable_phases=8 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=1045
phreeqc_petsc_statistics: solves=3 accepted=3 cl1_fallbacks=0 non_finite_inputs=0 petsc_errors=0 ineligible_systems=1056 solver_slots_created=1
```

Summary command:

```
python3 tests/petsc_linear_solver/tools/summarize_petsc_cl1_step_comparison.py tests/petsc_linear_solver/results/step5_ex2_comparison_mode/petsc_cl1_step_comparison.log
```

Full output (exit status 0):

```
comparison records: 3
  LINEAR_SOLVE_ACCEPTED: 3
  cl1 return codes: {1: 3}
  system sizes: {2: 3}

accepted PETSc steps: normwise relative difference from the cl1 step
  median=0.000e+00  90th percentile=0.000e+00  99th percentile=0.000e+00  maximum=8.427e-19
  backward error: median=0.000e+00  maximum=0.000e+00
```

No sign-inversion flag and no large-difference group appears in the output.

Byte comparisons (both exit status 0; `cmp` prints nothing for identical files):

```
cmp tests/petsc_linear_solver/baseline/ex2_gypsum_anhydrite_equilibria.sel tests/petsc_linear_solver/results/step5_ex2_comparison_mode/ex2_gypsum_anhydrite_equilibria.sel
cmp tests/petsc_linear_solver/baseline/ex2.sel tests/petsc_linear_solver/results/step5_ex2_comparison_mode/ex2.sel
```

Main-output comparison (exit status 0):

```
python3 tests/petsc_linear_solver/tools/compare_main_output_to_baseline.py tests/petsc_linear_solver/baseline/ex2_gypsum_anhydrite_equilibria.out tests/petsc_linear_solver/results/step5_ex2_comparison_mode/ex2_gypsum_anhydrite_equilibria.out
```

```
IDENTICAL  compared_lines=4369 ignored_run_time_lines baseline=3 new=3
```

## Step 6 — Memory check

### 6a. PETSc allocation tracking (`-malloc_debug -malloc_dump`)

Runs (working directory `build/petsc_on/step6_memory_check`), both exit status 0:

```
PETSC_OPTIONS="-phreeqc_petsc_statistics -malloc_debug -malloc_dump" ../../../build/petsc_on/phreeqc ../../../tests/petsc_linear_solver/inputs/ex1_seawater_speciation.pqi ex1_seawater_speciation.out ../database/phreeqc.dat ex1_seawater_speciation.log 1> stdout_ex1.txt 2> stderr_ex1.txt
PETSC_OPTIONS="-phreeqc_petsc_statistics -malloc_debug -malloc_dump" ../../../build/petsc_on/phreeqc ../../../tests/petsc_linear_solver/inputs/ex2_gypsum_anhydrite_equilibria.pqi ex2_gypsum_anhydrite_equilibria.out ../database/phreeqc.dat ex2_gypsum_anhydrite_equilibria.log 1> stdout_ex2.txt 2> stderr_ex2.txt
```

Full captured output (copies in `tests/petsc_linear_solver/results/step6_memory_check/`):

`stdout_ex1.txt` — empty (0 bytes).

`stderr_ex1.txt`:

```
phreeqc_petsc_ineligible: not_first_attempt=0 removing_unstable_phases=0 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=0
phreeqc_petsc_statistics: solves=8 accepted=8 cl1_fallbacks=0 non_finite_inputs=0 petsc_errors=0 ineligible_systems=0 solver_slots_created=1
```

`stdout_ex2.txt` — empty (0 bytes).

`stderr_ex2.txt`:

```
phreeqc_petsc_ineligible: not_first_attempt=3 removing_unstable_phases=8 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=1045
phreeqc_petsc_statistics: solves=3 accepted=3 cl1_fallbacks=0 non_finite_inputs=0 petsc_errors=0 ineligible_systems=1056 solver_slots_created=1
```

No line reporting memory still allocated at `PetscFinalize` appears in either run; `-malloc_dump` printed nothing. The D16 exception (`PetscFPTrapPush()` in `src/sys/error/fp.c`) did not arise; it was also not applicable, since both statistics lines show `petsc_errors=0`.

### 6b. AddressSanitizer

Configure (exit status 0), exactly as specified:

```
cmake -S . -B build/petsc_on_asan -DCMAKE_BUILD_TYPE=Release -DPHREEQC_USE_PETSC=ON "-DCMAKE_CXX_FLAGS=-fsanitize=address -fno-omit-frame-pointer" "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address"
```

Output ends:

```
-- Checking for module 'PETSc'
--   Found PETSc, version 3.26.0
-- Configuring done (0.9s)
-- Generating done (0.0s)
-- Build files have been written to: /home/chen/phreeqc3/build/petsc_on_asan
```

Build (exit status 0):

```
cmake --build build/petsc_on_asan --target phreeqc -j 8
```

The build completed (`[100%] Built target phreeqc`). It emitted two compiler warnings, both `-Wformat-truncation` "null destination pointer" for `snprintf` inlined from `valrec PBasic::factor(LOC_exec*)` at `src/PBasic.cpp:4399` and `src/PBasic.cpp:4356`. These are compiler warnings in `PBasic.cpp`, not AddressSanitizer output.

Runs (working directory `build/petsc_on_asan/step6_memory_check`):

```
ASAN_OPTIONS=detect_leaks=1 PETSC_OPTIONS="-phreeqc_petsc_statistics" ../../../build/petsc_on_asan/phreeqc ../../../tests/petsc_linear_solver/inputs/ex1_seawater_speciation.pqi ex1_seawater_speciation.out ../database/phreeqc.dat ex1_seawater_speciation.log 1> asan_stdout_ex1.txt 2> asan_stderr_ex1.txt
```

ex1 exit status: 0.

```
ASAN_OPTIONS=detect_leaks=1 PETSC_OPTIONS="-phreeqc_petsc_statistics" ../../../build/petsc_on_asan/phreeqc ../../../tests/petsc_linear_solver/inputs/ex2_gypsum_anhydrite_equilibria.pqi ex2_gypsum_anhydrite_equilibria.out ../database/phreeqc.dat ex2_gypsum_anhydrite_equilibria.log 1> asan_stdout_ex2.txt 2> asan_stderr_ex2.txt
```

ex2 exit status: 0.

Full sanitizer output (copies in `tests/petsc_linear_solver/results/step6_memory_check/` with the `asan_` prefix):

`asan_stdout_ex1.txt` and `asan_stdout_ex2.txt` — empty (0 bytes).

`asan_stderr_ex1.txt`:

```
phreeqc_petsc_ineligible: not_first_attempt=0 removing_unstable_phases=0 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=0
phreeqc_petsc_statistics: solves=8 accepted=8 cl1_fallbacks=0 non_finite_inputs=0 petsc_errors=0 ineligible_systems=0 solver_slots_created=1
```

`asan_stderr_ex2.txt`:

```
phreeqc_petsc_ineligible: not_first_attempt=3 removing_unstable_phases=8 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=1045
phreeqc_petsc_statistics: solves=3 accepted=3 cl1_fallbacks=0 non_finite_inputs=0 petsc_errors=0 ineligible_systems=1056 solver_slots_created=1
```

No AddressSanitizer error appears in either run. No LeakSanitizer report appears in either run; there is therefore no leak whose stack contains a function from `src/petsc_linear_solver/` or `Phreeqc::ineq`, and no other leak report to list.

## Step 7 — OFF build regression

Configure and build (decision D12; both exit status 0; the tree already existed and was current, so the build reported `[100%] Built target phreeqc` without compiling):

```
cmake -S . -B build/petsc_off -DCMAKE_BUILD_TYPE=Release -DPHREEQC_USE_PETSC=OFF
cmake --build build/petsc_off --target phreeqc -j 8
```

Runs (working directory `build/petsc_off/step7_off_build_regression`), both exit status 0; the captured `stderr` files (`stderr_ex1.txt`, `stderr_ex2.txt`, copied to the results directory) are empty (0 bytes):

```
../../../build/petsc_off/phreeqc ../../../tests/petsc_linear_solver/inputs/ex1_seawater_speciation.pqi ex1_seawater_speciation.out ../database/phreeqc.dat ex1_seawater_speciation.log 2> stderr_ex1.txt
../../../build/petsc_off/phreeqc ../../../tests/petsc_linear_solver/inputs/ex2_gypsum_anhydrite_equilibria.pqi ex2_gypsum_anhydrite_equilibria.out ../database/phreeqc.dat ex2_gypsum_anhydrite_equilibria.log 2> stderr_ex2.txt
```

Byte comparisons of every `.sel` file (all exit status 0; `cmp` prints nothing for identical files):

```
cmp tests/petsc_linear_solver/baseline/ex1_seawater_speciation.sel tests/petsc_linear_solver/results/step7_off_build_regression/ex1_seawater_speciation.sel
cmp tests/petsc_linear_solver/baseline/ex2_gypsum_anhydrite_equilibria.sel tests/petsc_linear_solver/results/step7_off_build_regression/ex2_gypsum_anhydrite_equilibria.sel
cmp tests/petsc_linear_solver/baseline/ex2.sel tests/petsc_linear_solver/results/step7_off_build_regression/ex2.sel
```

Main-output comparisons (both exit status 0):

```
python3 tests/petsc_linear_solver/tools/compare_main_output_to_baseline.py tests/petsc_linear_solver/baseline/ex1_seawater_speciation.out tests/petsc_linear_solver/results/step7_off_build_regression/ex1_seawater_speciation.out
```

```
IDENTICAL  compared_lines=355 ignored_run_time_lines baseline=3 new=3
```

```
python3 tests/petsc_linear_solver/tools/compare_main_output_to_baseline.py tests/petsc_linear_solver/baseline/ex2_gypsum_anhydrite_equilibria.out tests/petsc_linear_solver/results/step7_off_build_regression/ex2_gypsum_anhydrite_equilibria.out
```

```
IDENTICAL  compared_lines=4369 ignored_run_time_lines baseline=3 new=3
```

## Observations (statements of fact)

- **Statistics that differ from the expected values.** None. Step 2: `solves=8 accepted=8 cl1_fallbacks=0 non_finite_inputs=0 petsc_errors=0 ineligible_systems=0`, all ineligibility counters 0. Step 3: `ineligible_unknown_type=1045 > 0` as expected; `solves=3 accepted=3 cl1_fallbacks=0 non_finite_inputs=0 petsc_errors=0 ineligible_systems=1056`; the per-reason counts (1045 pure-phase, 8 unstable-phase-removal, 3 not-first-attempt) equal the D18 record. The statistics lines in steps 4, 5, 6a, and 6b are identical to those of steps 2 and 3 for the same input and option set.
- **Columns that failed the tolerance comparison.** None. All three `compare_selected_output_to_baseline.py` runs (steps 2 and 3) report PASS with 0 failures and `largest_abs_diff = largest_rel_diff = 0.000e+00` in every column of every table.
- **Sign inversion or large differences flagged by `summarize_petsc_cl1_step_comparison.py`.** None in either mode. Step 4 (ex1): 8 records, all `LINEAR_SOLVE_ACCEPTED`, relative difference median 2.131e-16 and maximum 5.468e-16, backward error maximum 6.005e-17; no sign-inversion flag and no large-difference group. Step 5 (ex2): 3 records, all `LINEAR_SOLVE_ACCEPTED`, relative difference median 0.000e+00 and maximum 8.427e-19; no flags.
- **Iteration-count differences between PETSc and baseline.** None. ex1: 8 (baseline) vs 8 (step 2). ex2: the 52-value sequences extracted from the baseline `.out` and the step 3 `.out` are identical, and no line matched the `(N overall)` form in either file.
- In comparison mode (steps 4 and 5) the `.sel` files are byte-identical to the baseline and the `.out` files compare `IDENTICAL` with only the run-time lines ignored (3 per file), consistent with D11.
- The ASAN build emitted two pre-existing `-Wformat-truncation` compiler warnings in `src/PBasic.cpp` (lines 4356, 4399); no AddressSanitizer or LeakSanitizer diagnostic appeared in any ASAN run.
- `-malloc_dump` printed no allocation line in any step 6a run; the D16 `PetscFPTrapPush` exception never arose.

## Artifacts created

- `tests/petsc_linear_solver/results/step2_ex1_petsc_steering/`: `ex1_seawater_speciation.out`, `ex1_seawater_speciation.sel`, `stderr.txt`, `selected_output_comparison.json`
- `tests/petsc_linear_solver/results/step3_ex2_eligibility_gate/`: `ex2_gypsum_anhydrite_equilibria.out`, `ex2_gypsum_anhydrite_equilibria.sel`, `ex2.sel`, `stderr.txt`, `selected_output_comparison_ex2_gypsum_anhydrite_equilibria.json`, `selected_output_comparison_ex2.json`
- `tests/petsc_linear_solver/results/step4_ex1_comparison_mode/`: `ex1_seawater_speciation.out`, `ex1_seawater_speciation.sel`, `petsc_cl1_step_comparison.log`, `stderr.txt`
- `tests/petsc_linear_solver/results/step5_ex2_comparison_mode/`: `ex2_gypsum_anhydrite_equilibria.out`, `ex2_gypsum_anhydrite_equilibria.sel`, `ex2.sel`, `petsc_cl1_step_comparison.log`, `stderr.txt`
- `tests/petsc_linear_solver/results/step6_memory_check/`: ex1/ex2 `.out` and `.sel` from the `petsc_on` malloc runs, `stdout_ex1.txt`, `stderr_ex1.txt`, `stdout_ex2.txt`, `stderr_ex2.txt`, and the ASAN-run copies prefixed `asan_` (`asan_ex1_seawater_speciation.out/.sel`, `asan_ex2_gypsum_anhydrite_equilibria.out/.sel`, `asan_ex2.sel`, `asan_stdout_ex1.txt`, `asan_stderr_ex1.txt`, `asan_stdout_ex2.txt`, `asan_stderr_ex2.txt`)
- `tests/petsc_linear_solver/results/step7_off_build_regression/`: ex1/ex2 `.out` and `.sel`, `stderr_ex1.txt`, `stderr_ex2.txt`
- Run directories: `build/petsc_on/step2_ex1_petsc_steering`, `build/petsc_on/step3_ex2_eligibility_gate`, `build/petsc_on/step4_ex1_comparison_mode`, `build/petsc_on/step5_ex2_comparison_mode`, `build/petsc_on/step6_memory_check`, `build/petsc_on_asan` (with `step6_memory_check`), `build/petsc_off/step7_off_build_regression`
- This report: `docs/petsc_linear_solver/task5_validation_report.md`
