#!/usr/bin/env python3
"""Sum the Newton linear-solver statistics lines found in one or more stderr captures.

Reads the process-summary lines
  phreeqc_linear_solver_process_summary: rank=<r> segment=<s> objects=<n> ... key=value ...
written by the adapter when PHREEQC_LINEAR_SOLVER_STATISTICS is "summary" or "all" (Phase 2 Task 3),
and, for cross-checking, the per-object lines
  phreeqc_linear_solver_statistics: backend=<b> solves=<n> ...
  phreeqc_lapack_statistics: solves=<n> ... ill_conditioned=<n>
written when it is "1" or "all".

Prints, for each line kind, the number of lines and the sum of every numeric key=value field,
plus the summary-line count per rank. Label fields (rank, segment, backend) are counted, never summed.

Exit status: 0 = at least one statistics line found, 1 = none found, 2 = read error.
"""
import argparse
import re
import sys
from collections import Counter, OrderedDict

LINE_KINDS = ("phreeqc_linear_solver_process_summary:", "phreeqc_linear_solver_statistics:",
              "phreeqc_lapack_statistics:", "phreeqc_linear_solver_ineligible:")
FIELD = re.compile(r"(\w+)=(\S+)")
LABEL_KEYS = {"rank", "segment", "backend"}   # identify a line; counted, never summed


def main():
    argument_parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    argument_parser.add_argument("stderr_files", nargs="+")
    arguments = argument_parser.parse_args()

    sums = {kind: OrderedDict() for kind in LINE_KINDS}
    line_counts = Counter()
    text_values = {kind: Counter() for kind in LINE_KINDS}
    summary_lines_per_rank = Counter()
    try:
        for file_path in arguments.stderr_files:
            with open(file_path, encoding="utf-8", errors="replace") as stderr_file:
                for line in stderr_file:
                    for kind in LINE_KINDS:
                        position = line.find(kind)
                        if position < 0:
                            continue
                        line_counts[kind] += 1
                        for key, value in FIELD.findall(line[position + len(kind):]):
                            if value.isdigit() and key not in LABEL_KEYS:
                                sums[kind][key] = sums[kind].get(key, 0) + int(value)
                            else:
                                text_values[kind][f"{key}={value}"] += 1
                                if kind == LINE_KINDS[0] and key == "rank":
                                    summary_lines_per_rank[value] += 1
                        break
    except OSError as read_error:
        print(f"ERROR: {read_error}", file=sys.stderr)
        return 2

    if not line_counts:
        print("no statistics lines found")
        return 1
    for kind in LINE_KINDS:
        if line_counts[kind] == 0:
            continue
        print(f"{kind} lines={line_counts[kind]}")
        if text_values[kind]:
            print("  text fields: " + "  ".join(f"{k} x{v}" for k, v in sorted(text_values[kind].items())))
        print("  sums: " + "  ".join(f"{k}={v}" for k, v in sums[kind].items()))
    if summary_lines_per_rank:
        print("summary lines per rank: " + "  ".join(f"rank {r}: {n}" for r, n in sorted(summary_lines_per_rank.items())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
