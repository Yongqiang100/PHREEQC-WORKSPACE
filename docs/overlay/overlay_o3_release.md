# Overlay Task O3: Release 1.0.0 (default policy, documentation, tarball, consumer test)

## Context

O2 validated the overlay on fresh official clones (OV-D7 (a)). O3 makes it releasable:
1. it sets the default backend to `cl1` (OV-D7 (b)), with a CMake option to choose `lapack` instead;
2. it installs the reviewer's documentation;
3. it re-validates the defaults;
4. it builds the tarball, and tests it the way a new user would.

## Rules

1. Edit only inside `~/phreeqc3/overlay/phreeqc_newton_solver/`, and write runs and builds only under `~/overlay_o3/`. Change nothing else.
2. Code changes are limited to those in step 1. List them all.
3. Run long commands in the background (`nohup … &`), poll them, and write `~/overlay_o3/progress.md` before each long step. Keep your reasoning short.
4. Use Plan mode first, present the plan, and execute only after the reviewer replies `APPROVED`. Do not run `git commit`. The report is given in your final reply. If a test fails, stop and report; do not work around it.

## Steps

**1. The default-backend option.**
- **`cmake/attach.cmake`:** add `set(NEWTON_SOLVER_DEFAULT_BACKEND "cl1" CACHE STRING "Default Newton linear-solver backend: cl1 or lapack")`, validated as exactly `cl1` or `lapack`, with `FATAL_ERROR` otherwise. When it is `cl1`, add the PRIVATE definition `NEWTON_SOLVER_DEFAULT_BACKEND_CL1` to the target **and** to the four test executables.
- **`src/core/newton_linear_solver_options.cpp`:** in `default_backend_of_this_build()`, add a first branch `#if defined(NEWTON_SOLVER_DEFAULT_BACKEND_CL1)` that returns `NEWTON_BACKEND_CL1`. The existing branches follow as `#elif`.
- **`tests/test_newton_linear_solver_options.cpp`:** make the expected default backend and its validity depend on that macro, with `#ifdef`. Nothing else changes.

**2. Documentation.**
- Copy `~/phreeqc3/overlay_docs/README.md` over the folder's `README.md`, and `~/phreeqc3/overlay_docs/docs/RESULTS.md` and `VALIDATION.md` into a new `docs/` subfolder, all verbatim (`cmp` afterwards).
- Check that `LICENSE` exists in the folder. If it does not, stop and report.

**3. Re-validation,** in `~/overlay_o3/`. Fresh official clones: `git clone --branch v3.9.0` of `usgs-coupled/phreeqc3` and of `usgs-coupled/phreeqcrm`. Copy the folder in, plus the inputs, as in O2 S0.
- **(a) Default build (`cl1` default), no environment variables:**
  - phreeqc3: ex1 and ex2 `.sel` `cmp`-identical to the baseline; `.out` `IDENTICAL`.
  - With `PHREEQC_LINEAR_SOLVER_STATISTICS=1`: the statistics line shows `backend=cl1 solves=0`.
  - PhreeqcRM OpenMP `TestRM`, against `openmp_run1` in **exact** P2-D16 mode: every non-`.dmp` file `IDENTICAL`, including `Advect_cpp_units_utility.txt`.
  - `ctest -R newton_solver`: 4 of 4 in both trees.
- **(b) Same builds with `PHREEQC_LINEAR_SOLVER=lapack`:**
  - phreeqc3 ex1 and ex2 statistics identical to O2's default-mode references (`~/overlay_o2/ref/ex*_default`);
  - PhreeqcRM `TestRM` summary sums 490/368/122/122.
- **(c) phreeqc3 build with `-DNEWTON_SOLVER_DEFAULT_BACKEND=lapack`, no variables:** ex1 statistics identical to (b); `ctest` 4 of 4.
- **(d) Invalid option:** configure with `-DNEWTON_SOLVER_DEFAULT_BACKEND=bogus` must fail with the option named in the message, and write nothing (the three engine files' `sha256sum` unchanged).
- **(e) Detach** both clones: `git diff --stat` empty.

**4. Package.**
- Remove any `__pycache__` from the folder.
- Run `tar -C ~/phreeqc3/overlay -czf ~/overlay_o3/phreeqc_newton_solver-1.0.0.tar.gz phreeqc_newton_solver`, then `sha256sum` it into `phreeqc_newton_solver-1.0.0.tar.gz.sha256`.
- Report `tar -tzf … | wc -l` and the top-level entries.

**5. Consumer test: exactly what README.md says.**
- In `~/overlay_o3/consumer/`, follow the README's *Installation* block literally for PhreeqcRM: clone, copy the extracted folder (from the tarball, not from `~/phreeqc3`), configure, build, `ctest`. Then do the same for phreeqc3, with `phreeqc3` in place of `phreeqcrm`.
- Run the README's *Removing it* command, then `git diff --stat`, which must be empty.
- Report each command and its exit status. Any step where the README is wrong or unclear is a finding: report it.

## Report (in your final reply)

Report:
- the code changes;
- pass or fail and the outputs for every step;
- the tarball path, size and SHA-256;
- every README finding;
- any deviation, with the reason for it.

Then stop.
