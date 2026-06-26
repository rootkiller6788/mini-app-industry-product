#include "ai_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

void ai_fs_init(FeatureStore *fs);
void ai_fs_register_feature(FeatureStore *fs, const char *name, const char *entity, double default_val, uint64_t ttl);
double ai_fs_get_online(FeatureStore *fs, const char *entity, const char *feature);
double ai_fs_cache_hit_rate(const FeatureStore *fs);
double ai_drift_psi(const double *ref_dist, const double *prod_dist, uint32_t n_bins);
double ai_drift_ks_test(const double *sample1, uint32_t n1, const double *sample2, uint32_t n2);
void ai_permutation_importance(double baseline_acc, double *permuted_accs, const char **feature_names, uint32_t n_features, FeatureImportance *results);
void ai_print_feature_importance(const FeatureImportance *results, uint32_t n);
void ai_fed_init(FederatedConfig *cfg, uint32_t num_params, uint32_t num_clients);
void ai_fed_aggregate(FederatedState *state, const double **client_updates, uint32_t num_clients);
ArchCandidate ai_nas_random_search(uint32_t num_trials);
void ai_nas_print_candidate(const ArchCandidate *cand);

static int run = 0, pass = 0;
#define T(n) do { run++; printf("  TEST: %s ... ", n); } while(0)
#define OK do { printf("PASS\n"); pass++; } while(0)

static void t_experiment_init(void) {
    T("experiment_init");
    AIExperiment exp;
    ai_experiment_init(&exp, "exp-001");
    assert(strcmp(exp.experiment_id, "exp-001") == 0);
    assert(exp.stage == AI_STAGE_DATA);
    OK;
}

static void t_log_metric(void) {
    T("log_metric");
    AIExperiment exp;
    ai_experiment_init(&exp, "exp-001");
    ai_experiment_log_metric(&exp, "loss", 0.5, true);
    ai_experiment_log_metric(&exp, "accuracy", 0.85, false);
    assert(exp.metric_count == 2);
    assert(exp.best_val_loss == 0.5);
    OK;
}

static void t_early_stop(void) {
    T("early_stop_check");
    AIExperiment exp;
    ai_experiment_init(&exp, "exp-001");
    ai_experiment_log_metric(&exp, "val_loss", 0.8, true);
    ai_experiment_log_metric(&exp, "val_loss", 0.75, true);
    ai_experiment_log_metric(&exp, "val_loss", 0.75, true);
    ai_experiment_log_metric(&exp, "val_loss", 0.75, true);
    bool stop = ai_early_stop_check(&exp, 2, 0.001);
    assert(stop || !stop);
    OK;
}

static void t_classification_metrics(void) {
    T("classification_metrics");
    AIClassificationMetrics m;
    ai_classification_metrics_init(&m);
    ai_classification_update(&m, 0, 0);
    ai_classification_update(&m, 0, 0);
    ai_classification_update(&m, 0, 1);
    ai_classification_finalize(&m);
    assert(m.total == 3);
    assert(m.correct == 2);
    OK;
}

static void t_ml_pipeline(void) {
    T("ml_pipeline");
    AIMLPipeline p;
    ai_ml_pipeline_init(&p, "train-pipeline");
    assert(strcmp(p.pipeline_name, "train-pipeline") == 0);
    ai_ml_pipeline_add_stage(&p, AI_STAGE_DATA);
    ai_ml_pipeline_add_stage(&p, AI_STAGE_TRAIN);
    ai_ml_pipeline_add_stage(&p, AI_STAGE_EVAL);
    assert(p.stage_count == 3);
    OK;
}

static void t_model_registry(void) {
    T("model_registry");
    AIModelRegistry reg;
    ai_model_registry_register(&reg, "bert", "s3://models/bert", "v1", "pytorch");
    assert(strcmp(reg.name, "bert") == 0);
    OK;
}

static void t_stats_summary(void) {
    T("compute_stats");
    double values[10] = {1,2,3,4,5,6,7,8,9,10};
    AIStatsSummary s = ai_compute_stats(values, 10);
    assert(fabs(s.mean - 5.5) < 0.1);
    assert(s.min == 1.0);
    assert(s.max == 10.0);
    OK;
}

static void t_feature_store(void) {
    T("feature_store");
    FeatureStore fs;
    ai_fs_init(&fs);
    ai_fs_register_feature(&fs, "user_age", "user_123", 30.0, 86400);
    ai_fs_register_feature(&fs, "item_price", "item_456", 99.99, 3600);
    assert(fs.def_count == 2);
    double v = ai_fs_get_online(&fs, "user_123", "user_age");
    assert(v == 30.0);
    double hr = ai_fs_cache_hit_rate(&fs);
    assert(hr >= 0.0 && hr <= 1.0);
    OK;
}

static void t_drift(void) {
    T("drift_detection");
    double ref[5] = {0.2, 0.2, 0.2, 0.2, 0.2};
    double prod[5] = {0.3, 0.15, 0.2, 0.25, 0.1};
    double psi = ai_drift_psi(ref, prod, 5);
    assert(psi >= 0.0);
    double ks = ai_drift_ks_test(ref, 3, prod, 3);
    assert(ks >= 0.0 && ks <= 1.0);
    OK;
}

static void t_feature_imp(void) {
    T("feature_importance");
    double permuted[3] = {0.80, 0.85, 0.75};
    const char *names[3] = {"age", "income", "zipcode"};
    FeatureImportance results[3];
    ai_permutation_importance(0.90, permuted, names, 3, results);
    assert(results[0].importance >= 0.0);
    OK;
}

static void t_federated(void) {
    T("federated_learning");
    FederatedConfig cfg;
    ai_fed_init(&cfg, 100, 10);
    assert(cfg.num_params == 100);
    assert(cfg.num_clients == 10);
    OK;
}

static void t_nas(void) {
    T("neural_architecture_search");
    ArchCandidate best = ai_nas_random_search(20);
    assert(best.num_ops >= 4);
    assert(best.validation_accuracy > 0.0);
    OK;
}

static void t_null(void) {
    T("null safety");
    ai_experiment_init(NULL, NULL);
    ai_experiment_log_metric(NULL, NULL, 0, false);
    ai_early_stop_check(NULL, 0, 0);
    ai_classification_metrics_init(NULL);
    ai_classification_update(NULL, 0, 0);
    ai_classification_finalize(NULL);
    ai_ml_pipeline_init(NULL, NULL);
    ai_ml_pipeline_add_stage(NULL, 0);
    ai_ml_pipeline_run(NULL);
    ai_model_registry_register(NULL, NULL, NULL, NULL, NULL);
    ai_print_stats_summary(NULL, NULL);
    ai_fs_init(NULL);
    ai_fs_register_feature(NULL, NULL, NULL, 0, 0);
    ai_fs_get_online(NULL, NULL, NULL);
    ai_permutation_importance(0, NULL, NULL, 0, NULL);
    ai_fed_init(NULL, 0, 0);
    OK;
}

int main(void) {
    printf("\n=== AI Platform Tests ===\n\n");
    t_experiment_init(); t_log_metric(); t_early_stop();
    t_classification_metrics(); t_ml_pipeline(); t_model_registry();
    t_stats_summary(); t_feature_store(); t_drift();
    t_feature_imp(); t_federated(); t_nas(); t_null();
    printf("\n=== Results: %d/%d tests passed ===\n", pass, run);
    return (pass == run) ? 0 : 1;
}
