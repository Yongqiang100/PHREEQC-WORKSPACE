#!/usr/bin/env python3
"""Summarise cl1-problem shadow logs written in Phase 1B Task 1 shadow mode.

Each non-comment line has 15 columns (Phase 1B Task 1), or 16 (Task 1b: column 16 is
reciprocal_condition_equilibrated, the reciprocal condition after row AND column equilibration):
  call_index n k l m live outcome cl1_kode reciprocal_condition backward_error
  relative_difference max_excluded_cl1 largest_violation violated_row_unknown_type
  largest_difference_unknown_type
(nan where not applicable; unknown types are PHREEQC codes, e.g. MB 10, ALK 11, CB 12,
 SOLUTION_PHASE_BOUNDARY 13, MU 14, AH2O 15, MH 16, MH2O 17, PP 18, EXCH 19, SURFACE 20,
 SURFACE_CB 21, SURFACE_CB1 22, SURFACE_CB2 23, GAS_MOLES 24, SS_MOLES 25, PITZER_GAMMA 26).

Outcome codes: 0 accept, 1 excluded_retry, 2 excluded_pitzer_sit, 3 excluded_mass_water,
4 excluded_numerical_derivatives, 5 not_square, 6 non_finite, 7 ill_conditioned, 8 solve_failed,
9 backward_error, 10 inequality_violated, 11 sign_violated, 12 componentwise_backward_error (Task 2).

Reports: outcome counts; for accepted records the agreement with cl1 (relative difference on the
live columns, and the largest |cl1 value| on excluded columns); accepted records where cl1 did not
return kode 0; violations by the unknown type of the violated row; and the "falsifiers": accepted
records whose relative difference exceeds --agreement while cl1 returned kode 0.

Falsifier criterion (decision B-D8): an accepted record with cl1 kode 0 whose relative difference
exceeds max(--agreement, --condition-factor * 2.22e-16 / reciprocal_condition). With the default
--condition-factor 0 the criterion is the fixed --agreement threshold of Task 1.

Exit status: 0 = no falsifier, 1 = at least one falsifier, 2 = read error.
"""
import argparse
import math
import sys
from collections import Counter

OUTCOMES = ("accept", "excluded_retry", "excluded_pitzer_sit", "excluded_mass_water",
            "excluded_numerical_derivatives", "not_square", "non_finite", "ill_conditioned",
            "solve_failed", "backward_error", "inequality_violated", "sign_violated",
            "componentwise_backward_error")
COLUMN_COUNTS = (15, 16)
DOUBLE_EPSILON = 2.220446049250313e-16


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
    argument_parser.add_argument("--max-listed", type=int, default=20)
    argument_parser.add_argument("--condition-factor", type=float, default=0.0,
                                 help="scale the falsifier threshold by factor*eps/rcond (B-D8)")
    arguments = argument_parser.parse_args()

    outcome_counts = Counter()
    violation_types = Counter()
    sign_types = Counter()
    accepted_differences, excluded_values = [], []
    accepted_cl1_failed = Counter()
    falsifiers, records, malformed = [], 0, 0
    live_minus_rows = Counter()
    try:
        for log in arguments.logs:
            with open(log, encoding="utf-8") as handle:
                for line_number, line in enumerate(handle, start=1):
                    if not line.strip() or line.lstrip().startswith("#"):
                        continue
                    cells = line.split()
                    if len(cells) not in COLUMN_COUNTS and len(cells) != 22:
                        malformed += 1
                        continue
                    records += 1
                    outcome = int(cells[6])
                    outcome_name = OUTCOMES[outcome] if 0 <= outcome < len(OUTCOMES) else f"code_{outcome}"
                    outcome_counts[outcome_name] += 1
                    k, l, live = int(cells[2]), int(cells[3]), int(cells[5])
                    if outcome_name == "not_square":
                        live_minus_rows[live - (k + l)] += 1
                    cl1_kode = int(cells[7])
                    relative_difference = number(cells[10])
                    if outcome_name == "accept":
                        if cl1_kode != 0:
                            accepted_cl1_failed[cl1_kode] += 1
                        else:
                            accepted_differences.append(relative_difference)
                            excluded_values.append(number(cells[11]))
                            threshold = arguments.agreement
                            reciprocal_condition = number(cells[8])
                            if arguments.condition_factor > 0.0 and reciprocal_condition > 0.0:
                                threshold = max(threshold, arguments.condition_factor * DOUBLE_EPSILON
                                                / reciprocal_condition)
                            if not relative_difference <= threshold:
                                falsifiers.append((log, line_number, line.strip()))
                    elif outcome_name == "inequality_violated":
                        violation_types[cells[13]] += 1
                    elif outcome_name == "sign_violated":
                        sign_types[cells[13]] += 1
    except OSError as read_error:
        print(f"ERROR: {read_error}", file=sys.stderr)
        return 2

    print(f"records={records} malformed_lines={malformed}")
    for name in OUTCOMES:
        if outcome_counts[name]:
            print(f"  {name:<32}{outcome_counts[name]:>10}  {100.0 * outcome_counts[name] / records:7.3f} %")
    for name, count in outcome_counts.items():
        if name not in OUTCOMES:
            print(f"  {name:<32}{count:>10}")
    if live_minus_rows:
        print("not_square: (live columns - (k + l)) -> count: " +
              "  ".join(f"{d:+d}:{c}" for d, c in sorted(live_minus_rows.items())))
    if accepted_differences:
        finite = sorted(d for d in accepted_differences if not math.isnan(d))
        print(f"accepted with cl1 kode 0: {len(accepted_differences)}  relative difference: "
              f"median={finite[len(finite) // 2]:.3e}  max={finite[-1]:.3e}" if finite else "")
        excluded = [v for v in excluded_values if not math.isnan(v)]
        if excluded:
            print(f"  largest |cl1 value| on excluded columns: {max(excluded):.3e}")
    if accepted_cl1_failed:
        print("accepted records where cl1 returned kode != 0: " +
              "  ".join(f"kode {k}: {c}" for k, c in sorted(accepted_cl1_failed.items())))
    if violation_types:
        print("inequality_violated by violated-row unknown type: " +
              "  ".join(f"{t}:{c}" for t, c in violation_types.most_common()))
    if sign_types:
        print("sign_violated by variable unknown type: " +
              "  ".join(f"{t}:{c}" for t, c in sign_types.most_common()))
    criterion = (f"max({arguments.agreement:g}, {arguments.condition_factor:g}*eps/rcond)"
                 if arguments.condition_factor > 0.0 else f"{arguments.agreement:g}")
    print(f"falsifiers (accepted, cl1 kode 0, relative difference > {criterion}): {len(falsifiers)}")
    for log, line_number, text in falsifiers[:arguments.max_listed]:
        print(f"  {log}:{line_number}: {text}")
    return 1 if falsifiers else 0


if __name__ == "__main__":
    sys.exit(main())
