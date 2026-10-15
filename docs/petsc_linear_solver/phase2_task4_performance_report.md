# Phase 2 Task 4: PhreeqcRM Performance Study — Report

Measurement only. No source file in `~/phreeqc3` or `~/phreeqcrm` changed. All runs used the reviewer-supplied driver `tests/petsc_linear_solver/phreeqcrm_benchmark/phreeqcrm_chemistry_benchmark.cpp` with input `co2_basalt_phreeqcrm.pqi` and database `~/phreeqc3/database/phreeqc.dat`.

## 1. Environment (step 0)

- `git status --short` in `~/phreeqc3`: empty. `git -C ~/phreeqcrm status --short`: empty.
- `nproc`: 128
- `lscpu`:
  ```
  Model name:                              INTEL(R) XEON(R) GOLD 6530
  Core(s) per socket:                      32
  Socket(s):                               2
  NUMA node(s):                            2
  ```
- `uptime` (08:48:22): `up 10 days, 23:26, 32 users, load average: 0.14, 0.22, 0.18`
- `squeue`: no jobs.

Stop-rule event during step 4: before series 2 the 1-minute load average read 4.37 (> 4) with `squeue` empty; it was the decaying tail of the just-finished `v390_openmp` series. The agent stopped and reported; the reviewer instructed to proceed. All later series started with the 1-minute load average below 4 (values at the pre-series checks: 0.68, 1.57, 2.44, 1.52, 2.80, 3.82).

## 2. Installations and driver builds (steps 1–2)

`cmake --build ~/phreeqcrm/build-patched-openmp-off -j 8` exited 0 and **rebuilt nothing** (0 compile/link steps; the tree was already up to date with the committed sources; only a cmake re-configure ran).

Installations (`cmake --install <tree> --prefix ~/phreeqcrm_installs/<name>`), all exit 0:

| name | tree | exit |
|---|---|---|
| v390_openmp | ~/phreeqcrm_baseline_src/build-openmp | 0 |
| patched_openmp_off | ~/phreeqcrm/build-patched-openmp-off | 0 |
| patched_openmp_lapack | ~/phreeqcrm/build-patched-openmp-lapack | 0 |
| v390_mpi | ~/phreeqcrm_baseline_src/build-mpi | 0 |
| patched_mpi_lapack | ~/phreeqcrm/build-patched-mpi-lapack | 0 |

Driver builds: `cmake -S tests/petsc_linear_solver/phreeqcrm_benchmark -B build/phreeqcrm_benchmark/<name> -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$HOME/phreeqcrm_installs/<name>` and `cmake --build ... -j 8`. All five configures and all five builds exited 0.

`Found MPI` lines from the two MPI driver configures (identical):

```
-- Found MPI_CXX: /usr/lib/x86_64-linux-gnu/openmpi/lib/libmpi.so (found version "3.1")
-- Found MPI: TRUE (found version "3.1")
```

## 3. Smoke runs (step 3)

50 cells, 2 steps; OpenMP with 2 threads, MPI with `mpiexec -n 2`; `PHREEQC_LINEAR_SOLVER_STATISTICS=summary`. All five exited 0.

`v390_openmp` (stderr empty):
```
benchmark: nxyz=50 threads=2 mpi_tasks=1 steps=2 components=10 setup_seconds=0.038836 initial_run_cells_seconds=0.008072 run_cells_seconds_total=0.543775 run_cells_seconds_median=0.430289
```

`patched_openmp_off` (stderr empty):
```
benchmark: nxyz=50 threads=2 mpi_tasks=1 steps=2 components=10 setup_seconds=0.036959 initial_run_cells_seconds=0.008023 run_cells_seconds_total=0.442525 run_cells_seconds_median=0.372640
```

`patched_openmp_lapack`:
```
benchmark: nxyz=50 threads=2 mpi_tasks=1 steps=2 components=10 setup_seconds=0.034594 initial_run_cells_seconds=0.008041 run_cells_seconds_total=0.535414 run_cells_seconds_median=0.424102
```
stderr:
```
phreeqc_linear_solver_process_summary: rank=0 segment=0 objects=4 backend_lapack_objects=4 backend_petsc_objects=0 backend_cl1_objects=0 solves=67 accepted=67 cl1_fallbacks=0 non_finite_inputs=0 ineligible_systems=36269 ill_conditioned=0 lapack_errors=0 not_first_attempt=421 removing_unstable_phases=0 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=35848
```

`v390_mpi` (stderr empty):
```
benchmark: nxyz=50 threads=1 mpi_tasks=2 steps=2 components=10 setup_seconds=0.032634 initial_run_cells_seconds=0.007985 run_cells_seconds_total=0.403592 run_cells_seconds_median=0.315414
```

`patched_mpi_lapack`:
```
benchmark: nxyz=50 threads=1 mpi_tasks=2 steps=2 components=10 setup_seconds=0.032866 initial_run_cells_seconds=0.007929 run_cells_seconds_total=0.407902 run_cells_seconds_median=0.320983
```
stderr:
```
phreeqc_linear_solver_process_summary: rank=1 segment=0 objects=3 backend_lapack_objects=3 backend_petsc_objects=0 backend_cl1_objects=0 solves=51 accepted=51 cl1_fallbacks=0 non_finite_inputs=0 ineligible_systems=22237 ill_conditioned=0 lapack_errors=0 not_first_attempt=183 removing_unstable_phases=0 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=22054
phreeqc_linear_solver_process_summary: rank=0 segment=0 objects=3 backend_lapack_objects=3 backend_petsc_objects=0 backend_cl1_objects=0 solves=51 accepted=51 cl1_fallbacks=0 non_finite_inputs=0 ineligible_systems=14062 ill_conditioned=0 lapack_errors=0 not_first_attempt=238 removing_unstable_phases=0 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=13824
```

## 4. Timing series (step 4)

Seven series run strictly one after another via `run_phreeqcrm_benchmark_series.sh`, 3 repeats, 1,000 cells, 10 steps, counts 1 4 16 32, all appending to `build/phreeqcrm_benchmark_runs/benchmark_runs.csv`. All 84 runs exited 0. Console outputs:

`v390_openmp`:
```
v390_openmp openmp n=1 repeat=1 exit=0 22.391925 s
v390_openmp openmp n=1 repeat=2 exit=0 21.727496 s
v390_openmp openmp n=1 repeat=3 exit=0 21.167596 s
v390_openmp openmp n=4 repeat=1 exit=0 7.801048 s
v390_openmp openmp n=4 repeat=2 exit=0 7.823864 s
v390_openmp openmp n=4 repeat=3 exit=0 8.227377 s
v390_openmp openmp n=16 repeat=1 exit=0 3.561797 s
v390_openmp openmp n=16 repeat=2 exit=0 3.614330 s
v390_openmp openmp n=16 repeat=3 exit=0 3.794158 s
v390_openmp openmp n=32 repeat=1 exit=0 2.657287 s
v390_openmp openmp n=32 repeat=2 exit=0 2.434410 s
v390_openmp openmp n=32 repeat=3 exit=0 2.189624 s
```

`patched_openmp_off`:
```
patched_openmp_off openmp n=1 repeat=1 exit=0 18.682297 s
patched_openmp_off openmp n=1 repeat=2 exit=0 20.111258 s
patched_openmp_off openmp n=1 repeat=3 exit=0 19.244830 s
patched_openmp_off openmp n=4 repeat=1 exit=0 7.695547 s
patched_openmp_off openmp n=4 repeat=2 exit=0 7.312952 s
patched_openmp_off openmp n=4 repeat=3 exit=0 7.255331 s
patched_openmp_off openmp n=16 repeat=1 exit=0 4.084464 s
patched_openmp_off openmp n=16 repeat=2 exit=0 3.882325 s
patched_openmp_off openmp n=16 repeat=3 exit=0 3.239224 s
patched_openmp_off openmp n=32 repeat=1 exit=0 2.670201 s
patched_openmp_off openmp n=32 repeat=2 exit=0 2.427939 s
patched_openmp_off openmp n=32 repeat=3 exit=0 2.474042 s
```

`lapack_gate` (no linear-solver variables set):
```
lapack_gate openmp n=1 repeat=1 exit=0 19.388568 s
lapack_gate openmp n=1 repeat=2 exit=0 19.456415 s
lapack_gate openmp n=1 repeat=3 exit=0 20.044885 s
lapack_gate openmp n=4 repeat=1 exit=0 7.506855 s
lapack_gate openmp n=4 repeat=2 exit=0 7.664580 s
lapack_gate openmp n=4 repeat=3 exit=0 7.461770 s
lapack_gate openmp n=16 repeat=1 exit=0 3.449437 s
lapack_gate openmp n=16 repeat=2 exit=0 3.763635 s
lapack_gate openmp n=16 repeat=3 exit=0 3.241110 s
lapack_gate openmp n=32 repeat=1 exit=0 2.644329 s
lapack_gate openmp n=32 repeat=2 exit=0 2.350973 s
lapack_gate openmp n=32 repeat=3 exit=0 2.699406 s
```

`lapack_cl1` (`PHREEQC_LINEAR_SOLVER=cl1`):
```
lapack_cl1 openmp n=1 repeat=1 exit=0 19.000460 s
lapack_cl1 openmp n=1 repeat=2 exit=0 19.508260 s
lapack_cl1 openmp n=1 repeat=3 exit=0 19.542826 s
lapack_cl1 openmp n=4 repeat=1 exit=0 7.589318 s
lapack_cl1 openmp n=4 repeat=2 exit=0 7.260806 s
lapack_cl1 openmp n=4 repeat=3 exit=0 7.358685 s
lapack_cl1 openmp n=16 repeat=1 exit=0 3.703230 s
lapack_cl1 openmp n=16 repeat=2 exit=0 3.603246 s
lapack_cl1 openmp n=16 repeat=3 exit=0 4.217256 s
lapack_cl1 openmp n=32 repeat=1 exit=0 2.377315 s
lapack_cl1 openmp n=32 repeat=2 exit=0 2.795327 s
lapack_cl1 openmp n=32 repeat=3 exit=0 2.557553 s
```

`v390_mpi`:
```
v390_mpi mpi n=1 repeat=1 exit=0 19.436488 s
v390_mpi mpi n=1 repeat=2 exit=0 19.706906 s
v390_mpi mpi n=1 repeat=3 exit=0 19.800780 s
v390_mpi mpi n=4 repeat=1 exit=0 7.256963 s
v390_mpi mpi n=4 repeat=2 exit=0 7.035629 s
v390_mpi mpi n=4 repeat=3 exit=0 7.046083 s
v390_mpi mpi n=16 repeat=1 exit=0 3.088785 s
v390_mpi mpi n=16 repeat=2 exit=0 2.912958 s
v390_mpi mpi n=16 repeat=3 exit=0 2.965196 s
v390_mpi mpi n=32 repeat=1 exit=0 2.236936 s
v390_mpi mpi n=32 repeat=2 exit=0 2.063892 s
v390_mpi mpi n=32 repeat=3 exit=0 2.012400 s
```

`mpi_lapack_gate` (no linear-solver variables set):
```
mpi_lapack_gate mpi n=1 repeat=1 exit=0 19.395574 s
mpi_lapack_gate mpi n=1 repeat=2 exit=0 19.468189 s
mpi_lapack_gate mpi n=1 repeat=3 exit=0 20.363700 s
mpi_lapack_gate mpi n=4 repeat=1 exit=0 7.447307 s
mpi_lapack_gate mpi n=4 repeat=2 exit=0 7.058062 s
mpi_lapack_gate mpi n=4 repeat=3 exit=0 6.878243 s
mpi_lapack_gate mpi n=16 repeat=1 exit=0 3.012426 s
mpi_lapack_gate mpi n=16 repeat=2 exit=0 3.048839 s
mpi_lapack_gate mpi n=16 repeat=3 exit=0 2.970988 s
mpi_lapack_gate mpi n=32 repeat=1 exit=0 2.175166 s
mpi_lapack_gate mpi n=32 repeat=2 exit=0 2.027897 s
mpi_lapack_gate mpi n=32 repeat=3 exit=0 2.041348 s
```

`mpi_lapack_cl1` (`PHREEQC_LINEAR_SOLVER=cl1`):
```
mpi_lapack_cl1 mpi n=1 repeat=1 exit=0 19.567134 s
mpi_lapack_cl1 mpi n=1 repeat=2 exit=0 19.902188 s
mpi_lapack_cl1 mpi n=1 repeat=3 exit=0 19.487698 s
mpi_lapack_cl1 mpi n=4 repeat=1 exit=0 8.363132 s
mpi_lapack_cl1 mpi n=4 repeat=2 exit=0 6.989820 s
mpi_lapack_cl1 mpi n=4 repeat=3 exit=0 6.748610 s
mpi_lapack_cl1 mpi n=16 repeat=1 exit=0 2.983772 s
mpi_lapack_cl1 mpi n=16 repeat=2 exit=0 2.956102 s
mpi_lapack_cl1 mpi n=16 repeat=3 exit=0 2.937217 s
mpi_lapack_cl1 mpi n=32 repeat=1 exit=0 2.055530 s
mpi_lapack_cl1 mpi n=32 repeat=2 exit=0 2.018756 s
mpi_lapack_cl1 mpi n=32 repeat=3 exit=0 2.033262 s
```

## 5. Full CSV: `build/phreeqcrm_benchmark_runs/benchmark_runs.csv` (`cat`)

```csv
label,mode,count,repeat,exit_status,run_cells_seconds_total,run_cells_seconds_median,setup_seconds,initial_run_cells_seconds,load_average_1min
v390_openmp,openmp,1,1,0,22.391925,0.904529,0.285259,0.246795,0.78
v390_openmp,openmp,1,2,0,21.727496,0.882160,0.284780,0.246513,1.49
v390_openmp,openmp,1,3,0,21.167596,0.904421,0.288299,0.250016,1.42
v390_openmp,openmp,4,1,0,7.801048,0.407677,0.132417,0.077714,1.37
v390_openmp,openmp,4,2,0,7.823864,0.420085,0.131530,0.077917,1.99
v390_openmp,openmp,4,3,0,8.227377,0.450562,0.099965,0.058693,1.91
v390_openmp,openmp,16,1,0,3.561797,0.280089,0.114626,0.031044,1.99
v390_openmp,openmp,16,2,0,3.614330,0.302197,0.114798,0.028519,1.99
v390_openmp,openmp,16,3,0,3.794158,0.280263,0.117489,0.033245,2.80
v390_openmp,openmp,32,1,0,2.657287,0.266327,0.154268,0.029072,2.80
v390_openmp,openmp,32,2,0,2.434410,0.213115,0.134773,0.022884,5.21
v390_openmp,openmp,32,3,0,2.189624,0.166466,0.137366,0.023064,4.96
patched_openmp_off,openmp,1,1,0,18.682297,0.848635,0.292005,0.250233,0.78
patched_openmp_off,openmp,1,2,0,20.111258,0.912094,0.287762,0.248587,1.12
patched_openmp_off,openmp,1,3,0,19.244830,0.901537,0.286777,0.248079,1.08
patched_openmp_off,openmp,4,1,0,7.695547,0.480283,0.131358,0.077407,1.33
patched_openmp_off,openmp,4,2,0,7.312952,0.473322,0.130565,0.078217,2.06
patched_openmp_off,openmp,4,3,0,7.255331,0.419510,0.115298,0.072416,2.05
patched_openmp_off,openmp,16,1,0,4.084464,0.357136,0.119327,0.030936,2.59
patched_openmp_off,openmp,16,2,0,3.882325,0.309342,0.117993,0.031780,3.67
patched_openmp_off,openmp,16,3,0,3.239224,0.260565,0.115279,0.030497,3.61
patched_openmp_off,openmp,32,1,0,2.670201,0.238212,0.155225,0.030615,3.61
patched_openmp_off,openmp,32,2,0,2.427939,0.236778,0.151857,0.029993,5.09
patched_openmp_off,openmp,32,3,0,2.474042,0.239775,0.151291,0.028158,5.09
lapack_gate,openmp,1,1,0,19.388568,0.863463,0.294701,0.251422,0.82
lapack_gate,openmp,1,2,0,19.456415,0.904056,0.211947,0.181700,1.17
lapack_gate,openmp,1,3,0,20.044885,0.883167,0.287646,0.249117,1.20
lapack_gate,openmp,4,1,0,7.506855,0.441858,0.132216,0.078547,1.21
lapack_gate,openmp,4,2,0,7.664580,0.420144,0.134913,0.079225,1.19
lapack_gate,openmp,4,3,0,7.461770,0.408712,0.133314,0.079593,1.62
lapack_gate,openmp,16,1,0,3.449437,0.257452,0.117243,0.028749,2.37
lapack_gate,openmp,16,2,0,3.763635,0.329284,0.120436,0.032109,2.34
lapack_gate,openmp,16,3,0,3.241110,0.231532,0.104015,0.027231,2.24
lapack_gate,openmp,32,1,0,2.644329,0.236652,0.157598,0.030240,2.24
lapack_gate,openmp,32,2,0,2.350973,0.193931,0.152843,0.029323,2.14
lapack_gate,openmp,32,3,0,2.699406,0.208786,0.155858,0.029402,2.21
lapack_cl1,openmp,1,1,0,19.000460,0.875561,0.294807,0.250701,0.21
lapack_cl1,openmp,1,2,0,19.508260,0.878361,0.191777,0.162883,0.50
lapack_cl1,openmp,1,3,0,19.542826,0.910780,0.235551,0.196904,0.64
lapack_cl1,openmp,4,1,0,7.589318,0.434542,0.116177,0.075169,0.95
lapack_cl1,openmp,4,2,0,7.260806,0.430573,0.117527,0.074648,0.95
lapack_cl1,openmp,4,3,0,7.358685,0.448911,0.132557,0.077675,1.42
lapack_cl1,openmp,16,1,0,3.703230,0.309463,0.117973,0.030888,1.55
lapack_cl1,openmp,16,2,0,3.603246,0.259859,0.109485,0.026573,1.66
lapack_cl1,openmp,16,3,0,4.217256,0.366508,0.117082,0.030649,2.81
lapack_cl1,openmp,32,1,0,2.377315,0.218136,0.158023,0.027732,2.67
lapack_cl1,openmp,32,2,0,2.795327,0.252295,0.156165,0.029411,2.67
lapack_cl1,openmp,32,3,0,2.557553,0.233776,0.159540,0.029759,2.61
v390_mpi,mpi,1,1,0,19.436488,0.911464,0.195327,0.169376,1.52
v390_mpi,mpi,1,2,0,19.706906,0.858190,0.207351,0.178275,1.56
v390_mpi,mpi,1,3,0,19.800780,0.922941,0.217378,0.187552,1.69
v390_mpi,mpi,4,1,0,7.256963,0.430140,0.098211,0.069394,1.75
v390_mpi,mpi,4,2,0,7.035629,0.404217,0.062821,0.045661,2.10
v390_mpi,mpi,4,3,0,7.046083,0.427434,0.081476,0.059159,2.39
v390_mpi,mpi,16,1,0,3.088785,0.266367,0.043539,0.017803,2.52
v390_mpi,mpi,16,2,0,2.912958,0.252984,0.039364,0.018977,3.60
v390_mpi,mpi,16,3,0,2.965196,0.260418,0.038522,0.019184,4.59
v390_mpi,mpi,32,1,0,2.236936,0.195368,0.040917,0.015181,5.59
v390_mpi,mpi,32,2,0,2.063892,0.204383,0.042174,0.016315,8.10
v390_mpi,mpi,32,3,0,2.012400,0.173240,0.037254,0.016168,8.10
mpi_lapack_gate,mpi,1,1,0,19.395574,0.885628,0.182938,0.157632,2.66
mpi_lapack_gate,mpi,1,2,0,19.468189,0.876261,0.217887,0.188582,2.82
mpi_lapack_gate,mpi,1,3,0,20.363700,0.956724,0.215339,0.185407,2.38
mpi_lapack_gate,mpi,4,1,0,7.447307,0.427197,0.097463,0.068895,2.52
mpi_lapack_gate,mpi,4,2,0,7.058062,0.406570,0.083593,0.058336,2.83
mpi_lapack_gate,mpi,4,3,0,6.878243,0.369520,0.081365,0.058932,2.93
mpi_lapack_gate,mpi,16,1,0,3.012426,0.252776,0.049707,0.023188,3.09
mpi_lapack_gate,mpi,16,2,0,3.048839,0.266295,0.048784,0.022514,4.13
mpi_lapack_gate,mpi,16,3,0,2.970988,0.264548,0.047908,0.022208,5.16
mpi_lapack_gate,mpi,32,1,0,2.175166,0.194625,0.042381,0.016072,6.11
mpi_lapack_gate,mpi,32,2,0,2.027897,0.177963,0.037338,0.016027,6.11
mpi_lapack_gate,mpi,32,3,0,2.041348,0.181200,0.045704,0.016161,8.18
mpi_lapack_cl1,mpi,1,1,0,19.567134,0.929769,0.220245,0.189405,0.37
mpi_lapack_cl1,mpi,1,2,0,19.902188,0.938978,0.190697,0.166838,0.86
mpi_lapack_cl1,mpi,1,3,0,19.487698,0.899684,0.217120,0.187270,1.52
mpi_lapack_cl1,mpi,4,1,0,8.363132,0.551883,0.097407,0.068435,1.44
mpi_lapack_cl1,mpi,4,2,0,6.989820,0.374378,0.077206,0.056009,1.83
mpi_lapack_cl1,mpi,4,3,0,6.748610,0.377562,0.076175,0.055489,2.17
mpi_lapack_cl1,mpi,16,1,0,2.983772,0.262384,0.041782,0.019577,2.67
mpi_lapack_cl1,mpi,16,2,0,2.956102,0.265241,0.040648,0.020020,3.74
mpi_lapack_cl1,mpi,16,3,0,2.937217,0.267115,0.039993,0.019902,3.74
mpi_lapack_cl1,mpi,32,1,0,2.055530,0.182344,0.047128,0.016960,3.76
mpi_lapack_cl1,mpi,32,2,0,2.018756,0.181800,0.037049,0.016065,6.18
mpi_lapack_cl1,mpi,32,3,0,2.033262,0.176985,0.036989,0.016018,8.25
```

## 6. Summary outputs (step 5)

`summarize_benchmark_series.py benchmark_runs.csv --reference v390_openmp`:

```
label                        count  runs  failed    median_s     min_s     max_s  max_load
lapack_cl1                       1     3       0      19.508    19.000    19.543      0.64
lapack_cl1                       4     3       0       7.359     7.261     7.589      1.42
lapack_cl1                      16     3       0       3.703     3.603     4.217      2.81
lapack_cl1                      32     3       0       2.558     2.377     2.795      2.67
lapack_gate                      1     3       0      19.456    19.389    20.045      1.20
lapack_gate                      4     3       0       7.507     7.462     7.665      1.62
lapack_gate                     16     3       0       3.449     3.241     3.764      2.37
lapack_gate                     32     3       0       2.644     2.351     2.699      2.24
mpi_lapack_cl1                   1     3       0      19.567    19.488    19.902      1.52
mpi_lapack_cl1                   4     3       0       6.990     6.749     8.363      2.17
mpi_lapack_cl1                  16     3       0       2.956     2.937     2.984      3.74
mpi_lapack_cl1                  32     3       0       2.033     2.019     2.056      8.25
mpi_lapack_gate                  1     3       0      19.468    19.396    20.364      2.82
mpi_lapack_gate                  4     3       0       7.058     6.878     7.447      2.93
mpi_lapack_gate                 16     3       0       3.012     2.971     3.049      5.16
mpi_lapack_gate                 32     3       0       2.041     2.028     2.175      8.18
patched_openmp_off               1     3       0      19.245    18.682    20.111      1.12
patched_openmp_off               4     3       0       7.313     7.255     7.696      2.06
patched_openmp_off              16     3       0       3.882     3.239     4.084      3.67
patched_openmp_off              32     3       0       2.474     2.428     2.670      5.09
v390_mpi                         1     3       0      19.707    19.436    19.801      1.69
v390_mpi                         4     3       0       7.046     7.036     7.257      2.39
v390_mpi                        16     3       0       2.965     2.913     3.089      4.59
v390_mpi                        32     3       0       2.064     2.012     2.237      8.10
v390_openmp                      1     3       0      21.727    21.168    22.392      1.49
v390_openmp                      4     3       0       7.824     7.801     8.227      1.99
v390_openmp                     16     3       0       3.614     3.562     3.794      2.80
v390_openmp                     32     3       0       2.434     2.190     2.657      5.21

ratio of median to reference 'v390_openmp' at the same count:
label                              n=1       n=4      n=16      n=32
lapack_cl1                       0.898     0.941     1.025     1.051
lapack_gate                      0.895     0.959     0.954     1.086
mpi_lapack_cl1                   0.901     0.893     0.818     0.835
mpi_lapack_gate                  0.896     0.902     0.833     0.839
patched_openmp_off               0.886     0.935     1.074     1.016
v390_mpi                         0.907     0.901     0.820     0.848
v390_openmp                      1.000     1.000     1.000     1.000

speed-up relative to the label's own smallest count:
label                              n=1       n=4      n=16      n=32
lapack_cl1                        1.00      2.65      5.27      7.63
lapack_gate                       1.00      2.59      5.64      7.36
mpi_lapack_cl1                    1.00      2.80      6.62      9.62
mpi_lapack_gate                   1.00      2.76      6.46      9.54
patched_openmp_off                1.00      2.63      4.96      7.78
v390_mpi                          1.00      2.80      6.65      9.55
v390_openmp                       1.00      2.78      6.01      8.93
```

`summarize_benchmark_series.py benchmark_runs.csv --reference v390_mpi`:

```
label                        count  runs  failed    median_s     min_s     max_s  max_load
lapack_cl1                       1     3       0      19.508    19.000    19.543      0.64
lapack_cl1                       4     3       0       7.359     7.261     7.589      1.42
lapack_cl1                      16     3       0       3.703     3.603     4.217      2.81
lapack_cl1                      32     3       0       2.558     2.377     2.795      2.67
lapack_gate                      1     3       0      19.456    19.389    20.045      1.20
lapack_gate                      4     3       0       7.507     7.462     7.665      1.62
lapack_gate                     16     3       0       3.449     3.241     3.764      2.37
lapack_gate                     32     3       0       2.644     2.351     2.699      2.24
mpi_lapack_cl1                   1     3       0      19.567    19.488    19.902      1.52
mpi_lapack_cl1                   4     3       0       6.990     6.749     8.363      2.17
mpi_lapack_cl1                  16     3       0       2.956     2.937     2.984      3.74
mpi_lapack_cl1                  32     3       0       2.033     2.019     2.056      8.25
mpi_lapack_gate                  1     3       0      19.468    19.396    20.364      2.82
mpi_lapack_gate                  4     3       0       7.058     6.878     7.447      2.93
mpi_lapack_gate                 16     3       0       3.012     2.971     3.049      5.16
mpi_lapack_gate                 32     3       0       2.041     2.028     2.175      8.18
patched_openmp_off               1     3       0      19.245    18.682    20.111      1.12
patched_openmp_off               4     3       0       7.313     7.255     7.696      2.06
patched_openmp_off              16     3       0       3.882     3.239     4.084      3.67
patched_openmp_off              32     3       0       2.474     2.428     2.670      5.09
v390_mpi                         1     3       0      19.707    19.436    19.801      1.69
v390_mpi                         4     3       0       7.046     7.036     7.257      2.39
v390_mpi                        16     3       0       2.965     2.913     3.089      4.59
v390_mpi                        32     3       0       2.064     2.012     2.237      8.10
v390_openmp                      1     3       0      21.727    21.168    22.392      1.49
v390_openmp                      4     3       0       7.824     7.801     8.227      1.99
v390_openmp                     16     3       0       3.614     3.562     3.794      2.80
v390_openmp                     32     3       0       2.434     2.190     2.657      5.21

ratio of median to reference 'v390_mpi' at the same count:
label                              n=1       n=4      n=16      n=32
lapack_cl1                       0.990     1.044     1.249     1.239
lapack_gate                      0.987     1.065     1.163     1.281
mpi_lapack_cl1                   0.993     0.992     0.997     0.985
mpi_lapack_gate                  0.988     1.002     1.016     0.989
patched_openmp_off               0.977     1.038     1.309     1.199
v390_mpi                         1.000     1.000     1.000     1.000
v390_openmp                      1.103     1.110     1.219     1.180

speed-up relative to the label's own smallest count:
label                              n=1       n=4      n=16      n=32
lapack_cl1                        1.00      2.65      5.27      7.63
lapack_gate                       1.00      2.59      5.64      7.36
mpi_lapack_cl1                    1.00      2.80      6.62      9.62
mpi_lapack_gate                   1.00      2.76      6.46      9.54
patched_openmp_off                1.00      2.63      4.96      7.78
v390_mpi                          1.00      2.80      6.65      9.55
v390_openmp                       1.00      2.78      6.01      8.93
```

## 7. Eligibility in this workload (step 6)

Two runs, both exit 0, `PHREEQC_LINEAR_SOLVER_STATISTICS=summary`:
- `build/phreeqcrm_benchmark_runs/elig_openmp_n4/`: `patched_openmp_lapack` driver, 1,000 cells, 10 steps, 4 threads;
- `build/phreeqcrm_benchmark_runs/elig_mpi_n4/`: `patched_mpi_lapack` driver, `mpiexec -n 4`.

stderr of `elig_openmp_n4` (one summary line):
```
phreeqc_linear_solver_process_summary: rank=0 segment=0 objects=6 backend_lapack_objects=6 backend_petsc_objects=0 backend_cl1_objects=0 solves=99 accepted=99 cl1_fallbacks=0 non_finite_inputs=0 ineligible_systems=1010436 ill_conditioned=0 lapack_errors=0 not_first_attempt=10553 removing_unstable_phases=0 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=999883
```

`sum_linear_solver_statistics.py` on it:
```
phreeqc_linear_solver_process_summary: lines=1
  text fields: rank=0 x1  segment=0 x1
  sums: objects=6  backend_lapack_objects=6  backend_petsc_objects=0  backend_cl1_objects=0  solves=99  accepted=99  cl1_fallbacks=0  non_finite_inputs=0  ineligible_systems=1010436  ill_conditioned=0  lapack_errors=0  not_first_attempt=10553  removing_unstable_phases=0 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=999883
summary lines per rank: rank 0: 1
```

stderr of `elig_mpi_n4` (four summary lines):
```
phreeqc_linear_solver_process_summary: rank=1 segment=0 objects=3 backend_lapack_objects=3 backend_petsc_objects=0 backend_cl1_objects=0 solves=51 accepted=51 cl1_fallbacks=0 non_finite_inputs=0 ineligible_systems=261028 ill_conditioned=0 lapack_errors=0 not_first_attempt=2653 removing_unstable_phases=0 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=258375
phreeqc_linear_solver_process_summary: rank=0 segment=0 objects=3 backend_lapack_objects=3 backend_petsc_objects=0 backend_cl1_objects=0 solves=51 accepted=51 cl1_fallbacks=0 non_finite_inputs=0 ineligible_systems=173739 ill_conditioned=0 lapack_errors=0 not_first_attempt=3830 removing_unstable_phases=0 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=169909
phreeqc_linear_solver_process_summary: rank=2 segment=0 objects=3 backend_lapack_objects=3 backend_petsc_objects=0 backend_cl1_objects=0 solves=51 accepted=51 cl1_fallbacks=0 non_finite_inputs=0 ineligible_systems=282389 ill_conditioned=0 lapack_errors=0 not_first_attempt=1572 removing_unstable_phases=0 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=280817
phreeqc_linear_solver_process_summary: rank=3 segment=0 objects=3 backend_lapack_objects=3 backend_petsc_objects=0 backend_cl1_objects=0 solves=51 accepted=51 cl1_fallbacks=0 non_finite_inputs=0 ineligible_systems=293370 ill_conditioned=0 lapack_errors=0 not_first_attempt=2498 removing_unstable_phases=0 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=290872
```

`sum_linear_solver_statistics.py` on it:
```
phreeqc_linear_solver_process_summary: lines=4
  text fields: rank=0 x1  rank=1 x1  rank=2 x1  rank=3 x1  segment=0 x4
  sums: objects=12  backend_lapack_objects=12  backend_petsc_objects=0  backend_cl1_objects=0  solves=204  accepted=204  cl1_fallbacks=0  non_finite_inputs=0  ineligible_systems=1010526  ill_conditioned=0 lapack_errors=0 not_first_attempt=10553  removing_unstable_phases=0 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=999973
summary lines per rank: rank 0: 1  rank 1: 1  rank 2: 1  rank 3: 1
```

Derived fractions, with inputs:

OpenMP, 4 threads (solves=99, ineligible_systems=1,010,436, ill_conditioned=0):
- `eligible_fraction = solves / (solves + ineligible_systems) = 99 / (99 + 1,010,436) = 99 / 1,010,535 = 9.797e-5 = 0.0098 %`
- `gated_fraction = ill_conditioned / solves = 0 / 99 = 0`

MPI, 4 ranks (solves=204, ineligible_systems=1,010,526, ill_conditioned=0):
- `eligible_fraction = 204 / (204 + 1,010,526) = 204 / 1,010,730 = 2.018e-4 = 0.0202 %`
- `gated_fraction = 0 / 204 = 0`

The dominant ineligibility reason is `ineligible_unknown_type` (999,883 of 1,010,436 OpenMP; 999,973 of 1,010,526 MPI). `cl1_fallbacks=0` and `lapack_errors=0` in both runs. The OpenMP and MPI runs report the same `not_first_attempt` total (10,553) but different solve counts (99 vs 204) and total system counts (1,010,535 vs 1,010,730).

## 8. Results agree (step 7)

Reference: `build/phreeqcrm_benchmark_runs/v390_openmp_n1_r1/bench_concentrations.tsv`. Compared with `compare_selected_output_to_baseline.py` (default tolerances: relative 1e-6, absolute amount 1e-14, absolute log 1e-6) against the repeat-1 count-4 run of every label. All seven comparisons: **PASS**, rows=1000 columns=11, zero failing columns; every column's `largest_abs_diff` and `largest_rel_diff` are 0.000e+00.

Verdicts:

| compared file | verdict | failing columns |
|---|---|---|
| v390_openmp_n4_r1 | PASS (exit 0) | none |
| patched_openmp_off_n4_r1 | PASS (exit 0) | none |
| lapack_gate_n4_r1 | PASS (exit 0) | none |
| lapack_cl1_n4_r1 | PASS (exit 0) | none |
| v390_mpi_n4_r1 | PASS (exit 0) | none |
| mpi_lapack_gate_n4_r1 | PASS (exit 0) | none |
| mpi_lapack_cl1_n4_r1 | PASS (exit 0) | none |

Example of a full comparison output (`v390_openmp_n4_r1`), identical in structure for all seven:
```
PASS  rows=1000 columns=11 relative_tolerance=1e-06 absolute_tolerance_amount=1e-14 absolute_tolerance_log=1e-06
column                      kind      largest_abs_diff  largest_rel_diff  failures   row
cell                        amount           0.000e+00         0.000e+00         0     -
H                           amount           0.000e+00         0.000e+00         0     -
O                           amount           0.000e+00         0.000e+00         0     -
Charge                      amount           0.000e+00         0.000e+00         0     -
Al                          amount           0.000e+00         0.000e+00         0     -
C                           amount           0.000e+00         0.000e+00         0     -
Ca                          amount           0.000e+00         0.000e+00         0     -
Cl                          amount           0.000e+00         0.000e+00         0     -
Mg                          amount           0.000e+00         0.000e+00         0     -
Na                          amount           0.000e+00         0.000e+00         0     -
Si                          amount           0.000e+00         0.000e+00         0     -
```

## 9. Observations (facts only)

### 9.1 Run-to-run spread

Spread per label and count over the three repeats, `(max - min) / median`, from the medians/min/max table in section 6:

| label | n=1 | n=4 | n=16 | n=32 |
|---|---|---|---|---|
| v390_openmp | 5.6 % | 5.4 % | 6.4 % | 19.2 % |
| patched_openmp_off | 7.4 % | 6.0 % | 21.8 % | 9.8 % |
| lapack_gate | 3.4 % | 2.7 % | 15.2 % | 13.2 % |
| lapack_cl1 | 2.8 % | 4.5 % | 16.6 % | 16.3 % |
| v390_mpi | 1.9 % | 3.1 % | 5.9 % | 10.9 % |
| mpi_lapack_gate | 5.0 % | 8.1 % | 2.6 % | 7.2 % |
| mpi_lapack_cl1 | 2.1 % | 23.1 % | 1.6 % | 1.8 % |

### 9.2 Ratios against the references, and whether they lie outside the run-to-run spread

A ratio of medians is called **outside** when the three-repeat run-time ranges of the label and of the reference at that count do not overlap; **inside** when they overlap.

Against `v390_openmp` (ratios from section 6):

| label | n=1 (0.886–0.907) | n=4 (0.893–0.959) | n=16 (0.818–1.074) | n=32 (0.835–1.086) |
|---|---|---|---|---|
| patched_openmp_off | outside (faster) | outside (faster) | inside | inside |
| lapack_gate | outside (faster) | outside (faster) | inside | inside |
| lapack_cl1 | outside (faster) | outside (faster) | inside | inside |
| v390_mpi | outside (faster) | outside (faster) | outside (faster) | inside |
| mpi_lapack_gate | outside (faster) | outside (faster) | outside (faster) | outside (faster) |
| mpi_lapack_cl1 | outside (faster) | inside (range 6.749–8.363 s overlaps) | outside (faster) | outside (faster) |

Against `v390_mpi` (ratios from section 6):

| label | n=1 (0.977–1.103) | n=4 (0.992–1.110) | n=16 (0.997–1.309) | n=32 (0.985–1.281) |
|---|---|---|---|---|
| v390_openmp | outside (slower) | outside (slower) | outside (slower) | inside (range 2.190–2.657 s overlaps) |
| patched_openmp_off | inside | inside (range 7.255–7.696 s overlaps) | outside (slower) | outside (slower) |
| lapack_gate | inside | outside (slower) | outside (slower) | outside (slower) |
| lapack_cl1 | inside | outside (slower, 7.261 s min vs 7.257 s max) | outside (slower) | outside (slower) |
| mpi_lapack_gate | inside | inside | inside | inside |
| mpi_lapack_cl1 | inside | inside | inside | inside |

Facts about these ratios:
- At n=1 every label other than `v390_openmp` has all three repeats in 18.68–20.36 s; all three `v390_openmp` repeats are 21.17–22.39 s. The n=1 ratios 0.886–0.907 lie outside the spread of every label involved.
- The three OpenMP labels built from the patched sources (`patched_openmp_off`, `lapack_gate`, `lapack_cl1`) differ from each other in medians by at most 1.4 % (n=1), 2.6 % (n=4), 11.2 % (n=16), and 6.4 % (n=32), relative to the larger median; every such difference lies inside the run-to-run spread of the two labels involved.
- The three MPI labels (`v390_mpi`, `mpi_lapack_gate`, `mpi_lapack_cl1`) differ from each other in medians by at most 1.9 % at every count (relative to the larger median); every difference lies inside the run-to-run spread.
- The OpenMP-versus-MPI difference at n=16 and n=32 (MPI medians 2.91–3.05 s versus OpenMP medians 3.45–3.88 s at 16; 2.02–2.06 s versus 2.43–2.64 s at 32) is outside the run-to-run spread of the labels involved.

### 9.3 Speed-up with count (from section 6)

- OpenMP labels reach 4.96–6.01x at 16 threads and 7.36–8.93x at 32 threads relative to their own n=1 median.
- MPI labels reach 6.46–6.65x at 16 ranks and 9.54–9.62x at 32 ranks.
- `v390_openmp` itself reaches 6.01x at 16 and 8.93x at 32; `patched_openmp_off` 4.96x and 7.78x; `lapack_gate` 5.64x and 7.36x; `lapack_cl1` 5.27x and 7.63x.

### 9.4 Eligibility counts

- OpenMP 4-thread run: 99 LAPACK solves out of 1,010,535 Newton systems (eligible_fraction 0.0098 %); MPI 4-rank run: 204 out of 1,010,730 (0.0202 %). `ill_conditioned=0` in both, so `gated_fraction = 0`. `cl1_fallbacks=0`, `lapack_errors=0`, `accepted=solves` in both.
- 98.96 % of the ineligible systems carry reason `ineligible_unknown_type` in both runs (999,883 / 1,010,436 and 999,973 / 1,010,526).
- The same workload yields different solve counts under OpenMP (99) and MPI (204), and total system counts differing by 195 (1,010,535 vs 1,010,730), while `not_first_attempt` totals are equal (10,553).

### 9.5 Results

- All seven `bench_concentrations.tsv` comparisons (section 8) are PASS with largest absolute and relative differences 0.000e+00 in every column: the final concentrations of every label at 4 threads/ranks, repeat 1, are identical to the `v390_openmp` 1-thread run at the printed precision.
