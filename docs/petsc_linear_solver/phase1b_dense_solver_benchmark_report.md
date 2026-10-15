# Phase 1B Task 1: Dense-Solver Micro-Benchmark Report

Specification: `docs/petsc_linear_solver/implementation_tasks/phase1b_task1_dense_solver_benchmark.md`.
Program: `tests/petsc_linear_solver/benchmark_dense_solvers.cpp`. Target: `benchmark_dense_solvers` in `CMakeLists.txt`.

## Environment

- CPU model (`lscpu | grep "Model name"`): `INTEL(R) XEON(R) GOLD 6530`
- Compiler (`/usr/bin/c++ --version`): `c++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0`
- Build type: `-DCMAKE_BUILD_TYPE=Release` (`CMAKE_CXX_FLAGS_RELEASE:STRING=-O3 -DNDEBUG` in `build/petsc_on/CMakeCache.txt`)
- PETSc: `PETSC_DIR=/opt/petsc/3.26.0`, `PETSC_ARCH` empty. `petscversion.h` lines 7-10: `PETSC_VERSION_RELEASE 1`, `PETSC_VERSION_MAJOR 3`, `PETSC_VERSION_MINOR 26`, `PETSC_VERSION_SUBMINOR 0`.
- PETSc configure options (`/opt/petsc/3.26.0/lib/petsc/conf/petscvariables`, `CONFIGURE_OPTIONS`): `--with-debugging=0 --with-mpi=0 --with-shared-libraries=1 --with-scalar-type=real --with-precision=double --with-64-bit-indices=0 --download-f2cblaslapack=0 --with-blaslapack-lib="[.../libopenblas.so]" COPTFLAGS="-O3 -march=native" CXXOPTFLAGS="-O3 -march=native"`

`uptime` before each benchmark run:

```
run 1: 17:32:54 up 8 days,  8:10, 24 users,  load average: 1.32, 0.91, 0.72
run 2: 17:34:00 up 8 days,  8:11, 24 users,  load average: 1.04, 0.91, 0.73
run 3: 17:34:45 up 8 days,  8:12, 24 users,  load average: 0.98, 0.92, 0.74
```

## Build and link

The first ON build failed to link the benchmark target, as anticipated by the specification:

```
CMakeFiles/benchmark_dense_solvers.dir/tests/petsc_linear_solver/benchmark_dense_solvers.cpp.o: undefined reference to symbol 'dgetrf_'
/usr/bin/x86_64-linux-gnu-ld.bfd: /usr/lib/x86_64-linux-gnu/libopenblas.so.0: error adding symbols: DSO missing from command line
```

The fallback in the specification was applied. The libraries are taken from `/opt/petsc/3.26.0/lib/petsc/conf/petscvariables` line 43, `BLASLAPACK_LIB = -Wl,-rpath,/usr/lib/x86_64-linux-gnu -L/usr/lib/x86_64-linux-gnu -lopenblas`. The line added to `CMakeLists.txt` is:

```cmake
target_link_libraries(benchmark_dense_solvers PRIVATE phreeqc_petsc_linear_solver
    "-Wl,-rpath,/usr/lib/x86_64-linux-gnu" "-L/usr/lib/x86_64-linux-gnu" -lopenblas)
```

No other change was made. After the fallback, `cmake --build build/petsc_on -j 8` built all targets, exit status 0.

## Validation

1. OFF build unchanged. `build/petsc_off` configured with `-DPHREEQC_USE_PETSC=OFF`, `phreeqc` built. ex1 and ex2 were run from `build/petsc_off/phase1b_task1_off_build_regression/` with the commands in `tests/petsc_linear_solver/baseline/README.md` (both exit status 0). Comparisons were made directly against the files in that directory; nothing was copied under `tests/`:

```
cmp tests/petsc_linear_solver/baseline/ex1_seawater_speciation.sel build/petsc_off/phase1b_task1_off_build_regression/ex1_seawater_speciation.sel            -> exit 0
cmp tests/petsc_linear_solver/baseline/ex2_gypsum_anhydrite_equilibria.sel build/petsc_off/phase1b_task1_off_build_regression/ex2_gypsum_anhydrite_equilibria.sel -> exit 0
cmp tests/petsc_linear_solver/baseline/ex2.sel build/petsc_off/phase1b_task1_off_build_regression/ex2.sel                                                  -> exit 0
compare_main_output_to_baseline.py ex1: IDENTICAL  compared_lines=355 ignored_run_time_lines baseline=3 new=3                                              -> exit 0
compare_main_output_to_baseline.py ex2: IDENTICAL  compared_lines=4369 ignored_run_time_lines baseline=3 new=3                                             -> exit 0
```

2. ON build tests. `ctest --test-dir build/petsc_on -R "test_initialization|test_newton_matrix_transfer|test_newton_solver"`: `100% tests passed, 0 tests failed out of 6` (all six tests Passed).

3. Benchmark exit status: run 1 `exit_status=0`, run 2 `exit_status=0`, run 3 `exit_status=0`. Each CSV has 145 lines: the header plus 144 rows (8 sizes x 3 classes x (1 + 1 + 4) rows).

## Symbols used, with the header where each declaration was found

All paths are under `/opt/petsc/3.26.0/include`. Signatures were taken from these declarations.

| Symbol | Header and line |
|---|---|
| `MatCreateSeqDense(MPI_Comm, PetscInt, PetscInt, PetscScalar[], Mat *)` | `petscmat.h:492` |
| `MatCreateVecs(Mat, Vec *, Vec *)` | `petscmat.h:1025` |
| `MatDestroy(Mat *)` | `petscmat.h:609` |
| `MatMult(Mat, Vec, Vec)` | `petscmat.h:788` |
| `MatNorm(Mat, NormType, PetscReal *)` | `petscmat.h:919` |
| `MATORDERINGNATURAL` (`"natural"`) | `petscmat.h:1520` |
| `VecDuplicate(Vec, Vec *)` | `petscvec.h:359` |
| `VecDestroy(Vec *)` | `petscvec.h:145` |
| `VecAYPX(Vec, PetscScalar, Vec)` | `petscvec.h:342` |
| `VecNorm(Vec, NormType, PetscReal *)` | `petscvec.h:319` |
| `NORM_INFINITY` (`NormType` enumerator, value 3) | `petscvec.h:186` |
| `KSPCreate(MPI_Comm, KSP *)` | `petscksp.h:101` |
| `KSPSetType(KSP, KSPType)` | `petscksp.h:102` |
| `KSPSolve(KSP, Vec, Vec)` | `petscksp.h:106` |
| `KSPDestroy(KSP *)` | `petscksp.h:123` |
| `KSPSetInitialGuessNonzero(KSP, PetscBool)` | `petscksp.h:190` |
| `KSPSetErrorIfNotConverged(KSP, PetscBool)` | `petscksp.h:192` |
| `KSPGetPC(KSP, PC *)` | `petscksp.h:231` |
| `KSPSetFromOptions(KSP)` | `petscksp.h:647` |
| `KSPSetOperators(KSP, Mat, Mat)` | `petscksp.h:708` |
| `KSPSetOptionsPrefix(KSP, const char[])` | `petscksp.h:711` |
| `KSPPREONLY` (`"preonly"`) | `petscksp.h:79` |
| `PCSetType(PC, PCType)` | `petscpc.h:44` |
| `PCSetUp(PC)` | `petscpc.h:46` |
| `PCFactorSetMatOrderingType(PC, MatOrderingType)` | `petscpc.h:214` |
| `PCLU` (`"lu"`) | `petscpctypes.h:31` |
| `LAPACKgetrf_(const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *, PetscBLASInt *)` | `petscblaslapack.h:104` |
| `LAPACKgetrs_(const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *)` | `petscblaslapack.h:150` |
| `PetscBLASInt` (`typedef int`, the 32-bit-index branch at line 402) | `petscsystypes.h:402` |
| `PETSC_MAX_REAL` (`1.7976931348623157e+308`, the double branch) | `petscmath.h:2021` |

Adapter symbols (not PETSc) are declared in `src/petsc_linear_solver/`: `acquire_petsc`, `release_petsc`, `shutdown_petsc_if_owned_by_phreeqc` (`petsc_initialization.h`); `DenseMatrixLayout`, `phreeqc_newton_matrix_layout`, `load_newton_system_into_petsc`, `copy_solution_from_petsc` (`newton_matrix_transfer.h`); `PetscNewtonSolver`, `create_petsc_newton_solver`, `destroy_petsc_newton_solver`, `solve_newton_system` (`petsc_newton_solver.h`); `LinearSolveStatus` (`linear_solve_status.h`).

## How the program runs

- Systems are built exactly as the specification defines (LCG `state = state * 6364136223846793005 + 1442695040888963407`, seed `12345 + n`, `x_true[i] = (i % 2 == 0 ? 1.0 : -1.0) * (1.0 + 0.1 * i)`, `rhs = J * x_true` in `long double` rounded to `double`), stored row-major with leading dimension `n + 1`, and loaded through `phreeqc_newton_matrix_layout()`.
- For each (solver, class, n): 200 warm-up calls, then 5 batches. The call count per batch is fixed once from the warm-up timing so that a batch takes at least 0.1 s, and is used for all 5 batches. The reported value is the median (and the minimum) of the 5 batch averages.
- `calls_per_batch` is derived from each run's own warm-up timing, so it differs between runs and between solvers. The reported values are per-call, so they are comparable across runs.
- Correctness is checked on the last call of each batch; the reported `max_relative_solution_error`, `backward_error`, and `accepted` are from that call.

## Full CSV, run 1 (`build/petsc_on/benchmark_dense_solvers.csv`), verbatim

```
solver,matrix_class,n,phase,ns_per_call_median,ns_per_call_min,calls_per_batch,accepted,max_relative_solution_error,backward_error
lapack_direct,well_conditioned,5,total,496.1,495.7,129966,1,1.586033e-16,7.307727e-17
petsc_adapter,well_conditioned,5,total,1779.6,1774.7,23013,1,1.586033e-16,5.846182e-17
petsc_breakdown,well_conditioned,5,load,488.5,487.5,51074,1,1.586033e-16,5.846182e-17
petsc_breakdown,well_conditioned,5,factor_solve,967.3,966.4,51074,1,1.586033e-16,5.846182e-17
petsc_breakdown,well_conditioned,5,backward_error,287.3,286.5,51074,1,1.586033e-16,5.846182e-17
petsc_breakdown,well_conditioned,5,copy_out,31.1,30.7,51074,1,1.586033e-16,5.846182e-17
lapack_direct,badly_row_scaled,5,total,278.8,259.1,318690,1,1.586033e-16,5.846182e-17
petsc_adapter,badly_row_scaled,5,total,1909.7,1884.7,54631,1,1.586033e-16,5.846182e-17
petsc_breakdown,badly_row_scaled,5,load,544.2,527.1,49087,1,1.586033e-16,5.846182e-17
petsc_breakdown,badly_row_scaled,5,factor_solve,1073.8,1040.2,49087,1,1.586033e-16,5.846182e-17
petsc_breakdown,badly_row_scaled,5,backward_error,317.9,307.9,49087,1,1.586033e-16,5.846182e-17
petsc_breakdown,badly_row_scaled,5,copy_out,34.2,33.2,49087,1,1.586033e-16,5.846182e-17
lapack_direct,near_singular,5,total,276.8,265.8,309852,1,2.101546e-06,5.115415e-17
petsc_adapter,near_singular,5,total,2502.3,1929.4,53940,1,2.101546e-06,5.846188e-17
petsc_breakdown,near_singular,5,load,709.8,703.5,37926,1,2.101546e-06,5.846188e-17
petsc_breakdown,near_singular,5,factor_solve,1397.5,1389.8,37926,1,2.101546e-06,5.846188e-17
petsc_breakdown,near_singular,5,backward_error,412.3,411.0,37926,1,2.101546e-06,5.846188e-17
petsc_breakdown,near_singular,5,copy_out,45.1,44.0,37926,1,2.101546e-06,5.846188e-17
lapack_direct,well_conditioned,9,total,848.1,847.5,110910,1,2.467162e-16,9.562004e-17
petsc_adapter,well_conditioned,9,total,3720.2,3716.7,24636,1,2.467162e-16,9.870455e-17
petsc_breakdown,well_conditioned,9,load,1409.1,1403.8,24468,1,2.467162e-16,9.870455e-17
petsc_breakdown,well_conditioned,9,factor_solve,1808.1,1804.7,24468,1,2.467162e-16,9.870455e-17
petsc_breakdown,well_conditioned,9,backward_error,466.4,464.9,24468,1,2.467162e-16,9.870455e-17
petsc_breakdown,well_conditioned,9,copy_out,68.7,68.2,24468,1,2.467162e-16,9.870455e-17
lapack_direct,badly_row_scaled,9,total,859.0,839.8,117227,1,2.467162e-16,9.562004e-17
petsc_adapter,badly_row_scaled,9,total,3718.6,3640.0,25395,1,2.467162e-16,9.870455e-17
petsc_breakdown,badly_row_scaled,9,load,1412.2,1381.9,25693,1,2.467162e-16,9.870455e-17
petsc_breakdown,badly_row_scaled,9,factor_solve,1816.8,1774.8,25693,1,2.467162e-16,9.870455e-17
petsc_breakdown,badly_row_scaled,9,backward_error,464.9,455.5,25693,1,2.467162e-16,9.870455e-17
petsc_breakdown,badly_row_scaled,9,copy_out,69.1,67.7,25693,1,2.467162e-16,9.870455e-17
lapack_direct,near_singular,9,total,709.0,659.8,119466,1,4.452854e-06,1.573108e-16
petsc_adapter,near_singular,9,total,3206.1,3000.6,33878,1,4.452854e-06,1.480572e-16
petsc_breakdown,near_singular,9,load,1360.2,1214.0,27042,1,4.452854e-06,1.480572e-16
petsc_breakdown,near_singular,9,factor_solve,1743.5,1560.0,27042,1,4.452854e-06,1.480572e-16
petsc_breakdown,near_singular,9,backward_error,449.4,404.1,27042,1,4.452854e-06,1.480572e-16
petsc_breakdown,near_singular,9,copy_out,66.5,59.6,27042,1,4.452854e-06,1.480572e-16
lapack_direct,well_conditioned,12,total,1359.9,1253.7,64191,1,3.172066e-16,1.362157e-16
petsc_adapter,well_conditioned,12,total,4853.7,4811.7,19164,1,3.172066e-16,1.257376e-16
petsc_breakdown,well_conditioned,12,load,2015.7,1946.3,19581,1,3.172066e-16,1.257376e-16
petsc_breakdown,well_conditioned,12,factor_solve,2224.6,2152.4,19581,1,3.172066e-16,1.257376e-16
petsc_breakdown,well_conditioned,12,backward_error,499.3,484.1,19581,1,3.172066e-16,1.257376e-16
petsc_breakdown,well_conditioned,12,copy_out,77.5,75.2,19581,1,3.172066e-16,1.257376e-16
lapack_direct,badly_row_scaled,12,total,1359.0,1336.5,71392,1,3.172066e-16,9.233856e-17
petsc_adapter,badly_row_scaled,12,total,4797.9,4199.5,20249,1,3.172066e-16,1.676501e-16
petsc_breakdown,badly_row_scaled,12,load,1710.8,1653.7,24171,1,3.172066e-16,1.676501e-16
petsc_breakdown,badly_row_scaled,12,factor_solve,1885.3,1827.7,24171,1,3.172066e-16,1.676501e-16
petsc_breakdown,badly_row_scaled,12,backward_error,422.6,408.9,24171,1,3.172066e-16,1.676501e-16
petsc_breakdown,badly_row_scaled,12,copy_out,65.8,63.7,24171,1,3.172066e-16,1.676501e-16
lapack_direct,near_singular,12,total,1103.0,1061.5,88473,1,7.247032e-06,1.101231e-16
petsc_adapter,near_singular,12,total,3804.4,3672.1,25180,1,7.247032e-06,1.258550e-16
petsc_breakdown,near_singular,12,load,1583.5,1524.0,24997,1,7.247032e-06,1.258550e-16
petsc_breakdown,near_singular,12,factor_solve,1745.7,1675.3,24997,1,7.247032e-06,1.258550e-16
petsc_breakdown,near_singular,12,backward_error,392.6,377.4,24997,1,7.247032e-06,1.258550e-16
petsc_breakdown,near_singular,12,copy_out,61.0,58.5,24997,1,7.247032e-06,1.258550e-16
lapack_direct,well_conditioned,16,total,1670.5,1659.2,57979,1,3.552714e-16,9.914282e-17
petsc_adapter,well_conditioned,16,total,5378.3,4962.0,17932,1,3.552714e-16,1.321904e-16
petsc_breakdown,well_conditioned,16,load,2551.5,2541.5,18091,1,3.552714e-16,1.321904e-16
petsc_breakdown,well_conditioned,16,factor_solve,2286.2,2275.1,18091,1,3.552714e-16,1.321904e-16
petsc_breakdown,well_conditioned,16,backward_error,467.8,467.2,18091,1,3.552714e-16,1.321904e-16
petsc_breakdown,well_conditioned,16,copy_out,81.9,81.3,18091,1,3.552714e-16,1.321904e-16
lapack_direct,badly_row_scaled,16,total,1659.2,1540.4,59535,1,3.552714e-16,9.914282e-17
petsc_adapter,badly_row_scaled,16,total,5474.2,5373.5,18487,1,3.552714e-16,1.321904e-16
petsc_breakdown,badly_row_scaled,16,load,3159.6,3154.5,14846,1,3.552714e-16,1.321904e-16
petsc_breakdown,badly_row_scaled,16,factor_solve,2838.4,2831.9,14846,1,3.552714e-16,1.321904e-16
petsc_breakdown,badly_row_scaled,16,backward_error,582.9,582.3,14846,1,3.552714e-16,1.321904e-16
petsc_breakdown,badly_row_scaled,16,copy_out,103.4,102.5,14846,1,3.552714e-16,1.321904e-16
lapack_direct,near_singular,16,total,2123.1,2098.2,45733,1,5.312397e-07,1.222308e-16
petsc_adapter,near_singular,16,total,6936.0,6717.8,14103,1,5.312397e-07,1.086496e-16
petsc_breakdown,near_singular,16,load,3240.2,3157.3,14840,1,5.312397e-07,1.086496e-16
petsc_breakdown,near_singular,16,factor_solve,2890.0,2825.6,14840,1,5.312397e-07,1.086496e-16
petsc_breakdown,near_singular,16,backward_error,596.1,578.3,14840,1,5.312397e-07,1.086496e-16
petsc_breakdown,near_singular,16,copy_out,104.8,102.6,14840,1,5.312397e-07,1.086496e-16
lapack_direct,well_conditioned,20,total,3132.9,3112.1,31062,1,3.062684e-16,1.317211e-16
petsc_adapter,well_conditioned,20,total,9439.4,9338.0,10172,1,3.062684e-16,1.221761e-16
petsc_breakdown,well_conditioned,20,load,4851.1,4808.3,9887,1,3.062684e-16,1.221761e-16
petsc_breakdown,well_conditioned,20,factor_solve,3922.9,3899.1,9887,1,3.062684e-16,1.221761e-16
petsc_breakdown,well_conditioned,20,backward_error,717.3,712.5,9887,1,3.062684e-16,1.221761e-16
petsc_breakdown,well_conditioned,20,copy_out,129.2,128.4,9887,1,3.062684e-16,1.221761e-16
lapack_direct,badly_row_scaled,20,total,3214.6,2812.6,31049,1,4.594026e-16,1.832641e-16
petsc_adapter,badly_row_scaled,20,total,9140.3,8892.7,10837,1,4.594026e-16,1.832641e-16
petsc_breakdown,badly_row_scaled,20,load,4646.0,4210.2,10506,1,4.594026e-16,1.832641e-16
petsc_breakdown,badly_row_scaled,20,factor_solve,3765.4,3412.0,10506,1,4.594026e-16,1.832641e-16
petsc_breakdown,badly_row_scaled,20,backward_error,688.9,627.3,10506,1,4.594026e-16,1.832641e-16
petsc_breakdown,badly_row_scaled,20,copy_out,124.1,111.8,10506,1,4.594026e-16,1.832641e-16
lapack_direct,near_singular,20,total,3076.4,2991.4,38133,1,4.051962e-06,1.765830e-16
petsc_adapter,near_singular,20,total,9123.5,8971.6,10433,1,4.051962e-06,1.527205e-16
petsc_breakdown,near_singular,20,load,4671.2,4543.6,10312,1,4.051962e-06,1.527205e-16
petsc_breakdown,near_singular,20,factor_solve,3762.1,3708.0,10312,1,4.051962e-06,1.527205e-16
petsc_breakdown,near_singular,20,backward_error,690.7,671.7,10312,1,4.051962e-06,1.527205e-16
petsc_breakdown,near_singular,20,copy_out,123.9,120.7,10312,1,4.051962e-06,1.527205e-16
lapack_direct,well_conditioned,25,total,4941.8,4848.2,20024,1,2.612289e-16,1.845135e-16
petsc_adapter,well_conditioned,25,total,12973.4,12843.0,7447,1,2.612289e-16,1.527008e-16
petsc_breakdown,well_conditioned,25,load,7011.2,6027.2,7282,1,2.612289e-16,1.527008e-16
petsc_breakdown,well_conditioned,25,factor_solve,4942.4,4183.9,7282,1,2.612289e-16,1.527008e-16
petsc_breakdown,well_conditioned,25,backward_error,849.9,712.7,7282,1,2.612289e-16,1.527008e-16
petsc_breakdown,well_conditioned,25,copy_out,159.7,137.5,7282,1,2.612289e-16,1.527008e-16
lapack_direct,badly_row_scaled,25,total,3903.3,3864.6,25011,1,5.224579e-16,1.972386e-16
petsc_adapter,badly_row_scaled,25,total,10753.3,10239.8,9381,1,5.224579e-16,2.036011e-16
petsc_breakdown,badly_row_scaled,25,load,5662.1,5638.5,9517,1,5.224579e-16,2.036011e-16
petsc_breakdown,badly_row_scaled,25,factor_solve,3971.2,3951.0,9517,1,5.224579e-16,2.036011e-16
petsc_breakdown,badly_row_scaled,25,backward_error,681.7,677.5,9517,1,5.224579e-16,2.036011e-16
petsc_breakdown,badly_row_scaled,25,copy_out,129.2,128.7,9517,1,5.224579e-16,2.036011e-16
lapack_direct,near_singular,25,total,4083.5,3930.1,26181,1,5.113089e-05,1.586475e-16
petsc_adapter,near_singular,25,total,10435.0,10325.9,9374,1,5.113089e-05,1.586475e-16
petsc_breakdown,near_singular,25,load,5717.6,5707.3,9435,1,5.113089e-05,1.586475e-16
petsc_breakdown,near_singular,25,factor_solve,4006.7,3967.6,9435,1,5.113089e-05,1.586475e-16
petsc_breakdown,near_singular,25,backward_error,694.3,684.0,9435,1,5.113089e-05,1.586475e-16
petsc_breakdown,near_singular,25,copy_out,130.1,129.8,9435,1,5.113089e-05,1.586475e-16
lapack_direct,well_conditioned,30,total,5297.6,5226.5,19102,1,5.693451e-16,2.234740e-16
petsc_adapter,well_conditioned,30,total,13892.9,13715.4,6037,1,5.693451e-16,2.549246e-16
petsc_breakdown,well_conditioned,30,load,9134.2,8675.0,6180,1,5.693451e-16,2.549246e-16
petsc_breakdown,well_conditioned,30,factor_solve,6008.5,5650.5,6180,1,5.693451e-16,2.549246e-16
petsc_breakdown,well_conditioned,30,backward_error,954.6,904.3,6180,1,5.693451e-16,2.549246e-16
petsc_breakdown,well_conditioned,30,copy_out,193.0,187.1,6180,1,5.693451e-16,2.549246e-16
lapack_direct,badly_row_scaled,30,total,5757.1,5328.8,15423,1,5.693451e-16,2.354235e-16
petsc_adapter,badly_row_scaled,30,total,14874.6,13911.3,7377,1,5.693451e-16,2.549246e-16
petsc_breakdown,badly_row_scaled,30,load,9488.6,9029.2,5860,1,5.693451e-16,2.549246e-16
petsc_breakdown,badly_row_scaled,30,factor_solve,6317.1,5977.6,5860,1,5.693451e-16,2.549246e-16
petsc_breakdown,badly_row_scaled,30,backward_error,1005.2,958.7,5860,1,5.693451e-16,2.549246e-16
petsc_breakdown,badly_row_scaled,30,copy_out,196.1,186.2,5860,1,5.693451e-16,2.549246e-16
lapack_direct,near_singular,30,total,6506.3,6200.5,15405,1,1.326138e-05,1.966605e-16
petsc_adapter,near_singular,30,total,16872.4,15698.6,5693,1,1.326138e-05,1.355792e-16
petsc_breakdown,near_singular,30,load,8782.5,8352.7,6373,1,1.326138e-05,1.355792e-16
petsc_breakdown,near_singular,30,factor_solve,5965.2,5449.5,6373,1,1.326138e-05,1.355792e-16
petsc_breakdown,near_singular,30,backward_error,951.3,874.8,6373,1,1.326138e-05,1.355792e-16
petsc_breakdown,near_singular,30,copy_out,188.3,177.4,6373,1,1.326138e-05,1.355792e-16
lapack_direct,well_conditioned,40,total,10729.0,10005.5,8860,1,9.063045e-16,3.319098e-16
petsc_adapter,well_conditioned,40,total,28328.1,27834.5,3470,1,9.063045e-16,2.749803e-16
petsc_breakdown,well_conditioned,40,load,16352.7,16327.7,3481,1,9.063045e-16,2.749803e-16
petsc_breakdown,well_conditioned,40,factor_solve,10019.1,9971.6,3481,1,9.063045e-16,2.749803e-16
petsc_breakdown,well_conditioned,40,backward_error,1479.1,1476.5,3481,1,9.063045e-16,2.749803e-16
petsc_breakdown,well_conditioned,40,copy_out,277.5,273.8,3481,1,9.063045e-16,2.749803e-16
lapack_direct,badly_row_scaled,40,total,12286.4,11762.7,8892,1,7.250436e-16,3.990436e-16
petsc_adapter,badly_row_scaled,40,total,30126.2,30016.2,3263,1,7.250436e-16,3.437253e-16
petsc_breakdown,badly_row_scaled,40,load,17506.9,17473.3,3231,1,7.250436e-16,3.437253e-16
petsc_breakdown,badly_row_scaled,40,factor_solve,10771.1,10751.8,3231,1,7.250436e-16,3.437253e-16
petsc_breakdown,badly_row_scaled,40,backward_error,1600.1,1591.0,3231,1,7.250436e-16,3.437253e-16
petsc_breakdown,badly_row_scaled,40,copy_out,297.0,295.3,3231,1,7.250436e-16,3.437253e-16
lapack_direct,near_singular,40,total,12291.7,12275.4,8284,1,2.269855e-05,2.674576e-16
petsc_adapter,near_singular,40,total,29991.8,29944.8,3279,1,2.269855e-05,3.437206e-16
petsc_breakdown,near_singular,40,load,17471.8,11741.3,3281,1,2.269855e-05,3.437206e-16
petsc_breakdown,near_singular,40,factor_solve,10728.2,7046.3,3281,1,2.269855e-05,3.437206e-16
petsc_breakdown,near_singular,40,backward_error,1588.2,1081.1,3281,1,2.269855e-05,3.437206e-16
petsc_breakdown,near_singular,40,copy_out,294.2,199.9,3281,1,2.269855e-05,3.437206e-16
```

## Full CSV, run 2 (`build/petsc_on/benchmark_dense_solvers_run2.csv`), verbatim

```
solver,matrix_class,n,phase,ns_per_call_median,ns_per_call_min,calls_per_batch,accepted,max_relative_solution_error,backward_error
lapack_direct,well_conditioned,5,total,495.4,494.2,148103,1,1.586033e-16,7.307727e-17
petsc_adapter,well_conditioned,5,total,3338.4,3334.0,24497,1,1.586033e-16,5.846182e-17
petsc_breakdown,well_conditioned,5,load,975.4,973.6,26121,1,1.586033e-16,5.846182e-17
petsc_breakdown,well_conditioned,5,factor_solve,1883.0,1882.5,26121,1,1.586033e-16,5.846182e-17
petsc_breakdown,well_conditioned,5,backward_error,620.3,618.8,26121,1,1.586033e-16,5.846182e-17
petsc_breakdown,well_conditioned,5,copy_out,84.1,83.8,26121,1,1.586033e-16,5.846182e-17
lapack_direct,badly_row_scaled,5,total,353.2,317.6,184686,1,1.586033e-16,5.846182e-17
petsc_adapter,badly_row_scaled,5,total,2442.6,2295.0,40622,1,1.586033e-16,5.846182e-17
petsc_breakdown,badly_row_scaled,5,load,704.6,686.3,35899,1,1.586033e-16,5.846182e-17
petsc_breakdown,badly_row_scaled,5,factor_solve,1373.3,1325.5,35899,1,1.586033e-16,5.846182e-17
petsc_breakdown,badly_row_scaled,5,backward_error,466.9,450.8,35899,1,1.586033e-16,5.846182e-17
petsc_breakdown,badly_row_scaled,5,copy_out,61.0,59.8,35899,1,1.586033e-16,5.846182e-17
lapack_direct,near_singular,5,total,325.3,299.8,261592,1,2.101546e-06,5.115415e-17
petsc_adapter,near_singular,5,total,2313.9,2267.2,49221,1,2.101546e-06,5.846188e-17
petsc_breakdown,near_singular,5,load,632.1,612.1,37136,1,2.101546e-06,5.846188e-17
petsc_breakdown,near_singular,5,factor_solve,1220.5,1184.1,37136,1,2.101546e-06,5.846188e-17
petsc_breakdown,near_singular,5,backward_error,402.5,391.4,37136,1,2.101546e-06,5.846188e-17
petsc_breakdown,near_singular,5,copy_out,55.4,53.2,37136,1,2.101546e-06,5.846188e-17
lapack_direct,well_conditioned,9,total,812.9,701.3,117185,1,2.467162e-16,9.562004e-17
petsc_adapter,well_conditioned,9,total,3634.6,3616.6,25776,1,2.467162e-16,9.870455e-17
petsc_breakdown,well_conditioned,9,load,1386.7,1206.6,24171,1,2.467162e-16,9.870455e-17
petsc_breakdown,well_conditioned,9,factor_solve,1744.7,1501.5,24171,1,2.467162e-16,9.870455e-17
petsc_breakdown,well_conditioned,9,backward_error,491.8,423.1,24171,1,2.467162e-16,9.870455e-17
petsc_breakdown,well_conditioned,9,copy_out,83.7,72.5,24171,1,2.467162e-16,9.870455e-17
lapack_direct,badly_row_scaled,9,total,821.6,821.3,116299,1,2.467162e-16,9.562004e-17
petsc_adapter,badly_row_scaled,9,total,3634.0,3621.5,22303,1,2.467162e-16,9.870455e-17
petsc_breakdown,badly_row_scaled,9,load,1453.2,1447.3,25613,1,2.467162e-16,9.870455e-17
petsc_breakdown,badly_row_scaled,9,factor_solve,1824.0,1801.3,25613,1,2.467162e-16,9.870455e-17
petsc_breakdown,badly_row_scaled,9,backward_error,507.5,501.6,25613,1,2.467162e-16,9.870455e-17
petsc_breakdown,badly_row_scaled,9,copy_out,87.8,86.9,25613,1,2.467162e-16,9.870455e-17
lapack_direct,near_singular,9,total,836.7,830.1,113318,1,4.452854e-06,1.573108e-16
petsc_adapter,near_singular,9,total,3655.0,3581.9,27521,1,4.452854e-06,1.480572e-16
petsc_breakdown,near_singular,9,load,1468.2,1460.9,25566,1,4.452854e-06,1.480572e-16
petsc_breakdown,near_singular,9,factor_solve,1824.2,1823.7,25566,1,4.452854e-06,1.480572e-16
petsc_breakdown,near_singular,9,backward_error,509.9,506.4,25566,1,4.452854e-06,1.480572e-16
petsc_breakdown,near_singular,9,copy_out,88.5,88.2,25566,1,4.452854e-06,1.480572e-16
lapack_direct,well_conditioned,12,total,1395.6,1378.5,66895,1,3.172066e-16,1.362157e-16
petsc_adapter,well_conditioned,12,total,4880.7,4448.5,18583,1,3.172066e-16,1.257376e-16
petsc_breakdown,well_conditioned,12,load,1791.6,1747.3,23975,1,3.172066e-16,1.257376e-16
petsc_breakdown,well_conditioned,12,factor_solve,1916.3,1849.5,23975,1,3.172066e-16,1.257376e-16
petsc_breakdown,well_conditioned,12,backward_error,468.4,450.2,23975,1,3.172066e-16,1.257376e-16
petsc_breakdown,well_conditioned,12,copy_out,86.4,84.2,23975,1,3.172066e-16,1.257376e-16
lapack_direct,badly_row_scaled,12,total,1164.1,1123.2,71252,1,3.172066e-16,9.233856e-17
petsc_adapter,badly_row_scaled,12,total,4005.9,3955.5,24545,1,3.172066e-16,1.676501e-16
petsc_breakdown,badly_row_scaled,12,load,1828.5,1754.4,23425,1,3.172066e-16,1.676501e-16
petsc_breakdown,badly_row_scaled,12,factor_solve,1941.7,1881.4,23425,1,3.172066e-16,1.676501e-16
petsc_breakdown,badly_row_scaled,12,backward_error,484.7,458.7,23425,1,3.172066e-16,1.676501e-16
petsc_breakdown,badly_row_scaled,12,copy_out,88.5,84.7,23425,1,3.172066e-16,1.676501e-16
lapack_direct,near_singular,12,total,1294.0,1174.7,86423,1,7.247032e-06,1.101231e-16
petsc_adapter,near_singular,12,total,4601.2,4079.3,20332,1,7.247032e-06,1.258550e-16
petsc_breakdown,near_singular,12,load,2025.8,1845.1,19535,1,7.247032e-06,1.258550e-16
petsc_breakdown,near_singular,12,factor_solve,2170.4,1978.7,19535,1,7.247032e-06,1.258550e-16
petsc_breakdown,near_singular,12,backward_error,525.1,478.1,19535,1,7.247032e-06,1.258550e-16
petsc_breakdown,near_singular,12,copy_out,97.6,88.4,19535,1,7.247032e-06,1.258550e-16
lapack_direct,well_conditioned,16,total,1671.1,1662.0,57554,1,3.552714e-16,9.914282e-17
petsc_adapter,well_conditioned,16,total,5515.6,5414.0,17182,1,3.552714e-16,1.321904e-16
petsc_breakdown,well_conditioned,16,load,3176.8,3009.3,17259,1,3.552714e-16,1.321904e-16
petsc_breakdown,well_conditioned,16,factor_solve,2772.6,2616.3,17259,1,3.552714e-16,1.321904e-16
petsc_breakdown,well_conditioned,16,backward_error,603.1,567.9,17259,1,3.552714e-16,1.321904e-16
petsc_breakdown,well_conditioned,16,copy_out,123.5,117.4,17259,1,3.552714e-16,1.321904e-16
lapack_direct,badly_row_scaled,16,total,1733.4,1658.1,59920,1,3.552714e-16,9.914282e-17
petsc_adapter,badly_row_scaled,16,total,6174.5,6025.1,14318,1,3.552714e-16,1.321904e-16
petsc_breakdown,badly_row_scaled,16,load,2922.3,2800.1,14208,1,3.552714e-16,1.321904e-16
petsc_breakdown,badly_row_scaled,16,factor_solve,2560.1,2435.8,14208,1,3.552714e-16,1.321904e-16
petsc_breakdown,badly_row_scaled,16,backward_error,553.2,529.0,14208,1,3.552714e-16,1.321904e-16
petsc_breakdown,badly_row_scaled,16,copy_out,113.1,108.8,14208,1,3.552714e-16,1.321904e-16
lapack_direct,near_singular,16,total,2020.3,1731.5,58156,1,5.312397e-07,1.222308e-16
petsc_adapter,near_singular,16,total,5761.5,5553.3,15119,1,5.312397e-07,1.086496e-16
petsc_breakdown,near_singular,16,load,2684.7,2668.4,17925,1,5.312397e-07,1.086496e-16
petsc_breakdown,near_singular,16,factor_solve,2350.7,2328.4,17925,1,5.312397e-07,1.086496e-16
petsc_breakdown,near_singular,16,backward_error,509.8,502.2,17925,1,5.312397e-07,1.086496e-16
petsc_breakdown,near_singular,16,copy_out,104.9,103.6,17925,1,5.312397e-07,1.086496e-16
lapack_direct,well_conditioned,20,total,3191.2,3114.2,30873,1,3.062684e-16,1.317211e-16
petsc_adapter,well_conditioned,20,total,9279.2,9250.2,9666,1,3.062684e-16,1.221761e-16
petsc_breakdown,well_conditioned,20,load,4778.8,4727.5,10270,1,3.062684e-16,1.221761e-16
petsc_breakdown,well_conditioned,20,factor_solve,3771.3,3757.9,10270,1,3.062684e-16,1.221761e-16
petsc_breakdown,well_conditioned,20,backward_error,721.1,715.1,10270,1,3.062684e-16,1.221761e-16
petsc_breakdown,well_conditioned,20,copy_out,151.3,149.6,10270,1,3.062684e-16,1.221761e-16
lapack_direct,badly_row_scaled,20,total,3108.7,2962.1,32100,1,4.594026e-16,1.832641e-16
petsc_adapter,badly_row_scaled,20,total,9157.0,9092.1,10148,1,4.594026e-16,1.832641e-16
petsc_breakdown,badly_row_scaled,20,load,4764.0,4753.6,10521,1,4.594026e-16,1.832641e-16
petsc_breakdown,badly_row_scaled,20,factor_solve,3757.7,3741.7,10521,1,4.594026e-16,1.832641e-16
petsc_breakdown,badly_row_scaled,20,backward_error,707.9,707.2,10521,1,4.594026e-16,1.832641e-16
petsc_breakdown,badly_row_scaled,20,copy_out,151.8,150.6,10521,1,4.594026e-16,1.832641e-16
lapack_direct,near_singular,20,total,3113.7,3110.2,31374,1,4.051962e-06,1.765830e-16
petsc_adapter,near_singular,20,total,9094.2,8804.3,10511,1,4.051962e-06,1.527205e-16
petsc_breakdown,near_singular,20,load,3922.6,3860.7,10423,1,4.051962e-06,1.527205e-16
petsc_breakdown,near_singular,20,factor_solve,3057.8,3038.1,10423,1,4.051962e-06,1.527205e-16
petsc_breakdown,near_singular,20,backward_error,579.1,578.0,10423,1,4.051962e-06,1.527205e-16
petsc_breakdown,near_singular,20,copy_out,124.8,122.1,10423,1,4.051962e-06,1.527205e-16
lapack_direct,well_conditioned,25,total,4057.2,3915.4,25674,1,2.612289e-16,1.845135e-16
petsc_adapter,well_conditioned,25,total,11120.1,11058.5,8902,1,2.612289e-16,1.527008e-16
petsc_breakdown,well_conditioned,25,load,6883.6,6663.1,7626,1,2.612289e-16,1.527008e-16
petsc_breakdown,well_conditioned,25,factor_solve,4624.6,4494.2,7626,1,2.612289e-16,1.527008e-16
petsc_breakdown,well_conditioned,25,backward_error,826.9,801.9,7626,1,2.612289e-16,1.527008e-16
petsc_breakdown,well_conditioned,25,copy_out,175.5,171.1,7626,1,2.612289e-16,1.527008e-16
lapack_direct,badly_row_scaled,25,total,4754.6,4311.9,21018,1,5.224579e-16,1.972386e-16
petsc_adapter,badly_row_scaled,25,total,13275.7,11633.3,7282,1,5.224579e-16,2.036011e-16
petsc_breakdown,badly_row_scaled,25,load,6899.8,6221.5,7431,1,5.224579e-16,2.036011e-16
petsc_breakdown,badly_row_scaled,25,factor_solve,4648.9,4188.5,7431,1,5.224579e-16,2.036011e-16
petsc_breakdown,badly_row_scaled,25,backward_error,828.1,748.8,7431,1,5.224579e-16,2.036011e-16
petsc_breakdown,badly_row_scaled,25,copy_out,176.4,163.4,7431,1,5.224579e-16,2.036011e-16
lapack_direct,near_singular,25,total,3893.8,3867.1,26075,1,5.113089e-05,1.586475e-16
petsc_adapter,near_singular,25,total,11126.9,11060.1,8876,1,5.113089e-05,1.586475e-16
petsc_breakdown,near_singular,25,load,7106.2,6645.7,8948,1,5.113089e-05,1.586475e-16
petsc_breakdown,near_singular,25,factor_solve,4777.8,4454.5,8948,1,5.113089e-05,1.586475e-16
petsc_breakdown,near_singular,25,backward_error,856.4,796.8,8948,1,5.113089e-05,1.586475e-16
petsc_breakdown,near_singular,25,copy_out,182.0,170.2,8948,1,5.113089e-05,1.586475e-16
lapack_direct,well_conditioned,30,total,6480.4,6133.7,15399,1,5.693451e-16,2.234740e-16
petsc_adapter,well_conditioned,30,total,17576.7,17366.9,5572,1,5.693451e-16,2.549246e-16
petsc_breakdown,well_conditioned,30,load,9745.2,8329.6,5553,1,5.693451e-16,2.549246e-16
petsc_breakdown,well_conditioned,30,factor_solve,6121.2,5115.7,5553,1,5.693451e-16,2.549246e-16
petsc_breakdown,well_conditioned,30,backward_error,1026.8,870.7,5553,1,5.693451e-16,2.549246e-16
petsc_breakdown,well_conditioned,30,copy_out,235.9,197.7,5553,1,5.693451e-16,2.549246e-16
lapack_direct,badly_row_scaled,30,total,6474.0,5312.8,18802,1,5.693451e-16,2.354235e-16
petsc_adapter,badly_row_scaled,30,total,17367.0,17317.8,5508,1,5.693451e-16,2.549246e-16
petsc_breakdown,badly_row_scaled,30,load,10006.2,9175.1,5612,1,5.693451e-16,2.549246e-16
petsc_breakdown,badly_row_scaled,30,factor_solve,6285.8,5753.2,5612,1,5.693451e-16,2.549246e-16
petsc_breakdown,badly_row_scaled,30,backward_error,1057.2,969.5,5612,1,5.693451e-16,2.549246e-16
petsc_breakdown,badly_row_scaled,30,copy_out,226.8,209.0,5612,1,5.693451e-16,2.549246e-16
lapack_direct,near_singular,30,total,5967.5,5659.6,18744,1,1.326138e-05,1.966605e-16
petsc_adapter,near_singular,30,total,17383.9,16912.1,5625,1,1.326138e-05,1.355792e-16
petsc_breakdown,near_singular,30,load,9273.5,8927.9,6940,1,1.326138e-05,1.355792e-16
petsc_breakdown,near_singular,30,factor_solve,5798.2,5620.5,6940,1,1.326138e-05,1.355792e-16
petsc_breakdown,near_singular,30,backward_error,976.6,945.5,6940,1,1.326138e-05,1.355792e-16
petsc_breakdown,near_singular,30,copy_out,232.0,210.6,6940,1,1.326138e-05,1.355792e-16
lapack_direct,well_conditioned,40,total,11583.2,11002.3,8826,1,9.063045e-16,3.319098e-16
petsc_adapter,well_conditioned,40,total,28798.4,28201.2,3404,1,9.063045e-16,2.749803e-16
petsc_breakdown,well_conditioned,40,load,17261.9,17215.8,3422,1,9.063045e-16,2.749803e-16
petsc_breakdown,well_conditioned,40,factor_solve,10061.5,10002.4,3422,1,9.063045e-16,2.749803e-16
petsc_breakdown,well_conditioned,40,backward_error,1533.7,1527.3,3422,1,9.063045e-16,2.749803e-16
petsc_breakdown,well_conditioned,40,copy_out,307.7,304.4,3422,1,9.063045e-16,2.749803e-16
lapack_direct,badly_row_scaled,40,total,11563.7,11037.2,8920,1,7.250436e-16,3.990436e-16
petsc_adapter,badly_row_scaled,40,total,29260.2,22836.0,3472,1,7.250436e-16,3.437253e-16
petsc_breakdown,badly_row_scaled,40,load,14747.9,13887.7,4308,1,7.250436e-16,3.437253e-16
petsc_breakdown,badly_row_scaled,40,factor_solve,8231.6,7807.6,4308,1,7.250436e-16,3.437253e-16
petsc_breakdown,badly_row_scaled,40,backward_error,1307.1,1228.4,4308,1,7.250436e-16,3.437253e-16
petsc_breakdown,badly_row_scaled,40,copy_out,260.1,245.2,4308,1,7.250436e-16,3.437253e-16
lapack_direct,near_singular,40,total,9660.0,8912.8,10948,1,2.269855e-05,2.674576e-16
petsc_adapter,near_singular,40,total,26994.2,25509.3,3474,1,2.269855e-05,3.437206e-16
petsc_breakdown,near_singular,40,load,14180.1,13979.3,3439,1,2.269855e-05,3.437206e-16
petsc_breakdown,near_singular,40,factor_solve,7951.9,7815.4,3439,1,2.269855e-05,3.437206e-16
petsc_breakdown,near_singular,40,backward_error,1260.8,1243.5,3439,1,2.269855e-05,3.437206e-16
petsc_breakdown,near_singular,40,copy_out,254.2,247.0,3439,1,2.269855e-05,3.437206e-16
```

## Full CSV, run 3 (`build/petsc_on/benchmark_dense_solvers_run3_single_thread_blas.csv`), verbatim

Run 3 command: `OPENBLAS_NUM_THREADS=1 PETSC_OPTIONS="" build/petsc_on/benchmark_dense_solvers > build/petsc_on/benchmark_dense_solvers_run3_single_thread_blas.csv`, exit status 0.

```
solver,matrix_class,n,phase,ns_per_call_median,ns_per_call_min,calls_per_batch,accepted,max_relative_solution_error,backward_error
lapack_direct,well_conditioned,5,total,495.8,494.7,143939,1,1.586033e-16,7.307727e-17
petsc_adapter,well_conditioned,5,total,2849.1,2442.5,23778,1,1.586033e-16,5.846182e-17
petsc_breakdown,well_conditioned,5,load,575.2,546.7,38059,1,1.586033e-16,5.846182e-17
petsc_breakdown,well_conditioned,5,factor_solve,1134.4,1072.9,38059,1,1.586033e-16,5.846182e-17
petsc_breakdown,well_conditioned,5,backward_error,336.9,318.3,38059,1,1.586033e-16,5.846182e-17
petsc_breakdown,well_conditioned,5,copy_out,36.2,34.2,38059,1,1.586033e-16,5.846182e-17
lapack_direct,badly_row_scaled,5,total,322.1,306.8,307187,1,1.586033e-16,5.846182e-17
petsc_adapter,badly_row_scaled,5,total,2413.0,2341.5,39980,1,1.586033e-16,5.846182e-17
petsc_breakdown,badly_row_scaled,5,load,638.2,591.6,47712,1,1.586033e-16,5.846182e-17
petsc_breakdown,badly_row_scaled,5,factor_solve,1257.5,1163.3,47712,1,1.586033e-16,5.846182e-17
petsc_breakdown,badly_row_scaled,5,backward_error,375.7,346.3,47712,1,1.586033e-16,5.846182e-17
petsc_breakdown,badly_row_scaled,5,copy_out,40.1,37.3,47712,1,1.586033e-16,5.846182e-17
lapack_direct,near_singular,5,total,303.3,299.2,309780,1,2.101546e-06,5.115415e-17
petsc_adapter,near_singular,5,total,2152.7,1994.4,40555,1,2.101546e-06,5.846188e-17
petsc_breakdown,near_singular,5,load,546.5,543.8,47966,1,2.101546e-06,5.846188e-17
petsc_breakdown,near_singular,5,factor_solve,1079.0,1071.3,47966,1,2.101546e-06,5.846188e-17
petsc_breakdown,near_singular,5,backward_error,322.4,320.3,47966,1,2.101546e-06,5.846188e-17
petsc_breakdown,near_singular,5,copy_out,34.2,33.9,47966,1,2.101546e-06,5.846188e-17
lapack_direct,well_conditioned,9,total,692.3,668.3,132629,1,2.467162e-16,9.562004e-17
petsc_adapter,well_conditioned,9,total,3271.6,3158.4,25240,1,2.467162e-16,9.870455e-17
petsc_breakdown,well_conditioned,9,load,1358.7,1357.6,26325,1,2.467162e-16,9.870455e-17
petsc_breakdown,well_conditioned,9,factor_solve,1744.9,1739.8,26325,1,2.467162e-16,9.870455e-17
petsc_breakdown,well_conditioned,9,backward_error,449.4,447.0,26325,1,2.467162e-16,9.870455e-17
petsc_breakdown,well_conditioned,9,copy_out,66.1,65.6,26325,1,2.467162e-16,9.870455e-17
lapack_direct,badly_row_scaled,9,total,840.6,820.9,113824,1,2.467162e-16,9.562004e-17
petsc_adapter,badly_row_scaled,9,total,3698.6,3631.8,26304,1,2.467162e-16,9.870455e-17
petsc_breakdown,badly_row_scaled,9,load,1386.6,1369.6,26825,1,2.467162e-16,9.870455e-17
petsc_breakdown,badly_row_scaled,9,factor_solve,1771.8,1754.1,26825,1,2.467162e-16,9.870455e-17
petsc_breakdown,badly_row_scaled,9,backward_error,457.3,451.0,26825,1,2.467162e-16,9.870455e-17
petsc_breakdown,badly_row_scaled,9,copy_out,66.4,65.9,26825,1,2.467162e-16,9.870455e-17
lapack_direct,near_singular,9,total,836.3,831.5,119085,1,4.452854e-06,1.573108e-16
petsc_adapter,near_singular,9,total,3756.0,3638.0,26098,1,4.452854e-06,1.480572e-16
petsc_breakdown,near_singular,9,load,1397.2,1378.2,25522,1,4.452854e-06,1.480572e-16
petsc_breakdown,near_singular,9,factor_solve,1787.6,1763.9,25522,1,4.452854e-06,1.480572e-16
petsc_breakdown,near_singular,9,backward_error,461.7,456.5,25522,1,4.452854e-06,1.480572e-16
petsc_breakdown,near_singular,9,copy_out,67.3,67.0,25522,1,4.452854e-06,1.480572e-16
lapack_direct,well_conditioned,12,total,1261.1,1110.4,62584,1,3.172066e-16,1.362157e-16
petsc_adapter,well_conditioned,12,total,4500.5,4202.1,23021,1,3.172066e-16,1.257376e-16
petsc_breakdown,well_conditioned,12,load,2050.3,1924.7,19695,1,3.172066e-16,1.257376e-16
petsc_breakdown,well_conditioned,12,factor_solve,2264.6,2132.1,19695,1,3.172066e-16,1.257376e-16
petsc_breakdown,well_conditioned,12,backward_error,503.7,473.6,19695,1,3.172066e-16,1.257376e-16
petsc_breakdown,well_conditioned,12,copy_out,78.7,73.9,19695,1,3.172066e-16,1.257376e-16
lapack_direct,badly_row_scaled,12,total,1280.5,1160.6,72226,1,3.172066e-16,9.233856e-17
petsc_adapter,badly_row_scaled,12,total,4904.8,4882.4,20078,1,3.172066e-16,1.676501e-16
petsc_breakdown,badly_row_scaled,12,load,2063.6,2042.7,20169,1,3.172066e-16,1.676501e-16
petsc_breakdown,badly_row_scaled,12,factor_solve,2265.2,2246.6,20169,1,3.172066e-16,1.676501e-16
petsc_breakdown,badly_row_scaled,12,backward_error,505.1,502.2,20169,1,3.172066e-16,1.676501e-16
petsc_breakdown,badly_row_scaled,12,copy_out,78.9,78.5,20169,1,3.172066e-16,1.676501e-16
lapack_direct,near_singular,12,total,1358.8,1338.7,72216,1,7.247032e-06,1.101231e-16
petsc_adapter,near_singular,12,total,4860.0,4856.0,20412,1,7.247032e-06,1.258550e-16
petsc_breakdown,near_singular,12,load,1960.3,1661.2,20123,1,7.247032e-06,1.258550e-16
petsc_breakdown,near_singular,12,factor_solve,2153.4,1835.2,20123,1,7.247032e-06,1.258550e-16
petsc_breakdown,near_singular,12,backward_error,480.7,408.9,20123,1,7.247032e-06,1.258550e-16
petsc_breakdown,near_singular,12,copy_out,75.7,63.5,20123,1,7.247032e-06,1.258550e-16
lapack_direct,well_conditioned,16,total,2058.4,2057.1,47711,1,3.552714e-16,9.914282e-17
petsc_adapter,well_conditioned,16,total,6681.4,6520.7,14076,1,3.552714e-16,1.321904e-16
petsc_breakdown,well_conditioned,16,load,3167.1,3090.9,14606,1,3.552714e-16,1.321904e-16
petsc_breakdown,well_conditioned,16,factor_solve,2835.1,2760.4,14606,1,3.552714e-16,1.321904e-16
petsc_breakdown,well_conditioned,16,backward_error,569.0,556.9,14606,1,3.552714e-16,1.321904e-16
petsc_breakdown,well_conditioned,16,copy_out,103.9,101.7,14606,1,3.552714e-16,1.321904e-16
lapack_direct,badly_row_scaled,16,total,2054.4,1853.1,47307,1,3.552714e-16,9.914282e-17
petsc_adapter,badly_row_scaled,16,total,6690.2,6639.7,14083,1,3.552714e-16,1.321904e-16
petsc_breakdown,badly_row_scaled,16,load,3164.0,3160.5,14791,1,3.552714e-16,1.321904e-16
petsc_breakdown,badly_row_scaled,16,factor_solve,2821.2,2817.2,14791,1,3.552714e-16,1.321904e-16
petsc_breakdown,badly_row_scaled,16,backward_error,573.9,570.8,14791,1,3.552714e-16,1.321904e-16
petsc_breakdown,badly_row_scaled,16,copy_out,104.2,103.9,14791,1,3.552714e-16,1.321904e-16
lapack_direct,near_singular,16,total,2079.6,2039.8,48256,1,5.312397e-07,1.222308e-16
petsc_adapter,near_singular,16,total,6753.8,5850.5,14335,1,5.312397e-07,1.086496e-16
petsc_breakdown,near_singular,16,load,2787.7,2688.2,14738,1,5.312397e-07,1.086496e-16
petsc_breakdown,near_singular,16,factor_solve,2489.4,2385.9,14738,1,5.312397e-07,1.086496e-16
petsc_breakdown,near_singular,16,backward_error,509.8,486.1,14738,1,5.312397e-07,1.086496e-16
petsc_breakdown,near_singular,16,copy_out,91.1,87.2,14738,1,5.312397e-07,1.086496e-16
lapack_direct,well_conditioned,20,total,2613.0,2536.7,38546,1,3.062684e-16,1.317211e-16
petsc_adapter,well_conditioned,20,total,9170.1,8227.4,12657,1,3.062684e-16,1.221761e-16
petsc_breakdown,well_conditioned,20,load,4511.0,4340.0,10663,1,3.062684e-16,1.221761e-16
petsc_breakdown,well_conditioned,20,factor_solve,3663.2,3516.6,10663,1,3.062684e-16,1.221761e-16
petsc_breakdown,well_conditioned,20,backward_error,641.1,613.4,10663,1,3.062684e-16,1.221761e-16
petsc_breakdown,well_conditioned,20,copy_out,121.6,115.2,10663,1,3.062684e-16,1.221761e-16
lapack_direct,badly_row_scaled,20,total,3160.7,3114.6,31586,1,4.594026e-16,1.832641e-16
petsc_adapter,badly_row_scaled,20,total,9132.4,7789.5,10086,1,4.594026e-16,1.832641e-16
petsc_breakdown,badly_row_scaled,20,load,4147.8,4036.5,13054,1,4.594026e-16,1.832641e-16
petsc_breakdown,badly_row_scaled,20,factor_solve,3382.5,3279.6,13054,1,4.594026e-16,1.832641e-16
petsc_breakdown,badly_row_scaled,20,backward_error,586.9,572.0,13054,1,4.594026e-16,1.832641e-16
petsc_breakdown,badly_row_scaled,20,copy_out,110.1,107.0,13054,1,4.594026e-16,1.832641e-16
lapack_direct,near_singular,20,total,3065.2,2663.3,32149,1,4.051962e-06,1.765830e-16
petsc_adapter,near_singular,20,total,9123.7,8903.4,9354,1,4.051962e-06,1.527205e-16
petsc_breakdown,near_singular,20,load,4611.1,4527.5,10884,1,4.051962e-06,1.527205e-16
petsc_breakdown,near_singular,20,factor_solve,3754.7,3680.1,10884,1,4.051962e-06,1.527205e-16
petsc_breakdown,near_singular,20,backward_error,652.4,641.4,10884,1,4.051962e-06,1.527205e-16
petsc_breakdown,near_singular,20,copy_out,122.0,119.6,10884,1,4.051962e-06,1.527205e-16
lapack_direct,well_conditioned,25,total,4877.0,4834.0,18542,1,2.612289e-16,1.845135e-16
petsc_adapter,well_conditioned,25,total,12751.7,12381.3,7414,1,2.612289e-16,1.527008e-16
petsc_breakdown,well_conditioned,25,load,7054.0,6942.7,7446,1,2.612289e-16,1.527008e-16
petsc_breakdown,well_conditioned,25,factor_solve,4999.3,4945.4,7446,1,2.612289e-16,1.527008e-16
petsc_breakdown,well_conditioned,25,backward_error,831.9,820.8,7446,1,2.612289e-16,1.527008e-16
petsc_breakdown,well_conditioned,25,copy_out,162.8,161.1,7446,1,2.612289e-16,1.527008e-16
lapack_direct,badly_row_scaled,25,total,4997.0,4917.2,20340,1,5.224579e-16,1.972386e-16
petsc_adapter,badly_row_scaled,25,total,13548.4,13180.7,7338,1,5.224579e-16,2.036011e-16
petsc_breakdown,badly_row_scaled,25,load,6974.7,6943.6,7192,1,5.224579e-16,2.036011e-16
petsc_breakdown,badly_row_scaled,25,factor_solve,5019.0,4965.5,7192,1,5.224579e-16,2.036011e-16
petsc_breakdown,badly_row_scaled,25,backward_error,826.7,825.3,7192,1,5.224579e-16,2.036011e-16
petsc_breakdown,badly_row_scaled,25,copy_out,162.6,160.7,7192,1,5.224579e-16,2.036011e-16
lapack_direct,near_singular,25,total,3852.9,3773.9,20823,1,5.113089e-05,1.586475e-16
petsc_adapter,near_singular,25,total,10375.1,10270.9,9624,1,5.113089e-05,1.586475e-16
petsc_breakdown,near_singular,25,load,5749.9,5574.1,9516,1,5.113089e-05,1.586475e-16
petsc_breakdown,near_singular,25,factor_solve,4026.3,3912.8,9516,1,5.113089e-05,1.586475e-16
petsc_breakdown,near_singular,25,backward_error,674.1,654.3,9516,1,5.113089e-05,1.586475e-16
petsc_breakdown,near_singular,25,copy_out,132.9,128.1,9516,1,5.113089e-05,1.586475e-16
lapack_direct,well_conditioned,30,total,5829.8,5458.7,15393,1,5.693451e-16,2.234740e-16
petsc_adapter,well_conditioned,30,total,13591.2,13527.5,6840,1,5.693451e-16,2.549246e-16
petsc_breakdown,well_conditioned,30,load,7578.9,7528.3,7189,1,5.693451e-16,2.549246e-16
petsc_breakdown,well_conditioned,30,factor_solve,5015.9,4980.9,7189,1,5.693451e-16,2.549246e-16
petsc_breakdown,well_conditioned,30,backward_error,803.6,794.0,7189,1,5.693451e-16,2.549246e-16
petsc_breakdown,well_conditioned,30,copy_out,160.5,159.5,7189,1,5.693451e-16,2.549246e-16
lapack_direct,badly_row_scaled,30,total,5389.2,5191.7,19166,1,5.693451e-16,2.354235e-16
petsc_adapter,badly_row_scaled,30,total,13882.4,13721.1,7213,1,5.693451e-16,2.549246e-16
petsc_breakdown,badly_row_scaled,30,load,8061.6,7533.6,5784,1,5.693451e-16,2.549246e-16
petsc_breakdown,badly_row_scaled,30,factor_solve,5333.2,4982.4,5784,1,5.693451e-16,2.549246e-16
petsc_breakdown,badly_row_scaled,30,backward_error,857.0,806.3,5784,1,5.693451e-16,2.549246e-16
petsc_breakdown,badly_row_scaled,30,copy_out,169.1,161.0,5784,1,5.693451e-16,2.549246e-16
lapack_direct,near_singular,30,total,6478.3,5486.9,18844,1,1.326138e-05,1.966605e-16
petsc_adapter,near_singular,30,total,15559.7,13417.8,5907,1,1.326138e-05,1.355792e-16
petsc_breakdown,near_singular,30,load,8709.4,8360.9,5866,1,1.326138e-05,1.355792e-16
petsc_breakdown,near_singular,30,factor_solve,5865.6,5606.4,5866,1,1.326138e-05,1.355792e-16
petsc_breakdown,near_singular,30,backward_error,936.2,891.9,5866,1,1.326138e-05,1.355792e-16
petsc_breakdown,near_singular,30,copy_out,184.6,177.0,5866,1,1.326138e-05,1.355792e-16
lapack_direct,well_conditioned,40,total,11437.3,9504.5,8886,1,9.063045e-16,3.319098e-16
petsc_adapter,well_conditioned,40,total,25230.7,24599.2,4151,1,9.063045e-16,2.749803e-16
petsc_breakdown,well_conditioned,40,load,13998.6,13654.1,4317,1,9.063045e-16,2.749803e-16
petsc_breakdown,well_conditioned,40,factor_solve,8336.9,8118.1,4317,1,9.063045e-16,2.749803e-16
petsc_breakdown,well_conditioned,40,backward_error,1251.4,1229.1,4317,1,9.063045e-16,2.749803e-16
petsc_breakdown,well_conditioned,40,copy_out,251.8,248.6,4317,1,9.063045e-16,2.749803e-16
lapack_direct,badly_row_scaled,40,total,9695.8,9190.5,8853,1,7.250436e-16,3.990436e-16
petsc_adapter,badly_row_scaled,40,total,26312.4,23416.4,3455,1,7.250436e-16,3.437253e-16
petsc_breakdown,badly_row_scaled,40,load,15581.6,14938.1,3948,1,7.250436e-16,3.437253e-16
petsc_breakdown,badly_row_scaled,40,factor_solve,9589.8,9167.6,3948,1,7.250436e-16,3.437253e-16
petsc_breakdown,badly_row_scaled,40,backward_error,1413.1,1364.2,3948,1,7.250436e-16,3.437253e-16
petsc_breakdown,badly_row_scaled,40,copy_out,287.3,252.6,3948,1,7.250436e-16,3.437253e-16
lapack_direct,near_singular,40,total,11761.3,11070.6,10992,1,2.269855e-05,2.674576e-16
petsc_adapter,near_singular,40,total,29777.5,28965.9,3278,1,2.269855e-05,3.437206e-16
petsc_breakdown,near_singular,40,load,16268.5,15994.4,3459,1,2.269855e-05,3.437206e-16
petsc_breakdown,near_singular,40,factor_solve,10059.0,9994.5,3459,1,2.269855e-05,3.437206e-16
petsc_breakdown,near_singular,40,backward_error,1482.6,1472.7,3459,1,2.269855e-05,3.437206e-16
petsc_breakdown,near_singular,40,copy_out,306.6,267.1,3459,1,2.269855e-05,3.437206e-16
```

## Summary tables

### Summary table, `well_conditioned` (run 1 medians; last two columns are run 3 medians)

| n | lapack_direct median ns | petsc_adapter median ns | petsc_adapter / lapack_direct | load ns | factor_solve ns | backward_error ns | copy_out ns | lapack_direct median ns, run 3 | petsc_adapter median ns, run 3 |
|---|---|---|---|---|---|---|---|---|---|
| 5 | 496.1 | 1779.6 | 3.59 | 488.5 | 967.3 | 287.3 | 31.1 | 495.8 | 2849.1 |
| 9 | 848.1 | 3720.2 | 4.39 | 1409.1 | 1808.1 | 466.4 | 68.7 | 692.3 | 3271.6 |
| 12 | 1359.9 | 4853.7 | 3.57 | 2015.7 | 2224.6 | 499.3 | 77.5 | 1261.1 | 4500.5 |
| 16 | 1670.5 | 5378.3 | 3.22 | 2551.5 | 2286.2 | 467.8 | 81.9 | 2058.4 | 6681.4 |
| 20 | 3132.9 | 9439.4 | 3.01 | 4851.1 | 3922.9 | 717.3 | 129.2 | 2613.0 | 9170.1 |
| 25 | 4941.8 | 12973.4 | 2.63 | 7011.2 | 4942.4 | 849.9 | 159.7 | 4877.0 | 12751.7 |
| 30 | 5297.6 | 13892.9 | 2.62 | 9134.2 | 6008.5 | 954.6 | 193.0 | 5829.8 | 13591.2 |
| 40 | 10729.0 | 28328.1 | 2.64 | 16352.7 | 10019.1 | 1479.1 | 277.5 | 11437.3 | 25230.7 |

### Summary table, `badly_row_scaled` (run 1 medians; last two columns are run 3 medians)

| n | lapack_direct median ns | petsc_adapter median ns | petsc_adapter / lapack_direct | load ns | factor_solve ns | backward_error ns | copy_out ns | lapack_direct median ns, run 3 | petsc_adapter median ns, run 3 |
|---|---|---|---|---|---|---|---|---|---|
| 5 | 278.8 | 1909.7 | 6.85 | 544.2 | 1073.8 | 317.9 | 34.2 | 322.1 | 2413.0 |
| 9 | 859.0 | 3718.6 | 4.33 | 1412.2 | 1816.8 | 464.9 | 69.1 | 840.6 | 3698.6 |
| 12 | 1359.0 | 4797.9 | 3.53 | 1710.8 | 1885.3 | 422.6 | 65.8 | 1280.5 | 4904.8 |
| 16 | 1659.2 | 5474.2 | 3.30 | 3159.6 | 2838.4 | 582.9 | 103.4 | 2054.4 | 6690.2 |
| 20 | 3214.6 | 9140.3 | 2.84 | 4646.0 | 3765.4 | 688.9 | 124.1 | 3160.7 | 9132.4 |
| 25 | 3903.3 | 10753.3 | 2.75 | 5662.1 | 3971.2 | 681.7 | 129.2 | 4997.0 | 13548.4 |
| 30 | 5757.1 | 14874.6 | 2.58 | 9488.6 | 6317.1 | 1005.2 | 196.1 | 5389.2 | 13882.4 |
| 40 | 12286.4 | 30126.2 | 2.45 | 17506.9 | 10771.1 | 1600.1 | 297.0 | 9695.8 | 26312.4 |

## `near_singular`: acceptance, solution error, backward error

Values are from run 1. For `petsc_breakdown` the row shown is the `backward_error` phase, which is the phase that produces the acceptance decision; its `accepted` flag covers the whole four-phase call. All three runs give the same `accepted` values and the same `max_relative_solution_error` values.

| n | solver | accepted | max_relative_solution_error | backward_error |
|---|---|---|---|---|
| 5 | lapack_direct | 1 | 2.101546e-06 | 5.115415e-17 |
| 5 | petsc_adapter | 1 | 2.101546e-06 | 5.846188e-17 |
| 5 | petsc_breakdown | 1 | 2.101546e-06 | 5.846188e-17 |
| 9 | lapack_direct | 1 | 4.452854e-06 | 1.573108e-16 |
| 9 | petsc_adapter | 1 | 4.452854e-06 | 1.480572e-16 |
| 9 | petsc_breakdown | 1 | 4.452854e-06 | 1.480572e-16 |
| 12 | lapack_direct | 1 | 7.247032e-06 | 1.101231e-16 |
| 12 | petsc_adapter | 1 | 7.247032e-06 | 1.258550e-16 |
| 12 | petsc_breakdown | 1 | 7.247032e-06 | 1.258550e-16 |
| 16 | lapack_direct | 1 | 5.312397e-07 | 1.222308e-16 |
| 16 | petsc_adapter | 1 | 5.312397e-07 | 1.086496e-16 |
| 16 | petsc_breakdown | 1 | 5.312397e-07 | 1.086496e-16 |
| 20 | lapack_direct | 1 | 4.051962e-06 | 1.765830e-16 |
| 20 | petsc_adapter | 1 | 4.051962e-06 | 1.527205e-16 |
| 20 | petsc_breakdown | 1 | 4.051962e-06 | 1.527205e-16 |
| 25 | lapack_direct | 1 | 5.113089e-05 | 1.586475e-16 |
| 25 | petsc_adapter | 1 | 5.113089e-05 | 1.586475e-16 |
| 25 | petsc_breakdown | 1 | 5.113089e-05 | 1.586475e-16 |
| 30 | lapack_direct | 1 | 1.326138e-05 | 1.966605e-16 |
| 30 | petsc_adapter | 1 | 1.326138e-05 | 1.355792e-16 |
| 30 | petsc_breakdown | 1 | 1.326138e-05 | 1.355792e-16 |
| 40 | lapack_direct | 1 | 2.269855e-05 | 2.674576e-16 |
| 40 | petsc_adapter | 1 | 2.269855e-05 | 3.437206e-16 |
| 40 | petsc_breakdown | 1 | 2.269855e-05 | 3.437206e-16 |

## Run-to-run variation

Largest relative difference between run 1 and run 2 medians, over all 144 rows: **1.7042** (170.4 %) at `petsc_breakdown,well_conditioned,5,copy_out` (run 1 31.1 ns, run 2 84.1 ns).

Largest relative difference restricted to `total` rows: **0.8759** (87.6 %) at `petsc_adapter,well_conditioned,5,total` (run 1 1779.6 ns, run 2 3338.4 ns).

Per solver, largest run 1 vs run 2 relative difference in medians:

| solver | largest relative difference | row |
|---|---|---|
| lapack_direct | 0.2669 | `lapack_direct,badly_row_scaled,5,total` (run 1 278.8 ns, run 2 353.2 ns) |
| petsc_adapter | 0.8759 | `petsc_adapter,well_conditioned,5,total` (run 1 1779.6 ns, run 2 3338.4 ns) |
| petsc_breakdown | 1.7042 | `petsc_breakdown,well_conditioned,5,copy_out` (run 1 31.1 ns, run 2 84.1 ns) |

Run 1 vs run 3 (single-threaded BLAS), `total` rows: largest relative difference for `lapack_direct` **0.2802** at `lapack_direct,badly_row_scaled,25,total` (run 1 3903.3 ns, run 3 4997.0 ns).
Run 1 vs run 3, `total` rows: largest relative difference for `petsc_adapter` **0.6010** at `petsc_adapter,well_conditioned,5,total` (run 1 1779.6 ns, run 3 2849.1 ns).

## Observations

1. Every row in all three runs reports `accepted=1`; no row has a negative `backward_error`. All three runs exited 0. Over `well_conditioned` and `badly_row_scaled` in run 1, the largest `max_relative_solution_error` is 9.063e-16 and the largest `backward_error` is 3.990e-16. Over `near_singular`, the largest `max_relative_solution_error` is 5.113e-05 and the largest `backward_error` is 3.437e-16.

2. `petsc_adapter` medians are larger than `lapack_direct` medians at every size in both required classes. The ratio `petsc_adapter / lapack_direct` ranges from 2.62 to 4.39 for `well_conditioned` and from 2.45 to 6.85 for `badly_row_scaled` (run 1). For n <= 16 the ratio is between 3.22 and 6.85; for n >= 25 it is between 2.45 and 2.75.

3. In the breakdown, `factor_solve` is the largest phase at n = 5, 9, and 12, and `load` is the largest phase at n = 16, 20, 25, 30, and 40, in both `well_conditioned` and `badly_row_scaled` (run 1). `copy_out` is the smallest phase at every size.

4. The sum of the four `petsc_breakdown` phase medians and the `petsc_adapter` total median agree to within 22.1 % at every size in both classes (run 1); the largest deviation is +22.1 % at `badly_row_scaled` n=16 (phase sum 6684.3 ns, adapter total 5474.2 ns).

5. Comparison with the profiling report at n = 9, the size the task specification attributes to the profiled run (`phase1b_task1_dense_solver_benchmark.md` line 9). The profiling report gives `cl1_seconds_per_call = 1.36e-05 s`, i.e. 13600 ns (`phase1b_profiling_report.md` line 43). Run 1 `lapack_direct` median at n = 9 is 848.1 ns (6.2 % of 13600 ns) and `petsc_adapter` median is 3720.2 ns (27.4 % of 13600 ns) for `well_conditioned`.

6. Comparison with the profiling report at n = 9, the size the task specification attributes to the profiled run (`phase1b_task1_dense_solver_benchmark.md` line 9). The profiling report gives `cl1_seconds_per_call = 1.36e-05 s`, i.e. 13600 ns (`phase1b_profiling_report.md` line 43). Run 1 `lapack_direct` median at n = 9 is 859.0 ns (6.3 % of 13600 ns) and `petsc_adapter` median is 3718.6 ns (27.3 % of 13600 ns) for `badly_row_scaled`.


7. The profiling report lists `petsc_kspsolve_seconds_per_call = 2.94e-06 s` (2940 ns) and a `PCSetUp` maximum of 2.2364e-04 s over 25 calls (8945.6 ns per call). The measured `factor_solve` phase (`KSPSetOperators` + `KSPSolve`, with the factorization already triggered by the warm-up calls) is 1808.1 ns and 1816.8 ns at n = 9 for `well_conditioned` and `badly_row_scaled`. The profiling-report figures come from a different binary and a different run, and are maxima divided by counts.

8. Run 3 sets `OPENBLAS_NUM_THREADS=1`. The largest run 1 vs run 3 relative difference in `total` medians is 0.2802 for `lapack_direct` and 0.6010 for `petsc_adapter`; the signs of the differences are not consistent across sizes (see the run-3 columns in the summary tables).

9. The largest run 1 vs run 2 relative difference in medians is 1.7042, at `petsc_breakdown,well_conditioned,5,copy_out`, whose run-1 median is 31.1 ns. The largest difference among `total` rows is 0.8759. Among rows whose run-1 median exceeds 1000 ns the largest relative difference is 0.8759 at `petsc_adapter,well_conditioned,5,total` (1779.6 vs 3338.4 ns); among rows whose run-1 median exceeds 5000 ns it is 0.2652 at `petsc_adapter,well_conditioned,30,total` (13892.9 vs 17576.7 ns).

10. `calls_per_batch` is not identical between runs: 0 of 144 rows have the same value in run 1 and run 2 (it is fixed from each run's warm-up timing). For example `petsc_adapter,well_conditioned,5,total` used 23013 calls per batch in run 1 and 24497 in run 2. The reported medians are per-call values.

