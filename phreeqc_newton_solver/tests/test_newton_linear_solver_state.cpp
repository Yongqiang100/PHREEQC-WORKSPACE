/* Phase 2 Task 3 test: process-wide statistics and comparison-log path expansion in
   newton_linear_solver_state.cpp. Built only when the LAPACK backend is available
   (PHREEQC_USE_LAPACK_SOLVER): it drives the adapter through the LAPACK backend and
   checks the process-summary line and the expanded comparison-log paths. */
#include "newton_linear_solver_state.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

#define CHECK_OR_FAIL(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "FAILED line %d: %s\n", __LINE__, #condition); \
            return 1; \
        } \
    } while (0)

/* Generated well-conditioned tridiagonal system (same recipe as the other tests):
   (row,row)=4, (row,row+1)=1, (row+1,row)=2; solution (row%3)-1. Row-major,
   leading dimension n+1, RHS in column n. n stays far below the GATE 1e-8 limit,
   so every solve is accepted. */
static const int generated_size_max = 60;

static void build_generated_system(int n, LDBLE *storage)
{
    for (int i = 0; i < n * (n + 1); i++) storage[i] = 0.0;
    for (int row = 0; row < n; row++) {
        storage[row * (n + 1) + row] = 4.0;
        if (row + 1 < n) {
            storage[row * (n + 1) + row + 1] = 1.0;
            storage[(row + 1) * (n + 1) + row] = 2.0;
        }
    }
    LDBLE expected[generated_size_max];
    for (int row = 0; row < n; row++) expected[row] = (LDBLE)((row % 3) - 1);
    for (int row = 0; row < n; row++) {
        LDBLE rhs = 0.0;
        for (int column = 0; column < n; column++) rhs += storage[row * (n + 1) + column] * expected[column];
        storage[row * (n + 1) + n] = rhs;
    }
}

/* One accepted solve of a generated system through the adapter. */
static int solve_generated_through_state(NewtonLinearSolverState *state, int n)
{
    LDBLE storage[generated_size_max * (generated_size_max + 1)];
    LDBLE output[generated_size_max];
    build_generated_system(n, storage);
    LinearSolveStatus status = solve_newton_step_with_backend(state, storage, n, output);
    CHECK_OR_FAIL(status == LINEAR_SOLVE_ACCEPTED);
    return 0;
}

/* Redirect stderr to a temporary file, then read it back. */
static int begin_stderr_capture(int *saved, char *temp_name)
{
    std::strcpy(temp_name, "t3_state_stderr.XXXXXX");
    int fd = mkstemp(temp_name);
    if (fd < 0) return 1;
    *saved = dup(fileno(stderr));
    if (*saved < 0) { close(fd); return 1; }
    if (dup2(fd, fileno(stderr)) < 0) { close(fd); close(*saved); return 1; }
    close(fd);
    return 0;
}

static std::string end_stderr_capture(int saved, const char *temp_name)
{
    std::fflush(stderr);
    dup2(saved, fileno(stderr));
    close(saved);
    std::string content;
    FILE *f = std::fopen(temp_name, "r");
    if (f != NULL) {
        char buffer[4096];
        size_t count;
        while ((count = std::fread(buffer, 1, sizeof(buffer), f)) > 0) content.append(buffer, count);
        std::fclose(f);
    }
    unlink(temp_name);
    return content;
}

/* Returns true only when content holds exactly one process-summary line. */
static bool extract_single_summary_line(const std::string &content, std::string *line)
{
    std::istringstream stream(content);
    std::string text, found;
    int count = 0;
    while (std::getline(stream, text)) {
        if (text.find("phreeqc_linear_solver_process_summary:") != std::string::npos) {
            ++count;
            found = text;
        }
    }
    if (count != 1) return false;
    *line = found;
    return true;
}

static bool get_summary_field(const std::string &line, const char *key, unsigned long *out)
{
    std::istringstream stream(line);
    std::string token, prefix(key);
    prefix += "=";
    while (stream >> token) {
        if (token.compare(0, prefix.size(), prefix) == 0) {
            *out = std::strtoul(token.c_str() + prefix.size(), NULL, 10);
            return true;
        }
    }
    return false;
}

/* Test 1: three objects, one solve each, all destroyed -> one summary line, segment 0. */
static int test_one_summary_after_three_objects(unsigned long expected_segment)
{
    NewtonLinearSolverState *states[3] = {NULL, NULL, NULL};
    for (int k = 0; k < 3; k++) {
        states[k] = create_newton_linear_solver_state();
        CHECK_OR_FAIL(states[k] != NULL);
        CHECK_OR_FAIL(states[k]->backend == NEWTON_BACKEND_LAPACK);
    }
    for (int k = 0; k < 3; k++)
        CHECK_OR_FAIL(solve_generated_through_state(states[k], 3) == 0);

    int saved = -1;
    char temp_name[64];
    CHECK_OR_FAIL(begin_stderr_capture(&saved, temp_name) == 0);
    for (int k = 0; k < 3; k++) destroy_newton_linear_solver_state(&states[k]);
    std::string captured = end_stderr_capture(saved, temp_name);
    for (int k = 0; k < 3; k++) CHECK_OR_FAIL(states[k] == NULL);

    std::string line;
    CHECK_OR_FAIL(extract_single_summary_line(captured, &line));
    unsigned long v;
    CHECK_OR_FAIL(get_summary_field(line, "segment", &v) && v == expected_segment);
    CHECK_OR_FAIL(get_summary_field(line, "objects", &v) && v == 3);
    CHECK_OR_FAIL(get_summary_field(line, "backend_lapack_objects", &v) && v == 3);
    CHECK_OR_FAIL(get_summary_field(line, "solves", &v) && v == 3);
    CHECK_OR_FAIL(get_summary_field(line, "accepted", &v) && v == 3);
    return 0;
}

/* Test 2: one object, two solves, destroyed -> one summary line, the next segment. */
static int test_second_segment_after_one_object(unsigned long expected_segment)
{
    NewtonLinearSolverState *state = create_newton_linear_solver_state();
    CHECK_OR_FAIL(state != NULL);
    CHECK_OR_FAIL(solve_generated_through_state(state, 3) == 0);
    CHECK_OR_FAIL(solve_generated_through_state(state, 4) == 0);

    int saved = -1;
    char temp_name[64];
    CHECK_OR_FAIL(begin_stderr_capture(&saved, temp_name) == 0);
    destroy_newton_linear_solver_state(&state);
    std::string captured = end_stderr_capture(saved, temp_name);
    CHECK_OR_FAIL(state == NULL);

    std::string line;
    CHECK_OR_FAIL(extract_single_summary_line(captured, &line));
    unsigned long v;
    CHECK_OR_FAIL(get_summary_field(line, "segment", &v) && v == expected_segment);
    CHECK_OR_FAIL(get_summary_field(line, "objects", &v) && v == 1);
    CHECK_OR_FAIL(get_summary_field(line, "solves", &v) && v == 2);
    CHECK_OR_FAIL(get_summary_field(line, "accepted", &v) && v == 2);
    return 0;
}

/* Test 3: comparison-log path expansion. Two objects with pattern cmp_%o.log produce
   two header-prefixed log files named by their object indices. */
static int test_comparison_log_path_expansion(unsigned long expected_segment,
                                              unsigned long expected_object_index_first)
{
    char temp_dir[] = "t3_cmp_logs.XXXXXX";
    CHECK_OR_FAIL(mkdtemp(temp_dir) != NULL);
    std::string pattern = std::string(temp_dir) + "/cmp_%o.log";
    setenv("PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1", "1", 1);
    setenv("PHREEQC_LINEAR_SOLVER_COMPARE_LOG", pattern.c_str(), 1);

    NewtonLinearSolverState *states[2] = {NULL, NULL};
    for (int k = 0; k < 2; k++) {
        states[k] = create_newton_linear_solver_state();
        CHECK_OR_FAIL(states[k] != NULL);
        CHECK_OR_FAIL(states[k]->comparison_log_file != NULL);
    }
    int saved = -1;
    char temp_name[64];
    CHECK_OR_FAIL(begin_stderr_capture(&saved, temp_name) == 0);
    for (int k = 0; k < 2; k++) destroy_newton_linear_solver_state(&states[k]);
    std::string captured = end_stderr_capture(saved, temp_name);
    unsetenv("PHREEQC_LINEAR_SOLVER_COMPARE_WITH_CL1");
    unsetenv("PHREEQC_LINEAR_SOLVER_COMPARE_LOG");

    for (int k = 0; k < 2; k++) {
        unsigned long idx = expected_object_index_first + (unsigned long)k;
        std::string path = std::string(temp_dir) + "/cmp_" + std::to_string(idx) + ".log";
        FILE *f = std::fopen(path.c_str(), "r");
        CHECK_OR_FAIL(f != NULL);
        char first[512];
        CHECK_OR_FAIL(std::fgets(first, sizeof(first), f) != NULL);
        CHECK_OR_FAIL(std::strstr(first, "# call_index system_size") == first);
        std::fclose(f);
        unlink(path.c_str());
    }
    rmdir(temp_dir);

    std::string line;
    CHECK_OR_FAIL(extract_single_summary_line(captured, &line));
    unsigned long v;
    CHECK_OR_FAIL(get_summary_field(line, "segment", &v) && v == expected_segment);
    CHECK_OR_FAIL(get_summary_field(line, "objects", &v) && v == 2);
    return 0;
}

/* Test 4: four threads, each with its own object. A barrier keeps all four objects
   live at once, so exactly one summary line is printed after the last destroy. */
static std::atomic<int> t4_ready_count;
static std::atomic<int> t4_ok;

static void t4_worker(void)
{
    int ok = 1;
    NewtonLinearSolverState *state = create_newton_linear_solver_state();
    if (state == NULL) { t4_ok.store(0); return; }
    if (state->backend != NEWTON_BACKEND_LAPACK) ok = 0;

    t4_ready_count.fetch_add(1);
    while (t4_ready_count.load() < 4) std::this_thread::yield();

    LDBLE storage[generated_size_max * (generated_size_max + 1)];
    LDBLE output[generated_size_max];
    for (int iteration = 0; iteration < 100 && ok; iteration++) {
        for (int n = 5; n <= 20 && ok; n++) {
            build_generated_system(n, storage);
            if (solve_newton_step_with_backend(state, storage, n, output) != LINEAR_SOLVE_ACCEPTED) ok = 0;
        }
    }
    destroy_newton_linear_solver_state(&state);
    if (state != NULL) ok = 0;
    if (!ok) t4_ok.store(0);
}

static int test_concurrent_objects_one_summary(unsigned long expected_segment)
{
    t4_ready_count.store(0);
    t4_ok.store(1);

    int saved = -1;
    char temp_name[64];
    CHECK_OR_FAIL(begin_stderr_capture(&saved, temp_name) == 0);
    std::thread workers[4];
    for (int t = 0; t < 4; t++) workers[t] = std::thread(t4_worker);
    for (int t = 0; t < 4; t++) workers[t].join();
    std::string captured = end_stderr_capture(saved, temp_name);
    CHECK_OR_FAIL(t4_ok.load() == 1);

    std::string line;
    CHECK_OR_FAIL(extract_single_summary_line(captured, &line));
    unsigned long v;
    CHECK_OR_FAIL(get_summary_field(line, "segment", &v) && v == expected_segment);
    CHECK_OR_FAIL(get_summary_field(line, "objects", &v) && v == 4);
    CHECK_OR_FAIL(get_summary_field(line, "solves", &v) && v == 4 * 100 * 16);
    CHECK_OR_FAIL(get_summary_field(line, "accepted", &v) && v == 4 * 100 * 16);
    return 0;
}

int main(void)
{
    setenv("PHREEQC_LINEAR_SOLVER", "lapack", 1);
    setenv("PHREEQC_LINEAR_SOLVER_STATISTICS", "summary", 1);

    unsigned long expected_segment = 0;
    unsigned long next_object_index = 0;

    CHECK_OR_FAIL(test_one_summary_after_three_objects(expected_segment) == 0);
    expected_segment++;
    next_object_index += 3;

    CHECK_OR_FAIL(test_second_segment_after_one_object(expected_segment) == 0);
    expected_segment++;
    next_object_index += 1;

    CHECK_OR_FAIL(test_comparison_log_path_expansion(expected_segment, next_object_index) == 0);
    expected_segment++;
    next_object_index += 2;

    CHECK_OR_FAIL(test_concurrent_objects_one_summary(expected_segment) == 0);

    std::printf("PASS test_newton_linear_solver_state\n");
    return 0;
}
