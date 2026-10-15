#pragma once
#ifdef USE_LAPACK_NEWTON_SOLVER
/* Phase 1B Task 1: the cl1-problem check (shadow mode). No PETSc anywhere. */
#include <vector>
#include "phrqtype.h"
#include "lapack_newton_solver.h"

enum Cl1ProblemOutcome {
	CL1_PROBLEM_ACCEPT = 0, CL1_PROBLEM_EXCLUDED_RETRY = 1, CL1_PROBLEM_EXCLUDED_PITZER_SIT = 2,
	CL1_PROBLEM_EXCLUDED_MASS_WATER = 3, CL1_PROBLEM_EXCLUDED_NUMERICAL_DERIVATIVES = 4,
	CL1_PROBLEM_NOT_SQUARE = 5, CL1_PROBLEM_NON_FINITE = 6, CL1_PROBLEM_ILL_CONDITIONED = 7,
	CL1_PROBLEM_SOLVE_FAILED = 8, CL1_PROBLEM_BACKWARD_ERROR = 9,
	CL1_PROBLEM_INEQUALITY_VIOLATED = 10, CL1_PROBLEM_SIGN_VIOLATED = 11,
	CL1_PROBLEM_COMPONENTWISE_BACKWARD_ERROR = 12, CL1_PROBLEM_OUTCOME_COUNT = 13
};

struct Cl1ProblemView {            /* cl1's input; valid only before cl1() runs */
	int k, l, m, n;                /* A rows, C rows, E rows, columns */
	int row_stride;                /* doubles per row of q */
	const LDBLE *q;                /* rows 0..k+l+m-1; columns 0..n-1; right-hand side in column n */
	const LDBLE *variable_signs;   /* delta1 on entry: -1, 0 or +1 per column */
};

struct Cl1ProblemCandidate {
	int outcome = CL1_PROBLEM_NOT_SQUARE;
	int live_column_count = 0;
	bool solved = false;               /* true if an LU solution exists in x */
	std::vector<LDBLE> x;              /* length n; 0 on non-live columns */
	std::vector<char> live;            /* length n; 1 for live columns */
	double reciprocal_condition = -1.0, backward_error = -1.0;
	double equilibrated_reciprocal_condition = -1.0;   /* Phase 1B Task 1b: row+column metric */
	double componentwise_error = -1.0;                 /* Phase 1B Task 2: max over A and C rows */
	double largest_violation = 0.0;    /* of the most violated E row or sign restriction */
	int violated_row = -1;             /* row index in q of the most violated E row, or -1 */
	int violated_column = -1;          /* column of the most violated sign restriction, or -1 */
};

/* Never modifies the view. Uses `solver` (its own LapackNewtonSolver, configured from the options)
   and `augmented` as reusable workspace. */
void evaluate_cl1_problem(LapackNewtonSolver *solver, const Cl1ProblemView &view,
                          Cl1ProblemCandidate &candidate, std::vector<LDBLE> &augmented,
                          std::vector<LDBLE> &live_solution, double max_componentwise_error);

/* For an ACCEPT candidate: writes the solution to x_out[0..n-1], residuals to res_out for all
   k+l+m rows (b - A x, d - C x, f - E x), and returns the sum of |A-row residuals|. */
double write_cl1_problem_solution(const Cl1ProblemView &view, const Cl1ProblemCandidate &candidate,
                                  LDBLE *x_out, LDBLE *res_out);
#endif
