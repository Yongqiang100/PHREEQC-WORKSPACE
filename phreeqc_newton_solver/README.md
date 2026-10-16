# phreeqc_newton_solver: a Drop-in Newton Linear-Solver Overlay for PHREEQC and PhreeqcRM

This folder adds an alternative linear solver to the Newton iteration of PHREEQC. That means the
standalone `phreeqc` program (USGS **phreeqc3**), or **PhreeqcRM**, the reaction module for
transport codes. You copy the folder into an official source tree and rebuild with **one extra
CMake flag**. You never edit PHREEQC's files by hand.

> **Default behaviour is unchanged.** Unless you opt in, every Newton step is solved by PHREEQC's own
> `cl1()` solver, exactly as in upstream PHREEQC. The new solvers are switched on with an environment
> variable (see [Using the solvers](#using-the-solvers)).

## Requirements

- An official **phreeqc3 v3.9.0** or **PhreeqcRM v3.9.0** source tree
  (`https://github.com/usgs-coupled/phreeqc3`, `https://github.com/usgs-coupled/phreeqcrm`).
  Other versions are refused at configure time (see [Supported versions](#supported-versions)).
- Or a **flat PhreeqcRM tree** (phreeqcrm-flat, branch `main`): the same PhreeqcRM v3.9.0 files
  with the engine in `src/phreeqc/` and the shared helpers in `src/phreeqc_common/`.
- CMake ≥ 3.19, a C++11 compiler, LAPACK (for example OpenBLAS), and POSIX threads.
- For PhreeqcRM with MPI: the MPI that PhreeqcRM itself is built with. The overlay makes no MPI calls.

## Installation

Work in an empty directory that contains the release tarball. Each source tree gets its own
build directory.

```bash
tar -xzf phreeqc_newton_solver-1.1.0.tar.gz          # creates ./phreeqc_newton_solver/

# PhreeqcRM
git clone --branch v3.9.0 https://github.com/usgs-coupled/phreeqcrm.git
cp -r phreeqc_newton_solver phreeqcrm/
cmake -S phreeqcrm -B build-phreeqcrm -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PROJECT_INCLUDE=$PWD/phreeqcrm/phreeqc_newton_solver/cmake/attach.cmake
cmake --build build-phreeqcrm -j 8
ctest --test-dir build-phreeqcrm -R newton_solver     # 4 overlay tests

# phreeqc3 (standalone phreeqc program): the same steps with its own tree and build directory
git clone --branch v3.9.0 https://github.com/usgs-coupled/phreeqc3.git
cp -r phreeqc_newton_solver phreeqc3/
cmake -S phreeqc3 -B build-phreeqc3 -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PROJECT_INCLUDE=$PWD/phreeqc3/phreeqc_newton_solver/cmake/attach.cmake
cmake --build build-phreeqc3 -j 8
ctest --test-dir build-phreeqc3 -R newton_solver
```

For a flat PhreeqcRM tree (phreeqcrm-flat), use the PhreeqcRM commands with that tree's directory name.

The flag is all you add. Your usual options, such as `-DPHREEQCRM_BUILD_MPI=ON`, work as before.
To make `lapack` the default backend at build time, add `-DNEWTON_SOLVER_DEFAULT_BACKEND=lapack`.
The default is `cl1`; only `cl1` and `lapack` are accepted.

**What happens at configure time:**
1. The overlay checks the SHA-256 of three PHREEQC files (`Phreeqc.h`, `model.cpp`, `Phreeqc.cpp`) against `SUPPORTED_UPSTREAM.txt`.
2. It checks that each of its **five one-line edits** finds its exact anchor text **once**.
3. Only then does it write the edits, keeping `*.phreeqc_newton_solver.orig` copies.
4. It adds its own sources to the PHREEQC target.

A second configure detects that the edits are already present, and does nothing.

**Removing it** restores the three files byte for byte. Run this from the same directory as above:

```bash
cmake -DTREE=$PWD/phreeqcrm -P phreeqcrm/phreeqc_newton_solver/cmake/detach.cmake
git -C phreeqcrm diff --stat          # prints nothing: the clone is back to upstream
```

Use `phreeqc3` in place of `phreeqcrm` for the other tree.

## Using the Solvers

All control is through environment variables, read when PHREEQC starts. With MPI, pass them to the ranks, for example `mpiexec -n 4 -x PHREEQC_LINEAR_SOLVER=lapack -x OPENBLAS_NUM_THREADS=1 ./app`. Always use `OPENBLAS_NUM_THREADS=1` with PhreeqcRM.

| Variable | Values (default first) | Meaning |
|---|---|---|
| `PHREEQC_LINEAR_SOLVER` | `cl1` (or the build default, see Installation), `lapack` | `lapack` solves eligible Newton systems with LAPACK (`dgetrf`/`dgetrs`); every other call still goes to `cl1()` |
| `PHREEQC_LINEAR_SOLVER_CONDITION_CHECK` | `gate`, `log`, `off` | Rejects ill-conditioned systems (back to `cl1()`); `log` records only |
| `PHREEQC_LINEAR_SOLVER_CONDITION_METRIC` | `row`, `row_column` | Conditioning measure; `row_column` is required for `cl1_problem` mode |
| `PHREEQC_LINEAR_SOLVER_MIN_RCOND` | `1e-8` | Gate threshold on the reciprocal condition number |
| `PHREEQC_LINEAR_SOLVER_INTERCEPT` | `entry`, `cl1_problem_shadow`, `cl1_problem` | Where the solver intervenes (see below) |
| `PHREEQC_LINEAR_SOLVER_STATISTICS` | unset, `1`, `summary`, `all` | Statistics on stderr: per solver object, per process, or both |
| `PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1` | unset, `1` | Comparison mode: `cl1()` still steers, and both steps are logged |
| `PHREEQC_LINEAR_SOLVER_COMPARE_LOG` | `newton_step_comparison.log` | Log path; `%r` = MPI rank, `%o` = object, `%p` = process id |
| `PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG`, `…_LOG_EVERY` | `cl1_problem_shadow.log`, `1` | Shadow-mode log and sampling |
| `PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_MAX_COMPONENTWISE_ERROR` | `1e-8` | Per-row residual check in `cl1_problem` mode |

### The Three Interception Modes

- **`entry`** (default with `lapack`): only **aqueous-only** Newton systems are eligible, i.e. systems without pure phases, gases, solid solutions, exchange or surfaces. These are solved as a plain square linear system. Results agree with `cl1()` to round-off, except in weakly poised redox systems (see Limitations).
- **`cl1_problem_shadow`** (diagnostic): for every `cl1()` call, the overlay also solves `cl1()`'s own constrained problem by LU and checks every bound, but `cl1()` always steers. Outputs are identical to a `cl1` run, and a log records how the two solutions compare.
- **`cl1_problem`** (**experimental**): when that check passes, the LU solution replaces the `cl1()` call. This covers systems with minerals and gases too. Use it only after validating your input class (below).

## Limitations: Read Before Using

Measured on an Intel Xeon Gold 6530 with PHREEQC v3.9.0 (details in `docs/RESULTS.md`):

1. **Little speed is available from the linear solver.** On a CO₂–basalt kinetics input, `cl1()` takes about 55 % of the run time, but on these small dense problems it is already about as fast as a dense direct solve.
   - `entry` mode touches only about 0.02 % of the calls in mineral-bearing chemistry, so it does not change the run time.
   - `cl1_problem` mode accepts 99.9 % of the calls, but is only about **10 % faster** overall.
2. **`cl1_problem` can change redox results.** In weakly poised systems, the converged pe is not determined by PHREEQC's convergence test, and changing the solver changes the Newton path. In two standard PHREEQC examples, pe moved by 12–13 units (ex3 and ex6), and ex3 needed 60 % more iterations. **Do not use `cl1_problem` for redox-sensitive chemistry (Fe, S, N, C(−4)) without the validation below.**
3. **A small difference also appears in `entry` mode,** in an unpoised NaCl solution in a PhreeqcRM test: pe −0.601 against −0.602. pH, totals and minerals are identical.
4. **PETSc is not included.** For cell-local chemistry (systems of about 10–40 unknowns), PETSc's per-call overhead makes it 2.5–7 times slower than LAPACK.
5. **For speed, use parallelism instead.** PhreeqcRM in MPI mode reached about 9.5 times speed-up at 32 ranks on the same machine.

## Validating Your Input Before Relying on a New Mode

```bash
PHREEQC_LINEAR_SOLVER=cl1                                            phreeqc in.pqi ref.out  db.dat
PHREEQC_LINEAR_SOLVER=lapack PHREEQC_LINEAR_SOLVER_INTERCEPT=cl1_problem \
  PHREEQC_LINEAR_SOLVER_CONDITION_METRIC=row_column PHREEQC_LINEAR_SOLVER_STATISTICS=1 \
                                                                    phreeqc in.pqi new.out  db.dat
python3 phreeqc_newton_solver/tools/compare_phreeqcrm_chem_output.py ref.out new.out \
        --relative-tolerance 1e-6 --absolute-tolerance 1e-12
python3 phreeqc_newton_solver/tools/compare_selected_output_to_baseline.py ref.sel new.sel
```

Accept the mode for that input class only if **both** of these hold:
- no difference beyond the last digits of trace species, and **no pe change**;
- the `calls=` value in the statistics does not grow noticeably.

## Supported Versions

`SUPPORTED_UPSTREAM.txt` lists the accepted SHA-256 values of `Phreeqc.h`, `model.cpp` and `Phreeqc.cpp`. These three files are identical in phreeqc3 v3.9.0 and PhreeqcRM v3.9.0.

For a new upstream release, a maintainer:
1. checks that all five anchors are still unique;
2. reruns the validation suite (`docs/VALIDATION.md`);
3. only then adds the new hashes.

**Never bypass the hash check.**

## Contents

```text
cmake/        attach.cmake (entry point), edits.cmake (the five anchored edits), detach.cmake
src/glue/     Phreeqc member functions (dispatcher, eligibility, hooks) and the two headers the edits include
src/core/     backend-neutral layer: options, solver state, statistics, comparison logs
src/lapack/   LAPACK backend, condition gate, cl1-problem check (shadow and steering)
tests/        unit tests, registered as newton_solver.* with CTest
tools/        comparison and analysis scripts
docs/         results, validation record, design notes
```

## Developer Note

The edit anchors contain `;` (for example `int ineq(int kode);`). **Never pass them through CMake lists**, which split on `;`. `edits.cmake` stores each edit in its own variable for exactly this reason.
