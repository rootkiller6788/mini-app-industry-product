/* cicd_pipeline.c - CI/CD Pipeline Implementation
 *
 * Continuous Integration & Deployment pipeline with:
 *   - Multi-stage pipeline execution (L7)
 *   - Job dependency graph with topological sort (L5)
 *   - Content-hashed build cache (L5)
 *   - Blue-green and canary deployment strategies (L8)
 *   - Critical path analysis (L5)
 *   - Build matrix expansion (L7)
 *
 * References:
 *   - Humble & Farley, "Continuous Delivery", 2010
 *   - Kim et al., "The DevOps Handbook", 2016
 *   - Fowler, "BlueGreenDeployment", martinfowler.com, 2010
 *   - Kahn, "Canary Deployments", Netflix Tech Blog, 2013
 */

#include "cicd_pipeline.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* =========================================================================
 * Section 1: Pipeline Lifecycle (L7)
 * ========================================================================= */

cicd_pipeline_t* cicd_pipeline_create(const char *name, cicd_trigger_t trigger) {
    if (!name) return NULL;
    cicd_pipeline_t *p = (cicd_pipeline_t*)calloc(1, sizeof(cicd_pipeline_t));
    if (!p) return NULL;
    strncpy(p->name, name, CICD_MAX_NAME - 1);
    p->trigger = trigger;
    p->pipeline_id = (uint64_t)(uintptr_t)p;
    return p;
}

void cicd_pipeline_free(cicd_pipeline_t *p) { free(p); }

int cicd_add_stage(cicd_pipeline_t *p, const char *name, int parallel_limit) {
    if (!p || !name || p->stage_count >= CICD_MAX_STAGES) return -1;
    cicd_stage_t *s = &p->stages[p->stage_count];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, CICD_MAX_NAME - 1);
    s->parallel_limit = (parallel_limit > 0) ? parallel_limit : 4;
    p->stage_count++;
    return p->stage_count - 1;
}

int cicd_add_job(cicd_pipeline_t *p, int stage_idx, const char *name,
                  const char *command, int timeout_s) {
    if (!p || !name || !command) return -1;
    if (stage_idx < 0 || stage_idx >= p->stage_count) return -1;
    cicd_stage_t *s = &p->stages[stage_idx];
    if (s->job_count >= CICD_MAX_JOBS) return -1;
    cicd_job_t *j = &s->jobs[s->job_count];
    memset(j, 0, sizeof(*j));
    strncpy(j->name, name, CICD_MAX_NAME - 1);
    strncpy(j->command, command, CICD_MAX_NAME - 1);
    j->timeout_seconds = (timeout_s > 0) ? timeout_s : 3600;
    j->status = CICD_JOB_PENDING;
    p->total_jobs++;
    return s->job_count++;
}

/* =========================================================================
 * Section 2: Job Dependencies (L3: DAG Construction)
 * ========================================================================= */

int cicd_job_depends_on(cicd_pipeline_t *p, int stage_idx, int job_idx,
                         int dep_stage, int dep_job) {
    if (!p || stage_idx < 0 || stage_idx >= p->stage_count) return -1;
    cicd_stage_t *s = &p->stages[stage_idx];
    if (job_idx < 0 || job_idx >= s->job_count) return -1;
    cicd_job_t *j = &s->jobs[job_idx];
    if (j->dep_count >= CICD_MAX_DEPS) return -1;
    /* Encode dependency as (dep_stage * CICD_MAX_JOBS + dep_job) */
    j->depends_on[j->dep_count] = dep_stage * CICD_MAX_JOBS + dep_job;
    j->dep_count++;
    return 0;
}

int cicd_job_needs_artifact(cicd_pipeline_t *p, int stage_idx, int job_idx,
                              int art_stage, int art_job) {
    if (!p || stage_idx < 0 || stage_idx >= p->stage_count) return -1;
    cicd_stage_t *s = &p->stages[stage_idx];
    if (job_idx < 0 || job_idx >= s->job_count) return -1;
    cicd_job_t *j = &s->jobs[job_idx];
    if (j->artifact_dep_count >= CICD_MAX_DEPS) return -1;
    j->needs_artifacts[j->artifact_dep_count] = art_stage * CICD_MAX_JOBS + art_job;
    j->artifact_dep_count++;
    return 0;
}

/* =========================================================================
 * Section 3: Topological Sort for Dependency Resolution (L5)
 *
 * Kahn's algorithm for topological sorting of a DAG.
 * O(V + E) time, O(V) space.
 *
 * Theorem (Topological Sort): A directed graph has a topological
 * ordering iff it is acyclic. Kahn's algorithm detects cycles by
 * checking if all vertices are processed.
 * ========================================================================= */

int cicd_resolve_dependencies(const cicd_pipeline_t *p,
                               int *exec_order, int *exec_count) {
    if (!p || !exec_order || !exec_count) return -1;

    int total = p->total_jobs;
    if (total == 0) { *exec_count = 0; return 0; }

    /* Build in-degree array and adjacency list */
    int *in_degree = (int*)calloc((size_t)total, sizeof(int));
    int *adj = (int*)calloc((size_t)(total * CICD_MAX_DEPS), sizeof(int));
    int *adj_count = (int*)calloc((size_t)total, sizeof(int));
    if (!in_degree || !adj || !adj_count) {
        free(in_degree); free(adj); free(adj_count);
        return -1;
    }

    /* Map global job index to (stage, job) and build edges */
    int idx = 0;
    for (int si = 0; si < p->stage_count; si++) {
        const cicd_stage_t *s = &p->stages[si];
        for (int ji = 0; ji < s->job_count; ji++) {
            const cicd_job_t *j = &s->jobs[ji];
            int my_idx = idx;
            /* Process dependencies */
            for (int di = 0; di < j->dep_count; di++) {
                int dep_encoded = j->depends_on[di];
                int dep_si = dep_encoded / CICD_MAX_JOBS;
                int dep_ji = dep_encoded % CICD_MAX_JOBS;
                /* Find global index of dependency */
                int dep_idx = 0;
                for (int dsi = 0; dsi < dep_si; dsi++)
                    dep_idx += p->stages[dsi].job_count;
                dep_idx += dep_ji;
                /* Edge: dep_idx -> my_idx */
                adj[dep_idx * CICD_MAX_DEPS + adj_count[dep_idx]] = my_idx;
                adj_count[dep_idx]++;
                in_degree[my_idx]++;
            }
            idx++;
        }
    }

    /* Kahn's algorithm */
    int *queue = (int*)malloc((size_t)total * sizeof(int));
    int qh = 0, qt = 0;
    if (!queue) { free(in_degree); free(adj); free(adj_count); return -1; }

    /* Enqueue all vertices with in-degree 0 */
    for (int i = 0; i < total; i++)
        if (in_degree[i] == 0) queue[qt++] = i;

    int count = 0;
    while (qh < qt) {
        int u = queue[qh++];
        exec_order[count++] = u;
        for (int i = 0; i < adj_count[u]; i++) {
            int v = adj[u * CICD_MAX_DEPS + i];
            in_degree[v]--;
            if (in_degree[v] == 0) queue[qt++] = v;
        }
    }

    free(in_degree); free(adj); free(adj_count); free(queue);

    if (count != total) {
        /* Cycle detected - incomplete topological sort */
        *exec_count = count;
        return -1;
    }
    *exec_count = count;
    return 0;
}

/* =========================================================================
 * Section 4: Content-Hashed Build Cache (L5)
 *
 * Uses a simple hash of content for cache keys. In production systems
 * this would use SHA-256 and a distributed cache (e.g., Redis, S3).
 *
 * Algorithm: DJB2 hash for cache key computation.
 * Cache hit = same input + same command → skip job execution.
 * ========================================================================= */

static uint64_t djb2_hash(const char *str) {
    uint64_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + (uint64_t)(unsigned char)c;
    return hash;
}

static char cache_store[64][CICD_MAX_NAME];
static int cache_count = 0;

int cicd_cache_lookup(const char *cache_key, cicd_artifact_t *out) {
    if (!cache_key || !out) return -1;
    for (int i = 0; i < cache_count; i++) {
        if (strcmp(cache_store[i], cache_key) == 0) {
            memset(out, 0, sizeof(*out));
            snprintf(out->sha256, sizeof(out->sha256), "%016llx",
                     (unsigned long long)djb2_hash(cache_key));
            out->is_cached = true;
            return 0;
        }
    }
    return -1;
}

int cicd_cache_store(const char *cache_key, const cicd_artifact_t *artifact) {
    if (!cache_key || cache_count >= 64) return -1;
    strncpy(cache_store[cache_count], cache_key, CICD_MAX_NAME - 1);
    cache_count++;
    (void)artifact;
    return 0;
}

int cicd_cache_invalidate(const char *cache_key_pattern) {
    if (!cache_key_pattern) return -1;
    int removed = 0;
    for (int i = 0; i < cache_count; ) {
        if (strstr(cache_store[i], cache_key_pattern)) {
            /* Remove by shifting remaining entries */
            for (int j = i; j < cache_count - 1; j++)
                strcpy(cache_store[j], cache_store[j + 1]);
            cache_count--;
            removed++;
        } else {
            i++;
        }
    }
    return removed;
}

/* =========================================================================
 * Section 5: Deployment Strategies (L8)
 *
 * Blue-Green: Two identical environments. Traffic switches atomically.
 *   Active = current production, Standby = new version.
 *   After validation, swap: old Active becomes Standby.
 *
 * Canary: Gradually shift traffic percentage to new version.
 *   Start with 5%, increase to 25%, 50%, 100% if metrics are healthy.
 *   Rollback by setting canary_pct back to 0.
 *
 * Reference: Fowler, "BlueGreenDeployment", 2010
 *            Netflix, "Canary Analysis", 2013
 * ========================================================================= */

const char* cicd_deploy_strategy_name(cicd_deploy_strategy_t s) {
    switch (s) {
    case CICD_DEPLOY_BLUEGREEN: return "Blue-Green";
    case CICD_DEPLOY_CANARY:    return "Canary";
    case CICD_DEPLOY_ROLLING:   return "Rolling";
    case CICD_DEPLOY_RECREATE:  return "Recreate";
    default: return "Unknown";
    }
}

int cicd_compute_canary_traffic(int total_instances, double canary_pct,
                                 int *canary_instances, int *stable_instances) {
    if (!canary_instances || !stable_instances) return -1;
    if (total_instances <= 0 || canary_pct < 0.0 || canary_pct > 100.0) return -1;
    *canary_instances = (int)((double)total_instances * canary_pct / 100.0);
    if (*canary_instances < 1 && canary_pct > 0.0) *canary_instances = 1;
    *stable_instances = total_instances - *canary_instances;
    if (*stable_instances < 0) *stable_instances = 0;
    return 0;
}

int cicd_bluegreen_swap(int *active_env, int *standby_env) {
    if (!active_env || !standby_env) return -1;
    int tmp = *active_env;
    *active_env = *standby_env;
    *standby_env = tmp;
    return 0;
}

/* =========================================================================
 * Section 6: Critical Path Analysis (L5)
 *
 * Critical Path Method (CPM) for pipeline optimization.
 * The critical path is the longest chain of dependent jobs.
 * Reducing any job on the critical path reduces total pipeline time.
 *
 * Algorithm:
 *   1. Topological sort to get execution order
 *   2. Forward pass: compute earliest finish time for each job
 *   3. Backward pass: compute latest start time
 *   4. Jobs with zero slack are on the critical path
 *
 * Reference: Kelley & Walker, "Critical-Path Planning and Scheduling", 1959
 * ========================================================================= */

int cicd_critical_path(const cicd_pipeline_t *p, int *path, int *path_len) {
    if (!p || !path || !path_len || p->total_jobs == 0) return -1;

    int *order = (int*)malloc((size_t)p->total_jobs * sizeof(int));
    int count = 0;
    if (!order) return -1;

    if (cicd_resolve_dependencies(p, order, &count) < 0) {
        free(order); return -1;
    }

    /* Forward pass: earliest start/finish times */
    int64_t *est = (int64_t*)calloc((size_t)p->total_jobs, sizeof(int64_t));
    int64_t *eft = (int64_t*)calloc((size_t)p->total_jobs, sizeof(int64_t));
    if (!est || !eft) { free(order); free(est); free(eft); return -1; }

    for (int i = 0; i < count; i++) {
        int u = order[i];
        /* Find job's duration */
        int idx = 0;
        int64_t dur = 0;
        for (int si = 0; si < p->stage_count && idx <= u; si++) {
            const cicd_stage_t *s = &p->stages[si];
            for (int ji = 0; ji < s->job_count && idx <= u; ji++, idx++) {
                if (idx == u) { dur = (int64_t)s->jobs[ji].duration_ms; break; }
            }
        }
        eft[u] = est[u] + dur;

        /* Update successors' EST via adjacency built from deps */
        idx = 0;
        for (int si = 0; si < p->stage_count; si++) {
            const cicd_stage_t *s = &p->stages[si];
            for (int ji = 0; ji < s->job_count; ji++, idx++) {
                const cicd_job_t *j = &s->jobs[ji];
                for (int di = 0; di < j->dep_count; di++) {
                    int dep_enc = j->depends_on[di];
                    int dep_global = 0;
                    for (int dsi = 0; dsi < (dep_enc / CICD_MAX_JOBS); dsi++)
                        dep_global += p->stages[dsi].job_count;
                    dep_global += dep_enc % CICD_MAX_JOBS;
                    if (dep_global == u && eft[u] > est[idx])
                        est[idx] = eft[u];
                }
            }
        }
    }

    /* Find longest path end */
    int64_t max_eft = 0;
    int end_job = -1;
    for (int i = 0; i < p->total_jobs; i++) {
        if (eft[i] > max_eft) { max_eft = eft[i]; end_job = i; }
    }

    /* Trace back critical path */
    *path_len = 0;
    if (end_job >= 0) {
        int cur = end_job;
        while (cur >= 0 && *path_len < p->total_jobs) {
            path[*path_len] = cur;
            (*path_len)++;
            /* Find predecessor with eft == est[cur] */
            int found = 0;
            int idx = 0;
            for (int si = 0; si < p->stage_count && !found; si++) {
                const cicd_stage_t *s = &p->stages[si];
                for (int ji = 0; ji < s->job_count && !found; ji++, idx++) {
                    const cicd_job_t *j = &s->jobs[ji];
                    for (int di = 0; di < j->dep_count; di++) {
                        int dep_enc = j->depends_on[di];
                        int dep_global = 0;
                        for (int dsi = 0; dsi < (dep_enc / CICD_MAX_JOBS); dsi++)
                            dep_global += p->stages[dsi].job_count;
                        dep_global += dep_enc % CICD_MAX_JOBS;
                        if (idx == cur && eft[dep_global] == est[cur]) {
                            cur = dep_global;
                            found = 1;
                            break;
                        }
                    }
                }
            }
            if (!found) break;  /* Start of path */
        }
    }

    free(order); free(est); free(eft);
    return 0;
}

int cicd_critical_path_duration_ms(const cicd_pipeline_t *p) {
    if (!p) return 0;
    int *path = (int*)malloc((size_t)p->total_jobs * sizeof(int));
    int path_len = 0;
    if (!path) return 0;

    int64_t total = 0;
    if (cicd_critical_path(p, path, &path_len) == 0) {
        int idx = 0;
        for (int i = 0; i < path_len; i++) {
            int u = path[i];
            idx = 0;
            for (int si = 0; si < p->stage_count && idx <= u; si++) {
                const cicd_stage_t *s = &p->stages[si];
                for (int ji = 0; ji < s->job_count && idx <= u; ji++, idx++) {
                    if (idx == u) {
                        total += (int64_t)s->jobs[ji].duration_ms;
                        break;
                    }
                }
            }
        }
    }
    free(path);
    return (int)total;
}

/* =========================================================================
 * Section 7: Build Matrix Expansion (L7)
 *
 * GitHub Actions / GitLab CI style build matrix.
 * Input: "os: [linux, windows], compiler: [gcc, clang]"
 * Expands to: ["linux-gcc", "linux-clang", "windows-gcc", "windows-clang"]
 *
 * This is a Cartesian product over dimension values.
 * ========================================================================= */

#include <ctype.h>

static int split_trim(char *s, char tokens[][CICD_MAX_NAME], int max_tokens) {
    int count = 0;
    char *tok = strtok(s, ",");
    while (tok && count < max_tokens) {
        /* Trim whitespace */
        while (isspace((unsigned char)*tok)) tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && isspace((unsigned char)*end)) *end-- = '\0';
        strncpy(tokens[count], tok, CICD_MAX_NAME - 1);
        count++;
        tok = strtok(NULL, ",");
    }
    return count;
}

int cicd_build_matrix_expand(const char *matrix_def,
                              char matrix_configs[][CICD_MAX_NAME], int max_configs) {
    if (!matrix_def || !matrix_configs || max_configs <= 0) return -1;

    /* Parse dimensions from "key: [v1, v2, v3], key2: [v4, v5]" */
    char *def_copy = strdup(matrix_def);
    if (!def_copy) return -1;

    char dim_names[8][CICD_MAX_NAME];
    char dim_values[8][8][CICD_MAX_NAME];
    int dim_counts[8];
    int num_dims = 0;

    char *saveptr;
    char *dim = strtok_r(def_copy, ",", &saveptr);
    while (dim && num_dims < 8) {
        /* Skip leading whitespace */
        while (isspace((unsigned char)*dim)) dim++;
        /* Find colon separator */
        char *colon = strchr(dim, ':');
        if (!colon) { dim = strtok_r(NULL, ",", &saveptr); continue; }
        *colon = '\0';
        strncpy(dim_names[num_dims], dim, CICD_MAX_NAME - 1);

        /* Parse values inside [brackets] */
        char *val_part = colon + 1;
        char *lb = strchr(val_part, '[');
        char *rb = strchr(val_part, ']');
        if (!lb || !rb || rb <= lb) { dim = strtok_r(NULL, ",", &saveptr); continue; }
        *rb = '\0';
        char *vals = lb + 1;

        char val_tokens[8][CICD_MAX_NAME];
        int num_vals = split_trim(vals, val_tokens, 8);
        for (int i = 0; i < num_vals && i < 8; i++)
            strncpy(dim_values[num_dims][i], val_tokens[i], CICD_MAX_NAME - 1);
        dim_counts[num_dims] = num_vals;
        num_dims++;

        dim = strtok_r(NULL, ",", &saveptr);
    }

    free(def_copy);

    if (num_dims == 0) { matrix_configs[0][0] = '\0'; return 0; }

    /* Cartesian product */
    int total = 1;
    for (int d = 0; d < num_dims; d++) total *= dim_counts[d];
    if (total > max_configs) total = max_configs;

    int indices[8] = {0};
    for (int config = 0; config < total; config++) {
        matrix_configs[config][0] = '\0';
        for (int d = 0; d < num_dims; d++) {
            if (d > 0) strncat(matrix_configs[config], "-", CICD_MAX_NAME - 1);
            strncat(matrix_configs[config], dim_values[d][indices[d]], CICD_MAX_NAME - 1);
        }
        /* Increment indices like a mixed-radix counter */
        int carry = 1;
        for (int d = num_dims - 1; d >= 0 && carry; d--) {
            indices[d]++;
            if (indices[d] >= dim_counts[d]) indices[d] = 0;
            else carry = 0;
        }
    }
    return total;
}

/* =========================================================================
 * Section 8: Pipeline Execution & Status (L7)
 * ========================================================================= */

const char* cicd_job_status_string(cicd_job_status_t s) {
    switch (s) {
    case CICD_JOB_PENDING:   return "pending";
    case CICD_JOB_RUNNING:   return "running";
    case CICD_JOB_SUCCESS:   return "success";
    case CICD_JOB_FAILED:    return "failed";
    case CICD_JOB_SKIPPED:   return "skipped";
    case CICD_JOB_CANCELLED: return "cancelled";
    default: return "unknown";
    }
}

void cicd_env_init(cicd_environment_t *env) {
    if (env) memset(env, 0, sizeof(*env));
}

int cicd_env_set(cicd_environment_t *env, const char *key, const char *value) {
    if (!env || !key || !value || env->count >= 64) return -1;
    strncpy(env->vars[env->count].key, key, CICD_MAX_NAME - 1);
    strncpy(env->vars[env->count].value, value, CICD_MAX_NAME - 1);
    env->count++;
    return 0;
}

const char* cicd_env_get(const cicd_environment_t *env, const char *key) {
    if (!env || !key) return NULL;
    for (int i = 0; i < env->count; i++)
        if (strcmp(env->vars[i].key, key) == 0)
            return env->vars[i].value;
    return NULL;
}

double cicd_pipeline_success_rate(const cicd_pipeline_t *p) {
    if (!p || p->total_jobs == 0) return 0.0;
    int passed = 0;
    for (int si = 0; si < p->stage_count; si++) {
        const cicd_stage_t *s = &p->stages[si];
        for (int ji = 0; ji < s->job_count; ji++)
            if (s->jobs[ji].status == CICD_JOB_SUCCESS) passed++;
    }
    return (double)passed / (double)p->total_jobs * 100.0;
}

uint64_t cicd_stage_duration_ms(const cicd_stage_t *s) {
    if (!s) return 0;
    uint64_t max_dur = 0;
    for (int i = 0; i < s->job_count; i++)
        if (s->jobs[i].duration_ms > max_dur)
            max_dur = s->jobs[i].duration_ms;
    return max_dur;
}

int cicd_pipeline_failed_jobs(const cicd_pipeline_t *p,
                               int *stage_out, int *job_out, int max_out) {
    if (!p || !stage_out || !job_out) return 0;
    int count = 0;
    for (int si = 0; si < p->stage_count && count < max_out; si++) {
        const cicd_stage_t *s = &p->stages[si];
        for (int ji = 0; ji < s->job_count && count < max_out; ji++) {
            if (s->jobs[ji].status == CICD_JOB_FAILED) {
                stage_out[count] = si;
                job_out[count] = ji;
                count++;
            }
        }
    }
    return count;
}

cicd_job_status_t cicd_job_status(const cicd_pipeline_t *p,
                                    int stage_idx, int job_idx) {
    if (!p || stage_idx < 0 || stage_idx >= p->stage_count) return CICD_JOB_FAILED;
    const cicd_stage_t *s = &p->stages[stage_idx];
    if (job_idx < 0 || job_idx >= s->job_count) return CICD_JOB_FAILED;
    return s->jobs[job_idx].status;
}

void cicd_pipeline_summary(const cicd_pipeline_t *p) {
    if (!p) { printf("Pipeline: NULL\n"); return; }
    printf("Pipeline [%s] trigger=%d branch=%s\n", p->name, p->trigger, p->branch);
    int pass = 0, fail = 0, skip = 0;
    for (int si = 0; si < p->stage_count; si++) {
        const cicd_stage_t *s = &p->stages[si];
        printf("  Stage [%s]: %d jobs, parallel=%d\n",
               s->name, s->job_count, s->parallel_limit);
        for (int ji = 0; ji < s->job_count; ji++) {
            const cicd_job_t *j = &s->jobs[ji];
            printf("    Job [%s]: %s (%llums)\n",
                   j->name, cicd_job_status_string(j->status),
                   (unsigned long long)j->duration_ms);
            if (j->status == CICD_JOB_SUCCESS) pass++;
            else if (j->status == CICD_JOB_FAILED) fail++;
            else if (j->status == CICD_JOB_SKIPPED) skip++;
        }
    }
    printf("  Total: %d passed, %d failed, %d skipped (%.1f%%)\n",
           pass, fail, skip, cicd_pipeline_success_rate(p));
}

int cicd_retry_failed_jobs(cicd_pipeline_t *p) {
    if (!p) return -1;
    int retried = 0;
    for (int si = 0; si < p->stage_count; si++) {
        cicd_stage_t *s = &p->stages[si];
        for (int ji = 0; ji < s->job_count; ji++) {
            cicd_job_t *j = &s->jobs[ji];
            if (j->status == CICD_JOB_FAILED && j->attempt < j->retry_count) {
                j->status = CICD_JOB_PENDING;
                j->attempt++;
                retried++;
            }
        }
    }
    return retried;
}
