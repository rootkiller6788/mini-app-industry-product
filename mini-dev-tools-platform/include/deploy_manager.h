#ifndef DEPLOY_MANAGER_H
#define DEPLOY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define MAX_DEPLOY_TARGETS   32
#define MAX_DEPLOY_STEPS     16
#define MAX_DEPLOY_ENV_NAME  32
#define MAX_DEPLOY_VERSION   32
#define MAX_APPROVAL_RULES   8

typedef enum {
    DEPLOY_DEV        = 0,
    DEPLOY_STAGING    = 1,
    DEPLOY_PRODUCTION = 2,
    DEPLOY_CANARY     = 3
} DeployEnvironment;

typedef enum {
    DEPLOY_IDLE       = 0,
    DEPLOY_IN_PROGRESS = 1,
    DEPLOY_SUCCEEDED  = 2,
    DEPLOY_FAILED     = 3,
    DEPLOY_ROLLING_BACK = 4,
    DEPLOY_ROLLED_BACK  = 5
} DeployStatus;

typedef enum {
    STEP_PRE_DEPLOY   = 0,
    STEP_DEPLOY       = 1,
    STEP_HEALTH_CHECK = 2,
    STEP_SMOKE_TEST   = 3,
    STEP_APPROVAL     = 4,
    STEP_POST_DEPLOY  = 5
} DeployStepType;

typedef struct {
    DeployStepType type;
    char   name[64];
    char   command[256];
    int    timeout_seconds;
    bool   required;
    bool   completed;
    bool   success;
    time_t started_at;
    time_t completed_at;
    int    exit_code;
} DeployStep;

typedef struct {
    char   service[64];
    char   version[MAX_DEPLOY_VERSION];
    DeployEnvironment env;
    DeployStep steps[MAX_DEPLOY_STEPS];
    int    step_count;
    int    current_step;
    DeployStatus status;
    char   triggered_by[48];
    bool   auto_rollback;
    int    max_retries;
    int    retry_count;
    time_t created_at;
    time_t started_at;
    time_t completed_at;
    char   rollback_version[MAX_DEPLOY_VERSION];
} DeployManager;

DeployManager* deploy_create(const char *service, const char *version,
                              DeployEnvironment env, const char *who);
void           deploy_destroy(DeployManager *dm);
int            deploy_add_step(DeployManager *dm, DeployStepType type,
                                const char *name, int timeout_s, bool required);
int            deploy_execute_step(DeployManager *dm);
int            deploy_execute_all(DeployManager *dm);
int            deploy_rollback(DeployManager *dm);
bool           deploy_is_complete(const DeployManager *dm);
bool           deploy_was_successful(const DeployManager *dm);
double         deploy_progress(const DeployManager *dm);
int            deploy_set_auto_rollback(DeployManager *dm, bool enable);
int            deploy_skip_step(DeployManager *dm);
int            deploy_fail_step(DeployManager *dm, int exit_code);
int            deploy_set_rollback_version(DeployManager *dm, const char *version);
int            deploy_retry(DeployManager *dm);
int            deploy_get_completed_steps(const DeployManager *dm, int *indices, int max_count);
const char*    deploy_get_current_step_name(const DeployManager *dm);
const char*    deploy_env_str(DeployEnvironment e);
const char*    deploy_status_str(DeployStatus s);
const char*    deploy_step_type_str(DeployStepType t);
void           deploy_dump(const DeployManager *dm, char *buf, int buf_sz);

#endif
