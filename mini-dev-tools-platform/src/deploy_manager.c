#include "deploy_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char* deploy_env_str(DeployEnvironment e) {
    switch (e) {
    case DEPLOY_DEV:        return "dev";
    case DEPLOY_STAGING:    return "staging";
    case DEPLOY_PRODUCTION: return "production";
    case DEPLOY_CANARY:     return "canary";
    default:                return "???";
    }
}

const char* deploy_status_str(DeployStatus s) {
    switch (s) {
    case DEPLOY_IDLE:         return "idle";
    case DEPLOY_IN_PROGRESS:  return "in-progress";
    case DEPLOY_SUCCEEDED:    return "succeeded";
    case DEPLOY_FAILED:       return "failed";
    case DEPLOY_ROLLING_BACK: return "rolling-back";
    case DEPLOY_ROLLED_BACK:  return "rolled-back";
    default:                  return "???";
    }
}

const char* deploy_step_type_str(DeployStepType t) {
    switch (t) {
    case STEP_PRE_DEPLOY:   return "pre-deploy";
    case STEP_DEPLOY:       return "deploy";
    case STEP_HEALTH_CHECK: return "health-check";
    case STEP_SMOKE_TEST:   return "smoke-test";
    case STEP_APPROVAL:     return "approval";
    case STEP_POST_DEPLOY:  return "post-deploy";
    default:                return "???";
    }
}

DeployManager* deploy_create(const char *service, const char *version,
                              DeployEnvironment env, const char *who) {
    DeployManager *dm = (DeployManager*)calloc(1, sizeof(DeployManager));
    if (!dm) return NULL;
    if (service) snprintf(dm->service, sizeof(dm->service), "%s", service);
    if (version) snprintf(dm->version, sizeof(dm->version), "%s", version);
    if (who) snprintf(dm->triggered_by, sizeof(dm->triggered_by), "%s", who);
    dm->env = env;
    dm->status = DEPLOY_IDLE;
    dm->auto_rollback = true;
    dm->max_retries = 3;
    dm->retry_count = 0;
    dm->step_count = 0;
    dm->current_step = 0;
    dm->created_at = time(NULL);
    return dm;
}

void deploy_destroy(DeployManager *dm) { free(dm); }

int deploy_add_step(DeployManager *dm, DeployStepType type,
                     const char *name, int timeout_s, bool required) {
    if (!dm || !name || dm->step_count >= MAX_DEPLOY_STEPS) return -1;
    DeployStep *s = &dm->steps[dm->step_count];
    memset(s, 0, sizeof(DeployStep));
    s->type = type;
    snprintf(s->name, sizeof(s->name), "%s", name);
    s->timeout_seconds = timeout_s > 0 ? timeout_s : 300;
    s->required = required;
    s->completed = false;
    s->success = false;
    dm->step_count++;
    return dm->step_count - 1;
}

int deploy_execute_step(DeployManager *dm) {
    if (!dm || dm->current_step >= dm->step_count) return -1;
    if (dm->status == DEPLOY_IDLE) {
        dm->status = DEPLOY_IN_PROGRESS;
        dm->started_at = time(NULL);
    }
    DeployStep *s = &dm->steps[dm->current_step];
    s->started_at = time(NULL);
    s->success = true;
    s->completed = true;
    s->completed_at = time(NULL);
    dm->current_step++;

    if (dm->current_step >= dm->step_count) {
        dm->status = DEPLOY_SUCCEEDED;
        dm->completed_at = time(NULL);
    }
    return dm->current_step;
}

int deploy_execute_all(DeployManager *dm) {
    if (!dm) return -1;
    while (dm->current_step < dm->step_count && dm->status != DEPLOY_FAILED)
        deploy_execute_step(dm);
    return dm->current_step;
}

int deploy_rollback(DeployManager *dm) {
    if (!dm) return -1;
    dm->status = DEPLOY_ROLLING_BACK;
    for (int i = dm->current_step - 1; i >= 0; i--)
        dm->steps[i].completed = false;
    dm->status = DEPLOY_ROLLED_BACK;
    if (dm->rollback_version[0])
        snprintf(dm->version, sizeof(dm->version), "%s", dm->rollback_version);
    dm->completed_at = time(NULL);
    return 0;
}

bool deploy_is_complete(const DeployManager *dm) {
    return dm && (dm->status == DEPLOY_SUCCEEDED || dm->status == DEPLOY_FAILED ||
           dm->status == DEPLOY_ROLLED_BACK);
}

bool deploy_was_successful(const DeployManager *dm) {
    return dm && dm->status == DEPLOY_SUCCEEDED;
}

double deploy_progress(const DeployManager *dm) {
    if (!dm || dm->step_count == 0) return 0.0;
    return 100.0 * dm->current_step / dm->step_count;
}

int deploy_set_auto_rollback(DeployManager *dm, bool enable) {
    if (!dm) return -1;
    dm->auto_rollback = enable;
    return 0;
}

void deploy_dump(const DeployManager *dm, char *buf, int buf_sz) {
    if (!dm || !buf) return;
    int off = 0;
    off += snprintf(buf + off, buf_sz - off, "%s", "=== Deploy Manager ===\n");
    off += snprintf(buf + off, buf_sz - off, "Service: %s  Version: %s  Env: %s\n",
                    dm->service, dm->version, deploy_env_str(dm->env));
    off += snprintf(buf + off, buf_sz - off, "Status: %s  Progress: %.0f%%  Step: %d/%d\n",
                    deploy_status_str(dm->status), deploy_progress(dm),
                    dm->current_step, dm->step_count);
    off += snprintf(buf + off, buf_sz - off, "Triggered by: %s  Auto-rollback: %s\n",
                    dm->triggered_by, dm->auto_rollback ? "on" : "off");
    for (int i = 0; i < dm->step_count; i++) {
        const DeployStep *s = &dm->steps[i];
        off += snprintf(buf + off, buf_sz - off,
                        "  [%d] %s (%s) %s %s\n",
                        i, s->name, deploy_step_type_str(s->type),
                        s->completed ? (s->success ? "OK" : "FAIL") : "...",
                        s->required ? "[required]" : "");
    }
    if (off < buf_sz) buf[off] = '\0';
}


int deploy_skip_step(DeployManager *dm) {
    if (!dm || dm->current_step >= dm->step_count) return -1;
    dm->steps[dm->current_step].completed = true;
    dm->steps[dm->current_step].success = true;
    dm->current_step++;
    return dm->current_step;
}

int deploy_fail_step(DeployManager *dm, int exit_code) {
    if (!dm || dm->current_step >= dm->step_count) return -1;
    dm->steps[dm->current_step].completed = true;
    dm->steps[dm->current_step].success = false;
    dm->steps[dm->current_step].exit_code = exit_code;
    if (dm->steps[dm->current_step].required) {
        dm->status = DEPLOY_FAILED;
        if (dm->auto_rollback) deploy_rollback(dm);
    }
    dm->current_step++;
    return dm->current_step;
}

int deploy_set_rollback_version(DeployManager *dm, const char *version) {
    if (!dm || !version) return -1;
    snprintf(dm->rollback_version, sizeof(dm->rollback_version), "%s", version);
    return 0;
}

int deploy_retry(DeployManager *dm) {
    if (!dm || dm->retry_count >= dm->max_retries) return -1;
    dm->status = DEPLOY_IN_PROGRESS;
    if (dm->current_step > 0) dm->current_step--;
    dm->retry_count++;
    return dm->retry_count;
}

int deploy_get_completed_steps(const DeployManager *dm, int *indices, int max_count) {
    if (!dm || !indices || max_count <= 0) return 0;
    int count = 0;
    for (int i = 0; i < dm->current_step && count < max_count; i++)
        if (dm->steps[i].completed) indices[count++] = i;
    return count;
}

const char* deploy_get_current_step_name(const DeployManager *dm) {
    if (!dm || dm->current_step >= dm->step_count) return NULL;
    return dm->steps[dm->current_step].name;
}
