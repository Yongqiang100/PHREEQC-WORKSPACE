#pragma once
#include <string>

enum NewtonLinearSolverBackend {
	NEWTON_BACKEND_CL1 = 0,     /* no replacement: every call goes to ineq_cl1_original */
	NEWTON_BACKEND_PETSC = 1,
	NEWTON_BACKEND_LAPACK = 2   /* Task 1b; never compiled in this task */
};

enum NewtonConditionCheckMode {
	NEWTON_CONDITION_CHECK_OFF = 0,
	NEWTON_CONDITION_CHECK_LOG = 1,    /* compute and record; never changes the decision */
	NEWTON_CONDITION_CHECK_GATE = 2    /* reciprocal condition below the minimum -> cl1 */
};

/* Phase 1B Task 1: what the adapter intercepts. ENTRY is the Phase 2 behavior (the backend
   replaces the solve at the ineq() entry). CL1_PROBLEM_SHADOW is shadow mode (B-D7): the
   entry dispatcher is off, cl1() always steers, and the cl1 problem is classified and
   solved alongside. CL1_PROBLEM is steering mode (Phase 1B Task 2, B-D10): the entry
   dispatcher is off, and when the cl1-problem check accepts, its LU solution replaces the
   cl1() call. Both are valid only in LAPACK builds. */
enum NewtonInterceptMode { NEWTON_INTERCEPT_ENTRY = 0, NEWTON_INTERCEPT_CL1_PROBLEM_SHADOW = 1,
	NEWTON_INTERCEPT_CL1_PROBLEM = 2 };

struct NewtonLinearSolverOptions {
	NewtonLinearSolverBackend backend;
	bool print_statistics;
	bool print_summary;
	bool compare_with_cl1;
	std::string comparison_log_path;
	std::string backend_name_as_given;   /* value of PHREEQC_LINEAR_SOLVER, or empty */
	bool backend_name_valid;             /* false if the name was unknown or not built in */
	NewtonConditionCheckMode condition_check_mode;   /* Task 2d; LAPACK backend only */
	double min_reciprocal_condition;                 /* used only in GATE mode */
	bool condition_options_valid;                    /* false if either condition option was invalid */
	int condition_metric_row_column;                 /* Phase 1B Task 1b; 0: row metric (default), 1: row and column */
	NewtonInterceptMode intercept_mode;              /* Phase 1B Task 1; default ENTRY */
	bool intercept_mode_valid;                       /* false if an intercept option was invalid */
	std::string cl1_problem_log_path;                /* shadow-log path pattern (Phase 1B Task 1) */
	unsigned long cl1_problem_log_every;             /* shadow-log sampling period, >= 1 */
	double cl1_problem_max_componentwise_error;      /* Phase 1B Task 2; default 1e-8 */
};

/* Reads the environment on every call (no caching):
      PHREEQC_LINEAR_SOLVER                    cl1 | petsc | lapack; unset or empty: build default
      PHREEQC_LINEAR_SOLVER_STATISTICS         matched exactly: "1" -> per-object statistics only;
                                               "summary" -> process-summary line only; "all" -> both;
                                               anything else, unset or empty -> neither
      PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1   exactly "1" enables comparison mode
     PHREEQC_LINEAR_SOLVER_COMPARE_LOG        log path; unset or empty: newton_step_comparison.log
     PHREEQC_LINEAR_SOLVER_CONDITION_CHECK    off | log | gate, matched exactly; unset or empty: gate;
                                              any other value: gate with condition_options_valid = false
       PHREEQC_LINEAR_SOLVER_MIN_RCOND          positive finite number (std::strtod, whole string must
                                                be consumed); unset or empty: 1.0e-8; invalid,
                                                non-positive or non-finite: 1.0e-8 with
                                                condition_options_valid = false
       PHREEQC_LINEAR_SOLVER_CONDITION_METRIC   row | row_column, matched exactly (Phase 1B Task 1b);
                                                "row", unset or empty: condition_metric_row_column = 0;
                                                "row_column": 1; anything else: 0 with
                                                condition_options_valid = false
       PHREEQC_LINEAR_SOLVER_INTERCEPT          entry | cl1_problem_shadow | cl1_problem, matched
                                                exactly; unset or empty: entry; cl1_problem_shadow
                                                and cl1_problem are valid only when
                                                USE_LAPACK_NEWTON_SOLVER is defined; anything else, or
                                                shadow/steering without LAPACK: entry with
                                                intercept_mode_valid = false
       PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG    shadow-log path; %r, %o, %p placeholders as in
                                                Task 3; unset or empty: cl1_problem_shadow.log
       PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG_EVERY  positive integer (std::strtoul, whole string must
                                                be consumed); unset: 1; invalid: 1 with
                                                intercept_mode_valid = false
       PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_MAX_COMPONENTWISE_ERROR  positive finite number
                                                (std::strtod, whole string must be consumed)
                                                (Phase 1B Task 2); unset or empty: 1.0e-8; invalid,
                                                non-positive or non-finite: 1.0e-8 with
                                                intercept_mode_valid = false
    An unknown name, or a backend not compiled into this build, gives backend = NEWTON_BACKEND_CL1
    and backend_name_valid = false. */
NewtonLinearSolverOptions read_newton_linear_solver_options(void);

/* "cl1", "petsc", "lapack"; "unknown" for any other value. */
const char *newton_linear_solver_backend_name(NewtonLinearSolverBackend backend);

/* Rank label from the first of OMPI_COMM_WORLD_RANK, PMIX_RANK, PMI_RANK that is set and
   non-empty; "0" if none is set. Reads the environment only; never calls MPI. */
std::string current_process_rank_text(void);

/* Expands placeholders in a comparison-log path pattern:
     %r -> rank_text      %p -> process_id      %o -> object_index      %% -> %
   Any other character after %, and a trailing %, are copied literally. */
std::string expand_comparison_log_path(const std::string &pattern, const std::string &rank_text,
                                       unsigned long object_index, long process_id);
