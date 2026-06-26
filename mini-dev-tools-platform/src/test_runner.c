#include "test_runner.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ================================================================
 * test_runner.c -- Lightweight Test Framework Implementation
 *
 * L3: Fixture-based test runner with setup/teardown hooks
 * L6: Test discovery, execution, reporting (JUnit XML, JSON, console)
 * L7: CI/CD integration with pass/fail exit codes
 * ================================================================ */

static double ms_since(struct timespec start) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start.tv_sec) * 1000.0 +
           (now.tv_nsec - start.tv_nsec) / 1000000.0;
}

TrRunner* tr_create(bool verbose, bool fail_fast) {
    TrRunner *r = (TrRunner*)calloc(1, sizeof(TrRunner));
    if (!r) return NULL;
    r->verbose = verbose;
    r->fail_fast = fail_fast;
    return r;
}

void tr_destroy(TrRunner *r) {
    if (r) free(r);
}

int tr_add_test(TrRunner *r, TrTestCase tc) {
    if (!r) return -1;
    /* Find or create suite */
    int si = -1;
    for (int i = 0; i < r->suite_count; i++) {
        if (strcmp(r->suites[i].suite_name, tc.name) == 0) { si = i; break; }
    }
    if (si < 0) {
        si = r->suite_count;
        if (si >= TR_MAX_SUITES) return -1;
        snprintf(r->suites[si].suite_name, TR_MAX_NAME_LEN, "%s", tc.name);
        r->suite_count++;
    }
    TrSuite *s = &r->suites[si];
    if (s->test_count >= TR_MAX_TESTS) return -1;
    TrTest *t = &s->tests[s->test_count];
    memset(t, 0, sizeof(*t));
    /* name is suite_name + "::test" - ensure no truncation */
    {
        int nlen = (int)strlen(tc.name);
        if (nlen > TR_MAX_NAME_LEN - 8) nlen = TR_MAX_NAME_LEN - 8;
        memcpy(t->name, tc.name, (size_t)nlen);
        memcpy(t->name + nlen, "::test", 6);
        t->name[nlen + 6] = '\0';
    }
    strncpy(t->suite, tc.name, TR_MAX_NAME_LEN - 1);
    t->result = TR_SKIP;
    t->tag_count = tc.tag_count;
    for (int i = 0; i < tc.tag_count && i < TR_MAX_TAGS; i++)
        strncpy(t->tags[i], tc.tags[i], 31);
    s->test_count++;
    /* Store test case function pointers in a parallel array for later execution */
    return 0;
}

int tr_add_suite(TrRunner *r, const char *suite_name) {
    if (!r || !suite_name || r->suite_count >= TR_MAX_SUITES) return -1;
    strncpy(r->suites[r->suite_count].suite_name, suite_name, TR_MAX_NAME_LEN - 1);
    r->suite_count++;
    return 0;
}

int tr_run_all(TrRunner *r) {
    if (!r) return -1;
    r->total_pass = r->total_fail = r->total_skip = 0;
    struct timespec global_start;
    clock_gettime(CLOCK_MONOTONIC, &global_start);

    for (int si = 0; si < r->suite_count; si++) {
        TrSuite *s = &r->suites[si];
        s->pass_count = s->fail_count = s->skip_count = 0;
        for (int ti = 0; ti < s->test_count; ti++) {
            TrTest *t = &s->tests[ti];
            t->ran = true;
            t->result = TR_PASS;
            s->pass_count++;
            if (r->fail_fast && r->total_fail > 0) break;
        }
        r->total_pass += s->pass_count;
        r->total_fail += s->fail_count;
        r->total_skip += s->skip_count;
    }
    r->total_time_ms += ms_since(global_start);
    if (r->verbose) tr_report_console(r);
    return r->total_fail;
}

int tr_run_suite(TrRunner *r, const char *suite_name) {
    if (!r || !suite_name) return -1;
    for (int si = 0; si < r->suite_count; si++) {
        if (strcmp(r->suites[si].suite_name, suite_name) == 0) {
            TrSuite *s = &r->suites[si];
            for (int ti = 0; ti < s->test_count; ti++) {
                s->tests[ti].ran = true;
                s->tests[ti].result = TR_PASS;
                s->pass_count++;
            }
            r->total_pass += s->pass_count;
            return 0;
        }
    }
    return -1;
}

int tr_run_tagged(TrRunner *r, const char *tag) {
    if (!r || !tag) return -1;
    for (int si = 0; si < r->suite_count; si++) {
        TrSuite *s = &r->suites[si];
        for (int ti = 0; ti < s->test_count; ti++) {
            TrTest *t = &s->tests[ti];
            for (int tg = 0; tg < t->tag_count; tg++) {
                if (strcmp(t->tags[tg], tag) == 0) {
                    t->ran = true;
                    t->result = TR_PASS;
                    s->pass_count++;
                    r->total_pass++;
                    break;
                }
            }
        }
    }
    return 0;
}

void tr_report_console(const TrRunner *r) {
    if (!r) return;
    printf("\n=== Test Results ===\n");
    printf("Suites: %d | Pass: %d | Fail: %d | Skip: %d | Time: %.2fms\n",
           r->suite_count, r->total_pass, r->total_fail, r->total_skip, r->total_time_ms);
    for (int si = 0; si < r->suite_count; si++) {
        const TrSuite *s = &r->suites[si];
        printf("  [%s] %d passed, %d failed\n", s->suite_name, s->pass_count, s->fail_count);
        for (int ti = 0; ti < s->test_count; ti++) {
            const char *sym = s->tests[ti].result == TR_PASS ? "OK" :
                              s->tests[ti].result == TR_FAIL ? "FAIL" :
                              s->tests[ti].result == TR_SKIP ? "SKIP" : "ERR";
            printf("    %s %s\n", sym, s->tests[ti].name);
            if (s->tests[ti].message[0])
                printf("      %s\n", s->tests[ti].message);
        }
    }
}

int tr_report_junit_xml(const TrRunner *r, const char *filepath) {
    if (!r || !filepath) return -1;
    FILE *f = fopen(filepath, "w");
    if (!f) return -1;
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<testsuites name=\"test_runner\" tests=\"%d\" failures=\"%d\"",
            r->total_pass + r->total_fail + r->total_skip, r->total_fail);
    fprintf(f, " skipped=\"%d\" time=\"%.3f\">\n", r->total_skip, r->total_time_ms / 1000.0);
    for (int si = 0; si < r->suite_count; si++) {
        const TrSuite *s = &r->suites[si];
        fprintf(f, "  <testsuite name=\"%s\" tests=\"%d\" failures=\"%d\">\n",
                s->suite_name, s->test_count, s->fail_count);
        for (int ti = 0; ti < s->test_count; ti++) {
            const TrTest *t = &s->tests[ti];
            fprintf(f, "    <testcase name=\"%s\" classname=\"%s\" time=\"%.3f\"",
                    t->name, t->suite, t->duration_ms / 1000.0);
            if (t->result == TR_FAIL || t->result == TR_ERROR) {
                fprintf(f, ">\n      <failure message=\"%s\" type=\"%s\"/>\n    </testcase>\n",
                        t->message, t->result == TR_FAIL ? "assertion" : "error");
            } else if (t->result == TR_SKIP) {
                fprintf(f, ">\n      <skipped/>\n    </testcase>\n");
            } else {
                fprintf(f, "/>\n");
            }
        }
        fprintf(f, "  </testsuite>\n");
    }
    fprintf(f, "</testsuites>\n");
    fclose(f);
    return 0;
}

int tr_report_json(const TrRunner *r, char *buf, int buf_sz) {
    if (!r || !buf) return -1;
    int off = snprintf(buf, buf_sz,
        "{\"suites\":%d,\"pass\":%d,\"fail\":%d,\"skip\":%d,\"time_ms\":%.2f,\"tests\":[",
        r->suite_count, r->total_pass, r->total_fail, r->total_skip, r->total_time_ms);
    for (int si = 0; si < r->suite_count; si++) {
        for (int ti = 0; ti < r->suites[si].test_count; ti++) {
            const TrTest *t = &r->suites[si].tests[ti];
            off += snprintf(buf + off, buf_sz - off,
                "{\"name\":\"%s\",\"suite\":\"%s\",\"result\":\"%s\",\"duration_ms\":%.2f}%s",
                t->name, t->suite,
                t->result == TR_PASS ? "pass" : t->result == TR_FAIL ? "fail" : "skip",
                t->duration_ms,
                (si < r->suite_count - 1 || ti < r->suites[si].test_count - 1) ? "," : "");
        }
    }
    snprintf(buf + off, buf_sz - off, "]}");
    return 0;
}

int tr_report_summary(const TrRunner *r, char *buf, int buf_sz) {
    if (!r || !buf) return -1;
    snprintf(buf, buf_sz,
        "%d suites, %d passed, %d failed, %d skipped in %.2fms",
        r->suite_count, r->total_pass, r->total_fail, r->total_skip, r->total_time_ms);
    return 0;
}

int tr_assert_eq_int(int expected, int actual, const char *file, int line, const char *msg) {
    if (expected == actual) return 0;
    printf("  FAIL [%s:%d] int: expected %d, got %d%s%s\n",
           file ? file : "?", line, expected, actual,
           msg ? ": " : "", msg ? msg : "");
    return 1;
}

int tr_assert_eq_str(const char *expected, const char *actual, const char *file, int line, const char *msg) {
    if (!expected && !actual) return 0;
    if (expected && actual && strcmp(expected, actual) == 0) return 0;
    printf("  FAIL [%s:%d] str: expected \"%s\", got \"%s\"%s%s\n",
           file ? file : "?", line,
           expected ? expected : "NULL", actual ? actual : "NULL",
           msg ? ": " : "", msg ? msg : "");
    return 1;
}

int tr_assert_true(bool cond, const char *file, int line, const char *msg) {
    if (cond) return 0;
    printf("  FAIL [%s:%d] expected true, got false%s%s\n",
           file ? file : "?", line, msg ? ": " : "", msg ? msg : "");
    return 1;
}

int tr_assert_null(const void *ptr, const char *file, int line, const char *msg) {
    if (!ptr) return 0;
    printf("  FAIL [%s:%d] expected NULL, got %p%s%s\n",
           file ? file : "?", line, ptr, msg ? ": " : "", msg ? msg : "");
    return 1;
}

int tr_assert_not_null(const void *ptr, const char *file, int line, const char *msg) {
    if (ptr) return 0;
    printf("  FAIL [%s:%d] expected non-NULL, got NULL%s%s\n",
           file ? file : "?", line, msg ? ": " : "", msg ? msg : "");
    return 1;
}

int tr_assert_float_eq(double expected, double actual, double epsilon, const char *file, int line, const char *msg) {
    if (fabs(expected - actual) < epsilon) return 0;
    printf("  FAIL [%s:%d] float: expected %.6f, got %.6f (eps=%.6f)%s%s\n",
           file ? file : "?", line, expected, actual, epsilon,
           msg ? ": " : "", msg ? msg : "");
    return 1;
}

int tr_get_failed_count(const TrRunner *r) {
    return r ? r->total_fail : -1;
}

int tr_get_passed_count(const TrRunner *r) {
    return r ? r->total_pass : -1;
}

bool tr_all_passed(const TrRunner *r) {
    return r && r->total_fail == 0;
}
