# Phase 2 Task 2a: PhreeqcRM Baselines (OpenMP and MPI) — Report

Date: 2026-10-12. Worktree: `~/phreeqcrm_baseline_src` (v3.9.0, per P2-D13).
No source file in any repository was modified. Outputs were written only under
`~/phreeqcrm_baseline_src/build-openmp`, `~/phreeqcrm_baseline_src/build-mpi`,
`~/phreeqcrm_phase2_baseline/v390/`, and this report.

## Step 0 — Pre-checks

```
$ git -C ~/phreeqcrm_baseline_src describe --tags
v3.9.0
```

```
$ git -C ~/phreeqcrm_baseline_src status --short
(no output; exit 0)
```

```
$ command -v mpiexec
/usr/bin/mpiexec
```

```
$ nproc
128
```

```
$ echo "$PETSC_DIR"
/opt/petsc/3.26.0
```

All pre-checks pass. PETSc is not used by any build in this task.

## Step 1 — OpenMP baseline

### Configure

```
$ cmake -S ~/phreeqcrm_baseline_src -B ~/phreeqcrm_baseline_src/build-openmp -DCMAKE_BUILD_TYPE=Release
-- The CXX compiler identification is GNU 15.2.0
-- The C compiler identification is GNU 15.2.0
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Setting CMAKE_POLICY_VERSION_MINIMUM=3.31 for yaml-cpp
-- Found OpenMP_C: -fopenmp (found version "4.5")
-- Found OpenMP_CXX: -fopenmp (found version "4.5")
-- Found OpenMP: TRUE (found version "4.5")
-- Performing Test COMPILER_HAS_HIDDEN_VISIBILITY
-- Performing Test COMPILER_HAS_HIDDEN_VISIBILITY - Success
-- Performing Test COMPILER_HAS_HIDDEN_INLINE_VISIBILITY
-- Performing Test COMPILER_HAS_HIDDEN_INLINE_VISIBILITY - Success
-- Performing Test COMPILER_HAS_DEPRECATED_ATTR
-- Performing Test COMPILER_HAS_DEPRECATED_ATTR - Success
-- SKBUILD is NOT defined
-- Configuring done (1.5s)
-- Generating done (0.0s)
-- Build files have been written to: /home/chen/phreeqcrm_baseline_src/build-openmp
EXIT=0
```

No MPI was found in this configuration (no `Found MPI` lines). OpenMP 4.5 found.

### Build

```
$ cmake --build ~/phreeqcrm_baseline_src/build-openmp -j 8
EXIT=0
```

Capture deviation, stated plainly: the first build's stdout was piped through
`tail -40` before a log file was set up, so only its last 40 lines were
captured (exit 0; final lines below). An attempt to re-run the build from a
clean tree was denied by the environment's permission rules (`rm -rf` denied).
A subsequent no-op build was captured in full:

```
$ cmake --build ~/phreeqcrm_baseline_src/build-openmp -j 8
[ 84%] Built target PhreeqcRM
[ 86%] Built target TestRMdtor
[ 87%] Built target TestBMIdtor
[100%] Built target TestRM
EXIT=0
```

Last 40 lines of the first (full) build, ending in success: targets
`PhreeqcRM` (static library `libPhreeqcRM.a`), `TestRMdtor`, `TestBMIdtor`,
`TestRM` all built; `[100%] Linking CXX executable TestRM`, `[100%] Built
target TestRM`, `EXIT=0`.

### Run 1

```
$ touch ~/phreeqcrm_baseline_src/build-openmp/run1_marker
$ ctest --test-dir ~/phreeqcrm_baseline_src/build-openmp --output-on-failure
Test project /home/chen/phreeqcrm_baseline_src/build-openmp
    Start 1: TestRMdtor
1/3 Test #1: TestRMdtor .......................   Passed    0.00 sec
    Start 2: TestBMIdtor
2/3 Test #2: TestBMIdtor ......................   Passed    0.01 sec
    Start 3: TestRM
3/3 Test #3: TestRM ...........................   Passed    0.24 sec

100% tests passed, 0 tests failed out of 3

Total Test time (real) =   0.26 sec
EXIT=0
```

ctest registers exactly 3 tests in this build (`TestRMdtor`, `TestBMIdtor`,
`TestRM`). Facts from `Tests/main.cpp` (read-only, not modified): the
`AdvectBMI_*`, `TestAllMethods_*`, and YAML tests are inside
`#ifdef USE_YAML` (lines 74–89) and the Fortran tests inside
`#if defined(TEST_FORTRAN)` (line 102); neither macro is defined in this
build, which is why the run produces the 27 files listed below and finishes
in 0.24 s.

Files newer than `run1_marker` (full list, 27 files):

```
Tests/Advect_c.chem.txt
Tests/Advect_c.dmp
Tests/Advect_c.log.txt
Tests/Advect_cpp.chem.txt
Tests/Advect_cpp.dmp
Tests/Advect_cpp.log.txt
Tests/Advect_cpp_units_utility.txt
Tests/Advect_cpp_units_worker.chem.txt
Tests/Advect_cpp_units_worker.log.txt
Tests/Advect_cpp_utility.txt
Tests/Advect_c_utility.txt
Tests/Gas_c.chem.txt
Tests/Gas_c.log.txt
Tests/Gas_cpp.chem.txt
Tests/Gas_cpp.log.txt
Tests/SimpleAdvect_c.chem.txt
Tests/SimpleAdvect_c.log.txt
Tests/SimpleAdvect_cpp.chem.txt
Tests/SimpleAdvect_cpp.log.txt
Tests/Species_c.chem.txt
Tests/Species_c.dmp
Tests/Species_c.log.txt
Tests/Species_cpp.chem.txt
Tests/Species_cpp.dmp
Tests/Species_cpp.log.txt
Tests/Species_cpp_utility.txt
Tests/Species_c_utility.txt
```

(Paths shown relative to `<build>/`; all are flat files directly under
`Tests/`.) Copied with `cp --parents` from inside `Tests/` to
`~/phreeqcrm_phase2_baseline/v390/openmp_run1/`; 27 files present after copy.

### Run 2

```
$ touch ~/phreeqcrm_baseline_src/build-openmp/run2_marker
$ ctest --test-dir ~/phreeqcrm_baseline_src/build-openmp --output-on-failure
Test project /home/chen/phreeqcrm_baseline_src/build-openmp
    Start 1: TestRMdtor
1/3 Test #1: TestRMdtor .......................   Passed    0.01 sec
    Start 2: TestBMIdtor
2/3 Test #2: TestBMIdtor ......................   Passed    0.01 sec
    Start 3: TestRM
3/3 Test #3: TestRM ...........................   Passed    0.31 sec

100% tests passed, 0 tests failed out of 3

Total Test time (real) =   0.32 sec
EXIT=0
```

`find ... -newer run2_marker` returned 27 files; `diff` of the run-1 and run-2
lists printed nothing (`FILE LISTS IDENTICAL`). Copied to
`~/phreeqcrm_phase2_baseline/v390/openmp_run2/`; 27 files present after copy.

## Step 2 — Comparison tools on PhreeqcRM outputs (P2-D6)

For each file present in both runs: `.dmp`/`.log.txt` with `cmp`,
`.chem.txt`/`_utility.txt` with
`python3 ~/phreeqc3/tests/petsc_linear_solver/tools/compare_main_output_to_baseline.py`.
One line per file (`file | tool | result`); `IDENTICAL` for `cmp` means exit 0
(byte-identical).

```
Advect_c.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Advect_c.dmp | cmp | DIFFER
Advect_c.log.txt | cmp | IDENTICAL
Advect_c_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Advect_cpp.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Advect_cpp.dmp | cmp | DIFFER
Advect_cpp.log.txt | cmp | IDENTICAL
Advect_cpp_units_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Advect_cpp_units_worker.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Advect_cpp_units_worker.log.txt | cmp | IDENTICAL
Advect_cpp_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Gas_c.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Gas_c.log.txt | cmp | IDENTICAL
Gas_cpp.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Gas_cpp.log.txt | cmp | IDENTICAL
SimpleAdvect_c.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
SimpleAdvect_c.log.txt | cmp | IDENTICAL
SimpleAdvect_cpp.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
SimpleAdvect_cpp.log.txt | cmp | IDENTICAL
Species_c.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Species_c.dmp | cmp | DIFFER
Species_c.log.txt | cmp | IDENTICAL
Species_c_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Species_cpp.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Species_cpp.dmp | cmp | DIFFER
Species_cpp.log.txt | cmp | IDENTICAL
Species_cpp_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
```

Counts: 22 identical, 5 not identical (4 `.dmp`, 1 `.chem.txt`).

### Differing files, first 20 diff lines each

`Advect_c.dmp` (diff line count: 203):

```
156,186d155
<     SOLUTION_RAW                 5 Solution after simulation 1.
<       -temp                      20
<       -pressure                  2
<       -potential                 0
<       -total_h                   22.136415506019
<       -total_o                   11.068314447391
<       -cb                        9.1761423306653e-14
<       -density                   0.99830599410196
<       -viscosity                 1.0017269354673
<       -viscos_0                  1.001568160569
<       -totals
<         Ca                   2.1235996952813e-06
<         Cl                   0.00023928579660052
<         K                    0.00021202857277532
<         Na                   2.3010024526399e-05
<       -pH                        7.0816583781613
<       -pe                        13.966479570909
<       -mu                        0.0012107356130103
<       -ah2o                      0.9999548300763
```

`Advect_cpp.dmp` (diff line count: 201):

```
348a349,408
>     SOLUTION_RAW                 8 Solution after simulation 1.
>       -temp                      20
>       -pressure                  2
>       -potential                 0
>       -total_h                   22.136415506019
>       -total_o                   11.068314447391
>       -cb                        1.4446771502959e-13
>       -density                   0.9983011293439
>       -viscosity                 1.0018316494637
>       -viscos_0                  1.0015681605689
>       -totals
>         Ca                   3.5591651509235e-24
>         Cl                   0.00023928579659791
>         K                    3.9783056143315e-05
>         Na                   0.00019950274059907
>       -pH                        7.0817104543639
>       -pe                        13.966427267761
>       -mu                        0.0012000859379213
>       -ah2o                      0.99995464903202
```

`Species_c.dmp` (diff line count: 347):

```
351,420d350
<     SOLUTION_RAW                 5 Solution after simulation 1.
<       -temp                      20
<       -pressure                  2
<       -potential                 0
<       -total_h                   22.162948776769
<       -total_o                   11.081581210653
<       -cb                        1.0286510017268e-13
<       -density                   0.99830599745198
<       -viscosity                 1.0017270160145
<       -viscos_0                  1.001568160569
<       -totals
<         Ca                   2.1789424973458e-06
<         Cl                   0.00023957261064348
<         K                    0.0002123205525714
<         Na                   2.2894173180255e-05
<       -pH                        7.0816570859384
<       -pe                        13.966480868764
<       -mu                        0.001211000070613
<       -ah2o                      0.99995483457207
```

`Species_cpp.dmp` (diff line count: 183):

```
351,420d350
<     SOLUTION_RAW                 5 Solution after simulation 1.
<       -temp                      20
<       -pressure                  2
<       -potential                 0
<       -total_h                   22.162948776769
<       -total_o                   11.081581210653
<       -cb                        1.0286510017268e-13
<       -density                   0.99830599745198
<       -viscosity                 1.0017270160145
<       -viscos_0                  1.001568160569
<       -totals
<         Ca                   2.1789424973458e-06
<         Cl                   0.00023957261064348
<         K                    0.0002123205525714
<         Na                   2.2894173180255e-05
<       -pH                        7.0816570859384
<       -pe                        13.966480868764
<       -mu                        0.001211000070613
<       -ah2o                      0.99995483457207
```

`Advect_cpp.chem.txt`, comparison script output (exit 1):

```
DIFFERENT  compared_lines=2460 ignored_run_time_lines baseline=63 new=63
  baseline line 2522: '1           5                 7'
  new      line 2522: '1           5                 9'
  baseline line 2523: '2           8                 19'
  new      line 2523: '2           10                19'
```

`Advect_cpp.chem.txt`, `diff | head -20` (diff line count: 98):

```
202c202
< End of Run after 0.195706 Seconds.
---
> End of Run after 0.211589 Seconds.
335c335
< End of Run after 0.237162 Seconds.
---
> End of Run after 0.253695 Seconds.
448,450c448,450
< ----------------------------------
< End of Run after 0.237467 Seconds.
< ----------------------------------
---
> ---------------------------------
> End of Run after 0.25427 Seconds.
> ---------------------------------
563c563
< End of Run after 0.237703 Seconds.
---
> End of Run after 0.254885 Seconds.
```

## Step 3 — Absolute paths in outputs

Specified command (non-recursive, as written in the task):

```
$ grep -l "/home/" ~/phreeqcrm_phase2_baseline/v390/openmp_run1/* 2>/dev/null
(no output; exit 1)
```

Approved additional recursive command:

```
$ grep -rl "/home/" ~/phreeqcrm_phase2_baseline/v390/openmp_run1/ 2>/dev/null
(no output; exit 1)
```

No file in `openmp_run1` contains the string `/home/`. No matching files, so
there are no matching lines to report.

## Step 4 — MPI build

### Configure

```
$ cmake -S ~/phreeqcrm_baseline_src -B ~/phreeqcrm_baseline_src/build-mpi -DCMAKE_BUILD_TYPE=Release -DPHREEQCRM_BUILD_MPI=ON -DMPIEXEC_MAX_NUMPROCS=4
-- The CXX compiler identification is GNU 15.2.0
-- The C compiler identification is GNU 15.2.0
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Setting CMAKE_POLICY_VERSION_MINIMUM=3.31 for yaml-cpp
-- Found OpenMP_C: -fopenmp (found version "4.5")
-- Found OpenMP_CXX: -fopenmp (found version "4.5")
-- Found OpenMP: TRUE (found version "4.5")
-- Found MPI_C: /usr/lib/x86_64-linux-gnu/openmpi/lib/libmpi.so (found version "3.1")
-- Found MPI_CXX: /usr/lib/x86_64-linux-gnu/openmpi/lib/libmpi.so (found version "3.1")
-- Found MPI: TRUE (found version "3.1")
-- Performing Test COMPILER_HAS_HIDDEN_VISIBILITY
-- Performing Test COMPILER_HAS_HIDDEN_VISIBILITY - Success
-- Performing Test COMPILER_HAS_HIDDEN_INLINE_VISIBILITY
-- Performing Test COMPILER_HAS_HIDDEN_INLINE_VISIBILITY - Success
-- Performing Test COMPILER_HAS_DEPRECATED_ATTR
-- Performing Test COMPILER_HAS_DEPRECATED_ATTR - Success
-- SKBUILD is NOT defined
-- Configuring done (2.1s)
-- Generating done (0.0s)
-- Build files have been written to: /home/chen/phreeqcrm_baseline_src/build-mpi
EXIT=0
```

### Build

```
$ cmake --build ~/phreeqcrm_baseline_src/build-mpi -j 8
EXIT=0
(149 lines of output; first 40 lines below)
```

First 40 lines:

```
[  0%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/BMI_interface_C.cpp.o
[  1%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/BMI_interface_F.cpp.o
[  3%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/BMIVariant.cpp.o
[  3%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/BMIPhreeqcRM.cpp.o
[  4%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/CSelectedOutput.cpp.o
[  7%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/IPhreeqc_interface_F.cpp.o
[  7%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/IPhreeqcLib.cpp.o
[  7%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/IPhreeqc.cpp.o
[  7%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/advection.cpp.o
[  8%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/basicsubs.cpp.o
[  9%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/cl1.cpp.o
[ 10%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/common/Parser.cxx.o
[ 11%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/common/PHRQ_base.cxx.o
[ 12%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/common/PHRQ_io.cpp.o
[ 13%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/common/Utils.cxx.o
[ 14%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/cvdense.cpp.o
[ 14%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/cvode.cpp.o
[ 15%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/cxxKinetics.cxx.o
[ 16%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/cxxMix.cxx.o
[ 17%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/dense.cpp.o
[ 18%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/Dictionary.cpp.o
[ 19%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/dumper.cpp.o
[ 20%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/Exchange.cxx.o
[ 21%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/ExchComp.cxx.o
[ 21%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/GasComp.cxx.o
[ 22%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/gases.cpp.o
[ 23%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/GasPhase.cxx.o
[ 24%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/input.cpp.o
[ 25%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/integrate.cpp.o
[ 26%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/inverse.cpp.o
[ 27%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/ISolution.cxx.o
[ 28%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/ISolutionComp.cxx.o
[ 28%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/isotopes.cpp.o
[ 29%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/kinetics.cpp.o
[ 30%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/KineticsComp.cxx.o
[ 31%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/mainsubs.cpp.o
[ 32%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/model.cpp.o
[ 33%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/NameDouble.cxx.o
[ 34%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/NumKeyword.cxx.o
[ 35%] Building CXX object CMakeFiles/PhreeqcRM.dir/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/nvector_serial.cpp.o
```

### MPIEXEC variables from `CMakeCache.txt`

```
$ grep -E "^MPIEXEC_(EXECUTABLE|MAX_NUMPROCS|NUMPROC_FLAG)" ~/phreeqcrm_baseline_src/build-mpi/CMakeCache.txt
MPIEXEC_EXECUTABLE:FILEPATH=/usr/bin/mpiexec
MPIEXEC_MAX_NUMPROCS:STRING=4
MPIEXEC_NUMPROC_FLAG:STRING=-n
MPIEXEC_EXECUTABLE-ADVANCED:INTERNAL=1
MPIEXEC_MAX_NUMPROCS-ADVANCED:INTERNAL=1
MPIEXEC_NUMPROC_FLAG-ADVANCED:INTERNAL=1
```

## Step 5 — MPI runs, each in its own copy of the test directory

For every case: `cp -r build-mpi/Tests build-mpi/<case>`, `touch <case>/run_marker`,
run the command with `timeout 600` from inside the copy, then
`find <case> -newer <case>/run_marker -type f`.

Fact common to all four `TestRM` cases: the set of produced files, taken
relative to the case directory, is identical to the 27-file list of Step 1
(verified with `diff` after stripping the case-directory prefix; the raw
absolute-path lists differ only in that prefix). The per-case `find` outputs
below therefore give the count and the verification result rather than
reprinting the same 27 names four times.

### Case `Tests_dtor` — `mpiexec -n 1 ./TestRMdtor; mpiexec -n 1 ./TestBMIdtor`

```
EXIT_TestRMdtor=0    (stdout+stderr: 0 lines)
EXIT_TestBMIdtor=0   (stdout+stderr: 0 lines)
$ find ~/phreeqcrm_baseline_src/build-mpi/Tests_dtor -newer .../run_marker -type f
0 files
```

Last 15 lines of output: none (both programs print nothing). No files were
produced; `~/phreeqcrm_phase2_baseline/v390/Tests_dtor/` was created and
contains 0 files.

### Case `Tests_np1` — `mpiexec -n 1 ./TestRM`

```
EXIT=0    (stdout+stderr: 506 lines)
find -newer run_marker: 27 files (relative-name set identical to the Step 1 list)
```

Last 15 lines of program output:

```
  CH4(g)      0.0200      0.0200      0.9998
  Ntg(g)      0.0401      0.0400      0.9999
  Oxg(g)      0.0601      0.0600      0.9999
Cell: 1
               Moles         P         Phi
  CH4(g)      0.0100      0.0200      0.9998
  Ntg(g)      0.0200      0.0399      0.9999
  Oxg(g)      0.0300      0.0599      0.9999
Cell: 2
               Moles         P         Phi
  CH4(g)      0.2605      0.2598      0.9980
  Ntg(g)      0.3406      0.3398      0.9996
  Oxg(g)      0.3807      0.3798      0.9991
Done Gas_c.==========================================
Done with tests.
```

27 files copied to `~/phreeqcrm_phase2_baseline/v390/Tests_np1/`.

### Case `Tests_np2` — `mpiexec -n 2 ./TestRM`

```
EXIT=0    (stdout+stderr: 722 lines)
find -newer run_marker: 27 files (relative-name set identical)
```

Last 15 lines of program output: identical text to the `Tests_np1` block
above (ending `Done Gas_c.==========================================` /
`Done with tests.`).

27 files copied to `~/phreeqcrm_phase2_baseline/v390/Tests_np2/`.

### Case `Tests_np4` — `mpiexec -n 4 ./TestRM`

```
EXIT=0    (stdout+stderr: 728 lines)
find -newer run_marker: 27 files (relative-name set identical)
```

Last 15 lines of program output: identical text to the `Tests_np1` block
above.

27 files copied to `~/phreeqcrm_phase2_baseline/v390/Tests_np4/`.

### Case `Tests_np2_repeat` — `mpiexec -n 2 ./TestRM`

```
EXIT=0    (stdout+stderr: 722 lines)
find -newer run_marker: 27 files (relative-name set identical)
```

Last 15 lines of program output: identical text to the `Tests_np1` block
above.

27 files copied to `~/phreeqcrm_phase2_baseline/v390/Tests_np2_repeat/`.

### `ctest -R TestMPI` in `build-mpi`

```
$ ctest --test-dir ~/phreeqcrm_baseline_src/build-mpi -R TestMPI --output-on-failure --timeout 600
Test project /home/chen/phreeqcrm_baseline_src/build-mpi
    Start 4: TestMPI
1/1 Test #4: TestMPI ..........................   Passed    9.52 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   9.52 sec
EXIT=0
```

Fact: the registered command, from `build-mpi/Tests/CTestTestfile.cmake`
line 13, is `"/usr/bin/mpiexec" "-n" "4" ".../build-mpi/Tests/TestRM"` —
i.e. `mpiexec -n 4 TestRM` in the original `Tests` directory, consistent with
`Tests/CMakeLists.txt` lines 332–335 using `${MPIEXEC_MAX_NUMPROCS}` (= 4).
This run wrote into `build-mpi/Tests` after the case copies were made; the
copies are unaffected.

## Step 6 — Rank-count invariance

Same tools and one line per file per comparison, as in Step 2.

### Set A — `Tests_np2` vs `Tests_np2_repeat` (determinism at fixed rank count)

```
Advect_c.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Advect_c.dmp | cmp | DIFFER
Advect_c.log.txt | cmp | IDENTICAL
Advect_c_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Advect_cpp.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Advect_cpp.dmp | cmp | DIFFER
Advect_cpp.log.txt | cmp | IDENTICAL
Advect_cpp_units_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Advect_cpp_units_worker.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Advect_cpp_units_worker.log.txt | cmp | IDENTICAL
Advect_cpp_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Gas_c.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Gas_c.log.txt | cmp | IDENTICAL
Gas_cpp.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Gas_cpp.log.txt | cmp | IDENTICAL
SimpleAdvect_c.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
SimpleAdvect_c.log.txt | cmp | IDENTICAL
SimpleAdvect_cpp.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
SimpleAdvect_cpp.log.txt | cmp | IDENTICAL
Species_c.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Species_c.dmp | cmp | DIFFER
Species_c.log.txt | cmp | IDENTICAL
Species_c_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Species_cpp.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Species_cpp.dmp | cmp | DIFFER
Species_cpp.log.txt | cmp | IDENTICAL
Species_cpp_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
```

### Set B — `Tests_np1` vs `Tests_np2` (invariance)

```
Advect_c.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Advect_c.dmp | cmp | DIFFER
Advect_c.log.txt | cmp | IDENTICAL
Advect_c_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Advect_cpp.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Advect_cpp.dmp | cmp | DIFFER
Advect_cpp.log.txt | cmp | IDENTICAL
Advect_cpp_units_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Advect_cpp_units_worker.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Advect_cpp_units_worker.log.txt | cmp | IDENTICAL
Advect_cpp_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Gas_c.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Gas_c.log.txt | cmp | IDENTICAL
Gas_cpp.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Gas_cpp.log.txt | cmp | IDENTICAL
SimpleAdvect_c.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
SimpleAdvect_c.log.txt | cmp | IDENTICAL
SimpleAdvect_cpp.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
SimpleAdvect_cpp.log.txt | cmp | IDENTICAL
Species_c.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Species_c.dmp | cmp | DIFFER
Species_c.log.txt | cmp | IDENTICAL
Species_c_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Species_cpp.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Species_cpp.dmp | cmp | DIFFER
Species_cpp.log.txt | cmp | IDENTICAL
Species_cpp_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
```

### Set C — `Tests_np1` vs `Tests_np4` (invariance)

```
Advect_c.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Advect_c.dmp | cmp | DIFFER
Advect_c.log.txt | cmp | IDENTICAL
Advect_c_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Advect_cpp.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Advect_cpp.dmp | cmp | DIFFER
Advect_cpp.log.txt | cmp | IDENTICAL
Advect_cpp_units_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Advect_cpp_units_worker.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Advect_cpp_units_worker.log.txt | cmp | IDENTICAL
Advect_cpp_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Gas_c.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Gas_c.log.txt | cmp | IDENTICAL
Gas_cpp.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Gas_cpp.log.txt | cmp | IDENTICAL
SimpleAdvect_c.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
SimpleAdvect_c.log.txt | cmp | IDENTICAL
SimpleAdvect_cpp.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
SimpleAdvect_cpp.log.txt | cmp | IDENTICAL
Species_c.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Species_c.dmp | cmp | DIFFER
Species_c.log.txt | cmp | IDENTICAL
Species_c_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Species_cpp.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Species_cpp.dmp | cmp | DIFFER
Species_cpp.log.txt | cmp | IDENTICAL
Species_cpp_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
```

### Set D — `Tests_np1` vs `openmp_run1` (MPI vs OpenMP; all 27 files present in both)

```
Advect_c.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Advect_c.dmp | cmp | DIFFER
Advect_c.log.txt | cmp | IDENTICAL
Advect_c_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Advect_cpp.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Advect_cpp.dmp | cmp | DIFFER
Advect_cpp.log.txt | cmp | IDENTICAL
Advect_cpp_units_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Advect_cpp_units_worker.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Advect_cpp_units_worker.log.txt | cmp | IDENTICAL
Advect_cpp_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Gas_c.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Gas_c.log.txt | cmp | IDENTICAL
Gas_cpp.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
Gas_cpp.log.txt | cmp | IDENTICAL
SimpleAdvect_c.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
SimpleAdvect_c.log.txt | cmp | IDENTICAL
SimpleAdvect_cpp.chem.txt | compare_main_output_to_baseline.py | IDENTICAL
SimpleAdvect_cpp.log.txt | cmp | IDENTICAL
Species_c.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Species_c.dmp | cmp | DIFFER
Species_c.log.txt | cmp | IDENTICAL
Species_c_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
Species_cpp.chem.txt | compare_main_output_to_baseline.py | NOT_IDENTICAL(rc=1)
Species_cpp.dmp | cmp | DIFFER
Species_cpp.log.txt | cmp | IDENTICAL
Species_cpp_utility.txt | compare_main_output_to_baseline.py | IDENTICAL
```

### Detail: comparison-script output for every non-identical `.chem.txt` in Step 6

Captured by re-running the script on each failing pair; full output
(181 lines), part 1:

```
=== SET A: Tests_np2/Advect_c.chem.txt vs Tests_np2_repeat/Advect_c.chem.txt ===
DIFFERENT  compared_lines=2452 ignored_run_time_lines baseline=63 new=63
  baseline line 1041: 'Using solution 7.\tSolution after simulation 1.'
  new      line 1041: 'Using solution 7.\t  '
  baseline line 1042: 'Using exchange 7.\tExchange assemblage after simulation 1.'
  new      line 1042: 'Using exchange 7.\t '
  baseline line 1155: 'Using solution 8.\t  '
  new      line 1155: 'Using solution 8.\tSolution after simulation 1.'
  baseline line 1156: 'Using exchange 8.\t '
  new      line 1156: 'Using exchange 8.\tExchange assemblage after simulation 1.'
=== SET A: Tests_np2/Advect_cpp.chem.txt vs Tests_np2_repeat/Advect_cpp.chem.txt ===
DIFFERENT  compared_lines=2459 ignored_run_time_lines baseline=63 new=63
  baseline line 1158: 'Using solution 8.\t  '
  new      line 1158: 'Using solution 8.\tSolution after simulation 1.'
  baseline line 1159: 'Using exchange 8.\t '
  new      line 1159: 'Using exchange 8.\tExchange assemblage after simulation 1.'
  baseline line 2521: '0           0                 9'
  new      line 2521: '0           0                 7'
  baseline line 2522: '1           10                 19'
  new      line 2522: '1           8                 19'
=== SET A: Tests_np2/SimpleAdvect_c.chem.txt vs Tests_np2_repeat/SimpleAdvect_c.chem.txt ===
DIFFERENT  compared_lines=2445 ignored_run_time_lines baseline=63 new=63
  baseline line 1034: 'Using solution 7.\tSolution after simulation 1.'
  new      line 1034: 'Using solution 7.\t  '
  baseline line 1035: 'Using exchange 7.\tExchange assemblage after simulation 1.'
  new      line 1035: 'Using exchange 7.\t '
=== SET A: Tests_np2/SimpleAdvect_cpp.chem.txt vs Tests_np2_repeat/SimpleAdvect_cpp.chem.txt ===
DIFFERENT  compared_lines=2445 ignored_run_time_lines baseline=63 new=63
  baseline line 1034: 'Using solution 7.\tSolution after simulation 1.'
  new      line 1034: 'Using solution 7.\t  '
  baseline line 1035: 'Using exchange 7.\tExchange assemblage after simulation 1.'
  new      line 1035: 'Using exchange 7.\t '
=== SET A: Tests_np2/Species_c.chem.txt vs Tests_np2_repeat/Species_c.chem.txt ===
DIFFERENT  compared_lines=2505 ignored_run_time_lines baseline=63 new=63
  baseline line 1094: 'Using solution 7.\tSolution after simulation 1.'
  new      line 1094: 'Using solution 7.\t  '
  baseline line 1095: 'Using exchange 7.\tExchange assemblage after simulation 1.'
  new      line 1095: 'Using exchange 7.\t '
  baseline line 1208: 'Using solution 8.\t  '
  new      line 1208: 'Using solution 8.\tSolution after simulation 1.'
  baseline line 1209: 'Using exchange 8.\t '
  new      line 1209: 'Using exchange 8.\tExchange assemblage after simulation 1.'
=== SET A: Tests_np2/Species_cpp.chem.txt vs Tests_np2_repeat/Species_cpp.chem.txt ===
DIFFERENT  compared_lines=2550 ignored_run_time_lines baseline=63 new=63
  baseline line 1253: 'Using solution 8.\t  '
  new      line 1253: 'Using solution 8.\tSolution after simulation 1.'
  baseline line 1254: 'Using exchange 8.\t '
  new      line 1254: 'Using exchange 8.\tExchange assemblage after simulation 1.'
=== SET B: Tests_np1/Advect_c.chem.txt vs Tests_np2/Advect_c.chem.txt ===
DIFFERENT  compared_lines=2452 ignored_run_time_lines baseline=63 new=63
  baseline line 206: 'Number of MPI processes:                          1'
  new      line 206: 'Number of MPI processes:                          2'
  baseline line 1155: 'Using solution 8.\tSolution after simulation 1.'
  new      line 1155: 'Using solution 8.\t  '
  baseline line 1156: 'Using exchange 8.\tExchange assemblage after simulation 1.'
  new      line 1156: 'Using exchange 8.\t '
=== SET B: Tests_np1/Advect_cpp.chem.txt vs Tests_np2/Advect_cpp.chem.txt ===
DIFFERENT  compared_lines=2458 ignored_run_time_lines baseline=63 new=63
  line counts differ after removing run-time lines: baseline=2458 new=2459
  baseline line 207: 'Number of MPI processes:                          1'
  new      line 207: 'Number of MPI processes:                          2'
  baseline line 1158: 'Using solution 8.\tSolution after simulation 1.'
  new      line 1158: 'Using solution 8.\t  '
  baseline line 1159: 'Using exchange 8.\tExchange assemblage after simulation 1.'
  new      line 1159: 'Using exchange 8.\t '
  baseline line 2521: '0           0                 19'
  new      line 2521: '0           0                 9'
=== SET B: Tests_np1/Species_c.chem.txt vs Tests_np2/Species_c.chem.txt ===
DIFFERENT  compared_lines=2505 ignored_run_time_lines baseline=63 new=63
  baseline line 206: 'Number of MPI processes:                          1'
  new      line 206: 'Number of MPI processes:                          2'
  baseline line 1208: 'Using solution 8.\tSolution after simulation 1.'
  new      line 1208: 'Using solution 8.\t  '
  baseline line 1209: 'Using exchange 8.\tExchange assemblage after simulation 1.'
  new      line 1209: 'Using exchange 8.\t '
=== SET B: Tests_np1/Species_cpp.chem.txt vs Tests_np2/Species_cpp.chem.txt ===
DIFFERENT  compared_lines=2550 ignored_run_time_lines baseline=63 new=63
  baseline line 206: 'Number of MPI processes:                          1'
  new      line 206: 'Number of MPI processes:                          2'
  baseline line 1253: 'Using solution 8.\tSolution after simulation 1.'
  new      line 1253: 'Using solution 8.\t  '
  baseline line 1254: 'Using exchange 8.\tExchange assemblage after simulation 1.'
  new      line 1254: 'Using exchange 8.\t '
```

Part 2:

```
=== SET C: Tests_np1/Advect_c.chem.txt vs Tests_np4/Advect_c.chem.txt ===
DIFFERENT  compared_lines=2452 ignored_run_time_lines baseline=63 new=63
  baseline line 206: 'Number of MPI processes:                          1'
  new      line 206: 'Number of MPI processes:                          4'
  baseline line 699: 'Using solution 4.\tSolution after simulation 1.'
  new      line 699: 'Using solution 4.\t  '
  baseline line 700: 'Using exchange 4.\tExchange assemblage after simulation 1.'
  new      line 700: 'Using exchange 4.\t '
  baseline line 927: 'Using solution 6.\tSolution after simulation 1.'
  new      line 927: 'Using solution 6.\t  '
  baseline line 928: 'Using exchange 6.\tExchange assemblage after simulation 1.'
  new      line 928: 'Using exchange 6.\t '
  baseline line 1155: 'Using solution 8.\tSolution after simulation 1.'
  new      line 1155: 'Using solution 8.\t  '
  baseline line 1156: 'Using exchange 8.\tExchange assemblage after simulation 1.'
  new      line 1156: 'Using exchange 8.\t '
=== SET C: Tests_np1/Advect_cpp.chem.txt vs Tests_np4/Advect_cpp.chem.txt ===
DIFFERENT  compared_lines=2458 ignored_run_time_lines baseline=63 new=63
  line counts differ after removing run-time lines: baseline=2458 new=2461
  baseline line 207: 'Number of MPI processes:                          1'
  new      line 207: 'Number of MPI processes:                          4'
  baseline line 930: 'Using solution 6.\tSolution after simulation 1.'
  new      line 930: 'Using solution 6.\t  '
  baseline line 931: 'Using exchange 6.\tExchange assemblage after simulation 1.'
  new      line 931: 'Using exchange 6.\t '
  baseline line 1158: 'Using solution 8.\tSolution after simulation 1.'
  new      line 1158: 'Using solution 8.\t  '
  baseline line 1159: 'Using exchange 8.\tExchange assemblage after simulation 1.'
  new      line 1159: 'Using exchange 8.\t '
  baseline line 2521: '0           0                 19'
  new      line 2521: '0           0                 4'
=== SET C: Tests_np1/SimpleAdvect_c.chem.txt vs Tests_np4/SimpleAdvect_c.chem.txt ===
DIFFERENT  compared_lines=2445 ignored_run_time_lines baseline=63 new=63
  baseline line 692: 'Using solution 4.\tSolution after simulation 1.'
  new      line 692: 'Using solution 4.\t  '
  baseline line 693: 'Using exchange 4.\tExchange assemblage after simulation 1.'
  new      line 693: 'Using exchange 4.\t '
  baseline line 920: 'Using solution 6.\tSolution after simulation 1.'
  new      line 920: 'Using solution 6.\t  '
  baseline line 921: 'Using exchange 6.\tExchange assemblage after simulation 1.'
  new      line 921: 'Using exchange 6.\t '
  baseline line 1148: 'Using solution 8.\tSolution after simulation 1.'
  new      line 1148: 'Using solution 8.\t  '
  baseline line 1149: 'Using exchange 8.\tExchange assemblage after simulation 1.'
  new      line 1149: 'Using exchange 8.\t '
=== SET C: Tests_np1/SimpleAdvect_cpp.chem.txt vs Tests_np4/SimpleAdvect_cpp.chem.txt ===
DIFFERENT  compared_lines=2445 ignored_run_time_lines baseline=63 new=63
  baseline line 692: 'Using solution 4.\tSolution after simulation 1.'
  new      line 692: 'Using solution 4.\t  '
  baseline line 693: 'Using exchange 4.\tExchange assemblage after simulation 1.'
  new      line 693: 'Using exchange 4.\t '
  baseline line 920: 'Using solution 6.\tSolution after simulation 1.'
  new      line 920: 'Using solution 6.\t  '
  baseline line 921: 'Using exchange 6.\tExchange assemblage after simulation 1.'
  new      line 921: 'Using exchange 6.\t '
  baseline line 1148: 'Using solution 8.\tSolution after simulation 1.'
  new      line 1148: 'Using solution 8.\t  '
  baseline line 1149: 'Using exchange 8.\tExchange assemblage after simulation 1.'
  new      line 1149: 'Using exchange 8.\t '
=== SET C: Tests_np1/Species_c.chem.txt vs Tests_np4/Species_c.chem.txt ===
DIFFERENT  compared_lines=2505 ignored_run_time_lines baseline=63 new=63
  baseline line 206: 'Number of MPI processes:                          1'
  new      line 206: 'Number of MPI processes:                          4'
  baseline line 752: 'Using solution 4.\tSolution after simulation 1.'
  new      line 752: 'Using solution 4.\t  '
  baseline line 753: 'Using exchange 4.\tExchange assemblage after simulation 1.'
  new      line 753: 'Using exchange 4.\t '
  baseline line 980: 'Using solution 6.\tSolution after simulation 1.'
  new      line 980: 'Using solution 6.\t  '
  baseline line 981: 'Using exchange 6.\tExchange assemblage after simulation 1.'
  new      line 981: 'Using exchange 6.\t '
  baseline line 1208: 'Using solution 8.\tSolution after simulation 1.'
  new      line 1208: 'Using solution 8.\t  '
  baseline line 1209: 'Using exchange 8.\tExchange assemblage after simulation 1.'
  new      line 1209: 'Using exchange 8.\t '
=== SET C: Tests_np1/Species_cpp.chem.txt vs Tests_np4/Species_cpp.chem.txt ===
DIFFERENT  compared_lines=2550 ignored_run_time_lines baseline=63 new=63
  baseline line 206: 'Number of MPI processes:                          1'
  new      line 206: 'Number of MPI processes:                          4'
=== SET D: Tests_np1/Advect_c.chem.txt vs openmp_run1/Advect_c.chem.txt ===
DIFFERENT  compared_lines=2452 ignored_run_time_lines baseline=63 new=63
  baseline line 205: 'Number of threads:                                1'
  new      line 205: 'Number of threads:                                3'
=== SET D: Tests_np1/Advect_cpp.chem.txt vs openmp_run1/Advect_cpp.chem.txt ===
DIFFERENT  compared_lines=2458 ignored_run_time_lines baseline=63 new=63
  line counts differ after removing run-time lines: baseline=2458 new=2460
  baseline line 206: 'Number of threads:                                1'
  new      line 206: 'Number of threads:                                3'
  baseline line 2521: '0           0                 19'
  new      line 2521: '0           0                 4'
=== SET D: Tests_np1/Species_c.chem.txt vs openmp_run1/Species_c.chem.txt ===
DIFFERENT  compared_lines=2505 ignored_run_time_lines baseline=63 new=63
  baseline line 205: 'Number of threads:                                1'
  new      line 205: 'Number of threads:                                3'
=== SET D: Tests_np1/Species_cpp.chem.txt vs openmp_run1/Species_cpp.chem.txt ===
DIFFERENT  compared_lines=2550 ignored_run_time_lines baseline=63 new=63
  baseline line 205: 'Number of threads:                                1'
  new      line 205: 'Number of threads:                                3'
```

### Detail: diff output for every non-identical `.dmp` in Step 6

Full captured diff detail is 336 lines (`/tmp/opencode/p2t2a/step6_dmp_details.txt`);
first 40 lines:

```
=== SET A: Tests_np2/Advect_c.dmp vs Tests_np2_repeat/Advect_c.dmp (diff lines: 105) ===
249,279d248
<     SOLUTION_RAW                 8 Solution after simulation 1.
<       -temp                      20
<       -pressure                  2
<       -potential                 0
<       -total_h                   22.136415506019
<       -total_o                   11.068314447391
<       -cb                        1.4446771502959e-13
<       -density                   0.9983011293439
<       -viscosity                 1.0018316494637
<       -viscos_0                  1.0015681605689
<       -totals
<         Ca                   3.5591651509235e-24
<         Cl                   0.00023928579659791
<         K                    3.9783056143315e-05
<         Na                   0.00019950274059907
<       -pH                        7.0817104543639
<       -pe                        13.966427267761
<       -mu                        0.0012000859379213
<       -ah2o                      0.99995464903202
=== SET A: Tests_np2/Advect_cpp.dmp vs Tests_np2_repeat/Advect_cpp.dmp (diff lines: 213) ===
218c218
<     SOLUTION_RAW                 7 Solution after simulation 1.
---
>     SOLUTION_RAW                 7   
227c227
<       -viscos_0                  1.0015681605689
---
>       -viscos_0                  1
249,308d248
<     SOLUTION_RAW                 8 Solution after simulation 1.
<       -temp                      20
<       -pressure                  2
<       -potential                 0
<       -total_h                   22.136415506019
<       -total_o                   11.068314447391
<       -cb                        1.4446771502959e-13
<       -density                   0.9983011293439
<       -viscosity                 1.0018316494637
```

## Summary table

| Comparison set | Files identical | Files different |
|---|---|---|
| Step 2: `openmp_run1` vs `openmp_run2` | 22 | 5 |
| Step 6 Set A: `Tests_np2` vs `Tests_np2_repeat` | 17 | 10 |
| Step 6 Set B: `Tests_np1` vs `Tests_np2` | 19 | 8 |
| Step 6 Set C: `Tests_np1` vs `Tests_np4` | 17 | 10 |
| Step 6 Set D: `Tests_np1` vs `openmp_run1` | 19 | 8 |

All sets compare the 27 output files under `Tests/`. Differences are reported,
not judged.

## Reviewer decisions required

1. Step 2 (OpenMP run1 vs run2) was not identical for five files:
   `Advect_c.dmp`, `Advect_cpp.dmp`, `Species_c.dmp`, `Species_cpp.dmp`
   (`cmp` DIFFER), and `Advect_cpp.chem.txt`
   (`compare_main_output_to_baseline.py` NOT_IDENTICAL, rc=1).
2. Step 3: no absolute path `/home/` was found in any file under
   `~/phreeqcrm_phase2_baseline/v390/openmp_run1/` (both the specified
   non-recursive `grep -l` and the recursive `grep -rl` returned no matches,
   exit status 1).
3. No MPI run failed or timed out: `Tests_dtor` (both programs), `Tests_np1`,
   `Tests_np2`, `Tests_np4`, and `Tests_np2_repeat` all exited 0 under
   `timeout 600`, and `ctest -R TestMPI` passed (9.52 s).
4. Files that differ between rank counts in Step 6:
   - Set A (`Tests_np2` vs `Tests_np2_repeat`, same rank count):
     `Advect_c.chem.txt`, `Advect_cpp.chem.txt`, `SimpleAdvect_c.chem.txt`,
     `SimpleAdvect_cpp.chem.txt`, `Species_c.chem.txt`,
     `Species_cpp.chem.txt`, `Advect_c.dmp`, `Advect_cpp.dmp`,
     `Species_c.dmp`, `Species_cpp.dmp`.
   - Set B (`Tests_np1` vs `Tests_np2`):
     `Advect_c.chem.txt`, `Advect_cpp.chem.txt`, `Species_c.chem.txt`,
     `Species_cpp.chem.txt`, `Advect_c.dmp`, `Advect_cpp.dmp`,
     `Species_c.dmp`, `Species_cpp.dmp`.
   - Set C (`Tests_np1` vs `Tests_np4`):
     `Advect_c.chem.txt`, `Advect_cpp.chem.txt`, `SimpleAdvect_c.chem.txt`,
     `SimpleAdvect_cpp.chem.txt`, `Species_c.chem.txt`,
     `Species_cpp.chem.txt`, `Advect_c.dmp`, `Advect_cpp.dmp`,
     `Species_c.dmp`, `Species_cpp.dmp`.
   - Set D (`Tests_np1` vs `openmp_run1`):
     `Advect_c.chem.txt`, `Advect_cpp.chem.txt`, `Species_c.chem.txt`,
     `Species_cpp.chem.txt`, `Advect_c.dmp`, `Advect_cpp.dmp`,
     `Species_c.dmp`, `Species_cpp.dmp`.
   - Observed in the comparison output (not judged): the differing
     `.chem.txt` lines are the "Number of MPI processes:" / "Number of
     threads:" header lines (the OpenMP baseline reports
     `Number of threads: 3`), the "Using solution N." / "Using exchange N."
     comment suffixes, and the transport cell-count table in the
     `Advect_cpp` files; the `.dmp` differences are missing or reordered
     `SOLUTION_RAW` blocks.
