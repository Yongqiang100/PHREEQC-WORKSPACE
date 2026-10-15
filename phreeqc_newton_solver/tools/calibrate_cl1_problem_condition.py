#!/usr/bin/env python3
"""Calibrate the conditioning gate for the cl1-problem path from shadow logs (Phase 1B Task 1b).

Input: shadow logs with 16 columns, written with PHREEQC_LINEAR_SOLVER_CONDITION_CHECK=log so that
no call was gated. Column 9 is the reciprocal condition after row equilibration ("row" metric),
column 16 after row AND column equilibration ("row_column" metric).

Records used: outcome accept (0) in log mode, i.e. solved and feasible, with both condition values
present. Classes:
  agreeing      cl1 kode 0 and relative difference <= --agreement (default 1e-10)
  disagreeing   cl1 kode 0 and relative difference >  --disagreement (default 1e-6)
  intermediate  cl1 kode 0, in between
  cl1_failed    cl1 kode != 0 (cl1's step is not a reference)

For each metric and candidate threshold, prints how many records of each class a gate
"metric < threshold -> cl1" would send to cl1, and the resulting acceptance of agreeing records.
Choosing the metric and threshold is a reviewer decision.

Exit status: 0 = table printed, 2 = read error or no usable record.
"""
import argparse
import math
import sys

THRESHOLDS = (1e-4, 1e-6, 1e-8, 1e-10, 1e-12, 1e-14, 1e-16)
METRICS = (("row", 8), ("row_column", 15))
CLASSES = ("agreeing", "disagreeing", "intermediate", "cl1_failed")


def number(text):
    try:
        return float(text)
    except ValueError:
        return math.nan


def main():
    argument_parser = argparse.ArgumentParser(description=__doc__,
                                              formatter_class=argparse.RawDescriptionHelpFormatter)
    argument_parser.add_argument("logs", nargs="+")
    argument_parser.add_argument("--agreement", type=float, default=1e-10)
    argument_parser.add_argument("--disagreement", type=float, default=1e-6)
    arguments = argument_parser.parse_args()

    records = []
    try:
        for log in arguments.logs:
            with open(log, encoding="utf-8") as handle:
                for line in handle:
                    if not line.strip() or line.lstrip().startswith("#"):
                        continue
                    cells = line.split()
                    if len(cells) != 16 or int(cells[6]) != 0:
                        continue
                    row_value, row_column_value = number(cells[8]), number(cells[15])
                    if math.isnan(row_value) or math.isnan(row_column_value):
                        continue
                    kode, difference = int(cells[7]), number(cells[10])
                    if kode != 0:
                        group = "cl1_failed"
                    elif difference <= arguments.agreement:
                        group = "agreeing"
                    elif difference > arguments.disagreement:
                        group = "disagreeing"
                    else:
                        group = "intermediate"
                    records.append((group, row_value, row_column_value))
    except OSError as read_error:
        print(f"ERROR: {read_error}", file=sys.stderr)
        return 2
    if not records:
        print("no usable record (need 16-column logs written in log mode)")
        return 2

    totals = {c: sum(1 for r in records if r[0] == c) for c in CLASSES}
    print(f"usable records: {len(records)}  " + "  ".join(f"{c}={totals[c]}" for c in CLASSES))
    for metric, column in METRICS:
        index = 1 if column == 8 else 2
        print(f"\nmetric {metric}:")
        for c in CLASSES:
            values = sorted(r[index] for r in records if r[0] == c)
            if values:
                print(f"  {c:<13} min={values[0]:.3e}  median={values[len(values) // 2]:.3e}  max={values[-1]:.3e}")
        print(f"  {'threshold':<11}" + "".join(f"{'gated_' + c:>22}" for c in CLASSES) + f"{'agreeing_kept':>16}")
        for threshold in THRESHOLDS:
            gated = {c: sum(1 for r in records if r[0] == c and r[index] < threshold) for c in CLASSES}
            kept = totals["agreeing"] - gated["agreeing"]
            print(f"  {threshold:<11.0e}" + "".join(f"{gated[c]:>12}/{totals[c]:<9}" for c in CLASSES) +
                  f"{kept:>10}/{totals['agreeing']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
