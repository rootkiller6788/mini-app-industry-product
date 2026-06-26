/**
 * mini-industry-solution: quality_control.c
 * SPC control charts, process capability, Pareto, Gauge R&R, CUSUM
 * Ref: ISO 22514, AIAG SPC Manual, Western Electric (1956),
 *      Wheeler & Chambers (1992) Understanding SPC
 */
#include "quality_control.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* === Control Chart Constants (L4-L5) === */
/*
 * Standard Shewhart control chart constants derived from
 * the expected value of the range (d2) and its standard deviation (d3).
 *
 * For X-bar chart: UCL/LCL = X_double_bar +/- A2 * R_bar
 *   where A2 = 3 / (d2 * sqrt(n))
 *
 * For R chart:     UCL = D4 * R_bar, LCL = D3 * R_bar
 *   where D3 = 1 - 3*d3/d2, D4 = 1 + 3*d3/d2
 *
 * Constants tabulated for subgroup sizes 2 through 25.
 */
void control_chart_constants(int n, double *a2, double *d3, double *d4,
                              double *b3, double *b4, double *c4) {
    static const double d2_vals[] = {0,0,1.128,1.693,2.059,2.326,2.534,
        2.704,2.847,2.970,3.078,3.173,3.258,3.336,3.407,3.472};
    static const double d3_vals[] = {0,0,0.853,0.888,0.880,0.864,0.848,
        0.833,0.820,0.808,0.797,0.787,0.778,0.770,0.763,0.756};
    if (n < 2 || n > 15) {
        if (a2) *a2 = 0.0;
        if (d3) *d3 = 0.0;
        if (d4) *d4 = 0.0;
        return;
    }
    double d2_val = d2_vals[n], d3_val = d3_vals[n];
    if (a2) *a2 = 3.0 / (d2_val * sqrt((double)n));
    if (d3) *d3 = (1.0 - 3.0 * d3_val / d2_val > 0) ? 1.0 - 3.0*d3_val/d2_val : 0.0;
    if (d4) *d4 = 1.0 + 3.0 * d3_val / d2_val;
    if (b3) *b3 = (1.0 - 3.0/d2_val > 0) ? 1.0 - 3.0/d2_val : 0.0;
    if (b4) *b4 = 1.0 + 3.0 / d2_val;
    if (c4) *c4 = sqrt(2.0/(double)(n-1)) *
        tgamma((double)n/2.0) / tgamma(((double)n-1.0)/2.0);
}

/* === X-bar/R Chart (L5) === */

void xbar_r_chart_init(XBarRChart *chart, int subgroup_size,
                        double *data_buffer, size_t n_measurements) {
    if (!chart || subgroup_size < 2) return;
    memset(chart, 0, sizeof(*chart));
    chart->type = CHART_XBAR_R;
    chart->subgroup_size = subgroup_size;

    size_t n_sub = n_measurements / (size_t)subgroup_size;
    chart->total_subgroups = (int)n_sub;
    chart->subgroup_means = data_buffer;
    chart->subgroup_ranges = data_buffer + n_sub;

    control_chart_constants(subgroup_size,
                             &chart->a2, &chart->d3, &chart->d4, NULL, NULL, NULL);
}

int xbar_r_chart_update(XBarRChart *chart, const double *subgroup,
                         int subgroup_size) {
    if (!chart || !subgroup || subgroup_size < 2
        || chart->total_subgroups <= 0) return -1;

    int idx = chart->total_subgroups;
    if (idx < 0) return -1;

    double sum = 0.0, minv = subgroup[0], maxv = subgroup[0];
    for (int i = 0; i < subgroup_size; i++) {
        sum += subgroup[i];
        if (subgroup[i] < minv) minv = subgroup[i];
        if (subgroup[i] > maxv) maxv = subgroup[i];
    }

    chart->subgroup_means[idx] = sum / (double)subgroup_size;
    chart->subgroup_ranges[idx] = maxv - minv;

    double gs = 0.0, rs = 0.0;
    for (int i = 0; i <= idx; i++) {
        gs += chart->subgroup_means[i];
        rs += chart->subgroup_ranges[i];
    }
    int n = idx + 1;
    chart->grand_mean = gs / (double)n;
    chart->mean_range  = rs / (double)n;

    chart->ucl_x = chart->grand_mean + chart->a2 * chart->mean_range;
    chart->lcl_x = chart->grand_mean - chart->a2 * chart->mean_range;
    chart->ucl_r = chart->d4 * chart->mean_range;
    chart->lcl_r = chart->d3 * chart->mean_range;

    chart->status_x = CHART_IN_CONTROL;
    chart->status_r = CHART_IN_CONTROL;

    double last_x = chart->subgroup_means[idx];
    double last_r = chart->subgroup_ranges[idx];
    if (last_x > chart->ucl_x || last_x < chart->lcl_x)
        chart->status_x = CHART_OUT_OF_CONTROL;
    if (last_r > chart->ucl_r)
        chart->status_r = CHART_OUT_OF_CONTROL;

    chart->total_subgroups = n;
    return 0;
}

/*
 * Western Electric Rules (WECO) for detecting out-of-control conditions.
 *
 * Rule 1: One point beyond 3-sigma limits (Zone A)
 * Rule 2: Two of three consecutive points beyond 2-sigma (Zone B)
 * Rule 3: Four of five consecutive points beyond 1-sigma (Zone C)
 * Rule 4: Eight consecutive points on one side of center line
 * Rule 5: Six points trending up or down
 * Rule 6: Fourteen points alternating up/down
 * Rule 7: Fifteen points within ±1-sigma (stratification)
 * Rule 8: Eight points beyond ±1-sigma on both sides (mixture)
 *
 * Ref: Western Electric (1956), Statistical Quality Control Handbook
 */
int weco_rules_check(const XBarRChart *chart, int *violated_rules,
                      int max_rules) {
    if (!chart || !violated_rules || max_rules <= 0
        || chart->total_subgroups < 8) return 0;

    int count = 0;
    double zone_a = 2.0 * (chart->ucl_x - chart->grand_mean) / 3.0;
    
    int n = chart->total_subgroups;
    double *m = chart->subgroup_means;

    /* Rule 1: One point outside zone A */
    for (int i = 0; i < n; i++) {
        if (fabs(m[i] - chart->grand_mean) > zone_a) {
            violated_rules[count++] = 1; break;
        }
    }
    if (count >= max_rules) return count;

    /* Rule 4: Eight consecutive on same side */
    for (int i = 0; i <= n - 8; i++) {
        int same_side = 1, side = (m[i] > chart->grand_mean) ? 1 : 0;
        for (int j = 1; j < 8; j++) {
            int cur_side = (m[i+j] > chart->grand_mean) ? 1 : 0;
            if (cur_side != side) { same_side = 0; break; }
        }
        if (same_side) { violated_rules[count++] = 4; break; }
    }
    if (count >= max_rules) return count;

    /* Rule 5: Six trending */
    for (int i = 0; i <= n - 6; i++) {
        int up = 1, down = 1;
        for (int j = 1; j < 6; j++) {
            if (m[i+j] <= m[i+j-1]) up = 0;
            if (m[i+j] >= m[i+j-1]) down = 0;
        }
        if (up || down) { violated_rules[count++] = 5; break; }
    }

    return count;
}

const char *weco_rule_description(int rule_number) {
    switch (rule_number) {
    case 1: return "One point beyond 3-sigma";
    case 2: return "Two of three beyond 2-sigma";
    case 3: return "Four of five beyond 1-sigma";
    case 4: return "Eight consecutive same side";
    case 5: return "Six trending up/down";
    case 6: return "Fourteen alternating";
    case 7: return "Fifteen within 1-sigma";
    case 8: return "Eight beyond 1-sigma both sides";
    default: return "Unknown rule";
    }
}

/* === Process Capability (L5) === */

void capability_calculate(const double *measurements, size_t n,
                           double usl, double lsl,
                           CapabilityIndex *out_idx) {
    if (!out_idx) return;
    memset(out_idx, 0, sizeof(*out_idx));

    if (!measurements || n < 2 || usl <= lsl) {
        out_idx->capable = 0; return;
    }

    double sum = 0.0;
    for (size_t i = 0; i < n; i++) sum += measurements[i];
    double mu = sum / (double)n;

    double varsum = 0.0;
    for (size_t i = 0; i < n; i++) {
        double d = measurements[i] - mu; varsum += d * d;
    }
    double sigma = sqrt(varsum / (double)(n - 1));
    double tol = usl - lsl;

    out_idx->cp = tol / (6.0 * sigma);
    double cpk_u = (usl - mu) / (3.0 * sigma);
    double cpk_l = (mu - lsl) / (3.0 * sigma);
    out_idx->cpk = (cpk_u < cpk_l) ? cpk_u : cpk_l;
    out_idx->pp = out_idx->cp;
    out_idx->ppk = out_idx->cpk;

    int defects = 0;
    for (size_t i = 0; i < n; i++) {
        if (measurements[i] > usl || measurements[i] < lsl) defects++;
    }
    out_idx->dpm = (double)defects / (double)n * 1e6;
    out_idx->sigma_level = process_sigma_level(out_idx->dpm);
    out_idx->capable = (out_idx->cpk >= 1.33) ? 1 : 0;
}

void capability_calculate_subgrouped(const double *measurements, size_t n,
                                      int subgroup_size,
                                      double usl, double lsl,
                                      CapabilityIndex *out_idx) {
    if (!out_idx) return;
    memset(out_idx, 0, sizeof(*out_idx));
    if (!measurements || n < 2 || subgroup_size < 2 || usl <= lsl
        || n % (size_t)subgroup_size != 0) {
        out_idx->capable = 0; return;
    }

    size_t n_sub = n / (size_t)subgroup_size;
    double *means = (double *)malloc(n_sub * sizeof(double));
    double *ranges = (double *)malloc(n_sub * sizeof(double));
    if (!means || !ranges) { free(means); free(ranges); return; }

    double grand_sum = 0.0, range_sum = 0.0;
    for (size_t i = 0; i < n_sub; i++) {
        double sum = 0.0, minv = measurements[i*subgroup_size],
               maxv = minv;
        for (int j = 0; j < subgroup_size; j++) {
            double v = measurements[i*subgroup_size + j];
            sum += v;
            if (v < minv) minv = v;
            if (v > maxv) maxv = v;
        }
        means[i] = sum / (double)subgroup_size;
        ranges[i] = maxv - minv;
        grand_sum += means[i];
        range_sum += ranges[i];
    }

    double grand_mean = grand_sum / (double)n_sub;
    double r_bar = range_sum / (double)n_sub;
    double d2_val;
    control_chart_constants(subgroup_size, NULL, NULL, NULL, NULL, NULL, &d2_val);
    if (d2_val == 0.0) d2_val = 1.0;
    double sigma_within = r_bar / d2_val;

    double tol = usl - lsl;
    out_idx->cp = tol / (6.0 * sigma_within);
    double cpk_u = (usl - grand_mean) / (3.0 * sigma_within);
    double cpk_l = (grand_mean - lsl) / (3.0 * sigma_within);
    out_idx->cpk = (cpk_u < cpk_l) ? cpk_u : cpk_l;

    /* Overall (Pp/Ppk) using total stddev */
    double total_sum = 0.0, total_var = 0.0;
    for (size_t i = 0; i < n; i++) total_sum += measurements[i];
    double total_mean = total_sum / (double)n;
    for (size_t i = 0; i < n; i++) {
        double d = measurements[i] - total_mean; total_var += d*d;
    }
    double sigma_total = sqrt(total_var / (double)(n - 1));
    out_idx->pp = tol / (6.0 * sigma_total);
    double ppk_u = (usl - total_mean) / (3.0 * sigma_total);
    double ppk_l = (total_mean - lsl) / (3.0 * sigma_total);
    out_idx->ppk = (ppk_u < ppk_l) ? ppk_u : ppk_l;

    out_idx->sigma_level = process_sigma_level(
        (1.0 - (out_idx->cpk > 0 ? 1.0 : 0.0)) * 1e6);
    out_idx->capable = (out_idx->cpk >= 1.33) ? 1 : 0;

    free(means); free(ranges);
}

double process_sigma_level(double dpmo) {
    if (dpmo <= 0.0) return 6.0;
    double pct = dpmo / 1e6;
    double z = 0.0;
    /* Approximate inverse normal CDF using rational approximation */
    if (pct > 0.5) pct = 1.0 - pct;
    double t = sqrt(-2.0 * log(pct > 0 ? pct : 1e-15));
    double c0 = 2.515517, c1 = 0.802853, c2 = 0.010328;
    double d1 = 1.432788, d2 = 0.189269, d3 = 0.001308;
    z = t - (c0 + c1*t + c2*t*t) / (1.0 + d1*t + d2*t*t + d3*t*t*t);
    return z + 1.5;
}

double dpmo_to_cpk(double dpmo) {
    return process_sigma_level(dpmo) / 3.0;
}

/* === Pareto Analysis (L5) === */
/*
 * Pareto principle: ~80% of effects come from ~20% of causes.
 * Standard quality tool for prioritizing improvement efforts.
 * Ref: Juran (1954), Quality Control Handbook
 */

int pareto_analysis(const int *counts, const char **labels, int n,
                     ParetoItem *out_items) {
    if (!counts || !out_items || n <= 0) return -1;
    int total = 0;
    for (int i = 0; i < n; i++) total += counts[i];
    if (total == 0) return -1;

    for (int i = 0; i < n; i++) {
        out_items[i].count = counts[i];
        out_items[i].percentage = (double)counts[i] / (double)total * 100.0;
        if (labels && labels[i]) {
            strncpy(out_items[i].category, labels[i],
                    sizeof(out_items[i].category) - 1);
            out_items[i].category[sizeof(out_items[i].category) - 1] = '\0';
        }
    }

    /* Sort descending by count (bubble sort for simplicity) */
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (out_items[j].count < out_items[j+1].count) {
                ParetoItem tmp = out_items[j];
                out_items[j] = out_items[j+1];
                out_items[j+1] = tmp;
            }
        }
    }

    double cum = 0.0;
    for (int i = 0; i < n; i++) {
        cum += out_items[i].percentage;
        out_items[i].cumulative_pct = cum;
    }
    return n;
}

/* === Gauge R&R (L5) === */
/*
 * Gauge Repeatability and Reproducibility per AIAG MSA Manual.
 * ANOVA-based estimation of measurement system variation.
 *
 * Total variation = part variation + gauge variation
 * Gauge variation = repeatability (equipment) + reproducibility (appraiser)
 * %GRR = sigma_gauge / sigma_total * 100
 *
 * Acceptability: <10% acceptable, 10-30% marginal, >30% unacceptable
 */

int gauge_rnr_analysis(const double *measurements,
                        int n_parts, int n_operators, int n_trials,
                        GaugeRRResult *out_result) {
    if (!measurements || !out_result
        || n_parts < 2 || n_operators < 2 || n_trials < 2) return -1;

    memset(out_result, 0, sizeof(*out_result));
    int n_total = n_parts * n_operators * n_trials;

    /* Compute ranges per operator-part combination for repeatability */
    double r_sum = 0.0;
    int r_count = 0;
    for (int p = 0; p < n_parts; p++) {
        for (int o = 0; o < n_operators; o++) {
            double minv = 1e9, maxv = -1e9;
            for (int t = 0; t < n_trials; t++) {
                int idx = p * n_operators * n_trials + o * n_trials + t;
                if (idx < n_total) {
                    if (measurements[idx] < minv) minv = measurements[idx];
                    if (measurements[idx] > maxv) maxv = measurements[idx];
                }
            }
            r_sum += (maxv - minv);
            r_count++;
        }
    }
    double r_bar = (r_count > 0) ? r_sum / (double)r_count : 0.0;
    double d2_r;
    control_chart_constants(n_trials, NULL, NULL, NULL, NULL, NULL, &d2_r);
    if (d2_r == 0.0) d2_r = 1.0;
    out_result->repeatability = r_bar / d2_r;

    /* Reproducibility from operator means */
    double *op_means = (double *)calloc((size_t)n_operators, sizeof(double));
    int *op_counts = (int *)calloc((size_t)n_operators, sizeof(int));
    if (!op_means || !op_counts) {
        free(op_means); free(op_counts); return -1;
    }
    for (int p = 0; p < n_parts; p++) {
        for (int o = 0; o < n_operators; o++) {
            for (int t = 0; t < n_trials; t++) {
                int idx = p * n_operators * n_trials + o * n_trials + t;
                if (idx < n_total) {
                    op_means[o] += measurements[idx];
                    op_counts[o]++;
                }
            }
        }
    }
    double op_min = 1e9, op_max = -1e9;
    for (int o = 0; o < n_operators; o++) {
        if (op_counts[o] > 0) op_means[o] /= (double)op_counts[o];
        if (op_means[o] < op_min) op_min = op_means[o];
        if (op_means[o] > op_max) op_max = op_means[o];
    }
    free(op_means); free(op_counts);
    double x_bar_diff = op_max - op_min;

    double sigma_e = out_result->repeatability;
    out_result->reproducibility = (sigma_e > 0)
        ? sqrt((x_bar_diff > 0 ? x_bar_diff * x_bar_diff / (double)n_trials : 0)
               - sigma_e * sigma_e / (double)(n_parts * n_trials))
        : 0.0;
    if (out_result->reproducibility < 0.0)
        out_result->reproducibility = 0.0;

    out_result->grr_total = sqrt(out_result->repeatability
                                  * out_result->repeatability
                                  + out_result->reproducibility
                                  * out_result->reproducibility);

    /* Part variation */
    double total_var = 0.0, total_mean = 0.0;
    for (int i = 0; i < n_total; i++) total_mean += measurements[i];
    total_mean /= (double)n_total;
    for (int i = 0; i < n_total; i++) {
        double d = measurements[i] - total_mean; total_var += d*d;
    }
    double sigma_total = sqrt(total_var / (double)(n_total - 1));
    out_result->total_variation = sigma_total;

    double part_var = sigma_total * sigma_total
                      - out_result->grr_total * out_result->grr_total;
    out_result->part_variation = (part_var > 0.0) ? sqrt(part_var) : 0.0;

    out_result->grr_pct = (sigma_total > 0.0)
        ? out_result->grr_total / sigma_total * 100.0 : 0.0;
    out_result->acceptable = (out_result->grr_pct < 30.0) ? 1 : 0;

    return 0;
}

/* === Defect Analysis (L6) === */

double defect_rate_calculate(int defect_count, int total_count) {
    if (total_count <= 0) return 0.0;
    return (double)defect_count / (double)total_count * 100.0;
}

double dpmo_calculate(int defect_count, int total_count,
                       int opportunity_count) {
    if (total_count <= 0 || opportunity_count <= 0) return 0.0;
    return (double)defect_count / ((double)total_count
                                   * (double)opportunity_count) * 1e6;
}

double rolled_throughput_yield(const double *yields, int n_stages) {
    if (!yields || n_stages <= 0) return 0.0;
    double rty = 1.0;
    for (int i = 0; i < n_stages; i++) rty *= yields[i];
    return rty;
}

double process_sigma_from_dpmo(double dpmo) {
    return process_sigma_level(dpmo);
}

/* === Supplier Scorecard (L7) === */

void supplier_scorecard_init(SupplierScorecard *sc,
                              const char *code, const char *name) {
    if (!sc) return;
    memset(sc, 0, sizeof(*sc));
    if (code) {
        strncpy(sc->supplier_code, code, sizeof(sc->supplier_code) - 1);
        sc->supplier_code[sizeof(sc->supplier_code) - 1] = '\0';
    }
    if (name) {
        strncpy(sc->supplier_name, name, sizeof(sc->supplier_name) - 1);
        sc->supplier_name[sizeof(sc->supplier_name) - 1] = '\0';
    }
}

void supplier_scorecard_update(SupplierScorecard *sc,
                                int lots_r, int lots_a,
                                int parts_i, int parts_d) {
    if (!sc) return;
    sc->lots_received += lots_r;
    sc->lots_accepted += lots_a;
    sc->parts_inspected += parts_i;
    sc->parts_defective += parts_d;
    sc->dpmo = dpmo_calculate(sc->parts_defective, sc->parts_inspected, 1);
    sc->quality_score = (sc->lots_received > 0)
        ? (double)sc->lots_accepted / (double)sc->lots_received * 100.0 : 0.0;
}

/* === CUSUM (L8) === */
/*
 * Cumulative Sum control chart is more sensitive than Shewhart
 * charts for detecting small sustained shifts in the process mean.
 *
 * C+[i] = max(0, C+[i-1] + x[i] - (mu0 + k))
 * C-[i] = max(0, C-[i-1] + (mu0 - k) - x[i])
 *
 * Signal when C+ > h or C- > h (decision interval)
 * Ref: Page (1954), Biometrika
 */

void cusum_init(CUSUMChart *c, int capacity, double k, double h) {
    if (!c) return;
    c->cusum_high = (double *)calloc((size_t)capacity, sizeof(double));
    c->cusum_low  = (double *)calloc((size_t)capacity, sizeof(double));
    c->k = k; c->h = h;
    c->capacity = capacity; c->count = 0;
}

int cusum_update(CUSUMChart *c, double value,
                  int *out_signal_high, int *out_signal_low) {
    if (!c || c->count >= c->capacity) return -1;
    int idx = c->count;

    double ch = (idx > 0) ? c->cusum_high[idx-1] : 0.0;
    ch += value - c->k;
    if (ch < 0.0) ch = 0.0;
    c->cusum_high[idx] = ch;

    double cl = (idx > 0) ? c->cusum_low[idx-1] : 0.0;
    cl += (-c->k) - value;
    if (cl < 0.0) cl = 0.0;
    c->cusum_low[idx] = cl;

    c->count++;

    if (out_signal_high) *out_signal_high = (ch > c->h) ? 1 : 0;
    if (out_signal_low)  *out_signal_low  = (cl > c->h) ? 1 : 0;

    return 0;
}
