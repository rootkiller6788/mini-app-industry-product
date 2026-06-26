/**
 * mini-industry-solution: predictive_maintenance.c
 * CBM, Weibull reliability, anomaly detection, RUL estimation, FMEA RPN
 * Ref: ISO 14224 RCM, ISO 13374 CM&D, Weibull (1951),
 *      MIL-STD-1629A FMECA, SAE JA1011
 */
#include "predictive_maintenance.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* === Weibull Distribution (L4-L5) === */
/*
 * Weibull distribution is fundamental to reliability engineering.
 *
 * PDF: f(t) = (beta/eta) * (t/eta)^(beta-1) * exp(-(t/eta)^beta)
 * CDF: F(t) = 1 - exp(-(t/eta)^beta)
 * Reliability: R(t) = exp(-(t/eta)^beta)
 * Failure rate: h(t) = (beta/eta) * (t/eta)^(beta-1)
 * MTTF: eta * Gamma(1 + 1/beta)
 *
 * Where: eta = scale parameter (characteristic life)
 *        beta = shape parameter
 *          beta < 1: infant mortality (decreasing failure rate)
 *          beta = 1: random failures (exponential, constant rate)
 *          beta > 1: wear-out (increasing failure rate)
 *
 * Ref: Weibull, W. (1951) "A Statistical Distribution Function
 *      of Wide Applicability", Journal of Applied Mechanics
 */

double weibull_pdf(double t, double eta, double beta) {
    if (t <= 0.0 || eta <= 0.0 || beta <= 0.0) return 0.0;
    double ratio = t / eta;
    return (beta / eta) * pow(ratio, beta - 1.0)
           * exp(-pow(ratio, beta));
}

double weibull_cdf(double t, double eta, double beta) {
    if (t <= 0.0 || eta <= 0.0 || beta <= 0.0) return 0.0;
    return 1.0 - exp(-pow(t / eta, beta));
}

double weibull_reliability(double t, double eta, double beta) {
    if (t <= 0.0 || eta <= 0.0 || beta <= 0.0) return 1.0;
    return exp(-pow(t / eta, beta));
}

double weibull_failure_rate(double t, double eta, double beta) {
    if (t <= 0.0 || eta <= 0.0 || beta <= 0.0) return 0.0;
    return (beta / eta) * pow(t / eta, beta - 1.0);
}

/*
 * Stirling's approximation for Gamma function (used in MTTF)
 * Gamma(x) ≈ sqrt(2*pi/x) * (x/e)^x for large x
 */
static double gamma_approx(double x) {
    if (x <= 0.0) return 1.0;
    /* Use Lanczos approximation for small values */
    if (x < 0.5) return M_PI / (sin(M_PI * x) * gamma_approx(1.0 - x));
    x -= 1.0;
    static const double p[] = {
        0.99999999999980993, 676.5203681218851, -1259.1392167224028,
        771.32342877765313, -176.61502916214059, 12.507343278686905,
        -0.13857109526572012, 9.9843695780195716e-6, 1.5056327351493116e-7
    };
    double y = p[0];
    for (int i = 1; i < 9; i++) y += p[i] / (x + (double)i);
    double t = x + 7.5;
    return sqrt(2.0 * M_PI) * pow(t, x + 0.5) * exp(-t) * y;
}

double weibull_mttf(double eta, double beta) {
    if (eta <= 0.0 || beta <= 0.0) return 0.0;
    return eta * gamma_approx(1.0 + 1.0 / beta);
}

/*
 * Weibull parameter estimation using rank regression on X (RRX).
 * Uses median rank approximation: F(t_i) ≈ (i - 0.3)/(n + 0.4)
 * Linearizes: ln(-ln(1-F)) = beta*ln(t) - beta*ln(eta)
 *
 * This yields a simple linear regression in log-log space
 * to estimate beta (slope) and eta (from intercept).
 */
int weibull_parameter_estimate(const double *failure_times, size_t n,
                                double *out_eta, double *out_beta) {
    if (!failure_times || n < 3 || !out_eta || !out_beta) return -1;

    double *x = (double *)malloc(n * sizeof(double));
    double *y = (double *)malloc(n * sizeof(double));
    if (!x || !y) { free(x); free(y); return -1; }

    for (size_t i = 0; i < n; i++) {
        double rank = ((double)(i + 1) - 0.3) / ((double)n + 0.4);
        if (failure_times[i] <= 0.0) { free(x); free(y); return -1; }
        x[i] = log(failure_times[i]);
        y[i] = log(-log(1.0 - rank));
    }

    double mx = 0.0, my = 0.0;
    for (size_t i = 0; i < n; i++) { mx += x[i]; my += y[i]; }
    mx /= (double)n; my /= (double)n;

    double num = 0.0, den = 0.0;
    for (size_t i = 0; i < n; i++) {
        double dx = x[i] - mx;
        num += dx * (y[i] - my);
        den += dx * dx;
    }

    free(x); free(y);
    if (den == 0.0) return -1;
    *out_beta = num / den;
    *out_eta = exp(-my / *out_beta + mx);
    return 0;
}

/* === Trend Analysis (L5) === */

void trend_series_init(TrendSeries *ts, double *buffer, size_t capacity) {
    if (!ts || !buffer) return;
    ts->values = buffer;
    ts->timestamps = NULL;
    ts->count = 0;
    ts->capacity = capacity;
    ts->mean = 0.0; ts->stddev = 0.0;
    ts->slope = 0.0; ts->trend_direction = 0;
}

int trend_series_append(TrendSeries *ts, double value, double timestamp) {
    if (!ts || ts->count >= ts->capacity) return -1;
    ts->values[ts->count] = value;
    if (ts->timestamps) ts->timestamps[ts->count] = timestamp;
    ts->count++;

    /* Recompute statistics incrementally */
    if (ts->count >= 2) {
        double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
        for (size_t i = 0; i < ts->count; i++) {
            double x = (double)i;
            sum_x += x; sum_y += ts->values[i];
            sum_xy += x * ts->values[i]; sum_x2 += x * x;
        }
        double n = (double)ts->count;
        double den = n * sum_x2 - sum_x * sum_x;
        ts->slope = (den != 0.0) ? (n * sum_xy - sum_x * sum_y) / den : 0.0;

        ts->mean = sum_y / n;
        double var_sum = 0.0;
        for (size_t i = 0; i < ts->count; i++) {
            double d = ts->values[i] - ts->mean;
            var_sum += d * d;
        }
        ts->stddev = sqrt(var_sum / n);
        ts->trend_direction = (ts->slope > 0) ? 1 : (ts->slope < 0) ? -1 : 0;
    }
    return 0;
}

double trend_series_forecast(const TrendSeries *ts, double future_time) {
    if (!ts || ts->count < 2) return 0.0;
    double last_x = (double)(ts->count - 1);
    double future_x = last_x + future_time;
    return ts->values[ts->count - 1] + ts->slope * (future_x - last_x);
}

int trend_series_detect_change(const TrendSeries *ts,
                                double threshold, int *out_index) {
    if (!ts || ts->count < 4) return 0;
    for (size_t i = 2; i < ts->count - 1; i++) {
        double before = 0.0, after = 0.0;
        for (size_t j = 0; j < i; j++) before += ts->values[j];
        for (size_t j = i; j < ts->count; j++) after += ts->values[j];
        before /= (double)i;
        after /= (double)(ts->count - i);
        if (fabs(after - before) > threshold * ts->stddev) {
            if (out_index) *out_index = (int)i;
            return 1;
        }
    }
    return 0;
}

/* === Anomaly Detection (L5) === */

int anomaly_zscore_detect(const double *values, size_t n,
                           double threshold, int *out_indices,
                           int max_out, int *out_count) {
    if (!values || n < 3 || !out_indices || max_out <= 0) return -1;
    double m = 0.0;
    for (size_t i = 0; i < n; i++) m += values[i];
    m /= (double)n;
    double sd = 0.0;
    for (size_t i = 0; i < n; i++) { double d = values[i]-m; sd += d*d; }
    sd = sqrt(sd/(double)n);
    if (sd < 1e-10) { if (out_count) *out_count = 0; return 0; }
    int cnt = 0;
    for (size_t i = 0; i < n && cnt < max_out; i++) {
        double z = fabs(values[i] - m) / sd;
        if (z > threshold) out_indices[cnt++] = (int)i;
    }
    if (out_count) *out_count = cnt;
    return cnt;
}

int anomaly_moving_average_detect(const double *values, size_t n,
                                   int window, double threshold,
                                   int *out_indices, int max_out,
                                   int *out_count) {
    if (!values || n < (size_t)(window+1) || window < 2
        || !out_indices || max_out <= 0) return -1;
    int cnt = 0;
    for (size_t i = (size_t)window; i < n && cnt < max_out; i++) {
        double ma = 0.0;
        for (int j = 0; j < window; j++) ma += values[i - j - 1];
        ma /= (double)window;
        if (fabs(values[i] - ma) > threshold * ma) {
            out_indices[cnt++] = (int)i;
        }
    }
    if (out_count) *out_count = cnt;
    return cnt;
}

/* === Remaining Useful Life (L5) === */

double rul_linear_estimate(double current_value, double failure_threshold,
                            double degradation_rate) {
    if (degradation_rate <= 0.0) return 1e9;
    return (failure_threshold - current_value) / degradation_rate;
}

double rul_degradation_rate_estimate(const double *values, size_t n,
                                      const double *timestamps) {
    if (!values || n < 2) return 0.0;
    double t0 = timestamps ? timestamps[0] : 0.0;
    double tn = timestamps ? timestamps[n-1] : (double)(n-1);
    if (tn <= t0) return 0.0;
    return (values[n-1] - values[0]) / (tn - t0);
}

double rul_exponential_estimate(double current_value, double initial_value,
                                 double failure_threshold, double lambda) {
    if (lambda <= 0.0 || initial_value <= 0.0) return 1e9;
    double t_current = log(current_value / initial_value) / lambda;
    double t_failure = log(failure_threshold / initial_value) / lambda;
    return t_failure - t_current;
}

/* === Bearing Fault Frequencies (L6) === */

void bearing_fault_frequencies(BearingGeometry *bg, double speed_rpm,
                                 int n_balls, double pitch_dia,
                                 double ball_dia, double contact_angle) {
    if (!bg) return;
    bg->shaft_speed_rpm = speed_rpm;
    bg->n_balls = n_balls;
    bg->pitch_diameter = pitch_dia;
    bg->ball_diameter = ball_dia;
    bg->contact_angle_deg = contact_angle;
    double fr = speed_rpm / 60.0;
    double cos_a = cos(contact_angle * M_PI / 180.0);
    double ratio = ball_dia / pitch_dia;
    bg->bpfo = (double)n_balls / 2.0 * fr * (1.0 - ratio * cos_a);
    bg->bpfi = (double)n_balls / 2.0 * fr * (1.0 + ratio * cos_a);
    bg->bsf  = pitch_dia / (2.0 * ball_dia) * fr
               * (1.0 - ratio * ratio * cos_a * cos_a);
    bg->ftf  = fr / 2.0 * (1.0 - ratio * cos_a);
}

int bearing_fault_diagnose(double bpfo_amp, double bpfi_amp,
                            double bsf_amp, double threshold,
                            int *fault_type) {
    if (!fault_type) return 0;
    if (bpfo_amp > threshold) { *fault_type = 1; return 1; }
    if (bpfi_amp > threshold) { *fault_type = 2; return 1; }
    if (bsf_amp > threshold)  { *fault_type = 3; return 1; }
    *fault_type = 0;
    return 0;
}

/* === Maintenance Optimization (L6-L7) === */

/*
 * Optimal preventive maintenance interval based on cost minimization.
 *
 * Total cost rate: C(T) = (Cp * R(T) + Cc * F(T)) / MTTF_interval
 * Where: Cp = preventive maintenance cost
 *        Cc = corrective maintenance cost (Cc > Cp)
 *
 * For Weibull-distributed failures, the optimal interval
 * balances PM cost against CM risk.
 */
double maintenance_optimal_interval(double cost_pm,
                                     double cost_corrective,
                                     double eta, double beta) {
    if (cost_pm <= 0.0 || cost_corrective <= 0.0
        || eta <= 0.0 || beta <= 0.0) return eta;
    if (beta <= 1.0) return eta * 0.5;

    double ratio = cost_corrective / cost_pm;
    double optimal = eta * pow(1.0 / (ratio * (beta - 1.0)), 1.0 / beta);
    if (optimal <= 0.0 || optimal > eta * 5.0) optimal = eta;
    return optimal;
}

double maintenance_cost_per_hour(double interval, double cost_pm,
                                   double cost_cm, double eta, double beta) {
    if (interval <= 0.0 || eta <= 0.0 || beta <= 0.0) return 1e9;
    double R = weibull_reliability(interval, eta, beta);
    double F = 1.0 - R;
    double mttf_interval = weibull_mttf(eta * 0.5, beta);
    return (cost_pm * R + cost_cm * F) / (mttf_interval > 0 ? mttf_interval : 1.0);
}

/* === Fleet Maintenance Scheduling (L7) === */

static int cmp_priority(const void *a, const void *b) {
    const MaintenanceTask *ta = (const MaintenanceTask *)a;
    const MaintenanceTask *tb = (const MaintenanceTask *)b;
    return tb->priority - ta->priority;
}

static int cmp_due_date(const void *a, const void *b) {
    const MaintenanceTask *ta = (const MaintenanceTask *)a;
    const MaintenanceTask *tb = (const MaintenanceTask *)b;
    double diff = (double)(ta->due_date.tv_sec - tb->due_date.tv_sec);
    return (diff > 0) ? 1 : (diff < 0) ? -1 : 0;
}

int maintenance_sort_by_priority(MaintenanceTask *tasks, int count) {
    if (!tasks || count <= 0) return -1;
    qsort(tasks, (size_t)count, sizeof(MaintenanceTask), cmp_priority);
    return 0;
}

int maintenance_sort_by_due_date(MaintenanceTask *tasks, int count) {
    if (!tasks || count <= 0) return -1;
    qsort(tasks, (size_t)count, sizeof(MaintenanceTask), cmp_due_date);
    return 0;
}

int maintenance_filter_overdue(MaintenanceTask *tasks, int count,
                                struct timespec now,
                                MaintenanceTask *out_overdue,
                                int max_out) {
    if (!tasks || !out_overdue || max_out <= 0) return 0;
    int n = 0;
    for (int i = 0; i < count && n < max_out; i++) {
        if (!tasks[i].completed
            && tasks[i].due_date.tv_sec < now.tv_sec) {
            memcpy(&out_overdue[n], &tasks[i], sizeof(MaintenanceTask));
            n++;
        }
    }
    return n;
}

/* === Condition Baseline (L7) === */

int condition_baseline_compute(const double *measurements, size_t n,
                                double *out_mean, double *out_stddev,
                                double *out_warning, double *out_alarm) {
    if (!measurements || n < 10) return -1;
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) sum += measurements[i];
    double mean = sum / (double)n;
    double varsum = 0.0;
    for (size_t i = 0; i < n; i++) {
        double d = measurements[i] - mean;
        varsum += d * d;
    }
    double std = sqrt(varsum / (double)(n - 1));
    if (out_mean) *out_mean = mean;
    if (out_stddev) *out_stddev = std;
    if (out_warning) *out_warning = mean + 2.0 * std;
    if (out_alarm) *out_alarm = mean + 3.0 * std;
    return 0;
}

double health_score_compute(double current_value, double baseline,
                             double alarm_limit) {
    double range = alarm_limit - baseline;
    if (range <= 0.0) return 100.0;
    double score = 100.0 * (1.0 - (current_value - baseline) / range);
    if (score < 0.0) score = 0.0;
    if (score > 100.0) score = 100.0;
    return score;
}
