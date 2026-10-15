/* Phase 1B Task 1 test: the cl1-problem check (shadow mode). LAPACK builds only.
   Each problem is built in a q buffer with the given row stride, with the right-hand
   side in column n. No PETSc anywhere. */
#include "cl1_problem_check.h"
#include "newton_linear_solver_state.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define CHECK_OR_FAIL(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "FAILED line %d: %s\n", __LINE__, #condition); \
            return 1; \
        } \
    } while (0)

/* Problem 1: k = 1, l = 2, m = 0, n = 3, row_stride = n + 1 = 4.
   A row:  1 2 0 | -1     C rows: 0 3 4 | 5    and   5 0 6 | 17
   Solution x = {1, -1, 2} (the 3x3 system of the Task 1b/2/3 tests). */
static const int p1_n = 3;
static const LDBLE p1_q[3 * 4] = {
    1.0, 2.0, 0.0,  -1.0,
    0.0, 3.0, 4.0,   5.0,
    5.0, 0.0, 6.0,  17.0,
};
static const LDBLE p1_expected_x[3] = { 1.0, -1.0, 2.0 };

static bool x_close(const std::vector<LDBLE> &x, const LDBLE *expected, int n, double tolerance)
{
    for (int j = 0; j < n; ++j)
        if (std::fabs((double)x[(size_t)j] - (double)expected[j]) > tolerance)
            return false;
    return true;
}

int main()
{
    /* Test 1: square, equality only. Expect ACCEPT, solved, x within 1e-12. */
    {
        LDBLE signs[3] = { 0.0, 0.0, 0.0 };
        Cl1ProblemView view;
        view.k = 1; view.l = 2; view.m = 0; view.n = p1_n; view.row_stride = p1_n + 1;
        view.q = p1_q; view.variable_signs = signs;
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        Cl1ProblemCandidate candidate;
        std::vector<LDBLE> augmented, live_solution;
        evaluate_cl1_problem(solver, view, candidate, augmented, live_solution, 1.0e-8);
        std::printf("test 1: outcome = %d, solved = %d, live = %d\n",
                    candidate.outcome, (int)candidate.solved, candidate.live_column_count);
        CHECK_OR_FAIL(candidate.outcome == CL1_PROBLEM_ACCEPT);
        CHECK_OR_FAIL(candidate.solved);
        CHECK_OR_FAIL(candidate.live_column_count == 3);
        CHECK_OR_FAIL((int)candidate.x.size() == p1_n);
        CHECK_OR_FAIL(x_close(candidate.x, p1_expected_x, p1_n, 1.0e-12));
        CHECK_OR_FAIL(candidate.violated_row == -1 && candidate.violated_column == -1);
        destroy_lapack_newton_solver(&solver);
    }

    /* Test 2: problem 1 plus one E row x0 <= 10: expect ACCEPT.
       With x0 <= 0.5, where the solution has x0 = 1: expect INEQUALITY_VIOLATED,
       violated_row == 3, and largest_violation within 1e-12 of 0.5. */
    {
        LDBLE q[4 * 4];
        for (size_t t = 0; t < sizeof(p1_q) / sizeof(p1_q[0]); ++t)
            q[t] = p1_q[t];
        q[3 * 4 + 0] = 1.0; q[3 * 4 + 1] = 0.0; q[3 * 4 + 2] = 0.0; q[3 * 4 + 3] = 10.0;
        LDBLE signs[3] = { 0.0, 0.0, 0.0 };
        Cl1ProblemView view;
        view.k = 1; view.l = 2; view.m = 1; view.n = p1_n; view.row_stride = p1_n + 1;
        view.q = q; view.variable_signs = signs;
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        Cl1ProblemCandidate candidate;
        std::vector<LDBLE> augmented, live_solution;
        evaluate_cl1_problem(solver, view, candidate, augmented, live_solution, 1.0e-8);
        std::printf("test 2a: outcome = %d\n", candidate.outcome);
        CHECK_OR_FAIL(candidate.outcome == CL1_PROBLEM_ACCEPT);
        CHECK_OR_FAIL(candidate.solved);
        CHECK_OR_FAIL(x_close(candidate.x, p1_expected_x, p1_n, 1.0e-12));

        q[3 * 4 + 3] = 0.5;   /* x0 <= 0.5, violated by 0.5 */
        evaluate_cl1_problem(solver, view, candidate, augmented, live_solution, 1.0e-8);
        std::printf("test 2b: outcome = %d, violated_row = %d, largest_violation = %.17g\n",
                    candidate.outcome, candidate.violated_row, candidate.largest_violation);
        CHECK_OR_FAIL(candidate.outcome == CL1_PROBLEM_INEQUALITY_VIOLATED);
        CHECK_OR_FAIL(candidate.violated_row == 3);
        CHECK_OR_FAIL(std::fabs(candidate.largest_violation - 0.5) <= 1.0e-12);
        CHECK_OR_FAIL(candidate.violated_column == -1);
        destroy_lapack_newton_solver(&solver);
    }

    /* Test 3: problem 1 with variable_signs[0] = -1 and the solution x0 = 1:
       expect SIGN_VIOLATED, violated_column == 0.
       With +1 on a variable whose solution is negative (x1 = -1): also SIGN_VIOLATED. */
    {
        Cl1ProblemView view;
        view.k = 1; view.l = 2; view.m = 0; view.n = p1_n; view.row_stride = p1_n + 1;
        view.q = p1_q;
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        Cl1ProblemCandidate candidate;
        std::vector<LDBLE> augmented, live_solution;

        LDBLE signs_minus[3] = { -1.0, 0.0, 0.0 };
        view.variable_signs = signs_minus;
        evaluate_cl1_problem(solver, view, candidate, augmented, live_solution, 1.0e-8);
        std::printf("test 3a: outcome = %d, violated_column = %d\n",
                    candidate.outcome, candidate.violated_column);
        CHECK_OR_FAIL(candidate.outcome == CL1_PROBLEM_SIGN_VIOLATED);
        CHECK_OR_FAIL(candidate.violated_column == 0);

        LDBLE signs_plus[3] = { 0.0, 1.0, 0.0 };
        view.variable_signs = signs_plus;
        evaluate_cl1_problem(solver, view, candidate, augmented, live_solution, 1.0e-8);
        std::printf("test 3b: outcome = %d, violated_column = %d\n",
                    candidate.outcome, candidate.violated_column);
        CHECK_OR_FAIL(candidate.outcome == CL1_PROBLEM_SIGN_VIOLATED);
        CHECK_OR_FAIL(candidate.violated_column == 1);
        destroy_lapack_newton_solver(&solver);
    }

    /* Test 4: zero column. n = 4, column 3 all zero, k + l = 3.
       Expect ACCEPT, live_column_count == 3, x[3] == 0. */
    {
        const int n4 = 4;
        LDBLE q[3 * 5] = {
            1.0, 2.0, 0.0, 0.0,  -1.0,
            0.0, 3.0, 4.0, 0.0,   5.0,
            5.0, 0.0, 6.0, 0.0,  17.0,
        };
        LDBLE signs[4] = { 0.0, 0.0, 0.0, 0.0 };
        Cl1ProblemView view;
        view.k = 1; view.l = 2; view.m = 0; view.n = n4; view.row_stride = n4 + 1;
        view.q = q; view.variable_signs = signs;
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        Cl1ProblemCandidate candidate;
        std::vector<LDBLE> augmented, live_solution;
        evaluate_cl1_problem(solver, view, candidate, augmented, live_solution, 1.0e-8);
        std::printf("test 4: outcome = %d, live = %d, x[3] = %.17g\n",
                    candidate.outcome, candidate.live_column_count, (double)candidate.x[3]);
        CHECK_OR_FAIL(candidate.outcome == CL1_PROBLEM_ACCEPT);
        CHECK_OR_FAIL(candidate.live_column_count == 3);
        CHECK_OR_FAIL(candidate.live[0] == 1 && candidate.live[1] == 1
                      && candidate.live[2] == 1 && candidate.live[3] == 0);
        CHECK_OR_FAIL(candidate.x[3] == 0.0);
        CHECK_OR_FAIL(x_close(candidate.x, p1_expected_x, p1_n, 1.0e-12));
        destroy_lapack_newton_solver(&solver);
    }

    /* Test 5: not square. k + l = 2 with 3 live columns gives NOT_SQUARE. */
    {
        LDBLE q[2 * 4] = {
            1.0, 2.0, 0.0,  -1.0,
            0.0, 3.0, 4.0,   5.0,
        };
        LDBLE signs[3] = { 0.0, 0.0, 0.0 };
        Cl1ProblemView view;
        view.k = 1; view.l = 1; view.m = 0; view.n = p1_n; view.row_stride = p1_n + 1;
        view.q = q; view.variable_signs = signs;
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        Cl1ProblemCandidate candidate;
        std::vector<LDBLE> augmented, live_solution;
        evaluate_cl1_problem(solver, view, candidate, augmented, live_solution, 1.0e-8);
        std::printf("test 5: outcome = %d, live = %d\n",
                    candidate.outcome, candidate.live_column_count);
        CHECK_OR_FAIL(candidate.outcome == CL1_PROBLEM_NOT_SQUARE);
        CHECK_OR_FAIL(candidate.live_column_count == 3);
        CHECK_OR_FAIL(!candidate.solved);
        destroy_lapack_newton_solver(&solver);
    }

    /* Test 6: nearly singular rows (test_lapack_newton_solver.cpp test 9), gate mode,
       minimum 1e-8: ILL_CONDITIONED. */
    {
        LDBLE q[2 * 3] = {
            1.0, 1.0,             2.0,
            1.0, 1.0 + 1.0e-14,   2.0 + 1.0e-14,
        };
        LDBLE signs[2] = { 0.0, 0.0 };
        Cl1ProblemView view;
        view.k = 1; view.l = 1; view.m = 0; view.n = 2; view.row_stride = 3;
        view.q = q; view.variable_signs = signs;
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        solver->condition_check_mode = NEWTON_CONDITION_CHECK_GATE;
        solver->min_reciprocal_condition = 1.0e-8;
        Cl1ProblemCandidate candidate;
        std::vector<LDBLE> augmented, live_solution;
        evaluate_cl1_problem(solver, view, candidate, augmented, live_solution, 1.0e-8);
        std::printf("test 6: outcome = %d, reciprocal_condition = %.17g\n",
                    candidate.outcome, candidate.reciprocal_condition);
        CHECK_OR_FAIL(candidate.outcome == CL1_PROBLEM_ILL_CONDITIONED);
        CHECK_OR_FAIL(!candidate.solved);
        CHECK_OR_FAIL(candidate.reciprocal_condition >= 0.0);
        CHECK_OR_FAIL(candidate.reciprocal_condition < 1.0e-8);
        destroy_lapack_newton_solver(&solver);
    }

    /* Test 7: problem 1 with row_stride = n + 5: expect ACCEPT with the same x. */
    {
        const int stride7 = p1_n + 5;
        std::vector<LDBLE> q(3 * stride7, 0.0);
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c <= p1_n; ++c)
                q[(size_t)r * stride7 + c] = p1_q[(size_t)r * (p1_n + 1) + c];
        LDBLE signs[3] = { 0.0, 0.0, 0.0 };
        Cl1ProblemView view;
        view.k = 1; view.l = 2; view.m = 0; view.n = p1_n; view.row_stride = stride7;
        view.q = q.data(); view.variable_signs = signs;
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        Cl1ProblemCandidate candidate;
        std::vector<LDBLE> augmented, live_solution;
        evaluate_cl1_problem(solver, view, candidate, augmented, live_solution, 1.0e-8);
        std::printf("test 7: outcome = %d\n", candidate.outcome);
        CHECK_OR_FAIL(candidate.outcome == CL1_PROBLEM_ACCEPT);
        CHECK_OR_FAIL(candidate.solved);
        CHECK_OR_FAIL(x_close(candidate.x, p1_expected_x, p1_n, 1.0e-12));
        destroy_lapack_newton_solver(&solver);
    }

    /* Test 8: componentwise backward-error rejection. The 5x5 system below was captured
       from the ex8 shadow run (the call whose lu_rel_residual_ac was 9.44e-3): row 2 has
       coefficients ~1e-7 and right-hand side ~1e-23, and the solution components in those
       columns are ~1e-16 rounding noise, so the LU solution leaves a componentwise residual
       of ~9.4e-3 on that row while the normwise backward error stays ~1e-17.
       With max_componentwise_error = 1e-12: outcome 12. With 1e-2: ACCEPT. */
    {
        const int n8 = 5;
        LDBLE q[5 * 6] = {
            0.10000000000000002, 0.0, 0.0, 0.10793129605048148, 0.0, -1.3877787807814457e-17,
            -0.10000000000000002, 0.10317718622856517, 1.8683702180207471e-07, -0.013540870438264576, 0.0, -0.0031760606862235529,
            0.0, 0.0, 9.9999999999999995e-08, 2.9533853149357845e-07, 0.0, 1.3234889800848443e-23,
            0.05000000000000001, 0.051588593114282585, 1.844447546109427e-07, -0.89883752359804536, 0.0, -0.0015880303431117793,
            -0.0017000000000000006, -0.0017540121658856081, -1.7000000000000001e-09, -0.003439509245703902, -0.99659995644339061, 5.3993031665812343e-05,
        };
        LDBLE signs[5] = { 0.0, 0.0, 0.0, 0.0, 0.0 };
        Cl1ProblemView view;
        view.k = 0; view.l = 5; view.m = 0; view.n = n8; view.row_stride = n8 + 1;
        view.q = q; view.variable_signs = signs;
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        Cl1ProblemCandidate candidate;
        std::vector<LDBLE> augmented, live_solution;
        evaluate_cl1_problem(solver, view, candidate, augmented, live_solution, 1.0e-12);
        std::printf("test 8: outcome = %d, componentwise = %.6e, backward = %.6e\n",
                    candidate.outcome, candidate.componentwise_error, candidate.backward_error);
        CHECK_OR_FAIL(candidate.solved);
        CHECK_OR_FAIL(candidate.outcome == CL1_PROBLEM_COMPONENTWISE_BACKWARD_ERROR);
        CHECK_OR_FAIL(candidate.componentwise_error > 1.0e-12);
        CHECK_OR_FAIL(candidate.componentwise_error < 1.0e-1);
        CHECK_OR_FAIL(candidate.backward_error <= 1.0e-10);

        evaluate_cl1_problem(solver, view, candidate, augmented, live_solution, 1.0e-2);
        std::printf("test 8b: outcome = %d, componentwise = %.6e\n",
                    candidate.outcome, candidate.componentwise_error);
        CHECK_OR_FAIL(candidate.outcome == CL1_PROBLEM_ACCEPT);
        destroy_lapack_newton_solver(&solver);
    }

    /* Test 9: write_cl1_problem_solution on problem 1 plus the satisfied E row
       (x0 <= 10): x equals the expected solution, res is 0 (within 1e-14) for the A and
       C rows and f - E*x = 9.0 for the E row, and the return value is within 1e-14 of 0. */
    {
        LDBLE q[4 * 4];
        for (size_t t = 0; t < sizeof(p1_q) / sizeof(p1_q[0]); ++t)
            q[t] = p1_q[t];
        q[3 * 4 + 0] = 1.0; q[3 * 4 + 1] = 0.0; q[3 * 4 + 2] = 0.0; q[3 * 4 + 3] = 10.0;
        LDBLE signs[3] = { 0.0, 0.0, 0.0 };
        Cl1ProblemView view;
        view.k = 1; view.l = 2; view.m = 1; view.n = p1_n; view.row_stride = p1_n + 1;
        view.q = q; view.variable_signs = signs;
        LapackNewtonSolver *solver = create_lapack_newton_solver();
        CHECK_OR_FAIL(solver != NULL);
        Cl1ProblemCandidate candidate;
        std::vector<LDBLE> augmented, live_solution;
        evaluate_cl1_problem(solver, view, candidate, augmented, live_solution, 1.0e-8);
        CHECK_OR_FAIL(candidate.outcome == CL1_PROBLEM_ACCEPT);
        LDBLE x_out[3], res_out[4];
        double sum_a = write_cl1_problem_solution(view, candidate, x_out, res_out);
        std::printf("test 9: sum_a = %.17g, res = {%.17g, %.17g, %.17g, %.17g}\n",
                    sum_a, (double)res_out[0], (double)res_out[1], (double)res_out[2], (double)res_out[3]);
        for (int j = 0; j < p1_n; ++j)
            CHECK_OR_FAIL(std::fabs((double)x_out[j] - (double)p1_expected_x[j]) <= 1.0e-12);
        for (int r = 0; r < 3; ++r)
            CHECK_OR_FAIL(std::fabs((double)res_out[r]) <= 1.0e-14);
        CHECK_OR_FAIL(std::fabs((double)res_out[3] - 9.0) <= 1.0e-14);
        CHECK_OR_FAIL(std::fabs(sum_a) <= 1.0e-14);
        destroy_lapack_newton_solver(&solver);
    }

    /* Test 10: cl1_problem_steer on the not-square problem 5: returns 0 and leaves
       delta1, res, l_kode, l_iter and l_error unchanged. */
    {
        LDBLE q[2 * 4] = {
            1.0, 2.0, 0.0,  -1.0,
            0.0, 3.0, 4.0,   5.0,
        };
        setenv("PHREEQC_LINEAR_SOLVER_INTERCEPT", "cl1_problem", 1);
        NewtonLinearSolverState *state = create_newton_linear_solver_state();
        unsetenv("PHREEQC_LINEAR_SOLVER_INTERCEPT");
        CHECK_OR_FAIL(state != NULL);
        CHECK_OR_FAIL(cl1_problem_steering_active(state));
        LDBLE delta1[3] = { -7.0, -8.0, -9.0 };
        LDBLE res[3] = { -1.0, -2.0, -3.0 };
        int l_kode = 42, l_iter = 7;
        LDBLE l_error = 3.5;
        int accepted = cl1_problem_steer(state, 0, 0, 0, 0, 1, 1, 0, p1_n, p1_n + 1,
                                         q, delta1, res, &l_kode, &l_iter, &l_error);
        std::printf("test 10: accepted = %d, l_kode = %d\n", accepted, l_kode);
        CHECK_OR_FAIL(accepted == 0);
        for (int j = 0; j < p1_n; ++j) {
            CHECK_OR_FAIL(delta1[j] == -7.0 - (LDBLE)j);
            CHECK_OR_FAIL(res[j] == -1.0 - (LDBLE)j);
        }
        CHECK_OR_FAIL(l_kode == 42 && l_iter == 7 && l_error == 3.5);
        destroy_newton_linear_solver_state(&state);
    }

    std::printf("test_cl1_problem_check: all tests passed\n");
    return 0;
}
