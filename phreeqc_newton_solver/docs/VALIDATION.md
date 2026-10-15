# Validation Record (overlay 1.0.0)

Validated on fresh clones of the official repositories: phreeqc3 tag v3.9.0 (`b0b3be76`) and PhreeqcRM tag v3.9.0 (`90b31ed8`).

| Check | Result |
|---|---|
| The three engine files' SHA-256 values in both clones, against `SUPPORTED_UPSTREAM.txt` | all match |
| Folder copied in, no flag: phreeqc3 ex1/ex2 and PhreeqcRM `TestRM` | byte-identical to unmodified upstream |
| With the overlay: 4 unit tests, phreeqc3 and PhreeqcRM, normal and ASan builds | all pass, no ASan report |
| phreeqc3 `lapack` statistics; shadow ex4; steering on ex1–ex8, `units`, CO₂–basalt | identical to the reference integration |
| PhreeqcRM OpenMP, MPI at 2 and 4 ranks: statistics sums | exactly as the reference (490/368/122, 654/460/194, 1194/856/338) |
| PhreeqcRM outputs against v3.9.0 | within 10⁻⁶ relative / 10⁻¹² absolute, except pe −0.601 against −0.602 in one unpoised solution |
| Installed PhreeqcRM consumed by an external program (`find_package(PhreeqcRM)`) | builds; results identical |
| Re-configure | no-op |
| Detach | `git diff` empty on both clones |
| Modified (unsupported) upstream file | refused at configure time, nothing written |

**To repeat the checks** for a new upstream release, use the scripts in `tools/` (`compare_main_output_to_baseline.py`, `compare_phreeqcrm_chem_output.py`, `compare_selected_output_to_baseline.py`, `sum_linear_solver_statistics.py`). Compare against the same release built without the overlay, and against a `PHREEQC_LINEAR_SOLVER=cl1` run.
