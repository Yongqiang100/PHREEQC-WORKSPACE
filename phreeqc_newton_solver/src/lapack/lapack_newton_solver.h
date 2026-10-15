#pragma once
#ifdef USE_LAPACK_NEWTON_SOLVER
#include <vector>
#include "phrqtype.h"
#include "linear_solve_status.h"
#include "newton_linear_solver_options.h"   /* NewtonConditionCheckMode (Task 2d) */

struct LapackNewtonSolver {
	bool   use_row_equilibration = true;
	double max_backward_error = 1.0e-10;   /* same acceptance as the PETSc adapter */
	bool   print_statistics = false;
	std::vector<double> scaled_matrix;      /* column-major n*n, kept for the backward error   */
	std::vector<double> factor_matrix;      /* column-major n*n, overwritten by dgetrf         */
	std::vector<double> scaled_rhs;         /* length n                                        */
	std::vector<double> solution;           /* length n; RHS on input to dgetrs, solution after */
	std::vector<int>    pivot_indices;      /* length n                                        */
	double last_backward_error = -1.0;
	unsigned long solve_count = 0, accepted_count = 0, cl1_fallback_count = 0,
	              non_finite_input_count = 0, lapack_error_count = 0;
	NewtonConditionCheckMode condition_check_mode = NEWTON_CONDITION_CHECK_OFF;
	double min_reciprocal_condition = 1.0e-12;
	std::vector<double> condition_work;      /* length 4n */
	std::vector<int>    condition_iwork;     /* length n  */
	double last_reciprocal_condition = -1.0; /* -1: not computed */
	unsigned long ill_conditioned_count = 0;
	/* Phase 1B Task 1b: reciprocal condition after row AND column equilibration. */
	bool   compute_equilibrated_condition = false;      /* diagnostics; shadow solver only */
	int    gate_on_equilibrated_condition = 0;          /* 0: gate on the row metric (unchanged); 1: on row_column */
	double last_equilibrated_reciprocal_condition = -1.0;
	std::vector<double> equilibrated_matrix;            /* n*n, column-major */
	std::vector<int>    equilibrated_pivots;            /* n */
	/* Phase 1B Task 3: fast two-sided metric from the existing factors (steering path only).
	   When true and compute_equilibrated_condition is false, the row-and-column metric is
	   obtained from the LU factors of the row-equilibrated matrix by scaling U by the column
	   maxima, and the row-metric dgecon is skipped (last_reciprocal_condition stays -1). */
	bool   equilibrated_condition_from_factors = false;
};

/* Returns nullptr only if allocation fails. */
LapackNewtonSolver *create_lapack_newton_solver(void);

/* If print_statistics: prints to stderr
     phreeqc_lapack_statistics: solves=%lu accepted=%lu cl1_fallbacks=%lu non_finite_inputs=%lu lapack_errors=%lu ill_conditioned=%lu
   then deletes the solver and sets *solver = nullptr. Safe on nullptr. */
void destroy_lapack_newton_solver(LapackNewtonSolver **solver);

/* PHREEQC layout: row-major, leading dimension count_unknowns + 1, RHS in column count_unknowns.
   On LINEAR_SOLVE_ACCEPTED, output[0..n-1] holds the solution; otherwise output is unchanged. */
LinearSolveStatus solve_newton_step_with_lapack(LapackNewtonSolver *solver,
	const LDBLE *augmented_newton_matrix, int count_unknowns, LDBLE *output);
#endif
