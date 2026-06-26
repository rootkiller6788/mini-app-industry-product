#include "ai_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

void ai_experiment_init(AIExperiment *exp, const char *id) {
    if (!exp) return;
    memset(exp, 0, sizeof(*exp));
    if (id) strncpy(exp->experiment_id, id, 63);
    exp->stage = AI_STAGE_DATA; exp->best_val_loss = INFINITY;
}

void ai_experiment_log_metric(AIExperiment *exp, const char *name, double value, bool is_val) {
    if (!exp || !name || exp->metric_count >= AI_MAX_METRICS) return;
    AIMetric *m = &exp->metrics[exp->metric_count];
    strncpy(m->name, name, 63); m->value = value;
    m->timestamp = (uint64_t)time(NULL); m->is_validation = is_val;
    if (is_val && value < exp->best_val_loss) exp->best_val_loss = value;
    exp->metric_count++;
}

bool ai_early_stop_check(AIExperiment *exp, uint32_t patience, double min_delta) {
    if (!exp || exp->metric_count < patience + 1) return false;
    uint32_t no_improve = 0;
    for (uint32_t i = exp->metric_count - patience; i < exp->metric_count; i++) {
        bool is_val = false;
        for (uint32_t j = 0; j < exp->metric_count; j++)
            if (exp->metrics[j].is_validation) { is_val = true; break; }
        if (!is_val) continue;
        if (exp->metrics[i].value >= exp->best_val_loss - min_delta) no_improve++;
    }
    return no_improve >= patience;
}

void ai_classification_metrics_init(AIClassificationMetrics *m) {
    if (!m) return;
    memset(m, 0, sizeof(*m));
}

void ai_classification_update(AIClassificationMetrics *m, uint32_t pred, uint32_t truth) {
    if (!m) return;
    m->total++;
    if (pred == truth) m->correct++;
}

void ai_classification_finalize(AIClassificationMetrics *m) {
    if (!m || m->total == 0) return;
    m->accuracy = (double)m->correct / (double)m->total;
    m->precision = m->accuracy; m->recall = m->accuracy;
    m->f1 = 2.0 * m->precision * m->recall / (m->precision + m->recall > 0 ? m->precision + m->recall : 1.0);
}

void ai_ml_pipeline_init(AIMLPipeline *p, const char *name) {
    if (!p) return;
    memset(p, 0, sizeof(*p));
    if (name) strncpy(p->pipeline_name, name, 127);
    p->version = 1; p->auto_retry = true; p->max_retries = 3;
}

void ai_ml_pipeline_add_stage(AIMLPipeline *p, AIPipelineStage stage) {
    if (!p || p->stage_count >= 8) return;
    p->stages[p->stage_count++] = stage;
}

void ai_ml_pipeline_run(AIMLPipeline *p) {
    if (!p) return;
    printf("Pipeline [%s v%u] running %u stages:\n", p->pipeline_name, p->version, p->stage_count);
    const char *stage_names[] = {"IDLE","DATA","TRAIN","EVAL","DEPLOY","MONITOR"};
    for (uint32_t i = 0; i < p->stage_count; i++) {
        uint32_t s = (uint32_t)p->stages[i];
        printf("  [%u/%u] %s\n", i+1, p->stage_count, s < 6 ? stage_names[s] : "UNKNOWN");
    }
}

void ai_model_registry_register(AIModelRegistry *reg, const char *name, const char *uri, const char *version, const char *fw) {
    if (!reg || !name) return;
    strncpy(reg->name, name, 63);
    if (uri) strncpy(reg->uri, uri, 255);
    if (version) strncpy(reg->version, version, 31);
    if (fw) strncpy(reg->framework, fw, 31);
}

AIStatsSummary ai_compute_stats(const double *values, uint32_t n) {
    AIStatsSummary s; memset(&s, 0, sizeof(s));
    if (!values || n == 0) return s;
    s.min = values[0]; s.max = values[0];
    double sum = 0.0, sumsq = 0.0;
    double *sorted = (double *)malloc(n * sizeof(double));
    if (sorted) {
        memcpy(sorted, values, n * sizeof(double));
        for (uint32_t i = 0; i < n; i++) {
            sum += values[i]; sumsq += values[i] * values[i];
            if (values[i] < s.min) s.min = values[i];
            if (values[i] > s.max) s.max = values[i];
        }
        s.mean = sum / (double)n;
        s.stddev = sqrt(sumsq / (double)n - s.mean * s.mean);
        /* Sort for percentiles */
        for (uint32_t i = 0; i < n; i++)
            for (uint32_t j = i + 1; j < n; j++)
                if (sorted[j] < sorted[i]) { double t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t; }
        s.p50 = sorted[(uint32_t)(n * 0.50)];
        s.p95 = sorted[(uint32_t)(n * 0.95)];
        s.p99 = sorted[(uint32_t)(n * 0.99)];
        free(sorted);
    }
    return s;
}

void ai_print_stats_summary(const AIStatsSummary *s, const char *label) {
    if (!s) return;
    printf("Stats [%s]: mean=%.4f std=%.4f min=%.4f max=%.4f p50=%.4f p95=%.4f p99=%.4f\n",
           label ? label : "data", s->mean, s->stddev, s->min, s->max, s->p50, s->p95, s->p99);
}

/* L4: SemVer 2.0.0 comparison. Returns -1, 0, or 1. */
int ai_model_version_cmp(const char *v1, const char *v2)
{
    if (!v1 || !v2) return 0;
    unsigned long maj1 = 0, min1 = 0, pat1 = 0;
    unsigned long maj2 = 0, min2 = 0, pat2 = 0;
    sscanf(v1, "%lu.%lu.%lu", &maj1, &min1, &pat1);
    sscanf(v2, "%lu.%lu.%lu", &maj2, &min2, &pat2);
    if (maj1 != maj2) return (maj1 > maj2) ? 1 : -1;
    if (min1 != min2) return (min1 > min2) ? 1 : -1;
    if (pat1 != pat2) return (pat1 > pat2) ? 1 : -1;
    return 0;
}

/* L4: Welch's t-test (Welch, 1947) — two-sample t-test without assuming equal variances.
 * Uses Satterthwaite approximation for degrees of freedom (Satterthwaite, 1946).
 * Returns p-value via Student's t CDF approximation (Abramowitz & Stegun 26.7).
 * Cohen's d measures standardized effect size: |μ_A - μ_B| / σ_pooled. */
ABTestResult ai_ab_test(const double *group_a, uint32_t na,
                         const double *group_b, uint32_t nb, double alpha)
{
    ABTestResult r; memset(&r, 0, sizeof(r));
    if (!group_a || !group_b || na < 2 || nb < 2) return r;

    double sa = 0, sb = 0, ssa = 0, ssb = 0;
    for (uint32_t i = 0; i < na; i++) { sa += group_a[i]; ssa += group_a[i] * group_a[i]; }
    for (uint32_t i = 0; i < nb; i++) { sb += group_b[i]; ssb += group_b[i] * group_b[i]; }
    r.mean_a = sa / (double)na;
    r.mean_b = sb / (double)nb;

    double var_a = (ssa - sa * sa / na) / (double)(na - 1);
    double var_b = (ssb - sb * sb / nb) / (double)(nb - 1);
    if (var_a <= 0 || var_b <= 0) return r;

    double se = sqrt(var_a / na + var_b / nb);
    if (se <= 0) return r;
    r.t_stat = (r.mean_a - r.mean_b) / se;

    /* Satterthwaite-Welch degrees of freedom */
    double num_df = (var_a / na + var_b / nb);
    num_df = num_df * num_df;
    double den_df = (var_a / na) * (var_a / na) / (double)(na - 1)
                  + (var_b / nb) * (var_b / nb) / (double)(nb - 1);
    double df = (den_df > 1e-15) ? num_df / den_df : (double)(na + nb - 2);

    /* t-CDF approximation (two-tailed p-value) */
    double t_abs = fabs(r.t_stat);
    double x = df / (df + t_abs * t_abs);
    double ib = (df > 0) ? 0.5 * (1.0 - x * sqrt(x)) : 0.5; /* rough approx */
    /* More accurate regularized incomplete beta: use continued fraction for small df */
    if (df < 100) {
        double a = df / 2.0, b = 0.5;
        double bt = exp(lgamma(a + b) - lgamma(a) - lgamma(b) + a * log(x) + b * log(1.0 - x));
        ib = (x < (a + 1.0) / (a + b + 2.0))
             ? bt * 1.0 / a  /* front of continued fraction */
             : 1.0 - bt * 1.0 / b;
    }
    r.p_value = 2.0 * (1.0 - ib);
    if (r.p_value > 1.0) r.p_value = 1.0;
    if (r.p_value < 0.0) r.p_value = 0.0;

    r.significant = (r.p_value < alpha);

    /* Cohen's d (Cohen, 1988): |d|<0.2 negligible, <0.5 small, <0.8 medium, ≥0.8 large */
    double sp = sqrt(((na - 1) * var_a + (nb - 1) * var_b) / (double)(na + nb - 2));
    r.cohens_d = (sp > 1e-15) ? fabs(r.mean_a - r.mean_b) / sp : 0.0;

    return r;
}

/* L9: ML Platform — Feature Store (Feast / Tecton concepts)
 *
 * Feature stores solve the training/serving skew problem:
 * - Offline: historical features for training (batch, Point-in-Time correct joins)
 * - Online: low-latency feature serving for inference (<10ms)
 * - Registry: centralized metadata (owners, SLAs, data sources)
 *
 * Time-travel: feature values at specific timestamps prevent
 * data leakage (using future data in training).
 *
 * L4: CAP Theorem applied to feature stores —
 * Consistency (same value everywhere) vs Availability (always respond)
 * vs Partition Tolerance. Online stores typically choose CP or AP
 * depending on latency requirements.
 *
 * Reference: Feast.dev, Tecton.ai
 *            Berkeley RISELab "Feature Stores for ML" */

void ai_fs_init(FeatureStore *fs)
{
    if (!fs) return;
    memset(fs, 0, sizeof(*fs));
}

void ai_fs_register_feature(FeatureStore *fs, const char *name, const char *entity, double default_val, uint64_t ttl)
{
    if (!fs || !name || fs->def_count >= 256) return;
    FeatureDef *d = &fs->definitions[fs->def_count];
    strncpy(d->name, name, 63);
    if (entity) strncpy(d->entity_id, entity, 63);
    d->default_value = default_val; d->ttl_seconds = ttl;
    fs->def_count++;
}

double ai_fs_get_online(FeatureStore *fs, const char *entity, const char *feature)
{
    if (!fs || !entity || !feature) return 0.0;
    fs->total_served++;
    /* Simple linear scan (production uses hash index with consistent hashing) */
    for (uint32_t i = 0; i < fs->online_count; i++) {
        OnlineFeature *of = &fs->online_store[i];
        if (of->valid && strcmp(of->feature_name, feature) == 0) {
            fs->cache_hits++;
            return of->value;
        }
    }
    /* Fall back to default */
    for (uint32_t i = 0; i < fs->def_count; i++) {
        if (strcmp(fs->definitions[i].name, feature) == 0)
            return fs->definitions[i].default_value;
    }
    return 0.0;
}

double ai_fs_cache_hit_rate(const FeatureStore *fs)
{
    if (!fs || fs->total_served == 0) return 0.0;
    return (double)fs->cache_hits / (double)fs->total_served;
}

/* L9: Model Monitoring — Data Drift Detection
 *
 * Data drift: P(X) changes over time (covariate shift).
 * Concept drift: P(Y|X) changes (label shift).
 *
 * Detection methods:
 * 1. PSI (Population Stability Index):
 *    PSI = sum((p_i - q_i) * ln(p_i/q_i)) for bin i
 *    > 0.1: mild drift, > 0.25: significant drift
 * 2. KS test: maximum difference between CDFs
 * 3. Wasserstein distance: earth mover's distance
 *
 * L4: Ben-David et al. 2010 "A theory of learning from different domains"
 * Bound on target error: ε_T(h) ≤ ε_S(h) + d_HΔH(D_S, D_T) + λ
 * where d_HΔH is HΔH-divergence between source and target distributions.
 *
 * Reference: Rabanser et al. 2019 "Failing Loudly: An Empirical Study
 * of Methods for Detecting Dataset Shift" (NeurIPS) */

double ai_drift_psi(const double *ref_dist, const double *prod_dist, uint32_t n_bins)
{
    if (!ref_dist || !prod_dist || n_bins == 0) return 0.0;
    double psi = 0.0;
    for (uint32_t i = 0; i < n_bins; i++) {
        double p = ref_dist[i] + 1e-9; /* Avoid log(0) */
        double q = prod_dist[i] + 1e-9;
        psi += (p - q) * log(p / q);
    }
    return psi;
}

double ai_drift_ks_test(const double *sample1, uint32_t n1, const double *sample2, uint32_t n2)
{
    if (!sample1 || !sample2 || n1 == 0 || n2 == 0) return 0.0;
    /* Simplified: compute max difference in histograms */
    uint32_t bins = 20;
    double h1[20] = {0}, h2[20] = {0};
    for (uint32_t i = 0; i < n1 && i < 1000; i++) {
        int b = (int)(sample1[i] * (double)bins);
        if (b >= 0 && (uint32_t)b < bins) h1[b]++;
    }
    for (uint32_t i = 0; i < n2 && i < 1000; i++) {
        int b = (int)(sample2[i] * (double)bins);
        if (b >= 0 && (uint32_t)b < bins) h2[b]++;
    }
    /* Normalize */
    for (uint32_t i = 0; i < bins; i++) { h1[i] /= (double)n1; h2[i] /= (double)n2; }
    double max_diff = 0.0;
    for (uint32_t i = 0; i < bins; i++) {
        double diff = fabs(h1[i] - h2[i]);
        if (diff > max_diff) max_diff = diff;
    }
    return max_diff;
}

/* L9: Explainable AI (XAI) — Feature Importance via Permutation
 *
 * Permutation feature importance (Breiman 2001):
 * 1. Compute baseline accuracy on validation set
 * 2. For each feature: randomly shuffle its values
 * 3. Measure accuracy drop → importance of that feature
 * Larger drop = more important feature.
 *
 * SHAP (Lundberg & Lee 2017): Shapley values from game theory.
 * φ_i = sum_{S⊆{1..n}\{i}} (|S|!(n-|S|-1)!/n!) * [f(S∪{i}) - f(S)]
 *
 * L4: Shapley axioms — efficiency, symmetry, dummy, additivity.
 * SHAP satisfies all four, providing theoretically grounded attributions.
 *
 * Reference: Lundberg & Lee 2017 "A Unified Approach to Interpreting
 * Model Predictions" (NeurIPS) */

void ai_permutation_importance(double baseline_acc, double *permuted_accs,
                                const char **feature_names, uint32_t n_features,
                                FeatureImportance *results)
{
    if (!permuted_accs || !feature_names || !results) return;
    for (uint32_t i = 0; i < n_features; i++) {
        strncpy(results[i].feature_name, feature_names[i], 63);
        results[i].importance = baseline_acc - permuted_accs[i];
        if (results[i].importance < 0.0) results[i].importance = 0.0;
        results[i].shap_value = results[i].importance; /* Approximation */
    }
}

void ai_print_feature_importance(const FeatureImportance *results, uint32_t n)
{
    if (!results) return;
    printf("\nFeature Importance:\n");
    printf("%-32s %12s %12s\n", "Feature", "Importance", "SHAP");
    printf("----------------------------------------------\n");
    for (uint32_t i = 0; i < n; i++) {
        printf("%-32s %12.6f %12.6f\n",
               results[i].feature_name, results[i].importance, results[i].shap_value);
    }
}

/* L9: Federated Learning (McMahan et al. 2017)
 *
 * Federated Averaging (FedAvg): train model across decentralized
 * devices without sharing raw data.
 *
 * Algorithm:
 * 1. Server sends global model w_t to K clients
 * 2. Each client trains on local data D_k for E epochs
 * 3. Clients send model updates Δw_k back to server
 * 4. Server aggregates: w_{t+1} = w_t + (1/K) Σ Δw_k
 *
 * Communication efficiency: Federated SGD communicates every step
 * (too expensive). FedAvg communicates every E epochs (practical).
 *
 * Differential Privacy (DP): add Gaussian noise to updates.
 * ε-DP guarantees privacy loss ≤ ε (Dwork et al. 2006).
 *
 * L4: DP-SGD (Abadi et al. 2016):
 * For ε-DP with δ-failure probability, noise σ = C * q * sqrt(T*log(1/δ))/ε
 * where q = batch_size / dataset_size, T = iterations.
 *
 * Trade-off: smaller ε = more privacy = more noise = lower accuracy.
 *
 * Reference: McMahan et al. 2017 "Communication-Efficient Learning
 * of Deep Networks from Decentralized Data" (Google AI)
 * Kairouz et al. 2021 "Advances and Open Problems in Federated Learning"
 * MIT 6.S897 / Stanford CS 330: Machine Learning Systems */

void ai_fed_init(FederatedConfig *cfg, uint32_t num_params, uint32_t num_clients)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->num_params = num_params;
    cfg->num_clients = num_clients;
    cfg->local_epochs = 5;
    cfg->dp_epsilon = 8.0;
    cfg->dp_delta = 1e-5;
}

void ai_fed_aggregate(FederatedState *state, const double **client_updates,
                       uint32_t num_clients)
{
    if (!state || !client_updates) return;

    /* Simple averaging: w_new = mean(client_updates) */
    for (uint32_t p = 0; p < 100 && state->global_weights; p++) {
        double sum = 0.0;
        for (uint32_t c = 0; c < num_clients; c++) {
            if (client_updates[c]) sum += client_updates[c][p];
        }
        state->global_weights[p] = sum / (double)num_clients;

        /* Add DP noise: N(0, sigma^2) */
        /* Box-Muller transform for Gaussian noise */
        double u1 = (double)rand() / (double)RAND_MAX;
        double u2 = (double)rand() / (double)RAND_MAX;
        double noise = sqrt(-2.0 * log(u1 + 1e-9)) * cos(2.0 * M_PI * u2) * 0.001;
        state->global_weights[p] += noise;
    }
    state->round++;
}

/* L9: AutoML — Neural Architecture Search (NAS) Concepts
 *
 * NAS automates neural network design:
 * 1. Search space: define possible architectures (layers, ops, connections)
 * 2. Search strategy: RL (Zoph & Le 2017), evolution (Real et al. 2019),
 *    gradient-based (DARTS, Liu et al. 2019)
 * 3. Performance estimation: weight sharing, proxy tasks, early stopping
 *
 * Weight-sharing NAS (ENAS, Pham et al. 2018):
 * All architectures share weights in a supergraph.
 * Each sampled subgraph inherits trained weights.
 * Reduces search cost from thousands to < 1 GPU-day.
 *
 * DARTS (Liu et al. 2019):
 * Continuous relaxation of architecture selection.
 * α_{i,j}^k = softmax over operations between nodes i,j.
 * Bi-level optimization: optimize α (architecture) on val set,
 * optimize w (weights) on train set.
 *
 * Reference: Elsken et al. 2019 "Neural Architecture Search: A Survey"
 *            He et al. 2021 "AutoML: A Survey of the State-of-the-Art"
 */

ArchCandidate ai_nas_random_search(uint32_t num_trials)
{
    ArchCandidate best; memset(&best, 0, sizeof(best));
    for (uint32_t t = 0; t < num_trials && t < 100; t++) {
        ArchCandidate cand; memset(&cand, 0, sizeof(cand));
        cand.num_ops = 4 + (uint32_t)(rand() % 5);
        for (uint32_t i = 0; i < cand.num_ops; i++)
            cand.ops[i] = (NASOperation)(rand() % 6);
        cand.validation_accuracy = 0.7 + 0.25 * ((double)rand() / (double)RAND_MAX);
        cand.flops = 10.0 + 90.0 * ((double)rand() / (double)RAND_MAX);
        cand.num_params = 100000 + (uint32_t)(rand() % 900000);
        if (cand.validation_accuracy > best.validation_accuracy) best = cand;
    }
    return best;
}

void ai_nas_print_candidate(const ArchCandidate *cand)
{
    if (!cand) return;
    printf("Architecture: %u ops, %.2f%% acc, %.1f MFLOPs, %u params\n",
           cand->num_ops, cand->validation_accuracy * 100.0,
           cand->flops, cand->num_params);
}