#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/**
 * test_runner.h -- Lightweight Test Framework
 *
 * L3: Fixture-based test runner with setup/teardown hooks
 * L6: Test discovery, execution, and reporting
 * L7: CI/CD integration (JUnit XML output)
 *
 * Design inspired by xUnit family (JUnit, NUnit, CUnit)
 * Reference: Meszaros, "xUnit Test Patterns" (2007)
 */

#define TR_MAX_TESTS      256
#define TR_MAX_SUITES      32
#define TR_MAX_NAME_LEN   128
#define TR_MAX_MSG_LEN    1024
#define TR_MAX_TAGS         8

typedef enum {
    TR_PASS,
    TR_FAIL,
    TR_SKIP,
    TR_ERROR,
    TR_TIMEOUT
} TrResult;

typedef struct {
    char name[TR_MAX_NAME_LEN];
    char suite[TR_MAX_NAME_LEN];
    TrResult result;
    char message[TR_MAX_MSG_LEN];
    char file[TR_MAX_NAME_LEN];
    int  line;
    double duration_ms;
    bool ran;
    char tags[TR_MAX_TAGS][32];
    int  tag_count;
} TrTest;

typedef int  (*TrTestFunc)(void);
typedef void (*TrSetupFunc)(void);
typedef void (*TrTeardownFunc)(void);

typedef struct {
    char name[TR_MAX_NAME_LEN];
    TrTestFunc test;
    TrSetupFunc setup;
    TrTeardownFunc teardown;
    char tags[TR_MAX_TAGS][32];
    int  tag_count;
    int  timeout_seconds;
} TrTestCase;

typedef struct {
    TrTest tests[TR_MAX_TESTS];
    int    test_count;
    int    pass_count;
    int    fail_count;
    int    skip_count;
    double total_time_ms;
    char   suite_name[TR_MAX_NAME_LEN];
} TrSuite;

typedef struct {
    TrSuite suites[TR_MAX_SUITES];
    int     suite_count;
    int     total_pass;
    int     total_fail;
    int     total_skip;
    double  total_time_ms;
    bool    fail_fast;
    bool    verbose;
} TrRunner;

TrRunner* tr_create(bool verbose, bool fail_fast);
void      tr_destroy(TrRunner *r);
int       tr_add_test(TrRunner *r, TrTestCase tc);
int       tr_add_suite(TrRunner *r, const char *suite_name);
int       tr_run_all(TrRunner *r);
int       tr_run_suite(TrRunner *r, const char *suite_name);
int       tr_run_tagged(TrRunner *r, const char *tag);

/* L6: Report generation */
void      tr_report_console(const TrRunner *r);
int       tr_report_junit_xml(const TrRunner *r, const char *filepath);
int       tr_report_json(const TrRunner *r, char *buf, int buf_sz);
int       tr_report_summary(const TrRunner *r, char *buf, int buf_sz);

/* L3: Assertion macros (implemented as functions for C compatibility) */
int  tr_assert_eq_int(int expected, int actual, const char *file, int line, const char *msg);
int  tr_assert_eq_str(const char *expected, const char *actual, const char *file, int line, const char *msg);
int  tr_assert_true(bool cond, const char *file, int line, const char *msg);
int  tr_assert_null(const void *ptr, const char *file, int line, const char *msg);
int  tr_assert_not_null(const void *ptr, const char *file, int line, const char *msg);
int  tr_assert_float_eq(double expected, double actual, double epsilon, const char *file, int line, const char *msg);

/* L7: CI integration */
int  tr_get_failed_count(const TrRunner *r);
int  tr_get_passed_count(const TrRunner *r);
bool tr_all_passed(const TrRunner *r);

#endif
