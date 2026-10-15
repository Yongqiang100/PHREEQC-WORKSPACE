# Task 1: Build Integration and Global PETSc Initialisation

## Context

PETSc is being integrated into PHREEQC's Newton linear solve, Phase 1A (serial, dense LU only). This task adds:

- the CMake option and a small adapter library;
- the module that initialises and finalises PETSc safely when PHREEQC may be one of several PETSc users in a process;
- unit tests for that module.

**No PHREEQC source file is modified in this task.**

**Required input:** `docs/petsc_linear_solver/task0_source_verification_report.md` and `docs/petsc_linear_solver/task0_reviewer_decisions.md`. The decisions file wins wherever the two differ. If either disagrees with this prompt, stop and ask.

Values established by Task 0 and used below:

| Item | Value | Source |
|---|---|---|
| PHREEQC source directory | `src` | report A2, B3 |
| Targets that compile PHREEQC core sources | only `phreeqc`, an executable; sources compiled directly into it | report A2, decision D2 |
| Existing `target_link_libraries` form on `phreeqc` | plain (no keyword), `CMakeLists.txt:300` | report A2, decision D2 |
| C++ standard | C++11, `CMakeLists.txt:263` | report A4 |
| PETSc | module `petsc/3.26.0`, `PETSC_DIR=/opt/petsc/3.26.0`, `PETSC_ARCH` empty | decision D1 |
| pkg-config module name | `PETSc` | report A3 |

## Rules (apply to every task)

1. **File scope.** Touch only the files listed under Deliverables. If another file seems to need a change, stop and ask.
2. **Check every PETSc name.** Before using any PETSc function, type, macro, or constant, grep `$PETSC_DIR/include` and `$PETSC_DIR/$PETSC_ARCH/include` for its declaration, and use the signature you find. If it is not declared, stop and report. Never substitute a similar-looking name.
3. **Language and error handling.** Code is C++.
   - Inside functions returning `PetscErrorCode`, use `PetscFunctionBeginUser` / `PetscCall(...)` / `PetscFunctionReturn(PETSC_SUCCESS)`.
   - Functions called from PHREEQC code never throw and never abort.
4. **Build isolation.** All PETSc code is compiled only when `PHREEQC_USE_PETSC=ON`. With the option OFF, the build must be identical to the unmodified tree.
5. **Test integrity.** Never change tolerances, baseline files, or test inputs to make a test pass. Report failures with numbers.
6. **No improvising.** If the code or environment contradicts this prompt, stop and report. Do not adapt the design yourself.
7. **Workflow.** Use Plan mode first, present the plan, and implement only after approval.
8. Do not run `git commit`.

## Deliverables

1. `src/petsc_linear_solver/petsc_initialization.h`
2. `src/petsc_linear_solver/petsc_initialization.cpp`
3. `tests/petsc_linear_solver/test_initialization_phreeqc_owns_petsc.cpp`, `tests/petsc_linear_solver/test_initialization_host_owns_petsc.cpp`, `tests/petsc_linear_solver/test_initialization_explicit_shutdown.cpp`
4. CMake changes in the top-level `CMakeLists.txt` only

## Specification

### Header: `petsc_initialization.h`

The header must not include any PETSc header. PHREEQC files will include it without needing PETSc include paths.

```cpp
#pragma once
#ifdef USE_PETSC
#include <mutex>

/* Returns 0 on success, -1 if PETSc is unusable (init failed, or already finalized).
   The first successful call initialises PETSc if nobody else has, and registers an
   atexit handler that finalizes it at process exit (only if PHREEQC initialised it). */
int  acquire_petsc(void);

/* Decrements the reference count. Never finalizes PETSc by itself. */
void release_petsc(void);

/* Finalizes PETSc if and only if PHREEQC initialised it and the reference count is 0.
   Returns 0 on success or if there is nothing to do; -1 if references remain.
   Idempotent. */
int  shutdown_petsc_if_owned_by_phreeqc(void);

/* Serialises operations touching PETSc global state outside the hot path.
   Never call acquire_petsc/release_petsc/shutdown_petsc_if_owned_by_phreeqc while holding it. */
std::mutex &petsc_global_state_mutex(void);

/* For tests and diagnostics. */
int  petsc_reference_count(void);
bool petsc_initialized_by_phreeqc(void);
#endif
```

### Implementation: `petsc_initialization.cpp`

Required behaviour:

```cpp
#ifdef USE_PETSC
#include "petsc_initialization.h"
#include <cstdlib>
#include <petscsys.h>

namespace {
std::mutex global_state_mutex;                 /* guards everything below            */
int        reference_count           = 0;
bool       phreeqc_initialized_petsc = false;   /* PHREEQC called PetscInitialize*     */
bool       exit_handler_registered   = false;

/* Caller holds global_state_mutex. Precondition: phreeqc_initialized_petsc. */
void finalize_petsc_while_locked(void)
{
    PetscPopErrorHandler();
    PetscFinalize();
    phreeqc_initialized_petsc = false;
}

void finalize_petsc_at_process_exit(void)
{
    std::lock_guard<std::mutex> state_lock(global_state_mutex);
    if (phreeqc_initialized_petsc && reference_count == 0) finalize_petsc_while_locked();
    /* If references remain at exit (objects with static storage still alive),
       do nothing: their destructors check PetscFinalized() before destroying. */
}
} // namespace

std::mutex &petsc_global_state_mutex(void) { return global_state_mutex; }

int acquire_petsc(void)
{
    std::lock_guard<std::mutex> state_lock(global_state_mutex);
    if (reference_count == 0 && !phreeqc_initialized_petsc) {
        PetscBool petsc_is_initialized = PETSC_FALSE, petsc_is_finalized = PETSC_FALSE;
        if (PetscFinalized(&petsc_is_finalized) != PETSC_SUCCESS || petsc_is_finalized) return -1;
        if (PetscInitialized(&petsc_is_initialized) != PETSC_SUCCESS) return -1;
        if (!petsc_is_initialized) {
            /* Never pass argv: PHREEQC's command line is positional.
               PETSc options come from the PETSC_OPTIONS environment variable. */
            if (PetscInitializeNoArguments() != PETSC_SUCCESS) return -1;
            phreeqc_initialized_petsc = true;
            PetscPushErrorHandler(PetscReturnErrorHandler, NULL);
            if (!exit_handler_registered) {
                std::atexit(finalize_petsc_at_process_exit);
                exit_handler_registered = true;
            }
        }
    }
    ++reference_count;
    return 0;
}

void release_petsc(void)
{
    std::lock_guard<std::mutex> state_lock(global_state_mutex);
    if (reference_count > 0) --reference_count;
}

int shutdown_petsc_if_owned_by_phreeqc(void)
{
    std::lock_guard<std::mutex> state_lock(global_state_mutex);
    if (reference_count != 0) return -1;
    if (phreeqc_initialized_petsc) finalize_petsc_while_locked();
    return 0;
}

int petsc_reference_count(void)
{
    std::lock_guard<std::mutex> state_lock(global_state_mutex);
    return reference_count;
}

bool petsc_initialized_by_phreeqc(void)
{
    std::lock_guard<std::mutex> state_lock(global_state_mutex);
    return phreeqc_initialized_petsc;
}
#endif
```

**Why release never finalizes.** PETSc cannot be reliably re-initialised after `PetscFinalize()`. Hosts such as IPhreeqc users may create and destroy PHREEQC objects repeatedly. If release finalized when the count reached zero, the next object would silently lose PETSc. Finalization therefore happens only:

- at the end of the phreeqc executable's `main()`, through an explicit `shutdown_petsc_if_owned_by_phreeqc()` (added in task Task 4); or
- in the `atexit` handler.

**Host-owned PETSc.** If PETSc was already initialised by a host application, PHREEQC never finalizes it and never installs an error handler.

### CMake

All changes go into the top-level `CMakeLists.txt`, directly after `include(CTest)` (`CMakeLists.txt:312`). That position is after the `phreeqc` target (`CMakeLists.txt:260-302`), so the target exists when it is linked, and after `include(CTest)`, which is what defines `BUILD_TESTING`; placed earlier, the test block would silently create no tests on a fresh configure.

The existing call `target_link_libraries(phreeqc ${GMP_LIBRARY})` (`CMakeLists.txt:300`) uses the **plain** form. Link the adapter to `phreeqc` with the plain form as well (no `PUBLIC`/`PRIVATE` keyword), because CMake rejects mixing the two forms on one target. The keyword form is fine on the new targets, which this task creates.

```cmake
option(PHREEQC_USE_PETSC "Use PETSc for the Newton linear solve (Phase 1A)" OFF)
if(PHREEQC_USE_PETSC)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(PETSC REQUIRED IMPORTED_TARGET PETSc)

  add_library(phreeqc_petsc_linear_solver STATIC
      src/petsc_linear_solver/petsc_initialization.cpp)
  set_target_properties(phreeqc_petsc_linear_solver PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_compile_features(phreeqc_petsc_linear_solver PUBLIC cxx_std_11)
  target_include_directories(phreeqc_petsc_linear_solver PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src/petsc_linear_solver
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/src/common)
  target_compile_definitions(phreeqc_petsc_linear_solver PUBLIC USE_PETSC)   # PUBLIC: every consumer sees it
  target_link_libraries(phreeqc_petsc_linear_solver PUBLIC PkgConfig::PETSC)

  # Plain form, matching CMakeLists.txt:300. Usage requirements (USE_PETSC, include
  # directories, PETSc link flags) still propagate to phreeqc.
  target_link_libraries(phreeqc phreeqc_petsc_linear_solver)

  if(BUILD_TESTING)   # add enable_testing()/include(CTest) inside this if() only if absent
    foreach(test_name test_initialization_phreeqc_owns_petsc test_initialization_host_owns_petsc test_initialization_explicit_shutdown)
      add_executable(${test_name} tests/petsc_linear_solver/${test_name}.cpp)
      target_link_libraries(${test_name} PRIVATE phreeqc_petsc_linear_solver)
      add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
  endif()
endif()
```

`src/common` is on the include path because `LDBLE` is defined in `src/common/phrqtype.h` (report B7); later tasks include it.

Before writing the `BUILD_TESTING` part, check with `grep -n "enable_testing\|include(CTest)\|BUILD_TESTING" CMakeLists.txt` whether testing is already enabled. Report what you find.

### Tests

Use a simple `CHECK_OR_FAIL(condition)` macro that prints the failed condition with its line number and returns 1 from `main`. Do not use `assert`, because it is disabled under `NDEBUG`. Each test is its own executable, because PETSc can be initialised only once per process.

- **`test_initialization_phreeqc_owns_petsc`:**
  1. `acquire_petsc() == 0`; PETSc is initialised; `petsc_initialized_by_phreeqc()` is true.
  2. A second `acquire_petsc() == 0`; `petsc_reference_count() == 2`.
  3. Call `release_petsc()` twice. `petsc_reference_count() == 0`, and PETSc is **not** finalized.
  4. `shutdown_petsc_if_owned_by_phreeqc() == 0`, and PETSc is finalized.
  5. `acquire_petsc() == -1` (re-initialisation refused); a second `shutdown_petsc_if_owned_by_phreeqc() == 0`.
- **`test_initialization_host_owns_petsc`:**
  1. The test calls `PetscInitializeNoArguments()` itself, acting as the host application.
  2. `acquire_petsc() == 0`; `petsc_initialized_by_phreeqc()` is false.
  3. Call `release_petsc()`, then `shutdown_petsc_if_owned_by_phreeqc() == 0`. PETSc is still initialised, not finalized.
  4. The test calls `PetscFinalize()` itself and exits 0.
- **`test_initialization_explicit_shutdown`:**
  1. `acquire_petsc() == 0`; `shutdown_petsc_if_owned_by_phreeqc() == -1` while the reference is held; PETSc is not finalized.
  2. `release_petsc()`; `shutdown_petsc_if_owned_by_phreeqc() == 0`; PETSc is finalized.

## Validation

0. **Environment.** `echo "$PETSC_DIR"` must print `/opt/petsc/3.26.0`, and `pkg-config --modversion PETSc` must print `3.26.0`. If either fails, stop and report; do not change the environment yourself.
1. **OFF build.** Configure and build in `build/petsc_off`:
   ```
   cmake -S . -B build/petsc_off -DCMAKE_BUILD_TYPE=Release -DPHREEQC_USE_PETSC=OFF
   cmake --build build/petsc_off --target phreeqc -j 8
   ```
   Run `git diff --stat` and confirm that only `CMakeLists.txt` changed, apart from the new files listed above. Run ex1 and ex2 with the commands from `tests/petsc_linear_solver/baseline/README.md`, using `build/petsc_off/phreeqc` and the working directory `build/petsc_off/run`. Then compare:
   - each `.sel` file (`ex1_seawater_speciation.sel`, `ex2_gypsum_anhydrite_equilibria.sel`, `ex2.sel`) with its baseline copy using `cmp`: all must be identical;
   - each `.out` file with `python3 tests/petsc_linear_solver/tools/compare_main_output_to_baseline.py <baseline .out> <new .out>`: both must report `IDENTICAL`.
2. **ON build.** Configure and build in `build/petsc_on`:
   ```
   cmake -S . -B build/petsc_on -DCMAKE_BUILD_TYPE=Release -DPHREEQC_USE_PETSC=ON
   cmake --build build/petsc_on -j 8
   ```
   All targets must build.
3. Run `ctest --test-dir build/petsc_on -R test_initialization --output-on-failure`. All three tests must pass.
4. *(Superseded by decision D14: valgrind cannot execute the installed PETSc library. Task 1 was accepted without this step.)*

## Report

Give pass or fail and the output for each validation step, then a list of files changed with one line each. List every PETSc symbol you used with the header and line where you found its declaration. List any deviation from this prompt and the reason for it.

Stop after the report. Do not start Task 2.
