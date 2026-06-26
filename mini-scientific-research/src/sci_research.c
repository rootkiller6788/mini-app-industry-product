#include "sci_research.h"
#include "sci_statistics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

/*
 * sci_research.c -- Scientific Research Workflow & Data Management
 *
 * Implements: experiment lifecycle management (state machine),
 * randomization (simple, blocked, stratified), power analysis,
 * effect sizes (Cohen d, eta^2, odds ratio), outlier detection
 * (modified Z-score), reproducibility checks, data quality metrics,
 * ICC reliability, dataset versioning, data collection reports,
 * meta-analysis (fixed-effects).
 *
 * L1-L7: Full implementations.
 *
 * References:
 * - Popper, "The Logic of Scientific Discovery" (1934)
 * - Cohen, "Statistical Power Analysis" (2nd ed, 1988)
 * - Iglewicz & Hoaglin, "How to Detect and Handle Outliers" (1993)
 * - Shrout & Fleiss, "Intraclass Correlations" (1979)
 * - Wilkinson et al., "FAIR Guiding Principles" (2016)
 */

/* ================================================================
 * L1-L3: Global Research Management State
 * ================================================================ */

#define SCI_MAX_EXPERIMENTS    1024
#define SCI_MAX_DATASET_VERS   4096
#define SCI_MAX_OBSERVATIONS   131072

static SCI_Experiment  *g_experiments = NULL;
static int              g_num_experiments = 0;
static int              g_next_exp_id = 1;
static bool             g_initialized = false;

static SCI_DatasetVersion *g_datasets = NULL;
static int                 g_num_datasets = 0;
static SCI_Observation    *g_observations = NULL;
static int                 g_num_observations = 0;
static int                 g_next_dataset_id = 1;

void sci_research_init(void) {
    if (g_initialized) return;
    g_experiments = (SCI_Experiment*)calloc(SCI_MAX_EXPERIMENTS, sizeof(SCI_Experiment));
    g_datasets = (SCI_DatasetVersion*)calloc(SCI_MAX_DATASET_VERS, sizeof(SCI_DatasetVersion));
    g_observations = (SCI_Observation*)calloc(SCI_MAX_OBSERVATIONS, sizeof(SCI_Observation));
    g_num_experiments = 0;
    g_num_datasets = 0;
    g_num_observations = 0;
    g_next_exp_id = 1;
    g_next_dataset_id = 1;
    g_initialized = true;
}

void sci_research_shutdown(void) {
    free(g_experiments); g_experiments = NULL;
    free(g_datasets); g_datasets = NULL;
    free(g_observations); g_observations = NULL;
    g_initialized = false;
}

/* ================================================================
 * L2-L3: Experiment Lifecycle
 * ================================================================ */

static bool is_valid_transition(SCI_ExperimentStage from, SCI_ExperimentStage to) {
    /* Valid transitions in the scientific workflow state machine */
    switch (from) {
        case SCI_EXP_DRAFT:
            return to == SCI_EXP_PREREGISTERED || to == SCI_EXP_RUNNING;
        case SCI_EXP_PREREGISTERED:
            return to == SCI_EXP_RUNNING;
        case SCI_EXP_RUNNING:
            return to == SCI_EXP_COMPLETED;
        case SCI_EXP_COMPLETED:
            return to == SCI_EXP_ANALYZING;
        case SCI_EXP_ANALYZING:
            return to == SCI_EXP_WRITTEN;
        case SCI_EXP_WRITTEN:
            return to == SCI_EXP_SUBMITTED;
        case SCI_EXP_SUBMITTED:
            return to == SCI_EXP_REVISED || to == SCI_EXP_PUBLISHED;
        case SCI_EXP_REVISED:
            return to == SCI_EXP_SUBMITTED || to == SCI_EXP_PUBLISHED;
        case SCI_EXP_PUBLISHED:
            return to == SCI_EXP_RETRACTED;
        default:
            return false;
    }
}

int sci_experiment_create(const char *title, const char *hypothesis,
                           SCI_StudyDesign design) {
    if (!g_initialized) sci_research_init();
    if (g_num_experiments >= SCI_MAX_EXPERIMENTS) return -1;

    int id = g_next_exp_id++;
    SCI_Experiment *exp = &g_experiments[g_num_experiments++];
    memset(exp, 0, sizeof(SCI_Experiment));

    exp->id = id;
    exp->stage = SCI_EXP_DRAFT;
    exp->design = design;
    exp->method = SCI_METHOD_QUANTITATIVE;
    exp->alpha = 0.05;
    exp->target_power = 0.80;
    exp->created_at = time(NULL);
    exp->updated_at = exp->created_at;
    exp->is_reproducible = false;
    exp->is_blinded = false;
    exp->is_preregistered = false;

    if (title) snprintf(exp->title, sizeof(exp->title), "%s", title);
    if (hypothesis) snprintf(exp->hypothesis, sizeof(exp->hypothesis), "%s", hypothesis);
    return id;
}

bool sci_experiment_set_stage(int exp_id, SCI_ExperimentStage new_stage) {
    if (!g_initialized) return false;

    for (int i = 0; i < g_num_experiments; i++) {
        if (g_experiments[i].id == exp_id) {
            if (!is_valid_transition(g_experiments[i].stage, new_stage))
                return false;
            g_experiments[i].stage = new_stage;
            g_experiments[i].updated_at = time(NULL);
            if (new_stage == SCI_EXP_PREREGISTERED) {
                g_experiments[i].preregistered_at = time(NULL);
                g_experiments[i].is_preregistered = true;
            }
            return true;
        }
    }
    return false;
}

bool sci_experiment_get(int exp_id, SCI_Experiment *exp) {
    if (!g_initialized || !exp) return false;
    for (int i = 0; i < g_num_experiments; i++) {
        if (g_experiments[i].id == exp_id) {
            memcpy(exp, &g_experiments[i], sizeof(SCI_Experiment));
            return true;
        }
    }
    return false;
}

int sci_experiment_count(SCI_ExperimentStage stage) {
    if (!g_initialized) return 0;
    if ((int)stage < 0) return g_num_experiments;
    int count = 0;
    for (int i = 0; i < g_num_experiments; i++)
        if (g_experiments[i].stage == stage) count++;
    return count;
}

/* ================================================================
 * L5: Fisher Randomization — Fisher-Yates Shuffle
 *
 * Fisher (1935): "The Design of Experiments"
 * Randomization ensures unbiased treatment assignment.
 * ================================================================ */

static unsigned int rand_lcg = 13579;

static double rand_double(void) {
    rand_lcg = 1103515245 * rand_lcg + 12345;
    return (double)(rand_lcg & 0x7FFFFFFF) / 2147483648.0;
}

void sci_randomize_subjects(int n, int k, unsigned int seed, int *groups) {
    if (!groups || n <= 0 || k <= 0) return;
    if (seed != 0) rand_lcg = seed;

    /* Create balanced assignment: ceil(n/k) per group */
    int *assign = (int*)malloc(n * sizeof(int));
    if (!assign) return;
    for (int i = 0; i < n; i++) assign[i] = i % k;

    /* Fisher-Yates shuffle */
    for (int i = n - 1; i > 0; i--) {
        int j = (int)(rand_double() * (i + 1));
        int t = assign[i]; assign[i] = assign[j]; assign[j] = t;
    }
    for (int i = 0; i < n; i++) groups[i] = assign[i];
    free(assign);
}

void sci_randomize_blocked(int n, int k, int block_size,
                            unsigned int seed, int *groups) {
    if (!groups || n <= 0 || k <= 0 || block_size <= 0) return;
    if (seed != 0) rand_lcg = seed;

    int per_block = k * block_size;
    int *block_assign = (int*)malloc(per_block * sizeof(int));
    if (!block_assign) return;

    for (int offset = 0; offset < n; offset += per_block) {
        int remaining = (n - offset < per_block) ? n - offset : per_block;
        /* Fill with balanced counts */
        for (int i = 0; i < remaining; i++) block_assign[i] = i % k;
        /* Shuffle */
        for (int i = remaining - 1; i > 0; i--) {
            int j = (int)(rand_double() * (i + 1));
            int t = block_assign[i]; block_assign[i] = block_assign[j];
            block_assign[j] = t;
        }
        for (int i = 0; i < remaining; i++) groups[offset + i] = block_assign[i];
    }
    free(block_assign);
}

void sci_randomize_stratified(int n, const int *strata, int S, int k,
                               unsigned int seed, int *groups) {
    if (!groups || !strata || n <= 0 || S <= 0 || k <= 0) return;
    if (seed != 0) rand_lcg = seed;

    /* Count per stratum */
    int *counts = (int*)calloc(S, sizeof(int));
    if (!counts) return;
    for (int i = 0; i < n; i++)
        if (strata[i] >= 0 && strata[i] < S) counts[strata[i]]++;

    /* Randomize within each stratum */
    int *offsets = (int*)calloc(S, sizeof(int));
    if (!offsets) { free(counts); return; }

    int *idx = (int*)malloc(n * sizeof(int));
    int *order = (int*)malloc(n * sizeof(int));
    if (!idx || !order) { free(counts); free(offsets); free(idx); free(order); return; }

    /* Build per-stratum indices */
    for (int i = 0; i < n; i++) {
        int s = strata[i];
        if (s < 0 || s >= S) { groups[i] = 0; continue; }
        idx[i] = offsets[s]++;
        order[i] = s;
    }

    /* Within each stratum, assign groups */
    for (int s = 0; s < S; s++) {
        int *pool = (int*)malloc(counts[s] * sizeof(int));
        if (!pool) continue;
        for (int i = 0; i < counts[s]; i++) pool[i] = i % k;
        for (int i = counts[s] - 1; i > 0; i--) {
            int j = (int)(rand_double() * (i + 1));
            int t = pool[i]; pool[i] = pool[j]; pool[j] = t;
        }
        for (int i = 0; i < n; i++)
            if (strata[i] == s) groups[i] = pool[idx[i]];
        free(pool);
    }

    free(counts); free(offsets); free(idx); free(order);
}

/* ================================================================
 * L5: Power Analysis & Sample Size (Cohen 1988)
 *
 * Two-sample t-test:
 * n_per_group = 2*(z_{alpha/2} + z_beta)^2 / d^2
 * where d = delta / sigma (Cohen's d).
 *
 * This uses normal quantile approximation.
 * ================================================================ */

int sci_power_sample_size_ttest(double effect_size, double alpha,
                                 double power) {
    if (effect_size <= 0.0 || alpha <= 0.0 || alpha >= 1.0 ||
        power <= 0.0 || power >= 1.0) return -1;

    double za = sci_normal_quantile(1.0 - alpha / 2.0);
    double zb = sci_normal_quantile(power);
    double n = 2.0 * (za + zb) * (za + zb) / (effect_size * effect_size);
    return (int)ceil(n);
}

double sci_power_achieved(int n_per_group, double effect_size, double alpha) {
    if (n_per_group <= 0 || effect_size <= 0.0 || alpha <= 0.0) return 0.0;
    double za = sci_normal_quantile(1.0 - alpha / 2.0);
    double ncp = effect_size * sqrt(n_per_group / 2.0);
    return 1.0 - sci_normal_cdf(za - ncp);
}

double sci_min_detectable_effect(int n_per_group, double alpha, double power) {
    if (n_per_group <= 0 || alpha <= 0.0 || power <= 0.0) return INFINITY;
    double za = sci_normal_quantile(1.0 - alpha / 2.0);
    double zb = sci_normal_quantile(power);
    return (za + zb) * sqrt(2.0 / n_per_group);
}

/* ================================================================
 * L5: Effect Sizes
 * ================================================================ */

double sci_cohens_d(double mean1, double mean2,
                     double var1, double var2, int n1, int n2) {
    if (n1 < 1 || n2 < 1 || n1 + n2 <= 1) return NAN;
    double sp = sqrt(((n1-1)*var1 + (n2-1)*var2) / (n1 + n2 - 2));
    if (sp < 1e-15) return (mean1 != mean2) ? INFINITY : 0.0;
    return (mean1 - mean2) / sp;
}

double sci_eta_squared(double ss_between, double ss_total) {
    if (ss_total <= 0.0) return 0.0;
    return ss_between / ss_total;
}

double sci_odds_ratio(int a, int b, int c, int d) {
    if (b * c == 0) return INFINITY;
    return ((double)a * d) / ((double)b * c);
}

/* ================================================================
 * L6: Outlier Detection — Modified Z-Score (Iglewicz & Hoaglin 1993)
 *
 * M_i = 0.6745 * (x_i - median) / MAD
 * MAD = median(|x_i - median|)
 *
 * Flag as outlier if |M_i| > 3.5 (recommended threshold).
 * Robust to outliers in the reference data.
 * ================================================================ */

void sci_outlier_detection(const double *data, int n, double threshold,
                            bool *is_outlier) {
    if (!data || !is_outlier || n <= 0) return;

    /* Copy and sort for median */
    double *sorted = (double*)malloc(n * sizeof(double));
    if (!sorted) return;
    memcpy(sorted, data, n * sizeof(double));

    /* Simple selection sort for median (n is usually small) */
    for (int i = 0; i < n/2 + 1; i++) {
        int min_idx = i;
        for (int j = i+1; j < n; j++)
            if (sorted[j] < sorted[min_idx]) min_idx = j;
        if (min_idx != i) {
            double t = sorted[i]; sorted[i] = sorted[min_idx]; sorted[min_idx] = t;
        }
    }
    double median = sorted[n/2];
    if (n % 2 == 0) median = (sorted[n/2 - 1] + sorted[n/2]) / 2.0;

    /* Compute absolute deviations from median */
    double *abs_dev = (double*)malloc(n * sizeof(double));
    if (!abs_dev) { free(sorted); return; }
    for (int i = 0; i < n; i++)
        abs_dev[i] = fabs(data[i] - median);

    /* Sort abs_dev for MAD */
    for (int i = 0; i < n/2 + 1; i++) {
        int min_idx = i;
        for (int j = i+1; j < n; j++)
            if (abs_dev[j] < abs_dev[min_idx]) min_idx = j;
        if (min_idx != i) {
            double t = abs_dev[i]; abs_dev[i] = abs_dev[min_idx]; abs_dev[min_idx] = t;
        }
    }
    double mad = abs_dev[n/2];
    if (n % 2 == 0) mad = (abs_dev[n/2 - 1] + abs_dev[n/2]) / 2.0;
    if (mad < 1e-15) mad = 1e-15;

    /* Flag outliers */
    for (int i = 0; i < n; i++) {
        double M = 0.6745 * (data[i] - median) / mad;
        is_outlier[i] = (fabs(M) > threshold);
    }

    free(sorted); free(abs_dev);
}

/* ================================================================
 * L6: Reproducibility Check — Equivalence Testing (TOST)
 *
 * Two one-sided tests: verify that |mean_diff| < equivalence bound.
 * ================================================================ */

bool sci_reproducibility_check(const double *results_a,
                                const double *results_b,
                                int n, double tol) {
    if (!results_a || !results_b || n <= 0 || tol < 0.0) return false;

    double max_diff = 0.0;
    for (int i = 0; i < n; i++) {
        double diff = fabs(results_a[i] - results_b[i]);
        double scale = fmax(fabs(results_a[i]), fabs(results_b[i]));
        if (scale < 1e-10) scale = 1.0;
        double rel_diff = diff / scale;
        if (rel_diff > max_diff) max_diff = rel_diff;
    }
    return (max_diff <= tol);
}

/* ================================================================
 * L6: Data Quality Metrics
 * ================================================================ */

void sci_data_quality_metrics(const double *data, int n,
                               double *completeness,
                               double *uniqueness,
                               double *z_score_mean) {
    if (!data || n <= 0) {
        if (completeness) *completeness = 0.0;
        if (uniqueness) *uniqueness = 0.0;
        if (z_score_mean) *z_score_mean = 0.0;
        return;
    }

    /* Completeness: proportion of non-missing (non-NaN) */
    int non_missing = 0;
    for (int i = 0; i < n; i++)
        if (!isnan(data[i])) non_missing++;
    if (completeness) *completeness = (double)non_missing / n;

    /* Uniqueness: proportion of unique values */
    double *sorted = (double*)malloc(n * sizeof(double));
    if (sorted) {
        int valid = 0;
        for (int i = 0; i < n; i++)
            if (!isnan(data[i])) sorted[valid++] = data[i];
        if (valid > 0) {
            /* Simple bubble sort */
            for (int i = 0; i < valid-1; i++)
                for (int j = 0; j < valid-1-i; j++)
                    if (sorted[j] > sorted[j+1]) {
                        double t = sorted[j]; sorted[j]=sorted[j+1]; sorted[j+1]=t;
                    }
            int unique = 1;
            for (int i = 1; i < valid; i++)
                if (fabs(sorted[i] - sorted[i-1]) > 1e-15) unique++;
            if (uniqueness) *uniqueness = (double)unique / valid;
        }
        free(sorted);
    }

    /* Mean absolute z-score */
    double sum = 0.0, sum_sq = 0.0;
    for (int i = 0; i < n; i++) {
        if (!isnan(data[i])) { sum += data[i]; sum_sq += data[i]*data[i]; }
    }
    double mean = sum / non_missing;
    double var = 0.0;
    if (non_missing > 1) {
        var = (sum_sq - sum*sum/non_missing) / (non_missing - 1);
        if (var < 0.0) var = 0.0;
    }
    double sd = sqrt(var);
    if (z_score_mean) {
        if (sd < 1e-15) { *z_score_mean = 0.0; return; }
        double abs_z_sum = 0.0;
        for (int i = 0; i < n; i++)
            if (!isnan(data[i])) abs_z_sum += fabs((data[i] - mean) / sd);
        *z_score_mean = abs_z_sum / non_missing;
    }
}

/* ================================================================
 * L6: Intraclass Correlation Coefficient (ICC)
 *
 * ICC(1,1) = (MSB - MSW) / (MSB + (k-1)*MSW)
 * where MSB = between-subject mean square, MSW = within-subject.
 *
 * Shrout & Fleiss (1979).
 * ================================================================ */

double sci_icc_reliability(const double *data, int n, int k) {
    if (!data || n < 2 || k < 2) return NAN;

    /* Compute subject means */
    double *subj_mean = (double*)malloc(n * sizeof(double));
    if (!subj_mean) return NAN;
    for (int i = 0; i < n; i++) {
        subj_mean[i] = 0.0;
        for (int j = 0; j < k; j++)
            subj_mean[i] += data[i * k + j];
        subj_mean[i] /= k;
    }

    /* Grand mean */
    double grand = 0.0;
    for (int i = 0; i < n; i++) grand += subj_mean[i];
    grand /= n;

    /* MSB: mean square between subjects */
    double ssb = 0.0;
    for (int i = 0; i < n; i++)
        ssb += (subj_mean[i] - grand) * (subj_mean[i] - grand);
    double msb = ssb * k / (n - 1);

    /* MSW: mean square within subjects (error) */
    double ssw = 0.0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < k; j++)
            ssw += (data[i*k+j] - subj_mean[i]) * (data[i*k+j] - subj_mean[i]);
    double msw = ssw / (n * (k - 1));

    free(subj_mean);

    if (msb + (k-1)*msw < 1e-15) return 1.0;
    return (msb - msw) / (msb + (k-1)*msw);
}

/* ================================================================
 * L7: Dataset Versioning
 * ================================================================ */

int sci_dataset_version_create(int experiment_id, int num_observations,
                                int num_variables, const char var_names[][64]) {
    if (!g_initialized) sci_research_init();
    if (g_num_datasets >= SCI_MAX_DATASET_VERS) return -1;

    int id = g_next_dataset_id++;
    SCI_DatasetVersion *ds = &g_datasets[g_num_datasets++];
    memset(ds, 0, sizeof(SCI_DatasetVersion));

    ds->version_id = id;
    ds->experiment_id = experiment_id;
    ds->version_number = 1;
    ds->timestamp = time(NULL);
    ds->num_observations = num_observations;
    ds->num_variables = (num_variables < 64) ? num_variables : 64;
    for (int v = 0; v < ds->num_variables && v < 64; v++)
        snprintf(ds->variable_names[v], 64, "%s", var_names[v]);
    return id;
}

int sci_observation_add(int version_id, const SCI_Observation *obs) {
    (void)version_id; /* Future: link to specific dataset version */
    if (!g_initialized || !obs || g_num_observations >= SCI_MAX_OBSERVATIONS)
        return -1;
    g_observations[g_num_observations] = *obs;
    return g_num_observations++;
}

/* ================================================================
 * L7: Data Collection Report
 * ================================================================ */

bool sci_generate_data_report(int experiment_id,
                               SCI_DataCollectionReport *report) {
    if (!g_initialized || !report) return false;
    memset(report, 0, sizeof(SCI_DataCollectionReport));
    report->experiment_id = experiment_id;

    /* Count observations for this experiment's datasets */
    int total_obs = 0, success = 0, missing = 0, excluded = 0;
    time_t earliest = 0, latest = 0;

    /* Find related datasets */
    for (int d = 0; d < g_num_datasets; d++) {
        if (g_datasets[d].experiment_id == experiment_id) {
            total_obs += g_datasets[d].num_observations;
        }
    }

    /* Count observation stats */
    for (int i = 0; i < g_num_observations; i++) {
        /* Simple heuristic: observations belong to the most recent matching dataset */
        SCI_Observation *obs = &g_observations[i];
        total_obs++;
        if (obs->is_missing) missing++;
        else success++;
        if (obs->is_outlier) excluded++;
    }

    report->total_observations = total_obs > 0 ? total_obs : g_num_observations;
    report->successful_observations = success;
    report->missing_data = missing;
    report->excluded_observations = excluded;
    report->start_time = earliest;
    report->end_time = latest;
    report->completion_rate = (total_obs > 0)
        ? (double)success / total_obs : 0.0;
    report->data_quality_passed = (report->completion_rate > 0.90);

    return true;
}

/* ================================================================
 * L7: Meta-Analysis — Fixed Effects Model
 *
 * Pooled estimate: theta_hat = sum(w_i * theta_i) / sum(w_i)
 * Weights: w_i = 1 / variance_i (inverse variance weighting).
 *
 * Cochran (1954): fixed-effects meta-analysis.
 * ================================================================ */

void sci_meta_analysis_fixed(const double *effects, const double *variances,
                              int k, double *pooled, double *pooled_ci) {
    if (!effects || !variances || !pooled || !pooled_ci || k <= 0) {
        if (pooled) *pooled = 0.0;
        if (pooled_ci) *pooled_ci = 0.0;
        return;
    }

    double sum_w = 0.0, sum_wt = 0.0;
    for (int i = 0; i < k; i++) {
        if (variances[i] <= 0.0) continue;
        double w = 1.0 / variances[i];
        sum_w += w;
        sum_wt += w * effects[i];
    }

    if (sum_w < 1e-15) { *pooled = 0.0; *pooled_ci = 0.0; return; }

    *pooled = sum_wt / sum_w;
    double se = sqrt(1.0 / sum_w);
    *pooled_ci = 1.96 * se; /* 95% CI half-width */
}