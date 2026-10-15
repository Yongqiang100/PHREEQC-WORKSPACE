# Phase 1B Task 0: Profile the Linear-Solve Share on a Real Input (read-only)

## Context

Phase 1A integrated PETSc into PHREEQC's Newton linear solve and validated it on two small examples. It showed that the integration is correct; it did not measure speed. Before more unknown types are made eligible (Phase 1B), we need three numbers for a representative real input:

1. **Share.** What fraction of the run time is spent in the linear-solve step: `Phreeqc::ineq` and everything it calls, `cl1()` in particular. This bounds any speed-up a faster solver can give (Amdahl's law).
2. **Coverage.** What fraction of `ineq()` calls are eligible for PETSc today, and why the others are not.
3. **Cost per solve.** Roughly how long one `cl1()`-path call takes compared with one PETSc solve.

**This task changes no source file, build file, test input, baseline, or script.** It builds one extra profiling tree, runs PHREEQC, and writes a report.

**Inputs.** The reviewer has copied the input file and its database into `build/profiling_inputs/`. The slash command passes their file names:

- `INPUT_NAME` is the input file;
- `DATABASE_NAME` is the database.

The `build/` directory is excluded from git, so the input never enters the repository. Do not copy it anywhere else.

## Rules

1. Edit only `docs/petsc_linear_solver/phase1b_profiling_report.md`. Write all run output under `build/` only.
2. Never change a flag, option, or command given below. If one fails, report it with its output and continue with the steps that do not depend on it.
3. Do not edit, copy, or print the content of the input file beyond what a step requires, such as checking that it exists.
4. Use Plan mode first. Present the plan, then execute only after the reviewer replies `APPROVED`.
5. Do not run `git commit`.

## Run Command Form

Every PHREEQC run uses this form, from a directory two levels below the repository root (`build/<tree>/profiling`):

```
<executable> ../../profiling_inputs/INPUT_NAME profiling.out ../../profiling_inputs/DATABASE_NAME profiling.log
```

`<executable>` is `../../../build/<tree>/phreeqc`. Replace `INPUT_NAME` and `DATABASE_NAME` with the names given by the command. The command line is identical in every tree, so the paths that PHREEQC writes into `profiling.out` are identical too.

**Timing a run.** Use exactly this pattern, so that no external tool is required:

```
start=$(date +%s.%N); <run command>; status=$?; end=$(date +%s.%N); python3 -c "print('wall_seconds', $end - $start, 'exit', $status)"
```

## Steps

0. **Pre-checks.**
   - `echo "$PETSC_DIR"` must print `/opt/petsc/3.26.0`.
   - `ls -l build/profiling_inputs/` must list `INPUT_NAME` and `DATABASE_NAME`.
   - `git status --short` must be empty.
   - Run `cmake --build build/petsc_off --target phreeqc -j 8` and `cmake --build build/petsc_on --target phreeqc -j 8`. Both should report nothing to do; report what they print.
   - Check whether `perf` is usable: run `command -v perf`, then `cat /proc/sys/kernel/perf_event_paranoid`. `perf` is usable if it exists and the value is 2 or lower. Report both outputs.

1. **Reference timing, OFF build.** In `build/petsc_off/profiling`, run the input 3 times with the timing pattern. Report each wall time, the median, and the exit status. Keep the outputs of the last run.

   If the median exceeds 900 seconds, stop after this step and report: the input is too long for this task.

2. **Timing and coverage, ON build.** In `build/petsc_on/profiling`, run 3 times with `PETSC_OPTIONS="-phreeqc_petsc_statistics"` and the timing pattern, capturing stderr with `2> stderr_run<k>.txt`. Report each wall time, the median, the exit status, and the full `phreeqc_petsc_ineligible:` and `phreeqc_petsc_statistics:` lines from run 1. If the input runs several simulations or `Phreeqc` objects, there may be more than one pair of lines; report all of them.

3. **PETSc event log, ON build.** In `build/petsc_on/profiling_log_view`, run once (untimed) with `PETSC_OPTIONS="-phreeqc_petsc_statistics -log_view"`, capturing stdout and stderr separately (`1> stdout.txt 2> stderr.txt`). The PETSc event log goes to stdout. Report, from its event table, the lines for `KSPSolve`, `PCSetUp`, `MatLUFactorNum`, `MatSolve`, `MatAssemblyBegin`, `MatAssemblyEnd`, `MatMult`, and `VecNorm`, each with its count and maximum time, plus the table's header lines. Any event that does not appear: report it as absent.

4. **Sanity comparison, ON against OFF (information only, not pass/fail).**
   - For every `.sel` file the input writes, run `python3 tests/petsc_linear_solver/tools/compare_selected_output_to_baseline.py build/petsc_off/profiling/<file> build/petsc_on/profiling/<file>` and report the verdict line and any failing columns.
   - Run `python3 tests/petsc_linear_solver/tools/compare_main_output_to_baseline.py build/petsc_off/profiling/profiling.out build/petsc_on/profiling/profiling.out` and report its first 10 lines of output.
   - If the input writes no `.sel` file, say so.

5. **Profile the OFF build.** The goal is the inclusive time (self plus callees) of `Phreeqc::ineq`, `Phreeqc::ineq_cl1_original`, and `cl1`.
   - Find the exact symbol for `cl1` with `grep -n "cl1(" src/Phreeqc.h | head`. Report it.
   - Configure and build a dedicated tree:
     ```
     cmake -S . -B build/profile_off -DCMAKE_BUILD_TYPE=Release -DPHREEQC_USE_PETSC=OFF "-DCMAKE_CXX_FLAGS=-g -fno-omit-frame-pointer"
     cmake --build build/profile_off --target phreeqc -j 8
     ```
   - **If `perf` is usable (step 0):** in `build/profile_off/profiling`, run:
     ```
     perf record -F 999 -g -o perf.data -- ../../../build/profile_off/phreeqc ../../profiling_inputs/INPUT_NAME profiling.out ../../profiling_inputs/DATABASE_NAME profiling.log
     perf report -i perf.data --children --stdio --sort symbol 2>/dev/null > perf_children.txt
     ```
     Report the header of `perf_children.txt`, the first 40 symbol lines, and the `Children` and `Self` percentages for each of these symbols, wherever they appear: `Phreeqc::model`, `Phreeqc::ineq`, `Phreeqc::ineq_cl1_original`, `cl1` (with the exact symbol name), `Phreeqc::jacobian_sums`, `Phreeqc::residuals`, `Phreeqc::molalities`, `Phreeqc::reset`, `Phreeqc::gammas`, `Phreeqc::mb_sums`.
   - **If `perf` is not usable:** configure and build a gprof tree instead:
     ```
     cmake -S . -B build/profile_off_gprof -DCMAKE_BUILD_TYPE=Release -DPHREEQC_USE_PETSC=OFF "-DCMAKE_CXX_FLAGS=-pg" "-DCMAKE_EXE_LINKER_FLAGS=-pg"
     cmake --build build/profile_off_gprof --target phreeqc -j 8
     ```
     Run once in `build/profile_off_gprof/profiling`. Then run `gprof -b ../../../build/profile_off_gprof/phreeqc gmon.out > gprof_report.txt`. Report the first 30 lines of the flat profile, and, from the call graph, the line for each symbol listed above, with `% time`, `self`, and `children`.
   - Symbols missing from the profile: report them as absent. They may have been inlined.

6. **Derived numbers.** Compute these exactly as defined, show the inputs to each formula, and round to 3 significant figures.
   - `S` = inclusive share of `Phreeqc::ineq` from step 5, as a fraction. If `Phreeqc::ineq` is absent, use `Phreeqc::ineq_cl1_original`, and say so.
   - `S_cl1` = inclusive share of `cl1` from step 5.
   - `amdahl_ceiling = 1 / (1 - S)`: the largest possible whole-run speed-up if the linear-solve step cost nothing.
   - `ineq_calls` = `solves + ineligible_systems`, summed over all statistics-line pairs of step 2, run 1.
   - `eligible_fraction = solves / ineq_calls`.
   - `cl1_seconds_per_call = S * median_OFF_wall / ineq_calls`.
   - `petsc_kspsolve_seconds_per_call` = `KSPSolve` maximum time divided by its count, from step 3.
   - `on_vs_off_wall_ratio = median_ON_wall / median_OFF_wall`, from steps 1 and 2.

## Output

Write `docs/petsc_linear_solver/phase1b_profiling_report.md` with these sections:

- **Input:** file names, the size of each file in bytes, and the number of `SOLUTION`, `EQUILIBRIUM_PHASES`, `GAS_PHASE`, `EXCHANGE`, `SURFACE`, `KINETICS`, `SOLID_SOLUTIONS`, `TRANSPORT`, and `ADVECTION` keyword lines in the input. Count them with `grep -c -i "^ *<KEYWORD>"`. These counts and the file sizes are the only information the report may contain about the input.
- **Environment:** CPU model (`lscpu | grep "Model name"`), PETSc version, compiler, and the profiler used.
- **Summary table:** one row per derived number from step 6.
- **Step-by-step results:** the full commands and the outputs listed above.
- **Observations:** statements of fact only. Do not recommend anything.

Stop after the report.
