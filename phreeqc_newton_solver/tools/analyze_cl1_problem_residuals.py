#!/usr/bin/env python3
"""Test whether LU-versus-cl1 step differences come from cl1 not solving its problem exactly
(Phase 1B Task 1c, decision B-D9).

Input: shadow logs with 22 columns, written with PHREEQC_LINEAR_SOLVER_CONDITION_CHECK=log.
Columns 1-16 as in Task 1b; then
  17 cl1_rel_residual_a   max over A rows of |b - A x_cl1| / (|b| + sum|A||x_cl1|)
  18 cl1_rel_residual_c   same for the C rows (equalities)
  19 cl1_rel_violation_e  max over E rows of max(E x_cl1 - f, 0) / (|f| + sum|E||x_cl1|)
  20 lu_rel_residual_ac   max over A and C rows of the same quantity for the LU candidate
  21 min_abs_entry_m      smallest nonzero |entry| of the square system (A and C rows, live columns)
  22 entries_below_1e-15  number of nonzero entries of the square system with |entry| <= 1e-15
                          (PHREEQC's default cl1 pivot tolerance ineq_tol = 10^-DBL_DIG)

Records used: outcome accept (0) and cl1 kode 0. They are grouped by relative difference
(agreeing <= 1e-10 < intermediate <= 1e-6 < disagreeing). For each group the script reports the
medians and maxima of columns 17-22, and the fraction of records in which cl1's own relative
residual on the A and C rows exceeds --inexact (default 1e-10), i.e. where cl1 did not solve its
problem to round-off while LU did.

Exit status: 0 = report printed, 2 = read error or no usable record.
"""
import argparse
import math
import statistics
import sys

GROUPS = ("agreeing", "intermediate", "disagreeing")
EXTRA = ("cl1_rel_residual_a", "cl1_rel_residual_c", "cl1_rel_violation_e", "lu_rel_residual_ac",
         "min_abs_entry_m", "entries_below_1e-15")


def number(text):
    try:
        return float(text)
    except ValueError:
        return math.nan


def main():
    argument_parser = argparse.ArgumentParser(description=__doc__,
                                              formatter_class=argparse.RawDescriptionHelpFormatter)
    argument_parser.add_argument("logs", nargs="+")
    argument_parser.add_argument("--inexact", type=float, default=1e-10)
    arguments = argument_parser.parse_args()

    values = {g: {name: [] for name in EXTRA} for g in GROUPS}
    counts = {g: 0 for g in GROUPS}
    inexact = {g: 0 for g in GROUPS}
    try:
        for log in arguments.logs:
            with open(log, encoding="utf-8") as handle:
                for line in handle:
                    if not line.strip() or line.lstrip().startswith("#"):
                        continue
                    cells = line.split()
                    if len(cells) != 22 or int(cells[6]) != 0 or int(cells[7]) != 0:
                        continue
                    difference = number(cells[10])
                    if math.isnan(difference):
                        continue
                    group = ("agreeing" if difference <= 1e-10 else
                             "intermediate" if difference <= 1e-6 else "disagreeing")
                    counts[group] += 1
                    extra = [number(c) for c in cells[16:22]]
                    for name, value in zip(EXTRA, extra):
                        if not math.isnan(value):
                            values[group][name].append(value)
                    if max(extra[0], extra[1]) > arguments.inexact:
                        inexact[group] += 1
    except (OSError, ValueError) as read_error:
        print(f"ERROR: {read_error}", file=sys.stderr)
        return 2
    if not any(counts.values()):
        print("no usable record (need 22-column logs, outcome accept, cl1 kode 0)")
        return 2

    for group in GROUPS:
        if counts[group] == 0:
            continue
        print(f"{group}: records={counts[group]}  cl1 inexact (max(col17, col18) > {arguments.inexact:g}): "
              f"{inexact[group]} ({100.0 * inexact[group] / counts[group]:.2f} %)")
        for name in EXTRA:
            data = values[group][name]
            if data:
                print(f"  {name:<22} median={statistics.median(data):.3e}  max={max(data):.3e}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
