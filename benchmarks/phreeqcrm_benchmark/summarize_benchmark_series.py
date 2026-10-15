#!/usr/bin/env python3
"""Summarise benchmark_runs.csv written by run_phreeqcrm_benchmark_series.sh (Phase 2 Task 4).

For every (label, count): number of runs, number of failed runs, and the median, minimum and maximum
of run_cells_seconds_total. Then, for every count, the ratio of each label's median to the median of
the reference label (--reference), and the speed-up of each label relative to its own smallest count.

Exit status: 0 = summary printed, 2 = read error.
"""
import argparse
import csv
import statistics
import sys
from collections import defaultdict


def main():
    argument_parser = argparse.ArgumentParser(description=__doc__,
                                              formatter_class=argparse.RawDescriptionHelpFormatter)
    argument_parser.add_argument("csv_files", nargs="+")
    argument_parser.add_argument("--reference", required=True, help="label used as the denominator of the ratios")
    arguments = argument_parser.parse_args()

    times = defaultdict(list)
    failures = defaultdict(int)
    load_averages = defaultdict(list)
    try:
        for csv_file in arguments.csv_files:
            with open(csv_file, newline="", encoding="utf-8") as handle:
                for row in csv.DictReader(handle):
                    key = (row["label"], int(row["count"]))
                    if row["exit_status"] != "0" or not row["run_cells_seconds_total"]:
                        failures[key] += 1
                        continue
                    times[key].append(float(row["run_cells_seconds_total"]))
                    load_averages[key].append(float(row["load_average_1min"]))
    except (OSError, KeyError, ValueError) as read_error:
        print(f"ERROR: {read_error}", file=sys.stderr)
        return 2

    keys = sorted(set(times) | set(failures))
    labels = sorted({label for label, _ in keys})
    counts = sorted({count for _, count in keys})
    median = {key: statistics.median(values) for key, values in times.items() if values}

    print(f"{'label':<28}{'count':>6}{'runs':>6}{'failed':>8}{'median_s':>12}{'min_s':>10}{'max_s':>10}{'max_load':>10}")
    for key in keys:
        values = times.get(key, [])
        print(f"{key[0]:<28}{key[1]:>6}{len(values):>6}{failures.get(key, 0):>8}"
              f"{(median.get(key, float('nan'))):>12.3f}{(min(values) if values else float('nan')):>10.3f}"
              f"{(max(values) if values else float('nan')):>10.3f}"
              f"{(max(load_averages[key]) if load_averages[key] else float('nan')):>10.2f}")

    print(f"\nratio of median to reference '{arguments.reference}' at the same count:")
    print(f"{'label':<28}" + "".join(f"{('n=' + str(c)):>10}" for c in counts))
    for label in labels:
        cells = []
        for count in counts:
            reference = median.get((arguments.reference, count))
            value = median.get((label, count))
            cells.append(f"{value / reference:>10.3f}" if reference and value else f"{'-':>10}")
        print(f"{label:<28}" + "".join(cells))

    print("\nspeed-up relative to the label's own smallest count:")
    print(f"{'label':<28}" + "".join(f"{('n=' + str(c)):>10}" for c in counts))
    for label in labels:
        own_counts = [c for c in counts if (label, c) in median]
        if not own_counts:
            continue
        base = median[(label, own_counts[0])]
        cells = [f"{base / median[(label, c)]:>10.2f}" if (label, c) in median else f"{'-':>10}" for c in counts]
        print(f"{label:<28}" + "".join(cells))
    return 0


if __name__ == "__main__":
    sys.exit(main())
