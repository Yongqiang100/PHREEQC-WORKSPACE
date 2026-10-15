# Measured Results (PHREEQC v3.9.0, Intel Xeon Gold 6530, GCC 15, OpenBLAS)

| Measurement | Value |
|---|---|
| Share of run time in the Newton linear solve (CO₂–basalt kinetics input, profiled) | 57.5 % (`cl1()` alone: 55.4 %) |
| Eligible calls in `entry` mode (aqueous-only systems): CO₂–basalt input / PhreeqcRM benchmark | 0.0017 % / 0.01–0.02 % |
| LAPACK direct against the PETSc dense LU path, per solve, n = 9–40 | 2.5–7× faster |
| `entry` mode against `cl1()`, step difference (aqueous systems) | about 5 × 10⁻¹⁶ (round-off) |
| `cl1_problem` acceptance, CO₂–basalt input | 99.9 % |
| `cl1_problem` run time, CO₂–basalt input (median of 3 alternating runs) | 30.17 s against 33.56 s for `cl1`, **10.1 % faster** |
| Cost per accepted call (`cl1_problem`) against `cl1()` | about 8.0 µs against about 10.5 µs |
| `cl1_problem` redox changes (standard examples) | ex3: pe −1.59 → 10.53; ex6: pe 9.54 → −3.79; ex3 72 iterations instead of 45 |
| PhreeqcRM benchmark, 1,000 cells × 10 steps, integration overhead | none measurable (≤ 1.9 % between configurations, inside the run-to-run spread) |
| PhreeqcRM MPI speed-up at 32 ranks (unmodified or with the overlay) | about 9.5× |

**Conclusion.** For PHREEQC's cell-by-cell chemistry, the linear systems are small and dense, and `cl1()` is already close to the best a dense direct solver can do. The overlay is a safe, testable platform for solver research and diagnostics, not a production speed-up.
