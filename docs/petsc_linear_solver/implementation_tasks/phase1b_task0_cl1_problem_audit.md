# Phase 1B Task 0: Audit of the cl1 Problem Built by ineq() (read-only)

## Context

The Phase 1B design (`docs/petsc_linear_solver/phase1b_design.md`) proposes solving **the constrained L1 problem that `ineq()` hands to `cl1()`** with LU. A step is accepted only when the problem is square, well-conditioned, and feasible at its zero-residual point; otherwise `cl1()` runs as today. The equivalence argument (design §3) depends on exact facts about how `ineq()` builds that problem. This task verifies them in **PHREEQC v3.9.0 as it is in `~/phreeqc3` at tag `petsc-phase1a-task0`**, the unmodified upstream `src/model.cpp` and `src/cl1.cpp`.

Use `git -C ~/phreeqc3 show petsc-phase1a-task0:src/model.cpp` and `git -C ~/phreeqc3 show petsc-phase1a-task0:src/cl1.cpp` (piped to `grep -n`, `sed -n`) so that line numbers refer to the unmodified file. **Nothing is modified.**

## Rules

1. Edit only `docs/petsc_linear_solver/phase1b_task0_cl1_problem_audit_report.md`.
2. Every claim gives the file, the line numbers, and the quoted lines, all from the tag above. Write NOT FOUND or AMBIGUOUS rather than guessing.
3. Never run `git commit`, `checkout`, `apply`, `reset`, `stash`, or `clean`. Never build anything.
4. Use Plan mode first, present the plan, and execute only after the reviewer replies `APPROVED`. Keep your reasoning short before each tool call. Write the report in parts of at most about 150 lines per edit.

## Items

**C1. The `cl1()` interface** (`src/cl1.cpp`). Quote:
- the signature;
- the documentation of K, L, M, N, Q layout, KODE (entry and exit values), TOLER, ITER, X, RES and ERROR;
- in particular, the KODE = 1 sign restrictions on X(j) (−1, 0, 1) and on RES(i) for i ≤ K.

State whether the code reads `X` and `RES` on entry only when KODE = 1, quoting the lines where it does.

**C2. Row blocks in `ineq()`.** For each of the three blocks (A: "equations to optimize"; C: "equality equations"; E: "inequality constraints"), quote the loop and list every unknown type that can produce a row, with the exact condition. For every row, state:
- its source (a copy of a `my_array` row, or a constructed row);
- any modification of the copied row (for example the `mass_water_switch` subtraction for MH, or `pp_scale`);
- for A rows, the value written to `res[row]`, and when it is reset (`in_kode != 1`);
- for E rows, the constructed coefficients and right-hand side.

**C3. Variable sign restrictions.** Quote every write to `delta1[...]` in `ineq()` before the `cl1()` call: the initial clearing, and each `delta1[i] = …`.

**C4. Column exclusions.** Quote every loop that sets a whole column of `ineq_array` to zero, with its condition. For each, state whether the unknown's row is also excluded from the A, C and E blocks under the same condition, and quote both conditions side by side. List every case where a column is zeroed but a row for that unknown remains, or a row is excluded but its column remains.

**C5. Scalings.** Quote:
- the computation of `normal[i]` (the "Scaling column" block);
- the MH diagonal modification (`MIN_TOTAL`);
- the zero-row repair (`1e-5 * x[i]->moles`);
- `pp_scale`;
- `pp_column_scale`.

For each, state whether it is applied to `my_array` (before copying), to `ineq_array`, or to both, and quote where `pp_scale` and `pp_column_scale` are set elsewhere (`grep -n` over `src/*.cpp`).

**C6. Dimensions and layout.** Quote the computation of `max_row_count`, `max_column_count`, k, l, m, n, `l_klmd`, `l_nklmd` and `l_n2d`, and the `SHRINK_ARRAY` block. State the row stride of `ineq_array` in both the normal and the shrunk layout, and the column that holds the right-hand side.

**C7. After `cl1()`.** Quote everything from the `cl1(...)` call to the end of `ineq()`, and state exactly which of `delta1`, `res`, `l_kode`, `l_iter` and `l_error` are read afterwards, and under which conditions. Confirm or refute that `SLNQ` is not defined in the build (`grep -rn "SLNQ" CMakeLists.txt src/*.h`).

**C8. The caller.** Quote both call sites of `ineq(...)` in `model()` (the Task 0 report gives lines near 161 and 178) and how the return code (`OK`, `ERROR`, `2`) is handled, including the retry with `ineq(0)`.

**C9. Squareness.** From C2–C4, list every condition under which k + l (the A and C rows) differs from the number of columns that are not all zero. Examples to check, each confirmed or refuted from the code:
- `iterations < aqueous_only` (A rows for pure phases skipped while their columns stay);
- `force_equality` pure phases;
- `add_formula` pure phases;
- `dissolve_only`;
- exchange or surface with negligible mass but related phases;
- `gas_in == FALSE` with a gas phase present;
- `mass_water_switch`;
- Pitzer/SIT with `full_pitzer == FALSE`.

**C10. The hook location.** Quote 10 lines before and after the `cl1(` call, and state whether any statement between the end of the E-block construction and the `cl1()` call modifies `ineq_array`, `delta1` or `res`. This is where the Phase 1B hook would go.

## Output

Write `docs/petsc_linear_solver/phase1b_task0_cl1_problem_audit_report.md` with items C1–C10, each giving its quotes and a one-paragraph statement of fact. End with **"Answers to the design's [VERIFY] items"**, one line per item in `phase1b_design.md` §2–§5, saying CONFIRMED, REFUTED (with the correct fact) or AMBIGUOUS. Then add **"Reviewer decisions required"**. Stop.
