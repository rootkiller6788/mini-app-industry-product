/**
 * example_demo.c -- End-to-End DevTools Platform Demo
 *
 * L6: Canonical problem -- Complete CI/CD pipeline simulation
 * L7: Application -- Integrates all 9 modules in a realistic workflow
 *
 * Scenario: A developer triggers a CI build with feature flags, profiling,
 *           code review, and deployment orchestration.
 */
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
#include <stdlib.h>

static void demo_structured_logging(void) {
    printf("\n=== Demo 1: Structured Logging ===\n");
    Logger *log = logger_create();
    logger_add_sink(log, SINK_CONSOLE, "", LOG_DEBUG);
    logger_add_sink(log, SINK_JSON, "", LOG_INFO);
    logger_add_sink(log, SINK_RINGBUF, "audit", LOG_WARN);
    LOG_INFO_F(log, "demo", "Application starting v%d.%d", 2, 1);
    LOG_DEBUG_F(log, "demo", "Initializing subsystems...");
    LOG_WARN_F(log, "demo", "Feature beta is in preview mode");
    LOG_ERROR_F(log, "demo", "Connection to cache timed out");
    char buf[1024];
    logger_dump(log, buf, sizeof(buf));
    printf("%s", buf);
    printf("Total log entries: %lu\n", (unsigned long)logger_entry_count(log));
    logger_destroy(log);
}

static void demo_config_hot_reload(void) {
    printf("\n=== Demo 2: Config Management ===\n");
    /* Create config with environment overlay */
    ConfigManager *base = config_create("base.cfg", false);
    ConfigManager *overlay = config_create("overlay.cfg", false);

    config_set(base, "db.host", "localhost", CFG_STRING, SOURCE_FILE);
    config_set(base, "db.port", "5432", CFG_INT, SOURCE_FILE);
    config_set(base, "cache.enabled", "true", CFG_BOOL, SOURCE_FILE);
    config_set(base, "rate_limit", "100.5", CFG_FLOAT, SOURCE_FILE);

    config_set(overlay, "db.host", "prod-db.internal", CFG_STRING, SOURCE_FILE);
    config_set(overlay, "region", "us-east-1", CFG_STRING, SOURCE_FILE);

    /* Merge overlay into base */
    config_merge(base, overlay);
    printf("DB host: %s\n", config_get(base, "db.host", "unknown"));
    printf("DB port: %lld\n", (long long)config_get_int(base, "db.port", 0));
    printf("Cache: %s\n", config_get_bool(base, "cache.enabled", false) ? "on" : "off");
    printf("Rate limit: %.1f\n", config_get_float(base, "rate_limit", 0));
    printf("Region: %s\n", config_get(base, "region", "unknown"));

    char diff_buf[512];
    config_diff(base, overlay, diff_buf, sizeof(diff_buf));
    printf("Diff:\n%s", diff_buf);

    char json_buf[2048];
    config_export_json(base, json_buf, sizeof(json_buf));
    printf("JSON export:\n%s", json_buf);

    config_destroy(base);
    config_destroy(overlay);
}

static void demo_feature_flags(void) {
    printf("\n=== Demo 3: Feature Flags (Trunk-Based Development) ===\n");
    FeatureFlagSystem *ffs = ff_system_create("production");

    /* Define flags */
    ff_define_flag(ffs, "dark-mode", "New dark mode UI", FLAG_BOOLEAN);
    ff_define_flag(ffs, "search-v2", "Elasticsearch backend", FLAG_PERCENTAGE);
    ff_define_flag(ffs, "enterprise-sso", "Enterprise SSO for VIPs", FLAG_TARGETED);
    ff_define_flag(ffs, "christmas-theme", "Seasonal theme", FLAG_SCHEDULED);

    /* Configure flags */
    ff_set_state(ffs, "dark-mode", FLAG_ON);
    ff_set_rollout(ffs, "search-v2", 25.0);  /* 25% canary */

    TargetingAttribute attrs[] = {{"tier", "enterprise"}, {"region", "us"}};
    ff_add_targeting_rule(ffs, "enterprise-sso", "tier", "enterprise");

    /* Simulate requests from different users */
    for (int i = 0; i < 5; i++) {
        char username[32];
        snprintf(username, sizeof(username), "user-%d", i);
        bool dm = ff_is_enabled(ffs, "dark-mode", NULL, 0);
        bool sv2 = ff_is_enabled(ffs, "search-v2", NULL, 0);
        bool sso = ff_is_enabled(ffs, "enterprise-sso", attrs, 2);
        printf("  %s: dark-mode=%s search-v2=%s sso=%s\n",
               username, dm ? "on" : "off", sv2 ? "on" : "off", sso ? "on" : "off");
    }

    printf("Flags enabled: %d/%d\n",
           ff_get_enabled_count(ffs), ffs->flag_count);
    printf("Coverage: %.1f%%\n", ff_system_coverage(ffs));

    char export_buf[512];
    ff_export_config(ffs, export_buf, sizeof(export_buf));
    printf("Export:\n%s", export_buf);

    ff_system_destroy(ffs);
}

static void demo_profiling(void) {
    printf("\n=== Demo 4: Performance Profiling ===\n");
    Profiler *p = profiler_create();

    /* Simulate a request lifecycle with zones */
    int r1 = profiler_begin_zone(p, "request_parse", PROF_CPU);
    int r2 = profiler_begin_zone(p, "db_query", PROF_IO);
    int r3 = profiler_begin_zone(p, "template_render", PROF_CPU);
    int r4 = profiler_begin_zone(p, "cache_check", PROF_MEMORY);

    profiler_end_zone(p, r1);
    profiler_end_zone(p, r2);
    profiler_end_zone(p, r3);
    profiler_end_zone(p, r4);

    /* Simulate allocations */
    profiler_record_allocation(p, 1048576);  /* 1 MB */
    profiler_record_allocation(p, 2097152);  /* 2 MB */
    profiler_record_free(p, 1048576);         /* free 1 MB */

    printf("CPU utilization: %.1f%%\n", profiler_cpu_utilization(p));
    printf("Memory usage: %.2f MB\n", profiler_memory_usage_mb(p));
    printf("Peak memory: %.2f MB\n",
           profiler_memory_stats(p).peak_usage / (1024.0 * 1024.0));
    printf("Memory leak: %.2f MB\n", profiler_memory_leak_mb(p));

    int hottest = profiler_find_hottest_zone(p);
    if (hottest >= 0) {
        printf("Hottest zone: %s (%.2f ms, %d calls)\n",
               p->zones[hottest].name,
               profiler_zone_duration_ms(p, hottest),
               profiler_zone_call_count(p, hottest));
    }

    char flamegraph[2048];
    profiler_generate_flamegraph(p, flamegraph, sizeof(flamegraph));
    printf("Flamegraph:\n%s", flamegraph);

    profiler_destroy(p);
}

static void demo_code_review_flow(void) {
    printf("\n=== Demo 5: Code Review Pipeline ===\n");
    CodeReview *cr = review_create("Add 2FA support", "alice", "feature/2fa");

    review_add_reviewer(cr, "bob");
    review_add_reviewer(cr, "carol");
    review_add_file(cr, "src/auth/two_factor.c");
    review_add_file(cr, "src/auth/two_factor.h");
    review_add_file(cr, "tests/test_two_factor.c");

    /* Bob leaves comments */
    review_add_comment(cr, "bob", "Use constant-time comparison here", "src/auth/two_factor.c", 45);
    review_add_comment(cr, "carol", "Add null check for user pointer", "src/auth/two_factor.c", 12);

    /* Alice resolves them */
    review_resolve_comment(cr, 0, "alice");
    review_resolve_comment(cr, 1, "alice");

    /* CI checks */
    review_add_check(cr, "lint", CHECK_PASSED, 2.3, "No lint errors");
    review_add_check(cr, "unit-tests", CHECK_PASSED, 15.7, "All 42 tests passed");
    review_add_check(cr, "coverage", CHECK_PASSED, 8.1, "87% coverage");

    /* Approve then merge if all gates pass */
    review_approve(cr);
    if (review_is_mergeable(cr)) {
        review_merge(cr);
        printf("Review merged! Time to merge: %.1f hours\n",
               review_time_to_merge_hours(cr));
    } else {
        printf("Review approved but not mergeable yet.\n");
    }

    char dump_buf[512];
    review_dump(cr, dump_buf, sizeof(dump_buf));
    printf("%s", dump_buf);

    review_destroy(cr);
}

static void demo_deploy_pipeline(void) {
    printf("\n=== Demo 6: Deployment Pipeline ===\n");
    DeployManager *dm = deploy_create("web-api", "v3.2.0", DEPLOY_PRODUCTION, "ci-bot");
    deploy_set_rollback_version(dm, "v3.1.0");

    deploy_add_step(dm, STEP_PRE_DEPLOY,   "schema_migration", 120, true);
    deploy_add_step(dm, STEP_DEPLOY,       "push_container",    180, true);
    deploy_add_step(dm, STEP_HEALTH_CHECK, "health_endpoint",    30, true);
    deploy_add_step(dm, STEP_SMOKE_TEST,   "smoke_suite",        60, true);
    deploy_add_step(dm, STEP_POST_DEPLOY,  "cdn_purge",          30, false);

    printf("Starting deployment of %s to %s\n", dm->service, deploy_env_str(dm->env));

    /* Execute step by step */
    for (int i = 0; i < dm->step_count; i++) {
        const char *step_name = deploy_get_current_step_name(dm);
        printf("  [%d/%d] %s... ", i+1, dm->step_count, step_name);
        deploy_execute_step(dm);
        printf("OK\n");
    }

    printf("\nDeployment %s (%.0f%% complete)\n",
           deploy_was_successful(dm) ? "SUCCEEDED" : "FAILED",
           deploy_progress(dm));

    char dump_buf[1024];
    deploy_dump(dm, dump_buf, sizeof(dump_buf));
    printf("%s", dump_buf);

    deploy_destroy(dm);
}

static void demo_package_management(void) {
    printf("\n=== Demo 7: Package Dependency Resolution ===\n");

    /* Theorem: Topological sorting (Kahn's Algorithm) for dependency graphs
     * Reference: Kahn, "Topological Sorting of Large Networks", CACM 1962
     * Complexity: O(V + E) where V = packages, E = dependencies */

    PackageRegistry *reg = pm_registry_create(128);

    /* Register packages */
    Package pkgs[] = {
        {.name = "express",    .version = {4, 18, 2}, .dep_count = 0},
        {.name = "lodash",     .version = {4, 17, 21}, .dep_count = 0},
        {.name = "body-parser", .version = {1, 20, 0}, .dep_count = 0},
        {.name = "app-core",   .version = {2, 0, 0}, .dep_count = 3,
         .deps = {{.name = "express", .constraint = DEP_CARET, .version = {4, 0, 0}},
                   {.name = "lodash", .constraint = DEP_TILDE, .version = {4, 17, 0}},
                   {.name = "body-parser", .constraint = DEP_GTE, .version = {1, 18, 0}}}},
    };
    int npkg = (int)(sizeof(pkgs) / sizeof(pkgs[0]));
    for (int i = 0; i < npkg; i++)
        pm_registry_add(reg, &pkgs[i]);

    /* Install app-core with its dependencies */
    SemVer want = semver_parse("2.0.0");
    Package *app = pm_registry_find(reg, "app-core");
    if (app) {
        pm_install(reg, "app-core", want);

        /* Resolve all transitive dependencies */
        ResolveResult resolved = pm_resolve_deps(reg, "app-core", want);
        printf("Resolved %d packages for app-core:\n", resolved.resolved_count);
        for (int i = 0; i < resolved.resolved_count; i++) {
            char ver[32];
            semver_format(resolved.resolved[i]->version, ver, sizeof(ver));
            printf("  %s @ %s\n", resolved.resolved[i]->name, ver);
        }
        printf("Resolution %s\n", resolved.success ? "SUCCESS" : "FAILED");
    }

    /* Demonstrate SemVer precedence */
    printf("\nSemVer precedence examples:\n");
    SemVer v200 = semver_parse("2.0.0");
    SemVer v210 = semver_parse("2.1.0");
    SemVer v300 = semver_parse("3.0.0");
    printf("  2.0.0 < 2.1.0: %s\n", semver_compare(v200, v210) < 0 ? "yes" : "no");
    printf("  2.1.0 < 3.0.0: %s\n", semver_compare(v210, v300) < 0 ? "yes" : "no");

    /* Caret constraint */
    printf("\nCaret (^) constraint: ^1.2.3 means >=1.2.3 <2.0.0\n");
    SemVer v151 = semver_parse("1.5.1");
    printf("  1.5.1 satisfies ^1.2.3: %s\n",
           semver_range_satisfies(v151, "^1.2.3") ? "yes" : "no");

    /* Find updates */
    Package updates[8];
    int nupd = pm_find_updates(reg, updates, 8);
    printf("\nAvailable updates: %d\n", nupd);

    /* Build tree */
    char tree_buf[512];
    pm_format_tree(reg, "app-core", tree_buf, sizeof(tree_buf));
    printf("\nDependency tree:\n%s", tree_buf);

    pm_registry_destroy(reg);
}

int main(void) {
    printf("============================================\n");
    printf("  mini-dev-tools-platform -- Demo Suite\n");
    printf("  C99 Developer Tools Framework\n");
    printf("============================================\n");

    demo_structured_logging();
    demo_config_hot_reload();
    demo_feature_flags();
    demo_profiling();
    demo_code_review_flow();
    demo_deploy_pipeline();
    demo_package_management();

    printf("\n============================================\n");
    printf("  All demos completed successfully!\n");
    printf("============================================\n");
    return 0;
}