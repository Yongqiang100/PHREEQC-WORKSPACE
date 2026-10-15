# PHREEQC-WORKSPACE

Work on the Newton linear solver of **PHREEQC** and **PhreeqcRM** (USGS), 2026:
- a drop-in overlay that adds an alternative linear solver to an official source tree;
- the measurements and decisions behind it.

## What Is Here

| Path | Contents |
|---|---|
| `phreeqc_newton_solver/` | **The overlay (release 1.0.0).** Copy it into an official phreeqc3 v3.9.0 or PhreeqcRM v3.9.0 tree, and configure with one extra CMake flag. See its `README.md` |
| `benchmarks/phreeqcrm_benchmark/` | Chemistry-only PhreeqcRM benchmark driver (CO₂–basalt, 1,000 cells) and series scripts |
| `docs/` | Project summary, designs, reviewer decisions, task reports, measurements |

The release tarball (`phreeqc_newton_solver-1.0.0.tar.gz`, with its SHA-256) is attached to the GitHub release **v1.0.0**.

## Key Results (Details in `docs/PROJECT_SUMMARY.md`)

- **The overlay's default behaviour is identical to upstream PHREEQC.** The new solvers are opt-in, through environment variables.
- **The linear solve is about 57 % of the run time** in a CO₂–basalt kinetics test, but `cl1()` is already close to the best a dense direct solver can do on these small problems.
- **The experimental `cl1_problem` mode is about 10 % faster,** but can change redox results in weakly poised systems. Validate before use.
- **PETSc is not suitable for cell-local chemistry:** it was 2.5–7 times slower than LAPACK at 10–40 unknowns.
- **PhreeqcRM in MPI mode** gave about 9.5 times speed-up at 32 ranks. That is the practical speed lever.

## Development History

The full development history is on two branches:
- `dev/phreeqc3`: the phreeqc3 v3.9.0 base plus all tasks, tagged `petsc-phase1a-*`, `petsc-phase2-*`, `petsc-phase1b-*` and `overlay-*`;
- `dev/phreeqcrm`: the PhreeqcRM v3.9.0 base plus the port, tagged `newton-solver-*`.

## Licences

The overlay and the material in this repository: see `phreeqc_newton_solver/LICENSE`. PHREEQC and PhreeqcRM are USGS software, distributed under their own terms, which are kept in their trees on the `dev/*` branches.
