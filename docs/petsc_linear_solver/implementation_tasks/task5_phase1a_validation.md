# Task 5: Phase 1A Validation

## Context

PETSc has been integrated into PHREEQC's Newton linear solve for Phase 1A (serial, dense LU, eligible systems only). This task runs the Phase 1A acceptance tests and writes up the results.

**Do not modify any source file, build file, test input, or baseline file in this task.** If a check fails, report it. Do not fix it.

**Prerequisites:**

- Task 0–Task 4 are complete and approved.
- `compare_selected_output_to_baseline.py`, `compare_main_output_to_baseline.py` and `summarize_petsc_cl1_step_comparison.py` are in `tests/petsc_linear_solver/tools/` (reviewer-supplied).
- `docs/petsc_linear_solver/task0_reviewer_decisions.md` is authoritative for build directories (D12), output comparison (D11) and the eligibility rule (D8).
- Baseline files are in `tests/petsc_linear_solver/baseline/`. The exact run commands are in `tests/petsc_linear_solver/baseline/README.md`.

## Rules

1. Read-only for all existing files. Create only the outputs listed below.
2. Never change the tolerances passed to the scripts (defaults: relative tolerance 1e-6; absolute tolerance 1e-14 for amounts and 1e-6 for logarithmic columns). Never edit the baseline files, the inputs, or the scripts.
3. If a step fails, still run every remaining step that does not depend on it, and report all results.
4. Use Plan mode first. Present the plan, then execute only after approval.
5. Do not run `git commit`.

## Steps

**Where to run.** PHREEQC writes the input and database paths into the main output, so every run must use the baseline README command **exactly**, changing only the executable path, and must run in a directory at the same depth as `build/baseline/run`:

- ON-build steps run in `build/petsc_on/<step directory>` with `../../../build/petsc_on/phreeqc`.
- The OFF-build step runs in `build/petsc_off/<step directory>` with `../../../build/petsc_off/phreeqc`.
- Step directories: `step2_ex1_petsc_steering`, `step3_ex2_eligibility_gate`, `step4_ex1_comparison_mode`, `step5_ex2_comparison_mode`, `step6_memory_check`, `step7_off_build_regression`.

After each step, copy its `.out` and `.sel` files, any comparison log, and a text file with the captured `stderr` into `tests/petsc_linear_solver/results/<step directory>/`. Pass PETSc options through the `PETSC_OPTIONS` environment variable. Before step 1, confirm that `echo "$PETSC_DIR"` prints `/opt/petsc/3.26.0`; if not, stop.

**Statistics lines.** With `-phreeqc_petsc_statistics`, each run prints on `stderr` one line beginning `phreeqc_petsc_ineligible:` (counts per ineligibility reason) and one beginning `phreeqc_petsc_statistics:` (`solves`, `accepted`, `cl1_fallbacks`, `non_finite_inputs`, `petsc_errors`, `ineligible_systems`, `solver_slots_created`).

1. **Unit tests.** Run `ctest --test-dir build/petsc_on -R "test_initialization|test_newton_matrix_transfer|test_newton_solver" --output-on-failure` and report the summary.

2. **ex1, PETSc steering.**
   - Run `ex1_seawater_speciation.pqi` with `PETSC_OPTIONS="-phreeqc_petsc_statistics"`. Capture `stderr`.
   - Expected statistics: `solves > 0`, `accepted == solves`, `cl1_fallbacks == 0`, `non_finite_inputs == 0`, `petsc_errors == 0`. Report `ineligible_systems` and the full `phreeqc_petsc_ineligible:` line. If `solves == 0`, ex1 never used PETSc: report which ineligibility reasons were counted; do not treat this as a pass.
   - Run `python3 tests/petsc_linear_solver/tools/compare_selected_output_to_baseline.py tests/petsc_linear_solver/baseline/ex1_seawater_speciation.sel tests/petsc_linear_solver/results/step2_ex1_petsc_steering/ex1_seawater_speciation.sel --json-report tests/petsc_linear_solver/results/step2_ex1_petsc_steering/selected_output_comparison.json`. Report the verdict and the full table.
   - If the baseline README defines an iteration-count extraction command, run it on both `.out` files and report the counts side by side. Differences are reported, not judged.

3. **ex2, gate test.**
   - Run `ex2_gypsum_anhydrite_equilibria.pqi` with `PETSC_OPTIONS="-phreeqc_petsc_statistics"`.
   - Expected statistics: `ineligible_unknown_type > 0` in the `phreeqc_petsc_ineligible:` line, because calculations with pure phases must not use PETSc. Report both statistics lines in full.
   - Compare the selected output with the baseline as in step 2, and report the iteration counts.

4. **ex1, comparison mode.**
   - Run `ex1_seawater_speciation.pqi` with `PETSC_OPTIONS="-phreeqc_petsc_statistics -phreeqc_petsc_compare_with_cl1 -phreeqc_petsc_comparison_log petsc_cl1_step_comparison.log"` (the log is written in the run directory and then copied to the results directory).
   - Run `python3 tests/petsc_linear_solver/tools/summarize_petsc_cl1_step_comparison.py tests/petsc_linear_solver/results/step4_ex1_comparison_mode/petsc_cl1_step_comparison.log` and report its full output.
   - In this mode cl1 steers the calculation, so results must match the baseline exactly: `cmp` the `.sel` file against the baseline (identical), and run `compare_main_output_to_baseline.py` on the `.out` file (`IDENTICAL`).

5. **ex2, comparison mode.** Same as step 4, using `ex2_gypsum_anhydrite_equilibria.pqi` and the step directory `step5_ex2_comparison_mode`. Compare all three `.sel` files, including `ex2.sel`.

6. **Memory check** (decision D14; valgrind cannot execute the installed PETSc library).
   - **PETSc objects.** Run ex1 and ex2 with `build/petsc_on/phreeqc` and `PETSC_OPTIONS="-phreeqc_petsc_statistics -malloc_debug -malloc_dump"`. Report the full `stdout` and `stderr`. Any line reporting memory still allocated at `PetscFinalize` is a failure, with one exception (decision D16): a line reporting `PetscFPTrapPush()` in `src/sys/error/fp.c` is reported but not a failure if the statistics line of the same run shows `petsc_errors > 0`.
   - **AddressSanitizer.** Configure and build a separate tree:
     ```
     cmake -S . -B build/petsc_on_asan -DCMAKE_BUILD_TYPE=Release -DPHREEQC_USE_PETSC=ON \
           "-DCMAKE_CXX_FLAGS=-fsanitize=address -fno-omit-frame-pointer" \
           "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address"
     cmake --build build/petsc_on_asan --target phreeqc -j 8
     ```
     Run ex1 and ex2 in `build/petsc_on_asan/step6_memory_check` with `../../../build/petsc_on_asan/phreeqc`, `ASAN_OPTIONS=detect_leaks=1` and `PETSC_OPTIONS="-phreeqc_petsc_statistics"`. Report the exit status and the full sanitizer output. Any AddressSanitizer error is a failure. A leak whose stack contains a function from `src/petsc_linear_solver/` or `Phreeqc::ineq` is a failure; list other leak reports separately, without judging them.
   - Do not change any flag or option above. If a build or run fails, report it.

7. **OFF build regression.** Build in `build/petsc_off` (decision D12). Run ex1 and ex2 in `build/petsc_off/step7_off_build_regression`. `cmp` every `.sel` file against the baseline (identical), and run `compare_main_output_to_baseline.py` on both `.out` files (`IDENTICAL`).

## Output

Write `docs/petsc_linear_solver/task5_validation_report.md` containing:

- a summary table with one row per step: step, pass/fail/info, key numbers;
- for each step, the full command used and the full tool output;
- the observations below, kept as statements of fact (no fixes proposed):
  - any statistics that differ from the expected values;
  - any column that failed the tolerance comparison;
  - whether `summarize_petsc_cl1_step_comparison.py` flagged a sign inversion, or any unknown type with large differences;
  - iteration-count differences between PETSc and baseline.

Stop after the report.
