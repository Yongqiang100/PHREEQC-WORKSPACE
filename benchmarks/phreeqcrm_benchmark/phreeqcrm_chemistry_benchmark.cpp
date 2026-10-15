// Chemistry-only benchmark for PhreeqcRM (Phase 2 Task 4).
//
// Every cell starts as a mixture of two solutions (fraction1 varies linearly across the cells, so
// the chemistry differs from cell to cell), with equilibrium phases and kinetic reactants, and is
// then reacted for a number of time steps with RunCells. There is no transport, so the timings
// measure the chemistry, where the Newton linear solve lives.
//
// Usage: phreeqcrm_chemistry_benchmark <nxyz> <nthreads> <nsteps> <time_step_days>
//                                      <input.pqi> <database> <output_prefix>
// nthreads is used in OpenMP builds and ignored in MPI builds (one worker per rank).
// Writes <output_prefix>_concentrations.tsv (final concentrations, tab separated, high precision)
// and prints one line beginning "benchmark:" with the timings to stdout (root rank only).
#if defined(USE_MPI)
#include <mpi.h>
#endif
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include "PhreeqcRM.h"

static double seconds_since(const std::chrono::steady_clock::time_point &start)
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

static int run_benchmark(int argc, char *argv[])
{
	if (argc != 8)
	{
		std::cerr << "usage: " << argv[0]
		          << " <nxyz> <nthreads> <nsteps> <time_step_days> <input.pqi> <database> <output_prefix>\n";
		return 2;
	}
	const int nxyz = std::atoi(argv[1]);
	const int nthreads = std::atoi(argv[2]);
	const int nsteps = std::atoi(argv[3]);
	const double time_step_seconds = std::atof(argv[4]) * 86400.0;
	const std::string input_file = argv[5];
	const std::string database_file = argv[6];
	const std::string output_prefix = argv[7];
	if (nxyz < 2 || nsteps < 1 || time_step_seconds <= 0.0)
	{
		std::cerr << "invalid arguments\n";
		return 2;
	}

#if defined(USE_MPI)
	PhreeqcRM phreeqc_rm(nxyz, MPI_COMM_WORLD);
	int mpi_myself = 0;
	MPI_Comm_rank(MPI_COMM_WORLD, &mpi_myself);
	if (mpi_myself > 0)
	{
		phreeqc_rm.MpiWorker();   // workers serve the root until MpiWorkerBreak
		return 0;
	}
	(void)nthreads;
#else
	PhreeqcRM phreeqc_rm(nxyz, nthreads);
#endif
	const std::chrono::steady_clock::time_point setup_start = std::chrono::steady_clock::now();

	phreeqc_rm.SetErrorHandlerMode(1);            // throw PhreeqcRMStop on error
	phreeqc_rm.SetComponentH2O(false);
	phreeqc_rm.UseSolutionDensityVolume(false);
	phreeqc_rm.SetFilePrefix(output_prefix);
	phreeqc_rm.OpenFiles();
	phreeqc_rm.SetUnitsSolution(2);               // mol/L
	phreeqc_rm.SetUnitsPPassemblage(1);           // mol/L of water
	phreeqc_rm.SetUnitsKinetics(1);               // mol/L of water
	phreeqc_rm.SetTimeConversion(1.0 / 86400.0);
	phreeqc_rm.SetPorosity(std::vector<double>(nxyz, 0.2));
	phreeqc_rm.SetSaturationUser(std::vector<double>(nxyz, 1.0));
	phreeqc_rm.SetPrintChemistryOn(false, false, false);
	phreeqc_rm.SetSelectedOutputOn(false);
	phreeqc_rm.SetScreenOn(false);

	phreeqc_rm.LoadDatabase(database_file);
	phreeqc_rm.RunFile(true, true, true, input_file);
	phreeqc_rm.RunString(true, false, true, "DELETE; -all");
	const int ncomps = phreeqc_rm.FindComponents();
	const std::vector<std::string> &components = phreeqc_rm.GetComponents();

	// Reactant blocks, 7 per cell: solution, equilibrium phases, exchange, surface, gas, ss, kinetics.
	std::vector<int> initial_conditions1(nxyz * 7, -1), initial_conditions2(nxyz * 7, -1);
	std::vector<double> fraction1(nxyz * 7, 1.0);
	for (int cell = 0; cell < nxyz; cell++)
	{
		initial_conditions1[cell] = 1;                         // solution 1: basalt pore water
		initial_conditions2[cell] = 2;                         // solution 2: CO2-charged brine
		fraction1[cell] = 1.0 - (double)cell / (double)(nxyz - 1);
		initial_conditions1[1 * nxyz + cell] = 1;              // EQUILIBRIUM_PHASES 1
		initial_conditions1[6 * nxyz + cell] = 1;              // KINETICS 1
	}
	phreeqc_rm.InitialPhreeqc2Module(initial_conditions1, initial_conditions2, fraction1);
	phreeqc_rm.SetTemperature(std::vector<double>(nxyz, 50.0));
	phreeqc_rm.SetPressure(std::vector<double>(nxyz, 100.0));

	// Initial equilibration (time step 0).
	phreeqc_rm.SetTime(0.0);
	phreeqc_rm.SetTimeStep(0.0);
	std::chrono::steady_clock::time_point step_start = std::chrono::steady_clock::now();
	phreeqc_rm.RunCells();
	const double initial_run_seconds = seconds_since(step_start);
	const double setup_seconds = seconds_since(setup_start);

	std::vector<double> step_seconds;
	double time = 0.0;
	phreeqc_rm.SetTimeStep(time_step_seconds);
	for (int step = 0; step < nsteps; step++)
	{
		time += time_step_seconds;
		phreeqc_rm.SetTime(time);
		step_start = std::chrono::steady_clock::now();
		phreeqc_rm.RunCells();
		step_seconds.push_back(seconds_since(step_start));
	}

	std::vector<double> concentrations;
	phreeqc_rm.GetConcentrations(concentrations);
	const std::string table_name = output_prefix + "_concentrations.tsv";
	FILE *table = std::fopen(table_name.c_str(), "w");
	if (table == nullptr)
	{
		std::cerr << "cannot write " << table_name << "\n";
		return 3;
	}
	std::fprintf(table, "cell");
	for (int i = 0; i < ncomps; i++) std::fprintf(table, "\t%s", components[i].c_str());
	std::fprintf(table, "\n");
	for (int cell = 0; cell < nxyz; cell++)
	{
		std::fprintf(table, "%d", cell);
		for (int i = 0; i < ncomps; i++)
			std::fprintf(table, "\t%.15e", concentrations[(size_t)i * nxyz + cell]);
		std::fprintf(table, "\n");
	}
	std::fclose(table);

	double total_step_seconds = 0.0;
	for (double s : step_seconds) total_step_seconds += s;
	std::vector<double> sorted_steps = step_seconds;
	std::sort(sorted_steps.begin(), sorted_steps.end());
	const double median_step_seconds = sorted_steps[sorted_steps.size() / 2];
	std::printf("benchmark: nxyz=%d threads=%d mpi_tasks=%d steps=%d components=%d "
	            "setup_seconds=%.6f initial_run_cells_seconds=%.6f run_cells_seconds_total=%.6f "
	            "run_cells_seconds_median=%.6f\n",
	            nxyz, phreeqc_rm.GetThreadCount(), phreeqc_rm.GetMpiTasks(), nsteps, ncomps,
	            setup_seconds, initial_run_seconds, total_step_seconds, median_step_seconds);
	std::fflush(stdout);
	phreeqc_rm.CloseFiles();
	phreeqc_rm.MpiWorkerBreak();
	return 0;
}

int main(int argc, char *argv[])
{
#if defined(USE_MPI)
	if (MPI_Init(&argc, &argv) != MPI_SUCCESS) return EXIT_FAILURE;
#endif
	int return_code = 1;
	try
	{
		return_code = run_benchmark(argc, argv);
	}
	catch (PhreeqcRMStop &)
	{
		std::cerr << "benchmark failed with an error in PhreeqcRM\n";
#if defined(USE_MPI)
		MPI_Abort(MPI_COMM_WORLD, 1);
#endif
		return_code = 4;
	}
#if defined(USE_MPI)
	MPI_Finalize();
#endif
	return return_code;
}
