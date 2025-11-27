/*
 * Simple C Testing Framework for pmtr
 * Provides basic test assertions and reporting
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

/* Test result structure */
typedef struct {
    int total;
    int passed;
    int failed;
    int skipped;
} test_results_t;

/* Global test state */
static test_results_t g_test_results = {0, 0, 0, 0};
static const char *g_current_test = NULL;
static int g_test_failed = 0;
static jmp_buf g_test_jmp;

/* ANSI color codes for output */
#define COLOR_RED     "\033[0;31m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_YELLOW  "\033[0;33m"
#define COLOR_BLUE    "\033[0;34m"
#define COLOR_RESET   "\033[0m"

/* Assertion macros */
#define TEST_ASSERT(condition) do { \
    if (!(condition)) { \
        printf("  " COLOR_RED "FAIL" COLOR_RESET ": %s:%d: assertion failed: %s\n", \
               __FILE__, __LINE__, #condition); \
        g_test_failed = 1; \
        longjmp(g_test_jmp, 1); \
    } \
} while(0)

#define TEST_ASSERT_MSG(condition, msg) do { \
    if (!(condition)) { \
        printf("  " COLOR_RED "FAIL" COLOR_RESET ": %s:%d: %s\n", \
               __FILE__, __LINE__, msg); \
        g_test_failed = 1; \
        longjmp(g_test_jmp, 1); \
    } \
} while(0)

#define TEST_ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        printf("  " COLOR_RED "FAIL" COLOR_RESET ": %s:%d: expected %d, got %d\n", \
               __FILE__, __LINE__, (int)(expected), (int)(actual)); \
        g_test_failed = 1; \
        longjmp(g_test_jmp, 1); \
    } \
} while(0)

#define TEST_ASSERT_EQ_LONG(expected, actual) do { \
    if ((expected) != (actual)) { \
        printf("  " COLOR_RED "FAIL" COLOR_RESET ": %s:%d: expected %ld, got %ld\n", \
               __FILE__, __LINE__, (long)(expected), (long)(actual)); \
        g_test_failed = 1; \
        longjmp(g_test_jmp, 1); \
    } \
} while(0)

#define TEST_ASSERT_STR_EQ(expected, actual) do { \
    if (strcmp((expected), (actual)) != 0) { \
        printf("  " COLOR_RED "FAIL" COLOR_RESET ": %s:%d: expected \"%s\", got \"%s\"\n", \
               __FILE__, __LINE__, (expected), (actual)); \
        g_test_failed = 1; \
        longjmp(g_test_jmp, 1); \
    } \
} while(0)

#define TEST_ASSERT_NULL(ptr) do { \
    if ((ptr) != NULL) { \
        printf("  " COLOR_RED "FAIL" COLOR_RESET ": %s:%d: expected NULL, got %p\n", \
               __FILE__, __LINE__, (void*)(ptr)); \
        g_test_failed = 1; \
        longjmp(g_test_jmp, 1); \
    } \
} while(0)

#define TEST_ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        printf("  " COLOR_RED "FAIL" COLOR_RESET ": %s:%d: expected non-NULL\n", \
               __FILE__, __LINE__); \
        g_test_failed = 1; \
        longjmp(g_test_jmp, 1); \
    } \
} while(0)

#define TEST_ASSERT_TRUE(condition) TEST_ASSERT(condition)
#define TEST_ASSERT_FALSE(condition) TEST_ASSERT(!(condition))

/* Test definition macros */
#define TEST_CASE(name) static void test_##name(void)

#define RUN_TEST(name) do { \
    g_test_results.total++; \
    g_current_test = #name; \
    g_test_failed = 0; \
    printf("  Running: %s... ", #name); \
    fflush(stdout); \
    if (setjmp(g_test_jmp) == 0) { \
        test_##name(); \
    } \
    if (g_test_failed) { \
        g_test_results.failed++; \
    } else { \
        g_test_results.passed++; \
        printf(COLOR_GREEN "PASS" COLOR_RESET "\n"); \
    } \
} while(0)

#define SKIP_TEST(name, reason) do { \
    g_test_results.total++; \
    g_test_results.skipped++; \
    printf("  Skipping: %s - " COLOR_YELLOW "%s" COLOR_RESET "\n", #name, reason); \
} while(0)

/* Test suite macros */
#define TEST_SUITE_BEGIN(name) \
    printf("\n" COLOR_BLUE "=== Test Suite: %s ===" COLOR_RESET "\n", name)

#define TEST_SUITE_END() do { \
    printf("\n"); \
} while(0)

/* Print final results */
static inline void print_test_results(void) {
    printf("\n" COLOR_BLUE "=== Test Results ===" COLOR_RESET "\n");
    printf("Total:   %d\n", g_test_results.total);
    printf("Passed:  " COLOR_GREEN "%d" COLOR_RESET "\n", g_test_results.passed);
    printf("Failed:  " COLOR_RED "%d" COLOR_RESET "\n", g_test_results.failed);
    printf("Skipped: " COLOR_YELLOW "%d" COLOR_RESET "\n", g_test_results.skipped);
    printf("\n");

    if (g_test_results.failed == 0) {
        printf(COLOR_GREEN "All tests passed!" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "%d test(s) failed!" COLOR_RESET "\n", g_test_results.failed);
    }
}

/* Get exit code based on results */
static inline int get_test_exit_code(void) {
    return g_test_results.failed > 0 ? 1 : 0;
}

/* Reset test results (useful for multiple test suites) */
static inline void reset_test_results(void) {
    g_test_results.total = 0;
    g_test_results.passed = 0;
    g_test_results.failed = 0;
    g_test_results.skipped = 0;
}

#endif /* TEST_FRAMEWORK_H */
