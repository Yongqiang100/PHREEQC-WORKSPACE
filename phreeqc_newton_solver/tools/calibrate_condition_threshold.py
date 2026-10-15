#!/usr/bin/env python3
"""Calibrate the reciprocal-condition threshold for the LAPACK Newton solver (Phase 2 Task 2d).

Reads one or more comparison logs that have the 10th column `reciprocal_condition`
(written with PHREEQC_LINEAR_SOLVER_CONDITION_CHECK=log and comparison mode on), and, for each
candidate threshold, counts how many accepted steps a gate "reciprocal_condition < threshold
-> fall back to cl1" would remove, split by how well the LAPACK step agreed with the cl1 step.

Classes of accepted steps (solve_status 0, reciprocal_condition available):
  disagreeing   relative_difference > --disagreement (default 1e-6) and cl1_return_code == 1
  cl1_failed    cl1_return_code != 1 (cl1 itself failed; its step is not a reference)
  agreeing      relative_difference <= --agreement (default 1e-12) and cl1_return_code == 1
  intermediate  everything else

A good threshold gates (nearly) all disagreeing steps and (nearly) no agreeing steps. The script
reports counts only; choosing the threshold is a reviewer decision.

Exit status: 0 = report written, 2 = usage or read error.
"""
import argparse
import math
import sys

COLUMN_COUNT = 10
DEFAULT_THRESHOLDS = (1e-4, 1e-6, 1e-8, 1e-10, 1e-12, 1e-14, 1e-16)


def read_records(log_paths):
    records = []
    for log_path in log_paths:
        with open(log_path, encoding="utf-8") as log_file:
            for line_number, line in enumerate(log_file, start=1):
                if not line.strip() or line.lstrip().startswith("#"):
                    continue
                cells = line.split()
                if len(cells) != COLUMN_COUNT:
                    raise ValueError(f"{log_path}:{line_number}: expected {COLUMN_COUNT} columns "
                                     f"(log written without the condition check?), found {len(cells)}")
                try:
                    reciprocal_condition = float(cells[9])
                except ValueError:
                    reciprocal_condition = math.nan
                records.append({
                    "log": log_path, "system_size": int(cells[1]), "solve_status": int(cells[2]),
                    "cl1_return_code": int(cells[3]),
                    "relative_difference": float(cells[5]) if cells[5] != "nan" else math.nan,
                    "unknown_type": int(cells[8]), "reciprocal_condition": reciprocal_condition,
                })
    return records


def classify(record, disagreement, agreement):
    if record["cl1_return_code"] != 1:
        return "cl1_failed"
    if record["relative_difference"] > disagreement:
        return "disagreeing"
    if record["relative_difference"] <= agreement:
        return "agreeing"
    return "intermediate"


def main():
    argument_parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    argument_parser.add_argument("comparison_logs", nargs="+")
    argument_parser.add_argument("--disagreement", type=float, default=1e-6)
    argument_parser.add_argument("--agreement", type=float, default=1e-12)
    argument_parser.add_argument("--thresholds", type=float, nargs="*", default=list(DEFAULT_THRESHOLDS))
    arguments = argument_parser.parse_args()
    try:
        records = read_records(arguments.comparison_logs)
    except (OSError, ValueError) as read_error:
        print(f"ERROR: {read_error}", file=sys.stderr)
        return 2

    usable = [r for r in records if r["solve_status"] == 0 and not math.isnan(r["reciprocal_condition"])
              and not math.isnan(r["relative_difference"])]
    classes = ("disagreeing", "cl1_failed", "agreeing", "intermediate")
    totals = {c: 0 for c in classes}
    for record in usable:
        totals[classify(record, arguments.disagreement, arguments.agreement)] += 1
    print(f"records read: {len(records)}  usable accepted records with reciprocal_condition: {len(usable)}")
    print("class totals: " + "  ".join(f"{c}={totals[c]}" for c in classes))
    for c in classes:
        values = sorted(r["reciprocal_condition"] for r in usable
                        if classify(r, arguments.disagreement, arguments.agreement) == c)
        if values:
            print(f"  {c:<12} reciprocal_condition: min={values[0]:.3e} max={values[-1]:.3e}")

    print("\nthreshold    gated_total  gated_disagreeing  gated_cl1_failed  gated_agreeing  gated_intermediate")
    for threshold in sorted(arguments.thresholds, reverse=True):
        gated = {c: 0 for c in classes}
        for record in usable:
            if record["reciprocal_condition"] < threshold:
                gated[classify(record, arguments.disagreement, arguments.agreement)] += 1
        print(f"{threshold:<12.0e} {sum(gated.values()):>11}  "
              f"{gated['disagreeing']:>8}/{totals['disagreeing']:<8} "
              f"{gated['cl1_failed']:>7}/{totals['cl1_failed']:<8} "
              f"{gated['agreeing']:>7}/{totals['agreeing']:<6} "
              f"{gated['intermediate']:>8}/{totals['intermediate']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
