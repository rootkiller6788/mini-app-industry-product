#include "feature_flags.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char* ff_type_str(FeatureFlagType t) {
    switch (t) {
    case FLAG_BOOLEAN:    return "boolean";
    case FLAG_PERCENTAGE: return "percentage";
    case FLAG_TARGETED:   return "targeted";
    case FLAG_SCHEDULED:  return "scheduled";
    default:              return "???";
    }
}

const char* ff_state_str(FeatureFlagState s) {
    switch (s) {
    case FLAG_OFF:        return "off";
    case FLAG_ON:         return "on";
    case FLAG_RAMPING:    return "ramping";
    case FLAG_FULL:       return "full";
    case FLAG_DEPRECATED: return "deprecated";
    case FLAG_REMOVED:    return "removed";
    default:              return "???";
    }
}

FeatureFlagSystem* ff_system_create(const char *env) {
    FeatureFlagSystem *ffs = (FeatureFlagSystem*)calloc(1, sizeof(FeatureFlagSystem));
    if (!ffs) return NULL;
    ffs->flag_count = 0;
    ffs->initialized = true;
    ffs->version = 1;
    if (env) snprintf(ffs->environment, sizeof(ffs->environment), "%s", env);
    return ffs;
}

void ff_system_destroy(FeatureFlagSystem *ffs) { free(ffs); }

static FeatureFlag* ff_find(FeatureFlagSystem *ffs, const char *name) {
    for (int i = 0; i < ffs->flag_count; i++)
        if (strcmp(ffs->flags[i].name, name) == 0)
            return &ffs->flags[i];
    return NULL;
}

int ff_define_flag(FeatureFlagSystem *ffs, const char *name,
                    const char *desc, FeatureFlagType t) {
    if (!ffs || !name || ffs->flag_count >= MAX_FEATURE_FLAGS) return -1;
    FeatureFlag *f = &ffs->flags[ffs->flag_count];
    memset(f, 0, sizeof(FeatureFlag));
    snprintf(f->name, sizeof(f->name), "%s", name);
    if (desc) snprintf(f->description, sizeof(f->description), "%s", desc);
    f->type = t;
    f->state = FLAG_OFF;
    f->rollout_percentage = 0.0;
    f->enabled = false;
    f->killed = false;
    f->created_at = time(NULL);
    f->updated_at = f->created_at;
    ffs->flag_count++;
    ffs->version++;
    return ffs->flag_count - 1;
}

int ff_set_state(FeatureFlagSystem *ffs, const char *name, FeatureFlagState state) {
    if (!ffs || !name) return -1;
    FeatureFlag *f = ff_find(ffs, name);
    if (!f) return -1;
    f->state = state;
    f->enabled = (state == FLAG_ON || state == FLAG_RAMPING || state == FLAG_FULL);
    f->updated_at = time(NULL);
    ffs->version++;
    return 0;
}

int ff_set_rollout(FeatureFlagSystem *ffs, const char *name, double pct) {
    if (!ffs || !name) return -1;
    FeatureFlag *f = ff_find(ffs, name);
    if (!f) return -1;
    if (pct < 0.0) pct = 0.0;
    if (pct > 100.0) pct = 100.0;
    f->rollout_percentage = pct;
    f->state = (pct >= 100.0) ? FLAG_FULL : FLAG_RAMPING;
    f->enabled = (pct > 0.0);
    f->updated_at = time(NULL);
    return 0;
}

int ff_add_targeting_rule(FeatureFlagSystem *ffs, const char *name,
                           const char *attr_key, const char *attr_val) {
    if (!ffs || !name || !attr_key || !attr_val) return -1;
    FeatureFlag *f = ff_find(ffs, name);
    if (!f || f->rule_count >= MAX_TARGETING_RULES) return -1;
    TargetingRule *tr = &f->rules[f->rule_count];
    memset(tr, 0, sizeof(TargetingRule));
    snprintf(tr->attrs[0].key, MAX_ATTR_KEY_LEN, "%s", attr_key);
    snprintf(tr->attrs[0].value, MAX_ATTR_VAL_LEN, "%s", attr_val);
    tr->attr_count = 1;
    tr->enabled = true;
    tr->rollout_pct = 100.0;
    f->rule_count++;
    f->type = FLAG_TARGETED;
    return f->rule_count - 1;
}

int ff_schedule(FeatureFlagSystem *ffs, const char *name,
                 time_t on_at, time_t off_at) {
    if (!ffs || !name) return -1;
    FeatureFlag *f = ff_find(ffs, name);
    if (!f) return -1;
    f->scheduled_on_at = on_at;
    f->scheduled_off_at = off_at;
    f->type = FLAG_SCHEDULED;
    time_t now = time(NULL);
    f->enabled = (now >= on_at && now < off_at);
    f->updated_at = now;
    return 0;
}

bool ff_is_enabled(FeatureFlagSystem *ffs, const char *name,
                    TargetingAttribute *attrs, int attr_count) {
    if (!ffs || !name) return false;
    FeatureFlag *f = ff_find(ffs, name);
    if (!f || f->killed) return false;
    f->evaluation_count++;
    if (!f->enabled) return false;
    if (f->type == FLAG_SCHEDULED) {
        time_t now = time(NULL);
        if (now < f->scheduled_on_at || now >= f->scheduled_off_at) return false;
    }
    if (f->type == FLAG_PERCENTAGE ||
        (f->state == FLAG_RAMPING && f->rollout_percentage > 0.0)) {
        unsigned int h = 5381;
        for (const char *c = name; *c; c++) h = ((h << 5) + h) + (unsigned char)*c;
        double bucket = (h % 10000) / 100.0;
        if (bucket > f->rollout_percentage) return false;
    }
    if (f->type == FLAG_TARGETED && attrs && f->rule_count > 0) {
        bool matched = false;
        for (int r = 0; r < f->rule_count && !matched; r++)
            for (int a = 0; a < attr_count && !matched; a++)
                for (int ra = 0; ra < f->rules[r].attr_count; ra++)
                    if (strcmp(attrs[a].key, f->rules[r].attrs[ra].key) == 0 &&
                        strcmp(attrs[a].value, f->rules[r].attrs[ra].value) == 0)
                        matched = true;
        if (!matched) return false;
    }
    f->match_count++;
    return true;
}

bool ff_is_killed(const FeatureFlagSystem *ffs, const char *name) {
    if (!ffs || !name) return true;
    FeatureFlag *f = ff_find((FeatureFlagSystem*)ffs, name);
    return f ? f->killed : true;
}

int ff_kill_flag(FeatureFlagSystem *ffs, const char *name) {
    if (!ffs || !name) return -1;
    FeatureFlag *f = ff_find(ffs, name);
    if (!f) return -1;
    f->killed = true;
    f->enabled = false;
    f->state = FLAG_REMOVED;
    f->updated_at = time(NULL);
    return 0;
}

int ff_cleanup_deprecated(FeatureFlagSystem *ffs) {
    if (!ffs) return -1;
    int removed = 0;
    for (int i = ffs->flag_count - 1; i >= 0; i--)
        if (ffs->flags[i].state == FLAG_REMOVED) {
            for (int j = i; j < ffs->flag_count - 1; j++)
                ffs->flags[j] = ffs->flags[j + 1];
            ffs->flag_count--;
            removed++;
        }
    return removed;
}

double ff_rollout_coverage(const FeatureFlagSystem *ffs, const char *name) {
    if (!ffs || !name) return 0.0;
    FeatureFlag *f = ff_find((FeatureFlagSystem*)ffs, name);
    if (!f || f->evaluation_count == 0) return 0.0;
    return 100.0 * f->match_count / f->evaluation_count;
}

int ff_evaluate_all(FeatureFlagSystem *ffs, TargetingAttribute *attrs, int attr_count) {
    if (!ffs) return 0;
    int enabled = 0;
    for (int i = 0; i < ffs->flag_count; i++)
        if (ff_is_enabled(ffs, ffs->flags[i].name, attrs, attr_count)) enabled++;
    return enabled;
}

void ff_dump(const FeatureFlagSystem *ffs, char *buf, int buf_sz) {
    if (!ffs || !buf) return;
    int off = 0;
    off += snprintf(buf + off, buf_sz - off, "%s", "=== Feature Flags ===");
    off += snprintf(buf + off, buf_sz - off, "%s", "\n");
    off += snprintf(buf + off, buf_sz - off,
                    "Environment: %s  Flags: %d  Version: %d\n",
                    ffs->environment, ffs->flag_count, ffs->version);
    for (int i = 0; i < ffs->flag_count; i++) {
        const FeatureFlag *f = &ffs->flags[i];
        off += snprintf(buf + off, buf_sz - off, "  %s: %s/%s%s eval=%d match=%d\n",
                        f->name, ff_type_str(f->type), ff_state_str(f->state),
                        f->killed ? " KILLED" : "", f->evaluation_count, f->match_count);
    }
    if (off < buf_sz) buf[off] = '\0';
}


FeatureFlag* ff_get_flag(FeatureFlagSystem *ffs, const char *name) {
    return ff_find(ffs, name);
}

int ff_get_flag_count_by_state(const FeatureFlagSystem *ffs, FeatureFlagState state) {
    if (!ffs) return 0;
    int count = 0;
    for (int i = 0; i < ffs->flag_count; i++)
        if (ffs->flags[i].state == state) count++;
    return count;
}

int ff_get_enabled_count(const FeatureFlagSystem *ffs) {
    if (!ffs) return 0;
    int count = 0;
    for (int i = 0; i < ffs->flag_count; i++)
        if (ffs->flags[i].enabled && !ffs->flags[i].killed) count++;
    return count;
}

int ff_get_killed_count(const FeatureFlagSystem *ffs) {
    if (!ffs) return 0;
    int count = 0;
    for (int i = 0; i < ffs->flag_count; i++)
        if (ffs->flags[i].killed) count++;
    return count;
}

int ff_bulk_set_state(FeatureFlagSystem *ffs, const char **names, int name_count,
                       FeatureFlagState state) {
    if (!ffs || !names) return -1;
    int updated = 0;
    for (int i = 0; i < name_count; i++)
        if (ff_set_state(ffs, names[i], state) == 0) updated++;
    return updated;
}

int ff_export_config(const FeatureFlagSystem *ffs, char *buf, int buf_sz) {
    if (!ffs || !buf) return -1;
    int off = 0;
    for (int i = 0; i < ffs->flag_count; i++) {
        const FeatureFlag *f = &ffs->flags[i];
        off += snprintf(buf + off, buf_sz - off, "%s=%s:%s:%d\n",
                        f->name, ff_type_str(f->type), ff_state_str(f->state),
                        f->enabled ? 1 : 0);
    }
    if (off < buf_sz) buf[off] = '\0';
    return off;
}

double ff_system_coverage(const FeatureFlagSystem *ffs) {
    if (!ffs || ffs->flag_count == 0) return 0.0;
    int enabled = ff_get_enabled_count(ffs);
    return 100.0 * enabled / ffs->flag_count;
}
