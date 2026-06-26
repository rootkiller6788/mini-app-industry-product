#ifndef FEATURE_FLAGS_H
#define FEATURE_FLAGS_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define MAX_FEATURE_FLAGS      64
#define MAX_FLAG_NAME_LEN      48
#define MAX_FLAG_DESC_LEN     256
#define MAX_TARGETING_RULES    16
#define MAX_RULE_ATTRS         8
#define MAX_ATTR_KEY_LEN       32
#define MAX_ATTR_VAL_LEN       64

typedef enum {
    FLAG_BOOLEAN   = 0,
    FLAG_PERCENTAGE = 1,
    FLAG_TARGETED   = 2,
    FLAG_SCHEDULED  = 3
} FeatureFlagType;

typedef enum {
    FLAG_OFF       = 0,
    FLAG_ON         = 1,
    FLAG_RAMPING    = 2,
    FLAG_FULL       = 3,
    FLAG_DEPRECATED = 4,
    FLAG_REMOVED    = 5
} FeatureFlagState;

typedef struct {
    char key[MAX_ATTR_KEY_LEN];
    char value[MAX_ATTR_VAL_LEN];
} TargetingAttribute;

typedef struct {
    TargetingAttribute attrs[MAX_RULE_ATTRS];
    int  attr_count;
    double rollout_pct;
    bool  enabled;
} TargetingRule;

typedef struct {
    char    name[MAX_FLAG_NAME_LEN];
    char    description[MAX_FLAG_DESC_LEN];
    FeatureFlagType type;
    FeatureFlagState state;
    double  rollout_percentage;
    TargetingRule rules[MAX_TARGETING_RULES];
    int     rule_count;
    time_t  created_at;
    time_t  updated_at;
    time_t  scheduled_on_at;
    time_t  scheduled_off_at;
    bool    enabled;
    bool    killed;
    int     evaluation_count;
    int     match_count;
} FeatureFlag;

typedef struct {
    FeatureFlag flags[MAX_FEATURE_FLAGS];
    int  flag_count;
    char environment[32];
    bool initialized;
    int  version;
} FeatureFlagSystem;

FeatureFlagSystem* ff_system_create(const char *env);
void               ff_system_destroy(FeatureFlagSystem *ffs);
int                ff_define_flag(FeatureFlagSystem *ffs, const char *name,
                                   const char *desc, FeatureFlagType t);
int                ff_set_state(FeatureFlagSystem *ffs, const char *name,
                                 FeatureFlagState state);
int                ff_set_rollout(FeatureFlagSystem *ffs, const char *name,
                                   double pct);
int                ff_add_targeting_rule(FeatureFlagSystem *ffs, const char *name,
                                          const char *attr_key, const char *attr_val);
int                ff_schedule(FeatureFlagSystem *ffs, const char *name,
                                time_t on_at, time_t off_at);
bool               ff_is_enabled(FeatureFlagSystem *ffs, const char *name,
                                  TargetingAttribute *attrs, int attr_count);
bool               ff_is_killed(const FeatureFlagSystem *ffs, const char *name);
int                ff_kill_flag(FeatureFlagSystem *ffs, const char *name);
int                ff_cleanup_deprecated(FeatureFlagSystem *ffs);
double             ff_rollout_coverage(const FeatureFlagSystem *ffs, const char *name);
int                ff_evaluate_all(FeatureFlagSystem *ffs, TargetingAttribute *attrs,
                                    int attr_count);
FeatureFlag*        ff_get_flag(FeatureFlagSystem *ffs, const char *name);
int                 ff_get_flag_count_by_state(const FeatureFlagSystem *ffs, FeatureFlagState state);
int                 ff_get_enabled_count(const FeatureFlagSystem *ffs);
int                 ff_get_killed_count(const FeatureFlagSystem *ffs);
int                 ff_bulk_set_state(FeatureFlagSystem *ffs, const char **names, int name_count,
                                      FeatureFlagState state);
int                 ff_export_config(const FeatureFlagSystem *ffs, char *buf, int buf_sz);
double              ff_system_coverage(const FeatureFlagSystem *ffs);
const char*        ff_type_str(FeatureFlagType t);
const char*        ff_state_str(FeatureFlagState s);
void               ff_dump(const FeatureFlagSystem *ffs, char *buf, int buf_sz);

#endif
