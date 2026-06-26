#ifndef CICD_PIPELINE_H
#define CICD_PIPELINE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CI/CD Pipeline - Continuous Integration & Deployment (L7/L8) */

#define CICD_MAX_STAGES     32
#define CICD_MAX_JOBS       128
#define CICD_MAX_ARTIFACTS  64
#define CICD_MAX_DEPS       16
#define CICD_MAX_NAME       256
#define CICD_MAX_LOG_LINE   4096
#define CICD_SHA256_LEN     32

typedef enum {
    CICD_JOB_PENDING = 0,
    CICD_JOB_RUNNING,
    CICD_JOB_SUCCESS,
    CICD_JOB_FAILED,
    CICD_JOB_SKIPPED,
    CICD_JOB_CANCELLED
} cicd_job_status_t;

typedef enum {
    CICD_DEPLOY_BLUEGREEN = 0,
    CICD_DEPLOY_CANARY,
    CICD_DEPLOY_ROLLING,
    CICD_DEPLOY_RECREATE
} cicd_deploy_strategy_t;

typedef enum {
    CICD_TRIGGER_PUSH = 0,
    CICD_TRIGGER_PR,
    CICD_TRIGGER_SCHEDULE,
    CICD_TRIGGER_MANUAL,
    CICD_TRIGGER_WEBHOOK
} cicd_trigger_t;

typedef struct {
    char     name[CICD_MAX_NAME];
    char     command[CICD_MAX_NAME];
    char     image[CICD_MAX_NAME];
    int      timeout_seconds;
    int      retry_count;
    int      depends_on[CICD_MAX_DEPS];
    int      dep_count;
    int      needs_artifacts[CICD_MAX_DEPS];
    int      artifact_dep_count;
    uint64_t duration_ms;
    uint64_t start_time_ms;
    cicd_job_status_t status;
    int      attempt;
    char     log[32][CICD_MAX_LOG_LINE];
    int      log_lines;
    char     artifact_path[CICD_MAX_NAME];
} cicd_job_t;

typedef struct {
    char     name[CICD_MAX_NAME];
    cicd_job_t jobs[CICD_MAX_JOBS];
    int      job_count;
    int      parallel_limit;
    uint64_t total_duration_ms;
    int      passed;
    int      failed;
    int      skipped;
} cicd_stage_t;

typedef struct {
    char     name[CICD_MAX_NAME];
    cicd_stage_t stages[CICD_MAX_STAGES];
    int      stage_count;
    cicd_trigger_t trigger;
    char     branch[CICD_MAX_NAME];
    char     commit_sha[64];
    uint64_t pipeline_id;
    uint64_t started_at_ms;
    uint64_t finished_at_ms;
    int      total_jobs;
    int      completed_jobs;
    double   success_rate;
} cicd_pipeline_t;

typedef struct {
    char     sha256[CICD_SHA256_LEN * 2 + 1];
    char     path[CICD_MAX_NAME];
    uint64_t size_bytes;
    uint64_t mtime;
    bool     is_cached;
} cicd_artifact_t;

typedef struct {
    char     key[CICD_MAX_NAME];
    char     value[CICD_MAX_NAME];
} cicd_env_var_t;

typedef struct {
    cicd_env_var_t vars[64];
    int            count;
} cicd_environment_t;

/* API: Pipeline lifecycle */
cicd_pipeline_t* cicd_pipeline_create(const char *name, cicd_trigger_t trigger);
void cicd_pipeline_free(cicd_pipeline_t *p);

int cicd_add_stage(cicd_pipeline_t *p, const char *name, int parallel_limit);
int cicd_add_job(cicd_pipeline_t *p, int stage_idx, const char *name,
                  const char *command, int timeout_s);

/* Job dependency management */
int cicd_job_depends_on(cicd_pipeline_t *p, int stage_idx, int job_idx,
                         int dep_stage, int dep_job);
int cicd_job_needs_artifact(cicd_pipeline_t *p, int stage_idx, int job_idx,
                              int art_stage, int art_job);

/* Pipeline execution */
int cicd_pipeline_run(cicd_pipeline_t *p, const cicd_environment_t *env);
int cicd_pipeline_run_stage(cicd_pipeline_t *p, int stage_idx,
                              const cicd_environment_t *env);

/* Dependency graph resolution (L5: Topological Sort) */
int cicd_resolve_dependencies(const cicd_pipeline_t *p,
                               int *exec_order, int *exec_count);

/* Build cache with content hashing (L5: Hash-based caching) */
int cicd_cache_lookup(const char *cache_key, cicd_artifact_t *out);
int cicd_cache_store(const char *cache_key, const cicd_artifact_t *artifact);
int cicd_cache_invalidate(const char *cache_key_pattern);

/* Deployment strategies (L8) */
const char* cicd_deploy_strategy_name(cicd_deploy_strategy_t s);
int cicd_compute_canary_traffic(int total_instances, double canary_pct,
                                 int *canary_instances, int *stable_instances);
int cicd_bluegreen_swap(int *active_env, int *standby_env);

/* Performance analysis */
void cicd_pipeline_summary(const cicd_pipeline_t *p);
double cicd_pipeline_success_rate(const cicd_pipeline_t *p);
uint64_t cicd_stage_duration_ms(const cicd_stage_t *s);
int cicd_critical_path(const cicd_pipeline_t *p, int *path, int *path_len);
int cicd_critical_path_duration_ms(const cicd_pipeline_t *p);

/* Job status queries */
int cicd_pipeline_failed_jobs(const cicd_pipeline_t *p,
                               int *stage_out, int *job_out, int max_out);
cicd_job_status_t cicd_job_status(const cicd_pipeline_t *p,
                                    int stage_idx, int job_idx);
const char* cicd_job_status_string(cicd_job_status_t s);

/* Environment variable management */
void cicd_env_init(cicd_environment_t *env);
int  cicd_env_set(cicd_environment_t *env, const char *key, const char *value);
const char* cicd_env_get(const cicd_environment_t *env, const char *key);

/* Pipeline retry */
int cicd_retry_failed_jobs(cicd_pipeline_t *p);

/* Build matrix (L7: parameterized builds) */
int cicd_build_matrix_expand(const char *matrix_def,
                              char matrix_configs[][CICD_MAX_NAME], int max_configs);

#endif /* CICD_PIPELINE_H */
