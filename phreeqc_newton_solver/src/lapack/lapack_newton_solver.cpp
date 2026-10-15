/* Phase 2 Task 1b: LAPACK direct Newton linear-solver backend (decisions D19, P2-D10).
   Calls LAPACK dgetrf/dgetrs directly on the augmented Newton matrix. No PETSc, no
   global or static state, reentrant. Same row equilibration and backward-error
   acceptance as the PETSc adapter. */
#include "lapack_newton_solver.h"

#ifdef USE_LAPACK_NEWTON_SOLVER

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <new>

/* LAPACK prototypes. The trailing size_t is the hidden length of the character
   argument, as passed by gfortran-built LAPACK. On x86-64 it is harmless for
   C-implemented LAPACK. */
extern "C" {
void dgetrf_(const int *m, const int *n, double *a, const int *lda, int *ipiv, int *info);
void dgetrs_(const char *trans, const int *n, const int *nrhs, const double *a, const int *lda,
             const int *ipiv, double *b, const int *ldb, int *info, size_t trans_length);
void dgecon_(const char *norm, const int *n, const double *a, const int *lda, const double *anorm,
             double *rcond, double *work, int *iwork, int *info, size_t norm_length);
}

LapackNewtonSolver *create_lapack_newton_solver(void)
{
	return new (std::nothrow) LapackNewtonSolver;
}

void destroy_lapack_newton_solver(LapackNewtonSolver **solver)
{
	if (solver == nullptr || *solver == nullptr)
		return;
	LapackNewtonSolver *s = *solver;
	*solver = nullptr;

	if (s->print_statistics) {
		std::fprintf(stderr,
		             "phreeqc_lapack_statistics: solves=%lu accepted=%lu cl1_fallbacks=%lu "
		             "non_finite_inputs=%lu lapack_errors=%lu ill_conditioned=%lu\n",
		             s->solve_count, s->accepted_count, s->cl1_fallback_count,
		             s->non_finite_input_count, s->lapack_error_count, s->ill_conditioned_count);
	}
	delete s;
}

LinearSolveStatus solve_newton_step_with_lapack(LapackNewtonSolver *solver,
	const LDBLE *augmented_newton_matrix, int count_unknowns, LDBLE *output)
{
	if (solver == nullptr || augmented_newton_matrix == nullptr || output == nullptr)
		return LINEAR_SOLVE_FALL_BACK_TO_CL1;

	const int n = count_unknowns;
	const int ld = n + 1;

	/* 1. Count and check size. */
	solver->solve_count++;
	solver->last_backward_error = -1.0;
	solver->last_reciprocal_condition = -1.0;
	solver->last_equilibrated_reciprocal_condition = -1.0;
	if (n <= 0) {
		solver->cl1_fallback_count++;
		return LINEAR_SOLVE_FALL_BACK_TO_CL1;
	}

	/* J(i,j) = M[i*ld + j], r(i) = M[i*ld + n]. */
	/* 2. Finiteness. */
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++)
			if (!std::isfinite((double)augmented_newton_matrix[i * ld + j])) {
				solver->non_finite_input_count++;
				return LINEAR_SOLVE_NON_FINITE_INPUT;
			}
		if (!std::isfinite((double)augmented_newton_matrix[i * ld + n])) {
			solver->non_finite_input_count++;
			return LINEAR_SOLVE_NON_FINITE_INPUT;
		}
	}

	/* 3. Buffers (std::vector::resize reuses capacity between calls). */
	solver->scaled_matrix.resize((size_t)n * (size_t)n);
	solver->factor_matrix.resize((size_t)n * (size_t)n);
	solver->scaled_rhs.resize((size_t)n);
	solver->solution.resize((size_t)n);
	solver->pivot_indices.resize((size_t)n);

	/* 4. Row equilibration. Column-major write: j*n + i, not i*n + j. */
	for (int i = 0; i < n; i++) {
		double row_max = 0.0;
		for (int j = 0; j < n; j++) {
			double value = std::fabs((double)augmented_newton_matrix[i * ld + j]);
			if (value > row_max)
				row_max = value;
		}
		if (row_max == 0.0) {
			solver->cl1_fallback_count++;
			return LINEAR_SOLVE_FALL_BACK_TO_CL1;
		}
		double scale = solver->use_row_equilibration ? 1.0 / row_max : 1.0;
		for (int j = 0; j < n; j++)
			solver->scaled_matrix[(size_t)j * (size_t)n + (size_t)i] =
				scale * (double)augmented_newton_matrix[i * ld + j];
		solver->scaled_rhs[(size_t)i] = scale * (double)augmented_newton_matrix[i * ld + n];
	}

	/* 5. Copies. */
	solver->factor_matrix = solver->scaled_matrix;
	solver->solution = solver->scaled_rhs;

	/* 6. Factor. */
	int info = 0;
	dgetrf_(&n, &n, solver->factor_matrix.data(), &n, solver->pivot_indices.data(), &info);
	if (info < 0)
		solver->lapack_error_count++;
	if (info != 0) {   /* info > 0: exactly singular matrix */
		solver->cl1_fallback_count++;
		return LINEAR_SOLVE_FALL_BACK_TO_CL1;
	}

	/* 6a-fast. Two-sided metric from the factors already computed (Phase 1B Task 3,
	   decision B-D11). Partial pivoting is invariant to column scaling: if P*A = L*U
	   then P*A*D^-1 = L*(U*D^-1), so dgecon applied to L and the column-scaled U
	   gives the row-and-column metric with no second factorization. Active only when
	   equilibrated_condition_from_factors is set and the full method is not requested;
	   in that case the row metric of 6b is skipped and last_reciprocal_condition
	   stays -1. */
	const bool fast_equilibrated = solver->equilibrated_condition_from_factors
		&& !solver->compute_equilibrated_condition;
	if (fast_equilibrated) {
		std::vector<double> column_max((size_t)n);
		bool zero_column = false;
		for (int j = 0; j < n; j++) {   /* column maxima of the row-equilibrated matrix */
			double column_max_j = 0.0;
			for (int i = 0; i < n; i++) {
				double value = std::fabs(solver->scaled_matrix[(size_t)j * (size_t)n + (size_t)i]);
				if (value > column_max_j)
					column_max_j = value;
			}
			if (column_max_j == 0.0)
				zero_column = true;
			column_max[(size_t)j] = column_max_j;
		}
		if (zero_column) {
			solver->last_equilibrated_reciprocal_condition = -1.0;
		} else {
			/* L entries (row i > column j) unchanged, U entries (i <= j) scaled. */
			solver->equilibrated_matrix.resize((size_t)n * (size_t)n);
			for (int j = 0; j < n; j++)
				for (int i = 0; i < n; i++) {
					double value = solver->factor_matrix[(size_t)j * (size_t)n + (size_t)i];
					if (i <= j)
						value /= column_max[(size_t)j];
					solver->equilibrated_matrix[(size_t)j * (size_t)n + (size_t)i] = value;
				}
			/* 1-norm of the row-equilibrated matrix with each column divided by its maximum. */
			double equilibrated_one_norm = 0.0;
			for (int j = 0; j < n; j++) {
				double column_sum = 0.0;
				for (int i = 0; i < n; i++)
					column_sum += std::fabs(solver->scaled_matrix[(size_t)j * (size_t)n + (size_t)i])
						/ column_max[(size_t)j];
				if (column_sum > equilibrated_one_norm)
					equilibrated_one_norm = column_sum;
			}
			solver->condition_work.resize(4 * (size_t)n);
			solver->condition_iwork.resize((size_t)n);
			double rcond = 0.0;
			int equilibrated_info = 0;
			dgecon_("1", &n, solver->equilibrated_matrix.data(), &n, &equilibrated_one_norm,
			        &rcond, solver->condition_work.data(), solver->condition_iwork.data(),
			        &equilibrated_info, 1);
			if (equilibrated_info != 0) {
				solver->lapack_error_count++;
				solver->last_equilibrated_reciprocal_condition = -1.0;
			} else {
				solver->last_equilibrated_reciprocal_condition = rcond;
			}
		}
	} else if (solver->compute_equilibrated_condition || solver->gate_on_equilibrated_condition) {
		std::vector<double> column_max((size_t)n);
		bool zero_column = false;
		for (int j = 0; j < n; j++) {   /* column-major: column j is at j*n */
			double column_max_j = 0.0;
			for (int i = 0; i < n; i++) {
				double value = std::fabs(solver->scaled_matrix[(size_t)j * (size_t)n + (size_t)i]);
				if (value > column_max_j)
					column_max_j = value;
			}
			if (column_max_j == 0.0)
				zero_column = true;
			column_max[(size_t)j] = column_max_j;
		}
		if (zero_column) {
			solver->last_equilibrated_reciprocal_condition = -1.0;
		} else {
			solver->equilibrated_matrix.resize((size_t)n * (size_t)n);
			for (int j = 0; j < n; j++)
				for (int i = 0; i < n; i++)
					solver->equilibrated_matrix[(size_t)j * (size_t)n + (size_t)i] =
						solver->scaled_matrix[(size_t)j * (size_t)n + (size_t)i] / column_max[(size_t)j];
			double equilibrated_one_norm = 0.0;
			for (int j = 0; j < n; j++) {
				double column_sum = 0.0;
				for (int i = 0; i < n; i++)
					column_sum += std::fabs(solver->equilibrated_matrix[(size_t)j * (size_t)n + (size_t)i]);
				if (column_sum > equilibrated_one_norm)
					equilibrated_one_norm = column_sum;
			}
			solver->equilibrated_pivots.resize((size_t)n);
			int equilibrated_info = 0;
			dgetrf_(&n, &n, solver->equilibrated_matrix.data(), &n,
			        solver->equilibrated_pivots.data(), &equilibrated_info);
			double equilibrated_reciprocal_condition = -1.0;
			if (equilibrated_info != 0) {
				solver->lapack_error_count++;
			} else {
				solver->condition_work.resize(4 * (size_t)n);
				solver->condition_iwork.resize((size_t)n);
				double rcond = 0.0;
				dgecon_("1", &n, solver->equilibrated_matrix.data(), &n, &equilibrated_one_norm,
				        &rcond, solver->condition_work.data(), solver->condition_iwork.data(),
				        &equilibrated_info, 1);
				if (equilibrated_info != 0)
					solver->lapack_error_count++;
				else
					equilibrated_reciprocal_condition = rcond;
			}
			solver->last_equilibrated_reciprocal_condition = equilibrated_reciprocal_condition;
		}
	}

	/* 6b. Reciprocal condition of the factored matrix (Task 2d, design section 6.6).
	   The 1-norm is that of the row-equilibrated matrix before factorization;
	   scaled_matrix is not overwritten by dgetrf (factor_matrix holds the factors).
	   Skipped in the fast two-sided mode (Phase 1B Task 3): last_reciprocal_condition
	   stays -1 and the gate below uses the metric from 6a-fast. */
	if (solver->condition_check_mode != NEWTON_CONDITION_CHECK_OFF) {
		if (!fast_equilibrated) {
			double matrix_one_norm = 0.0;
			for (int j = 0; j < n; j++) {   /* column-major: column j is at j*n */
				double column_sum = 0.0;
				for (int i = 0; i < n; i++)
					column_sum += std::fabs(solver->scaled_matrix[(size_t)j * (size_t)n + (size_t)i]);
				if (column_sum > matrix_one_norm)
					matrix_one_norm = column_sum;
			}
			solver->condition_work.resize(4 * (size_t)n);
			solver->condition_iwork.resize((size_t)n);
			double reciprocal_condition = 0.0;
			info = 0;
			dgecon_("1", &n, solver->factor_matrix.data(), &n, &matrix_one_norm, &reciprocal_condition,
			        solver->condition_work.data(), solver->condition_iwork.data(), &info, 1);
			if (info != 0) {
				reciprocal_condition = -1.0;
				solver->lapack_error_count++;
			}
			solver->last_reciprocal_condition = reciprocal_condition;
		}
		/* Phase 1B Task 1b: the gate uses the row metric (unchanged default) or the
		   row-and-column-equilibrated metric, per gate_on_equilibrated_condition. */
		double gate_reciprocal_condition = solver->gate_on_equilibrated_condition
			? solver->last_equilibrated_reciprocal_condition
			: solver->last_reciprocal_condition;
		if (solver->condition_check_mode == NEWTON_CONDITION_CHECK_GATE
		    && gate_reciprocal_condition >= 0.0
		    && gate_reciprocal_condition < solver->min_reciprocal_condition) {
			solver->ill_conditioned_count++;
			solver->cl1_fallback_count++;
			return LINEAR_SOLVE_FALL_BACK_TO_CL1;   /* dgetrs is not called; output unchanged */
		}
	}

	/* 7. Solve. */
	const int one = 1;
	info = 0;
	dgetrs_("N", &n, &one, solver->factor_matrix.data(), &n, solver->pivot_indices.data(),
	        solver->solution.data(), &n, &info, 1);
	if (info != 0) {
		solver->lapack_error_count++;
		solver->cl1_fallback_count++;
		return LINEAR_SOLVE_FALL_BACK_TO_CL1;
	}

	/* 8. Check the solution. */
	for (int i = 0; i < n; i++) {
		if (!std::isfinite(solver->solution[(size_t)i])) {
			solver->cl1_fallback_count++;
			return LINEAR_SOLVE_FALL_BACK_TO_CL1;
		}
	}

	/* 9. Backward error on the scaled system, infinity norms. */
	double residual_norm = 0.0, matrix_norm = 0.0, solution_norm = 0.0, rhs_norm = 0.0;
	for (int i = 0; i < n; i++) {
		double row_sum = 0.0, scaled_residual = solver->scaled_rhs[(size_t)i];
		for (int j = 0; j < n; j++) {
			double a_ij = solver->scaled_matrix[(size_t)j * (size_t)n + (size_t)i];
			row_sum += std::fabs(a_ij);
			scaled_residual -= a_ij * solver->solution[(size_t)j];
		}
		if (std::fabs(scaled_residual) > residual_norm)
			residual_norm = std::fabs(scaled_residual);
		if (row_sum > matrix_norm)
			matrix_norm = row_sum;
		if (std::fabs(solver->solution[(size_t)i]) > solution_norm)
			solution_norm = std::fabs(solver->solution[(size_t)i]);
		if (std::fabs(solver->scaled_rhs[(size_t)i]) > rhs_norm)
			rhs_norm = std::fabs(solver->scaled_rhs[(size_t)i]);
	}
	double denominator = matrix_norm * solution_norm + rhs_norm;
	double backward_error = denominator > 0.0
		? residual_norm / denominator
		: (residual_norm == 0.0 ? 0.0 : DBL_MAX);
	solver->last_backward_error = backward_error;

	/* 10. Accept or fall back. */
	if (backward_error <= solver->max_backward_error) {
		for (int i = 0; i < n; i++)
			output[i] = (LDBLE)solver->solution[(size_t)i];
		solver->accepted_count++;
		return LINEAR_SOLVE_ACCEPTED;
	}
	solver->cl1_fallback_count++;
	return LINEAR_SOLVE_FALL_BACK_TO_CL1;
}

#endif
