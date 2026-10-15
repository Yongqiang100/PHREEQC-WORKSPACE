# Phase 2 Task 2c: LAPACK-versus-cl1 Difference Diagnosis — Report

Read-only task. No source file, build configuration, existing case directory, or
baseline was modified. New run directory created:
`~/phreeqcrm/build-patched-mpi-lapack/Tests_np1_compare`.

Conventions used below:
- `T=~/phreeqc3/tests/petsc_linear_solver/tools/compare_phreeqcrm_chem_output.py`
- `M=~/phreeqc3/tests/petsc_linear_solver/tools/compare_main_output_to_baseline.py`
- `B=~/phreeqcrm_phase2_baseline/v390`

## Step 0 — Pre-checks

`echo "$PETSC_DIR"` → `/opt/petsc/3.26.0` (matches the required value).

`git -C ~/phreeqcrm status --short` → empty (exit 0). Task 2b was committed.

`ls ~/phreeqcrm/build-patched-mpi-lapack/Tests/TestRM ~/phreeqcrm/build-patched-openmp-lapack/Tests_lapack`
→ both paths present (exit 0).

`python3 $T --help | head -5` mentions `--relative-tolerance`:

```
usage: compare_phreeqcrm_chem_output.py [-h]
                                        [--max-reported-differences MAX_REPORTED_DIFFERENCES]
                                        [--relative-tolerance RELATIVE_TOLERANCE]
                                        [--absolute-tolerance ABSOLUTE_TOLERANCE]
                                        baseline_file new_file
```

Baseline directories `$B`, `$B/openmp_run1`, `$B/Tests_np1` all exist.

## Step 1 — Tolerance comparison of the 12 differing files

Command form (per file):

```
python3 $T $B/openmp_run1/<file> ~/phreeqcrm/build-patched-openmp-lapack/Tests_lapack/<file> \
    --relative-tolerance 1e-6 --absolute-tolerance 1e-12 --max-reported-differences 200
```

### 7 `.chem.txt` files — all WITHIN_TOLERANCE, no differing pairs

```
Advect_c.chem.txt               WITHIN_TOLERANCE  compared_lines=2450 ...
Advect_cpp.chem.txt             WITHIN_TOLERANCE  compared_lines=2455 ...
Advect_cpp_units_worker.chem.txt WITHIN_TOLERANCE compared_lines=301  ...
SimpleAdvect_c.chem.txt         WITHIN_TOLERANCE  compared_lines=2445 ...
SimpleAdvect_cpp.chem.txt       WITHIN_TOLERANCE  compared_lines=2445 ...
Species_c.chem.txt              WITHIN_TOLERANCE  compared_lines=2503 ...
Species_cpp.chem.txt            WITHIN_TOLERANCE  compared_lines=2548 ...
```

### 5 `_utility.txt` files — 4 WITHIN_TOLERANCE, 1 DIFFERENT

```
Advect_c_utility.txt        WITHIN_TOLERANCE  compared_lines=95   (exit 0)
Advect_cpp_utility.txt      WITHIN_TOLERANCE  compared_lines=95   (exit 0)
Advect_cpp_units_utility.txt DIFFERENT        compared_lines=172  (exit 1)
Species_c_utility.txt       WITHIN_TOLERANCE  compared_lines=95   (exit 0)
Species_cpp_utility.txt     WITHIN_TOLERANCE  compared_lines=95   (exit 0)
```

The only file with lines exceeding the tolerance is `Advect_cpp_units_utility.txt`.
Its 9 differing line pairs (baseline / new):

```
line 57:  '   CH4             1.997e-29   2.017e-29   -28.700   -28.695     0.004     35.46'
       vs '   CH4             2.007e-29   2.027e-29   -28.698   -28.693     0.004     35.46'
line 75:  '   H2              3.053e-15   3.084e-15   -14.515   -14.511     0.004     28.61'
       vs '   H2              3.056e-15   3.088e-15   -14.515   -14.510     0.004     28.61'
line 81:  '   O2              0.000e+00   0.000e+00   -63.364   -63.359     0.004     30.40'
       vs '   O2              0.000e+00   0.000e+00   -63.365   -63.361     0.004     30.40'
line 89:  '  CH4(g)          -25.89    -28.70   -2.80  CH4'
       vs '  CH4(g)          -25.89    -28.69   -2.80  CH4'
line 117: '                                       pe  =  -0.602      Adjusted to redox equilibrium'
       vs '                                       pe  =  -0.558      Adjusted to redox equilibrium'
line 145: '   H2              1.057e-16   1.350e-16   -15.976   -15.870     0.106     28.61'
       vs '   H2              8.625e-17   1.101e-16   -16.064   -15.958     0.106     28.61'
line 150: '   O2              0.000e+00   0.000e+00   -60.779   -60.673     0.106     30.40'
       vs '   O2              0.000e+00   0.000e+00   -60.602   -60.496     0.106     30.40'
line 156: '  H2(g)           -12.77    -15.87   -3.10  H2'
       vs '  H2(g)           -12.86    -15.96   -3.10  H2'
line 159: '  O2(g)           -57.78    -60.67   -2.89  O2'
       vs '  O2(g)           -57.60    -60.50   -2.89  O2'
```

## Step 2 — Context of the redox difference

`grep -n "Adjusted to redox equilibrium"` in both files:

```
baseline  Advect_cpp_units_utility.txt:
  31:  pe  =  -4.265   Adjusted to redox equilibrium
  117: pe  =  -0.602   Adjusted to redox equilibrium
lapack    Advect_cpp_units_utility.txt:
  31:  pe  =  -4.265   Adjusted to redox equilibrium
  117: pe  =  -0.558   Adjusted to redox equilibrium
```

Line 31 is identical in both files. The first differing redox line is **117**.

### Owning solution / cell

`grep -n -E "Initial solution|Using solution|Beginning of|Reaction step"` on the
baseline file:

```
8:   Beginning of run as cells.
12:  Beginning of batch-reaction calculations.
15:  Reaction step 1.
17:  Using solution 1.
100: Beginning of batch-reaction calculations.
103: Reaction step 1.
105: Using solution 2.
```

Line 117 lies in the second batch-reaction block (begins line 100), under
`Reaction step 1.` (line 103) and `Using solution 2.` (line 105). The differing
redox state belongs to **solution 2**. (The identical redox line 31 belongs to
solution 1.)

### Window around line 117 (`sed -n '77,167p'`, i.e. 117−40 … 117+50)

Baseline (excerpt of the differing block, lines 100–160):

```
Beginning of batch-reaction calculations.
Reaction step 1.
Using solution 2.
-----------------------------Solution composition------------------------------
	Elements           Molality       Moles
	Cl                1.062e+00   1.038e+00
	Na                1.062e+00   1.038e+00
----------------------------Description of solution----------------------------
                                       pH  =   6.962      Charge balance
                                       pe  =  -0.602      Adjusted to redox equilibrium
                 Total alkalinity (eq/kg)  =   1.938e-16
                  Electrical balance (eq)  =  -2.660e-13
                               Iterations  =   9
   ...
H(0)          2.115e-16
   H2              1.057e-16   1.350e-16   -15.976   -15.870     0.106     28.61
O(0)          0.000e+00
   O2              0.000e+00   0.000e+00   -60.779   -60.673     0.106     30.40
  H2(g)           -12.77    -15.87   -3.10  H2
  O2(g)           -57.78    -60.67   -2.89  O2
```

LAPACK (same block):

```
                                       pe  =  -0.558      Adjusted to redox equilibrium
                 Total alkalinity (eq/kg)  =   1.587e-16
                  Electrical balance (eq)  =  -3.078e-13
                               Iterations  =   9
   ...
H(0)          1.725e-16
   H2              8.625e-17   1.101e-16   -16.064   -15.958     0.106     28.61
O(0)          0.000e+00
   O2              0.000e+00   0.000e+00   -60.602   -60.496     0.106     30.40
  H2(g)           -12.86    -15.96   -3.10  H2
  O2(g)           -57.60    -60.50   -2.89  O2
```

Both runs report `Iterations = 9` and the same pH (6.962), ionic strength,
density, volume, and Total H / Total O. The differing quantities are pe, the
H(0)/O(0) master-species amounts, H2, O2, and the H2(g)/O2(g)/CH4(g) saturation
indices.

### Input definition (`~/phreeqcrm/Tests/units.pqi`, read-only)

`grep -n -i "SOLUTION\|pe \|redox\|O(0)\|H(0)\|C(-4)" ~/phreeqcrm/Tests/units.pqi`
returns only the three `solution` keywords:

```
7:solution 1
13:solution 2
19:solution 3
```

No `pe`, `redox`, `O(0)`, `H(0)`, or `C(-4)` appears in the input. The file is 28
lines; every `SOLUTION` block in full (`sed -n '1,28p'`):

```
# database ..\phreeqc.dat
selected_output
-high_precision true
-file sel.out
-reset false
-eq Calcite
solution 1
pH 7
-units mg/L
Na   1000.
Cl   1000. charge
end
solution 2
pH 7 charge
-units mol/L
Na   1.0 
Cl   1.0
end
solution 3
pH 7 charge
-units mg/kgs
Na   1000. 
Cl   1000.
end
equilibrium_phases 1
calcite 0 1.0
END
```

Solution 2 fixes `pH 7 charge` and adds only Na and Cl (1.0 mol/L each). It
specifies no `pe`, no redox master species, and no O or C. The redox state of
this solution is not set by any input quantity.

## Step 3 — Comparison-mode run (MPI, 1 rank)

Case created and marked:

```
cp -r ~/phreeqcrm/build-patched-mpi-lapack/Tests \
      ~/phreeqcrm/build-patched-mpi-lapack/Tests_np1_compare      # exit 0
touch ~/phreeqcrm/build-patched-mpi-lapack/Tests_np1_compare/.run_marker   # exit 0
```

Run (in the case directory):

```
timeout 600 mpiexec -n 1 -x OPENBLAS_NUM_THREADS=1 \
  -x PHREEQC_LINEAR_SOLVER_STATISTICS=1 \
  -x PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1=1 \
  -x PHREEQC_LINEAR_SOLVER_COMPARE_LOG=newton_step_comparison.log \
  ./TestRM 2> stderr.txt
```

Exit status: **0**.

Files written by the run (`find . -maxdepth 1 -type f -newer .run_marker`, 29
entries): the 27 PHREEQC output files, plus `newton_step_comparison.log` and
`stderr.txt`.

### Output comparison against `$B/Tests_np1/<file>` (P2-D16, exact mode)

Method per type: `.log.txt` → `cmp`; `_utility.txt` → `compare_main_output_to_baseline.py`;
`.chem.txt` → `compare_phreeqcrm_chem_output.py` (no tolerance flags); `.dmp` →
`cmp`, report-only. One line per file:

```
Advect_c.chem.txt                 chem -> IDENTICAL
Advect_c.dmp                      dmp cmp IDENTICAL (report-only)
Advect_c.log.txt                  log.txt cmp IDENTICAL
Advect_cpp.chem.txt               chem -> IDENTICAL
Advect_cpp.dmp                    dmp cmp IDENTICAL (report-only)
Advect_cpp.log.txt                log.txt cmp IDENTICAL
Advect_cpp_units_utility.txt      utility -> IDENTICAL
Advect_cpp_units_worker.chem.txt  chem -> IDENTICAL
Advect_cpp_units_worker.log.txt   log.txt cmp IDENTICAL
Advect_cpp_utility.txt            utility -> IDENTICAL
Advect_c_utility.txt              utility -> IDENTICAL
Gas_c.chem.txt                    chem -> IDENTICAL
Gas_c.log.txt                     log.txt cmp IDENTICAL
Gas_cpp.chem.txt                  chem -> IDENTICAL
Gas_cpp.log.txt                   log.txt cmp IDENTICAL
SimpleAdvect_c.chem.txt           chem -> IDENTICAL
SimpleAdvect_c.log.txt            log.txt cmp IDENTICAL
SimpleAdvect_cpp.chem.txt         chem -> IDENTICAL
SimpleAdvect_cpp.log.txt          log.txt cmp IDENTICAL
Species_c.chem.txt                chem -> IDENTICAL
Species_c.dmp                     dmp cmp IDENTICAL (report-only)
Species_c.log.txt                 log.txt cmp IDENTICAL
Species_cpp.chem.txt              chem -> IDENTICAL
Species_cpp.dmp                   dmp cmp IDENTICAL (report-only)
Species_cpp.log.txt               log.txt cmp IDENTICAL
Species_cpp_utility.txt           utility -> IDENTICAL
Species_c_utility.txt             utility -> IDENTICAL
```

All 27 output files match the baseline exactly, including the four `.dmp` files.

### Statistics sums (Task 2b step 7 method)

Exact commands:

```
grep -c "phreeqc_linear_solver_statistics:" stderr.txt
grep "phreeqc_linear_solver_statistics:" stderr.txt | grep -c "backend=lapack"
grep "phreeqc_linear_solver_statistics:" stderr.txt | \
  awk '{for(i=1;i<=NF;i++){split($i,a,"="); if(a[1] ~ /^(solves|accepted|cl1_fallbacks|non_finite_inputs|ineligible_systems)$/) s[a[1]]+=a[2]}} END{for(k in s) print k"="s[k]}' | sort
grep "phreeqc_linear_solver_ineligible:" stderr.txt | \
  awk '{for(i=1;i<=NF;i++){split($i,a,"="); if(a[1]!="phreeqc_linear_solver_ineligible:") s[a[1]]+=a[2]}} END{for(k in s) if(s[k]!=0) print k"="s[k]}' | sort
```

Results:

```
statistics lines:            25
lines with backend=lapack:   25
solves=362  accepted=362  cl1_fallbacks=0  non_finite_inputs=0  ineligible_systems=6873
ineligible reasons (non-zero): ineligible_unknown_type=6873
```

Sample lines (verbatim):

```
phreeqc_linear_solver_statistics: backend=lapack solves=22 accepted=22 cl1_fallbacks=0 non_finite_inputs=0 ineligible_systems=17
phreeqc_linear_solver_ineligible: not_first_attempt=0 removing_unstable_phases=0 pitzer_or_sit_model=0 mass_water_switch=0 aqueous_only_iteration=0 numerical_derivatives=0 empty_system=0 ineligible_unknown_type=17
```

`stderr.txt` also contains a separate line type, `phreeqc_lapack_statistics:`
(the LAPACK backend's own counters), which is not a
`phreeqc_linear_solver_statistics:` line and therefore falls in the "other
stderr lines" bucket. Its distinct values, via
`grep -v -E "phreeqc_linear_solver_statistics:|phreeqc_linear_solver_ineligible:" stderr.txt | sort | uniq -c`,
include:

```
2  phreeqc_lapack_statistics: solves=0  accepted=0  cl1_fallbacks=0 non_finite_inputs=0 lapack_errors=0
4  phreeqc_lapack_statistics: solves=19 accepted=19 ... lapack_errors=0
3  phreeqc_lapack_statistics: solves=22 accepted=22 ... lapack_errors=0
6  phreeqc_lapack_statistics: solves=25 accepted=25 ... lapack_errors=0
10 phreeqc_lapack_statistics: solves=7  accepted=7  ... lapack_errors=0
```

The remaining 531 "other" stderr lines are PHREEQC run chatter (cell dumps,
component lists, `Beginning transport/reaction calculation`, `Done <test>.`,
`I am Groot.`/`I am root.`, etc.); the full `sort | uniq -c` output is in the
session transcript.

## Step 4 — Step differences

`python3 ~/phreeqc3/tests/petsc_linear_solver/tools/summarize_petsc_cl1_step_comparison.py newton_step_comparison.log --large-difference-threshold 1e-8`
(in the case directory), full output:

```
comparison records: 362
  LINEAR_SOLVE_ACCEPTED: 362
  cl1 return codes: {1: 356, 0: 6}
  system sizes: {2: 3, 4: 26, 5: 20, 6: 75, 7: 105, 9: 61, 10: 72}

accepted PETSc steps: normwise relative difference from the cl1 step
  median=2.456e-13  90th percentile=4.824e-01  99th percentile=3.531e+16  maximum=3.531e+16
  backward error: median=1.757e-18  maximum=6.963e-17

steps with relative_difference > 1e-08: 82 (grouped by unknown type at the largest difference)
  unknown type 16: count=76 maximum=5.077e+04
  unknown type 10: count=6 maximum=3.531e+16
  largest differences (call_index, system_size, relative_difference, largest_difference_index, unknown_type, backward_error):
    11, 10, 3.531e+16, 3, 10, 1.341e-33
    11, 10, 3.531e+16, 3, 10, 1.341e-33
    11, 10, 3.531e+16, 3, 10, 1.341e-33
    11, 10, 3.531e+16, 3, 10, 1.341e-33
    11, 10, 3.531e+16, 3, 10, 1.341e-33
    11, 10, 3.531e+16, 3, 10, 1.341e-33
    10, 9, 5.077e+04, 7, 16, 1.962e-40
    10, 9, 5.077e+04, 7, 16, 1.962e-40
    10, 9, 5.071e+04, 7, 16, 3.052e-40
    10, 9, 5.071e+04, 7, 16, 3.052e-40
```

### Records with `relative_difference > 1e-8` (all nine columns)

Command:

```
grep -v "^#" newton_step_comparison.log | awk '$3 == 0 && $6 > 1e-8' | sort -g -k6 -r
```

Total count: **82**. First 60 rows (columns: call_index system_size solve_status
cl1_return_code backward_error relative_difference relative_sum
largest_difference_index largest_difference_unknown_type):

```
11 10 0 0 1.341374e-33 3.530526e+16 3.530526e+16 3 10
11 10 0 0 1.341374e-33 3.530526e+16 3.530526e+16 3 10
11 10 0 0 1.341374e-33 3.530526e+16 3.530526e+16 3 10
11 10 0 0 1.341374e-33 3.530526e+16 3.530526e+16 3 10
11 10 0 0 1.341374e-33 3.530526e+16 3.530526e+16 3 10
11 10 0 0 1.341374e-33 3.530526e+16 3.530526e+16 3 10
10 9 0 1 1.961902e-40 5.077040e+04 5.077240e+04 7 16
10 9 0 1 1.961902e-40 5.077040e+04 5.077240e+04 7 16
10 9 0 1 3.051857e-40 5.070933e+04 5.071133e+04 7 16
10 9 0 1 3.051857e-40 5.070933e+04 5.071133e+04 7 16
14 10 0 1 1.618076e-39 4.398966e+04 4.399166e+04 8 16
14 10 0 1 1.618076e-39 4.398966e+04 4.399166e+04 8 16
14 10 0 1 1.618076e-39 4.398966e+04 4.399166e+04 8 16
14 10 0 1 1.618076e-39 4.398966e+04 4.399166e+04 8 16
14 10 0 1 1.618076e-39 4.398966e+04 4.399166e+04 8 16
14 10 0 1 1.618076e-39 4.398966e+04 4.399166e+04 8 16
11 9 0 1 1.537640e-39 5.077551e+03 5.079551e+03 7 16
11 9 0 1 1.537640e-39 5.077551e+03 5.079551e+03 7 16
11 9 0 1 5.726559e-40 5.071444e+03 5.073444e+03 7 16
11 9 0 1 5.726559e-40 5.071444e+03 5.073444e+03 7 16
13 10 0 1 3.309864e-39 1.405837e+02 1.425837e+02 8 16
13 10 0 1 3.309864e-39 1.405837e+02 1.425837e+02 8 16
13 10 0 1 3.309864e-39 1.405837e+02 1.425837e+02 8 16
13 10 0 1 3.309864e-39 1.405837e+02 1.425837e+02 8 16
13 10 0 1 3.309864e-39 1.405837e+02 1.425837e+02 8 16
13 10 0 1 3.309864e-39 1.405837e+02 1.425837e+02 8 16
9 9 0 1 1.717614e-37 1.275375e+02 1.295375e+02 7 16
9 9 0 1 1.717614e-37 1.275375e+02 1.295375e+02 7 16
9 9 0 1 1.252017e-37 1.273841e+02 1.293841e+02 7 16
9 9 0 1 1.252017e-37 1.273841e+02 1.293841e+02 7 16
15 10 0 1 4.522908e-34 9.517961e-01 2.951796e+00 8 16
15 10 0 1 4.522908e-34 9.517961e-01 2.951796e+00 8 16
15 10 0 1 4.522908e-34 9.517961e-01 2.951796e+00 8 16
15 10 0 1 4.522908e-34 9.517961e-01 2.951796e+00 8 16
15 10 0 1 4.522908e-34 9.517961e-01 2.951796e+00 8 16
15 10 0 1 4.522908e-34 9.517961e-01 2.951796e+00 8 16
15 7 0 1 7.870213e-25 7.942513e-01 2.794251e+00 5 16
14 7 0 1 4.678349e-27 4.824021e-01 2.482402e+00 5 16
17 7 0 1 1.927143e-20 4.263595e-01 1.573641e+00 5 16
12 10 0 1 4.758209e-36 3.530476e-01 2.353048e+00 8 16
12 10 0 1 4.758209e-36 3.530476e-01 2.353048e+00 8 16
12 10 0 1 4.758209e-36 3.530476e-01 2.353048e+00 8 16
12 10 0 1 4.758209e-36 3.530476e-01 2.353048e+00 8 16
12 10 0 1 4.758209e-36 3.530476e-01 2.353048e+00 8 16
12 10 0 1 4.758209e-36 3.530476e-01 2.353048e+00 8 16
1 9 0 1 1.444099e-24 3.516507e-01 2.351651e+00 7 16
8 9 0 1 2.348885e-35 3.202492e-01 2.320249e+00 7 16
8 9 0 1 2.348885e-35 3.202492e-01 2.320249e+00 7 16
8 9 0 1 3.729157e-35 3.198640e-01 2.319864e+00 7 16
8 9 0 1 3.729157e-35 3.198640e-01 2.319864e+00 7 16
16 7 0 1 4.580703e-23 2.624301e-01 2.262430e+00 5 16
12 9 0 1 2.590265e-34 3.204523e-02 2.032045e+00 7 16
12 9 0 1 2.590265e-34 3.204523e-02 2.032045e+00 7 16
12 9 0 1 1.057799e-34 3.200669e-02 2.032007e+00 7 16
12 9 0 1 1.057799e-34 3.200669e-02 2.032007e+00 7 16
21 7 0 1 2.979433e-18 3.184549e-02 2.031845e+00 5 16
19 7 0 1 4.875550e-17 7.728948e-03 2.007729e+00 5 16
18 7 0 1 1.778161e-18 5.381353e-03 1.994619e+00 5 16
20 7 0 1 6.381129e-18 3.369907e-03 2.003370e+00 5 16
2 9 0 1 9.692209e-23 3.105289e-03 2.003105e+00 7 16
```

### Unknown-type code → name

From `~/phreeqc3/src/global_structures.h:82-99` (verbatim):

```
82: #define MB 10
83: #define ALK 11
84: #define CB 12
85: #define SOLUTION_PHASE_BOUNDARY 13
86: #define MU 14
87: #define AH2O 15
88: #define MH 16
89: #define MH2O 17
```

The only codes appearing in the last column of the >1e-8 records are **10 = MB**
and **16 = MH**.

### Counts per unknown type

Command:

```
grep -v "^#" newton_step_comparison.log | awk '$3 == 0 && $6 > <thr> {c[$9]++} END{for(k in c) print "type",k,"count",c[k]}'
```

```
relative_difference > 1e-8 :  type 16 (MH) = 76,  type 10 (MB) = 6   (total 82)
relative_difference > 1e-3 :  type 16 (MH) = 57,  type 10 (MB) = 6   (total 63)
```

## Summary (facts)

- **Step 1.** Of the 12 files that differed in Task 2b, 11 are `WITHIN_TOLERANCE`
  at relative 1e-6 / absolute 1e-12: all 7 `.chem.txt` files and 4 of the 5
  `_utility.txt` files. Exactly one file, `Advect_cpp_units_utility.txt`, has
  lines exceeding that tolerance: **9 line pairs** (lines 57, 75, 81, 89, 117,
  145, 150, 156, 159). The round-off-scale `Total alkalinity` / `Electrical
  balance` values reported in P2-D17 (c) are below this tolerance and do not
  appear as differing lines here.
- **Step 2.** The differing redox line is 117 (`pe = -0.602` vs `-0.558`), inside
  the second batch-reaction block, under `Using solution 2.` (line 105). In
  `units.pqi`, solution 2 is `pH 7 charge` with only Na and Cl at 1.0 mol/L; it
  specifies no `pe`, no redox master species, and no O or C. The differing
  printed quantities are pe, H(0)/O(0), H2, O2, and the H2(g)/O2(g)/CH4(g)
  saturation indices; pH, ionic strength, density, volume, Total H, Total O, and
  iteration count (9) are identical.
- **Step 3.** Comparison mode (`PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1=1`, MPI 1
  rank) reproduced the baseline **exactly**: all 27 output files `IDENTICAL`,
  including the four `.dmp` files. Statistics: 25 `phreeqc_linear_solver_statistics:`
  lines, all `backend=lapack`; sums `solves=362 accepted=362 cl1_fallbacks=0
  non_finite_inputs=0 ineligible_systems=6873`; the only non-zero ineligible
  reason is `ineligible_unknown_type=6873`.
- **Step 4.** 362 comparison records, all `LINEAR_SOLVE_ACCEPTED`. 82 records have
  `relative_difference > 1e-8`: 76 with largest-difference unknown type **16
  (MH)** and 6 with type **10 (MB)**. At `> 1e-3`: 63 records, 57 type 16 (MH)
  and 6 type 10 (MB). The largest relative differences (3.5e+16, 5.1e+04) carry
  backward errors of 1e-33 and 1e-40.

## Reviewer decisions required

1. **Hypothesis status.** The facts are consistent with the P2-D17 (c) hypothesis
   (solution 2 has no redox poise; the differing quantities are exactly the
   redox-dependent ones; the large step-difference records are concentrated in
   MH/MB unknowns with negligible backward error). This task reports facts only;
   whether the hypothesis is *confirmed*, and whether the reciprocal-condition
   check of design §6.6 should be implemented, is a reviewer decision.
2. **`phreeqc_lapack_statistics:` line type.** `stderr.txt` contains a second
   statistics line type, `phreeqc_lapack_statistics:`, not covered by the Task 2b
   step 7 counting method (which counts only `phreeqc_linear_solver_statistics:`).
   Its per-instance sums (solves 362, accepted 362, lapack_errors 0) are
   consistent with the primary statistics, but the counting method does not name
   it. Flagging for the reviewer; no change made.
3. **Tool label.** `summarize_petsc_cl1_step_comparison.py` prints "accepted
   PETSc steps" although this run's backend is `lapack`. The numbers are the
   LAPACK-vs-cl1 step differences. Reported as-is; no tool was changed.
