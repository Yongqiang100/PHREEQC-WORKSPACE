# Flat PhreeqcRM: Design (supersedes the paused restructuring for PhreeqcRM alone)

| Field | Value |
|---|---|
| Status | Revision 1 (2026-10-15) |
| Basis | R0 audit (`docs/restructure/restructure_r0_audit_report.md`), decisions RS-D1 to RS-D5 |
| Scope | PhreeqcRM only. phreeqc3 stays upstream (its engine is byte-identical to PhreeqcRM's copy, RS-D2) |

## Decisions

**FL-D1. Repository and branches.**
- The repository is `~/phreeqcrm-flat`.
- Branch **`upstream`** holds only the output of `tools/sync_upstream.py` for an official release, plus the tool, the root README and this document.
- Branch **`main`** is `upstream` plus the flat build (F2). Upstream updates arrive by `git merge upstream`.

**FL-D2. Mapping: `tools/sync_upstream.py`,** with ordered rules and a manifest.
- Layer directories are as in the README.
- **IPhreeqc's own Fortran interface stays in `src/iphreeqc/`,** where its layer is. `src/interfaces/` holds only PhreeqcRM's C and Fortran APIs.
- **Dropped** (with reasons, in `UPSTREAM_DROPPED.tsv`): CI, VCS metadata, editor files, autotools, Visual Studio and Jenkins.
- **Upstream build files** are kept read-only under `cmake/upstream/`, so that their changes in a new release are visible in `git diff`.
- **The R0 AMBIGUOUS rows are resolved:**
  - `LICENSE` stays at the root;
  - `README.md` and `DEVELOPER.md` go to `docs/upstream/`;
  - `CMakePresets.json` goes to `cmake/upstream/` (the flat presets are new, F2);
  - `environment.yml`, `mamba.environment.yml` and `uv.lock` go to `python/`;
  - `src/phreeqcrm/__init__.py` goes to `python/phreeqcrm/`;
  - `test.xml` stays with the engine.
- **No source file is edited.** R0 U5 found that no include needs a path change, because all cross-layer includes go through `-I`.

**FL-D3. The build (F2).**
- **Libraries:** one CMake `OBJECT` library per layer (`phreeqc_common`, `phreeqc_engine`, `iphreeqc`, `iphreeqcphast`, `phreeqcrm_core`, `phreeqcrm_c`, `phreeqcrm_fortran`), all linked into the single library `PhreeqcRM`. The installed package stays exactly `PhreeqcRM::PhreeqcRM`.
- **Settings:** all common compile settings (definitions, include directories, language standard, OpenMP or MPI) come from one INTERFACE target, so that **every source file receives exactly the definitions it receives upstream**. RS-D2 and R0 U8: `IPhreeqc_EXPORTS SWIG_SHARED_OBJ USE_PHRQ_ALLOC`, plus `USE_OPENMP` or `USE_MPI`.
- **Parity:** the build parity is proven by comparing per-file compile commands with R0's `compile_flags_cc_phreeqcrm_{openmp,mpi}.tsv`.

**FL-D4. Acceptance (F2):**
1. **Compile flags:** identical per file (paths mapped through the manifest).
2. **Installed files:** the same installed file set and identical installed headers.
3. **Tests:** `ctest` passes in OpenMP and MPI.
4. **`TestRM` outputs:** identical to the v3.9.0 baselines (`openmp_run1`, `Tests_np2`, `Tests_np4`) under P2-D16.
5. **External program:** the benchmark driver builds against the flat installation, with identical concentrations.
6. **The overlay** attaches to the flat tree (F3, needs a third tree-detection branch in `attach.cmake`).

## Results of F1, and a refinement of FL-D3

**FL-D5. F1 is accepted.**
- **Import:** `~/phreeqcrm-flat`, branch `upstream`, commit `7413787`: PhreeqcRM v3.9.0 (`90b31ed8`), 506 tracked files, 438 imported, 68 dropped with reasons. A second run is a no-op.
- **Engine check:** `diff -rq` of `src/phreeqc` against phreeqc3 v3.9.0 `src/` reports no differing file. The only names present on one side alone are the expected ones: `class_main.cpp` (here in `app/`), `keywords/` (upstream `PhreeqcKeywords/`), and the dropped `.gitlab-ci.yml` and `Makefile.am`.
- **Legacy files carried unchanged,** for fidelity, though nothing builds them: `src/phreeqc/Makefile*`, `phreeqc.Makefile`, `Form1.*`, `ZedGraph.dll`, `tests/autotools.mk`, `tests/pkgconfig.mk`.

**FL-D6. The build structure is simplified relative to FL-D3, to keep exact parity with the least risk.**
- There is **one** `PhreeqcRM` library target, as upstream.
- **Each layer directory carries a `sources.cmake`** that sets one list variable (`PHREEQCRM_SOURCES_PHREEQC_COMMON`, `…_PHREEQC`, `…_IPHREEQC`, `…_IPHREEQCPHAST`, `…_PHREEQCRM`, `…_INTERFACES_C`, `…_INTERFACES_FORTRAN`). The root `CMakeLists.txt` includes them all and builds the one target. Each layer's file list then sits next to its code.
- **All compile settings stay target-wide, exactly as upstream,** which makes per-file parity hold by construction.
- **Per-layer OBJECT libraries** remain a possible later step. They are not needed for understanding or for use.

**FL-D7. Results of F2, accepted.**
- **Compile commands:** the same 110 compiled files in the OpenMP and MPI trees, with identical `-D` sets and identical other flags per file. The include directories map to identical sets.
- **Tests:** `ctest` names and results identical (3 of 3 OpenMP; 4 MPI). `TestRM` outputs: 23 of 23 non-`.dmp` files identical to `openmp_run1`, and 0 failures against `Tests_np2` and `Tests_np4`.
- **Installation:** the installed file sets are identical (64 files). Headers are byte-identical, the config files identical, and the 95 archive members have identical names.
- **External program:** the benchmark driver builds against the flat installation, with identical concentrations.
- **Fortran:** builds and passes from the flat paths.
- **Python:** NOT RUN (no SWIG on hpc02); accepted. It is to be checked once SWIG is installed.
- **Imported build files changed (moved paths only):** `tests/CMakeLists.txt` (10 lines) and `python/swig/python/CMakeLists.txt` (7 lines). These are the expected merge points when a new upstream release is imported.
- **The README's nonexistent `release` preset is corrected,** on branch `upstream`, to the plain `cmake -S . -B build` form plus the real presets (`ninja-static-release`, `ninja-shared-release`).
- **Next, F3: the overlay supports the flat tree** (overlay 1.1.0).

**FL-D8. Results of F3 accepted; overlay 1.1.0 released; flat PhreeqcRM complete.**
- **Overlay 1.1.0** recognises three layouts: the flat tree (checked first), PhreeqcRM and phreeqc3. Only the five specified files changed.
- **On a clone of `phreeqcrm-flat`:**
  - 4 of 4 overlay tests pass;
  - the default `TestRM` is **23 of 23 identical** to upstream v3.9.0;
  - `lapack` reproduces 490/368/122/122;
  - the installed config contains the two dependency lines once each, before the targets include;
  - detach leaves `git diff` empty.
- **Regression:** both official layouts still pass (4 of 4 tests, empty `git diff` after detach).
- **Tarball:** `phreeqc_newton_solver-1.1.0.tar.gz`, 57,641 bytes, 43 entries, SHA-256 `6a63a6dddf27e47d6a4416e72412f06a5b28dfa3300bdf3b1e110bbfc2dfb016`.
- **Publication:** `phreeqcrm-flat` goes to its own GitHub repository, with both branches and its tags. `PHREEQC-WORKSPACE` gets the 1.1.0 overlay folder and a release `v1.1.0`.
