# Phase 1B Task 0: Linear-Solve Share Profile — `co2_basalt_profiling.pqi`

Task specification: `docs/petsc_linear_solver/implementation_tasks/phase1b_task0_profile_cl1_share.md`.
No source file, build file, test input, baseline, or helper script was changed. All run output is under `build/`.

## Input

| File | Size (bytes) |
|---|---|
| `build/profiling_inputs/co2_basalt_profiling.pqi` | 5275 |
| `build/profiling_inputs/phreeqc.dat` | 54151 |

Keyword-line counts in the input (`grep -c -i "^ *<KEYWORD>"`):

| Keyword | Count |
|---|---|
| `SOLUTION` | 3 |
| `EQUILIBRIUM_PHASES` | 2 |
| `GAS_PHASE` | 1 |
| `EXCHANGE` | 1 |
| `SURFACE` | 0 |
| `KINETICS` | 2 |
| `SOLID_SOLUTIONS` | 0 |
| `TRANSPORT` | 1 |
| `ADVECTION` | 0 |

## Environment

- CPU model (`lscpu | grep "Model name"`): `Model name:                              INTEL(R) XEON(R) GOLD 6530`
- PETSc: 3.26.0. `PETSC_DIR=/opt/petsc/3.26.0`, `PETSC_ARCH` empty. `$PETSC_DIR/include/petscversion.h`: `PETSC_VERSION_MAJOR 3`, `PETSC_VERSION_MINOR 26`, `PETSC_VERSION_SUBMINOR 0`. The PETSc log line in step 3 reads `Using PETSc Release Version 3.26.0, unknown`.
- Compiler: `c++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0` / `g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0`. Every tree configured with `-DCMAKE_BUILD_TYPE=Release`.
- Profiler used: **gprof** (`GNU gprof (GNU Binutils for Ubuntu) 2.46`). `perf` is installed at `/usr/bin/perf`, but `/proc/sys/kernel/perf_event_paranoid` is `4`, above the threshold of 2 stated in step 0, so the `perf` branch of step 5 was not used.

## Summary table

| Quantity | Value (3 s.f.) |
|---|---|
| `S` — inclusive share of the linear-solve step | 0.575 |
| `S_cl1` — inclusive share of `cl1` | 0.554 |
| `amdahl_ceiling = 1 / (1 - S)` | 2.35 |
| `ineq_calls = solves + ineligible_systems` | 1.43e+06 (exact 1433110) |
| `eligible_fraction = solves / ineq_calls` | 1.74e-05 |
| `cl1_seconds_per_call = S * median_OFF_wall / ineq_calls` | 1.36e-05 s |
| `petsc_kspsolve_seconds_per_call = KSPSolve max / KSPSolve count` | 2.94e-06 s |
| `on_vs_off_wall_ratio = median_ON_wall / median_OFF_wall` | 1.00 (unrounded 1.00426) |

`S` is taken from `Phreeqc::ineq_cl1_original`, not from `Phreeqc::ineq`: `Phreeqc::ineq` does not appear anywhere in the profile (step 5).

### Inputs to each formula

- `S = 57.5 / 100 = 0.575` — call-graph line `[11]    57.5    0.47   12.73 1430467         Phreeqc::ineq_cl1_original(int) [11]`. Cross-check from the same line: self 0.47 s + children 12.73 s = 13.20 s of the 22.94 s sampled by gprof; 13.20 / 22.94 = 0.5754.
- `S_cl1 = 55.4 / 100 = 0.554` — call-graph line `[12]    55.4   12.72    0.00 1430398         Phreeqc::cl1(...) [12]`. Cross-check: 12.72 / 22.94 = 0.5545.
- `amdahl_ceiling = 1 / (1 - 0.575) = 1 / 0.425 = 2.35294…`
- `ineq_calls = 25 + 1433085 = 1433110` — the single statistics pair of step 2, run 1 (`solves=25`, `ineligible_systems=1433085`).
- `eligible_fraction = 25 / 1433110 = 1.74446e-05`.
- `median_OFF_wall = 33.91947650909424 s` — median of 35.10376000404358, 33.78315544128418, 33.91947650909424 (step 1).
  `cl1_seconds_per_call = 0.575 * 33.91947650909424 / 1433110 = 19.503698992729184 / 1433110 = 1.36094e-05 s`.
- `KSPSolve` maximum time `7.3433e-05 s`, count `25` (step 3 event table): `7.3433e-05 / 25 = 2.93732e-06 s`.
- `median_ON_wall = 34.06381940841675 s` — median of 34.400996923446655, 34.06381940841675, 33.12811017036438 (step 2).
  `on_vs_off_wall_ratio = 34.06381940841675 / 33.91947650909424 = 1.0042555`.

## Step-by-step results

### Step 0 — Pre-checks

```
$ echo "$PETSC_DIR"
/opt/petsc/3.26.0
$ ls -l build/profiling_inputs/
total 64
-rw-r--r-- 1 chen chen  5275 Oct 11 16:30 co2_basalt_profiling.pqi
-rw-rw-r-- 1 chen chen 54151 Oct 11 16:30 phreeqc.dat
$ git status --short
(no output)
$ cmake --build build/petsc_off --target phreeqc -j 8
[100%] Built target phreeqc
$ cmake --build build/petsc_on --target phreeqc -j 8
[  5%] Built target phreeqc_petsc_linear_solver
[100%] Built target phreeqc
$ command -v perf
/usr/bin/perf
$ cat /proc/sys/kernel/perf_event_paranoid
4
```

Both existing trees reported nothing to do. `perf` exists but `perf_event_paranoid` is 4, greater than 2, so `perf` is not usable under the rule in step 0; step 5 used the gprof branch.

### Step 1 — Reference timing, OFF build

Directory `build/petsc_off/profiling` created. Command, run three times:

```
start=$(date +%s.%N); ../../../build/petsc_off/phreeqc ../../profiling_inputs/co2_basalt_profiling.pqi profiling.out ../../profiling_inputs/phreeqc.dat profiling.log; status=$?; end=$(date +%s.%N); python3 -c "print('wall_seconds', $end - $start, 'exit', $status)"
```

| Run | wall_seconds | exit |
|---|---|---|
| 1 | 35.10376000404358 | 0 |
| 2 | 33.78315544128418 | 0 |
| 3 | 33.91947650909424 | 0 |

Median OFF wall time: **33.91947650909424 s**, below the 900 s stop threshold, so the later steps were run. Outputs of the last run are kept in `build/petsc_off/profiling/` (`profiling.out`, `profiling.log`, `phreeqc.log`, `co2_basalt_profiling.sel`).

### Step 2 — Timing and coverage, ON build

Directory `build/petsc_on/profiling` created. Command, run three times with `<k>` = 1, 2, 3:

```
start=$(date +%s.%N); PETSC_OPTIONS="-phreeqc_petsc_statistics" ../../../build/petsc_on/phreeqc ../../profiling_inputs/co2_basalt_profiling.pqi profiling.out ../../profiling_inputs/phreeqc.dat profiling.log 2> stderr_run<k>.txt; status=$?; end=$(date +%s.%N); python3 -c "print('wall_seconds', $end - $start, 'exit', $status)"
```

| Run | wall_seconds | exit |
|---|---|---|
| 1 | 34.400996923446655 | 0 |
| 2 | 34.06381940841675 | 0 |
| 3 | 33.12811017036438 | 0 |

Median ON wall time: **34.06381940841675 s**.

`build/petsc_on/profiling/stderr_run1.txt` contains exactly two lines — one pair, the input runs one `Phreeqc` object:

```
phreeqc_petsc_ineligible: not_first_attempt=20858 removing_unstable_phases=79 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=1412148
phreeqc_petsc_statistics: solves=25 accepted=25 cl1_fallbacks=0 non_finite_inputs=0 petsc_errors=0 ineligible_systems=1433085 solver_slots_created=2
```

`stderr_run2.txt` and `stderr_run3.txt` contain the same two lines.

### Step 3 — PETSc event log, ON build

Directory `build/petsc_on/profiling_log_view` created. One untimed run:

```
PETSC_OPTIONS="-phreeqc_petsc_statistics -log_view" ../../../build/petsc_on/phreeqc ../../profiling_inputs/co2_basalt_profiling.pqi profiling.out ../../profiling_inputs/phreeqc.dat profiling.log 1> stdout.txt 2> stderr.txt
```

Exit status 0. Event-table header lines from `stdout.txt`:

```
Event                Count      Time (sec)     Flop                              --- Global ---  --- Stage ----  Total
                   Max Ratio  Max     Ratio   Max  Ratio  Mess   AvgLen  Reduct  %T %F %M %L %R  %T %F %M %L %R Mflop/s
------------------------------------------------------------------------------------------------------------------------

--- Event Stage 0: Main Stage
```

Requested events:

| Event | Present | Count | Max time (s) |
|---|---|---|---|
| `KSPSolve` | yes | 25 | 7.3433e-05 |
| `PCSetUp` | yes | 25 | 2.2364e-04 |
| `MatLUFactorNum` | **absent** | — | — |
| `MatSolve` | yes | 25 | 4.8161e-05 |
| `MatAssemblyBegin` | yes | 25 | 2.1458e-06 |
| `MatAssemblyEnd` | yes | 25 | 1.9073e-06 |
| `MatMult` | yes | 25 | 2.0742e-05 |
| `VecNorm` | yes | 75 | 1.5736e-05 |

Verbatim lines for the events that appear, including `MatLUFactor`, the LU-factorisation event that is logged instead of `MatLUFactorNum`:

```
MatMult               25 1.0 2.0742e-05 1.0 4.08e+03 1.0 0.0e+00 0.0e+00 0.0e+00  0 17  0  0  0   0 17  0  0  0   197
MatSolve              25 1.0 4.8161e-05 1.0 4.08e+03 1.0 0.0e+00 0.0e+00 0.0e+00  0 17  0  0  0   0 17  0  0  0    85
MatLUFactor           25 1.0 1.3089e-04 1.0 1.34e+04 1.0 0.0e+00 0.0e+00 0.0e+00  0 56  0  0  0   0 56  0  0  0   102
MatAssemblyBegin      25 1.0 2.1458e-06 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
MatAssemblyEnd        25 1.0 1.9073e-06 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
VecNorm               75 1.0 1.5736e-05 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
KSPSolve              25 1.0 7.3433e-05 1.0 4.08e+03 1.0 0.0e+00 0.0e+00 0.0e+00  0 17  0  0  0   0 17  0  0  0    56
PCSetUp               25 1.0 2.2364e-04 1.0 1.34e+04 1.0 0.0e+00 0.0e+00 0.0e+00  0 56  0  0  0   0 56  0  0  0    60
```

Other events in the same table, not requested: `VecCopy` (25), `VecAYPX` (25), `KSPSetUp` (25), `PCApply` (25). Object counts: `Matrix 4/4`, `Vector 6/6`, `Krylov Solver 2/2`, `Preconditioner 2/2`. Stage summary: `0:      Main Stage: 3.7421e+01 100.0%`. `stderr.txt` holds the same two statistics lines as step 2.

### Step 4 — Sanity comparison, ON against OFF (information only)

The input writes one `.sel` file, `co2_basalt_profiling.sel`.

```
$ python3 tests/petsc_linear_solver/tools/compare_selected_output_to_baseline.py build/petsc_off/profiling/co2_basalt_profiling.sel build/petsc_on/profiling/co2_basalt_profiling.sel
PASS  rows=52 columns=35 relative_tolerance=1e-06 absolute_tolerance_amount=1e-14 absolute_tolerance_log=1e-06
```

Exit code 0. No column has a failure; the per-column largest differences (all `failures=0`) are:

```
state                       text             0.000e+00         0.000e+00         0     -
soln                        amount           0.000e+00         0.000e+00         0     -
dist_x                      amount           0.000e+00         0.000e+00         0     -
time                        amount           0.000e+00         0.000e+00         0     -
step                        amount           0.000e+00         0.000e+00         0     -
pH                          log              5.000e-12         9.575e-13         0    51
mu                          amount           7.000e-13         3.408e-12         0    51
Na                          amount           1.000e-13         9.997e-13         0    48
Ca                          amount           9.000e-14         7.139e-12         0    51
Mg                          amount           2.300e-13         8.191e-12         0    51
Si                          amount           3.000e-16         5.076e-12         0    51
Al                          amount           4.900e-19         1.500e-11         0    51
C                           amount           1.170e-11         1.787e-11         0    51
Calcite                     amount           1.000e-13         7.568e-13         0    52
d_Calcite                   amount           5.000e-15         4.302e-12         0    51
Magnesite                   amount           5.000e-13         6.006e-13         0    51
d_Magnesite                 amount           3.980e-13         1.655e-10         0    52
Chalcedony                  amount           2.000e-13         1.103e-12         0    48
d_Chalcedony                amount           1.800e-14         7.554e-12         0    51
Kaolinite                   amount           1.001e-15         1.201e-13         0    47
d_Kaolinite                 amount           9.996e-17         1.938e-13         0    51
si_Calcite                  log              0.000e+00         0.000e+00         0     -
si_Magnesite                log              0.000e+00         0.000e+00         0     -
si_Chalcedony               log              2.000e-12         7.349e-13         0    45
si_Kaolinite                log              8.882e-15         2.000e+00         0    52
pressure                    amount           0.000e+00         0.000e+00         0     -
total mol                   amount           0.000e+00         0.000e+00         0     -
volume                      amount           0.000e+00         0.000e+00         0     -
g_CO2(g)                    amount           0.000e+00         0.000e+00         0     -
k_Forsterite                amount           1.000e-13         6.976e-13         0    48
dk_Forsterite               amount           6.000e-15         5.237e-12         0    51
k_Diopside                  amount           9.998e-14         2.928e-13         0    52
dk_Diopside                 amount           5.400e-15         8.729e-12         0    51
k_Anorthite                 amount           0.000e+00         0.000e+00         0     -
dk_Anorthite                amount           3.000e-16         5.815e-13         0    51
```

```
$ python3 tests/petsc_linear_solver/tools/compare_main_output_to_baseline.py build/petsc_off/profiling/profiling.out build/petsc_on/profiling/profiling.out
IDENTICAL  compared_lines=101 ignored_run_time_lines baseline=3 new=3
```

Exit code 0. That single line is the whole output (fewer than 10 lines).

### Step 5 — Profile the OFF build

Exact symbol for `cl1`:

```
$ grep -n "cl1(" src/Phreeqc.h | head
199:	int cl1(int k, int l, int m, int n,
```

`cl1` is a non-static member of `Phreeqc`, declared at `src/Phreeqc.h:199-204`. The full demangled signature under which it appears in the profile is `Phreeqc::cl1(int, int, int, int, int, int, double*, int*, double, int*, double*, double*, double*, double*, int*, int*, int)`.

Dedicated tree, configured and built as specified:

```
cmake -S . -B build/profile_off -DCMAKE_BUILD_TYPE=Release -DPHREEQC_USE_PETSC=OFF "-DCMAKE_CXX_FLAGS=-g -fno-omit-frame-pointer"
cmake --build build/profile_off --target phreeqc -j 8
```

Both succeeded (`[100%] Built target phreeqc`). `perf` was not usable (step 0), so the gprof branch was used:

```
cmake -S . -B build/profile_off_gprof -DCMAKE_BUILD_TYPE=Release -DPHREEQC_USE_PETSC=OFF "-DCMAKE_CXX_FLAGS=-pg" "-DCMAKE_EXE_LINKER_FLAGS=-pg"
cmake --build build/profile_off_gprof --target phreeqc -j 8
```

Both succeeded (`[100%] Built target phreeqc`). One run in `build/profile_off_gprof/profiling`, same command form with executable `../../../build/profile_off_gprof/phreeqc`, exit status 0, `gmon.out` 1512429 bytes:

```
../../../build/profile_off_gprof/phreeqc ../../profiling_inputs/co2_basalt_profiling.pqi profiling.out ../../profiling_inputs/phreeqc.dat profiling.log
```

```
gprof -b ../../../build/profile_off_gprof/phreeqc gmon.out > gprof_report.txt
```

Exit status 0; `gprof_report.txt` is 5045 lines. gprof's sampled total is 22.94 s: `granularity: each sample hit covers 4 byte(s) for 0.04% of 22.94 seconds`.

First 30 lines of the flat profile:

```
Flat profile:

Each sample counts as 0.01 seconds.
  %   cumulative   self              self     total
 time   seconds   seconds    calls   s/call   s/call  name
 55.45     12.72    12.72  1430398     0.00     0.00  Phreeqc::cl1(int, int, int, int, int, int, double*, int*, double, int*, double*, double*, double*, double*, int*, int*, int)
  4.53     13.76     1.04                             _init
  3.36     14.53     0.77  1408259     0.00     0.00  Phreeqc::jacobian_sums()
  3.05     15.23     0.70  2506007     0.00     0.00  Phreeqc::k_temp(double, double)
  2.22     15.74     0.51  1408259     0.00     0.00  Phreeqc::reset()
  2.05     16.21     0.47  1430467     0.00     0.00  Phreeqc::ineq_cl1_original(int)
  2.01     16.67     0.46 34941033     0.00     0.00  PBasic::upexpr(LOC_exec*)
  1.66     17.05     0.38 71167740     0.00     0.00  void std::vector<int, std::allocator<int> >::_M_realloc_append<int>(int&&)
  1.66     17.43     0.38   365916     0.00     0.00  Phreeqc::model()
  1.22     17.71     0.28  2140091     0.00     0.00  Phreeqc::gammas(double)
  1.09     17.96     0.25  1774244     0.00     0.00  Phreeqc::residuals()
  1.00     18.19     0.23  1776028     0.00     0.00  Phreeqc::mb_sums()
  0.87     18.39     0.20 170340792     0.00     0.00  Phreeqc::calc_delta_v(CReaction&, bool)
  0.87     18.59     0.20 45905258     0.00     0.00  PBasic::factor(LOC_exec*)
  0.87     18.79     0.20   503196     0.00     0.00  Phreeqc::xsolution_save(int)
  0.83     18.98     0.19  2506007     0.00     0.00  Phreeqc::calc_vm(double, double)
  0.74     19.15     0.17  4020207     0.00     0.00  cxxSolution::dump_xml(std::ostream&, unsigned int) const
  0.65     19.30     0.15 11616665     0.00     0.00  Phreeqc::element_store(char const*)
  0.61     19.44     0.14  4553503     0.00     0.00  cxxNameDouble::add_extensive(cxxNameDouble const&, double)
  0.61     19.58     0.14   365915     0.00     0.00  Phreeqc::viscosity(cxxSurface*)
  0.52     19.70     0.12 84047260     0.00     0.00  Phreeqc::strcmp_nocase(char const*, char const*)
  0.52     19.82     0.12  2506009     0.00     0.00  Phreeqc::calc_rho_0(double, double)
  0.52     19.94     0.12   365927     0.00     0.00  Phreeqc::sum_species()
  0.52     20.06     0.12   365913     0.00     0.00  Phreeqc::step(double)
  0.48     20.17     0.11 22773521     0.00     0.00  PBasic::term(LOC_exec*)
  0.44     20.27     0.10  3894603     0.00     0.00  Phreeqc::string_hsave(char const*)
  0.44     20.37     0.10   367769     0.00     0.00  Phreeqc::molalities(int)
```

Call-graph lines for the requested symbols, in the gprof format `index % time    self  children    called     name`:

```
[1]     95.5    0.00   21.90      12         main [1]
[10]    71.6    0.38   16.05  365916         Phreeqc::model() [10]
[11]    57.5    0.47   12.73 1430467         Phreeqc::ineq_cl1_original(int) [11]
[12]    55.4   12.72    0.00 1430398         Phreeqc::cl1(int, int, int, int, int, int, double*, int*, double, int*, double*, double*, double*, double*, int*, int*, int) [12]
[15]     6.0    0.28    1.10 2140091         Phreeqc::gammas(double) [15]
[23]     3.4    0.77    0.00 1408259         Phreeqc::jacobian_sums() [23]
[25]     2.3    0.51    0.01 1408259         Phreeqc::reset() [25]
[36]     1.1    0.25    0.00 1774244         Phreeqc::residuals() [36]
[37]     1.0    0.23    0.00 1776028         Phreeqc::mb_sums() [37]
[58]     0.5    0.10    0.02  367769         Phreeqc::molalities(int) [58]
```

`Phreeqc::ineq`: **absent** from both the flat profile and the call graph. The only `ineq`-prefixed name anywhere in `gprof_report.txt` is `Phreeqc::ineq_cl1_original` (`grep -o "Phreeqc::ineq[a-z_0-9]*(" gprof_report.txt | sort | uniq -c` gives `9 Phreeqc::ineq_cl1_original(`). The symbol itself exists in the instrumented executable — `nm -C build/profile_off_gprof/phreeqc` lists `T Phreeqc::ineq(int)` — and in this OFF build its body is the single statement `return ineq_cl1_original(in_kode);` (`src/model.cpp:1007`, the code outside the `#ifdef USE_PETSC` block), with call sites at `src/model.cpp:161` and `src/model.cpp:178`.

### Step 6 — Derived numbers

Computed in the "Inputs to each formula" section above. Values, rounded to 3 significant figures:

| Quantity | Inputs | Result |
|---|---|---|
| `S` | `Phreeqc::ineq` absent → `Phreeqc::ineq_cl1_original` call-graph `% time` 57.5 | 0.575 |
| `S_cl1` | `Phreeqc::cl1` call-graph `% time` 55.4 | 0.554 |
| `amdahl_ceiling` | `1 / (1 - 0.575)` | 2.35 |
| `ineq_calls` | `25 + 1433085` | 1.43e+06 |
| `eligible_fraction` | `25 / 1433110` | 1.74e-05 |
| `cl1_seconds_per_call` | `0.575 * 33.91947650909424 / 1433110` | 1.36e-05 s |
| `petsc_kspsolve_seconds_per_call` | `7.3433e-05 / 25` | 2.94e-06 s |
| `on_vs_off_wall_ratio` | `34.06381940841675 / 33.91947650909424` | 1.00 |

## Observations

- The OFF build ran this input in 33.8–35.1 s wall time; the median is 33.9 s, below the 900 s stop threshold.
- gprof sampled 22.94 s of CPU time for a run whose wall time was about 34 s; the gprof-instrumented binary is a different executable from the timed OFF binary, and the two totals are not the same quantity.
- `Phreeqc::cl1` is the single largest entry in the flat profile: 55.45 % of sampled time, 12.72 s self, 1430398 calls.
- `Phreeqc::ineq` does not appear in the gprof output at all; `Phreeqc::ineq_cl1_original` appears instead, with 1430467 calls and 57.5 % inclusive. In the OFF build `ineq()` contains only a tail call to `ineq_cl1_original()`.
- `Phreeqc::ineq_cl1_original` (1430467 calls) and `Phreeqc::cl1` (1430398 calls) differ by 69 calls; `Phreeqc::jacobian_sums` and `Phreeqc::reset` were each called 1408259 times.
- `S = 0.575` and `S_cl1 = 0.554` are shares of gprof's sampled CPU time. `cl1_seconds_per_call` multiplies `S` by the wall-time median of the uninstrumented OFF binary, as the specification defines.
- The ON build reported one statistics pair: `solves=25`, `ineligible_systems=1433085`. The ineligibility counters sum to the total: `not_first_attempt=20858`, `removing_unstable_phases=79`, `ineligible_unknown_type=1412148`, all other reasons 0. `ineligible_unknown_type` accounts for 98.5 % of the 1433085 ineligible systems.
- `eligible_fraction = 1.74e-05`: 25 of 1433110 `ineq()` calls used the PETSc path. `accepted=25`, `cl1_fallbacks=0`, `petsc_errors=0`, `non_finite_inputs=0`, `solver_slots_created=2`.
- The PETSc event log records 25 of each of `KSPSolve`, `PCSetUp`, `MatLUFactor`, `MatSolve`, `MatAssemblyBegin`, `MatAssemblyEnd`, `MatMult`, `VecCopy`, `VecAYPX`, `KSPSetUp`, `PCApply`, and 75 `VecNorm`. `MatLUFactorNum` is not in the table; `MatLUFactor` is.
- Total time in the requested PETSc events is small in absolute terms: `KSPSolve` maximum 7.3433e-05 s over 25 calls, `PCSetUp` maximum 2.2364e-04 s over 25 calls. The PETSc stage summary reports 3.7421e+01 s total, 100 % in the main stage, and 2.397e+04 flops for the whole run.
- `petsc_kspsolve_seconds_per_call` (2.94e-06 s) is smaller than `cl1_seconds_per_call` (1.36e-05 s) by a factor of about 4.6. The two numbers come from different runs and different binaries, and the PETSc figure is a maximum time divided by a count.
- `on_vs_off_wall_ratio = 1.00426`. The three ON runs (34.40, 34.06, 33.13 s) and the three OFF runs (35.10, 33.78, 33.92 s) overlap.
- The `.sel` comparison reported PASS with 0 failing columns out of 35; the largest absolute difference in any numeric column was 1.170e-11 (column `C`). The `.out` comparison reported IDENTICAL over 101 compared lines.
- `git status --short` was empty at step 0 and, apart from this report under `docs/`, no repository file was modified; all run output is under `build/`.

