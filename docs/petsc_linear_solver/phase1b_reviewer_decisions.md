# Phase 1B: Reviewer Decisions

These decisions were written by the reviewer, not by an agent. Agents treat them as authoritative and must not edit this file. They resolve `phase1b_task0_cl1_problem_audit_report.md` and the open decisions of `phase1b_design.md` §9.

**B-D1. The audit is accepted; design §2 is confirmed with corrections.**
- `ineq()` always calls `cl1()` with `kode = 1`. The only callers pass `in_kode = 1`, and `in_kode = 0` for the retry after return code 2. The `in_kode == 2` branch is unreachable.
- `delta1` is cleared, then set to −1 only for in-model pure phases with moles ≤ 0 that are not (undersaturated with an empty `add_formula`). +1 is never written.
- In the A block, `res = 1` only for pure phases with an empty `add_formula` that are not `dissolve_only`, and for gas and SS rows. ALK and SOLUTION_PHASE_BOUNDARY rows have `res = 0`.
- Between the end of the E block and `cl1()`, `ineq_array` is still modified by three steps: mass-water column zeroing, `pp_column_scale` (kinetics retries, 10⁻¹⁰), and `SHRINK_ARRAY` compaction.
- After `cl1()`, only `delta1` (copied and multiplied by `normal`) and `l_kode` (return code, and the MB adjustment when kode ≠ 0) are used functionally. `res` is read only when kode ≠ 0. `l_iter` and `l_error` are read only for debug output.

**B-D2. The hook position H1 is approved:** one guarded hook immediately before the `cl1(...)` call in `ineq_cl1_original` (design §9, decision 1). C10 shows it is the only position that sees exactly the problem `cl1()` receives. The Phase 1 rule "only the name changes inside the old `ineq` body" is replaced by "the name changes, plus the guarded hook statements immediately before and after the `cl1(...)` call". Nothing else in the body may change.

**B-D3. The row/column mismatches are left to the generic rule; no special cases by unknown type.** The equivalence argument (design §3) is a property of the linear program, not of what the rows mean. If the A and C rows restricted to the non-zero columns form a square, nonsingular system, and its solution satisfies every E row and every sign restriction, that solution is the problem's unique optimum on those columns, whichever unknowns produced the rows. The audit's mismatches therefore cannot make an accepted step wrong:
- the `equi_delay` window;
- absent, undersaturated `force_equality` phases;
- exchangers with negligible mass;
- SURFACE_CB with a non-empty `add_formula`;
- the all-zero AH2O row under Pitzer/SIT.

They only change squareness, so they fall under `not_square`, or under `solve_failed` for a zero row. Two mismatches could also cancel and leave a square system; that case is covered by the same argument. The only remaining assumption is that `cl1()` returns 0 on all-zero columns, which Task 1 measures (the largest |cl1 value| on excluded columns).

**B-D4. The +1 sign check is kept** as a defensive check (design §5 step 5). It costs nothing, `cl1()` supports it, and a future PHREEQC could set it.

**B-D5. The live columns J are read from the final `ineq_array`**, as designed. That includes columns made non-zero by the zero-row repair (`1e-5 · moles`), and every column scaling already applied. The candidate is computed in cl1's scaled variables. `ineq()` multiplies by `normal` afterwards, as it does for `cl1()`'s result.

**B-D6. The scope of the H3 exclusions is confirmed.**
- `in_kode ≠ 1` covers only the `ineq(0)` retry.
- Pitzer/SIT is excluded (the compacted layout and the AH2O zero row).
- `mass_water_switch` is excluded.
- Numerical derivatives are excluded (D17).

Each exclusion is counted under its own outcome.

**B-D7. Task 1 is shadow mode only.** The hook classifies and solves every call, `cl1()` always runs, and the two are compared. The Phase 1A entry dispatcher is switched off in shadow mode (`PHREEQC_LINEAR_SOLVER_INTERCEPT=cl1_problem_shadow`), so that every Newton step is `cl1()`'s. A shadow run must therefore be **byte-identical** to a `PHREEQC_LINEAR_SOLVER=cl1` run; that is its primary correctness check. The equivalence claim stands unless an accepted record shows `cl1()` returning kode 0 with a relative difference above 10⁻¹⁰ (a *falsifier*). Any falsifier stops Phase 1B until it is explained.

**B-D8. Results of Phase 1B Task 1.**
- **(a) Accepted as an implementation.**
  - All 10 shadow runs are identical to their `cl1` runs (`cmp` on `.sel`, the script on `.out`), so the hook changes nothing.
  - The default mode is unchanged against Task 3, and there is no shadow line without `INTERCEPT`.
  - Tests pass in every tree (4, 7, 10, ASan 4), and ASan is clean.
  - `model.cpp` changed only in the dispatcher condition and the two blocks around the unchanged `cl1(...)` statement.
- **(b) The equivalence claim is not falsified. The fixed 10⁻¹⁰ criterion of B-D7 was too strict.**
  - The 23 records reported as falsifiers have relative differences of 1.1 × 10⁻¹⁰ to 8.7 × 10⁻¹⁰. The median over all 1,004 comparable accepted records is 1.0 × 10⁻¹⁵, and the largest |cl1 value| on excluded columns is 0.
  - Two different algorithms (LU and cl1's simplex) solving the same nonsingular system agree only to about κ·ε, where κ is the condition number. Accepted systems have κ up to 1/rcond = 10⁸, so a few × 10⁻¹⁰ is expected round-off, not a different answer.
  - **The falsifier criterion becomes: relative difference > max(10⁻¹⁰, 100·ε/rcond)**, with ε = 2.2 × 10⁻¹⁶. `summarize_cl1_problem_shadow_log.py --condition-factor 100` implements it. The Task 1 logs are re-evaluated with it before Task 1b, and any record that still fails is a real falsifier.
- **(c) The decisive finding: the conditioning gate, not feasibility, rejects almost everything.**
  - Of 1,430,398 CO₂–basalt calls, 1,408,087 (98.4 %) are rejected as `ill_conditioned`. ex2 has 861 of 863, ex6 5,743 of 5,855.
  - Bound violations are rare: 30 inequality violations, all on GAS_MOLES rows in ex7, and 7 sign violations in the CO₂–basalt input.
  - The likely cause is the metric. The reciprocal condition is computed after **row** equilibration only. cl1-problem systems mix columns of very different scale: trace-element activities against mineral mole-transfer columns, for example an Al mass-balance row whose Al-species entries are about 10⁻⁸ next to a coefficient of 1 for kaolinite. Such a system is well-posed after column scaling, but looks nearly singular in the row-only metric.
  - Partial-pivoting LU is essentially invariant to column scaling, so the solution itself is not the issue. The gate metric is.
  - The Phase 2 calibration (P2-D19) used aqueous-only systems, where column scales are similar.
- **(d) Calls on which cl1 failed.** 104 accepted records (kode 1: 89, kode 2: 15) are calls on which `cl1()` failed and LAPACK found a feasible solution. In steering mode these calls would succeed where `cl1()` fails. They are reported per input in steering validation, as ex4 was in P2-D20.
- **(e) Next: Task 1b, conditioning diagnostics** (shadow mode, no steering).
  - Compute a second metric, the reciprocal condition after row **and** column equilibration.
  - Run the shadow with the gate in log mode, so that every square, feasible call is solved and compared.
  - Calibrate both metrics with `calibrate_cl1_problem_condition.py`.

  The metric and threshold for the cl1-problem path are chosen from that table. If the two-sided metric separates agreeing from disagreeing steps while keeping most agreeing ones, the projected acceptance rate in the CO₂–basalt input rises from 0.007 % towards the about 98 % that are not bound-violated or excluded.

**B-D9. Results of Phase 1B Task 1b: the equivalence premise does not hold for most CO₂–basalt calls, and the likely reason is cl1, not LU.**
- **(a) Accepted as an implementation.**
  - All 10 log-mode shadow runs are identical to the `cl1` runs.
  - The defaults are byte-identical to Task 1.
  - All test suites pass, and ASan is clean.
  - The new tests behave as intended: the column-scaled system gives a row metric of 9.7 × 10⁻¹³ and a two-sided metric of 0.36, while the nearly singular system stays at 2.5 × 10⁻¹⁵ under both.
- **(b) Task 1 re-evaluated:** 0 falsifiers under the condition-scaled criterion.
- **(c) With the gate in log mode, the agreement collapses in the CO₂–basalt input.**
  - Of 1,038,592 accepted records with cl1 kode 0, the median relative difference from `cl1()` is **6.4 × 10⁻⁷**. Only 3,351 agree to 10⁻¹⁰; 461,129 differ by more than 10⁻⁶, and some by up to 1.
  - Their two-sided reciprocal condition is around 5 × 10⁻², i.e. well-conditioned. For two exact solvers that would allow differences of about 10⁻¹⁴, not 10⁻⁶.
  - ex2, ex6 and ex8 show the same effect on a smaller scale (medians 7.5 × 10⁻¹² to 3.9 × 10⁻¹⁰, maxima up to 2 × 10⁻⁵). ex1, ex5 and `units` do not.
  - The 31 falsifiers reported under the row-based criterion are a small visible part of this. **Design §3's conclusion, that the LU solution "is" cl1's answer, is therefore not supported in these systems.**
- **(d) Working hypothesis: `cl1()` does not solve its own problem exactly.**
  - `cl1()` refuses to pivot on any entry whose **absolute** value does not exceed `toler` (cl1.cpp documentation, lines 105–112). PHREEQC passes `ineq_tol = 10^-DBL_DIG = 10⁻¹⁵` in the default build (Phreeqc.cpp, `#else` branch).
  - Entries for trace species (O₂, H₂, CH₄, trace elements) in mass-balance and redox rows can be far below 10⁻¹⁵ even after PHREEQC's scaling. `cl1()` then effectively treats them as zero, and returns a point whose residuals on those rows are not zero, while still reporting kode 0.
  - LU has no absolute threshold and solves the full system (backward error ≤ 10⁻¹⁰ by construction).

  If this holds, LU's step is the *more* exact solution of the same Newton problem. Accepting it then cannot be justified as "identical to `cl1()`". It can only be justified by its effect on the final results.
- **(e) Next: Task 1c, a short diagnostic test of (d)** in shadow mode, with no change to `model.cpp`.
  - `prepare` keeps a copy of the problem.
  - `record` computes the relative residuals of `cl1()`'s solution on the A, C and E rows and of the LU candidate on the A and C rows, together with the smallest nonzero entry of the square system and the number of entries at or below 10⁻¹⁵.
  - `analyze_cl1_problem_residuals.py` reports, by agreement group, how often `cl1()` was inexact.

  **The hypothesis is confirmed** if disagreeing records show `cl1()` residuals well above round-off together with entries at or below 10⁻¹⁵, while agreeing records show neither.
- **(f) The plan, depending on Task 1c:**
  - **If (d) is confirmed,** Phase 1B Task 2 (steering) proceeds with a different acceptance basis. The step-level safety checks stay: square system; two-sided metric with an initial threshold of 10⁻⁸ (for co2 it gates 0 of 3,351 agreeing records, and only genuinely singular systems such as the unpoised-redox case); E rows; sign restrictions; backward error. Equivalence to `cl1()` is no longer claimed. Validation is by **final results**: P2-D19 (e) tolerances against `cl1()` runs, plus iteration counts and convergence failures, compared per input. A change that is larger than tolerance, or worse convergence, blocks steering for that class of systems.
  - **If (d) is refuted,** the disagreement has another cause, which must be found before any steering.
- **(g) The metric decision is deferred to Task 2.** The row metric is unsuitable: at 10⁻⁸ it gates 3,350 of 3,351 agreeing CO₂–basalt records. The two-sided metric is the candidate. Its threshold is then set from steering results (final outputs and convergence), not from step agreement with a solver that is itself inexact.

**B-D10. Results of Phase 1B Task 1c: the hypothesis of B-D9 (d) is refuted as stated, and the disagreement is understood differently.**
- **(a) Accepted as an implementation.**
  - Tests pass in all four trees, and the defaults are byte-identical to Task 1b.
  - All six log-mode runs are identical to their `cl1` runs, with the shadow statistics lines unchanged.
- **(b) "cl1 is inexact" does not separate the groups.**
  - `cl1()`'s relative row residual exceeds 10⁻¹⁰ in 48.9 % of agreeing records and in 35.7 % of disagreeing ones.
  - Entries at or below 10⁻¹⁵ occur in 26.9 % and 27.2 % respectively.
  - In the extreme cases `cl1()` clearly did not solve its rows: in the 10 largest CO₂–basalt differences its residuals are 0.6–1.0, against about 10⁻¹⁶ for LU. But most disagreeing records have `cl1()` residuals of about 10⁻¹² and still differ by more than 10⁻⁶.
- **(c) The interpretation that fits all of the data: weakly determined components.**
  - The largest difference sits on MB unknowns (type 10) in the disagreeing records, and on MH (type 16) in the agreeing ones.
  - If an unknown enters every row only through very small coefficients (a trace master species), the rows constrain it only weakly. Both solvers then satisfy every row to near round-off, the two-sided measure stays good (about 5 × 10⁻²), and yet that component differs greatly between them, because its effect on any residual is negligible.
  - The difference is therefore real but *inconsequential for the rows*. Whether it matters for the result depends on PHREEQC's step limiting and convergence test, and must be measured on final results, not on steps.
  - **Step-level equivalence to `cl1()` is abandoned as an acceptance criterion** (design §3 holds mathematically, but not numerically for these components). B-D9 (f) applies unchanged: steering is judged by final results and convergence.
- **(d) A componentwise residual check is added.** One ex8 record shows an LU componentwise (Oettli–Prager) row residual of 9.4 × 10⁻³. Elsewhere the maximum is 1.9 × 10⁻⁸ (intermediate group) and 1.5 × 10⁻¹⁰ (disagreeing group), against a median of 2 × 10⁻¹⁶. The normwise backward error alone does not catch this. Steering therefore requires `max_r |b_r − a_r·x| / (|b_r| + Σ_j |a_rj x_j|) ≤ 10⁻⁸` over the A and C rows (new outcome `componentwise_backward_error`). Default `PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_MAX_COMPONENTWISE_ERROR=1e-8`.
- **(e) Gate settings for steering (initial; revisited from Task 2 results).**
  - **Metric:** two-sided (`row_column`), threshold 10⁻⁸.
  - **Checks:** square system; backward error ≤ 10⁻¹⁰; componentwise error ≤ 10⁻⁸; E rows and signs with no tolerance.
- **(f) Next: Task 2, steering mode with result-based validation.** Every input in the Task 1 set is run with steering and with `cl1` only. Steering is acceptable for an input only if all four of these hold:
  1. its outputs are within the P2-D19 (e) tolerances, or the differences are listed and explained;
  2. its total number of `ineq()` calls (a proxy for Newton iterations) does not increase by more than 5 %;
  3. no new convergence warning or error appears;
  4. its run time does not get worse.

  The CO₂–basalt run time, 3 repeats each, is the headline number.

**B-D11. Results of Phase 1B Task 2.**
- **(a) Accepted as an implementation.**
  - All trees build and pass their tests. Defaults and the ex4 shadow line are unchanged (apart from the appended field).
  - `model.cpp` changed only in the specified block. Rejected calls write nothing.
  - ASan is clean.
  - The componentwise check fires in real runs (6 in ex8, 7 in co2).
  - The two deviations are accepted: the captured ex8 matrix for test 8, and the leftover `build/lapack_only/tmp_ex8dbg`, which the reviewer may delete.
- **(b) Acceptance:** CO₂–basalt 99.91 % (1,338,728 of 1,339,910). ex1, ex3 and `units` 100 %; ex4, ex6 and ex8 97–99 %; ex5 82 %; ex7 78 %; ex2 37 %.
- **(c) Results.**
  - Within tolerance: ex1, ex4, ex5, `units` and **co2**.
  - Different: ex2, ex3, ex6, ex7 and ex8. Every reported first difference is in a redox-sensitive trace quantity (H2, CH4, N2, pe, the SI of CH4(g)). The largest is ex2's `pe`, with a relative difference of up to 1.2 in 48 rows.
  - This matches B-D10 (c), weakly determined redox components, but **only the first differing lines were reported. The full lists must be classified before any of these inputs may steer.**
  - No warning or error count changed anywhere.
- **(d) Convergence.** The `ineq()` call ratio is ≤ 1.02 everywhere, and below 1 for co2 (0.94) and ex8 (0.91), **except ex3, at 1.60** (72 calls against 45). ex3 fails criterion 2 of B-D10 (f).
- **(e) Speed. Steering is 6.6 % slower on CO₂–basalt** (medians 37.74 s against 35.39 s), despite 99.9 % acceptance and 6 % fewer calls. ex6 and ex8 are slower too.
  - The accepted path is more expensive than the `cl1()` call it replaces. Each accepted call costs roughly 15–16 µs, against `cl1()`'s 9–14 µs.
  - Each call performs **two** LU factorizations and **two** condition estimates: the row metric is always computed when the mode is not `off`, and the two-sided metric uses a second `dgetrf`. On top of those come the copies and three residual passes.
  - Also, `cl1()` on a square, feasible problem is itself a dense pivoting elimination of the same O(n³) order. The Phase 1B design's estimate (design §8) assumed `cl1()` costs several times a plain LU, and that assumption was too optimistic.
- **(f) Next: Task 3, slimming the accepted path and measuring it, plus the classification work.**
  - **One factorization, one condition estimate.** Partial pivoting is invariant to column scaling. If P·A = L·U, then P·A·D⁻¹ = L·(U·D⁻¹), so the two-sided metric equals `dgecon` applied to L and the column-scaled U, with no second `dgetrf`. When gating on the two-sided metric, the row-metric `dgecon` is skipped. Shadow mode keeps its current diagnostic computations unchanged.
  - **A time accumulator** around the steering evaluation, printed in the steering statistics line.
  - **The full lists of differing lines** for ex2, ex3, ex6, ex7 and ex8, and the per-calculation iteration counts of ex3.
- **(g) Stop criterion for Phase 1B steering.** If the slimmed steering path is not at least **10 % faster** than `cl1` on the CO₂–basalt input (median of 3 alternating runs), steering is not pursued further. It stays in the code, off by default, as a diagnostic, and Phase 1B closes with the shadow-mode findings as its result.

**B-D12. Results of Phase 1B Task 3, and the closing decision for Phase 1B.**
- **(a) Accepted as an implementation.**
  - The fast two-sided metric (one factorization; U scaled by the column maxima; one `dgecon`; row metric skipped) is implemented as specified, together with the approved one-condition change in `cl1_problem_check.cpp`.
  - All test suites pass (4, 10, 7, ASan 4).
  - Defaults and shadow mode are byte-identical to Task 2. **All 10 steering runs make exactly the same decisions and produce byte-identical outputs to Task 2.** This is also the real-data confirmation that the fast metric gates exactly as the full one (12, 426, 1, 25, 68, 77, 1 gated calls, identical to Task 2). Test 14 does not exercise the gate (its smallest value is 1.4 × 10⁻⁷); that limitation is accepted, because the real-data evidence covers it.
  - The agent's deviations are accepted. The reviewer deletes `build/lapack_only/tmp_ex8dbg` by hand.
- **(b) Speed.**
  - On CO₂–basalt, with a quiet node, the medians are 33.56 s (`cl1`) and 30.17 s (steering): **10.1 % faster**. The per-pair gains are 10.9 %, 10.1 % and 7.0 %.
  - The accepted path costs **8.0 µs per accepted call** (Task 2: 15–16 µs). The `cl1()` calls it replaces cost about 10.5 µs each in this run: (10.7 s + 3.4 s saved) / 1.34 million.
  - The stop criterion B-D11 (g) is met formally by 0.12 percentage points, which is inside run-to-run spread. **The achievable gain is about 10 %, not the 1.5 times estimated in design §8.** `cl1()` on these square problems is already a dense elimination of the same order.
- **(c) Results. The B-D10 (f) acceptance criteria fail for two standard examples.**
  - ex2, ex7 and ex8 differ only in the last printed digit of redox trace species (H₂, O₂, sulfide species, N species), plus a few iteration counts.
  - **ex3**, from part C on, and **ex6** converge to **different redox states**:
    - ex3: pe −1.589 against 10.526; CH₄ 4 × 10⁻²⁶ against 0; the Sulfur SI −16.4 against −89.1;
    - ex6: pe 9.54 against −3.79; O(0) 2 × 10⁻¹² against 0.

    ex3 also needs 72 iterations instead of 45 (part D 5 → 17, part E 3 → 15), which fails criterion 2.

  These are weakly poised redox systems, where PHREEQC's convergence test does not pin pe (B-D10 (c)). Both answers satisfy the equations to tolerance, but a solver change must not silently move a standard example's pe by 12–13 units.
- **(d) Decision: Phase 1B steering is not adopted as a default, and Phase 1B closes.**
  - A borderline gain of about 10 %, against changed results in standard examples, does not justify making steering the default.
  - The code stays, **off by default**, as an opt-in experimental mode (`PHREEQC_LINEAR_SOLVER_INTERCEPT=cl1_problem`), together with shadow mode as a diagnostic.
  - Anyone opting in must validate each input class against a `cl1` run first, as described in `phase1b_final_report.md`. Opt-in steering is not ported to PhreeqcRM, and PhreeqcRM keeps the Phase 2 state.
- **(e) What a future attempt would have to solve first:** a criterion that recognises weakly poised redox systems *before* steering, and routes them to `cl1()`. Without one, steering cannot be made safe by default. With one, the gain is still bounded by about 10–15 % per the measurements above.

**B-D13. Leftovers from Phase 1B Task 3, and their handling.**
- **(a) Test 14 does not exercise the gate.** It is fixed by the test-only Task 3b (`phase1b_task3b_test14_gate_coverage.md`): the near-singular rows are built before the column scaling, and at least 80 gated and 80 accepted cases are required.
- **(b) `build/lapack_only/tmp_ex8dbg`** is deleted by the reviewer by hand.
- **(c) The full Task 3 difference lists** live only in `/tmp/opencode/p1b3_diff_*.log`, which is volatile. They are copied into `docs/petsc_linear_solver/phase1b_task3_differences/`.
- **(d) Reports given only in chat.** The final reports of Phase 2 Task 2b and of Phase 1B Tasks 1, 1b, 1c, 2 and 3 were given in chat only. Their session exports are copied into `docs/petsc_linear_solver/session_exports/`, so that every decision in this file has its evidence in the repository.
- **(e) The borderline speed result, and the ex3/ex6 redox changes,** are decided by B-D12. Nothing further is needed.

**B-D14. Task 3b: the agreement tolerance of test 14 is split by region.**
- **What the fixed generator showed.**
  - The gate is now exercised: 100 matrices are gated by both methods (all odd t), and 100 are passed by both (all even t).
  - Gate decisions agree in all 200 cases, and statuses and solutions are identical everywhere.
  - 58 near-singular cases fail the old assertion "relative agreement ≤ 10⁻⁶ whenever both values exceed 10⁻¹⁴". All of them have both values between 1.2 × 10⁻¹⁴ and 5.3 × 10⁻¹², i.e. far below the 10⁻⁸ gate, with relative differences of 1.07 × 10⁻⁶ to 1.40 × 10⁻⁵.
- **Why this is expected.** A condition estimate of a nearly singular matrix depends on its smallest U pivots, which are themselves only accurate to about ε relative to the largest entries. The two methods round differently: one scales U after factorizing, the other factorizes the scaled matrix. Their estimates can therefore differ by up to about ε/rcond relative, which is 10⁻⁴ at rcond = 10⁻¹². The observed maximum of 1.4 × 10⁻⁵ is inside that bound. What matters for the gate is the decision, and that is identical in every case.
- **Decision.** Test 14 asserts:
  1. identical gate decisions at 10⁻⁸ in all cases, as before;
  2. relative agreement ≤ 10⁻⁶ when both values are ≥ 10⁻⁸ (the accepted region, where the value is used);
  3. relative agreement ≤ 10⁻⁴ when both values are below 10⁻⁸ and above 10⁻¹⁴ (a sanity check in the gated region);
  4. at least 80 cases gated and at least 80 passed by both methods, as before.

  Statuses and solutions remain identical. No other tolerance changes.
