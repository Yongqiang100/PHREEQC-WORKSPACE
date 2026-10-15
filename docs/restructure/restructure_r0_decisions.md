# Restructuring R0: Reviewer Decisions

These decisions were written by the reviewer, not by an agent. They resolve `restructure_r0_audit_report.md`.

**RS-D1. Status: the restructuring is paused after R0.** The drop-in overlay (`docs/overlay/overlay_design.md`) meets the goal of sharing the integration, with far less change. R0's results are kept as the reference for any later restructuring, and the overlay uses them directly (RS-D3).

**RS-D2. Facts established (accepted).**
- **Identical copies.** The 163 PHREEQC engine files in PhreeqcRM's `phreeqcpp/` are byte-identical to phreeqc3 `src/` at `b0b3be76`, including `common/` and `PhreeqcKeywords/`. All 70 database files are identical too. `Tests/phreeqc.dat` is a separate, whitespace-reflowed test input; it stays a test input and is not reconciled.
- **No include text needs to change.** The 63 cross-layer includes are all resolved through `-I` directories, and no included name is ambiguous. The two `../../` includes are dead (guarded by `PHREEQCI_GUI`, defined nowhere).
- **The compile definitions are those measured in U8, which supersedes design §5.**
  - phreeqc3 compiles the engine with `-DNDEBUG` only.
  - PhreeqcRM compiles all of its library sources with `IPhreeqc_EXPORTS SWIG_SHARED_OBJ USE_PHRQ_ALLOC`, plus `USE_OPENMP` or `USE_MPI`.
  - `USE_YAML`, `USE_GZ` and `NO_UTF8_ENCODING` are not set in these configurations.
- **Class-layout macros.** `USE_MPI` changes `class PhreeqcRM` and `MP_TYPE`. `SWIG_SHARED_OBJ` gates `thread.h`, whose globals are defined or declared depending on other macros. A future split must therefore keep each file's exact definition set, and compile the one engine source twice (once per configuration) rather than share objects between the phreeqc and PhreeqcRM builds.
- **Provenance.** phreeqc3's only remote is `phreeqc-dev/phreeqc3`, not `usgs-coupled/phreeqc3`. Because PhreeqcRM's official v3.9.0 carries a byte-identical engine, the engine at `b0b3be76` is at least the official v3.9.0 engine. The reviewer still confirms against the official repository (RS-D4).

**RS-D3. Consequences for the overlay.**
- **Hashes.** The upstream files the overlay edits are byte-identical in both official trees, so one set of SHA-256 values and one set of anchors serves phreeqc3 and PhreeqcRM alike.
- **Definitions.** The overlay attaches to the existing target, so it inherits each tree's own definition set unchanged. It must add only its own PRIVATE definitions, never alter the upstream ones.
- **Headers.** The overlay's headers must not depend on `USE_MPI`, `USE_OPENMP` or `SWIG_SHARED_OBJ`. The adapter is already built that way (no MPI calls; P2-D10).

**RS-D4. Provenance check (the reviewer runs this once):**
```
git -C ~/phreeqc3 remote add usgs https://github.com/usgs-coupled/phreeqc3.git
git -C ~/phreeqc3 fetch usgs --tags
git -C ~/phreeqc3 branch -r --contains b0b3be76
```
If the commit is not on a `usgs/` branch, compare the trees directly against the official v3.9.0 release tag instead.

**RS-D5. The 12 AMBIGUOUS mappings and the U4 judgment calls are deferred.** They matter only if the restructuring resumes.
