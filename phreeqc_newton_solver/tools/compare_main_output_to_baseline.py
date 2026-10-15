#!/usr/bin/env python3
"""Compare a PHREEQC main output (.out) file with a baseline, ignoring run-time lines only.

Task 0 (determinism check, report section C2) showed that two runs of the same unmodified
binary differ only in the wall-clock line and in the dashed separators around it, whose
width follows the length of the timing value:

    ----------------------------------
    End of Run after 0.020343 Seconds.
    ----------------------------------

This script removes exactly those lines from both files before comparing:
  - every line matching  ^\\s*End of Run after .* Seconds\\.\\s*$
  - a line consisting only of '-' characters that is immediately before or after such a line.
Every other line must match exactly, in the same order.

Exit status: 0 = IDENTICAL (apart from ignored lines), 1 = DIFFERENT, 2 = usage or read error.
"""
import argparse
import re
import sys

RUN_TIME_LINE = re.compile(r"^\s*End of Run after .* Seconds\.\s*$")
DASHES_ONLY_LINE = re.compile(r"^\s*-+\s*$")


def read_lines_without_run_time(file_path):
    """Return a list of (original_line_number, line_text) with run-time lines removed."""
    with open(file_path, "r", encoding="utf-8", errors="replace") as output_file:
        lines = [line.rstrip("\r\n") for line in output_file]
    ignored_line_indices = set()
    for index, line in enumerate(lines):
        if RUN_TIME_LINE.match(line):
            ignored_line_indices.add(index)
            for neighbour in (index - 1, index + 1):
                if 0 <= neighbour < len(lines) and DASHES_ONLY_LINE.match(lines[neighbour]):
                    ignored_line_indices.add(neighbour)
    kept_lines = [(index + 1, line) for index, line in enumerate(lines)
                  if index not in ignored_line_indices]
    return kept_lines, len(ignored_line_indices)


def main():
    argument_parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    argument_parser.add_argument("baseline_file")
    argument_parser.add_argument("new_file")
    argument_parser.add_argument("--max-reported-differences", type=int, default=20)
    arguments = argument_parser.parse_args()

    try:
        baseline_lines, baseline_ignored_count = read_lines_without_run_time(arguments.baseline_file)
        new_lines, new_ignored_count = read_lines_without_run_time(arguments.new_file)
    except OSError as read_error:
        print(f"ERROR: {read_error}", file=sys.stderr)
        return 2

    differences = []
    for (baseline_line_number, baseline_text), (new_line_number, new_text) in zip(baseline_lines,
                                                                                   new_lines):
        if baseline_text != new_text:
            differences.append((baseline_line_number, baseline_text, new_line_number, new_text))
    line_count_differs = len(baseline_lines) != len(new_lines)

    verdict = "IDENTICAL" if not differences and not line_count_differs else "DIFFERENT"
    print(f"{verdict}  compared_lines={min(len(baseline_lines), len(new_lines))} "
          f"ignored_run_time_lines baseline={baseline_ignored_count} new={new_ignored_count}")
    if line_count_differs:
        print(f"  line counts differ after removing run-time lines: "
              f"baseline={len(baseline_lines)} new={len(new_lines)}")
    for baseline_line_number, baseline_text, new_line_number, new_text in \
            differences[:arguments.max_reported_differences]:
        print(f"  baseline line {baseline_line_number}: {baseline_text!r}")
        print(f"  new      line {new_line_number}: {new_text!r}")
    if len(differences) > arguments.max_reported_differences:
        print(f"  ... {len(differences) - arguments.max_reported_differences} more differing lines")
    return 0 if verdict == "IDENTICAL" else 1


if __name__ == "__main__":
    sys.exit(main())
