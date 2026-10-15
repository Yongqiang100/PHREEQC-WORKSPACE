#include "newton_linear_solver_options.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

static NewtonLinearSolverBackend default_backend_of_this_build(void)
{
#if defined(NEWTON_SOLVER_DEFAULT_BACKEND_CL1)
	/* Overlay release policy (OV-D7 (b)): with no environment variables an
	   overlay build must reproduce upstream PHREEQC exactly. */
	return NEWTON_BACKEND_CL1;
#elif defined(USE_LAPACK_NEWTON_SOLVER)
	return NEWTON_BACKEND_LAPACK;
#elif defined(USE_PETSC)
	return NEWTON_BACKEND_PETSC;
#else
	return NEWTON_BACKEND_CL1;
#endif
}

static bool backend_is_compiled(NewtonLinearSolverBackend backend)
{
	switch (backend)
	{
	case NEWTON_BACKEND_CL1:
		return true;
	case NEWTON_BACKEND_PETSC:
#if defined(USE_PETSC)
		return true;
#else
		return false;
#endif
	case NEWTON_BACKEND_LAPACK:
#if defined(USE_LAPACK_NEWTON_SOLVER)
		return true;
#else
		return false;
#endif
	}
	return false;
}

/* True only when the variable is set and its value is exactly "1". */
static bool environment_flag_is_one(const char *name)
{
	const char *value = std::getenv(name);
	return (value != nullptr && std::strcmp(value, "1") == 0);
}

NewtonLinearSolverOptions read_newton_linear_solver_options(void)
{
	NewtonLinearSolverOptions options;
	options.backend = default_backend_of_this_build();
	options.print_statistics = false;
	options.print_summary = false;
	options.compare_with_cl1 = false;
	options.comparison_log_path = "newton_step_comparison.log";
	options.backend_name_as_given = "";
	options.backend_name_valid = true;
	options.condition_check_mode = NEWTON_CONDITION_CHECK_GATE;
	options.min_reciprocal_condition = 1.0e-8;
	options.condition_options_valid = true;
	options.condition_metric_row_column = 0;
	options.intercept_mode = NEWTON_INTERCEPT_ENTRY;
	options.intercept_mode_valid = true;
	options.cl1_problem_log_path = "cl1_problem_shadow.log";
	options.cl1_problem_log_every = 1;
	options.cl1_problem_max_componentwise_error = 1.0e-8;

	/* Task 3 (P2-D16): PHREEQC_LINEAR_SOLVER_STATISTICS is matched exactly.
	   "1" -> per-object statistics only; "summary" -> process summary only;
	   "all" -> both; empty or any other value -> neither. */
	const char *statistics_name = std::getenv("PHREEQC_LINEAR_SOLVER_STATISTICS");
	if (statistics_name != nullptr)
	{
		if (std::strcmp(statistics_name, "1") == 0)
		{
			options.print_statistics = true;
			options.print_summary = false;
		}
		else if (std::strcmp(statistics_name, "summary") == 0)
		{
			options.print_statistics = false;
			options.print_summary = true;
		}
		else if (std::strcmp(statistics_name, "all") == 0)
		{
			options.print_statistics = true;
			options.print_summary = true;
		}
	}
	options.compare_with_cl1 = environment_flag_is_one("PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1");

	const char *log_path = std::getenv("PHREEQC_LINEAR_SOLVER_COMPARE_LOG");
	if (log_path != nullptr && log_path[0] != '\0')
		options.comparison_log_path = log_path;

	/* Task 2d: condition-check options (LAPACK backend only). Task 2e (P2-D19):
	   the default is gate mode with minimum reciprocal condition 1.0e-8. */
	const char *condition_check_name = std::getenv("PHREEQC_LINEAR_SOLVER_CONDITION_CHECK");
	if (condition_check_name != nullptr && condition_check_name[0] != '\0')
	{
		if (std::strcmp(condition_check_name, "off") == 0)
			options.condition_check_mode = NEWTON_CONDITION_CHECK_OFF;
		else if (std::strcmp(condition_check_name, "log") == 0)
			options.condition_check_mode = NEWTON_CONDITION_CHECK_LOG;
		else if (std::strcmp(condition_check_name, "gate") == 0)
			options.condition_check_mode = NEWTON_CONDITION_CHECK_GATE;
		else
		{
			options.condition_check_mode = NEWTON_CONDITION_CHECK_GATE;
			options.condition_options_valid = false;
		}
	}

	const char *min_rcond_name = std::getenv("PHREEQC_LINEAR_SOLVER_MIN_RCOND");
	if (min_rcond_name != nullptr && min_rcond_name[0] != '\0')
	{
		char *parse_end = nullptr;
		double min_rcond = std::strtod(min_rcond_name, &parse_end);
		if (parse_end == min_rcond_name || *parse_end != '\0'
		    || !(min_rcond > 0.0) || !std::isfinite(min_rcond))
		{
			options.min_reciprocal_condition = 1.0e-8;
			options.condition_options_valid = false;
		}
		else
			options.min_reciprocal_condition = min_rcond;
	}

	/* Phase 1B Task 1b: which metric the condition gate uses. Matched exactly;
	   "row", unset or empty gives 0; "row_column" gives 1; anything else gives 0
	   with condition_options_valid = false. */
	const char *condition_metric_name = std::getenv("PHREEQC_LINEAR_SOLVER_CONDITION_METRIC");
	if (condition_metric_name != nullptr && condition_metric_name[0] != '\0')
	{
		if (std::strcmp(condition_metric_name, "row") == 0)
			options.condition_metric_row_column = 0;
		else if (std::strcmp(condition_metric_name, "row_column") == 0)
			options.condition_metric_row_column = 1;
		else
		{
			options.condition_metric_row_column = 0;
			options.condition_options_valid = false;
		}
	}

	const char *backend_name = std::getenv("PHREEQC_LINEAR_SOLVER");
	if (backend_name != nullptr && backend_name[0] != '\0')
	{
		options.backend_name_as_given = backend_name;
		NewtonLinearSolverBackend requested = NEWTON_BACKEND_CL1;
		bool matched = true;
		if (std::strcmp(backend_name, "cl1") == 0)
			requested = NEWTON_BACKEND_CL1;
		else if (std::strcmp(backend_name, "petsc") == 0)
			requested = NEWTON_BACKEND_PETSC;
		else if (std::strcmp(backend_name, "lapack") == 0)
			requested = NEWTON_BACKEND_LAPACK;
		else
			matched = false;
		if (matched && backend_is_compiled(requested))
			options.backend = requested;
		else
		{
			options.backend = NEWTON_BACKEND_CL1;
			options.backend_name_valid = false;
		}
	}

	/* Phase 1B Task 1: interception mode. cl1_problem_shadow and cl1_problem (Task 2) are
	   valid only in LAPACK builds (B-D7, B-D10); anything else gives ENTRY with
	   intercept_mode_valid = false. */
	const char *intercept_name = std::getenv("PHREEQC_LINEAR_SOLVER_INTERCEPT");
	if (intercept_name != nullptr && intercept_name[0] != '\0')
	{
		if (std::strcmp(intercept_name, "entry") == 0)
			options.intercept_mode = NEWTON_INTERCEPT_ENTRY;
		else if (std::strcmp(intercept_name, "cl1_problem_shadow") == 0)
		{
#if defined(USE_LAPACK_NEWTON_SOLVER)
			options.intercept_mode = NEWTON_INTERCEPT_CL1_PROBLEM_SHADOW;
#else
			options.intercept_mode = NEWTON_INTERCEPT_ENTRY;
			options.intercept_mode_valid = false;
#endif
		}
		else if (std::strcmp(intercept_name, "cl1_problem") == 0)
		{
#if defined(USE_LAPACK_NEWTON_SOLVER)
			options.intercept_mode = NEWTON_INTERCEPT_CL1_PROBLEM;
#else
			options.intercept_mode = NEWTON_INTERCEPT_ENTRY;
			options.intercept_mode_valid = false;
#endif
		}
		else
		{
			options.intercept_mode = NEWTON_INTERCEPT_ENTRY;
			options.intercept_mode_valid = false;
		}
	}

	/* Phase 1B Task 1: shadow-log path and sampling period. */
	const char *cl1_problem_log_path = std::getenv("PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG");
	if (cl1_problem_log_path != nullptr && cl1_problem_log_path[0] != '\0')
		options.cl1_problem_log_path = cl1_problem_log_path;

	const char *log_every_name = std::getenv("PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG_EVERY");
	if (log_every_name != nullptr && log_every_name[0] != '\0')
	{
		char *parse_end = nullptr;
		unsigned long log_every = std::strtoul(log_every_name, &parse_end, 10);
		if (parse_end == log_every_name || *parse_end != '\0' || log_every == 0
		    || log_every_name[0] == '-')
		{
			options.cl1_problem_log_every = 1;
			options.intercept_mode_valid = false;
		}
		else
			options.cl1_problem_log_every = log_every;
	}

	/* Phase 1B Task 2: maximum componentwise backward error for the cl1-problem check.
	   Positive finite, std::strtod, whole string consumed; invalid gives 1.0e-8 with
	   intercept_mode_valid = false. */
	const char *max_componentwise_name = std::getenv("PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_MAX_COMPONENTWISE_ERROR");
	if (max_componentwise_name != nullptr && max_componentwise_name[0] != '\0')
	{
		char *parse_end = nullptr;
		double max_componentwise = std::strtod(max_componentwise_name, &parse_end);
		if (parse_end == max_componentwise_name || *parse_end != '\0'
		    || !(max_componentwise > 0.0) || !std::isfinite(max_componentwise))
		{
			options.cl1_problem_max_componentwise_error = 1.0e-8;
			options.intercept_mode_valid = false;
		}
		else
			options.cl1_problem_max_componentwise_error = max_componentwise;
	}

	return options;
}

const char *newton_linear_solver_backend_name(NewtonLinearSolverBackend backend)
{
	switch (backend)
	{
	case NEWTON_BACKEND_CL1:
		return "cl1";
	case NEWTON_BACKEND_PETSC:
		return "petsc";
	case NEWTON_BACKEND_LAPACK:
		return "lapack";
	}
	return "unknown";
}

std::string current_process_rank_text(void)
{
	static const char *rank_variables[] = {
		"OMPI_COMM_WORLD_RANK", "PMIX_RANK", "PMI_RANK"
	};
	for (size_t k = 0; k < sizeof(rank_variables) / sizeof(rank_variables[0]); ++k)
	{
		const char *value = std::getenv(rank_variables[k]);
		if (value != nullptr && value[0] != '\0')
			return std::string(value);
	}
	return std::string("0");
}

std::string expand_comparison_log_path(const std::string &pattern, const std::string &rank_text,
                                       unsigned long object_index, long process_id)
{
	std::string result;
	for (size_t k = 0; k < pattern.size(); ++k)
	{
		if (pattern[k] != '%')
		{
			result += pattern[k];
			continue;
		}
		if (k + 1 >= pattern.size())
		{
			/* A trailing '%' is copied literally. */
			result += '%';
			break;
		}
		char directive = pattern[k + 1];
		switch (directive)
		{
		case 'r':
			result += rank_text;
			++k;
			break;
		case 'o':
			result += std::to_string(object_index);
			++k;
			break;
		case 'p':
			result += std::to_string(process_id);
			++k;
			break;
		case '%':
			result += '%';
			++k;
			break;
		default:
			/* Unknown directive: copy the '%' literally and let the next
			   iteration copy the directive character itself. */
			result += '%';
			break;
		}
	}
	return result;
}
