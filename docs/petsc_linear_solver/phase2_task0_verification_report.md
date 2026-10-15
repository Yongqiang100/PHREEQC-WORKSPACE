# Phase 2 Task 0: PhreeqcRM and Parallel-Environment Verification Report

Date: 2026-10-11. Specification: `docs/petsc_linear_solver/implementation_tasks/phase2_task0_phreeqcrm_verification.md`.
No source file in either repository was modified. Files created: this report;
`~/phreeqc3/build/phase2/phase1_phreeqc_core.patch` (B2, reviewer-approved);
`~/phreeqcrm/build-baseline-openmp/`, `~/phreeqcrm/build-baseline-mpi/` (C1, C2);
`~/phreeqcrm_phase2_baseline/openmp/Tests/`, `~/phreeqcrm_phase2_baseline/mpi/Tests/` (C1, C2 copies).

## A. Environment

### A1. PhreeqcRM tree

```
$ git -C ~/phreeqcrm describe --tags
v3.9.0
$ git -C ~/phreeqcrm rev-parse HEAD
90b31ed8f2080dd957af6cd75edb9ce099d0914b
$ git -C ~/phreeqcrm status --short
(empty — confirmed)
```

CONFIRMED: the tree is at tag v3.9.0 and is clean.

### A2. MPI

```
$ module avail 2>&1 | grep -i -E "mpi|openmpi|mpich|intel"
eigen/5.0.1    metis/5.1.0        openmpi/5.0.10
$ command -v mpicxx mpicc mpiexec mpirun
/usr/bin/mpicxx
/usr/bin/mpicc
/usr/bin/mpiexec
/usr/bin/mpirun
```

`mpicxx --version`, first line:

```
g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0
```

`mpiexec --version` prints no version string; its complete output (exit status 0) is:

```
--------------------------------------------------------------------------
Sorry!  You were supposed to get help about:
    version
But I couldn't open the help file:
    /usr/lib/x86_64-linux-gnu/pmix2/share/pmix/help-schizo-ompi.txt: No such file or directory
    /usr/lib/x86_64-linux-gnu/prrte3/share/prte/help-schizo-ompi.txt: No such file or directory.
Sorry!
--------------------------------------------------------------------------
```

The Open MPI version was taken from the package manager instead: `openmpi-bin 5.0.10-1`,
`libopenmpi-dev 5.0.10-1`, `libopenmpi40 5.0.10-1` (`dpkg -l | grep -i openmpi`).

`mpicxx -show` (succeeded; `--showme` not needed):

```
g++ -I/usr/lib/x86_64-linux-gnu/openmpi/include -I/usr/lib/x86_64-linux-gnu/openmpi/include/openmpi -L/usr/lib/x86_64-linux-gnu/openmpi/lib -lmpi
```

Underlying compiler: g++ 15.2.0 (Ubuntu). MPI: Open MPI 5.0.10 (system install, not the
`openmpi/5.0.10` module path; the tools resolve to `/usr/bin/*`).

### A3. PETSc installations

`ls /opt/petsc/` shows two directories: `3.20.6` and `3.26.0`.

**/opt/petsc/3.20.6 — NOT FOUND.** The directory is empty:

```
$ ls -la /opt/petsc/3.20.6/
total 8
drwxrwxr-x 2 thmc thmc 4096 Oct 11 09:15 .
drwxrwxr-x 4 thmc thmc 4096 Oct 11 09:38 ..
```

`include/petscversion.h`, `include/petscconf.h`, and `lib/petsc/conf/petscvariables` do not
exist (looked directly under `/opt/petsc/3.20.6/`; `ls` reports "No such file or directory").
There is also no `petsc/3.20.6` module: `module show petsc/3.20.6` reports
`Lmod has detected the following error: Failed to find the following module(s): "petsc/3.20.6"`.

**/opt/petsc/3.26.0 — serial (mpiuni) build.**

Version, `/opt/petsc/3.26.0/include/petscversion.h:7-10`:

```
#define PETSC_VERSION_RELEASE    1
#define PETSC_VERSION_MAJOR      3
#define PETSC_VERSION_MINOR      26
#define PETSC_VERSION_SUBMINOR   0
```

`/opt/petsc/3.26.0/include/petscconf.h:75`:

```
#define PETSC_HAVE_MPIUNI 1
```

`PETSC_USE_64BIT_INDICES`: **not defined** — `grep -c "PETSC_USE_64BIT_INDICES" petscconf.h`
returns 0 (whole file searched).

`/opt/petsc/3.26.0/lib/petsc/conf/petscvariables`:

```
8:MPICXX_SHOW = Unavailable
43:BLASLAPACK_LIB = -Wl,-rpath,/usr/lib/x86_64-linux-gnu -L/usr/lib/x86_64-linux-gnu -lopenblas
49:MPI_LIB = 
50:MPI_INCLUDE = 
72:CONFIGURE_OPTIONS = --prefix=/opt/petsc/3.26.0 --with-debugging=0 --with-mpi=0 --with-shared-libraries=1 --with-scalar-type=real --with-precision=double --with-64-bit-indices=0 --download-f2cblaslapack=0 --with-blaslapack-lib=\"[/usr/lib/x86_64-linux-gnu/libopenblas.so]\" COPTFLAGS=\"-O3 -march=native\" CXXOPTFLAGS=\"-O3 -march=native\" FOPTFLAGS=\"-O3 -march=native\"
86:CXX = g++
```

`module avail petsc` lists only `petsc/3.26.0 (L)` (loaded). `module show petsc/3.26.0`
(module file `/etc/lmod/modules/petsc/3.26.0.lua`) sets:

```
setenv("PETSC_DIR","/opt/petsc/3.26.0")
setenv("PETSC_ARCH","")
prepend_path("LD_LIBRARY_PATH","/opt/petsc/3.26.0/lib")
prepend_path("LIBRARY_PATH","/opt/petsc/3.26.0/lib")
prepend_path("CPATH","/opt/petsc/3.26.0/include")
prepend_path("CPLUS_INCLUDE_PATH","/opt/petsc/3.26.0/include")
prepend_path("PKG_CONFIG_PATH","/opt/petsc/3.26.0/lib/pkgconfig")
prepend_path("PATH","/opt/petsc/3.26.0/bin")
```

**No MPI-enabled PETSc exists on this system.** The only installed PETSc (3.26.0) was
configured `--with-mpi=0` and defines `PETSC_HAVE_MPIUNI`.

### A4. CPU

```
$ lscpu | grep "Model name"
Model name:                              INTEL(R) XEON(R) GOLD 6530
$ grep -m1 -o avx512f /proc/cpuinfo
avx512f
```

AVX-512F is available. `nproc` reports 128 logical CPUs.

## B. PhreeqcRM source facts

All paths below are relative to `~/phreeqcrm` unless written as absolute.

### B1. PHREEQC copy against the Phase 1 base (b0b3be76)

Commands exactly as specified (`git -C ~/phreeqc3 show b0b3be76:src/<file> | diff - ~/phreeqcrm/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/<file>`):

| file | result |
|---|---|
| `model.cpp` | identical |
| `Phreeqc.h` | identical |
| `Phreeqc.cpp` | identical |
| `global_structures.h` | identical |
| `class_main.cpp` | identical |

CONFIRMED: the PhreeqcRM v3.9.0 copy of all five files is byte-identical to PHREEQC
v3.9.0 at `b0b3be76`. No differing hunks exist, so the requested line-range analysis is
vacuous.

### B2. Patch applicability (dry run only)

```
$ git -C ~/phreeqc3 diff petsc-phase1a-task0 petsc-phase1a-task4 -- src/model.cpp src/Phreeqc.h src/Phreeqc.cpp > ~/phreeqc3/build/phase2/phase1_phreeqc_core.patch
$ wc -l ~/phreeqc3/build/phase2/phase1_phreeqc_core.patch
159
$ git -C ~/phreeqcrm apply --check -p2 --directory=src/IPhreeqcPhast/IPhreeqc/phreeqcpp ~/phreeqc3/build/phase2/phase1_phreeqc_core.patch && echo APPLIES_CLEANLY
APPLIES_CLEANLY
```

The full output of `git apply --check` is empty (no diagnostics); it exited 0.
The patch is 159 lines. It was **not** applied. (For context, the full tag-to-tag diff
also touches `CMakeLists.txt`, `src/class_main.cpp`, and `src/petsc_linear_solver/*`;
those are outside the three files the specification asked to extract.)

### B3. Build structure

All quotes from `~/phreeqcrm/CMakeLists.txt` (649 lines).

**phreeqcpp sources listed and compiled into `PhreeqcRM`** — `CMakeLists.txt:140-317`
(abridged; the list contains every `phreeqcpp` `.cpp/.cxx/.h` file, including the Phase 1
touched files):

```
140: set(PhreeqcRM_SOURCES
...
162:   ${CMAKE_CURRENT_SOURCE_DIR}/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/advection.cpp
...
198:   ${CMAKE_CURRENT_SOURCE_DIR}/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/global_structures.h
...
211:   ${CMAKE_CURRENT_SOURCE_DIR}/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/model.cpp
...
226:   ${CMAKE_CURRENT_SOURCE_DIR}/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/Phreeqc.cpp
227:   ${CMAKE_CURRENT_SOURCE_DIR}/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/Phreeqc.h
...
317: )
```

**Target definition** — `CMakeLists.txt:322-323`:

```
add_library(PhreeqcRM ${PhreeqcRM_SOURCES})
add_library(PhreeqcRM::PhreeqcRM ALIAS PhreeqcRM)
```

CONFIRMED: the `PhreeqcRM` target compiles the `phreeqcpp` sources directly (no separate
PHREEQC library target).

**C++ standard** — `CMakeLists.txt:342-343`:

```
# c++11
target_compile_features(PhreeqcRM PUBLIC cxx_std_11)
```

**Every `target_link_libraries(PhreeqcRM …)` call** (all use the keyword form):

```
376:     target_link_libraries(PhreeqcRM PUBLIC yaml-cpp::yaml-cpp)     (guarded by PHREEQCRM_WITH_YAML_CPP, 373)
378:     target_link_libraries(PhreeqcRM PUBLIC yaml-cpp)               (same guard)
387:   target_link_libraries(PhreeqcRM PUBLIC yaml-cpp)                 (guarded by SKBUILD, 384)
407:   target_link_libraries(PhreeqcRM PRIVATE ZLIB::ZLIB)              (guarded by PHREEQCRM_USE_ZLIB, 405)
414:   target_link_libraries(PhreeqcRM PRIVATE zlibstatic)              (guarded by SKBUILD, 410)
475:   target_link_libraries(PhreeqcRM PUBLIC MPI::MPI_CXX)             (guarded by PHREEQCRM_BUILD_MPI, 471)
482:       target_link_libraries(PhreeqcRM PUBLIC OpenMP::OpenMP_CXX)   (else-branch, 478-480)
485:         target_link_libraries(PhreeqcRM PUBLIC OpenMP::OpenMP_Fortran) (also PHREEQCRM_FORTRAN_TESTING, 484)
```

All eight active calls use `PUBLIC`/`PRIVATE` keywords: CONFIRMED keyword form.
One non-keyword call exists only inside a comment block, `CMakeLists.txt:423-426`:

```
# # At least with MSMVC PhreeqcRM.dll requires MPI_CXX_LIBRARIES to link
# if(MSVC AND BUILD_SHARED_LIBS AND PHREEQCRM_BUILD_MPI)
#   target_link_libraries(PhreeqcRM ${MPI_CXX_LIBRARIES})
# endif()
```

(the first commented line actually reads `# # At least with MSMVC ...` — quoted verbatim;
it is dead code).

**MPI/OpenMP option blocks** — `CMakeLists.txt:103-115`:

```
option(PHREEQCRM_DISABLE_OPENMP "Disable openmp" OFF)
if(NOT PHREEQCRM_DISABLE_OPENMP)
  if(STANDALONE_BUILD EQUAL 1)
    # check for openmp
    find_package(OpenMP)
  endif()
endif()

# check for MPI
option(PHREEQCRM_BUILD_MPI "Build MPI configuration" OFF)
if(PHREEQCRM_BUILD_MPI)
  find_package(MPI REQUIRED)
endif()
```

CONFIRMED: `PHREEQCRM_BUILD_MPI` default OFF (112), `PHREEQCRM_DISABLE_OPENMP` default OFF (103).

**Mutual exclusion** — `CMakeLists.txt:471-489`:

```
if(PHREEQCRM_BUILD_MPI)
  # USE_MPI needs to be public in order to compile the
  # fortran interface files
  target_compile_definitions(PhreeqcRM PUBLIC USE_MPI)
  target_link_libraries(PhreeqcRM PUBLIC MPI::MPI_CXX)
  set(usempi_define "-DUSE_MPI")
  string(APPEND exportPackageDependencies "find_dependency(MPI)\n")
else()
  if(NOT PHREEQCRM_DISABLE_OPENMP)
    if(OPENMP_FOUND)
      target_compile_definitions(PhreeqcRM PRIVATE USE_OPENMP)
      target_link_libraries(PhreeqcRM PUBLIC OpenMP::OpenMP_CXX)
      string(APPEND exportPackageDependencies "find_dependency(OpenMP)\n")
      if(PHREEQCRM_FORTRAN_TESTING)
        target_link_libraries(PhreeqcRM PUBLIC OpenMP::OpenMP_Fortran)
      endif()
    endif()
  endif()
endif()
```

CONFIRMED: an `if(PHREEQCRM_BUILD_MPI) … else() … OpenMP … endif()` block; MPI and
OpenMP are mutually exclusive.

**Compile definitions, PUBLIC vs PRIVATE**:

- `USE_MPI` — **PUBLIC** (`CMakeLists.txt:474`), only when `PHREEQCRM_BUILD_MPI=ON`.
- `USE_OPENMP` — **PRIVATE** (`CMakeLists.txt:481`), only in the non-MPI branch when
  `OPENMP_FOUND` and `PHREEQCRM_DISABLE_OPENMP=OFF`.
- Always PRIVATE: `SWIG_SHARED_OBJ`, `USE_PHRQ_ALLOC`, `IPhreeqc_EXPORTS`
  (`CMakeLists.txt:346-348`); conditionally PRIVATE `NO_UTF8_ENCODING` (351),
  `_WINDLL` (366), `USE_GZ` (406, 412); conditionally PUBLIC `USE_YAML` (374, 386).
- `USE_PETSC` does not appear anywhere in PhreeqcRM:
  `grep -rn "USE_PETSC" ~/phreeqcrm/CMakeLists.txt ~/phreeqcrm/src/IPhreeqcPhast/IPhreeqc/phreeqcpp/Phreeqc.h`
  exits 1 (no match).

**Tests subdirectory** — `CMakeLists.txt:617`: `add_subdirectory(Tests)` (standalone builds only, 607-618).

**Test registration** (`~/phreeqcrm/Tests/CMakeLists.txt`) — CONFIRMED:

```
190: add_test(TestRMdtor TestRMdtor)
214: add_test(TestBMIdtor TestBMIdtor)
223: add_test(TestRM TestRM)
331: if(PHREEQCRM_BUILD_MPI)
332:   add_test(NAME TestMPI
333:            COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} ${MPIEXEC_MAX_NUMPROCS}
334:            ${MPIEXEC_PREFLAGS}
335:            $<TARGET_FILE:TestRM>
336:            ${MPIEXEC_POSTFLAGS}
337:            )
338: endif()
```

Observation (not a judgment): `CMakeLists.txt:5` declares `VERSION 3.8.0` in `project()`
while the git tag is `v3.9.0`.

### B4. Lifecycle entry points

**`PhreeqcRM` constructor** — one user-defined constructor, declared
`src/PhreeqcRM.h:383-384`:

```
	PhreeqcRM(int nxyz, MP_TYPE thread_count_or_communicator, PHRQ_io * io=NULL, bool delay_construct=false);
	virtual ~PhreeqcRM(void);
```

Defined `src/PhreeqcRM.cpp:194-242` (abridged; full body read):

```
194: PhreeqcRM::PhreeqcRM(int nxyz_arg, MP_TYPE data_for_parallel_processing, PHRQ_io *io/*=NULL*/, bool delay_construct/*=false*/)
...
212: , initializer(std::unique_ptr<PhreeqcRM::Initializer>(new PhreeqcRM::Initializer(nxyz_arg, data_for_parallel_processing, io)))
213: {
214: #ifdef USE_MPI
215: 	phreeqcrm_comm = data_for_parallel_processing;
...
226: 	if (!delay_construct)
227: 	{
228: #ifdef USE_MPI
229: 		if (mpi_myself == 0)
230: 		{
231: 			this->Construct();
232: 			MpiWorkerBreak();
233: 		}
234: 		else
235: 		{
236: 			MpiWorker();
237: 		}
238: #else
239: 		this->Construct();
240: #endif
241: 	}
242: }
```

No copy constructor or assignment operator is declared in `PhreeqcRM.h`.

**Destructor** — `src/PhreeqcRM.cpp:426-450` (abridged):

```
426: PhreeqcRM::~PhreeqcRM(void)
427: {
428: 	this->CloseFiles();
429: 
430: 	for (auto worker : this->GetWorkers())
431: 	{
432: 		delete worker;
433: 	}
...
435: 	delete this->phreeqc_bin;
```

**Worker creation and count** — `PhreeqcRM::Construct()`, `src/PhreeqcRM.cpp:343-350`:

```
343: 	// last one is to calculate well pH
344: 	for (int i = 0; i < this->nthreads + 2; i++)
345: 	{
346: 		this->workers.push_back(new IPhreeqcPhast);
347: 		this->workers.back()->Set_error_ostream(phreeqcrm_io->Get_error_ostream());
348: 		this->workers.back()->SetErrorFileOn(true);
349: 		this->workers.back()->Set_error_on(true);
350: 	}
```

Count: **`nthreads + 2` `IPhreeqcPhast` instances per `PhreeqcRM` object** (per MPI rank;
in the OpenMP build `nthreads` is set at `PhreeqcRM.cpp:337-339`). `GetWorkers` bounds the
index to `0 .. nthreads+1` (`PhreeqcRM.cpp:4363`):

```
4363: 	return (i >= 0 && i < this->nthreads + 2) ? this->workers[i] : NULL;
```

Roles of the two extra instances — **AMBIGUOUS**. The comment at line 343 says the last
instance is for well pH. The code uses index `nthreads` as the "utility" instance in
`FindComponents` (`PhreeqcRM.cpp:2910-2911`):

```
2910: 		// Get other components
2911: 		IPhreeqcPhast * phast_iphreeqc_worker = this->GetWorkers()[this->nthreads];
```

but `Concentrations2UtilityH2O`/`Concentrations2UtilityNoH2O` use index `nthreads + 1`
(`PhreeqcRM.cpp:1354`, `1421-1423`):

```
1354: 	size_t nutil= this->nthreads + 1;
...
1421: 		this->workers[nutil]->PhreeqcPtr->Rxn_solution_map[(int) (i + 1)] = soln;
1422: 	}
1423: 	return (dynamic_cast< IPhreeqc *> (this->workers[nutil]));
```

The count is unambiguous (`nthreads + 2`); which of the two non-cell instances is "the
utility" and which is "well pH" is not consistently identified in the sources.

**`MpiWorker`** — `src/PhreeqcRM.cpp:7072-7091` (beginning of the worker loop; full body
7074-7537):

```
7072: /* ---------------------------------------------------------------------- */
7073: IRM_RESULT
7074: PhreeqcRM::MpiWorker()
7075: /* ---------------------------------------------------------------------- */
7076: {
7077: 	// Called by all workers
7078: 	IRM_RESULT return_value = IRM_OK;
7079: #ifdef USE_MPI
7080: 	this->worker_waiting = true;
7081: 	bool debug_worker = false;
7082: 	bool loop_break = false;
7083: 	while (!loop_break)
7084: 	{
7085: 		try
7086: 		{
7087: 			return_value = IRM_OK;
7088: 			int method = -1;
7089: 			//std::cerr << "Worker waiting..." << std::endl;
7090: 			MPI_Bcast(&method, 1, MPI_INT, 0, phreeqcrm_comm);
7091: 			switch (method)
```

**`MpiWorkerBreak`** — `src/PhreeqcRM.cpp:7538-7550`:

```
7538: /* ---------------------------------------------------------------------- */
7539: IRM_RESULT
7540: PhreeqcRM::MpiWorkerBreak()
7541: {
7542: #ifdef USE_MPI
7543: 	if (mpi_myself == 0)
7544: 	{
7545: 		int method = METHOD_MPIWORKERBREAK;
7546: 		MPI_Bcast(&method, 1, MPI_INT, 0, phreeqcrm_comm);
7547: 	}
7548: #endif
7549: 	return IRM_OK;
7550: }
```

**C-interface factories** — `src/PhreeqcRM.cpp:122-148`:

```
122: PhreeqcRM::CreateReactionModule(int nxyz, MP_TYPE nthreads)
...
130: 		PhreeqcRM * Reaction_module_ptr = new PhreeqcRM(nxyz, nthreads);
...
143: PhreeqcRM::DestroyReactionModule(int id)
144: {
147: 	return PhreeqcRM::Destroy(id);
148: }
```

**`RM_Create` / `RM_Destroy` (C interface)** — `src/RM_interface_C.cpp:769-789, 827-832`:

```
769: #ifdef USE_MPI
770: /* ---------------------------------------------------------------------- */
771: int RM_Create(int nxyz, MPI_Comm comm)
...
777: 	return PhreeqcRM::CreateReactionModule(nxyz, comm);
778: }
779: #else
780: /* ---------------------------------------------------------------------- */
781: int RM_Create(int nxyz, int nthreads)
...
787: 	return PhreeqcRM::CreateReactionModule(nxyz, nthreads);
788: }
789: #endif
...
827: /* ---------------------------------------------------------------------- */
828: IRM_RESULT RM_Destroy(int id)
829: /* ---------------------------------------------------------------------- */
830: {
831: 	return PhreeqcRM::DestroyReactionModule(id);
832: }
```

**BMI class constructor and destructor** — `BMIPhreeqcRM`, declared
`src/BMIPhreeqcRM.h:75,77`:

```
    BMIPhreeqcRM(int ngrid = PhreeqcRM::default_nxyz, MP_TYPE nthreads = PhreeqcRM::default_data_for_parallel_processing);
    ~BMIPhreeqcRM() override;
```

Defined `src/BMIPhreeqcRM.cpp:108-127`:

```
108: // Constructor
109: BMIPhreeqcRM::BMIPhreeqcRM(int nxyz, MP_TYPE nthreads)
110: : PhreeqcRM(nxyz, nthreads, nullptr, true) 
111: , var_man( nullptr )
112: , constructed( false )
113: {
114: 	this->language = "cpp";
...
122: }
123: // Destructor
124: BMIPhreeqcRM::~BMIPhreeqcRM()
125: {
126: 	delete(this->var_man);
127: }
```

Note: `BMIPhreeqcRM` passes `delay_construct=true` (line 110) and constructs in its own
`Construct()` (`BMIPhreeqcRM.cpp:138-141`).

### B5. Threads

Every `#pragma omp` in `src/PhreeqcRM.cpp` (40 occurrences), with the enclosing function
(function definition line in parentheses):

| lines | enclosing function |
|---|---|
| 2576, 2577 | `PhreeqcRM::DumpModule` (2493, `#ifdef NEWDUMP` variant) |
| 2741, 2742 | `PhreeqcRM::DumpModule` (2673, `#ifdef ORIGINALDUMP` variant) |
| 2831 | `PhreeqcRM::ErrorMessage` (2827) — `#pragma omp critical` |
| 3923, 3924 | `PhreeqcRM::GetGasCompMoles` (3910) |
| 4066, 4067 | `PhreeqcRM::GetGasCompPressures` (4053) |
| 4207, 4208 | `PhreeqcRM::GetGasCompPhi` (4194) |
| 4332, 4333 | `PhreeqcRM::GetGasPhaseVolume` (4319) |
| 7011, 7012 | `PhreeqcRM::LoadDatabase` (6985) |
| 9950, 9951 | `PhreeqcRM::RunCells` (9882) |
| 10644, 10645 | `PhreeqcRM::RunFile` (10586) |
| 10782, 10783 | `PhreeqcRM::RunString` (10722) |
| 11204, 11205 | `PhreeqcRM::SetConcentrations` (11147) |
| 11322, 11323 | `PhreeqcRM::SetGasCompMoles` (11221) |
| 11455, 11456 | `PhreeqcRM::SetGasCompMoles` (11355) |
| 11585, 11586 | `PhreeqcRM::SetGasPhaseVolume` (11504) |
| 12050, 12051 | `PhreeqcRM::SetPressure` (12018) |
| 12330, 12331 | `PhreeqcRM::SetTemperature` (12304) |
| 12841, 12842, 12874, 12875, 12913, 12914 | `PhreeqcRM::SpeciesConcentrations2Module` (12738) |
| 13100 | `PhreeqcRM::WarningMessage` (13096) — `#pragma omp critical` |

The function that runs the chemistry of a block of cells on each thread is
**`PhreeqcRM::RunCells()`**, which parallel-for over `RunCellsThread(n)`
(`src/PhreeqcRM.cpp:9945-9956`):

```
9945: #ifdef USE_OPENMP
9946: #if defined(swig_python_EXPORTS)
9947: 		Py_BEGIN_ALLOW_THREADS
9948: #endif
9949: 		omp_set_num_threads(this->nthreads);
9950: #pragma omp parallel
9951: #pragma omp for
9952: #endif
9953: 		for (int n = 0; n < this->nthreads; n++)
9954: 		{
9955: 			r_vector[n] = RunCellsThread(n);
9956: 		}
```

`PhreeqcRM::RunCellsThread(int n)` (`src/PhreeqcRM.cpp:10258`) takes worker
`GetWorkers()[n]` (line 10271) and runs the cells of that thread's block
(`start_cell[n] .. end_cell[n]`) by feeding one `RUN_CELLS` input per cell to the
worker (`src/PhreeqcRM.cpp:10404-10406, 10443-10450`):

```
10404: 			// run the cells
10405: 			for (i = start; i <= end; i++)
10406: 			{							/* i is count_chem number */
...
10443: 					// do the calculation
10444: 					std::ostringstream input;
10445: 					input << "RUN_CELLS\n";
10446: 					input << "  -start_time " << (this->time - this->time_step) << "\n";
10447: 					input << "  -time_step  " << this->time_step << "\n";
10448: 					input << "  -cells      " << i << "\n";
10449: 					input << "END" << "\n";
10450: 					if (phast_iphreeqc_worker->RunString(input.str().c_str()) != 0)
```

(The single-thread path `PhreeqcRM.cpp:9793` calls `RunCellsThread(0)` without pragmas.)

### B6. MPI ownership

```
$ grep -rn "MPI_Init\|MPI_Finalize\|MPI_Initialized\|MPI_Finalized" src Tests   (run in ~/phreeqcrm)
Tests/main.cpp:49:	if (MPI_Init(&argc, &argv) != MPI_SUCCESS)
Tests/main.cpp:127:	MPI_Finalize();
Tests/TestBMIdtor.cpp:13:    if (MPI_Init(&argc, &argv) != MPI_SUCCESS)
Tests/TestBMIdtor.cpp:28:    if (MPI_Finalize() != MPI_SUCCESS)
Tests/TestRMdtor.cpp:10:    if (MPI_Init(&argc, &argv) != MPI_SUCCESS)
Tests/TestRMdtor.cpp:23:    if (MPI_Finalize() != MPI_SUCCESS)
```

No match in `src/` (the library). Quotes, `Tests/main.cpp:48-52, 126-128`:

```
48: #if defined(USE_MPI)
49: 	if (MPI_Init(&argc, &argv) != MPI_SUCCESS)
50: 	{
51: 		return EXIT_FAILURE;
52: 	}
...
126: #if defined(USE_MPI)
127: 	MPI_Finalize();
128: #endif
```

`Tests/TestRMdtor.cpp:10-15, 22-26`:

```
10:     if (MPI_Init(&argc, &argv) != MPI_SUCCESS)
11:     {
12:         return EXIT_FAILURE;
13:     }
14:     PhreeqcRM* rm = new PhreeqcRM(10, MPI_COMM_WORLD);
...
22: #if defined(USE_MPI)
23:     if (MPI_Finalize() != MPI_SUCCESS)
24:     {
25:         return EXIT_FAILURE;
26:     }
```

`Tests/TestBMIdtor.cpp:13-16, 27-31` is the same pattern around `new BMIPhreeqcRM()`.

**Who initialises/finalises MPI: the application.** PhreeqcRM (`src/`) never calls
`MPI_Init`, `MPI_Finalize`, `MPI_Initialized`, or `MPI_Finalized`; it requires the host to
have an initialised MPI environment and uses the communicator passed to the constructor.

### B7. Translation units that include `Phreeqc.h`

```
$ grep -rln '#include "Phreeqc.h"' src      (run in ~/phreeqcrm)
```

returns **73 files**: the 69 files

`src/BMI_interface_C.cpp`, `src/BMI_interface_F.cpp`, `src/BMIPhreeqcRM.cpp`,
`src/IPhreeqcPhast/IPhreeqc/IPhreeqc.cpp`, `src/IPhreeqcPhast/IPhreeqc/IPhreeqc_interface_F.cpp`,
`src/IPhreeqcPhast/IPhreeqcPhast.cxx`, `src/PhreeqcRM.cpp`, `src/RM_interface_C.cpp`,
`src/RM_interface_F.cpp`, `src/VarManager.cpp`, and 59 files under
`src/IPhreeqcPhast/IPhreeqc/phreeqcpp/` (advection, basicsubs, cl1, cvdense, cvode,
cxxKinetics, cxxMix, Exchange, ExchComp, GasComp, gases, GasPhase, input, integrate,
inverse, ISolution, isotopes, kinetics, KineticsComp, mainsubs, model, NameDouble, parse,
PBasic, phqalloc, Phreeqc, PHRQ_io_output, pitzer, pitzer_structures, PPassemblage,
PPassemblageComp, prep, Pressure, print, Reaction, ReadClass, read, readtr, Serializer,
sit, Solution, SolutionIsotope, spread, SS, SSassemblage, SScomp, step, StorageBin,
structures, Surface, SurfaceCharge, SurfaceComp, System, tally, Temperature, tidy,
transport, UserPunch, utilities `.cpp/.cxx`),

plus **4 files that are NOT compiled into `PhreeqcRM`**:

- `src/IPhreeqcPhast/IPhreeqc/phreeqcpp/cl1mp.cpp`
- `src/IPhreeqcPhast/IPhreeqc/phreeqcpp/class_main.cpp`
- `src/IPhreeqcPhast/IPhreeqc/phreeqcpp/ChartHandler.cpp`
- `src/IPhreeqcPhast/IPhreeqc/phreeqcpp/ChartObject.cpp`

Verified by comparing the grep result against `PhreeqcRM_SOURCES` (`CMakeLists.txt:140-317`):
none of the four appears in it, and `grep -rn "cl1mp\|ChartHandler\|ChartObject\|class_main"`
over all `CMakeLists.txt`, `*.cmake`, `Makefile.am`, `*.py`, `*.toml` in the repository finds
them only in the autotools files `src/IPhreeqcPhast/IPhreeqc/phreeqcpp/Makefile.am:2-5,22,155`
and `src/IPhreeqcPhast/IPhreeqc/Makefile.am:16-19` — no CMake target compiles them.

**Compile definitions for the 69 compiled includers:** all are sources of the single
`PhreeqcRM` target and are compiled unconditionally; the only per-configuration definitions
are target-wide: `USE_MPI` PUBLIC (CMakeLists.txt:474) or `USE_OPENMP` PRIVATE (481), plus
the always-PRIVATE `SWIG_SHARED_OBJ`, `USE_PHRQ_ALLOC`, `IPhreeqc_EXPORTS` (346-348).
There is no per-file conditional compilation of any `Phreeqc.h` includer.

**Phase 1 rule check ("class layout does not depend on `USE_PETSC`"):** `USE_PETSC` does
not exist anywhere in PhreeqcRM (grep in B3), and every translation unit that includes
`Phreeqc.h` is part of one target with uniform target-wide definitions, so within the
library the class layout cannot diverge. Two caveats for Phase 2, stated as facts:
`USE_OPENMP` is PRIVATE while `USE_MPI` is PUBLIC (the comment at CMakeLists.txt:472-473
explains that consumers of the Fortran interface need `USE_MPI` publicly); and the test
programs do not include `Phreeqc.h` directly
(`grep -rln '#include "Phreeqc.h"' Tests` exits 1). If Phase 2 changes the layout of any
class in `Phreeqc.h` under `USE_PETSC`, the definition must be applied uniformly to the
whole `PhreeqcRM` target, and made PUBLIC if any TU outside the target ever includes
those headers.

## C. Baseline builds and tests

### C1. OpenMP configuration

Commands exactly as specified, plus the reviewer-approved `ctest_start_marker` amendment.

Configure (`cmake -S ~/phreeqcrm -B ~/phreeqcrm/build-baseline-openmp -DCMAKE_BUILD_TYPE=Release`), exit 0.
CMake found OpenMP — `build-baseline-openmp` configure output lines 14-16:

```
-- Found OpenMP_C: -fopenmp (found version "4.5")
-- Found OpenMP_CXX: -fopenmp (found version "4.5")
-- Found OpenMP: TRUE (found version "4.5")
```

Build (`cmake --build … -j 8`): exit 0, no errors.

`ctest --test-dir ~/phreeqcrm/build-baseline-openmp --output-on-failure` (run 1, exit 0):

```
1/3 Test #1: TestRMdtor .......................   Passed    0.01 sec
2/3 Test #2: TestBMIdtor ......................   Passed    0.00 sec
3/3 Test #3: TestRM ...........................   Passed    0.28 sec

100% tests passed, 0 tests failed out of 3
Total Test time (real) =   0.29 sec
```

Test outputs, `find ~/phreeqcrm/build-baseline-openmp/Tests -newer …/ctest_start_marker -type f`:
**27 files** (full list, not truncated):

```
Tests/Advect_c.chem.txt
Tests/Advect_c.dmp
Tests/Advect_c.log.txt
Tests/Advect_cpp.chem.txt
Tests/Advect_cpp.dmp
Tests/Advect_cpp.log.txt
Tests/Advect_cpp_units_utility.txt
Tests/Advect_cpp_units_worker.chem.txt
Tests/Advect_cpp_units_worker.log.txt
Tests/Advect_cpp_utility.txt
Tests/Advect_c_utility.txt
Tests/Gas_c.chem.txt
Tests/Gas_c.log.txt
Tests/Gas_cpp.chem.txt
Tests/Gas_cpp.log.txt
Tests/SimpleAdvect_c.chem.txt
Tests/SimpleAdvect_c.log.txt
Tests/SimpleAdvect_cpp.chem.txt
Tests/SimpleAdvect_cpp.log.txt
Tests/Species_c.chem.txt
Tests/Species_c.dmp
Tests/Species_c.log.txt
Tests/Species_cpp.chem.txt
Tests/Species_cpp.dmp
Tests/Species_cpp.log.txt
Tests/Species_cpp_utility.txt
Tests/Species_c_utility.txt
```

All 27 were copied, with paths relative to the build tree, to
`~/phreeqcrm_phase2_baseline/openmp/Tests/` (verified: 27 files).

### C2. MPI configuration

A2 found MPI, so C2 was run. Configure
(`cmake -S ~/phreeqcrm -B ~/phreeqcrm/build-baseline-mpi -DCMAKE_BUILD_TYPE=Release -DPHREEQCRM_BUILD_MPI=ON`),
exit 0; configure output lines 17-19:

```
-- Found MPI_C: /usr/lib/x86_64-linux-gnu/openmpi/lib/libmpi.so (found version "3.1")
-- Found MPI_CXX: /usr/lib/x86_64-linux-gnu/openmpi/lib/libmpi.so (found version "3.1")
-- Found MPI: TRUE (found version "3.1")
```

From `build-baseline-mpi/CMakeCache.txt`:

```
304: MPIEXEC_EXECUTABLE:FILEPATH=/usr/bin/mpiexec
307: MPIEXEC_MAX_NUMPROCS:STRING=64
311: MPIEXEC_NUMPROC_FLAG:STRING=-n
```

Build (`cmake --build … -j 8`): exit 0, no errors.

ctest, run 1, the exact specified command
`ctest --test-dir ~/phreeqcrm/build-baseline-mpi --output-on-failure`: **did not
terminate**; after 20 minutes the log contained only

```
Test project /home/chen/phreeqcrm/build-baseline-mpi
    Start 1: TestRMdtor
```

and the run was killed. Diagnosis facts (not a judgment): `TestRMdtor` run directly in
`build-baseline-mpi/Tests` hangs (60 s timeout, prints
"Authorization required, but no authorization protocol specified"); the same binary under
`/usr/bin/mpiexec -n 1` exits 0 immediately. The registered tests `TestRMdtor`,
`TestBMIdtor`, and `TestRM` are launched directly by ctest (Tests/CMakeLists.txt:190, 214,
223), not through `mpiexec`, in the MPI configuration.

ctest, run 2 (same command plus `--timeout 120` to obtain a complete summary; deviation
from the specified command, flagged in "Reviewer decisions required"), exit 8:

```
1/4 Test #1: TestRMdtor .......................***Timeout 120.14 sec
2/4 Test #2: TestBMIdtor ......................***Timeout 120.12 sec
3/4 Test #3: TestRM ...........................***Timeout 120.13 sec
4/4 Test #4: TestMPI ..........................***Failed    3.15 sec

0% tests passed, 4 tests failed out of 4
Total Test time (real) =   363.54 sec
```

`TestMPI` output (excerpt):

```
          Estimated efficiency of chemistry without communication: 59.1162
Done units_tester.===================================
ERROR: PhreeqcRM failed.
ERROR: Number of processes must be less than or equal to number of model cells, 20.

SimpleAdvect_cpp failed with an error in PhreeqcRM.
MPI_ABORT was invoked on rank 0 in communicator MPI_COMM_WORLD
  Proc: [[58164,1],0]
  Errorcode: 1
```

Test outputs (`find … -newer …/ctest_start_marker -type f`): **5 files** (full list):

```
Tests/Advect_cpp.chem.txt
Tests/Advect_cpp.log.txt
Tests/Advect_cpp_units_utility.txt
Tests/Advect_cpp_units_worker.chem.txt
Tests/Advect_cpp_units_worker.log.txt
```

All 5 copied to `~/phreeqcrm_phase2_baseline/mpi/Tests/` (verified: 5 files).

### C3. Determinism

**OpenMP.** Re-ran `ctest --test-dir ~/phreeqcrm/build-baseline-openmp --output-on-failure`
(exit 0): `TestRMdtor 0.00 sec Passed`, `TestBMIdtor 0.00 sec Passed`,
`TestRM 0.23 sec Passed`, "100% tests passed", total 0.23 sec. The second run rewrote all
27 output files (verified by mtimes, e.g. `Tests/Advect_c.log.txt` 2026-10-11 20:25:19,
newer than the marker at 19:55:10). `cmp` of every file against the run-1 copy:
**14 of 27 differ**:

```
Tests/Advect_c.chem.txt
Tests/Advect_cpp.chem.txt
Tests/Advect_cpp_units_utility.txt
Tests/Advect_cpp_units_worker.chem.txt
Tests/Advect_cpp_utility.txt
Tests/Advect_c_utility.txt
Tests/Gas_c.chem.txt
Tests/Gas_cpp.chem.txt
Tests/SimpleAdvect_c.chem.txt
Tests/SimpleAdvect_cpp.chem.txt
Tests/Species_c.chem.txt
Tests/Species_cpp.chem.txt
Tests/Species_cpp_utility.txt
Tests/Species_c_utility.txt
```

The 13 files identical byte for byte are all 8 `.log.txt` and all 5 `.dmp` files. In every
differing file the only differing lines are wall-clock lines of the form
`End of Run after <seconds> Seconds.` (example: `Tests/Advect_c.chem.txt` line 202:
`0.372201` vs `0.307114`). Not judged.

**MPI.** Re-ran the same command used for the C2 baseline copies
(`ctest … --output-on-failure --timeout 120`, exit 8): identical failure pattern —
`TestRMdtor ***Timeout 120.13 sec`, `TestBMIdtor ***Timeout 120.12 sec`,
`TestRM ***Timeout 120.03 sec`, `TestMPI ***Failed 2.93 sec`, total 363.22 sec.
`cmp` of the 5 output files against the C2 copies: **2 of 5 differ**:

```
Tests/Advect_cpp_units_utility.txt
Tests/Advect_cpp_units_worker.chem.txt
```

Again only `End of Run after <seconds> Seconds.` lines differ (example:
`Advect_cpp_units_utility.txt` line 173: `1.1963` vs `1.1916`). Not judged.

## Reviewer decisions required

- **MPI-enabled PETSc (A3):** none exists. `/opt/petsc/3.26.0` is configured
  `--with-mpi=0` with `PETSC_HAVE_MPIUNI 1` (petscconf.h:75); `/opt/petsc/3.20.6` is an
  empty directory; only the `petsc/3.26.0` module exists.
- **Phase 1 patch (B2):** the 159-line core patch (`model.cpp`, `Phreeqc.h`,
  `Phreeqc.cpp`) applies cleanly to PhreeqcRM v3.9.0 with
  `git apply --check -p2 --directory=src/IPhreeqcPhast/IPhreeqc/phreeqcpp` (empty output,
  exit 0). The five compared files are byte-identical to base `b0b3be76` (B1).
- **Baseline tests (C1, C2):** OpenMP configuration: 3/3 pass (0.29 s). MPI
  configuration: 0/4 pass — `TestRMdtor`, `TestBMIdtor`, `TestRM` hang when launched
  directly (MPI_Init outside `mpiexec` hangs in this environment; killed after 20 min on
  run 1, each hit the 120 s timeout on run 2); `TestMPI` fails at 64 ranks with
  "Number of processes must be less than or equal to number of model cells, 20."
  (MPIEXEC_MAX_NUMPROCS=64 > 20 cells in `SimpleAdvect_cpp`).
- **Deviation from the specified C2 command:** the exact `ctest` command never terminates;
  the reported summary was produced with `ctest … --timeout 120`. The C3 MPI re-run used
  the same `--timeout 120` variant. Decide whether this is acceptable or the MPI baseline
  must be redefined (e.g. tests launched through `mpiexec`).
- **Non-reproducible outputs (C3):** OpenMP: 14 of 27 files (all `.chem.txt` and
  `_utility.txt`; differences only in `End of Run after … Seconds.` lines). MPI: 2 of 5
  files (same pattern). All `.log.txt` and `.dmp` files are byte-identical across runs.
- **NOT FOUND:** `/opt/petsc/3.20.6` — `include/petscversion.h`, `include/petscconf.h`,
  `lib/petsc/conf/petscvariables` do not exist (directory is empty); no `petsc/3.20.6`
  module (A3).
- **AMBIGUOUS:** the roles of the two extra `IPhreeqcPhast` instances beyond one per
  thread: the count is `nthreads + 2` (PhreeqcRM.cpp:344) and the comment at line 343
  says the last is for well pH, but index `nthreads` is used as the utility in
  FindComponents (2911) while index `nthreads + 1` is used as "utility" in
  Concentrations2Utility* (1354, 1421-1423) (B4).
- **Observations for the record (not stop conditions):** `mpiexec --version` prints no
  version (missing OpenPMIx help files); version 5.0.10 taken from `dpkg` (A2).
  `CMakeLists.txt:5` declares `project(PhreeqcRM VERSION 3.8.0)` although the tree is
  tagged `v3.9.0` (B3). Four `Phreeqc.h` includers (`cl1mp.cpp`, `class_main.cpp`,
  `ChartHandler.cpp`, `ChartObject.cpp`) are compiled by no CMake target (B7).
