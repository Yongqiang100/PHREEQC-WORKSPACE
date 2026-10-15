#include "newton_linear_solver_state.h"

#ifdef USE_NEWTON_LINEAR_SOLVER

#include <chrono>
#include <cmath>
#include <mutex>
#include <new>
#include <unistd.h>   /* getpid (Task 3: comparison-log %p expansion) */

#ifdef USE_PETSC
#include "petsc_newton_solver.h"   /* create_petsc_newton_solver, solve_newton_step_with_petsc, destroy_petsc_newton_solver */
#endif

#ifdef USE_LAPACK_NEWTON_SOLVER
#include "lapack_newton_solver.h"   /* create_lapack_newton_solver, solve_newton_step_with_lapack, destroy_lapack_newton_solver */
#endif

/* Name of a condition-check mode, for the invalid-options warning (Task 2d). */
static const char *newton_condition_check_mode_name(NewtonConditionCheckMode mode)
{
	switch (mode)
	{
	case NEWTON_CONDITION_CHECK_LOG:
		return "log";
	case NEWTON_CONDITION_CHECK_GATE:
		return "gate";
	case NEWTON_CONDITION_CHECK_OFF:
	default:
		return "off";
	}
}

/* Task 3 (P2-D17): process-wide totals, guarded by one mutex. A summary line is printed
   when the last live state is destroyed and options.print_summary is set; the totals are
   then reset and the segment counter advanced. */
struct NewtonSolverProcessTotals {
	unsigned long objects = 0;
	unsigned long backend_lapack_objects = 0;
	unsigned long backend_petsc_objects = 0;
	unsigned long backend_cl1_objects = 0;
	unsigned long solves = 0;
	unsigned long accepted = 0;
	unsigned long cl1_fallbacks = 0;
	unsigned long non_finite_inputs = 0;
	unsigned long ineligible_systems = 0;
	unsigned long ill_conditioned = 0;
	unsigned long lapack_errors = 0;
	unsigned long reason[NEWTON_SOLVER_REASON_COUNT] = {};
};

static std::mutex newton_solver_accumulator_mutex;
static NewtonSolverProcessTotals newton_solver_process_totals;
static unsigned long newton_solver_live_state_count = 0;
static unsigned long newton_solver_next_object_index = 0;
static unsigned long newton_solver_summary_segment = 0;

NewtonLinearSolverState *create_newton_linear_solver_state(void)
{
	NewtonLinearSolverState *state = new (std::nothrow) NewtonLinearSolverState;
	if (state == nullptr)
		return nullptr;

	state->options = read_newton_linear_solver_options();
	if (!state->options.backend_name_valid)
		std::fprintf(stderr,
		             "phreeqc_linear_solver: warning: PHREEQC_LINEAR_SOLVER=%s is unknown or not built in; using cl1\n",
		             state->options.backend_name_as_given.c_str());
	if (!state->options.condition_options_valid)
		std::fprintf(stderr,
		             "phreeqc_linear_solver: warning: invalid PHREEQC_LINEAR_SOLVER_CONDITION_CHECK or PHREEQC_LINEAR_SOLVER_MIN_RCOND; using condition check %s and minimum %.6e\n",
		             newton_condition_check_mode_name(state->options.condition_check_mode),
		             state->options.min_reciprocal_condition);
	if (!state->options.intercept_mode_valid)
		std::fprintf(stderr,
		             "phreeqc_linear_solver: warning: invalid PHREEQC_LINEAR_SOLVER_INTERCEPT or PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG_EVERY; using intercept mode entry\n");

	state->backend = state->options.backend;

	{
		std::lock_guard<std::mutex> lock(newton_solver_accumulator_mutex);
		state->object_index = newton_solver_next_object_index++;
		++newton_solver_live_state_count;
	}

#ifdef USE_PETSC
	if (state->backend == NEWTON_BACKEND_PETSC) {
		/* create_petsc_newton_solver() acquires PETSc and reads the PETSc-internal options.
		   On failure *petsc_solver is nullptr and PETSc is not held. */
		if (create_petsc_newton_solver(&state->petsc_solver) != 0) {
			state->backend = NEWTON_BACKEND_CL1;   /* do not retry creation every Newton iteration */
		} else {
			state->petsc_solver->print_statistics = state->options.print_statistics;
		}
	}
#endif

#ifdef USE_LAPACK_NEWTON_SOLVER
	if (state->backend == NEWTON_BACKEND_LAPACK) {
		/* create_lapack_newton_solver() returns nullptr only if allocation fails. */
		state->lapack_solver = create_lapack_newton_solver();
		if (state->lapack_solver == nullptr) {
			state->backend = NEWTON_BACKEND_CL1;   /* do not retry creation every Newton iteration */
		} else {
			state->lapack_solver->print_statistics = state->options.print_statistics;
			state->lapack_solver->condition_check_mode = state->options.condition_check_mode;
			state->lapack_solver->min_reciprocal_condition = state->options.min_reciprocal_condition;
			/* Phase 1B Task 1b: metric choice for the gate. */
			state->lapack_solver->gate_on_equilibrated_condition = state->options.condition_metric_row_column;
		}
	}

	/* Phase 1B Task 1: shadow mode. A separate solver keeps the Phase 1A counters
	   unchanged; its statistics are never printed. Phase 1B Task 2: steering mode uses
	   the same solver configuration (B-D10); it has no shadow log. */
	if (state->options.intercept_mode == NEWTON_INTERCEPT_CL1_PROBLEM_SHADOW
	    || state->options.intercept_mode == NEWTON_INTERCEPT_CL1_PROBLEM) {
		state->cl1_problem_solver = create_lapack_newton_solver();
		if (state->cl1_problem_solver != nullptr) {
			state->cl1_problem_solver->condition_check_mode = state->options.condition_check_mode;
			state->cl1_problem_solver->min_reciprocal_condition = state->options.min_reciprocal_condition;
			state->cl1_problem_solver->print_statistics = false;
			/* Phase 1B Task 1b: the shadow solver always records the row-and-column
			   metric; the gate metric follows the option. */
			state->cl1_problem_solver->gate_on_equilibrated_condition = state->options.condition_metric_row_column;
			if (state->options.intercept_mode == NEWTON_INTERCEPT_CL1_PROBLEM) {
				/* Phase 1B Task 3 (B-D11): the steering path takes the two-sided metric
				   from the factors it already computed and skips the row metric. */
				state->cl1_problem_solver->compute_equilibrated_condition = false;
				state->cl1_problem_solver->equilibrated_condition_from_factors =
					(state->options.condition_metric_row_column == 1);
			} else {
				state->cl1_problem_solver->compute_equilibrated_condition = true;
			}
		}
		if (state->options.intercept_mode == NEWTON_INTERCEPT_CL1_PROBLEM_SHADOW) {
			std::string cl1_problem_log_path = expand_comparison_log_path(
				state->options.cl1_problem_log_path, current_process_rank_text(),
				state->object_index, (long)getpid());
			state->cl1_problem_log = std::fopen(cl1_problem_log_path.c_str(), "a");
			if (state->cl1_problem_log == nullptr) {
				std::fprintf(stderr, "phreeqc_linear_solver: cannot open cl1-problem shadow log \"%s\", counting without a log\n",
				             cl1_problem_log_path.c_str());
			} else {
				std::fprintf(state->cl1_problem_log,
				             "# call_index n k l m live outcome cl1_kode reciprocal_condition backward_error "
				             "relative_difference max_excluded_cl1 largest_violation violated_row_unknown_type "
				             "largest_difference_unknown_type reciprocal_condition_equilibrated "
				             "cl1_rel_residual_a cl1_rel_residual_c cl1_rel_violation_e "
				             "lu_rel_residual_ac min_abs_entry_m entries_below_1e-15\n");
			}
		}
	}
#endif

	if (state->options.compare_with_cl1 && state->backend != NEWTON_BACKEND_CL1) {
		std::string comparison_log_path = expand_comparison_log_path(
			state->options.comparison_log_path, current_process_rank_text(),
			state->object_index, (long)getpid());
		state->comparison_log_file = std::fopen(comparison_log_path.c_str(), "a");
		if (state->comparison_log_file == nullptr) {
			std::fprintf(stderr, "phreeqc_linear_solver: cannot open comparison log \"%s\", comparison mode disabled\n",
			             comparison_log_path.c_str());
			state->options.compare_with_cl1 = false;
		} else {
			std::fprintf(state->comparison_log_file,
			             "# call_index system_size solve_status cl1_return_code backward_error "
			             "relative_difference relative_sum largest_difference_index "
			             "largest_difference_unknown_type reciprocal_condition\n");
		}
	}
	return state;
}

void destroy_newton_linear_solver_state(NewtonLinearSolverState **state)
{
	if (state == nullptr || *state == nullptr)
		return;
	NewtonLinearSolverState *s = *state;
	*state = nullptr;

	if (s->options.print_statistics) {
		std::fprintf(stderr,
		             "phreeqc_linear_solver_ineligible: not_first_attempt=%lu removing_unstable_phases=%lu "
		             "pitzer_or_sit_model=%lu mass_water_switch=%lu aqueous_only_iteration=%lu "
		             "numerical_derivatives=%lu empty_system=%lu ineligible_unknown_type=%lu\n",
		             s->ineligible_count_by_reason[NEWTON_SOLVER_NOT_FIRST_ATTEMPT],
		             s->ineligible_count_by_reason[NEWTON_SOLVER_REMOVING_UNSTABLE_PHASES],
		             s->ineligible_count_by_reason[NEWTON_SOLVER_PITZER_OR_SIT_MODEL],
		             s->ineligible_count_by_reason[NEWTON_SOLVER_MASS_WATER_SWITCH],
		             s->ineligible_count_by_reason[NEWTON_SOLVER_AQUEOUS_ONLY_ITERATION],
		             s->ineligible_count_by_reason[NEWTON_SOLVER_NUMERICAL_DERIVATIVES],
		             s->ineligible_count_by_reason[NEWTON_SOLVER_EMPTY_SYSTEM],
		             s->ineligible_count_by_reason[NEWTON_SOLVER_INELIGIBLE_UNKNOWN_TYPE]);
		std::fprintf(stderr,
		             "phreeqc_linear_solver_statistics: backend=%s solves=%lu accepted=%lu cl1_fallbacks=%lu "
		             "non_finite_inputs=%lu ineligible_systems=%lu\n",
		             newton_linear_solver_backend_name(s->backend),
		             s->solve_count, s->accepted_count, s->cl1_fallback_count,
		             s->non_finite_input_count, s->ineligible_system_count);
#ifdef USE_LAPACK_NEWTON_SOLVER
		/* Phase 1B Task 1: shadow-mode statistics, only when shadow mode was active.
		   Phase 1B Task 2: the componentwise outcome is appended (B-D10). */
		if (s->options.intercept_mode == NEWTON_INTERCEPT_CL1_PROBLEM_SHADOW) {
			std::fprintf(stderr,
			             "phreeqc_cl1_problem_shadow: calls=%lu accept=%lu excluded_retry=%lu "
			             "excluded_pitzer_sit=%lu excluded_mass_water=%lu excluded_numerical_derivatives=%lu "
			             "not_square=%lu non_finite=%lu ill_conditioned=%lu solve_failed=%lu backward_error=%lu "
			             "inequality_violated=%lu sign_violated=%lu accept_agree=%lu accept_disagree=%lu "
			             "accept_cl1_failed=%lu max_accept_relative_difference=%.3e max_excluded_cl1_value=%.3e "
			             "componentwise_backward_error=%lu\n",
			             s->cl1_problem_record_count,
			             s->cl1_problem_outcome_count[CL1_PROBLEM_ACCEPT],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_EXCLUDED_RETRY],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_EXCLUDED_PITZER_SIT],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_EXCLUDED_MASS_WATER],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_EXCLUDED_NUMERICAL_DERIVATIVES],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_NOT_SQUARE],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_NON_FINITE],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_ILL_CONDITIONED],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_SOLVE_FAILED],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_BACKWARD_ERROR],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_INEQUALITY_VIOLATED],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_SIGN_VIOLATED],
			             s->accept_agree, s->accept_disagree, s->accept_cl1_failed,
			             s->max_accept_relative_difference, s->max_excluded_cl1_value,
			             s->cl1_problem_outcome_count[CL1_PROBLEM_COMPONENTWISE_BACKWARD_ERROR]);
		}
		/* Phase 1B Task 2: steering-mode statistics, only when steering mode was active.
		   Phase 1B Task 3: seconds= is the accumulated time of the steering evaluation. */
		if (s->options.intercept_mode == NEWTON_INTERCEPT_CL1_PROBLEM) {
			std::fprintf(stderr,
			             "phreeqc_cl1_problem_steer: calls=%lu accept=%lu excluded_retry=%lu "
			             "excluded_pitzer_sit=%lu excluded_mass_water=%lu excluded_numerical_derivatives=%lu "
			             "not_square=%lu non_finite=%lu ill_conditioned=%lu solve_failed=%lu backward_error=%lu "
			             "inequality_violated=%lu sign_violated=%lu componentwise_backward_error=%lu seconds=%.6f\n",
			             s->cl1_problem_steer_call_count,
			             s->cl1_problem_outcome_count[CL1_PROBLEM_ACCEPT],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_EXCLUDED_RETRY],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_EXCLUDED_PITZER_SIT],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_EXCLUDED_MASS_WATER],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_EXCLUDED_NUMERICAL_DERIVATIVES],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_NOT_SQUARE],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_NON_FINITE],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_ILL_CONDITIONED],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_SOLVE_FAILED],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_BACKWARD_ERROR],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_INEQUALITY_VIOLATED],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_SIGN_VIOLATED],
			             s->cl1_problem_outcome_count[CL1_PROBLEM_COMPONENTWISE_BACKWARD_ERROR],
			             s->cl1_problem_steer_seconds);
		}
#endif
	}

	if (s->comparison_log_file != nullptr) {
		std::fprintf(s->comparison_log_file, "# summary comparison_calls=%lu largest_relative_difference=%.6e\n",
		             s->comparison_call_count, s->largest_relative_difference);
		std::fclose(s->comparison_log_file);
		s->comparison_log_file = nullptr;
	}

#ifdef USE_LAPACK_NEWTON_SOLVER
	/* Phase 1B Task 1: close the shadow log and destroy the shadow solver. */
	if (s->cl1_problem_log != nullptr) {
		std::fclose(s->cl1_problem_log);
		s->cl1_problem_log = nullptr;
	}
#endif

	{
		std::lock_guard<std::mutex> lock(newton_solver_accumulator_mutex);
		newton_solver_process_totals.objects++;
		switch (s->backend) {
		case NEWTON_BACKEND_LAPACK:
			newton_solver_process_totals.backend_lapack_objects++;
			break;
		case NEWTON_BACKEND_PETSC:
			newton_solver_process_totals.backend_petsc_objects++;
			break;
		case NEWTON_BACKEND_CL1:
		default:
			newton_solver_process_totals.backend_cl1_objects++;
			break;
		}
		newton_solver_process_totals.solves += s->solve_count;
		newton_solver_process_totals.accepted += s->accepted_count;
		newton_solver_process_totals.cl1_fallbacks += s->cl1_fallback_count;
		newton_solver_process_totals.non_finite_inputs += s->non_finite_input_count;
		newton_solver_process_totals.ineligible_systems += s->ineligible_system_count;
		for (int r = 0; r < (int)NEWTON_SOLVER_REASON_COUNT; ++r)
			newton_solver_process_totals.reason[r] += s->ineligible_count_by_reason[r];
#ifdef USE_LAPACK_NEWTON_SOLVER
		if (s->lapack_solver != nullptr) {
			newton_solver_process_totals.ill_conditioned += s->lapack_solver->ill_conditioned_count;
			newton_solver_process_totals.lapack_errors += s->lapack_solver->lapack_error_count;
		}
#endif
		if (newton_solver_live_state_count > 0)
			--newton_solver_live_state_count;
		if (newton_solver_live_state_count == 0 && s->options.print_summary) {
			std::fprintf(stderr,
			             "phreeqc_linear_solver_process_summary: rank=%s segment=%lu objects=%lu "
			             "backend_lapack_objects=%lu backend_petsc_objects=%lu backend_cl1_objects=%lu "
			             "solves=%lu accepted=%lu cl1_fallbacks=%lu non_finite_inputs=%lu "
			             "ineligible_systems=%lu ill_conditioned=%lu lapack_errors=%lu "
			             "not_first_attempt=%lu removing_unstable_phases=%lu pitzer_or_sit_model=%lu "
			             "mass_water_switch=%lu aqueous_only_iteration=%lu numerical_derivatives=%lu "
			             "empty_system=%lu ineligible_unknown_type=%lu\n",
			             current_process_rank_text().c_str(),
			             newton_solver_summary_segment,
			             newton_solver_process_totals.objects,
			             newton_solver_process_totals.backend_lapack_objects,
			             newton_solver_process_totals.backend_petsc_objects,
			             newton_solver_process_totals.backend_cl1_objects,
			             newton_solver_process_totals.solves,
			             newton_solver_process_totals.accepted,
			             newton_solver_process_totals.cl1_fallbacks,
			             newton_solver_process_totals.non_finite_inputs,
			             newton_solver_process_totals.ineligible_systems,
			             newton_solver_process_totals.ill_conditioned,
			             newton_solver_process_totals.lapack_errors,
			             newton_solver_process_totals.reason[NEWTON_SOLVER_NOT_FIRST_ATTEMPT],
			             newton_solver_process_totals.reason[NEWTON_SOLVER_REMOVING_UNSTABLE_PHASES],
			             newton_solver_process_totals.reason[NEWTON_SOLVER_PITZER_OR_SIT_MODEL],
			             newton_solver_process_totals.reason[NEWTON_SOLVER_MASS_WATER_SWITCH],
			             newton_solver_process_totals.reason[NEWTON_SOLVER_AQUEOUS_ONLY_ITERATION],
			             newton_solver_process_totals.reason[NEWTON_SOLVER_NUMERICAL_DERIVATIVES],
			             newton_solver_process_totals.reason[NEWTON_SOLVER_EMPTY_SYSTEM],
			             newton_solver_process_totals.reason[NEWTON_SOLVER_INELIGIBLE_UNKNOWN_TYPE]);
			std::fflush(stderr);
			newton_solver_process_totals = NewtonSolverProcessTotals();
			++newton_solver_summary_segment;
		}
	}

#ifdef USE_PETSC
	destroy_petsc_newton_solver(&s->petsc_solver);   /* prints phreeqc_petsc_statistics: if its print_statistics is set */
#endif
#ifdef USE_LAPACK_NEWTON_SOLVER
	destroy_lapack_newton_solver(&s->lapack_solver);   /* prints phreeqc_lapack_statistics: if its print_statistics is set */
	destroy_lapack_newton_solver(&s->cl1_problem_solver);   /* Phase 1B Task 1 shadow solver; print_statistics is false */
#endif
	delete s;
}

void count_ineligible_newton_system(NewtonLinearSolverState *state, int reason)
{
	if (state == nullptr)
		return;
	if (reason > 0 && reason < (int)NEWTON_SOLVER_REASON_COUNT)
		state->ineligible_count_by_reason[reason]++;
	state->ineligible_system_count++;
#ifdef USE_PETSC
	if (state->petsc_solver != nullptr)
		state->petsc_solver->ineligible_system_count++;
#endif
}

LinearSolveStatus solve_newton_step_with_backend(NewtonLinearSolverState *state,
	const LDBLE *augmented_newton_matrix, int count_unknowns, LDBLE *output)
{
	if (state == nullptr)
		return LINEAR_SOLVE_FALL_BACK_TO_CL1;

	state->last_backward_error = -1.0;
	state->last_reciprocal_condition = -1.0;

	LinearSolveStatus solve_status = LINEAR_SOLVE_FALL_BACK_TO_CL1;
	switch (state->backend)
	{
#ifdef USE_PETSC
	case NEWTON_BACKEND_PETSC:
		solve_status = solve_newton_step_with_petsc(state->petsc_solver, augmented_newton_matrix,
		                                            count_unknowns, output);
		state->last_backward_error = (double)state->petsc_solver->last_backward_error;
		break;
#endif
#ifdef USE_LAPACK_NEWTON_SOLVER
	case NEWTON_BACKEND_LAPACK:
		solve_status = solve_newton_step_with_lapack(state->lapack_solver, augmented_newton_matrix,
		                                             count_unknowns, output);
		state->last_backward_error = state->lapack_solver->last_backward_error;
		state->last_reciprocal_condition = state->lapack_solver->last_reciprocal_condition;
		break;
#endif
	case NEWTON_BACKEND_CL1:
	default:
		break;
	}

	state->solve_count++;
	switch (solve_status)
	{
	case LINEAR_SOLVE_ACCEPTED:
		state->accepted_count++;
		break;
	case LINEAR_SOLVE_NON_FINITE_INPUT:
		state->non_finite_input_count++;
		break;
	case LINEAR_SOLVE_FALL_BACK_TO_CL1:
	default:
		state->cl1_fallback_count++;
		break;
	}
	return solve_status;
}

int compare_newton_steps(const LDBLE *petsc_step, const LDBLE *cl1_step, int system_size,
                         double *relative_difference, double *relative_sum)
{
    if (petsc_step == nullptr || cl1_step == nullptr || system_size <= 0) {
        if (relative_difference != nullptr) *relative_difference = 0.0;
        if (relative_sum != nullptr)       *relative_sum = 0.0;
        return -1;
    }
    double largest_difference = 0.0, largest_sum = 0.0, largest_cl1 = 0.0, largest_petsc = 0.0;
    int largest_difference_index = 0;
    for (int i = 0; i < system_size; ++i) {
        const double petsc_value = (double)petsc_step[i];
        const double cl1_value   = (double)cl1_step[i];
        const double difference  = std::fabs(petsc_value - cl1_value);
        const double sum         = std::fabs(petsc_value + cl1_value);
        if (difference > largest_difference) {
            largest_difference = difference;
            largest_difference_index = i;
        }
        if (sum > largest_sum)          largest_sum   = sum;
        if (std::fabs(cl1_value) > largest_cl1)   largest_cl1   = std::fabs(cl1_value);
        if (std::fabs(petsc_value) > largest_petsc) largest_petsc = std::fabs(petsc_value);
    }
    if (largest_cl1 == 0.0) {
        if (relative_difference != nullptr) *relative_difference = largest_petsc;
        if (relative_sum != nullptr)       *relative_sum = largest_petsc;
    } else {
        if (relative_difference != nullptr) *relative_difference = largest_difference / largest_cl1;
        if (relative_sum != nullptr)       *relative_sum = largest_sum / largest_cl1;
    }
    return largest_difference_index;
}

void write_step_comparison_record(NewtonLinearSolverState *state, int system_size,
                                  LinearSolveStatus solve_status, int cl1_return_code,
                                  double backward_error, double relative_difference,
                                  double relative_sum, int largest_difference_index,
                                  int largest_difference_unknown_type)
{
    if (state == nullptr || state->comparison_log_file == nullptr)
        return;
    state->comparison_call_count++;
    if (solve_status == LINEAR_SOLVE_ACCEPTED && relative_difference > state->largest_relative_difference)
        state->largest_relative_difference = relative_difference;

    /* 10th column (Task 2d): reciprocal condition of the last solve, written for every
       status including fallbacks, so that gated calls are visible; nan if not computed. */
    char reciprocal_condition_text[64];
    if (state->last_reciprocal_condition >= 0.0)
        std::snprintf(reciprocal_condition_text, sizeof(reciprocal_condition_text),
                      "%.6e", state->last_reciprocal_condition);
    else
        std::snprintf(reciprocal_condition_text, sizeof(reciprocal_condition_text), "nan");

    if (solve_status == LINEAR_SOLVE_ACCEPTED) {
        std::fprintf(state->comparison_log_file, "%lu %d %d %d %.6e %.6e %.6e %d %d %s\n",
                     state->comparison_call_count, system_size, (int)solve_status, cl1_return_code,
                     backward_error, relative_difference, relative_sum,
                     largest_difference_index, largest_difference_unknown_type,
                     reciprocal_condition_text);
    } else {
        std::fprintf(state->comparison_log_file, "%lu %d %d %d nan nan nan -1 -1 %s\n",
                     state->comparison_call_count, system_size, (int)solve_status, cl1_return_code,
                     reciprocal_condition_text);
    }
    std::fflush(state->comparison_log_file);
}

/* Phase 1B Task 1: cl1-problem shadow mode hooks (B-D2, B-D7). Declared for every
   USE_NEWTON_LINEAR_SOLVER build; without USE_LAPACK_NEWTON_SOLVER they are no-ops. */

bool cl1_problem_shadow_active(const NewtonLinearSolverState *state)
{
#ifdef USE_LAPACK_NEWTON_SOLVER
	return (state != nullptr
	        && state->options.intercept_mode == NEWTON_INTERCEPT_CL1_PROBLEM_SHADOW
	        && state->cl1_problem_solver != nullptr);
#else
	(void)state;
	return false;
#endif
}

/* Phase 1B Task 1c: relative residual of one equality row of the cl1 problem.
   row holds columns 0..n-1 followed by the right-hand side b at row[n]; z has length n.
   relative_residual = |b - sum_j a_j z_j| / (|b| + sum_j |a_j z_j|), 0 if the
   denominator is 0. */
static double cl1_problem_row_relative_residual(const LDBLE *row, const LDBLE *z, int n)
{
	const LDBLE b = row[n];
	LDBLE sum = 0.0, abs_sum = 0.0;
	for (int j = 0; j < n; ++j) {
		const LDBLE term = row[j] * z[j];
		sum += term;
		abs_sum += std::fabs(term);
	}
	const LDBLE denominator = std::fabs(b) + abs_sum;
	if (denominator == 0.0)
		return 0.0;
	return (double)std::fabs(b - sum) / (double)denominator;
}

/* Phase 1B Task 1c: relative violation of one E row, max(sum_j e_j z_j - f, 0) /
   (|f| + sum_j |e_j z_j|), 0 if the denominator is 0. row and z as above. */
static double cl1_problem_row_violation_e(const LDBLE *row, const LDBLE *z, int n)
{
	const LDBLE f = row[n];
	LDBLE sum = 0.0, abs_sum = 0.0;
	for (int j = 0; j < n; ++j) {
		const LDBLE term = row[j] * z[j];
		sum += term;
		abs_sum += std::fabs(term);
	}
	const LDBLE denominator = std::fabs(f) + abs_sum;
	if (denominator == 0.0)
		return 0.0;
	const LDBLE violation = sum - f;
	if (violation <= 0.0)
		return 0.0;
	return (double)violation / (double)denominator;
}

void cl1_problem_shadow_prepare(NewtonLinearSolverState *state, int in_kode, int pitzer_or_sit,
	int mass_water_switch_on, int numerical_derivatives_on, int k, int l, int m, int n, int row_stride,
	const LDBLE *q, const LDBLE *variable_signs, const std::vector<int> &unknown_types, const int *back_eq)
{
#ifdef USE_LAPACK_NEWTON_SOLVER
	if (!cl1_problem_shadow_active(state))
		return;
	state->cl1_problem_pending = true;
	state->cl1_problem_unknown_types = unknown_types;
	state->cl1_problem_back_eq = back_eq;
	state->cl1_problem_view.k = k;
	state->cl1_problem_view.l = l;
	state->cl1_problem_view.m = m;
	state->cl1_problem_view.n = n;
	state->cl1_problem_view.row_stride = row_stride;
	state->cl1_problem_view.q = q;
	state->cl1_problem_view.variable_signs = variable_signs;
	state->cl1_problem_candidate = Cl1ProblemCandidate();
	state->cl1_problem_copy_valid = false;
	state->cl1_problem_copy_k = 0;
	state->cl1_problem_copy_l = 0;
	state->cl1_problem_copy_m = 0;
	state->cl1_problem_copy_n = 0;
	if (in_kode != 1)
		state->cl1_problem_candidate.outcome = CL1_PROBLEM_EXCLUDED_RETRY;
	else if (pitzer_or_sit)
		state->cl1_problem_candidate.outcome = CL1_PROBLEM_EXCLUDED_PITZER_SIT;
	else if (mass_water_switch_on)
		state->cl1_problem_candidate.outcome = CL1_PROBLEM_EXCLUDED_MASS_WATER;
	else if (numerical_derivatives_on)
		state->cl1_problem_candidate.outcome = CL1_PROBLEM_EXCLUDED_NUMERICAL_DERIVATIVES;
	else {
		evaluate_cl1_problem(state->cl1_problem_solver, state->cl1_problem_view,
		                     state->cl1_problem_candidate, state->cl1_problem_augmented,
		                     state->cl1_problem_live_solution,
		                     state->options.cl1_problem_max_componentwise_error);
		/* Phase 1B Task 1c: if the evaluation reached the square-system step, keep a copy
		   of the problem. This must be done before cl1() runs: cl1() destroys ineq_array. */
		if (state->cl1_problem_candidate.live_column_count == k + l) {
			const int row_count = k + l + m;
			state->cl1_problem_q_copy.assign((size_t)row_count * (size_t)(n + 1), 0.0);
			for (int r = 0; r < row_count; ++r)
				for (int c = 0; c <= n; ++c)
					state->cl1_problem_q_copy[(size_t)r * (size_t)(n + 1) + (size_t)c] =
						q[(size_t)r * (size_t)row_stride + (size_t)c];
			state->cl1_problem_copy_k = k;
			state->cl1_problem_copy_l = l;
			state->cl1_problem_copy_m = m;
			state->cl1_problem_copy_n = n;
			state->cl1_problem_copy_valid = true;
		}
	}
#else
	(void)state; (void)in_kode; (void)pitzer_or_sit; (void)mass_water_switch_on;
	(void)numerical_derivatives_on; (void)k; (void)l; (void)m; (void)n; (void)row_stride;
	(void)q; (void)variable_signs; (void)unknown_types; (void)back_eq;
#endif
}

void cl1_problem_shadow_record(NewtonLinearSolverState *state, int cl1_kode, const LDBLE *cl1_x)
{
#ifdef USE_LAPACK_NEWTON_SOLVER
	if (state == nullptr || !state->cl1_problem_pending)
		return;
	Cl1ProblemCandidate &candidate = state->cl1_problem_candidate;
	const int n = (int)candidate.x.size();

	/* 1. Count the outcome. */
	if (candidate.outcome >= 0 && candidate.outcome < (int)CL1_PROBLEM_OUTCOME_COUNT)
		state->cl1_problem_outcome_count[candidate.outcome]++;

	/* 2. If an LU solution exists, compare it with cl1()'s result on the live columns. */
	bool have_comparison = false;
	double relative_difference = 0.0, max_excluded = 0.0;
	int largest_difference_column = -1;
	if (candidate.solved) {
		double largest_difference = 0.0, largest_cl1 = 0.0;
		for (int j = 0; j < n; ++j) {
			if (candidate.live[(size_t)j]) {
				const double difference = std::fabs((double)candidate.x[(size_t)j] - (double)cl1_x[j]);
				if (difference > largest_difference) {
					largest_difference = difference;
					largest_difference_column = j;
				}
				if (std::fabs((double)cl1_x[j]) > largest_cl1)
					largest_cl1 = std::fabs((double)cl1_x[j]);
			} else if (std::fabs((double)cl1_x[j]) > max_excluded) {
				max_excluded = std::fabs((double)cl1_x[j]);
			}
		}
		relative_difference = largest_difference / (largest_cl1 > 1.0e-300 ? largest_cl1 : 1.0e-300);
		have_comparison = true;
	}

	/* 3. Accepted records: agreement with cl1(). */
	bool accept_disagree_record = false, accept_cl1_failed_record = false;
	if (candidate.outcome == CL1_PROBLEM_ACCEPT) {
		if (cl1_kode != 0) {
			state->accept_cl1_failed++;
			accept_cl1_failed_record = true;
		} else if (relative_difference <= 1.0e-10) {
			state->accept_agree++;
		} else {
			state->accept_disagree++;
			accept_disagree_record = true;
		}
		if (relative_difference > state->max_accept_relative_difference)
			state->max_accept_relative_difference = relative_difference;
		if (max_excluded > state->max_excluded_cl1_value)
			state->max_excluded_cl1_value = max_excluded;
	}

	/* 4. Write the log record: every cl1_problem_log_every-th record (0-based index),
	   plus every accept-disagree and every accept with cl1 kode != 0. */
	const unsigned long record_index = state->cl1_problem_record_count;
	const bool write_record =
		(record_index % state->options.cl1_problem_log_every == 0)
		|| accept_disagree_record || accept_cl1_failed_record;
	if (write_record && state->cl1_problem_log != nullptr) {
		char reciprocal_condition_text[64], backward_error_text[64];
		char relative_difference_text[64], max_excluded_text[64], largest_violation_text[64];
		if (candidate.reciprocal_condition >= 0.0)
			std::snprintf(reciprocal_condition_text, sizeof(reciprocal_condition_text), "%.6e", candidate.reciprocal_condition);
		else
			std::snprintf(reciprocal_condition_text, sizeof(reciprocal_condition_text), "nan");
		if (candidate.backward_error >= 0.0)
			std::snprintf(backward_error_text, sizeof(backward_error_text), "%.6e", candidate.backward_error);
		else
			std::snprintf(backward_error_text, sizeof(backward_error_text), "nan");
		if (have_comparison) {
			std::snprintf(relative_difference_text, sizeof(relative_difference_text), "%.6e", relative_difference);
			std::snprintf(max_excluded_text, sizeof(max_excluded_text), "%.6e", max_excluded);
		} else {
			std::snprintf(relative_difference_text, sizeof(relative_difference_text), "nan");
			std::snprintf(max_excluded_text, sizeof(max_excluded_text), "nan");
		}
		if (candidate.outcome == CL1_PROBLEM_INEQUALITY_VIOLATED || candidate.outcome == CL1_PROBLEM_SIGN_VIOLATED)
			std::snprintf(largest_violation_text, sizeof(largest_violation_text), "%.6e", candidate.largest_violation);
		else
			std::snprintf(largest_violation_text, sizeof(largest_violation_text), "nan");
		/* Phase 1B Task 1b: 16th column, reciprocal condition after row AND column
		   equilibration; nan if not computed (-1). */
		char equilibrated_condition_text[64];
		if (candidate.equilibrated_reciprocal_condition >= 0.0)
			std::snprintf(equilibrated_condition_text, sizeof(equilibrated_condition_text),
			              "%.6e", candidate.equilibrated_reciprocal_condition);
		else
			std::snprintf(equilibrated_condition_text, sizeof(equilibrated_condition_text), "nan");

		/* Phase 1B Task 1c: columns 17-22, the cl1-exactness diagnostics. Computed only
		   when the candidate is solved, the outcome is ACCEPT, and cl1_kode == 0;
		   otherwise the five reals are nan and the count is -1. */
		bool have_diagnostics = false;
		double cl1_residual_a = 0.0, cl1_residual_c = 0.0, cl1_violation_e = 0.0;
		double lu_residual_ac = 0.0, min_abs_entry = -1.0;
		int entries_below_count = -1;
		if (candidate.solved && candidate.outcome == CL1_PROBLEM_ACCEPT && cl1_kode == 0
		    && state->cl1_problem_copy_valid) {
			const int kc = state->cl1_problem_copy_k, lc = state->cl1_problem_copy_l;
			const int mc = state->cl1_problem_copy_m, nc = state->cl1_problem_copy_n;
			const int stride = nc + 1;
			const LDBLE *qc = state->cl1_problem_q_copy.data();
			const LDBLE *lu_x = candidate.x.data();
			for (int r = 0; r < kc; ++r) {
				const double v = cl1_problem_row_relative_residual(qc + (size_t)r * (size_t)stride, cl1_x, nc);
				if (v > cl1_residual_a)
					cl1_residual_a = v;
			}
			for (int r = kc; r < kc + lc; ++r) {
				const double v = cl1_problem_row_relative_residual(qc + (size_t)r * (size_t)stride, cl1_x, nc);
				if (v > cl1_residual_c)
					cl1_residual_c = v;
			}
			for (int r = kc + lc; r < kc + lc + mc; ++r) {
				const double v = cl1_problem_row_violation_e(qc + (size_t)r * (size_t)stride, cl1_x, nc);
				if (v > cl1_violation_e)
					cl1_violation_e = v;
			}
			for (int r = 0; r < kc + lc; ++r) {
				const double v = cl1_problem_row_relative_residual(qc + (size_t)r * (size_t)stride, lu_x, nc);
				if (v > lu_residual_ac)
					lu_residual_ac = v;
			}
			double smallest_nonzero = -1.0;
			int entries_below = 0;
			for (int r = 0; r < kc + lc; ++r) {
				const LDBLE *row = qc + (size_t)r * (size_t)stride;
				for (int j = 0; j < nc; ++j) {
					if (!candidate.live[(size_t)j])
						continue;
					const double a = std::fabs((double)row[j]);
					if (a == 0.0)
						continue;
					if (smallest_nonzero < 0.0 || a < smallest_nonzero)
						smallest_nonzero = a;
					if (a <= 1.0e-15)
						++entries_below;
				}
			}
			min_abs_entry = smallest_nonzero;
			entries_below_count = entries_below;
			have_diagnostics = true;
		}
		char cl1_residual_a_text[64], cl1_residual_c_text[64], cl1_violation_e_text[64];
		char lu_residual_ac_text[64], min_abs_entry_text[64];
		if (have_diagnostics) {
			std::snprintf(cl1_residual_a_text, sizeof(cl1_residual_a_text), "%.6e", cl1_residual_a);
			std::snprintf(cl1_residual_c_text, sizeof(cl1_residual_c_text), "%.6e", cl1_residual_c);
			std::snprintf(cl1_violation_e_text, sizeof(cl1_violation_e_text), "%.6e", cl1_violation_e);
			std::snprintf(lu_residual_ac_text, sizeof(lu_residual_ac_text), "%.6e", lu_residual_ac);
			if (min_abs_entry >= 0.0)
				std::snprintf(min_abs_entry_text, sizeof(min_abs_entry_text), "%.6e", min_abs_entry);
			else
				std::snprintf(min_abs_entry_text, sizeof(min_abs_entry_text), "nan");
		} else {
			std::snprintf(cl1_residual_a_text, sizeof(cl1_residual_a_text), "nan");
			std::snprintf(cl1_residual_c_text, sizeof(cl1_residual_c_text), "nan");
			std::snprintf(cl1_violation_e_text, sizeof(cl1_violation_e_text), "nan");
			std::snprintf(lu_residual_ac_text, sizeof(lu_residual_ac_text), "nan");
			std::snprintf(min_abs_entry_text, sizeof(min_abs_entry_text), "nan");
		}

		int violated_row_unknown_type = -1, largest_difference_unknown_type = -1;
		if (!state->cl1_problem_unknown_types.empty()) {
			if (candidate.outcome == CL1_PROBLEM_INEQUALITY_VIOLATED && candidate.violated_row >= 0
			    && state->cl1_problem_back_eq != nullptr)
				violated_row_unknown_type = state->cl1_problem_unknown_types[state->cl1_problem_back_eq[candidate.violated_row]];
			else if (candidate.outcome == CL1_PROBLEM_SIGN_VIOLATED && candidate.violated_column >= 0)
				violated_row_unknown_type = state->cl1_problem_unknown_types[candidate.violated_column];
			if (candidate.solved && largest_difference_column >= 0)
				largest_difference_unknown_type = state->cl1_problem_unknown_types[largest_difference_column];
		}

		std::fprintf(state->cl1_problem_log,
		             "%lu %d %d %d %d %d %d %d %s %s %s %s %s %d %d %s %s %s %s %s %s %d\n",
		             record_index + 1, state->cl1_problem_view.n,
		             state->cl1_problem_view.k, state->cl1_problem_view.l, state->cl1_problem_view.m,
		             candidate.live_column_count, candidate.outcome, cl1_kode,
		             reciprocal_condition_text, backward_error_text,
		             relative_difference_text, max_excluded_text, largest_violation_text,
		             violated_row_unknown_type, largest_difference_unknown_type,
		             equilibrated_condition_text,
		             cl1_residual_a_text, cl1_residual_c_text, cl1_violation_e_text,
		             lu_residual_ac_text, min_abs_entry_text, entries_below_count);
		std::fflush(state->cl1_problem_log);
	}
	state->cl1_problem_record_count++;

	/* 5. The record is complete. */
	state->cl1_problem_pending = false;
#else
	(void)state; (void)cl1_kode; (void)cl1_x;
#endif
}

/* Phase 1B Task 2: cl1-problem steering mode (B-D10). When the check accepts, its LU
   solution replaces the cl1() call. */
bool cl1_problem_steering_active(const NewtonLinearSolverState *state)
{
#ifdef USE_LAPACK_NEWTON_SOLVER
	return (state != nullptr
	        && state->options.intercept_mode == NEWTON_INTERCEPT_CL1_PROBLEM
	        && state->cl1_problem_solver != nullptr);
#else
	(void)state;
	return false;
#endif
}

int cl1_problem_steer(NewtonLinearSolverState *state, int in_kode, int pitzer_or_sit,
	int mass_water_switch_on, int numerical_derivatives_on, int k, int l, int m, int n, int row_stride,
	const LDBLE *q, LDBLE *delta1, LDBLE *res, int *l_kode, int *l_iter, LDBLE *l_error)
{
#ifdef USE_LAPACK_NEWTON_SOLVER
	if (!cl1_problem_steering_active(state))
		return 0;
	state->cl1_problem_steer_call_count++;

	/* Same exclusions as cl1_problem_shadow_prepare, in the same order. */
	Cl1ProblemCandidate candidate;
	if (in_kode != 1)
		candidate.outcome = CL1_PROBLEM_EXCLUDED_RETRY;
	else if (pitzer_or_sit)
		candidate.outcome = CL1_PROBLEM_EXCLUDED_PITZER_SIT;
	else if (mass_water_switch_on)
		candidate.outcome = CL1_PROBLEM_EXCLUDED_MASS_WATER;
	else if (numerical_derivatives_on)
		candidate.outcome = CL1_PROBLEM_EXCLUDED_NUMERICAL_DERIVATIVES;
	else {
		/* Phase 1B Task 3 (B-D11): the time of the steering evaluation itself, from the
		   end of the exclusion checks through writing the solution on accept. */
		const std::chrono::steady_clock::time_point evaluation_start =
		    std::chrono::steady_clock::now();
		Cl1ProblemView view;
		view.k = k;
		view.l = l;
		view.m = m;
		view.n = n;
		view.row_stride = row_stride;
		view.q = q;
		view.variable_signs = delta1;   /* delta1 on entry holds the sign flags; read-only here */
		evaluate_cl1_problem(state->cl1_problem_solver, view, candidate,
		                     state->cl1_problem_augmented, state->cl1_problem_live_solution,
		                     state->options.cl1_problem_max_componentwise_error);
		if (candidate.outcome == CL1_PROBLEM_ACCEPT) {
			/* The view is still intact: cl1() has not run. */
			*l_error = (LDBLE)write_cl1_problem_solution(view, candidate, delta1, res);
			*l_kode = 0;
			*l_iter = 0;
		}
		state->cl1_problem_steer_seconds +=
		    std::chrono::duration_cast<std::chrono::duration<double>>(
		        std::chrono::steady_clock::now() - evaluation_start).count();
	}
	if (candidate.outcome >= 0 && candidate.outcome < (int)CL1_PROBLEM_OUTCOME_COUNT)
		state->cl1_problem_outcome_count[candidate.outcome]++;
	return (candidate.outcome == CL1_PROBLEM_ACCEPT) ? 1 : 0;
#else
	(void)state; (void)in_kode; (void)pitzer_or_sit; (void)mass_water_switch_on;
	(void)numerical_derivatives_on; (void)k; (void)l; (void)m; (void)n; (void)row_stride;
	(void)q; (void)delta1; (void)res; (void)l_kode; (void)l_iter; (void)l_error;
	return 0;
#endif
}

#endif
