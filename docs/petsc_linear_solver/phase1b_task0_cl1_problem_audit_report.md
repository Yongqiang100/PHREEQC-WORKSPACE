# Phase 1B Task 0: Audit of the cl1 Problem Built by ineq() — Report

| Field | Value |
|---|---|
| Tag audited | `petsc-phase1a-task0` = `422f5676c7f9333dc908a40fefba38fceb98ed12` ("Task 0 reviewer decisions; Phase 1A specs revised"), verified with `git rev-parse` / `git log`. Matches the reviewer's tag check `422f5676`. |
| Files | `src/model.cpp` (5846 lines), `src/cl1.cpp` (881 lines), `src/Phreeqc.h`, `src/global_structures.h`, `src/Phreeqc.cpp`, `src/read.cpp`, `src/kinetics.cpp`, `src/pitzer.cpp`, `src/sit.cpp`, `src/prep.cpp`, all read via `git show petsc-phase1a-task0:<file>` / `git grep <pattern> petsc-phase1a-task0`. Nothing modified. |
| Line numbers | Refer to the tagged files. |

Unknown-type constants (`src/global_structures.h:81-99`): `MB 10, ALK 11, CB 12, SOLUTION_PHASE_BOUNDARY 13, MU 14, AH2O 15, MH 16, MH2O 17, PP 18, EXCH 19, SURFACE 20, SURFACE_CB 21, SURFACE_CB1 22, SURFACE_CB2 23, GAS_MOLES 24, SS_MOLES 25, PITZER_GAMMA 26, SLACK 28`. The test `x[j]->type < PP` therefore means types MB..MH2O (aqueous-side balance unknowns).

## C1. The cl1() interface (src/cl1.cpp)

Signature, `src/cl1.cpp:21-27`:

```c
int Phreeqc::
cl1(int k, int l, int m, int n,
	int l_nklmd, int l_n2d,
	LDBLE * q,
	int *l_kode, LDBLE l_toler,
	int *l_iter, LDBLE * l_x, LDBLE * l_res, LDBLE * l_error,
	LDBLE * l_cu, int *l_iu, int *l_s, int check)
```

Declaration `src/Phreeqc.h:196-202`. Member workspace `std::vector<double> x_arg, res_arg, scratch;` `src/Phreeqc.h:1677`.

Documentation block `src/cl1.cpp:62-144` (verbatim highlights):

- Problem, lines 62-69: "THIS SUBROUTINE USES A MODIFICATION OF THE SIMPLEX METHOD OF LINEAR PROGRAMMING TO CALCULATE AN L1 SOLUTION TO A K BY N SYSTEM OF LINEAR EQUATIONS / AX=B / SUBJECT TO L LINEAR EQUALITY CONSTRAINTS / CX=D / AND M LINEAR INEQUALITY CONSTRAINTS / EX.LE.F."
- K, L, M, N, lines 71-74: "K NUMBER OF ROWS OF THE MATRIX A (K.GE.1). L NUMBER OF ROWS OF THE MATRIX C (L.GE.0). M NUMBER OF ROWS OF THE MATRIX E (M.GE.0). N NUMBER OF COLUMNS OF THE MATRICES A,C,E (N.GE.1)."
- Workspace dimensions, lines 75-78: "KLMD SET TO AT LEAST K+L+M ... KLM2D SET TO AT LEAST K+L+M+2 ... NKLMD SET TO AT LEAST N+K+L+M ... N2D SET TO AT LEAST N+2".
- Q layout, lines 79-87: "Q TWO DIMENSIONAL REAL ARRAY WITH KLM2D ROWS AND AT LEAST N2D COLUMNS. ON ENTRY THE MATRICES A,C AND E, AND THE VECTORS B,D AND F MUST BE STORED IN THE FIRST K+L+M ROWS AND N+1 COLUMNS OF Q AS FOLLOWS / A B / Q = C D / E F / THESE VALUES ARE DESTROYED BY THE SUBROUTINE."
- KODE, lines 88-104: "ON ENTRY, THIS SHOULD NORMALLY BE SET TO 0. HOWEVER, IF CERTAIN NONNEGATIVITY CONSTRAINTS ARE TO BE INCLUDED IMPLICITLY ... THEN KODE SHOULD BE SET TO 1, AND THE NONNEGATIVITY CONSTRAINTS INCLUDED IN THE ARRAYS X AND RES ... ON EXIT, KODE HAS ONE OF THE FOLLOWING VALUES / 0- OPTIMAL SOLUTION FOUND, / 1- NO FEASIBLE SOLUTION TO THE CONSTRAINTS, / 2- CALCULATIONS TERMINATED PREMATURELY DUE TO ROUNDING ERRORS, / 3- MAXIMUM NUMBER OF ITERATIONS REACHED."
- TOLER, lines 105-112: "A SMALL POSITIVE TOLERANCE ... IT WILL NOT PIVOT ON ANY NUMBER WHOSE MAGNITUDE DOES NOT EXCEED TOLER."
- ITER, lines 113-116: entry = upper bound on iterations ("A SUGGESTED VALUE IS 10*(K+L+M)"); exit = number of simplex iterations.
- X, lines 117-125: "ON EXIT THIS ARRAY CONTAINS A SOLUTION TO THE L1 PROBLEM. IF KODE=1 ON ENTRY, THIS ARRAY IS ALSO USED TO INCLUDE SIMPLE NONNEGATIVITY CONSTRAINTS ON THE VARIABLES. THE VALUES -1, 0, OR 1 FOR X(J) INDICATE THAT THE J-TH VARIABLE IS RESTRICTED TO BE .LE.0, UNRESTRICTED, OR .GE.0 RESPECTIVELY."
- RES, lines 126-136: "ON EXIT THIS CONTAINS THE RESIDUALS B-AX IN THE FIRST K COMPONENTS, D-CX IN THE NEXT L COMPONENTS (THESE WILL BE =0), AND F-EX IN THE NEXT M COMPONENTS. IF KODE=1 ON ENTRY, THIS ARRAY IS ALSO USED TO INCLUDE SIMPLE NONNEGATIVITY CONSTRAINTS ON THE RESIDUALS B-AX. THE VALUES -1, 0, OR 1 FOR RES(I) INDICATE THAT THE I-TH RESIDUAL (1.LE.I.LE.K) IS RESTRICTED TO BE .LE.0, UNRESTRICTED, OR .GE.0 RESPECTIVELY."
- ERROR, lines 137-138: "ON EXIT, THIS GIVES THE MINIMUM SUM OF ABSOLUTE VALUES OF THE RESIDUALS."

Entry reads of X and RES are guarded, `src/cl1.cpp:244-289`:

```c
244:	if (*l_kode != 0)
246:		for (j = 0; j < n; ++j)
248:			if (l_x[j] < 0.)
251:				l_cu[j] = 1.;
254:			else if (l_x[j] > 0.)
256:				l_cu[cu_dim + j] = 1.;
264:		for (j = 0; j < k; ++j)
266:			jpn = j + n;
267:			if (l_res[j] < 0.)
270:				l_cu[jpn] = 1.;
277:			else if (l_res[j] > 0.)
280:				l_cu[cu_dim + jpn] = 1.;
289:	}
```

`kode_arg = *l_kode;` is saved at line 151. Exit KODE is set at line 464 (`*l_kode = 2`, no pivot > toler in the ratio test, lines 440-448, 461-465), line 544 (`*l_kode = 3`, `*l_iter >= maxit`, lines 542-544), lines 627/630 (Phase-1 infeasible → 1; else 0). The post-solution check block (lines 743-847) runs only `if ((check == 1) && (*l_kode == 0))` and, within it, the X/RES sign-constraint checks only `if (kode_arg == 1)` (lines 749, 812); it reads the member copies `x_arg`/`res_arg`, which `cl1_space` only ever zeroes (lines 854-867) — this version never fills them with the entry values. `ineq()` passes `check = FALSE` (`src/model.cpp:1781`), so this block never runs from the forward model.

Statement of fact: the interface is exactly as the design quotes. X and RES are read on entry only inside `if (*l_kode != 0)` (line 244) — the guard is "nonzero", not "== 1", but the documented and actually used entry value is 1 (C2/C6 below). RES is reused as workspace during the simplex ratio test (line 446), after its entry flags have been consumed.

## C2. Row blocks in ineq() (src/model.cpp:928-1925)

### A block — "Copy equations to optimize" (model.cpp:1136-1252)

Loop head, lines 1138-1142:

```c
1138:	l_count_rows = 0;
1139:	for (i = 0; i < count_unknowns; i++)
1140:	{
1141:		if (iterations < aqueous_only)
1142:			continue;
```

When `iterations < aqueous_only`, every A row is skipped (all types, not only PP), so `l_count_optimize = 0`.

PP rows, lines 1146-1206. Skips: `x[i]->phase->in == FALSE` (1152-1153); `comp_ptr->Get_force_equality()` (1161-1162); absent and undersaturated within the delay window, `x[i]->f > 0e-8 && x[i]->moles <= 0 && iterations >= x[i]->iteration + equi_delay && comp_ptr->Get_add_formula().size() == 0` (1164-1169); satisfied `dissolve_only`, `x[i]->f < 0e-8 && x[i]->dissolve_only == TRUE && (x[i]->moles - comp_ptr->Get_initial_moles() >= 0)` (1170-1175). Otherwise the row is a copy of the `my_array` SI row (1179-1181):

```c
1179:				memcpy((void *) &(ineq_array[(size_t)l_count_rows * max_column_count]),
1180:					   (void *) &(my_array[(size_t)i * (count_unknowns + 1)]),
1181:					   (count_unknowns + 1) * sizeof(LDBLE));
1182:				back_eq[l_count_rows] = i;
1184:				if (comp_ptr->Get_add_formula().size() == 0
1185:					&& x[i]->dissolve_only == FALSE)
1186:				{
1187:					res[l_count_rows] = 1.0;
1188:				}
1192:				if (pp_scale != 1)
1196:						ineq_array[(size_t)l_count_rows * max_column_count + (size_t)j] *= pp_scale;
1200:				if (in_kode != 1)
1202:					res[l_count_rows] = 0.0;
```

`res[row] = 1` (residual ≥ 0) only when `add_formula` is empty and the phase is not `dissolve_only`; otherwise it stays 0 (from the memset at 1132). The whole copied row, including its RHS, is multiplied by `pp_scale` when `pp_scale != 1`. The flag is reset to 0 when `in_kode != 1` (1200-1203).

ALK and SOLUTION_PHASE_BOUNDARY rows, lines 1207-1216: plain copy of the `my_array` row; `res` is never set (stays 0). GAS_MOLES rows, lines 1221-1233: `else if (x[i]->type == GAS_MOLES && gas_in == TRUE)`, copy, `res[l_count_rows] = 1.0;` reset when `in_kode != 1`. SS_MOLES rows, lines 1238-1249: `else if (x[i]->type == SS_MOLES && x[i]->ss_in == TRUE)` (note: `ss_in`, not `phase->in`), copy, `res = 1.0`, reset when `in_kode != 1`. `l_count_optimize = l_count_rows;` (1252).

### C block — "Copy equality equations" (model.cpp:1254-1346)

Loop head 1256. The first branch admits every type except `SOLUTION_PHASE_BOUNDARY, ALK, GAS_MOLES, SS_MOLES, PITZER_GAMMA` (1275-1281), i.e. MB, CB, MU, AH2O, MH, MH2O, EXCH, SURFACE, SURFACE_CB/CB1/CB2, and PP. Skips inside it:

```c
1282:			if (x[i]->type == PP && !comp_ptr->Get_force_equality())
1283:				continue;
1284:			if (x[i]->type == MH && pitzer_model == TRUE && pitzer_pe == FALSE)
1285:				continue;
1287:			if (mass_water_switch == TRUE && x[i] == mass_oxygen_unknown)
1288:				continue;
1292:			if (x[i]->type == EXCH && x[i]->moles <= MIN_RELATED_SURFACE)
1293:				continue;
1294:			if (x[i]->type == SURFACE &&
1295:				x[i]->phase_unknown == NULL
1296:				&& x[i]->moles <= MIN_RELATED_SURFACE)
1297:				continue;
1301:				cxxSurfaceCharge *charge_ptr = use.Get_surface_ptr()->Find_charge(x[i]->surface_charge);
1302:				if (charge_ptr->Get_grams() <= MIN_RELATED_SURFACE)
1303:				{
1304:					continue;
1305:				}
1307:			if (x[i]->type == SURFACE && x[i]->phase_unknown != NULL &&
1308:				x[i]->phase_unknown->moles <= MIN_RELATED_SURFACE &&
1309:				comp_ptr->Get_add_formula().size() == 0)
1310:				continue;
1316:				if (charge_ptr->Get_grams() <= MIN_RELATED_SURFACE &&
1317:					comp_ptr1->Get_add_formula().size() == 0)
1318:				{
1319:					continue;
1320:				}
```

So `force_equality` PPs get a C row; MH is skipped for Pitzer without pe; the mass-oxygen (MH2O) unknown is skipped when `mass_water_switch`; exchangers/surfaces/surface-charge balances with negligible mass are skipped (the SURFACE_CB skip at 1302 is unconditional on `add_formula`; the paired column-zeroing at 1525-1529 is not — see C4). Row copy 1322-1325. `mass_water_switch` modification of the copied MH row, 1326-1334:

```c
1326:			if (mass_water_switch == TRUE && x[i] == mass_hydrogen_unknown)
1328:				k = (int)mass_oxygen_unknown->number;
1331:					ineq_array[(size_t)l_count_rows * max_column_count + (size_t)j] -=
1332:						2 * my_array[(size_t)k * (count_unknowns + 1) + (size_t)j];
```

PITZER_GAMMA rows only when `full_pitzer == TRUE` (1337-1344). `count_equal = l_count_rows - l_count_optimize;` (1346).

### E block — "Copy inequality constraints" (model.cpp:1348-1661)

PP inequalities, only `if (pure_phase_unknown != NULL)` (1350), loop 1352-1403:

```c
1359:				if (x[i]->phase->in == FALSE)
1360:					continue;
1362:				if (x[i]->moles <= 0.0 && x[i]->f > 0e-8 &&
1363:					comp_ptr->Get_add_formula().size() == 0)
1364:				{
1365:					continue;
1366:				}
1368:				else if (x[i]->moles <= 0.0)
1369:				{
1370:					delta1[i] = -1.0;
1371:				}
1372:				else if (x[i]->f < 0e-8 && x[i]->dissolve_only == TRUE
1373:						 && (x[i]->moles - comp_ptr->Get_initial_moles() >=
1374:							 0))
1375:				{
1376:					continue;
1377:				}
1378:				else
1382:					memset(&ineq_array[(size_t)l_count_rows * max_column_count], 0, ((size_t) count_unknowns + 1) * sizeof(LDBLE));
1383:					ineq_array[(size_t)l_count_rows * max_column_count + i] = 1.0;
1384:					ineq_array[(size_t)l_count_rows * max_column_count + count_unknowns] = x[i]->moles;
1389:				if (x[i]->dissolve_only == TRUE)
1396:					ineq_array[(size_t)l_count_rows * max_column_count + i] = -1.0;
1397:					ineq_array[(size_t)l_count_rows * max_column_count + count_unknowns] =
1398:						comp_ptr->Get_initial_moles() - x[i]->moles;
```

Present phase: constructed row δᵢ ≤ molesᵢ (1382-1386). Absent phase (moles ≤ 0) that is supersaturated (`f ≤ 0e-8`) or has `add_formula`: no row, but `delta1[i] = -1.0` (1368-1371). `dissolve_only` not yet satisfied: additional constructed row −δᵢ ≤ initial − moles (1389-1401), emitted also for absent supersaturated `dissolve_only` phases.

MH2O inequality, only `if (pitzer_model || sit_model)` (1408-1434): copy of the MH2O `my_array` row (1414-1416), then columns of types `< PP` set to 0 (1418-1423), RHS `= 0.5 * x[i]->moles` (1429-1430). Note the MH2O row is *also* copied into the C block (it is not in the 1275-1280 exclusion list), so under Pitzer/SIT the oxygen unknown carries both an equality and an inequality row.

Gas inequalities, `if (gas_in == TRUE)` (1570-1595): for the contiguous GAS_MOLES unknowns from `gas_unknown->number`, constructed row with coefficient `-1.0` at column i (1584) and RHS `x[i]->moles` (1585-1586), i.e. −δᵢ ≤ molesᵢ. Solid-solution inequalities, `if (ss_unknown != NULL)` (1611-1638): for contiguous SS_MOLES unknowns, if `x[i]->phase->in == TRUE && x[i]->ss_in == TRUE` a constructed row with coefficient `1.0` (1624) and RHS `0.99 * x[i]->moles - MIN_TOTAL_SS` (1625-1626), i.e. δᵢ ≤ 0.99·moles − MIN_TOTAL_SS; otherwise the whole column is zeroed (1632-1635). MB inequalities under `negative_concentrations` (1642-1661): for MB unknowns with `x[i]->moles < 0.0`, copy of the MB `my_array` row (1648-1650) with columns of types `< PP` set to 0 (1652-1658); the RHS is the copied row's RHS.

Statement of fact: the design's §2 table is reproduced by the code with three corrections. (1) A rows are not only "present or supersaturated" pure phases: an absent, undersaturated PP also gets an A row while `iterations < x[i]->iteration + equi_delay` (the skip at 1164-1166 requires the delay condition), and a PP with nonempty `add_formula` always gets an A row. (2) `res[row] = 1` for PP rows only when `add_formula` is empty and `dissolve_only == FALSE` (1184-1187); ALK and SOLUTION_PHASE_BOUNDARY A rows keep `res = 0`. (3) The C-block skip of SURFACE_CB balances at 1298-1306 is unconditional on `add_formula`, while the matching column zeroing is not (C4). The `in_kode != 1` reset of residual flags and the `pp_scale` scaling of PP A rows are exactly as described.

## C3. Variable sign restrictions (writes to delta1 before the cl1 call)

Complete list of writes to `delta1` inside `ineq()` (grep over model.cpp:928-1925):

```c
1133:	delta1.resize(max_column_count);
1134:	memset(&delta1[0], 0,max_column_count * sizeof(double));
1370:					delta1[i] = -1.0;
```

Statement of fact: `delta1` is cleared to 0 at 1134 and the only value ever written before the `cl1()` call is `-1.0` at 1370, for a PP unknown with `phase->in == TRUE`, `moles <= 0.0`, and not (undersaturated `f > 0e-8` with empty `add_formula`) — i.e. absent supersaturated phases and absent related (`add_formula`) phases. `+1` is never set by `ineq()`, although `cl1()` supports it (cl1.cpp:117-125); the design's §5 check of `x_j ≥ 0 where delta1[j] = +1` is dead for problems built by `ineq()` and can be kept only as a defensive check.

## C4. Column exclusions (loops zeroing a whole column of ineq_array)

1. PP, model.cpp:1443-1473, guarded by `pure_phase_unknown != NULL`:

```c
1451:				if ((x[i]->moles <= 0.0 && x[i]->f > 0e-8 &&
1452:					 comp_ptr->Get_add_formula().size() == 0)
1453:					|| x[i]->phase->in == FALSE)
1455:					for (j = 0; j < l_count_rows; j++)
1457:						ineq_array[(size_t)j * max_column_count + i] = 0.0;
1460:				if (x[i]->dissolve_only == TRUE)
1462:					if (x[i]->f < 0e-8
1463:						&& (x[i]->moles - comp_ptr->Get_initial_moles() >=
1464:							0))
1466:						for (j = 0; j < l_count_rows; j++)
1468:							ineq_array[(size_t)j * max_column_count + (size_t)i] = 0.0;
```

Side by side with the row conditions: A skip `phase->in == FALSE` (1152) = zero condition (1453); A skip absent+undersaturated (1164-1166) requires additionally `iterations >= x[i]->iteration + equi_delay`, which the zeroing (1451-1452) does not require; A skip `dissolve_only` satisfied (1170-1172) = zero condition (1462-1464); E skips (1359-1360, 1362-1363, 1372-1374) match the zeroing conditions. MISMATCH: an absent, undersaturated PP inside the `equi_delay` window has its column zeroed (1455-1458) but still has an A row (the 1164 skip fails). Also `force_equality` PPs are never column-zeroed by these loops, but their A row is skipped (1161) and a C row is present — balanced — unless they are absent and undersaturated with empty `add_formula`, in which case the column is zeroed (1451) while the C row (1282) remains: row without column.

2. EXCH, model.cpp:1478-1492:

```c
1478:	if (use.Get_exchange_ptr() != NULL
1479:		&& (use.Get_exchange_ptr()->Get_related_phases() ||
1480:		use.Get_exchange_ptr()->Get_related_rate()))
1484:			if (x[i]->type == EXCH && x[i]->moles <= 0)
1486:				for (j = 0; j < l_count_rows; j++)
1488:					ineq_array[(size_t)j * max_column_count + i] = 0.0;
```

C-row skip (1292): `x[i]->type == EXCH && x[i]->moles <= MIN_RELATED_SURFACE`, with no `related_phases/related_rate` condition. MISMATCH: for `0 < moles <= MIN_RELATED_SURFACE`, or `moles <= 0` without related phases/rate, the C row is skipped but the column is not zeroed (column without row).

3. SURFACE with related phases/rate, model.cpp:1496-1537: zeroing condition (1522-1529) `SURFACE && phase_unknown != NULL && phase_unknown->moles <= MIN_RELATED_SURFACE && add_formula empty`, or `SURFACE_CB* && x[i-1]->phase_unknown != NULL && grams <= MIN_RELATED_SURFACE && comp_ptr1 add_formula empty`. C skips: 1307-1310 (same as first, matches) and 1298-1306 (`grams <= MIN_RELATED_SURFACE`, unconditional). MISMATCH: `SURFACE_CB*` with `phase_unknown != NULL`, `grams <= MIN_RELATED_SURFACE` and nonempty `add_formula` → C row skipped, column not zeroed.

4. SURFACE without related phases, model.cpp:1541-1566: zeroing (1552-1558) `SURFACE && phase_unknown == NULL && moles <= MIN_RELATED_SURFACE`, or `SURFACE_CB* && x[i-1]->phase_unknown == NULL && grams <= MIN_RELATED_SURFACE`. Matches C skips 1294-1297 and 1298-1306 for the `phase_unknown == NULL` cases.

5. Gas not in, model.cpp:1596-1606: `else if (use.Get_gas_phase_ptr() != NULL && gas_in == FALSE)` zeroes column `gas_unknown->number` (1602-1605). A row requires `gas_in == TRUE` (1221); E rows require `gas_in == TRUE` (1570). Balanced (row and column both excluded).

6. SS not in, model.cpp:1630-1636: `else` of `x[i]->phase->in == TRUE && x[i]->ss_in == TRUE` zeroes the column. A row requires only `ss_in == TRUE` (1238). MISMATCH (harmless for squareness): `ss_in == TRUE` with `phase->in == FALSE` keeps both A row and column.

7. Mass of water, model.cpp:1666-1673: `if (mass_oxygen_unknown != NULL && mass_water_switch == TRUE)` zeroes column `k = mass_oxygen_unknown->number` for `j < l_count_rows + 1` (one row past the last built row; harmless). C-row skip 1287-1288 is the same condition. Balanced.

8. Pitzer/SIT, model.cpp:1010-1029 (before copying): for `AH2O` (and `PITZER_GAMMA`) when `full_pitzer == FALSE`, and `MH` when `pitzer_model && !pitzer_pe`, the whole `my_array` column (1019-1022) and the whole `my_array` row (1023-1026) are zeroed. Consequence: the AH2O C row is still copied (1322) but is entirely zero including its RHS, while its column is zero — row without column (see C9). PITZER_GAMMA gets no C row (1275-1278, 1337) and its columns are removed from `n` by SHRINK_ARRAY (C6): balanced. MH: row skipped (1284) and column zeroed: balanced.

Statement of fact: every design §2 exclusion exists in the code, but the pairing of column zeroing with row exclusion is not exact in four places: the `equi_delay` window (A row + zero column), `force_equality` absent+undersaturated PPs (C row + zero column), exchangers with negligible mass (no C row + live column), and SURFACE_CB with `phase_unknown != NULL`, negligible grams and nonempty `add_formula` (no C row + live column). The design's §5 step 1 (find J from nonzero columns) is therefore not equivalent to "the excluded unknowns" in those cases; they will typically make the system non-square and fall back to `cl1()`.

## C5. Scalings

normal[i], "Normalize column", model.cpp:1033-1119. `normal` is set to 1.0 (1033-1034). For types MB, ALK, EXCH, SURFACE, SURFACE_CB/CB1/CB2 (1040-1042; EXCH/SURFACE with `moles <= MIN_RELATED_SURFACE` are skipped at 1044-1046) the column max is found (1047-1060); for MH with `pitzer_model == FALSE || pitzer_pe == TRUE` (1076) similarly (1085-1103). Then:

```c
1106:		if (max > 0.0 && max < min_value)
1114:			for (j = 0; j < count_unknowns; j++)
1116:				my_array[(size_t)j * (count_unknowns + 1) + (size_t)i] *= min_value / max;
1118:			normal[i] = min_value / max;
```

Applied to `my_array` before copying; `normal[i]` is later used to unscale δ (1854). `min_value = 1e-10` (Phreeqc.cpp:1032).

MH diagonal modification, model.cpp:1076-1084:

```c
1080:			min = 1e-12;
1081:			min = MIN_TOTAL;
1082:			my_array[(size_t)x[i]->number * (count_unknowns + 1) + (size_t)x[i]->number] += min;
1083:			if (fabs(my_array[...diagonal...]) < min)
1084:				my_array[...diagonal...] = min;
```

Applied to `my_array`; `MIN_TOTAL = 1e-25` (Phreeqc.cpp:494). The `1e-12` at 1080 is immediately overwritten.

Zero-row repair, model.cpp:1069-1073:

```c
1069:			if (max == 0)
1071:				my_array[(size_t)i * (count_unknowns + 1) + (size_t)i] = 1e-5 * x[i]->moles;
1072:				max = fabs(1e-5 * x[i]->moles);
```

Applied to `my_array`, for MB/ALK/SURFACE_CB-type unknowns whose column max is 0 (not for EXCH/SURFACE with negligible mass, which `continue`d at 1044-1046). Note: with `moles != 0` this makes an otherwise all-zero column nonzero, a false positive for the §5 step-1 column detection.

pp_scale, model.cpp:1192-1198 (quoted in C2): multiplies the copied PP A row in `ineq_array` (all `count_unknowns + 1` entries, RHS included). Applied to `ineq_array` only, never to `my_array`. Set only by the user: default `pp_scale = 1.0` (Phreeqc.cpp:861), KNOBS option 4 (read.cpp:7951-7952).

pp_column_scale, model.cpp:1677-1689:

```c
1679:		if ((x[i]->type == PP || x[i]->type == SS_MOLES) && x[i]->phase->in == TRUE
1680:			&& pp_column_scale != 1.0)
1682:			for (j = 0; j < l_count_rows; j++)
1684:				ineq_array[(size_t)j * max_column_count + (size_t)i] *= pp_column_scale;
1686:			normal[i] = pp_column_scale;
```

Applied to `ineq_array` columns (all built rows, including already-zeroed columns) after all row construction, and overwrites `normal[i]` (does not multiply). Set to `1e-10` by the kinetics retry attempts j = 6 and j = 7 (kinetics.cpp:1275, 1285; skipped for Pitzer/SIT), restored afterwards (kinetics.cpp:1430); default 1.0 (Phreeqc.cpp:862). After `cl1()`, `my_array` columns are unscaled by `normal` (model.cpp:1857-1866); `ineq_array` is not.

Statement of fact: `normal[]` scaling and the MH/zero-row modifications act on `my_array` before copying; `pp_scale` acts on PP rows of `ineq_array` at copy time; `pp_column_scale` acts on PP/SS columns of `ineq_array` after construction and overwrites `normal[i]`. A hook placed at the `cl1()` call sees all of them already applied.

## C6. Dimensions and layout

model.cpp:1125-1134:

```c
1125:	max_row_count = 2 * count_unknowns + 2;
1126:	max_column_count = count_unknowns + 2;
1127:	ineq_array.resize(max_row_count * max_column_count);
1128:	back_eq.resize(max_row_count);
1129:	zero.resize(max_row_count);
1131:	res.resize(max_row_count);
1133:	delta1.resize(max_column_count);
```

model.cpp:1699-1736:

```c
1699:	k = l_count_optimize;			/* rows in A */
1700:	l = count_equal;			/* rows in C */
1701:	m = l_count_rows - l - k;		/* rows in E */
1702:	if (m < 0)
1703:		m = 0;
1709: #define SHRINK_ARRAY
1711:	if ((sit_model || pitzer_model) && full_pitzer == FALSE)
1713:		n = (int)count_unknowns - (int)s_list.size();
1714:		for (int i = 0; i < l_count_rows; i++)
1716:			for (int j = 0; j < n; j++)
1718:				ineq_array[(size_t)i*((size_t)n+2) + (size_t)j] = ineq_array[(size_t)i*(count_unknowns+2) + (size_t)j];
1724:			ineq_array[(size_t)i*((size_t)n+2) + (size_t)n] = ineq_array[(size_t)i*(count_unknowns+2) + count_unknowns];
1727:	else
1729:		n = (int)count_unknowns;			/* columns in A, C, E */
1734:	l_klmd = (int)max_row_count - 2;
1735:	l_nklmd = n + l_klmd;
1736:	l_n2d = n + 2;
```

and `l_iter = 2*(n + l_count_rows);` (1747). `SHRINK_ARRAY` is `#define`d unconditionally at 1709, so the shrunk branch is live for `(sit_model || pitzer_model) && full_pitzer == FALSE`. `s_list` is the Pitzer/SIT present-species index list, built in `pitzer_make_lists` (pitzer.cpp:2624-2660) and the SIT equivalent (sit.cpp:1641-1676); the trailing unknown columns are the PITZER_GAMMA unknowns created last (prep.cpp:1267-1287, "For Pitzer model add lg unknown for each aqueous species"). The compaction keeps columns 0..n−1 and moves the RHS from column `count_unknowns` to column `n`; the last `s_list.size()` columns are dropped. Whether `s_list.size()` always equals the number of PITZER_GAMMA unknowns is not proven inside `ineq()` (both exclude EX/SURF species; `s_list` additionally includes `ICON == IC`), AMBIGUOUS, but irrelevant to the layout facts.

Statement of fact: normal layout — row stride `max_column_count = count_unknowns + 2`, RHS in column `count_unknowns`, rows 0..k+l+m−1 in order A, C, E. Shrunk layout (Pitzer/SIT, `full_pitzer == FALSE`) — same buffer, row stride `n + 2` with `n = count_unknowns − s_list.size()`, RHS in column `n`. `l_klmd = 2*count_unknowns`, `l_nklmd = n + l_klmd`, `l_n2d = n + 2`, satisfying the cl1.cpp:75-78 minimums. The design §5 first sentence ("row stride `max_column_count`, or `n + 2` under `SHRINK_ARRAY`") is confirmed.

## C7. After the cl1() call (model.cpp:1779-1925)

```c
1779:	cl1(k, l, m, n, l_nklmd, l_n2d, &ineq_array[0],
1780:		&l_kode, ineq_tol, &l_iter, &delta1[0], &res[0],
1781:		&l_error, &cu[0], &iu[0], &is[0], FALSE);
1782:/*   Set return_kode */
1783:	if (l_kode == 1)
1785:		return_code = ERROR;
1787:	else if (l_kode == 2)
1789:		return_code = 2;
1791:	else if (l_kode == 3)
1793:		return_code = ERROR;
1794:		return_code = 2;
1795:		warning_msg("Too many iterations in Cl1. Should not have done this.");
1797:	else
1799:		return_code = OK;
```

The `#ifdef SLNQ` block (1801-1840) is inactive: `SLNQ` is not defined anywhere in the tagged tree (`git grep "define SLNQ"` finds nothing; `CMakeLists.txt` has no `SLNQ`), only `#ifdef SLNQ` guards at Phreeqc.h:470 and model.cpp:951, 1755, 1801. Then:

```c
1844:/*   Copy delta1 into delta and scale */
1845: #ifdef SHRINK_ARRAY
1848:	memset(&(delta[0]), 0,
1849:		count_unknowns * sizeof(LDBLE));
1850: #endif
1851:	memcpy((void *) &(delta[0]), (void *) &(delta1[0]),
1852:		   (size_t) n * sizeof(LDBLE));
1853:	for (i = 0; i < n; i++)
1854:		delta[i] *= normal[i];
1856:/*   Rescale columns of array */
1862:			my_array[(size_t)j * (count_unknowns + 1) + (size_t)i] /= normal[i];
1869:	/* adjust unknowns where cl1 failed */
1870:	if (l_kode != 0)
1872:		for (i = 0; i < l_count_rows; i++)
1878:			j = back_eq[i];
1879:			if (x[j]->type == MB && delta[j] == 0.0 && fabs(res[i]) > ineq_tol)
1881:				delta[j] = res[i]/fabs(res[i]) * 1;
```

`res` is read at 1879-1881 only when `l_kode != 0`, to force a ±1 MB delta when cl1 failed; and in the `debug_model` dump (1911-1916, output only). `l_kode` is read at 1783-1800 (return code), 1870 (MB adjustment). `l_iter` and `l_error` are read only in the `debug_model` dump (1892-1893). `delta1` is read only at 1851-1854 (copied to `delta`, unscaled by `normal`; under `SHRINK_ARRAY` `delta` is first zeroed, so entries beyond `n` stay 0). `ineq()` returns `return_code` (1924).

Statement of fact: the design §2 sentence "After `cl1()`: δ = x · normal, and `res[]` is used only if `kode ≠ 0`" is confirmed (the only other read of `res` is the debug dump). `l_iter` and `l_error` are never used functionally. `SLNQ` is not defined in the build.

## C8. The caller (model())

model.cpp:104 (`l_kode = 1;` before the reaction loop) and 155-176:

```c
155:			if (r == OK || remove_unstable_phases == TRUE)
156:			{
157:				return_kode = ineq(l_kode);
158:				if (return_kode != OK)
159:				{
166:					log_msg(sformatf("Ineq had infeasible solution, "
167:							   "kode %d, iteration %d\n", return_kode,
168:							   iterations));
169:					count_infeasible++;
170:				}
172:				if (return_kode == 2)
173:				{
174:					ineq(0);
175:				}
176:				reset();
177:			}
```

`l_kode` (model.cpp:48) is set to 1 at 104 and never changed before 157, so `ineq()` is always entered with `in_kode = 1`; the retry at 174 passes `in_kode = 0`. The `in_kode == 2` branch inside `ineq()` (1743-1746) is unreachable from these call sites. Identical call/retry pairs exist in `model_pz` (pitzer.cpp:2306, 2323) and `model_sit` (sit.cpp:1295, 1312). A non-OK return only logs and counts (`count_infeasible`); `return_code == 2` triggers the immediate `ineq(0)` retry, which rebuilds the problem with `res` flags cleared (C2) and overwrites `delta`; `reset()` then runs in both paths. The Task 0 report's "lines near 161 and 178" correspond to 157 and 174 at this tag.

## C9. Squareness: when k + l differs from the number of live columns

Normally each unknown contributes exactly one A or C row and one live column (PP → A or C via `force_equality`; ALK/SPB/GAS_MOLES/SS_MOLES → A; MB/CB/MU/AH2O/MH/MH2O/EXCH/SURFACE*/PITZER_GAMMA → C), so k + l = |J|. From C2-C4, the deviations in the code are:

1. `iterations < aqueous_only`: all A rows skipped (1141-1142), columns live → k + l < |J|. CONFIRMED.
2. `force_equality` PP: A skipped (1161), C present (1282), column live → balanced; but absent+undersaturated with empty `add_formula` → C row kept while column zeroed (1451-1458) → k + l > |J|. CONFIRMED (asymmetric case).
3. `add_formula` PP: A row kept (skip requires empty `add_formula`, 1164-1166), column kept, `delta1 = -1` if absent (1370) → balanced. No squareness effect. CONFIRMED (no imbalance).
4. `dissolve_only`: satisfied → A skipped (1170-1174), E skipped (1372-1377), column zeroed (1460-1471) → balanced; not satisfied → A + E + column → balanced. No imbalance by itself.
5. EXCH/SURFACE negligible mass: C skipped at `moles <= MIN_RELATED_SURFACE` (1292) but column zeroed only at `moles <= 0` and only with related phases/rate (1478-1491) → k + l < |J| for `0 < moles <= MIN_RELATED_SURFACE` or without related phases. SURFACE_CB with `phase_unknown != NULL`, `grams <= MIN_RELATED_SURFACE`, nonempty `add_formula` → same imbalance. CONFIRMED.
6. `gas_in == FALSE` with a gas phase present: column zeroed (1596-1606), no A/E rows → balanced. CONFIRMED (no imbalance).
7. `mass_water_switch`: mass-oxygen column zeroed (1666-1673), its C row skipped (1287-1288) → balanced; the MH C row is modified (1326-1334) but still present. No imbalance.
8. Pitzer/SIT `full_pitzer == FALSE`: AH2O `my_array` row and column zeroed (1014, 1021-1026) but the all-zero C row is still copied (1322) → k + l > |J| (a zero row counts in l, the column does not count in J); MH (pitzer, no pe) balanced (row skipped 1284, column zeroed); PITZER_GAMMA rows absent and columns removed from n (1713) → balanced. Net: not_square. CONFIRMED.
9. `equi_delay` window: absent+undersaturated PP with `iterations < x[i]->iteration + equi_delay` keeps its A row (1164-1166 skip fails) while its column is zeroed (1451-1458) → k + l > |J|. Not in the design's example list; found in the code.

Statement of fact: k + l ≠ |J| occurs for `iterations < aqueous_only`, `force_equality` absent+undersaturated PPs, negligible-mass exchangers/surface charges, Pitzer/SIT with `full_pitzer == FALSE` (AH2O zero row), and inside the `equi_delay` window. The design's H3 exclusion of Pitzer/SIT is consistent with item 8.

## C10. The hook location

Ten lines before and after the `cl1(` call, model.cpp:1769-1791:

```c
1769:	slnq_delta1 =
1770:		(LDBLE *) PHRQ_malloc((size_t) max_column_count * sizeof(LDBLE));
1771:	if (slnq_delta1 == NULL)
1772:		malloc_error();
1773:	memcpy((void *) &(slnq_delta1[0]), (void *) &(zero[0]),
1774:		   (size_t) max_column_count * sizeof(LDBLE));
1775: #endif
1776: /*
1777:  *   Call CL1
1778:  */
1779:	cl1(k, l, m, n, l_nklmd, l_n2d, &ineq_array[0],
1780:		&l_kode, ineq_tol, &l_iter, &delta1[0], &res[0],
1781:		&l_error, &cu[0], &iu[0], &is[0], FALSE);
1782: /*   Set return_kode */
1783:	if (l_kode == 1)
1784:	{
1785:		return_code = ERROR;
1786:	}
1787:	else if (l_kode == 2)
1788:	{
1789:		return_code = 2;
1790:	}
1791:	else if (l_kode == 3)
```

Between the end of the E-block construction (1661) and the call, `ineq_array` IS modified, three times: the mass-water column zeroing (1666-1673), the `pp_column_scale` column scaling (1677-1689), and the `SHRINK_ARRAY` compaction (1710-1733). `normal[]` is also modified at 1686. `delta1` and `res` are not modified after 1370 and 1247 respectively. The `#ifdef SLNQ` block (1755-1777) copies `ineq_array` but is inactive.

Statement of fact: a hook placed immediately before line 1779 sees the final `ineq_array` (all row, column, scaling and layout modifications applied), the final `delta1` sign flags, the `res` entry flags, and the final `k, l, m, n, l_nklmd, l_n2d` — exactly what `cl1()` sees. This is the H1 position, and it is the only safe position: any earlier point misses the three modifications listed above.

## Answers to the design's [VERIFY] items

- §2 line 24, "`ineq()` always calls `cl1()` with `kode = 1`": CONFIRMED. `l_kode = 1;` unconditionally at model.cpp:1741; the `in_kode == 2` branch (1743-1746) is redundant; `cl1()` reads X/RES flags under `if (*l_kode != 0)` (cl1.cpp:244).
- §2 table, A block ("one row for each pure phase that is present or supersaturated; ALK; SPB; GAS_MOLES if in; SS_MOLES if in; res[i]=1 for pure-phase/gas/SS; reset when in_kode != 1; pp_scale"): CONFIRMED with corrections — absent+undersaturated PPs also get A rows inside the `equi_delay` window or with nonempty `add_formula` (model.cpp:1164-1169); `res = 1` for PP rows only when `add_formula` empty and not `dissolve_only` (1184-1187); ALK/SPB rows have `res = 0`; `pp_scale` applies to PP rows only (1192-1198).
- §2 table, C block: CONFIRMED with additions — the listed types and skips are exact, plus: MH skipped for Pitzer without pe (1284), mass-oxygen skipped under `mass_water_switch` (1287) with the 2× subtraction on the MH row (1326-1334), and the SURFACE_CB skip at 1298-1306 is unconditional on `add_formula` while the paired column zeroing is not.
- §2 table, E block (δᵢ ≤ molesᵢ; −δᵢ ≤ initial − moles; −δᵢ ≤ molesᵢ for gas; δᵢ ≤ 0.99·moles − MIN_TOTAL_SS; MH2O row with Pitzer/SIT; MB rows with negative_concentrations): CONFIRMED (model.cpp:1382-1386, 1395-1398, 1582-1586, 1622-1626, 1408-1432 with RHS 0.5·moles and aqueous columns zeroed, 1642-1661 likewise).
- §2 table, variable signs ("`delta1[j] = −1` for a pure phase with moles ≤ 0 that is in the model and not excluded; otherwise 0"): CONFIRMED — the only writes are the memset (1134) and `delta1[i] = -1.0` (1370); the exact condition is `phase->in == TRUE && moles <= 0 && !(f > 0e-8 && add_formula empty)`. `+1` is never written by `ineq()`.
- §2 table, excluded unknowns (column zeroed): CONFIRMED with mismatches — all listed zeroings exist (1451-1471, 1478-1492, 1496-1566, 1596-1606, 1630-1636, 1666-1673), but the pairing with row exclusion is inexact in four cases (C4 items 1-3) and Pitzer/SIT zeroes AH2O rows+columns in `my_array` while an all-zero C row is still copied (C9 item 8).
- §2 line 34 (scalings, SHRINK layout, "After cl1(): δ = x · normal, and res[] is used only if kode ≠ 0"): CONFIRMED (model.cpp:1851-1854; `res` read only under `l_kode != 0` at 1870-1882, plus the debug dump; `SLNQ` not defined).
- §3 line 48 ("on the excluded columns `cl1()` returns 0" — property of the simplex on all-zero columns): DEFERRED TO TASK 1, per the reviewer's clarification. What the code shows: at exit `cl1()` zeroes all of X (cl1.cpp:700-703, "L590: for (j = 0; j < n; ++j) l_x[j] = 0.;") and assigns values only to variables whose label appears in the basis column `q2[i*q_dim + n1]` (lines 710-723); a column can enter the basis only through a pivot whose magnitude exceeds `l_toler` (lines 415, 440-448; TOLER documentation 105-112), and an entering column with no such entry sets `*l_kode = 2` (461-465). Whether an all-zero column can nevertheless be selected at L240 (lines 344-374) through the label/cost bookkeeping is not settled by inspection; empirical check in Task 1.
- §5 line 77 (row stride `max_column_count`, or `n + 2` under `SHRINK_ARRAY`): CONFIRMED (model.cpp:1718, 1724; C6).
- §5 line 79 (J = nonzero columns of the final ineq_array correctly identifies the excluded unknowns): DEFERRED TO TASK 1, as the design itself states; the code shows the four C4/C9 mismatches and the `1e-5 * x[i]->moles` zero-row repair (1069-1073) as sources of columns that are live without a row or zeroed with a row.

## Reviewer decisions required

1. C4/C9 mismatches (equi_delay window; absent+undersaturated `force_equality` PPs; negligible-mass exchangers; SURFACE_CB with `phase_unknown != NULL`, negligible grams and nonempty `add_formula`; AH2O all-zero C row under Pitzer/SIT): confirm that these are to be handled solely by the `not_square` fallback (design §5 step 2), not by special-casing the unknown types.
2. The `delta1[j] = +1` feasibility check in design §5 step 5 is unreachable for problems built by `ineq()` (C3). Keep it as a defensive check, or drop it?
3. The zero-row repair `my_array[i][i] = 1e-5 * x[i]->moles` (model.cpp:1071) makes otherwise-zero MB columns live. Confirm that J is to be read from the final `ineq_array` as designed (§5 step 1), accepting the extra live columns and any resulting `not_square`/`ill_conditioned` fallback.
4. C10 confirms the H1 hook position is the only one that sees the final problem (three `ineq_array` modifications lie between the E block and the call). Confirm H1 as the single guarded hook inside `ineq_cl1_original` (design §9 decision 1).
5. The `in_kode == 2` branch of `ineq()` (model.cpp:1743-1746) is unreachable from all call sites (C8). H3's exclusion "in_kode ≠ 1" therefore covers only the `ineq(0)` retry; confirm that scope.
