#ifndef AI_PLATFORM_H
#define AI_PLATFORM_H
#include <stdbool.h>
#include <stdint.h>

#define AI_MAX_EXPERIMENTS 32
#define AI_MAX_METRICS     16

typedef enum { AI_STAGE_IDLE, AI_STAGE_DATA, AI_STAGE_TRAIN, AI_STAGE_EVAL, AI_STAGE_DEPLOY, AI_STAGE_MONITOR } AIPipelineStage;

typedef struct {
    char name[64]; double value; uint64_t timestamp; bool is_validation;
} AIMetric;

typedef struct {
    char experiment_id[64]; AIPipelineStage stage;
    AIMetric metrics[AI_MAX_METRICS]; uint32_t metric_count;
    double best_val_loss; uint32_t epochs_run, early_stop_patience;
} AIExperiment;

typedef struct { uint32_t total, correct; double accuracy, precision, recall, f1; } AIClassificationMetrics;

void ai_experiment_init(AIExperiment *exp, const char *id);
void ai_experiment_log_metric(AIExperiment *exp, const char *name, double value, bool is_val);
bool ai_early_stop_check(AIExperiment *exp, uint32_t patience, double min_delta);
void ai_classification_metrics_init(AIClassificationMetrics *m);
void ai_classification_update(AIClassificationMetrics *m, uint32_t pred, uint32_t truth);
void ai_classification_finalize(AIClassificationMetrics *m);

/* L9: ML Platform Abstractions (Kubeflow / MLflow / TFX) */
typedef struct {
    char pipeline_name[128]; uint32_t version;
    AIPipelineStage stages[8]; uint32_t stage_count;
    bool auto_retry; uint32_t max_retries;
} AIMLPipeline;

void ai_ml_pipeline_init(AIMLPipeline *p, const char *name);
void ai_ml_pipeline_add_stage(AIMLPipeline *p, AIPipelineStage stage);
void ai_ml_pipeline_run(AIMLPipeline *p);

typedef struct { char name[64]; char uri[256]; char version[32]; char framework[32]; } AIModelRegistry;
void ai_model_registry_register(AIModelRegistry *reg, const char *name, const char *uri, const char *version, const char *fw);
int ai_model_version_cmp(const char *v1, const char *v2);

/* L4: A/B Test — Welch's t-test for unequal variances (Welch, 1947)
 * H0: μ_A = μ_B.  Returns p-value (two-tailed).  Statistically significant if p < α. */
typedef struct { double mean_a, mean_b, t_stat, p_value, cohens_d; bool significant; } ABTestResult;
ABTestResult ai_ab_test(const double *group_a, uint32_t na, const double *group_b, uint32_t nb, double alpha);

typedef struct { double mean, stddev, min, max, p50, p95, p99; } AIStatsSummary;
AIStatsSummary ai_compute_stats(const double *values, uint32_t n);
void ai_print_stats_summary(const AIStatsSummary *s, const char *label);

/* L9: Feature Store */
typedef struct { char name[64]; char entity_id[64]; double default_value; uint64_t ttl_seconds; char source_table[128]; char source_column[64]; } FeatureDef;
typedef struct { char feature_name[64]; double value; uint64_t timestamp; bool valid; } OnlineFeature;
typedef struct { FeatureDef definitions[256]; uint32_t def_count; OnlineFeature online_store[1024]; uint32_t online_count; uint64_t total_served; uint64_t cache_hits; } FeatureStore;
void ai_fs_init(FeatureStore *fs);
void ai_fs_register_feature(FeatureStore *fs, const char *name, const char *entity, double default_val, uint64_t ttl);
double ai_fs_get_online(FeatureStore *fs, const char *entity, const char *feature);
double ai_fs_cache_hit_rate(const FeatureStore *fs);

/* L9: Data Drift Detection */
double ai_drift_psi(const double *ref_dist, const double *prod_dist, uint32_t n_bins);
double ai_drift_ks_test(const double *sample1, uint32_t n1, const double *sample2, uint32_t n2);

/* L9: XAI - Feature Importance */
typedef struct { char feature_name[64]; double importance; double shap_value; } FeatureImportance;
void ai_permutation_importance(double baseline_acc, double *permuted_accs, const char **feature_names, uint32_t n_features, FeatureImportance *results);
void ai_print_feature_importance(const FeatureImportance *results, uint32_t n);

/* L9: Federated Learning */
typedef struct { double *weights; uint32_t num_params; uint32_t num_clients; uint32_t local_epochs; double dp_epsilon; double dp_delta; double noise_multiplier; } FederatedConfig;
typedef struct { double *global_weights; double *client_deltas; uint32_t round; double train_loss; double privacy_spent; } FederatedState;
void ai_fed_init(FederatedConfig *cfg, uint32_t num_params, uint32_t num_clients);
void ai_fed_aggregate(FederatedState *state, const double **client_updates, uint32_t num_clients);

/* L9: NAS */
typedef enum { NAS_OP_CONV3, NAS_OP_CONV5, NAS_OP_MAXPOOL, NAS_OP_AVGPOOL, NAS_OP_SKIP, NAS_OP_NONE } NASOperation;
typedef struct { NASOperation ops[8]; uint32_t num_ops; double validation_accuracy; double flops; uint32_t num_params; } ArchCandidate;
ArchCandidate ai_nas_random_search(uint32_t num_trials);
void ai_nas_print_candidate(const ArchCandidate *cand);

#endif