# Task 0: Source Verification and Baseline (read-only)

## Context

PETSc is being integrated into PHREEQC to replace the linear solve inside the Newton-Raphson loop. The design rests on several assumptions about the PHREEQC source. This task checks those assumptions and records a baseline from the **unmodified** code.

**Do not write or modify any source code or build files in this task.** You may only create the files listed under "Outputs".

## Rules

1. Read-only for all existing files. Create only the files listed under "Outputs".
2. Every factual claim about the source must include a file path and line numbers, and quote the relevant lines. If you cannot find something, write "NOT FOUND" and list where you looked. Do not guess.
3. If the code is ambiguous on a point, write "AMBIGUOUS" and explain why. Do not resolve it by assumption.
4. Use Plan mode first. Present the plan, then execute it after approval.
5. Do not run `git commit`.

## Part A: Environment

- **A1. Tree layout.** Report which of the phreeqc executable, IPhreeqc, and PhreeqcRM are present, and where. Also report the output of `git status --short` and `git rev-parse HEAD`.
- **A2. Build system.**
  - List the CMake project(s) and the exact names of every target that compiles PHREEQC core sources (the file defining `Phreeqc::ineq`).
  - Identify the phreeqc executable target, and the IPhreeqc and PhreeqcRM targets if present.
  - For each target, state whether the core sources are built as a static library, a shared library, or compiled directly into it.
  - Note whether existing `target_link_libraries` calls for these targets use the keyword form (`PUBLIC`/`PRIVATE`) or the plain form.
- **A3. PETSc.** Report the following:
  - `PETSC_DIR` and `PETSC_ARCH`. If PETSc comes from an environment module, give the module name and the paths it sets.
  - The version from `petscversion.h` (MAJOR.MINOR.SUBMINOR).
  - From `petscconf.h`, whether each of these is defined: `PETSC_USE_COMPLEX`, `PETSC_USE_REAL_DOUBLE`, `PETSC_HAVE_MPIUNI`, `PETSC_USE_64BIT_INDICES`, `PETSC_USE_DEBUG`.
  - The pkg-config file name and full path. Check for both `PETSc.pc` and `petsc.pc`.
- **A4. Toolchain.** Report the C++ compiler, its version, and the C++ standard the project uses.

## Part B: Source Facts (quote `file:line` for each)

- **B1. Augmented Newton matrix.** Identify the storage into which `jacobian_sums()` writes the Jacobian and `residuals()` writes the residual. Record its exact identifier in the report as `<UPSTREAM_NEWTON_MATRIX_NAME>`; later tasks refer to it only through that placeholder. Give its declaration and exact type (plain pointer, `std::vector`, or other), and whether it is a `Phreeqc` member. Show its allocation size, and the element-access patterns used in `residuals()` and `jacobian_sums()`. Confirm or refute each of these: row-major storage; leading dimension `count_unknowns + 1`; residual stored in column `count_unknowns`; 0-based indexing.
- **B2. `delta`.** Give its declaration, type, and allocation size, and state whether it is 0-based.
- **B3. `ineq()`.** Report:
  - its exact signature and the file that defines it;
  - whether it calls itself, directly or indirectly;
  - the return value names on success and on failure;
  - every member variable it writes other than `delta`, and for each, whether any caller reads it afterwards;
  - whether it reads `delta` before writing it;
  - whether it modifies `<UPSTREAM_NEWTON_MATRIX_NAME>`.
- **B4. Unknown types.**
  - List the exact names and values of all unknown-type constants.
  - Give the expression used to read an unknown's type (for example `x[i]->type`) and the declared type of `x`.
- **B5. Equality status.** Treat the types corresponding to *mass balance, alkalinity, charge balance, ionic strength, activity of water, hydrogen/pe balance, mass of water* (use the actual constant names from B4) separately:
  - For each one, state whether `ineq()` moves its row to the inequality block, drops it, rescales it, or treats it specially in any other way. Quote the deciding code.
  - Then answer: when **every** unknown has one of these types, does `ineq()` add any extra rows or columns (for example for mass of water, or optimisation rows)? Quote the code.
- **B6. Residual sign.**
  - Quote how `residuals()` defines the residual and fills the RHS column.
  - Quote one representative case in `reset()` showing how `delta` is applied to an unknown.
  - State the implied relation (J·δ = r or J·δ = −r) **only** if the code makes it unambiguous. Otherwise write "AMBIGUOUS"; a later comparison test will settle it.
- **B7. `LDBLE`.** Give its definition and the build flags that change it.
- **B8. `diagonal_scale`.** Show where it is applied relative to `ineq()`: is `<UPSTREAM_NEWTON_MATRIX_NAME>` already scaled when `ineq()` starts?
- **B9. `Phreeqc` lifecycle.** Give `file:line` for every constructor (including the copy constructor), any assignment operator, `InternalCopy()` or equivalent, and the destructor.
- **B10. `main()`.** Give the file and the full body of `main()` for the phreeqc executable.

## Part C: Baseline

- **C1. Instrumented inputs.**
  - Create `tests/petsc_linear_solver/inputs/ex1_seawater_speciation.pqi` and `tests/petsc_linear_solver/inputs/ex2_gypsum_anhydrite_equilibria.pqi` as copies of PHREEQC examples ex1 and ex2.
  - In each copy, add a `SELECTED_OUTPUT` block placed before the first `SOLUTION` block, containing:
    - `-file` (`ex1_seawater_speciation.sel` / `ex2_gypsum_anhydrite_equilibria.sel`) and `-high_precision true`;
    - pH, pe, ionic strength, mass of water, and temperature;
    - `-totals` for every element defined in the input's SOLUTION blocks;
    - `-molalities` for up to 20 major species;
    - `-saturation_indices` for the phases relevant to the input;
    - for ex2, `-equilibrium_phases` for its pure phases.
  - Before using any `SELECTED_OUTPUT` identifier, confirm it exists in the source that reads the block, and quote it.
  - Change nothing else in the copies.
  - These files are **frozen** after the baseline is recorded.
- **C2. Baseline run.**
  - Build the unmodified code as currently configured.
  - Run both inputs with the standard database from the tree.
  - Save the `.out` and `.sel` files to `tests/petsc_linear_solver/baseline/`.
  - Write `tests/petsc_linear_solver/baseline/README.md` with the commit hash, the build command, and the exact run commands.
- **C3. Iteration counts.** Determine whether the main output reports Newton iteration counts per calculation. If it does, give a `grep` (or similar) command that extracts them, and record the baseline counts in the README.

## Outputs

- `docs/petsc_linear_solver/task0_source_verification_report.md`: sections A, B, and C with items numbered as above, followed by a section titled "Stop conditions" (below).
- `tests/petsc_linear_solver/inputs/ex1_seawater_speciation.pqi` and `tests/petsc_linear_solver/inputs/ex2_gypsum_anhydrite_equilibria.pqi`.
- `tests/petsc_linear_solver/baseline/` with the outputs and README.

## Stop Conditions

For each condition, report YES or NO with a one-line justification:

- **S1.** The PETSc version is below 3.19, the build uses complex scalars, or the precision is not double.
- **S2.** The `<UPSTREAM_NEWTON_MATRIX_NAME>` layout differs from: row-major, leading dimension `count_unknowns + 1`, RHS in column `count_unknowns`, 0-based.
- **S3.** Any of the B5 types is treated specially by `ineq()`, or `ineq()` adds extra rows for systems made only of B5 types.
- **S4.** `ineq()` writes variables other than `delta` that callers use, reads `delta` before writing it, or calls itself.

If any answer is YES or AMBIGUOUS, **stop** and wait for instructions. Do not proceed to any other task.
