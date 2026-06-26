#include "logger.h"
#include "config_mgr.h"
#include "cli_parser.h"
#include "profiler.h"
#include "feature_flags.h"
#include "code_review.h"
#include "deploy_manager.h"
#include "package_manager.h"
#include "test_runner.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(n) do { tests_run++; printf("  TEST %s ... ", n); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(m) do { printf("FAIL: %s\n", m); return 1; } while(0)
#define CHECK(c, m) if (!(c)) FAIL(m)

/* ---- Logger Tests ---- */

static int test_logger_create(void) {
    TEST("logger_create");
    Logger *l = logger_create();
    CHECK(l != NULL, "create failed");
    CHECK(logger_entry_count(l) == 0, "init count");
    logger_destroy(l);
    PASS(); return 0;
}

static int test_logger_add_sink(void) {
    TEST("logger_add_sink");
    Logger *l = logger_create();
    CHECK(logger_add_sink(l, SINK_CONSOLE, "", LOG_INFO) >= 0, "sink failed");
    CHECK(logger_add_sink(l, SINK_RINGBUF, "ring", LOG_DEBUG) >= 0, "ringbuf failed");
    CHECK(logger_sink_count_by_type(l, SINK_CONSOLE) == 1, "sink count");
    logger_destroy(l);
    PASS(); return 0;
}

static int test_logger_parse_level(void) {
    TEST("logger_parse_level");
    CHECK(logger_parse_level("debug") == LOG_DEBUG, "debug level");
    CHECK(logger_parse_level("INFO") == LOG_INFO, "info level");
    CHECK(logger_parse_level("fatal") == LOG_FATAL, "fatal level");
    CHECK(logger_parse_level("unknown") == LOG_INFO, "unknown defaults");
    PASS(); return 0;
}

/* ---- Config Tests ---- */

static int test_config_set_get(void) {
    TEST("config_set_get");
    ConfigManager *cm = config_create("test.cfg", false);
    CHECK(config_set(cm, "key1", "val1", CFG_STRING, SOURCE_FILE) >= 0, "set fail");
    CHECK(strcmp(config_get(cm, "key1", ""), "val1") == 0, "get fail");
    CHECK(config_get_int(cm, "num", 42) == 42, "default int");
    config_destroy(cm);
    PASS(); return 0;
}

static int test_config_types(void) {
    TEST("config_types");
    ConfigManager *cm = config_create(NULL, false);
    CHECK(config_set(cm, "max_conn", "100", CFG_INT, SOURCE_FILE) >= 0, "set int");
    CHECK(config_get_int(cm, "max_conn", 0) == 100, "get int");
    CHECK(config_get_bool(cm, "enabled", false) == false, "missing bool");
    config_destroy(cm);
    PASS(); return 0;
}

/* ---- CLI Parser Tests ---- */

static int test_cli_basic(void) {
    TEST("cli_basic");
    CLIParser *cp = cli_create("mytool", "A test tool");
    CHECK(cp != NULL, "create");
    CHECK(cli_add_flag(&cp->root, "verbose", 'v', FLAG_BOOL, "Verbose output") >= 0, "add bool");
    CHECK(cli_add_flag(&cp->root, "output", 'o', FLAG_STRING, "Output file") >= 0, "add str");
    cli_destroy(cp);
    PASS(); return 0;
}

/* ---- Profiler Tests ---- */

static int test_profiler_zone(void) {
    TEST("profiler_zone");
    Profiler *p = profiler_create();
    int zid = profiler_begin_zone(p, "test", PROF_CPU);
    CHECK(zid >= 0, "begin_zone failed");
    CHECK(profiler_end_zone(p, zid) == 0, "end_zone failed");
    CHECK(profiler_zone_call_count(p, zid) == 1, "call count");
    profiler_reset(p);
    CHECK(profiler_zone_call_count(p, zid) == 0, "reset count");
    profiler_destroy(p);
    PASS(); return 0;
}

static int test_profiler_memory(void) {
    TEST("profiler_memory");
    Profiler *p = profiler_create();
    profiler_record_allocation(p, 1024);
    profiler_record_allocation(p, 2048);
    CHECK(profiler_memory_stats(p).total_allocated == 3072, "total alloc");
    profiler_record_free(p, 1024);
    CHECK(profiler_memory_stats(p).current_usage == 2048, "current usage");
    profiler_destroy(p);
    PASS(); return 0;
}

/* ---- Feature Flag Tests ---- */

static int test_feature_flag_basic(void) {
    TEST("feature_flag_basic");
    FeatureFlagSystem *ffs = ff_system_create("staging");
    CHECK(ff_define_flag(ffs, "new-ui", "New UI feature", FLAG_BOOLEAN) >= 0, "define fail");
    CHECK(ff_set_state(ffs, "new-ui", FLAG_ON) == 0, "set fail");
    CHECK(ff_is_enabled(ffs, "new-ui", NULL, 0) == true, "not on");
    ff_system_destroy(ffs);
    PASS(); return 0;
}

static int test_feature_flag_percentage(void) {
    TEST("feature_flag_percentage");
    FeatureFlagSystem *ffs = ff_system_create("prod");
    ff_define_flag(ffs, "beta", "Beta feature", FLAG_PERCENTAGE);
    ff_set_rollout(ffs, "beta", 50.0);
    /* Just verify no crash on evaluation */
    ff_is_enabled(ffs, "beta", NULL, 0);
    ff_system_destroy(ffs);
    PASS(); return 0;
}

/* ---- Code Review Tests ---- */

static int test_code_review_create(void) {
    TEST("code_review_create");
    CodeReview *cr = review_create("Feature X", "alice", "feat/x");
    CHECK(cr != NULL, "create failed");
    CHECK(review_add_reviewer(cr, "bob") >= 0, "reviewer fail");
    CHECK(review_add_file(cr, "src/main.c") >= 0, "add file");
    int total, resolved, unresolved;
    review_stats(cr, &total, &resolved, &unresolved);
    CHECK(total == 0, "no comments yet");
    review_destroy(cr);
    PASS(); return 0;
}

static int test_code_review_flow(void) {
    TEST("code_review_flow");
    CodeReview *cr = review_create("Bugfix", "carol", "fix/bug");
    review_add_comment(cr, "dave", "Use NULL check here", "src/foo.c", 42);
    review_resolve_comment(cr, 0, "carol");
    CHECK(review_approve(cr) == 0, "approve");
    review_destroy(cr);
    PASS(); return 0;
}

/* ---- Deploy Tests ---- */

static int test_deploy_basic(void) {
    TEST("deploy_basic");
    DeployManager *dm = deploy_create("web", "v2", DEPLOY_STAGING, "ops");
    CHECK(dm != NULL, "create failed");
    deploy_add_step(dm, STEP_DEPLOY, "deploy", 60, true);
    CHECK(deploy_execute_all(dm) == 1, "execute fail");
    CHECK(deploy_was_successful(dm) == true, "not successful");
    CHECK(deploy_progress(dm) == 100.0, "progress");
    deploy_destroy(dm);
    PASS(); return 0;
}

static int test_deploy_rollback(void) {
    TEST("deploy_rollback");
    DeployManager *dm = deploy_create("api", "v3", DEPLOY_PRODUCTION, "ci");
    deploy_add_step(dm, STEP_DEPLOY, "push", 30, true);
    deploy_execute_step(dm);
    deploy_rollback(dm);
    CHECK(deploy_is_complete(dm) == true, "complete after rollback");
    deploy_destroy(dm);
    PASS(); return 0;
}

/* ---- Package Manager Tests ---- */

static int test_semver_parse(void) {
    TEST("semver_parse");
    SemVer v = semver_parse("1.2.3-alpha+build.1");
    CHECK(v.major == 1, "major");
    CHECK(v.minor == 2, "minor");
    CHECK(v.patch == 3, "patch");
    CHECK(strcmp(v.prerelease, "alpha") == 0, "prerelease");
    CHECK(strcmp(v.build, "build.1") == 0, "build");
    PASS(); return 0;
}

static int test_semver_compare(void) {
    TEST("semver_compare");
    SemVer a = semver_parse("1.0.0");
    SemVer b = semver_parse("2.0.0");
    SemVer c = semver_parse("1.1.0");
    CHECK(semver_compare(a, b) < 0, "1<2");
    CHECK(semver_compare(b, a) > 0, "2>1");
    CHECK(semver_compare(a, c) < 0, "1.0<1.1");
    CHECK(semver_compare(a, a) == 0, "equal");
    PASS(); return 0;
}

static int test_semver_precedence(void) {
    TEST("semver_precedence");
    SemVer release = semver_parse("1.0.0");
    SemVer alpha   = semver_parse("1.0.0-alpha");
    SemVer beta    = semver_parse("1.0.0-beta");
    CHECK(semver_compare(release, alpha) > 0, "release > alpha");
    CHECK(semver_compare(alpha, beta) < 0, "alpha < beta");
    PASS(); return 0;
}

static int test_pm_registry(void) {
    TEST("pm_registry");
    PackageRegistry *reg = pm_registry_create(64);
    CHECK(reg != NULL, "create");
    Package pkg;
    memset(&pkg, 0, sizeof(pkg));
    strncpy(pkg.name, "libfoo", PM_MAX_NAME_LEN - 1);
    pkg.version = semver_parse("2.1.0");
    CHECK(pm_registry_add(reg, &pkg) == 0, "add");
    Package *found = pm_registry_find(reg, "libfoo");
    CHECK(found != NULL, "find");
    CHECK(semver_compare(found->version, pkg.version) == 0, "version match");
    pm_registry_destroy(reg);
    PASS(); return 0;
}

static int test_pm_install(void) {
    TEST("pm_install");
    PackageRegistry *reg = pm_registry_create(64);
    Package pkg;
    memset(&pkg, 0, sizeof(pkg));
    strncpy(pkg.name, "mylib", PM_MAX_NAME_LEN - 1);
    pkg.version = semver_parse("3.0.0");
    pm_registry_add(reg, &pkg);
    SemVer want = semver_parse("3.0.0");
    CHECK(pm_install(reg, "mylib", want) == 0, "install");
    Package installed[8];
    int cnt = pm_list_installed(reg, installed, 8);
    CHECK(cnt == 1, "installed count");
    pm_registry_destroy(reg);
    PASS(); return 0;
}

/* ---- Test Runner Tests ---- */

static int test_tr_assertions(void) {
    TEST("tr_assertions");
    CHECK(tr_assert_eq_int(5, 5, __FILE__, __LINE__, "eq int") == 0, "int eq");
    CHECK(tr_assert_eq_int(5, 3, __FILE__, __LINE__, "neq") == 1, "int neq");
    CHECK(tr_assert_true(true, __FILE__, __LINE__, "true") == 0, "true");
    CHECK(tr_assert_not_null(&tests_run, __FILE__, __LINE__, "not null") == 0, "not null");
    CHECK(tr_assert_null(NULL, __FILE__, __LINE__, "null") == 0, "null");
    CHECK(tr_assert_float_eq(1.0, 1.0, 0.001, __FILE__, __LINE__, "float") == 0, "float eq");
    PASS(); return 0;
}

static int test_tr_create(void) {
    TEST("tr_create");
    TrRunner *r = tr_create(true, false);
    CHECK(r != NULL, "create");
    CHECK(tr_all_passed(r) == true, "all passed initially");
    tr_destroy(r);
    PASS(); return 0;
}

/* ---- Main ---- */

int main(void) {
    printf("=== mini-dev-tools-platform Tests ===\n\n");
    int failed = 0;
    failed += test_logger_create();
    failed += test_logger_add_sink();
    failed += test_logger_parse_level();
    failed += test_config_set_get();
    failed += test_config_types();
    failed += test_cli_basic();
    failed += test_profiler_zone();
    failed += test_profiler_memory();
    failed += test_feature_flag_basic();
    failed += test_feature_flag_percentage();
    failed += test_code_review_create();
    failed += test_code_review_flow();
    failed += test_deploy_basic();
    failed += test_deploy_rollback();
    failed += test_semver_parse();
    failed += test_semver_compare();
    failed += test_semver_precedence();
    failed += test_pm_registry();
    failed += test_pm_install();
    failed += test_tr_assertions();
    failed += test_tr_create();
    printf("\n=== Results: %d/%d passed, %d failed ===\n", tests_passed, tests_run, failed);
    return failed > 0 ? 1 : 0;
}
