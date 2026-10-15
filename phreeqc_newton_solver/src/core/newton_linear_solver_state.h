#pragma once
#ifdef USE_NEWTON_LINEAR_SOLVER
#include <cstdio>
#include <vector>
#include "phrqtype.h"
#include "linear_solve_status.h"
#include "newton_linear_solver_options.h"

#ifdef USE_PETSC
struct PetscNewtonSolver;
#endif

#ifdef USE_LAPACK_NEWTON_SOLVER
struct LapackNewtonSolver;
#include "cl1_problem_check.h"   /* Cl1ProblemCandidate, Cl1ProblemView, Cl1ProblemOutcome */
#endif

/* Why a call to ineq() did not use the replacement solver (decisions D8, D17). Same order and
   meaning as Phase 1; only the names changed. */
enum NewtonSolverIneligibilityReason {
	NEWTON_SOLVER_ELIGIBLE = 0,
	NEWTON_SOLVER_NOT_FIRST_ATTEMPT,
	NEWTON_SOLVER_REMOVING_UNSTABLE_PHASES,
	NEWTON_SOLVER_PITZER_OR_SIT_MODEL,
	NEWTON_SOLVER_MASS_WATER_SWITCH,
	NEWTON_SOLVER_AQUEOUS_ONLY_ITERATION,
	NEWTON_SOLVER_NUMERICAL_DERIVATIVES,
	NEWTON_SOLVER_EMPTY_SYSTEM,
	NEWTON_SOLVER_INELIGIBLE_UNKNOWN_TYPE,
	NEWTON_SOLVER_REASON_COUNT
};

struct NewtonLinearSolverState {
	NewtonLinearSolverOptions options;
	NewtonLinearSolverBackend backend = NEWTON_BACKEND_CL1;   /* effective backend */
	unsigned long object_index = 0;   /* process-wide creation index (Task 3) */
#ifdef USE_PETSC
	PetscNewtonSolver *petsc_solver = nullptr;
#endif
#ifdef USE_LAPACK_NEWTON_SOLVER
	LapackNewtonSolver *lapack_solver = nullptr;
	/* Phase 1B Task 1: cl1-problem shadow mode. A separate solver, so that the Phase 1A
	   counters on lapack_solver stay unchanged. */
	LapackNewtonSolver *cl1_problem_solver = nullptr;
	Cl1ProblemView cl1_problem_view;                 /* view stored by prepare (q pointers dangle after cl1) */
	Cl1ProblemCandidate cl1_problem_candidate;
	std::vector<LDBLE> cl1_problem_augmented;        /* workspace for evaluate_cl1_problem */
	std::vector<LDBLE> cl1_problem_live_solution;    /* workspace for evaluate_cl1_problem */
	/* Phase 1B Task 1c: copy of rows 0..k+l+m-1 of q (columns 0..n-1, right-hand side in
	   column n, packed with stride n + 1), made in prepare before cl1() destroys ineq_array.
	   Valid only when cl1_problem_copy_valid; the dimensions of the copied problem. */
	std::vector<LDBLE> cl1_problem_q_copy;
	bool cl1_problem_copy_valid = false;
	int cl1_problem_copy_k = 0, cl1_problem_copy_l = 0, cl1_problem_copy_m = 0, cl1_problem_copy_n = 0;
	bool cl1_problem_pending = false;
	std::vector<int> cl1_problem_unknown_types;  /* stored by prepare, read by record; must live until record (OV-D2) */
	const int *cl1_problem_back_eq = nullptr;
	FILE *cl1_problem_log = nullptr;
	unsigned long cl1_problem_record_count = 0;
	unsigned long cl1_problem_steer_call_count = 0;   /* Phase 1B Task 2: steering-mode calls */
	double cl1_problem_steer_seconds = 0.0;           /* Phase 1B Task 3: time in the steering evaluation */
	unsigned long cl1_problem_outcome_count[CL1_PROBLEM_OUTCOME_COUNT] = {};
	unsigned long accept_agree = 0, accept_disagree = 0, accept_cl1_failed = 0;
	double max_accept_relative_difference = 0.0, max_excluded_cl1_value = 0.0;
#endif
	std::vector<LDBLE> candidate_newton_step;
	FILE *comparison_log_file = nullptr;
	unsigned long comparison_call_count = 0;
	double largest_relative_difference = 0.0;
	unsigned long ineligible_count_by_reason[NEWTON_SOLVER_REASON_COUNT] = {};
	unsigned long ineligible_system_count = 0;
	unsigned long solve_count = 0, accepted_count = 0, cl1_fallback_count = 0,
	              non_finite_input_count = 0;
	double last_backward_error = -1.0;   /* of the last solve; -1 if none */
	double last_reciprocal_condition = -1.0;   /* of the last solve; -1 if not computed (Task 2d) */
};

NewtonLinearSolverState *create_newton_linear_solver_state(void);
void destroy_newton_linear_solver_state(NewtonLinearSolverState **state);
LinearSolveStatus solve_newton_step_with_backend(NewtonLinearSolverState *state,
	const LDBLE *augmented_newton_matrix, int count_unknowns, LDBLE *output);
void count_ineligible_newton_system(NewtonLinearSolverState *state, int reason);
int compare_newton_steps(const LDBLE *petsc_step, const LDBLE *cl1_step, int system_size,
	double *relative_difference, double *relative_sum);
void write_step_comparison_record(NewtonLinearSolverState *state, int system_size,
	LinearSolveStatus solve_status, int cl1_return_code, double backward_error,
	double relative_difference, double relative_sum, int largest_difference_index,
	int largest_difference_unknown_type);

/* Phase 1B Task 1: cl1-problem shadow mode hooks, called from ineq_cl1_original around the
   cl1() call (B-D2). Declared unconditionally; without USE_LAPACK_NEWTON_SOLVER,
   cl1_problem_shadow_active returns false and the others do nothing. */
bool cl1_problem_shadow_active(const NewtonLinearSolverState *state);
void cl1_problem_shadow_prepare(NewtonLinearSolverState *state, int in_kode, int pitzer_or_sit,
	int mass_water_switch_on, int numerical_derivatives_on, int k, int l, int m, int n, int row_stride,
	const LDBLE *q, const LDBLE *variable_signs, const std::vector<int> &unknown_types, const int *back_eq);
void cl1_problem_shadow_record(NewtonLinearSolverState *state, int cl1_kode, const LDBLE *cl1_x);

/* Phase 1B Task 2: cl1-problem steering mode (B-D10). Declared unconditionally; without
   USE_LAPACK_NEWTON_SOLVER, cl1_problem_steering_active returns false and cl1_problem_steer
   returns 0. */
bool cl1_problem_steering_active(const NewtonLinearSolverState *state);
int cl1_problem_steer(NewtonLinearSolverState *state, int in_kode, int pitzer_or_sit,
	int mass_water_switch_on, int numerical_derivatives_on, int k, int l, int m, int n, int row_stride,
	const LDBLE *q, LDBLE *delta1, LDBLE *res, int *l_kode, int *l_iter, LDBLE *l_error);
#endif
