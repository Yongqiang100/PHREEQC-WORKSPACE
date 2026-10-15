/* newton_solver_phreeqc_glue.cpp -- the phreeqc_newton_solver glue (OV-D2).
   The dispatcher Phreeqc::ineq, the eligibility test, the accessor
   augmented_newton_matrix, and the hooks around the cl1() call. The bodies of
   ineq, newton_solver_ineligibility_reason and augmented_newton_matrix are
   those of ~/phreeqc3/src/model.cpp and src/Phreeqc.h at petsc-phase1b-complete
   (audit A2.1, A2.2), apart from the removed #ifdef USE_NEWTON_LINEAR_SOLVER
   wrappers: this file is compiled only when the overlay is attached. */
#include "Phreeqc.h"
#include "newton_linear_solver_state.h"
#include "newton_linear_solver_options.h"
#include "cl1_problem_check.h"

/* ------------------------------------------------------------------------------- */
int Phreeqc::
newton_solver_ineligibility_reason(int in_kode) const
/* ------------------------------------------------------------------------------- */
{
	if (in_kode != 1)
		return NEWTON_SOLVER_NOT_FIRST_ATTEMPT;
	if (remove_unstable_phases == TRUE)
		return NEWTON_SOLVER_REMOVING_UNSTABLE_PHASES;
	if (pitzer_model == TRUE || sit_model == TRUE)
		return NEWTON_SOLVER_PITZER_OR_SIT_MODEL;
	if (mass_water_switch == TRUE)
		return NEWTON_SOLVER_MASS_WATER_SWITCH;
	if (iterations < aqueous_only)
		return NEWTON_SOLVER_AQUEOUS_ONLY_ITERATION;
	if (numerical_deriv)   /* D17: numerical-derivative mode excluded entirely */
		return NEWTON_SOLVER_NUMERICAL_DERIVATIVES;
	if (count_unknowns <= 0)
		return NEWTON_SOLVER_EMPTY_SYSTEM;
	for (int i = 0; i < count_unknowns; i++)
	{
		switch (x[i]->type)
		{
		case MB: case ALK: case CB: case SOLUTION_PHASE_BOUNDARY:
		case MU: case AH2O: case MH: case MH2O:
			break;
		default:
			return NEWTON_SOLVER_INELIGIBLE_UNKNOWN_TYPE;
		}
	}
	return NEWTON_SOLVER_ELIGIBLE;
}
/* ------------------------------------------------------------------------------- */
int Phreeqc::ineq(int in_kode)
/* ------------------------------------------------------------------------------- */
{
	if (newton_linear_solver_state == nullptr) newton_linear_solver_state = create_newton_linear_solver_state();
	NewtonLinearSolverState *solver_state = newton_linear_solver_state;
	if (solver_state != nullptr && solver_state->backend != NEWTON_BACKEND_CL1
		&& solver_state->options.intercept_mode == NEWTON_INTERCEPT_ENTRY) {
		const int ineligibility_reason = newton_solver_ineligibility_reason(in_kode);
		if (ineligibility_reason != NEWTON_SOLVER_ELIGIBLE) {
			count_ineligible_newton_system(solver_state, ineligibility_reason);
		} else {
			solver_state->candidate_newton_step.resize((size_t)count_unknowns);
			/* The backend reads the augmented Newton matrix BEFORE ineq_cl1_original runs
			   (cl1 may modify its inputs) and writes only to candidate_newton_step. */
			LinearSolveStatus solve_status = solve_newton_step_with_backend(
				solver_state, augmented_newton_matrix(), count_unknowns,
				solver_state->candidate_newton_step.data());
			if (solver_state->options.compare_with_cl1) {   /* cl1 steers; backend result is only logged */
				int cl1_return_code = ineq_cl1_original(in_kode);
				double relative_difference = 0.0, relative_sum = 0.0;
				int largest_difference_index = -1;
				if (solve_status == LINEAR_SOLVE_ACCEPTED)
					largest_difference_index = compare_newton_steps(
						solver_state->candidate_newton_step.data(), delta.data(), count_unknowns,
						&relative_difference, &relative_sum);
				const int largest_difference_unknown_type =
					largest_difference_index >= 0 ? (int)x[largest_difference_index]->type : -1;
				write_step_comparison_record(
					solver_state, count_unknowns, solve_status, cl1_return_code,
					(double)solver_state->last_backward_error,
					relative_difference, relative_sum,
					largest_difference_index, largest_difference_unknown_type);
				return cl1_return_code;
			}
			if (solve_status == LINEAR_SOLVE_ACCEPTED) {
				for (int i = 0; i < count_unknowns; ++i) delta[i] = solver_state->candidate_newton_step[i];
				return OK;   /* success value from Task 0 item B3 */
			}
			/* Any other status, including LINEAR_SOLVE_NON_FINITE_INPUT, falls through to cl1. */
		}
	}
	return ineq_cl1_original(in_kode);
}
/* ------------------------------------------------------------------------------- */
const LDBLE *Phreeqc::
augmented_newton_matrix(void) const
/* ------------------------------------------------------------------------------- */
{
	return my_array.data();
}
/* ------------------------------------------------------------------------------- */
int Phreeqc::
newton_solver_before_cl1(int in_kode, int k, int l, int m, int n, LDBLE *ineq_array_data,
                        LDBLE *delta1_data, LDBLE *res_data, int *l_kode, int *l_iter, LDBLE *l_error)
/* ------------------------------------------------------------------------------- */
{
	if (cl1_problem_shadow_active(newton_linear_solver_state))
	{
		/* The unknown-types vector must live until newton_solver_after_cl1: it is the
		   state field, not a local (OV-D2). */
		std::vector<int> &unknown_types = newton_linear_solver_state->cl1_problem_unknown_types;
		unknown_types.resize(count_unknowns);
		for (size_t newton_index = 0; newton_index < count_unknowns; newton_index++)
			unknown_types[newton_index] = x[newton_index]->type;
		cl1_problem_shadow_prepare(newton_linear_solver_state, in_kode,
			(pitzer_model || sit_model) ? 1 : 0, mass_water_switch ? 1 : 0, numerical_deriv ? 1 : 0,
			k, l, m, n,
			((sit_model || pitzer_model) && full_pitzer == FALSE) ? n + 2 : (int) max_column_count,
			ineq_array_data, delta1_data, unknown_types, back_eq.data());
		return 0;
	}
	if (cl1_problem_steering_active(newton_linear_solver_state))
		return cl1_problem_steer(newton_linear_solver_state, in_kode,
			(pitzer_model || sit_model) ? 1 : 0, mass_water_switch ? 1 : 0, numerical_deriv ? 1 : 0,
			k, l, m, n,
			((sit_model || pitzer_model) && full_pitzer == FALSE) ? n + 2 : (int) max_column_count,
			ineq_array_data, delta1_data, res_data, l_kode, l_iter, l_error);
	return 0;
}
/* ------------------------------------------------------------------------------- */
void Phreeqc::
newton_solver_after_cl1(int l_kode, LDBLE *delta1_data)
/* ------------------------------------------------------------------------------- */
{
	if (cl1_problem_shadow_active(newton_linear_solver_state))
		cl1_problem_shadow_record(newton_linear_solver_state, l_kode, delta1_data);
}
/* ------------------------------------------------------------------------------- */
void Phreeqc::
newton_solver_destroy_state(void)
/* ------------------------------------------------------------------------------- */
{
	destroy_newton_linear_solver_state(&newton_linear_solver_state);
}
