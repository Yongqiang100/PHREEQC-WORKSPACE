/* Phase 2 Task 1a test: environment-variable reading in
   newton_linear_solver_options.cpp. Built whenever the adapter library is built
   (PHREEQC_USE_PETSC or PHREEQC_USE_LAPACK_SOLVER), so the expected default
   backend and the validity of "lapack" and "petsc" depend on the build
   (Phase 2 Task 1b table): with NEWTON_SOLVER_DEFAULT_BACKEND_CL1 defined
   (overlay release default, OV-D7 (b)) the default is CL1; otherwise with
   USE_LAPACK_NEWTON_SOLVER defined the default is LAPACK and "lapack" is
   valid; "petsc" is valid only with USE_PETSC defined. */
#include "newton_linear_solver_options.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK_OR_FAIL(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "FAILED line %d: %s\n", __LINE__, #condition); \
            return 1; \
        } \
    } while (0)

static void unset_all_newton_solver_environment_variables(void)
{
    unsetenv("PHREEQC_LINEAR_SOLVER");
    unsetenv("PHREEQC_LINEAR_SOLVER_STATISTICS");
    unsetenv("PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1");
    unsetenv("PHREEQC_LINEAR_SOLVER_COMPARE_LOG");
    unsetenv("PHREEQC_LINEAR_SOLVER_CONDITION_CHECK");
    unsetenv("PHREEQC_LINEAR_SOLVER_MIN_RCOND");
    unsetenv("PHREEQC_LINEAR_SOLVER_CONDITION_METRIC");
    unsetenv("PHREEQC_LINEAR_SOLVER_INTERCEPT");
    unsetenv("PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG");
    unsetenv("PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG_EVERY");
    unsetenv("PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_MAX_COMPONENTWISE_ERROR");
    unsetenv("OMPI_COMM_WORLD_RANK");
    unsetenv("PMIX_RANK");
    unsetenv("PMI_RANK");
}

int main(void)
{
    /* Case 1: nothing set: build default (cl1 with NEWTON_SOLVER_DEFAULT_BACKEND_CL1,
       lapack if built with the LAPACK backend, otherwise petsc), valid, flags off,
       default log. */
    unset_all_newton_solver_environment_variables();
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
#ifdef NEWTON_SOLVER_DEFAULT_BACKEND_CL1
        CHECK_OR_FAIL(options.backend == NEWTON_BACKEND_CL1);
#elif defined(USE_LAPACK_NEWTON_SOLVER)
        CHECK_OR_FAIL(options.backend == NEWTON_BACKEND_LAPACK);
#else
        CHECK_OR_FAIL(options.backend == NEWTON_BACKEND_PETSC);
#endif
        CHECK_OR_FAIL(options.backend_name_valid);
        CHECK_OR_FAIL(!options.print_statistics);
        CHECK_OR_FAIL(!options.print_summary);
        CHECK_OR_FAIL(!options.compare_with_cl1);
        CHECK_OR_FAIL(options.comparison_log_path == "newton_step_comparison.log");
        CHECK_OR_FAIL(options.backend_name_as_given.empty());
        /* Phase 1B Task 1: unset INTERCEPT gives ENTRY and valid; default shadow log. */
        CHECK_OR_FAIL(options.intercept_mode == NEWTON_INTERCEPT_ENTRY);
        CHECK_OR_FAIL(options.intercept_mode_valid);
        CHECK_OR_FAIL(options.cl1_problem_log_path == "cl1_problem_shadow.log");
        CHECK_OR_FAIL(options.cl1_problem_log_every == 1);
    }

    /* Case 2: PHREEQC_LINEAR_SOLVER=cl1 is honoured and valid in every build;
       =petsc is honoured and valid only when USE_PETSC is defined. */
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER", "cl1", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.backend == NEWTON_BACKEND_CL1);
        CHECK_OR_FAIL(options.backend_name_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER", "petsc", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
#ifdef USE_PETSC
        CHECK_OR_FAIL(options.backend == NEWTON_BACKEND_PETSC);
        CHECK_OR_FAIL(options.backend_name_valid);
#else
        CHECK_OR_FAIL(options.backend == NEWTON_BACKEND_CL1);
        CHECK_OR_FAIL(!options.backend_name_valid);
#endif
    }

    /* Case 3: lapack is valid when USE_LAPACK_NEWTON_SOLVER is defined, otherwise
       cl1 and not valid. bogus: cl1, not valid, name kept. */
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER", "lapack", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
#ifdef USE_LAPACK_NEWTON_SOLVER
        CHECK_OR_FAIL(options.backend == NEWTON_BACKEND_LAPACK);
        CHECK_OR_FAIL(options.backend_name_valid);
#else
        CHECK_OR_FAIL(options.backend == NEWTON_BACKEND_CL1);
        CHECK_OR_FAIL(!options.backend_name_valid);
#endif
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER", "bogus", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.backend == NEWTON_BACKEND_CL1);
        CHECK_OR_FAIL(!options.backend_name_valid);
        CHECK_OR_FAIL(options.backend_name_as_given == "bogus");
    }

    /* Case 4: statistics and comparison flags. PHREEQC_LINEAR_SOLVER_STATISTICS is
       matched exactly (Task 3): "1" -> per-object only; "summary" -> summary only;
       "all" -> both; "yes", "0", empty or unset -> neither. */
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_STATISTICS", "1", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.print_statistics);
        CHECK_OR_FAIL(!options.print_summary);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_STATISTICS", "summary", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(!options.print_statistics);
        CHECK_OR_FAIL(options.print_summary);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_STATISTICS", "all", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.print_statistics);
        CHECK_OR_FAIL(options.print_summary);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_STATISTICS", "yes", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(!options.print_statistics);
        CHECK_OR_FAIL(!options.print_summary);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_STATISTICS", "0", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(!options.print_statistics);
        CHECK_OR_FAIL(!options.print_summary);
    }
    unset_all_newton_solver_environment_variables();
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(!options.print_statistics);
        CHECK_OR_FAIL(!options.print_summary);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1", "1", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.compare_with_cl1);
    }

    /* Case 5: log path override; an empty value gives the default. */
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_COMPARE_LOG", "custom.log", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.comparison_log_path == "custom.log");
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_COMPARE_LOG", "", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.comparison_log_path == "newton_step_comparison.log");
    }

    /* Case 6: backend names. */
    CHECK_OR_FAIL(std::strcmp(newton_linear_solver_backend_name(NEWTON_BACKEND_CL1), "cl1") == 0);
    CHECK_OR_FAIL(std::strcmp(newton_linear_solver_backend_name(NEWTON_BACKEND_PETSC), "petsc") == 0);
    CHECK_OR_FAIL(std::strcmp(newton_linear_solver_backend_name(NEWTON_BACKEND_LAPACK), "lapack") == 0);

    /* Case 7 (Task 2d; defaults changed by Task 2e, P2-D19):
       PHREEQC_LINEAR_SOLVER_CONDITION_CHECK. Unset gives GATE and 1e-8 and valid;
       off, log and gate are matched exactly; anything else gives GATE and invalid. */
    unset_all_newton_solver_environment_variables();
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.condition_check_mode == NEWTON_CONDITION_CHECK_GATE);
        CHECK_OR_FAIL(options.min_reciprocal_condition == 1.0e-8);
        CHECK_OR_FAIL(options.condition_options_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_CONDITION_CHECK", "off", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.condition_check_mode == NEWTON_CONDITION_CHECK_OFF);
        CHECK_OR_FAIL(options.condition_options_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_CONDITION_CHECK", "log", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.condition_check_mode == NEWTON_CONDITION_CHECK_LOG);
        CHECK_OR_FAIL(options.condition_options_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_CONDITION_CHECK", "gate", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.condition_check_mode == NEWTON_CONDITION_CHECK_GATE);
        CHECK_OR_FAIL(options.condition_options_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_CONDITION_CHECK", "bogus", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.condition_check_mode == NEWTON_CONDITION_CHECK_GATE);
        CHECK_OR_FAIL(!options.condition_options_valid);
    }

    /* Case 8 (Task 2d; default changed by Task 2e, P2-D19):
       PHREEQC_LINEAR_SOLVER_MIN_RCOND. Unset gives 1e-8 (checked in Case 7); 1e-9 is
       honoured; abc, -1 and 0 each give 1e-8 and invalid. */
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_MIN_RCOND", "1e-9", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.min_reciprocal_condition == 1.0e-9);
        CHECK_OR_FAIL(options.condition_options_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_MIN_RCOND", "abc", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.min_reciprocal_condition == 1.0e-8);
        CHECK_OR_FAIL(!options.condition_options_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_MIN_RCOND", "-1", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.min_reciprocal_condition == 1.0e-8);
        CHECK_OR_FAIL(!options.condition_options_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_MIN_RCOND", "0", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.min_reciprocal_condition == 1.0e-8);
        CHECK_OR_FAIL(!options.condition_options_valid);
    }

    /* Case 9 (Task 3): expand_comparison_log_path placeholder expansion. */
    CHECK_OR_FAIL(expand_comparison_log_path("a_%r_%o_%p_%%.log", "3", 7, 42) == "a_3_7_42_%.log");
    CHECK_OR_FAIL(expand_comparison_log_path("plain.log", "0", 0, 1) == "plain.log");
    CHECK_OR_FAIL(expand_comparison_log_path("x%", "0", 0, 1) == "x%");
    CHECK_OR_FAIL(expand_comparison_log_path("y%q", "0", 0, 1) == "y%q");

    /* Case 10 (Task 3): current_process_rank_text. First of OMPI_COMM_WORLD_RANK,
       PMIX_RANK, PMI_RANK that is set and non-empty; "0" if none. */
    unset_all_newton_solver_environment_variables();
    CHECK_OR_FAIL(current_process_rank_text() == "0");
    unset_all_newton_solver_environment_variables();
    setenv("PMIX_RANK", "4", 1);
    CHECK_OR_FAIL(current_process_rank_text() == "4");
    unset_all_newton_solver_environment_variables();
    setenv("OMPI_COMM_WORLD_RANK", "5", 1);
    setenv("PMIX_RANK", "4", 1);
    CHECK_OR_FAIL(current_process_rank_text() == "5");
    unset_all_newton_solver_environment_variables();
    CHECK_OR_FAIL(current_process_rank_text() == "0");

    /* Case 11 (Phase 1B Task 1): PHREEQC_LINEAR_SOLVER_INTERCEPT. "entry" gives ENTRY
       and valid; "cl1_problem_shadow" gives SHADOW and valid in LAPACK builds, ENTRY and
       invalid otherwise; "bogus" gives ENTRY and invalid. */
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_INTERCEPT", "entry", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.intercept_mode == NEWTON_INTERCEPT_ENTRY);
        CHECK_OR_FAIL(options.intercept_mode_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_INTERCEPT", "cl1_problem_shadow", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
#ifdef USE_LAPACK_NEWTON_SOLVER
        CHECK_OR_FAIL(options.intercept_mode == NEWTON_INTERCEPT_CL1_PROBLEM_SHADOW);
        CHECK_OR_FAIL(options.intercept_mode_valid);
#else
        CHECK_OR_FAIL(options.intercept_mode == NEWTON_INTERCEPT_ENTRY);
        CHECK_OR_FAIL(!options.intercept_mode_valid);
#endif
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_INTERCEPT", "bogus", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.intercept_mode == NEWTON_INTERCEPT_ENTRY);
        CHECK_OR_FAIL(!options.intercept_mode_valid);
    }

    /* Case 12 (Phase 1B Task 1): PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG_EVERY.
       100 is honoured; 0 and x each give 1 and invalid. */
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG_EVERY", "100", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.cl1_problem_log_every == 100);
        CHECK_OR_FAIL(options.intercept_mode_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG_EVERY", "0", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.cl1_problem_log_every == 1);
        CHECK_OR_FAIL(!options.intercept_mode_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG_EVERY", "x", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.cl1_problem_log_every == 1);
        CHECK_OR_FAIL(!options.intercept_mode_valid);
    }

    /* Case 13 (Phase 1B Task 1): PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG. Unset gives
       the default name (checked in Case 1); a value is honoured. */
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG", "shadow_%o.log", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.cl1_problem_log_path == "shadow_%o.log");
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_LOG", "", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.cl1_problem_log_path == "cl1_problem_shadow.log");
    }

    /* Case 14 (Phase 1B Task 1b): PHREEQC_LINEAR_SOLVER_CONDITION_METRIC. Unset or
       empty or "row" gives 0 and valid; "row_column" gives 1 and valid; anything
       else, including "ROW_COLUMN", gives 0 and invalid. */
    unset_all_newton_solver_environment_variables();
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.condition_metric_row_column == 0);
        CHECK_OR_FAIL(options.condition_options_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_CONDITION_METRIC", "row", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.condition_metric_row_column == 0);
        CHECK_OR_FAIL(options.condition_options_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_CONDITION_METRIC", "row_column", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.condition_metric_row_column == 1);
        CHECK_OR_FAIL(options.condition_options_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_CONDITION_METRIC", "ROW_COLUMN", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.condition_metric_row_column == 0);
        CHECK_OR_FAIL(!options.condition_options_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_CONDITION_METRIC", "bogus", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.condition_metric_row_column == 0);
        CHECK_OR_FAIL(!options.condition_options_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_CONDITION_METRIC", "", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.condition_metric_row_column == 0);
        CHECK_OR_FAIL(options.condition_options_valid);
    }

    /* Case 15 (Phase 1B Task 2): PHREEQC_LINEAR_SOLVER_INTERCEPT="cl1_problem" gives
       mode 2 and valid in LAPACK builds, ENTRY and invalid otherwise. */
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_INTERCEPT", "cl1_problem", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
#ifdef USE_LAPACK_NEWTON_SOLVER
        CHECK_OR_FAIL(options.intercept_mode == NEWTON_INTERCEPT_CL1_PROBLEM);
        CHECK_OR_FAIL(options.intercept_mode_valid);
#else
        CHECK_OR_FAIL(options.intercept_mode == NEWTON_INTERCEPT_ENTRY);
        CHECK_OR_FAIL(!options.intercept_mode_valid);
#endif
    }

    /* Case 16 (Phase 1B Task 2): PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_MAX_COMPONENTWISE_ERROR.
       Unset gives 1e-8; "1e-6" gives 1e-6; "abc" gives 1e-8 and invalid. */
    unset_all_newton_solver_environment_variables();
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.cl1_problem_max_componentwise_error == 1.0e-8);
        CHECK_OR_FAIL(options.intercept_mode_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_MAX_COMPONENTWISE_ERROR", "1e-6", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.cl1_problem_max_componentwise_error == 1.0e-6);
        CHECK_OR_FAIL(options.intercept_mode_valid);
    }
    unset_all_newton_solver_environment_variables();
    setenv("PHREEQC_LINEAR_SOLVER_CL1_PROBLEM_MAX_COMPONENTWISE_ERROR", "abc", 1);
    {
        NewtonLinearSolverOptions options = read_newton_linear_solver_options();
        CHECK_OR_FAIL(options.cl1_problem_max_componentwise_error == 1.0e-8);
        CHECK_OR_FAIL(!options.intercept_mode_valid);
    }

    std::printf("test_newton_linear_solver_options: all cases passed\n");
    return 0;
}
