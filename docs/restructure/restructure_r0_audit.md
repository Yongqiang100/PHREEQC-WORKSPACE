# Restructuring Task R0: Read-Only Audit for the Flat Layout

## Context

The restructuring design (`docs/restructure/restructure_design.md`) flattens PhreeqcRM's nested layers into side-by-side libraries, keeps a single copy of PHREEQC, and stays able to import USGS releases. Before any file moves, this audit establishes the facts the move depends on: the file inventory, the duplicated files, the mapping, include paths, compile definitions, and build-script references.

**Upstream trees at v3.9.0, as prepared by the reviewer:**
- phreeqc3: `~/upstream/phreeqc3_v390`, a worktree of `~/phreeqc3` at commit `b0b3be76`;
- PhreeqcRM: `~/phreeqcrm_baseline_src`, the existing worktree at tag `v3.9.0`.

**This task modifies no repository.** It writes only:
- its report, `docs/restructure/restructure_r0_audit_report.md` in `~/phreeqc3`;
- data files under `~/phreeqc3/build/restructure/`;
- new configure-only build trees under `~/phreeqc3/build/restructure/cc_*`.

## Rules

1. Read-only for all sources. Never run `git commit`, `checkout`, `apply`, `reset`, `stash`, `clean`, or `mv`. Do not create or remove worktrees.
2. Every claim gives a file path (and line numbers for code), or the command and its output. Write NOT FOUND or AMBIGUOUS rather than guessing.
3. Put large outputs into files under `~/phreeqc3/build/restructure/`, and quote counts and representative lines in the report.
4. Use Plan mode first, present the plan, and execute only after the reviewer replies `APPROVED`. Keep your reasoning short before each tool call. Write the report in parts of at most about 150 lines per edit.

Throughout, `P3=~/upstream/phreeqc3_v390` and `RM=~/phreeqcrm_baseline_src`.

## Items

**U1. Origins.** Report the following, plus `git -C $P3 rev-parse HEAD` and `git -C $RM describe --tags`:
- `git -C ~/phreeqc3 remote -v` and `git -C ~/phreeqcrm remote -v`;
- `git -C ~/phreeqc3 log --oneline -1 b0b3be76`;
- whether the official USGS repositories (`https://github.com/usgs-coupled/phreeqc3`, `https://github.com/usgs-coupled/phreeqcrm`) are the remotes, or something else.

**U2. Inventory.** Write `git -C $P3 ls-files > build/restructure/inventory_phreeqc3.txt`, and the same for `$RM` (`inventory_phreeqcrm.txt`). Report the file counts per top-level directory, and per directory under `src/`.

**U3. The duplicated PHREEQC.** For every file under `$RM/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/` (recursively), `cmp` it with the same relative path under `$P3/src/`. Report the counts of identical files, differing files, files only in PhreeqcRM, and files only in phreeqc3, with the full lists of the last three. Do the same for `database/` (`$RM/database` against `$P3/database`), and for `$RM/Tests/phreeqc.dat` against `$P3/database/phreeqc.dat`.

**U4. Mapping proposal.** Write `build/restructure/mapping_proposal.tsv`, with one row per tracked file of **both** trees: `repository<TAB>upstream_path<TAB>destination<TAB>reason`.
- `destination` follows design §2: `src/phreeqc_common/`, `src/phreeqc/`, `src/phreeqc/keywords/`, `src/phreeqc/app/`, `src/iphreeqc/`, `src/iphreeqcphast/`, `src/phreeqcrm/`, `src/interfaces/`, `python/`, `tests/phreeqc/`, `tests/phreeqcrm/`, `data/databases/`, `docs/upstream/<repository>/`, or `NOT_CARRIED`.
- For the PHREEQC engine, the phreeqc3 file is the source and the PhreeqcRM duplicate is `DUPLICATE_OF phreeqc3:<path>`.
- `NOT_CARRIED` needs a reason, for example autotools, Visual Studio, Jenkins, or the installer.

Report the counts per destination, every `NOT_CARRIED` file grouped by reason, and every file for which the destination is AMBIGUOUS.

**U5. Include paths.** Report every `#include` with a path component, or one that reaches another layer:
```
grep -rnE '#include *"[^"]*/' $P3/src $RM/src
grep -rnE '#include *"\.\.' $P3/src $RM/src
```
Then, for each `#include "X"` in `$RM/src` (all layers), find which directory contains X (`find $RM/src -name X`). Report every include whose file lives in a **different** layer directory than the including file. Summarise these as counts per (from layer, to layer) pair, and write the list to `build/restructure/cross_layer_includes.txt`.

**U6. Conditional compilation in headers.** For all `*.h`, `*.hxx` and `*.hpp` files in both trees, list every macro tested by `#if`, `#ifdef`, `#ifndef` or `defined(...)`, with its count and files. Write the list to `build/restructure/header_macros.txt`. For each macro, report where it is defined: CMake files (`grep -rn "<macro>" $P3/CMakeLists.txt $RM/CMakeLists.txt $RM/Tests/CMakeLists.txt`), headers, or nowhere. Flag every macro that both changes a class or struct definition (quote the lines) and is set differently between the phreeqc3 and PhreeqcRM builds.

**U7. Build-script references to source paths.** Quote every line that names a source path or directory in `$RM/CMakeLists.txt`, `$RM/Tests/CMakeLists.txt`, `$RM/CMakePresets.json`, `$RM/swig/*`, `$RM/pyproject.toml`, `$RM/Doxygen/*` (configuration files only), `$RM/*.cmake.in`, `$RM/setup-fortran/*` and `$P3/CMakeLists.txt`. Report counts per file in the report, with the full list in `build/restructure/build_path_references.txt`.

**U8. Compile commands (configure only).** Configure three new trees **without building**:
```
cmake -S $P3 -B ~/phreeqc3/build/restructure/cc_phreeqc3 -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake -S $RM -B ~/phreeqc3/build/restructure/cc_phreeqcrm_openmp -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake -S $RM -B ~/phreeqc3/build/restructure/cc_phreeqcrm_mpi -DCMAKE_BUILD_TYPE=Release -DPHREEQCRM_BUILD_MPI=ON -DMPIEXEC_MAX_NUMPROCS=4 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```
For each `compile_commands.json`:
- report the number of entries;
- report the distinct sets of `-D` definitions, with how many files use each set;
- report the distinct sets of `-I` include directories, normalised by replacing each tree root with `<ROOT>`;
- report the compiler flags other than `-D`, `-I` and `-o`.

Write a per-file table (`file<TAB>definitions<TAB>includes`) to `build/restructure/compile_flags_<tree>.tsv`.

**U9. Tests.** Report `ctest -N` in `~/phreeqcrm_baseline_src/build-openmp` and `build-mpi` (these exist from Task 2a), and in `~/phreeqc3/build/baseline` (if present, otherwise NOT FOUND). Also list the test inputs under `$RM/Tests/` and `$P3/examples/`.

## Output

Write `docs/restructure/restructure_r0_audit_report.md` with items U1–U9: facts, counts, quotes, and the paths of the data files. End with **"Reviewer decisions required"**, listing:
- every AMBIGUOUS mapping;
- every PHREEQC file that differs between the two trees (U3);
- every cross-layer include that needs a path change (U5);
- every macro flagged in U6;
- anything NOT FOUND.

Stop after the report.
