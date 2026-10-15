#!/usr/bin/env python3
"""Compare a PhreeqcRM main chemistry output file (*.chem.txt) with a baseline.

Phase 2 Task 2a showed that unmodified PhreeqcRM v3.9.0 is not byte-reproducible between runs
or between parallel configurations, for reasons unrelated to the chemistry (decision P2-D15):

  1. wall-clock lines            "End of Run after <s> Seconds." and the dashed lines around them
  2. configuration header lines  "Number of MPI processes: <n>" / "Number of threads: <n>"
  3. solution descriptions       "Using solution 7.<TAB>Solution after simulation 1." versus
                                 "Using solution 7.<TAB>  ", depending on which worker last held the cell
  4. load-balancing table rows   lines of exactly three integers ("<worker> <first cell> <last cell>"),
                                 whose values follow the run-time rebalancing

This script removes 1, 2 and 4, and reduces 3 to "Using <keyword> <n>." in both files. It then
requires every remaining line to match exactly and in order. Any change to a printed chemical
quantity is therefore still detected.

Optional numeric mode (--relative-tolerance / --absolute-tolerance, decision P2-D17): lines are
compared token by token after normalization; tokens that parse as numbers must agree within
|new - baseline| <= relative_tolerance * |baseline| + absolute_tolerance, all other tokens exactly,
and the token counts must match. Without these options every line must match exactly.

Exit status: 0 = IDENTICAL (or WITHIN_TOLERANCE in numeric mode), 1 = DIFFERENT, 2 = usage or read error.
"""
import argparse
import re
import sys

RUN_TIME_LINE = re.compile(r"^\s*End of Run after .* Seconds\.\s*$")
DASHES_ONLY_LINE = re.compile(r"^\s*-+\s*$")
CONFIGURATION_LINE = re.compile(r"^\s*Number of (MPI processes|threads):\s*\d+\s*$")
USING_WITH_DESCRIPTION = re.compile(r"^(Using [A-Za-z_ ]+ \d+\.)\t.*$")
INTEGER_TRIPLET_LINE = re.compile(r"^\s*\d+\s+\d+\s+\d+\s*$")


def normalized_lines(file_path):
    """Return (list of (original_line_number, normalized_text), counts of each normalization)."""
    with open(file_path, "r", encoding="utf-8", errors="replace") as output_file:
        lines = [line.rstrip("\r\n") for line in output_file]
    removed_line_indices = set()
    counts = {"run_time": 0, "configuration": 0, "description": 0, "load_balance_row": 0}
    for index, line in enumerate(lines):
        if RUN_TIME_LINE.match(line):
            removed_line_indices.add(index)
            counts["run_time"] += 1
            for neighbour in (index - 1, index + 1):
                if 0 <= neighbour < len(lines) and DASHES_ONLY_LINE.match(lines[neighbour]):
                    removed_line_indices.add(neighbour)
        elif CONFIGURATION_LINE.match(line):
            removed_line_indices.add(index)
            counts["configuration"] += 1
        elif INTEGER_TRIPLET_LINE.match(line):
            removed_line_indices.add(index)
            counts["load_balance_row"] += 1
    kept_lines = []
    for index, line in enumerate(lines):
        if index in removed_line_indices:
            continue
        match = USING_WITH_DESCRIPTION.match(line)
        if match:
            counts["description"] += 1
            line = match.group(1)
        kept_lines.append((index + 1, line))
    return kept_lines, counts


def parse_number_or_none(token):
    try:
        return float(token)
    except ValueError:
        return None


def lines_agree(baseline_text, new_text, relative_tolerance, absolute_tolerance):
    """Exact comparison, or token-wise numeric comparison when tolerances are given."""
    if baseline_text == new_text:
        return True
    if relative_tolerance is None:
        return False
    baseline_tokens, new_tokens = baseline_text.split(), new_text.split()
    if len(baseline_tokens) != len(new_tokens):
        return False
    for baseline_token, new_token in zip(baseline_tokens, new_tokens):
        if baseline_token == new_token:
            continue
        baseline_value = parse_number_or_none(baseline_token)
        new_value = parse_number_or_none(new_token)
        if baseline_value is None or new_value is None:
            return False
        if abs(new_value - baseline_value) > relative_tolerance * abs(baseline_value) + absolute_tolerance:
            return False
    return True


def main():
    argument_parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    argument_parser.add_argument("baseline_file")
    argument_parser.add_argument("new_file")
    argument_parser.add_argument("--max-reported-differences", type=int, default=20)
    argument_parser.add_argument("--relative-tolerance", type=float, default=None)
    argument_parser.add_argument("--absolute-tolerance", type=float, default=None)
    arguments = argument_parser.parse_args()
    if (arguments.relative_tolerance is None) != (arguments.absolute_tolerance is None):
        print("ERROR: give both --relative-tolerance and --absolute-tolerance, or neither", file=sys.stderr)
        return 2

    try:
        baseline_lines, baseline_counts = normalized_lines(arguments.baseline_file)
        new_lines, new_counts = normalized_lines(arguments.new_file)
    except OSError as read_error:
        print(f"ERROR: {read_error}", file=sys.stderr)
        return 2

    differences = []
    for (baseline_number, baseline_text), (new_number, new_text) in zip(baseline_lines, new_lines):
        if not lines_agree(baseline_text, new_text,
                           arguments.relative_tolerance, arguments.absolute_tolerance):
            differences.append((baseline_number, baseline_text, new_number, new_text))
    line_count_differs = len(baseline_lines) != len(new_lines)
    agreeing_verdict = "IDENTICAL" if arguments.relative_tolerance is None else "WITHIN_TOLERANCE"
    verdict = agreeing_verdict if not differences and not line_count_differs else "DIFFERENT"

    print(f"{verdict}  compared_lines={min(len(baseline_lines), len(new_lines))} "
          f"normalized baseline={baseline_counts} new={new_counts}")
    if line_count_differs:
        print(f"  line counts differ after normalization: "
              f"baseline={len(baseline_lines)} new={len(new_lines)}")
    for baseline_number, baseline_text, new_number, new_text in \
            differences[:arguments.max_reported_differences]:
        print(f"  baseline line {baseline_number}: {baseline_text!r}")
        print(f"  new      line {new_number}: {new_text!r}")
    if len(differences) > arguments.max_reported_differences:
        print(f"  ... {len(differences) - arguments.max_reported_differences} more differing lines")
    return 0 if verdict != "DIFFERENT" else 1


if __name__ == "__main__":
    sys.exit(main())
