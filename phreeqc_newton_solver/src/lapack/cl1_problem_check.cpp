/* Phase 1B Task 1: the cl1-problem check (shadow mode). No PETSc anywhere.
   Algorithm: phase1b_design.md sections 2-5, as specified in
   implementation_tasks/phase1b_task1_cl1_problem_shadow_mode.md. */
#include "cl1_problem_check.h"

#ifdef USE_LAPACK_NEWTON_SOLVER

#include <cmath>

void evaluate_cl1_problem(LapackNewtonSolver *solver, const Cl1ProblemView &view,
                          Cl1ProblemCandidate &candidate, std::vector<LDBLE> &augmented,
                          std::vector<LDBLE> &live_solution, double max_componentwise_error)
{
	/* 1. Reset the candidate. Resize x and live to n. */
	candidate = Cl1ProblemCandidate();
	const int n = view.n;
	candidate.x.assign((size_t)n, 0.0);
	candidate.live.assign((size_t)n, 0);
	if (solver == nullptr || n < 0 || view.q == nullptr || view.variable_signs == nullptr)
		return;   /* candidate.outcome stays NOT_SQUARE */

	/* 2. Live columns: column j is live if any of the rows 0..k+l+m-1 has q[r*row_stride+j] != 0. */
	const int row_count = view.k + view.l + view.m;
	std::vector<int> J;   /* live columns in increasing order */
	for (int j = 0; j < n; ++j) {
		bool is_live = false;
		for (int r = 0; r < row_count; ++r) {
			if (view.q[(size_t)r * view.row_stride + j] != 0.0) {
				is_live = true;
				break;
			}
		}
		if (is_live) {
			candidate.live[(size_t)j] = 1;
			J.push_back(j);
		}
	}
	const int p = (int)J.size();
	candidate.live_column_count = p;

	/* 3. Squareness: the A and C rows must match the live columns. */
	if (view.k + view.l != p) {
		candidate.outcome = CL1_PROBLEM_NOT_SQUARE;
		return;
	}

	/* 4. Build the p x p system in the PHREEQC augmented layout (row-major, leading
	   dimension p + 1). Row i (i = 0..p-1) is q row i (A rows first, then C rows)
	   restricted to J, with right-hand side q[i*row_stride + n]. */
	augmented.assign((size_t)p * (size_t)(p + 1), 0.0);
	for (int i = 0; i < p; ++i) {
		for (int c = 0; c < p; ++c)
			augmented[(size_t)i * (p + 1) + c] = view.q[(size_t)i * view.row_stride + J[(size_t)c]];
		augmented[(size_t)i * (p + 1) + p] = view.q[(size_t)i * view.row_stride + n];
	}

	/* 5. Solve with the LAPACK backend (its own condition gate applies). */
	live_solution.assign((size_t)p, 0.0);
	LinearSolveStatus status = solve_newton_step_with_lapack(solver, augmented.data(), p, live_solution.data());
	candidate.reciprocal_condition = solver->last_reciprocal_condition;
	candidate.equilibrated_reciprocal_condition = solver->last_equilibrated_reciprocal_condition;
	candidate.backward_error = solver->last_backward_error;
	if (status == LINEAR_SOLVE_NON_FINITE_INPUT) {
		candidate.outcome = CL1_PROBLEM_NON_FINITE;
		return;
	}
	if (status == LINEAR_SOLVE_FALL_BACK_TO_CL1) {
		/* Phase 1B Task 3: classify with the metric the gate used. In the fast
		   two-sided mode last_reciprocal_condition is not computed (-1). */
		const double gate_metric = solver->gate_on_equilibrated_condition
			? solver->last_equilibrated_reciprocal_condition
			: solver->last_reciprocal_condition;
		if (solver->condition_check_mode == NEWTON_CONDITION_CHECK_GATE
		    && gate_metric >= 0.0
		    && gate_metric < solver->min_reciprocal_condition)
			candidate.outcome = CL1_PROBLEM_ILL_CONDITIONED;
		else if (solver->last_backward_error > solver->max_backward_error)
			candidate.outcome = CL1_PROBLEM_BACKWARD_ERROR;
		else
			candidate.outcome = CL1_PROBLEM_SOLVE_FAILED;
		return;
	}

	/* 6. Accepted solve: x[J[i]] = live_solution[i], x = 0 on the non-live columns. */
	candidate.solved = true;
	for (int i = 0; i < p; ++i)
		candidate.x[(size_t)J[(size_t)i]] = live_solution[(size_t)i];

	/* 6b. Componentwise backward error over the A and C rows, full row (all n columns),
	   x = candidate.x (Phase 1B Task 2). A row whose denominator is 0 gives 0. */
	double componentwise_error = 0.0;
	for (int r = 0; r < view.k + view.l; ++r) {
		LDBLE residual = view.q[(size_t)r * view.row_stride + n];
		LDBLE term_sum = 0.0;
		for (int j = 0; j < n; ++j) {
			LDBLE term = view.q[(size_t)r * view.row_stride + j] * candidate.x[(size_t)j];
			residual -= term;
			term_sum += std::fabs(term);
		}
		LDBLE denominator = std::fabs(view.q[(size_t)r * view.row_stride + n]) + term_sum;
		double ratio = denominator > 0.0 ? (double)std::fabs(residual) / (double)denominator : 0.0;
		if (ratio > componentwise_error)
			componentwise_error = ratio;
	}
	candidate.componentwise_error = componentwise_error;
	if (componentwise_error > max_componentwise_error) {
		candidate.outcome = CL1_PROBLEM_COMPONENTWISE_BACKWARD_ERROR;
		return;
	}

	/* 7. E rows (r = k+l .. k+l+m-1): slack = q[r*row_stride+n] - sum_j q[r*row_stride+j]*x[j].
	   Every row with slack < 0 is a violation of size -slack. No tolerance. */
	for (int r = view.k + view.l; r < row_count; ++r) {
		LDBLE slack = view.q[(size_t)r * view.row_stride + n];
		for (int j = 0; j < n; ++j)
			slack -= view.q[(size_t)r * view.row_stride + j] * candidate.x[(size_t)j];
		if (slack < 0.0 && -slack > candidate.largest_violation) {
			candidate.largest_violation = -slack;
			candidate.violated_row = r;
		}
	}
	if (candidate.violated_row >= 0) {
		candidate.outcome = CL1_PROBLEM_INEQUALITY_VIOLATED;
		return;
	}

	/* 8. Signs: variable_signs[j] < 0 && x[j] > 0, or variable_signs[j] > 0 && x[j] < 0. */
	for (int j = 0; j < n; ++j) {
		double violation = 0.0;
		if (view.variable_signs[j] < 0.0 && candidate.x[(size_t)j] > 0.0)
			violation = (double)candidate.x[(size_t)j];
		else if (view.variable_signs[j] > 0.0 && candidate.x[(size_t)j] < 0.0)
			violation = (double)-candidate.x[(size_t)j];
		if (violation > candidate.largest_violation) {
			candidate.largest_violation = violation;
			candidate.violated_column = j;
		}
	}
	if (candidate.violated_column >= 0) {
		candidate.outcome = CL1_PROBLEM_SIGN_VIOLATED;
		return;
	}

	/* 9. Otherwise the candidate is accepted. */
	candidate.outcome = CL1_PROBLEM_ACCEPT;
}

double write_cl1_problem_solution(const Cl1ProblemView &view, const Cl1ProblemCandidate &candidate,
                                  LDBLE *x_out, LDBLE *res_out)
{
	const int n = view.n;
	const int row_count = view.k + view.l + view.m;
	for (int j = 0; j < n; ++j)
		x_out[j] = candidate.x[(size_t)j];
	double sum_a_residuals = 0.0;
	for (int r = 0; r < row_count; ++r) {
		LDBLE residual = view.q[(size_t)r * view.row_stride + n];
		for (int j = 0; j < n; ++j)
			residual -= view.q[(size_t)r * view.row_stride + j] * candidate.x[(size_t)j];
		res_out[r] = residual;
		if (r < view.k)
			sum_a_residuals += (double)std::fabs(residual);
	}
	return sum_a_residuals;
}

#endif
