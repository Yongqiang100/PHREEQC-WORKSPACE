#!/usr/bin/env python3
"""Compare a PHREEQC SELECTED_OUTPUT file numerically against a baseline file.

A numeric value passes if:  |new_value - baseline_value| <= relative_tolerance * |baseline_value|
                                                            + absolute_tolerance
  absolute_tolerance = --absolute-tolerance-log    for logarithmic columns
                                                   (pH, pe, si_*, la_*, lm_*, lg_*)
  absolute_tolerance = --absolute-tolerance-amount for all other numeric columns
                                                   (amounts, molalities, totals, ...)
Text cells (for example the 'state' column) must match exactly.
Column headers and row counts must match exactly.

Exit status: 0 = PASS, 1 = FAIL, 2 = usage or parse error.
Do not loosen the tolerances to make a comparison pass; that requires human sign-off.
"""
import argparse
import json
import math
import sys

LOGARITHMIC_COLUMN_NAMES = {"ph", "pe"}
LOGARITHMIC_COLUMN_PREFIXES = ("si_", "la_", "lm_", "lg_")


def is_logarithmic_column(column_name):
    normalized_name = column_name.strip().lower()
    return (normalized_name in LOGARITHMIC_COLUMN_NAMES
            or normalized_name.startswith(LOGARITHMIC_COLUMN_PREFIXES))


def read_selected_output(file_path):
    """Return (column_names, rows) from a tab-separated SELECTED_OUTPUT file."""
    with open(file_path, "r", encoding="utf-8", errors="replace") as selected_output_file:
        non_empty_lines = [line.rstrip("\r\n") for line in selected_output_file if line.strip()]
    if not non_empty_lines:
        raise ValueError(f"{file_path}: file is empty")
    column_names = [cell.strip() for cell in non_empty_lines[0].split("\t") if cell.strip()]
    rows = []
    for line_number, line in enumerate(non_empty_lines[1:], start=2):
        cells = [cell.strip() for cell in line.split("\t") if cell.strip()]
        if len(cells) != len(column_names):
            raise ValueError(f"{file_path}:{line_number}: {len(cells)} cells, "
                             f"header has {len(column_names)} columns")
        rows.append(cells)
    return column_names, rows


def parse_number_or_none(cell_text):
    try:
        return float(cell_text)
    except ValueError:
        return None


def main():
    argument_parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    argument_parser.add_argument("baseline_file")
    argument_parser.add_argument("new_file")
    argument_parser.add_argument("--relative-tolerance", type=float, default=1e-6)
    argument_parser.add_argument("--absolute-tolerance-amount", type=float, default=1e-14)
    argument_parser.add_argument("--absolute-tolerance-log", type=float, default=1e-6)
    argument_parser.add_argument("--json-report", help="write a machine-readable report to this path")
    arguments = argument_parser.parse_args()

    try:
        baseline_columns, baseline_rows = read_selected_output(arguments.baseline_file)
        new_columns, new_rows = read_selected_output(arguments.new_file)
    except (OSError, ValueError) as read_error:
        print(f"ERROR: {read_error}", file=sys.stderr)
        return 2

    structural_problems = []
    if baseline_columns != new_columns:
        structural_problems.append(
            f"column headers differ:\n  baseline={baseline_columns}\n  new={new_columns}")
    if len(baseline_rows) != len(new_rows):
        structural_problems.append(
            f"row counts differ: baseline={len(baseline_rows)} new={len(new_rows)}")
    if structural_problems:
        print("FAIL")
        for problem in structural_problems:
            print("  " + problem)
        return 1

    column_statistics = {
        column_name: {
            "column_kind": "log" if is_logarithmic_column(column_name) else "amount",
            "largest_absolute_difference": 0.0,
            "largest_relative_difference": 0.0,
            "failing_value_count": 0,
            "row_of_largest_difference": None,
            "has_numeric_values": False,
        }
        for column_name in baseline_columns
    }
    text_mismatches = []

    for row_number, (baseline_row, new_row) in enumerate(zip(baseline_rows, new_rows), start=1):
        for column_name, baseline_cell, new_cell in zip(baseline_columns, baseline_row, new_row):
            baseline_value = parse_number_or_none(baseline_cell)
            new_value = parse_number_or_none(new_cell)
            if baseline_value is None or new_value is None:
                if baseline_cell != new_cell:
                    text_mismatches.append((row_number, column_name, baseline_cell, new_cell))
                continue
            statistics = column_statistics[column_name]
            statistics["has_numeric_values"] = True
            if math.isnan(baseline_value) or math.isnan(new_value):
                if not (math.isnan(baseline_value) and math.isnan(new_value)):
                    statistics["failing_value_count"] += 1
                continue
            absolute_difference = abs(new_value - baseline_value)
            if baseline_value != 0.0:
                relative_difference = absolute_difference / abs(baseline_value)
            else:
                relative_difference = 0.0 if absolute_difference == 0.0 else math.inf
            absolute_tolerance = (arguments.absolute_tolerance_log
                                  if statistics["column_kind"] == "log"
                                  else arguments.absolute_tolerance_amount)
            if absolute_difference > statistics["largest_absolute_difference"]:
                statistics["largest_absolute_difference"] = absolute_difference
                statistics["row_of_largest_difference"] = row_number
            statistics["largest_relative_difference"] = max(
                statistics["largest_relative_difference"], relative_difference)
            allowed_difference = (arguments.relative_tolerance * abs(baseline_value)
                                  + absolute_tolerance)
            if absolute_difference > allowed_difference:
                statistics["failing_value_count"] += 1

    total_failure_count = (sum(s["failing_value_count"] for s in column_statistics.values())
                           + len(text_mismatches))
    verdict = "PASS" if total_failure_count == 0 else "FAIL"

    print(f"{verdict}  rows={len(baseline_rows)} columns={len(baseline_columns)} "
          f"relative_tolerance={arguments.relative_tolerance:g} "
          f"absolute_tolerance_amount={arguments.absolute_tolerance_amount:g} "
          f"absolute_tolerance_log={arguments.absolute_tolerance_log:g}")
    print(f"{'column':<28}{'kind':<8}{'largest_abs_diff':>18}{'largest_rel_diff':>18}"
          f"{'failures':>10}{'row':>6}")
    for column_name in baseline_columns:
        statistics = column_statistics[column_name]
        if not statistics["has_numeric_values"]:
            statistics["column_kind"] = "text"
        row_text = str(statistics["row_of_largest_difference"] or "-")
        print(f"{column_name:<28}{statistics['column_kind']:<8}"
              f"{statistics['largest_absolute_difference']:>18.3e}"
              f"{statistics['largest_relative_difference']:>18.3e}"
              f"{statistics['failing_value_count']:>10}{row_text:>6}")
    for row_number, column_name, baseline_cell, new_cell in text_mismatches[:20]:
        print(f"  text mismatch at row {row_number}, column {column_name}: "
              f"baseline={baseline_cell!r} new={new_cell!r}")

    if arguments.json_report:
        with open(arguments.json_report, "w", encoding="utf-8") as report_file:
            json.dump({
                "verdict": verdict,
                "row_count": len(baseline_rows),
                "relative_tolerance": arguments.relative_tolerance,
                "absolute_tolerance_amount": arguments.absolute_tolerance_amount,
                "absolute_tolerance_log": arguments.absolute_tolerance_log,
                "column_statistics": column_statistics,
                "text_mismatches": text_mismatches,
            }, report_file, indent=1, default=str)
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
