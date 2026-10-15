/* Phase 2 Task 1b test: LAPACK direct Newton linear-solver backend.
   Same test data as Tasks 2 and 3 (augmented_newton_matrix_3x3, the 4x4 system,
   generated tridiagonal systems). No PETSc anywhere. */
#include "lapack_newton_solver.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>
#include <vector>
#include <unistd.h>

#define CHECK_OR_FAIL(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "FAILED line %d: %s\n", __LINE__, #condition); \
            return 1; \
        } \
    } while (0)

/* 3 x 3 system: augmented_newton_matrix_3x3 from Task 2.
   expected_jacobian  = [ 1 2 0 ; 0 3 4 ; 5 0 6 ]
   expected_newton_step = { 1, -1, 2 }
   row-major, leading_dimension = 4, RHS in column 3. */
static const int system_size_3 = 3;
static const LDBLE augmented_newton_matrix_3x3[3 * 4] = {
    1.0, 2.0, 0.0,  -1.0,
    0.0, 3.0, 4.0,   5.0,
    5.0, 0.0, 6.0,  17.0,
};
static const LDBLE expected_newton_step_3x3[3] = { 1.0, -1.0, 2.0 };

/* 4 x 4 system, also non-symmetric:
   expected_jacobian_4x4 = [2 0 1 0; 1 3 0 0; 0 1 4 1; 0 0 1 5]
   expected_newton_step_4x4 = {1, 2, -1, 1}, expected_rhs_4x4 = {1, 7, -1, 4}
   row-major, leading_dimension = 5, RHS in column 4. */
static const int system_size_4 = 4;
static const LDBLE augmented_newton_matrix_4x4[4 * 5] = {
    2.0, 0.0, 1.0, 0.0,   1.0,
    1.0, 3.0, 0.0, 0.0,   7.0,
    0.0, 1.0, 4.0, 1.0,  -1.0,
    0.0, 0.0, 1.0, 5.0,   4.0,
};
static const LDBLE expected_newton_step_4x4[4] = { 1.0, 2.0, -1.0, 1.0 };

/* Generated systems (Tasks 2 and 3 recipe, extended to n = 60):
   (row, row) = 4; (row, row+1) = 1; (row+1, row) = 2; all others 0.
   expected_newton_step[row] = (row % 3) - 1.  RHS = Jacobian * expected_newton_step. */
static const int generated_size_min = 2;
static const int generated_size_max = 60;
static const int generated_storage_max = generated_size_max * (generated_size_max + 1);

/* Concurrency test sizes and repetition count (test 6). */
static const int concurrent_size_min = 5;
static const int concurrent_size_max = 30;
static const int concurrent_thread_count = 4;
static const int concurrent_iterations = 2000;

static const LDBLE sentinel = 12345.0;

static int close_to(double actual, double expected, double absolute_tolerance)
{
    double difference = actual - expected;
    return difference < 0.0 ? -difference <= absolute_tolerance : difference <= absolute_tolerance;
}

static void fill_sentinel(LDBLE *output, int n)
{
    for (int i = 0; i < n; i++) output[i] = sentinel;
}

static int all_sentinel(const LDBLE *output, int n)
{
    for (int i = 0; i < n; i++) if (output[i] != sentinel) return 0;
    return 1;
}

/* Solves through the PHREEQC layout and requires LINEAR_SOLVE_ACCEPTED with
   every component within absolute_tolerance of expected_newton_step. */
static int solve_and_check_accepted(LapackNewtonSolver *solver, const LDBLE *storage, int n,
                                   const LDBLE *expected_newton_step, double absolute_tolerance,
                                   const char *title)
{
    LDBLE output[generated_size_max];
    fill_sentinel(output, n);
    LinearSolveStatus status = solve_newton_step_with_lapack(solver, storage, n, output);
    CHECK_OR_FAIL(status == LINEAR_SOLVE_ACCEPTED);
    for (int i = 0; i < n; i++) {
        CHECK_OR_FAIL(close_to((double)output[i], (double)expected_newton_step[i], absolute_tolerance));
    }
    if (title != NULL) {
        std::printf("%s\n", title);
        for (int i = 0; i < n; i++) std::printf("  solution[%d] = %.17g\n", i, (double)output[i]);
        std::printf("  last_backward_error = %.17g\n", solver->last_backward_error);
        std::fflush(stdout);
    }
    return 0;
}

/* Builds a generated system in PHREEQC layout (row-major, leading_dimension =
   n + 1, RHS in column n) and its expected solution. */
static void build_generated_system(int n, LDBLE *storage, LDBLE *expected_newton_step)
{
    for (int i = 0; i < n * (n + 1); i++) storage[i] = 0.0;
    for (int row = 0; row < n; row++) expected_newton_step[row] = (LDBLE)((row % 3) - 1);
    for (int row = 0; row < n; row++) {
        storage[row * (n + 1) + row] = 4.0;
        if (row + 1 < n) {
            storage[row * (n + 1) + row + 1] = 1.0;
            storage[(row + 1) * (n + 1) + row] = 2.0;
        }
    }
    for (int row = 0; row < n; row++) {
        LDBLE rhs = 0.0;
        for (int column = 0; column < n; column++) rhs += storage[row * (n + 1) + column] * expected_newton_step[column];
        storage[row * (n + 1) + n] = rhs;
    }
}

/* Test 6 worker: its own solver, generated systems n = 5..30, concurrent_iterations
   times. Records whether every solve was accepted and correct. */
static void concurrent_worker(int thread_index, int *ok_flag)
{
    int ok = 1;
    LapackNewtonSolver *solver = create_lapack_newton_solver();
    if (solver == NULL) { *ok_flag = 0; return; }
    LDBLE storage[concurrent_size_max * (concurrent_size_max + 1)];
    LDBLE expected_newton_step[concurrent_size_max];
    LDBLE output[concurrent_size_max];
    for (int iteration = 0; iteration < concurrent_iterations; iteration++) {
        for (int n = concurrent_size_min; n <= concurrent_size_max; n++) {
            build_generated_system(n, storage, expected_newton_step);
            fill_sentinel(output, n);
            LinearSolveStatus status = solve_newton_step_with_lapack(solver, storage, n, output);
            if (status != LINEAR_SOLVE_ACCEPTED) { ok = 0; break; }
            for (int i = 0; i < n; i++) {
                if (!close_to((double)output[i], (double)expected_newton_step[i], 1e-12)) { ok = 0; break; }
            }
            if (!ok) break;
        }
        if (!ok) break;
    }
    if (solver->accepted_count != (unsigned long)(concurrent_iterations * (concurrent_size_max - concurrent_size_min + 1)))
        ok = 0;
    destroy_lapack_newton_solver(&solver);
    if (solver != NULL) ok = 0;
    std::printf("test 6: thread %d finished, ok = %d\n", thread_index, ok);
    std::fflush(stdout);
    *ok_flag = ok;
}

int main(void)
{
    const double solution_tolerance = 1e-12;

    /* Test 1: 3 x 3 solve, with and without row equilibration. */
    {
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        solver->use_row_equilibration = true;
        CHECK_OR_FAIL(solve_and_check_accepted(solver, augmented_newton_matrix_3x3, system_size_3,
                                               expected_newton_step_3x3, solution_tolerance,
                                               "test 1a: 3x3, use_row_equilibration = true") == 0);
        CHECK_OR_FAIL(solver->last_backward_error <= 1e-14);
        solver->use_row_equilibration = false;
        CHECK_OR_FAIL(solve_and_check_accepted(solver, augmented_newton_matrix_3x3, system_size_3,
                                               expected_newton_step_3x3, solution_tolerance,
                                               "test 1b: 3x3, use_row_equilibration = false") == 0);
        CHECK_OR_FAIL(solver->last_backward_error <= 1e-14);
        destroy_lapack_newton_solver(&solver);
        CHECK_OR_FAIL(solver == NULL);
    }

    /* Test 2: badly scaled row (row 1, including its RHS, multiplied by 1e-12),
       with row equilibration. */
    {
        LDBLE badly_scaled_3x3[3 * 4];
        for (int i = 0; i < 3 * 4; i++) badly_scaled_3x3[i] = augmented_newton_matrix_3x3[i];
        for (int column = 0; column < 4; column++) badly_scaled_3x3[1 * 4 + column] *= 1.0e-12;
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        solver->use_row_equilibration = true;
        CHECK_OR_FAIL(solve_and_check_accepted(solver, badly_scaled_3x3, system_size_3,
                                               expected_newton_step_3x3, solution_tolerance,
                                               "test 2: badly scaled row 1 (x1e-12), use_row_equilibration = true") == 0);
        destroy_lapack_newton_solver(&solver);
        CHECK_OR_FAIL(solver == NULL);
    }

    /* Test 3a: zero row. */
    {
        const LDBLE zero_row_3x3[3 * 4] = {
            1.0, 2.0, 0.0,  -1.0,
            0.0, 0.0, 0.0,   5.0,
            5.0, 0.0, 6.0,  17.0,
        };
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        LDBLE output[3];
        fill_sentinel(output, 3);
        CHECK_OR_FAIL(solve_newton_step_with_lapack(solver, zero_row_3x3, system_size_3, output) == LINEAR_SOLVE_FALL_BACK_TO_CL1);
        CHECK_OR_FAIL(all_sentinel(output, 3));
        destroy_lapack_newton_solver(&solver);
        CHECK_OR_FAIL(solver == NULL);
    }

    /* Test 3b: NaN entry. */
    {
        const LDBLE nan_value = (LDBLE)std::numeric_limits<double>::quiet_NaN();
        LDBLE nan_3x3[3 * 4];
        for (int i = 0; i < 3 * 4; i++) nan_3x3[i] = augmented_newton_matrix_3x3[i];
        nan_3x3[2 * 4 + 1] = nan_value;   /* NaN at Jacobian entry (2,1) */
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        LDBLE output[3];
        fill_sentinel(output, 3);
        CHECK_OR_FAIL(solve_newton_step_with_lapack(solver, nan_3x3, system_size_3, output) == LINEAR_SOLVE_NON_FINITE_INPUT);
        CHECK_OR_FAIL(all_sentinel(output, 3));
        CHECK_OR_FAIL(solver->non_finite_input_count == 1);
        destroy_lapack_newton_solver(&solver);
        CHECK_OR_FAIL(solver == NULL);
    }

    /* Test 3c: exactly singular matrix [[1, 1], [1, 1]] with RHS {1, 1}.
       dgetrf reports the zero pivot through info > 0, not an error:
       lapack_error_count must stay 0 and the call must not crash. */
    {
        const LDBLE singular_2x2[2 * 3] = {
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
        };
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        LDBLE output[2];
        fill_sentinel(output, 2);
        CHECK_OR_FAIL(solve_newton_step_with_lapack(solver, singular_2x2, 2, output) == LINEAR_SOLVE_FALL_BACK_TO_CL1);
        CHECK_OR_FAIL(all_sentinel(output, 2));
        std::printf("test 3c: exactly singular 2x2: lapack_error_count = %lu, cl1_fallback_count = %lu\n",
                    solver->lapack_error_count, solver->cl1_fallback_count);
        std::fflush(stdout);
        CHECK_OR_FAIL(solver->lapack_error_count == 0);
        destroy_lapack_newton_solver(&solver);
        CHECK_OR_FAIL(solver == NULL);
    }

    /* Test 4: generated systems, n = 2..60. All accepted, errors within 1e-12.
       Growing n exercises buffer growth between calls. */
    {
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        LDBLE storage[generated_storage_max];
        LDBLE expected_newton_step[generated_size_max];
        for (int n = generated_size_min; n <= generated_size_max; n++) {
            build_generated_system(n, storage, expected_newton_step);
            CHECK_OR_FAIL(solve_and_check_accepted(solver, storage, n, expected_newton_step,
                                                   solution_tolerance, NULL) == 0);
        }
        std::printf("test 4: generated systems n = %d..%d all accepted, solve_count = %lu\n",
                    generated_size_min, generated_size_max, solver->solve_count);
        std::fflush(stdout);
        destroy_lapack_newton_solver(&solver);
        CHECK_OR_FAIL(solver == NULL);
    }

    /* Test 5: alternate the 3x3 and 4x4 systems, 10 times each, one solver. */
    {
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        for (int round = 0; round < 10; round++) {
            CHECK_OR_FAIL(solve_and_check_accepted(solver, augmented_newton_matrix_3x3, system_size_3,
                                                   expected_newton_step_3x3, solution_tolerance, NULL) == 0);
            CHECK_OR_FAIL(solve_and_check_accepted(solver, augmented_newton_matrix_4x4, system_size_4,
                                                   expected_newton_step_4x4, solution_tolerance, NULL) == 0);
        }
        std::printf("test 5: 10 rounds of 3x3/4x4 all correct, accepted_count = %lu\n",
                    solver->accepted_count);
        std::fflush(stdout);
        destroy_lapack_newton_solver(&solver);
        CHECK_OR_FAIL(solver == NULL);
    }

    /* Test 6: concurrency. Four threads, each with its own solver, solving the
       generated systems n = 5..30 in a loop, concurrent_iterations times. The
       main thread joins and checks every flag: the solver keeps no shared state. */
    {
        int ok_flag[concurrent_thread_count];
        std::thread workers[concurrent_thread_count];
        for (int t = 0; t < concurrent_thread_count; t++) {
            ok_flag[t] = 0;
            workers[t] = std::thread(concurrent_worker, t, &ok_flag[t]);
        }
        for (int t = 0; t < concurrent_thread_count; t++) workers[t].join();
        for (int t = 0; t < concurrent_thread_count; t++) {
            std::printf("test 6: thread %d ok = %d\n", t, ok_flag[t]);
            std::fflush(stdout);
            CHECK_OR_FAIL(ok_flag[t] == 1);
        }
    }

    /* Test 7: statistics. destroy_lapack_newton_solver must print exactly one
       line in the required format to stderr. Capture stderr through a
       temporary file (dup2/freopen), then verify it. */
    {
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        CHECK_OR_FAIL(solve_and_check_accepted(solver, augmented_newton_matrix_3x3, system_size_3,
                                               expected_newton_step_3x3, solution_tolerance, NULL) == 0);
        solver->print_statistics = true;

        char temp_name[] = "test_lapack_newton_solver_statistics.XXXXXX";
        int temp_descriptor = mkstemp(temp_name);   /* cwd is the build directory under ctest */
        CHECK_OR_FAIL(temp_descriptor != -1);
        FILE *temp_file = fdopen(temp_descriptor, "w");
        CHECK_OR_FAIL(temp_file != NULL);
        int saved_stderr = dup(fileno(stderr));
        CHECK_OR_FAIL(saved_stderr != -1);
        CHECK_OR_FAIL(dup2(fileno(temp_file), fileno(stderr)) != -1);
        destroy_lapack_newton_solver(&solver);
        CHECK_OR_FAIL(solver == NULL);
        fflush(stderr);
        CHECK_OR_FAIL(dup2(saved_stderr, fileno(stderr)) != -1);
        close(saved_stderr);
        fclose(temp_file);

        FILE *captured = fopen(temp_name, "r");
        CHECK_OR_FAIL(captured != NULL);
        char line[1024];
        int line_count = 0;
        char statistics_line[1024];
        statistics_line[0] = '\0';
        while (fgets(line, sizeof(line), captured) != NULL) {
            if (line_count == 0) {
                std::snprintf(statistics_line, sizeof(statistics_line), "%s", line);
            }
            line_count++;
        }
        fclose(captured);
        unlink(temp_name);
        std::printf("test 7: captured stderr (%d line(s)):\n%s", line_count, statistics_line);
        std::fflush(stdout);
        CHECK_OR_FAIL(line_count == 1);
        CHECK_OR_FAIL(strstr(statistics_line, "phreeqc_lapack_statistics:") == statistics_line);
        CHECK_OR_FAIL(strstr(statistics_line, "solves=") != NULL);
        CHECK_OR_FAIL(strstr(statistics_line, "accepted=") != NULL);
        CHECK_OR_FAIL(strstr(statistics_line, "cl1_fallbacks=") != NULL);
        CHECK_OR_FAIL(strstr(statistics_line, "non_finite_inputs=") != NULL);
        CHECK_OR_FAIL(strstr(statistics_line, "lapack_errors=") != NULL);
        CHECK_OR_FAIL(strchr(statistics_line, '\n') != NULL);   /* exactly one line, newline-terminated */
    }

    /* Test 8 (Task 2d): condition check mode LOG on the 3x3 system. The solve must
       still be accepted, and last_reciprocal_condition must be a real estimate in
       (0.001, 1]. The mode never changes the decision. */
    {
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        solver->condition_check_mode = NEWTON_CONDITION_CHECK_LOG;
        CHECK_OR_FAIL(solve_and_check_accepted(solver, augmented_newton_matrix_3x3, system_size_3,
                                               expected_newton_step_3x3, solution_tolerance,
                                               "test 8: 3x3, condition_check_mode = LOG") == 0);
        std::printf("test 8: last_reciprocal_condition = %.17g\n", solver->last_reciprocal_condition);
        std::fflush(stdout);
        CHECK_OR_FAIL(solver->last_reciprocal_condition > 0.001);
        CHECK_OR_FAIL(solver->last_reciprocal_condition <= 1.0);
        CHECK_OR_FAIL(solver->ill_conditioned_count == 0);
        CHECK_OR_FAIL(solver->lapack_error_count == 0);
        destroy_lapack_newton_solver(&solver);
        CHECK_OR_FAIL(solver == NULL);
    }

    /* Test 9 (Task 2d): nearly singular 2x2 system J = [[1, 1], [1, 1 + 1e-14]],
       r = [2, 2 + 1e-14]. Row equilibration leaves it nearly singular; dgecon must
       report a reciprocal condition below 1e-12.
       LOG: decision unchanged, estimate recorded.
       GATE with min_reciprocal_condition = 1e-10: fall back to cl1, output unchanged. */
    {
        const LDBLE near_singular_2x2[2 * 3] = {
            1.0, 1.0,               2.0,
            1.0, 1.0 + 1.0e-14,    2.0 + 1.0e-14,
        };
        /* LOG mode: report status, reciprocal condition, backward error. */
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        solver->condition_check_mode = NEWTON_CONDITION_CHECK_LOG;
        LDBLE output[2];
        fill_sentinel(output, 2);
        LinearSolveStatus status = solve_newton_step_with_lapack(solver, near_singular_2x2, 2, output);
        std::printf("test 9a: near-singular 2x2, LOG: status = %d, last_reciprocal_condition = %.17g, "
                    "last_backward_error = %.17g\n",
                    (int)status, solver->last_reciprocal_condition, solver->last_backward_error);
        std::fflush(stdout);
        CHECK_OR_FAIL(solver->last_reciprocal_condition >= 0.0);
        CHECK_OR_FAIL(solver->last_reciprocal_condition < 1.0e-12);
        CHECK_OR_FAIL(solver->ill_conditioned_count == 0);   /* LOG never gates */
        destroy_lapack_newton_solver(&solver);
        CHECK_OR_FAIL(solver == NULL);

        /* GATE mode, min_reciprocal_condition = 1e-10: gated to cl1, output unchanged. */
        solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        solver->condition_check_mode = NEWTON_CONDITION_CHECK_GATE;
        solver->min_reciprocal_condition = 1.0e-10;
        fill_sentinel(output, 2);
        status = solve_newton_step_with_lapack(solver, near_singular_2x2, 2, output);
        std::printf("test 9b: near-singular 2x2, GATE min=1e-10: status = %d, "
                    "last_reciprocal_condition = %.17g, ill_conditioned_count = %lu\n",
                    (int)status, solver->last_reciprocal_condition, solver->ill_conditioned_count);
        std::fflush(stdout);
        CHECK_OR_FAIL(status == LINEAR_SOLVE_FALL_BACK_TO_CL1);
        CHECK_OR_FAIL(solver->ill_conditioned_count == 1);
        CHECK_OR_FAIL(all_sentinel(output, 2));
        CHECK_OR_FAIL(solver->last_reciprocal_condition < 1.0e-12);
        destroy_lapack_newton_solver(&solver);
        CHECK_OR_FAIL(solver == NULL);
    }

    /* Test 10 (Task 2d): mode OFF (the default). After a solve,
       last_reciprocal_condition must still be -1.0: nothing is computed. */
    {
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        CHECK_OR_FAIL(solver->condition_check_mode == NEWTON_CONDITION_CHECK_OFF);
        CHECK_OR_FAIL(solve_and_check_accepted(solver, augmented_newton_matrix_3x3, system_size_3,
                                               expected_newton_step_3x3, solution_tolerance,
                                               "test 10: 3x3, condition_check_mode = OFF") == 0);
        std::printf("test 10: last_reciprocal_condition = %.17g\n", solver->last_reciprocal_condition);
        std::fflush(stdout);
        CHECK_OR_FAIL(solver->last_reciprocal_condition == -1.0);
        CHECK_OR_FAIL(solver->ill_conditioned_count == 0);
        CHECK_OR_FAIL(solver->lapack_error_count == 0);
        destroy_lapack_newton_solver(&solver);
        CHECK_OR_FAIL(solver == NULL);
    }

    /* Test 11 (Phase 1B Task 1b): the 3x3 system of Task 2 with column 2 multiplied
       by 1e-12. Row equilibration cannot undo a column scaling: the row-only
       reciprocal condition is below 1e-8, while the reciprocal condition after row AND
       column equilibration is above 1e-3. LOG mode: the solve is still accepted, both
       metrics are recorded, and the solution is the original one with the column
       scaling undone (y[2] itself is 1e12 times larger). */
    {
        LDBLE scaled_column_3x3[3 * 4];
        for (int i = 0; i < 3 * 4; i++) scaled_column_3x3[i] = augmented_newton_matrix_3x3[i];
        for (int row = 0; row < 3; row++) scaled_column_3x3[row * 4 + 2] *= 1.0e-12;
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        solver->condition_check_mode = NEWTON_CONDITION_CHECK_LOG;
        solver->compute_equilibrated_condition = true;
        LDBLE output[3];
        fill_sentinel(output, 3);
        LinearSolveStatus status = solve_newton_step_with_lapack(solver, scaled_column_3x3, system_size_3, output);
        std::printf("test 11: 3x3 column 2 x1e-12, LOG: status = %d, last_reciprocal_condition = %.17g, "
                    "last_equilibrated_reciprocal_condition = %.17g, output[2] = %.17g\n",
                    (int)status, solver->last_reciprocal_condition,
                    solver->last_equilibrated_reciprocal_condition, (double)output[2]);
        std::fflush(stdout);
        CHECK_OR_FAIL(status == LINEAR_SOLVE_ACCEPTED);
        CHECK_OR_FAIL(solver->last_reciprocal_condition >= 0.0);
        CHECK_OR_FAIL(solver->last_reciprocal_condition < 1.0e-8);
        CHECK_OR_FAIL(solver->last_equilibrated_reciprocal_condition > 1.0e-3);
        CHECK_OR_FAIL(std::fabs((double)output[2] * 1.0e-12 - (double)expected_newton_step_3x3[2])
                      <= 1.0e-12 * std::fabs((double)expected_newton_step_3x3[2]));
        CHECK_OR_FAIL(close_to((double)output[0], (double)expected_newton_step_3x3[0], solution_tolerance));
        CHECK_OR_FAIL(close_to((double)output[1], (double)expected_newton_step_3x3[1], solution_tolerance));
        destroy_lapack_newton_solver(&solver);
        CHECK_OR_FAIL(solver == NULL);
    }

    /* Test 12 (Phase 1B Task 1b): the same badly scaled-column system, GATE mode with
       minimum 1e-8. With the row metric (the default) it falls back to cl1 and
       ill_conditioned_count becomes 1; with gate_on_equilibrated_condition = 1 it is
       accepted, and the row metric is still computed and recorded. */
    {
        LDBLE scaled_column_3x3[3 * 4];
        for (int i = 0; i < 3 * 4; i++) scaled_column_3x3[i] = augmented_newton_matrix_3x3[i];
        for (int row = 0; row < 3; row++) scaled_column_3x3[row * 4 + 2] *= 1.0e-12;

        /* 12a: row metric (default). */
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        solver->condition_check_mode = NEWTON_CONDITION_CHECK_GATE;
        solver->min_reciprocal_condition = 1.0e-8;
        LDBLE output[3];
        fill_sentinel(output, 3);
        LinearSolveStatus status = solve_newton_step_with_lapack(solver, scaled_column_3x3, system_size_3, output);
        std::printf("test 12a: GATE min=1e-8, row metric: status = %d, last_reciprocal_condition = %.17g, "
                    "last_equilibrated_reciprocal_condition = %.17g, ill_conditioned_count = %lu\n",
                    (int)status, solver->last_reciprocal_condition,
                    solver->last_equilibrated_reciprocal_condition, solver->ill_conditioned_count);
        std::fflush(stdout);
        CHECK_OR_FAIL(status == LINEAR_SOLVE_FALL_BACK_TO_CL1);
        CHECK_OR_FAIL(solver->ill_conditioned_count == 1);
        CHECK_OR_FAIL(all_sentinel(output, 3));
        CHECK_OR_FAIL(solver->last_equilibrated_reciprocal_condition == -1.0);   /* no flag: not computed */
        destroy_lapack_newton_solver(&solver);
        CHECK_OR_FAIL(solver == NULL);

        /* 12b: gate on the row-and-column metric. */
        solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        solver->condition_check_mode = NEWTON_CONDITION_CHECK_GATE;
        solver->min_reciprocal_condition = 1.0e-8;
        solver->gate_on_equilibrated_condition = 1;
        fill_sentinel(output, 3);
        status = solve_newton_step_with_lapack(solver, scaled_column_3x3, system_size_3, output);
        std::printf("test 12b: GATE min=1e-8, gate_on_equilibrated_condition = 1: status = %d, "
                    "last_reciprocal_condition = %.17g, last_equilibrated_reciprocal_condition = %.17g, "
                    "ill_conditioned_count = %lu\n",
                    (int)status, solver->last_reciprocal_condition,
                    solver->last_equilibrated_reciprocal_condition, solver->ill_conditioned_count);
        std::fflush(stdout);
        CHECK_OR_FAIL(status == LINEAR_SOLVE_ACCEPTED);
        CHECK_OR_FAIL(solver->ill_conditioned_count == 0);
        CHECK_OR_FAIL(solver->last_reciprocal_condition < 1.0e-8);   /* row metric still recorded */
        CHECK_OR_FAIL(solver->last_equilibrated_reciprocal_condition > 1.0e-3);
        CHECK_OR_FAIL(std::fabs((double)output[2] * 1.0e-12 - (double)expected_newton_step_3x3[2])
                      <= 1.0e-12 * std::fabs((double)expected_newton_step_3x3[2]));
        CHECK_OR_FAIL(close_to((double)output[0], (double)expected_newton_step_3x3[0], solution_tolerance));
        CHECK_OR_FAIL(close_to((double)output[1], (double)expected_newton_step_3x3[1], solution_tolerance));
        destroy_lapack_newton_solver(&solver);
        CHECK_OR_FAIL(solver == NULL);
    }

    /* Test 13 (Phase 1B Task 1b): the nearly singular system of test 9 with
       compute_equilibrated_condition = true. A genuinely nearly singular matrix stays
       nearly singular after column scaling: last_equilibrated_reciprocal_condition is
       below 1e-12. Mode OFF: the row metric is not computed, the equilibrated one is. */
    {
        const LDBLE near_singular_2x2[2 * 3] = {
            1.0, 1.0,               2.0,
            1.0, 1.0 + 1.0e-14,    2.0 + 1.0e-14,
        };
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        CHECK_OR_FAIL(solver->condition_check_mode == NEWTON_CONDITION_CHECK_OFF);
        solver->compute_equilibrated_condition = true;
        LDBLE output[2];
        fill_sentinel(output, 2);
        LinearSolveStatus status = solve_newton_step_with_lapack(solver, near_singular_2x2, 2, output);
        std::printf("test 13: near-singular 2x2, compute_equilibrated_condition = true, mode OFF: "
                    "status = %d, last_reciprocal_condition = %.17g, "
                    "last_equilibrated_reciprocal_condition = %.17g\n",
                    (int)status, solver->last_reciprocal_condition,
                    solver->last_equilibrated_reciprocal_condition);
        std::fflush(stdout);
        CHECK_OR_FAIL(solver->last_reciprocal_condition == -1.0);   /* mode OFF: row metric not computed */
        CHECK_OR_FAIL(solver->last_equilibrated_reciprocal_condition >= 0.0);
        CHECK_OR_FAIL(solver->last_equilibrated_reciprocal_condition < 1.0e-12);
        CHECK_OR_FAIL(solver->ill_conditioned_count == 0);          /* mode OFF: never gates */
        destroy_lapack_newton_solver(&solver);
        CHECK_OR_FAIL(solver == NULL);
    }

    /* Test 14 (Phase 1B Task 3, decision B-D11; gate coverage per Phase 1B Task 3b):
       the fast two-sided metric, obtained from the LU factors of the row-equilibrated
       matrix by scaling U by the column maxima, equals the full method (a second
       dgetrf on the column-scaled matrix) to 1e-6 relative when both values are
       >= 1e-8 and to 1e-4 relative when both values are < 1e-8 and > 1e-14
       (decision B-D14), and gives the same gate decisions at 1e-8 in every case.
       At
       least 80 of the 200 cases are gated by both methods (both values < 1e-8) and
       at least 80 are passed by both (both values >= 1e-8). Generator: the Phase 1B
       Task 1 benchmark LCG (the only LCG in this repository), seeded with 777. Draw
       order: entries row-major, then (odd matrix indices only) the last-row
       perturbations over the matrix columns, then the column scale factors, then the
       RHS. The near-singular last row is built before the column scaling so the
       1e-10 perturbation scales with the columns and the matrix really is
       near-singular. */
    {
        unsigned long long lcg_state = 777ULL;
        auto next_uniform = [&lcg_state]() {
            lcg_state = lcg_state * 6364136223846793005ULL + 1442695040888963407ULL;
            return (double)(lcg_state >> 11) * (1.0 / 9007199254740992.0);
        };

        const int case_count = 200;
        const double agreement_tolerance_well_conditioned = 1.0e-6;
        const double agreement_tolerance_near_singular = 1.0e-4;
        const double gate_decision_minimum = 1.0e-8;
        const double small_value_cutoff = 1.0e-14;
        const int minimum_both_below = 80;
        const int minimum_both_at_or_above = 80;
        double largest_relative_difference_well_conditioned = 0.0;
        double largest_relative_difference_near_singular = 0.0;
        double smallest_value_seen = 1.0;
        double largest_value_seen = 0.0;
        int compared_count = 0, gated_a_count = 0, gated_b_count = 0;
        int both_below_count = 0, both_at_or_above_count = 0;

        for (int t = 0; t < case_count; t++) {
            int n = 5 + (t % 26);
            int ld = n + 1;
            std::vector<LDBLE> a((size_t)n * (size_t)ld);
            for (int i = 0; i < n; i++)
                for (int j = 0; j < n; j++)
                    a[(size_t)i * (size_t)ld + (size_t)j] = (LDBLE)(2.0 * next_uniform() - 1.0);
            for (int i = 0; i < n; i++)
                a[(size_t)i * (size_t)ld + (size_t)i] = (LDBLE)(n + 1);
            /* Odd matrix indices: replace the last row with row 0 plus a tiny
               perturbation, before the column scaling, so the perturbation is
               scaled with the columns and the matrix really is near-singular. */
            if (t % 2 == 1)
                for (int j = 0; j < n; j++)
                    a[(size_t)(n - 1) * (size_t)ld + (size_t)j] =
                        (LDBLE)((double)a[(size_t)j] + 1.0e-10 * (2.0 * next_uniform() - 1.0));
            for (int j = 0; j < n; j++) {
                double factor = std::pow(10.0, -12.0 * next_uniform());
                for (int i = 0; i < n; i++)
                    a[(size_t)i * (size_t)ld + (size_t)j] *= (LDBLE)factor;
            }
            for (int i = 0; i < n; i++)
                a[(size_t)i * (size_t)ld + (size_t)n] = (LDBLE)(2.0 * next_uniform() - 1.0);

            /* Method A: the full method, second dgetrf on the column-scaled matrix. */
            LapackNewtonSolver *solver_a = create_lapack_newton_solver();
            CHECK_OR_FAIL(solver_a != NULL);
            solver_a->condition_check_mode = NEWTON_CONDITION_CHECK_LOG;
            solver_a->compute_equilibrated_condition = true;
            std::vector<LDBLE> output_a((size_t)n, sentinel), output_b((size_t)n, sentinel);
            LinearSolveStatus status_a = solve_newton_step_with_lapack(solver_a, a.data(), n, output_a.data());
            double value_a = solver_a->last_equilibrated_reciprocal_condition;
            destroy_lapack_newton_solver(&solver_a);
            CHECK_OR_FAIL(solver_a == NULL);

            /* Method B: the fast path, from the factors already computed. */
            LapackNewtonSolver *solver_b = create_lapack_newton_solver();
            CHECK_OR_FAIL(solver_b != NULL);
            solver_b->condition_check_mode = NEWTON_CONDITION_CHECK_LOG;
            solver_b->equilibrated_condition_from_factors = true;
            LinearSolveStatus status_b = solve_newton_step_with_lapack(solver_b, a.data(), n, output_b.data());
            double value_b = solver_b->last_equilibrated_reciprocal_condition;
            CHECK_OR_FAIL(solver_b->last_reciprocal_condition == -1.0);   /* row metric skipped */
            CHECK_OR_FAIL(solver_b->equilibrated_pivots.empty());         /* no second dgetrf */
            destroy_lapack_newton_solver(&solver_b);
            CHECK_OR_FAIL(solver_b == NULL);

            /* The two methods factor and solve identically: only the metric differs. */
            CHECK_OR_FAIL(status_a == status_b);
            for (int i = 0; i < n; i++)
                CHECK_OR_FAIL(output_a[(size_t)i] == output_b[(size_t)i]);

            if (value_a >= 0.0) {
                if (value_a < smallest_value_seen)
                    smallest_value_seen = value_a;
                if (value_a > largest_value_seen)
                    largest_value_seen = value_a;
            }
            if (value_b >= 0.0) {
                if (value_b < smallest_value_seen)
                    smallest_value_seen = value_b;
                if (value_b > largest_value_seen)
                    largest_value_seen = value_b;
            }

            /* Same gate decision at 1e-8: a method gates the case when its value
               is below the gate minimum (-1 means "not computed", never gates). */
            bool a_gates = (value_a >= 0.0 && value_a < gate_decision_minimum);
            bool b_gates = (value_b >= 0.0 && value_b < gate_decision_minimum);
            CHECK_OR_FAIL(a_gates == b_gates);
            if (a_gates)
                gated_a_count++;
            if (b_gates)
                gated_b_count++;
            if (a_gates && b_gates)
                both_below_count++;
            if (value_a >= gate_decision_minimum && value_b >= gate_decision_minimum)
                both_at_or_above_count++;

            /* Region-split agreement (decision B-D14): the gate decisions were
               shown equal above, so both values here (> 1e-14) lie in the same
               region: both >= 1e-8, or both < 1e-8. */
            if (value_a > small_value_cutoff && value_b > small_value_cutoff) {
                double larger = value_a > value_b ? value_a : value_b;
                double difference = std::fabs(value_a - value_b) / larger;
                compared_count++;
                if (value_a >= gate_decision_minimum && value_b >= gate_decision_minimum) {
                    if (difference > largest_relative_difference_well_conditioned)
                        largest_relative_difference_well_conditioned = difference;
                    CHECK_OR_FAIL(difference <= agreement_tolerance_well_conditioned);
                } else {
                    if (difference > largest_relative_difference_near_singular)
                        largest_relative_difference_near_singular = difference;
                    CHECK_OR_FAIL(difference <= agreement_tolerance_near_singular);
                }
            }
        }
        std::printf("test 14: %d cases, %d values compared, gated at 1e-8: method A %d, "
                    "method B %d, both below 1e-8 %d, both at or above 1e-8 %d, "
                    "smallest value seen = %.3e, largest value seen = %.3e, "
                    "largest relative difference: both >= 1e-8 = %.3e, both < 1e-8 = %.3e\n",
                    case_count, compared_count, gated_a_count, gated_b_count,
                    both_below_count, both_at_or_above_count,
                    smallest_value_seen, largest_value_seen,
                    largest_relative_difference_well_conditioned,
                    largest_relative_difference_near_singular);
        std::fflush(stdout);
        CHECK_OR_FAIL(compared_count > 0);
        CHECK_OR_FAIL(largest_relative_difference_well_conditioned <= agreement_tolerance_well_conditioned);
        CHECK_OR_FAIL(largest_relative_difference_near_singular <= agreement_tolerance_near_singular);
        CHECK_OR_FAIL(both_below_count >= minimum_both_below);
        CHECK_OR_FAIL(both_at_or_above_count >= minimum_both_at_or_above);
    }

    std::printf("PASS test_lapack_newton_solver\n");
    return 0;
}
