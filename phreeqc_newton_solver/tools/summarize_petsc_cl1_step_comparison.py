#!/usr/bin/env python3
"""Summarise the backend-versus-cl1 Newton step comparison log (PETSc or LAPACK backend).

The log is written by PHREEQC when run with -phreeqc_petsc_compare_with_cl1. Columns:
  call_index system_size solve_status cl1_return_code backward_error
  relative_difference relative_sum largest_difference_index largest_difference_unknown_type
  [reciprocal_condition]   (optional 10th column, Phase 2 Task 2d; nan if not computed)

  solve_status:        0 = LINEAR_SOLVE_ACCEPTED, 1 = LINEAR_SOLVE_FALL_BACK_TO_CL1,
                       2 = LINEAR_SOLVE_NON_FINITE_INPUT
  relative_difference: max|petsc_step - cl1_step| / max|cl1_step|
  relative_sum:        max|petsc_step + cl1_step| / max|cl1_step|
                       (close to 0 means the PETSc step has the opposite sign)

Reported flags are review aids, not pass/fail criteria:
  - suspected sign inversion: relative_sum < 1e-6 on accepted calls
  - large differences: relative_difference > --large-difference-threshold,
    grouped by the unknown type at the largest difference
"""
import argparse
import math
import statistics
import sys
from collections import Counter, defaultdict

LOG_COLUMN_NAMES = (
    "call_index", "system_size", "solve_status", "cl1_return_code", "backward_error",
    "relative_difference", "relative_sum", "largest_difference_index",
    "largest_difference_unknown_type",
)
OPTIONAL_LOG_COLUMN_NAMES = ("reciprocal_condition",)
INTEGER_COLUMNS = {"call_index", "system_size", "solve_status", "cl1_return_code",
                   "largest_difference_index", "largest_difference_unknown_type"}
SOLVE_STATUS_NAMES = {0: "LINEAR_SOLVE_ACCEPTED", 1: "LINEAR_SOLVE_FALL_BACK_TO_CL1",
                      2: "LINEAR_SOLVE_NON_FINITE_INPUT"}
SIGN_INVERSION_THRESHOLD = 1e-6


def parse_float_or_nan(cell_text):
    try:
        return float(cell_text)
    except ValueError:
        return math.nan


def read_comparison_records(log_path):
    records = []
    with open(log_path, encoding="utf-8") as log_file:
        for line_number, line in enumerate(log_file, start=1):
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            cells = line.split()
            column_names = LOG_COLUMN_NAMES
            if len(cells) == len(LOG_COLUMN_NAMES) + len(OPTIONAL_LOG_COLUMN_NAMES):
                column_names = LOG_COLUMN_NAMES + OPTIONAL_LOG_COLUMN_NAMES
            elif len(cells) != len(LOG_COLUMN_NAMES):
                raise ValueError(f"line {line_number}: expected {len(LOG_COLUMN_NAMES)} or "
                                 f"{len(LOG_COLUMN_NAMES) + 1} columns, found {len(cells)}")
            record = {"reciprocal_condition": math.nan}
            for column_name, cell_text in zip(column_names, cells):
                record[column_name] = (int(cell_text) if column_name in INTEGER_COLUMNS
                                       else parse_float_or_nan(cell_text))
            records.append(record)
    return records


def value_at_fraction(sorted_values, fraction):
    position = min(len(sorted_values) - 1, int(fraction * (len(sorted_values) - 1)))
    return sorted_values[position]


def main():
    argument_parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    argument_parser.add_argument("comparison_logs", nargs="+",
                                 help="one or more logs (e.g. one per solver object); records are pooled")
    argument_parser.add_argument("--large-difference-threshold", type=float, default=1e-6)
    arguments = argument_parser.parse_args()

    try:
        records = []
        for comparison_log in arguments.comparison_logs:
            records.extend(read_comparison_records(comparison_log))
    except (OSError, ValueError) as read_error:
        print(f"ERROR: {read_error}", file=sys.stderr)
        return 2
    if not records:
        print("no comparison records found")
        return 2

    status_counts = Counter(record["solve_status"] for record in records)
    print(f"comparison records: {len(records)}")
    for solve_status, record_count in sorted(status_counts.items()):
        print(f"  {SOLVE_STATUS_NAMES.get(solve_status, 'UNKNOWN_STATUS')}: {record_count}")
    print(f"  cl1 return codes: {dict(Counter(r['cl1_return_code'] for r in records))}")
    print(f"  system sizes: {dict(sorted(Counter(r['system_size'] for r in records).items()))}")

    accepted_records = [r for r in records
                        if r["solve_status"] == 0 and not math.isnan(r["relative_difference"])]
    if not accepted_records:
        print("\nno accepted PETSc steps with a comparison value")
        return 0

    sorted_relative_differences = sorted(r["relative_difference"] for r in accepted_records)
    sorted_backward_errors = sorted(r["backward_error"] for r in accepted_records
                                    if not math.isnan(r["backward_error"]))
    print("\naccepted backend steps: normwise relative difference from the cl1 step")
    print(f"  median={statistics.median(sorted_relative_differences):.3e}  "
          f"90th percentile={value_at_fraction(sorted_relative_differences, 0.90):.3e}  "
          f"99th percentile={value_at_fraction(sorted_relative_differences, 0.99):.3e}  "
          f"maximum={sorted_relative_differences[-1]:.3e}")
    if sorted_backward_errors:
        print(f"  backward error: median={statistics.median(sorted_backward_errors):.3e}  "
              f"maximum={sorted_backward_errors[-1]:.3e}")

    reciprocal_conditions = sorted(r["reciprocal_condition"] for r in accepted_records
                                   if not math.isnan(r["reciprocal_condition"]))
    if reciprocal_conditions:
        print(f"  reciprocal condition (1-norm): minimum={reciprocal_conditions[0]:.3e}  "
              f"median={statistics.median(reciprocal_conditions):.3e}  "
              f"maximum={reciprocal_conditions[-1]:.3e}  (records with a value: {len(reciprocal_conditions)})")

    sign_inverted_records = [r for r in accepted_records
                             if r["relative_sum"] < SIGN_INVERSION_THRESHOLD]
    if sign_inverted_records:
        print(f"\n!! SUSPECTED SIGN INVERSION in {len(sign_inverted_records)} of "
              f"{len(accepted_records)} accepted steps (PETSc step is close to -1 times the "
              f"cl1 step). Check the residual sign convention (Task 0 item B6).")

    large_difference_records = [r for r in accepted_records
                                if r["relative_difference"] > arguments.large_difference_threshold]
    if large_difference_records:
        differences_by_unknown_type = defaultdict(list)
        for record in large_difference_records:
            differences_by_unknown_type[record["largest_difference_unknown_type"]].append(
                record["relative_difference"])
        print(f"\nsteps with relative_difference > {arguments.large_difference_threshold:g}: "
              f"{len(large_difference_records)} (grouped by unknown type at the largest difference)")
        for unknown_type, differences in sorted(differences_by_unknown_type.items(),
                                                key=lambda item: -len(item[1])):
            print(f"  unknown type {unknown_type}: count={len(differences)} "
                  f"maximum={max(differences):.3e}")
        print("  largest differences (call_index, system_size, relative_difference, "
              "largest_difference_index, unknown_type, backward_error, reciprocal_condition):")
        for record in sorted(large_difference_records,
                             key=lambda r: -r["relative_difference"])[:10]:
            print(f"    {record['call_index']}, {record['system_size']}, "
                  f"{record['relative_difference']:.3e}, {record['largest_difference_index']}, "
                  f"{record['largest_difference_unknown_type']}, {record['backward_error']:.3e}, "
                  f"{record['reciprocal_condition']:.3e}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
