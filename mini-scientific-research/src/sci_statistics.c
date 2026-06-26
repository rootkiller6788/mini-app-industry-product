#include "sci_statistics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <float.h>
#include <assert.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * sci_statistics.c -- Statistical Computing for Scientific Research
 *
 * Implements: descriptive statistics (Welford streaming, percentiles),
 * probability distributions (PDF/CDF/quantile for normal, t, chi2, F),
 * hypothesis testing (t-tests, chi-squared, KS, Mann-Whitney),
 * ANOVA (one-way, two-way), regression (OLS), bootstrap CI,
 * multiple testing correction (Holm-Bonferroni, Benjamini-Hochberg).
 *
 * L1-L7: Full implementations.
 *
 * References:
 * - Casella & Berger, "Statistical Inference" (2nd ed, 2002)
 * - Wasserman, "All of Statistics" (2004)
 * - Fisher, "Statistical Methods for Research Workers" (1925)
 * - Efron, "Bootstrap Methods: Another Look at the Jackknife" (1979)
 */

/* ================================================================
 * L5: Descriptive Statistics — Welford Streaming Algorithm
 *
 * Welford (1962): Numerically stable single-pass variance.
 *   M_1 = x_1
 *   M_k = M_{k-1} + (x_k - M_{k-1})/k
 *   S_k = S_{k-1} + (x_k - M_{k-1})(x_k - M_k)
 * Then: mean = M_n, variance = S_n/(n-1) (unbiased).
 *
 * Chan, Golub & LeVeque (1983): Parallel variance merging.
 * ================================================================ */

SCI_SummaryStats sci_compute_summary(const double *data, int n) {
    SCI_SummaryStats stats;
    memset(&stats, 0, sizeof(stats));
    if (!data || n <= 0) return stats;

    stats.n = n;
    double M = data[0], S = 0.0;
    stats.min = data[0]; stats.max = data[0];
    stats.sum = data[0]; stats.sum_sq = data[0] * data[0];

    for (int i = 1; i < n; i++) {
        double x = data[i];
        double M_prev = M;
        M = M_prev + (x - M_prev) / (i + 1);
        S = S + (x - M_prev) * (x - M);
        stats.sum += x;
        stats.sum_sq += x * x;
        if (x < stats.min) stats.min = x;
        if (x > stats.max) stats.max = x;
    }

    stats.mean = M;
    stats.variance = (n > 1) ? S / (n - 1) : 0.0;
    stats.sd = sqrt(stats.variance);

    /* Skewness and kurtosis (Fisher-Pearson) */
    if (n > 2 && stats.sd > 0.0) {
        double m3 = 0.0, m4 = 0.0;
        for (int i = 0; i < n; i++) {
            double z = (data[i] - stats.mean) / stats.sd;
            double z2 = z * z;
            m3 += z * z2;
            m4 += z2 * z2;
        }
        stats.skewness = (m3 / n) * n * n / ((n-1)*(n-2));
        stats.kurtosis = (m4 / n) * n*n*(n+1)/((n-1)*(n-2)*(n-3))
                        - 3.0*(n-1)*(n-1)/((n-2)*(n-3));
    }

    /* Median and quartiles via sorting copy */
    if (n > 0) {
        double *sorted = (double*)malloc(n * sizeof(double));
        if (sorted) {
            memcpy(sorted, data, n * sizeof(double));
            sci_quicksort(sorted, n);
            stats.median = sci_percentile(sorted, n, 50.0);
            stats.q1 = sci_percentile(sorted, n, 25.0);
            stats.q3 = sci_percentile(sorted, n, 75.0);
            free(sorted);
        }
    }
    return stats;
}

void sci_stats_update(SCI_SummaryStats *stats, double x, int n) {
    if (!stats) return;
    if (n == 0) { stats->mean = x; stats->sum = x; stats->sum_sq = x*x;
        stats->min = x; stats->max = x; stats->n = 1; return; }
    double M_prev = stats->mean;
    stats->n = n + 1;
    stats->mean = M_prev + (x - M_prev) / stats->n;
    double S_prev = stats->variance * (n - 1);
    double S_new = S_prev + (x - M_prev) * (x - stats->mean);
    stats->variance = (stats->n > 1) ? S_new / (stats->n - 1) : 0.0;
    stats->sd = sqrt(stats->variance);
    stats->sum += x;
    stats->sum_sq += x * x;
    if (x < stats->min) stats->min = x;
    if (x > stats->max) stats->max = x;
}

SCI_SummaryStats sci_stats_merge(const SCI_SummaryStats *a,
                                  const SCI_SummaryStats *b) {
    SCI_SummaryStats merged;
    memset(&merged, 0, sizeof(merged));
    if (!a || !b) {
        if (a) return *a;
        if (b) return *b;
        return merged;
    }
    int n = a->n + b->n;
    if (n == 0) return merged;
    /* Chan-Golub-LeVeque parallel variance merge */
    double delta = b->mean - a->mean;
    merged.mean = (a->n * a->mean + b->n * b->mean) / n;
    double va = a->variance * (a->n - 1);
    double vb = b->variance * (b->n - 1);
    merged.variance = (va + vb + delta*delta * a->n * b->n / n) / (n - 1);
    merged.sd = sqrt(merged.variance);
    merged.n = n;
    merged.sum = a->sum + b->sum;
    merged.sum_sq = a->sum_sq + b->sum_sq;
    merged.min = (a->min < b->min) ? a->min : b->min;
    merged.max = (a->max > b->max) ? a->max : b->max;
    return merged;
}

/* ================================================================
 * L5: Quicksort for statistical computations
 * ================================================================ */

static void quicksort_recursive(double *a, int lo, int hi) {
    if (lo >= hi) return;
    double pivot = a[(lo + hi) / 2];
    int i = lo - 1, j = hi + 1;
    while (1) {
        do { i++; } while (a[i] < pivot);
        do { j--; } while (a[j] > pivot);
        if (i >= j) break;
        double t = a[i]; a[i] = a[j]; a[j] = t;
    }
    quicksort_recursive(a, lo, j);
    quicksort_recursive(a, j + 1, hi);
}

void sci_quicksort(double *data, int n) {
    if (!data || n <= 1) return;
    quicksort_recursive(data, 0, n - 1);
}

/* ================================================================
 * L5: Percentile computation (Hyndman-Fan R7 definition)
 * ================================================================ */

double sci_percentile(const double *sorted_data, int n, double p) {
    if (!sorted_data || n <= 0) return NAN;
    if (p <= 0.0) return sorted_data[0];
    if (p >= 100.0) return sorted_data[n-1];
    double h = (n - 1) * p / 100.0;
    int lo = (int)floor(h);
    double frac = h - lo;
    if (lo >= n - 1) return sorted_data[n-1];
    return sorted_data[lo] + frac * (sorted_data[lo+1] - sorted_data[lo]);
}

/* ================================================================
 * L2: Probability Distributions
 * ================================================================ */

/* Normal CDF via Abramowitz & Stegun rational approximation (7.1.26).
 * Phi(x) = 0.5 * (1 + erf(x/sqrt(2)))
 * Accuracy: ~1.5e-7. */

double sci_normal_cdf(double x) {
    double t = 1.0 / (1.0 + 0.2316419 * fabs(x));
    double d = 0.3989422804014327; /* 1/sqrt(2*pi) */
    double poly = t * (0.319381530 + t * (-0.356563782
                 + t * (1.781477937 + t * (-1.821255978
                 + t * 1.330274429))));
    double pdf_part = d * exp(-x * x / 2.0);
    double cdf = 1.0 - pdf_part * poly;
    return (x >= 0.0) ? cdf : 1.0 - cdf;
}

/* Inverse normal CDF (quantile) via Moro (1995) / Acklam.
 * Accuracy: ~1e-8. */

double sci_normal_quantile(double p) {
    if (p <= 0.0) return -DBL_MAX;
    if (p >= 1.0) return DBL_MAX;
    if (p == 0.5) return 0.0;

    double a0 = 2.50662823884, a1 = -18.61500062529, a2 = 41.39119773534;
    double a3 = -25.44106049637;
    double b0 = -8.47351093090, b1 = 23.08336743743, b2 = -21.06224101826;
    double b3 = 3.13082909833;
    double c0 = 0.3374754822726147, c1 = 0.9761690190917186;
    double c2 = 0.1607979714918209, c3 = 0.0276438810333863;
    double c4 = 0.0038405729373609, c5 = 0.0003951896511919;
    double c6 = 0.0000321767881768, c7 = 0.0000002888167364;
    double c8 = 0.0000003960315187;

    double y = p - 0.5;
    if (fabs(y) < 0.42) {
        double r = y * y;
        return y * (((a3 * r + a2) * r + a1) * r + a0)
               / ((((b3 * r + b2) * r + b1) * r + b0) * r + 1.0);
    }
    double r = (p < 0.5) ? p : 1.0 - p;
    r = log(-log(r));
    double x = c0 + r * (c1 + r * (c2 + r * (c3 + r * (c4
             + r * (c5 + r * (c6 + r * (c7 + r * c8)))))));
    return (p < 0.5) ? -x : x;
}

/* Student t CDF via incomplete beta function.
 * Uses series expansion for the regularized incomplete beta.
 * P(T <= t) evaluated using relationship with beta distribution. */

static double beta_inc_reg(double a, double b, double x) {
    if (x < 0.0 || x > 1.0) return NAN;
    if (x == 0.0) return 0.0;
    if (x == 1.0) return 1.0;
    /* Continued fraction for incomplete beta (Lentz method) */
    double d = 1.0;
    double c = 1.0;
    double f = 1.0;
    for (int m = 1; m <= 100; m++) {
        double aa = (double)m;
        double t1 = -(aa * (b - aa) * x) / ((a + 2.0*aa - 1.0)*(a + 2.0*aa));
        d = 1.0 + t1 * d;
        if (fabs(d) < 1e-30) d = 1e-30;
        double t2 = aa * (a + aa - 1.0) * x / ((a + 2.0*aa)*(a + 2.0*aa + 1.0));
        d = 1.0 + t2 / d;
        if (fabs(d) < 1e-30) d = 1e-30;
        c = 1.0 + t1 / c;
        if (fabs(c) < 1e-30) c = 1e-30;
        c = 1.0 + t2 / c;
        if (fabs(c) < 1e-30) c = 1e-30;
        double del = d * c;
        f *= del;
        if (fabs(del - 1.0) < 1e-12) break;
    }
    double ln_beta = lgamma(a) + lgamma(b) - lgamma(a + b);
    return exp(a * log(x) + b * log(1.0 - x) - ln_beta) / (a * f);
}

double sci_student_t_cdf(double t, double df) {
    if (df <= 0.0) return NAN;
    double x = df / (df + t * t);
    double b = beta_inc_reg(df / 2.0, 0.5, x);
    if (t > 0.0) return 1.0 - 0.5 * b;
    return 0.5 * b;
}

/* Chi-squared CDF via regularized incomplete gamma P(a,x).
 * P(a,x) = 1/Gamma(a) * integral_0^x t^{a-1} e^{-t} dt
 * Using series expansion for x < a+1, continued fraction otherwise. */

static double gamma_inc_reg_P(double a, double x) {
    if (x < 0.0) return NAN;
    if (x == 0.0) return 0.0;
    if (a <= 0.0) return NAN;

    if (x < a + 1.0) {
        /* Series expansion */
        double ap = a, sum = 1.0 / a, del = sum;
        for (int n = 1; n <= 100; n++) {
            ap += 1.0;
            del *= x / ap;
            sum += del;
            if (fabs(del) < fabs(sum) * 1e-12) break;
        }
        return sum * exp(-x + a * log(x) - lgamma(a));
    } else {
        /* Continued fraction */
        double b = x + 1.0 - a;
        double c = 1.0 / 1e-30;
        double d = 1.0 / b;
        double h = d;
        for (int i = 1; i <= 100; i++) {
            double an = -i * (i - a);
            b += 2.0;
            d = an * d + b;
            if (fabs(d) < 1e-30) d = 1e-30;
            c = b + an / c;
            if (fabs(c) < 1e-30) c = 1e-30;
            d = 1.0 / d;
            double del = d * c;
            h *= del;
            if (fabs(del - 1.0) < 1e-12) break;
        }
        return 1.0 - exp(-x + a * log(x) - lgamma(a)) * h;
    }
}

double sci_chisq_cdf(double x, double df) {
    if (x <= 0.0) return 0.0;
    return gamma_inc_reg_P(df / 2.0, x / 2.0);
}

double sci_f_dist_cdf(double f, double df1, double df2) {
    if (f <= 0.0) return 0.0;
    double x = df1 * f / (df1 * f + df2);
    return beta_inc_reg(df1/2.0, df2/2.0, x);
}

double sci_dist_pdf(const SCI_Distribution *dist, double x) {
    if (!dist) return NAN;
    switch (dist->dist) {
        case SCI_DIST_NORMAL: {
            double z = (x - dist->params.normal.mean) / dist->params.normal.sd;
            return exp(-0.5*z*z) / (dist->params.normal.sd * sqrt(2.0 * M_PI));
        }
        case SCI_DIST_UNIFORM:
            return (x >= dist->params.uniform.a && x <= dist->params.uniform.b)
                   ? 1.0/(dist->params.uniform.b - dist->params.uniform.a) : 0.0;
        case SCI_DIST_EXPONENTIAL:
            return (x >= 0.0) ? dist->params.exponential.rate
                   * exp(-dist->params.exponential.rate * x) : 0.0;
        default: return NAN;
    }
}

double sci_dist_cdf(const SCI_Distribution *dist, double x) {
    if (!dist) return NAN;
    switch (dist->dist) {
        case SCI_DIST_NORMAL:
            return sci_normal_cdf((x - dist->params.normal.mean) / dist->params.normal.sd);
        case SCI_DIST_UNIFORM:
            if (x < dist->params.uniform.a) return 0.0;
            if (x > dist->params.uniform.b) return 1.0;
            return (x - dist->params.uniform.a)/(dist->params.uniform.b - dist->params.uniform.a);
        case SCI_DIST_EXPONENTIAL:
            return (x >= 0.0) ? 1.0 - exp(-dist->params.exponential.rate * x) : 0.0;
        case SCI_DIST_CHI_SQUARED:
            return sci_chisq_cdf(x, dist->params.chisq.df);
        case SCI_DIST_STUDENT_T:
            return sci_student_t_cdf(x, dist->params.student_t.df);
        case SCI_DIST_F:
            return sci_f_dist_cdf(x, dist->params.f_dist.df1, dist->params.f_dist.df2);
        default: return NAN;
    }
}

double sci_dist_quantile(const SCI_Distribution *dist, double p) {
    if (!dist || p < 0.0 || p > 1.0) return NAN;
    switch (dist->dist) {
        case SCI_DIST_NORMAL:
            return dist->params.normal.mean
                   + dist->params.normal.sd * sci_normal_quantile(p);
        case SCI_DIST_UNIFORM:
            return dist->params.uniform.a
                   + p * (dist->params.uniform.b - dist->params.uniform.a);
        case SCI_DIST_EXPONENTIAL:
            return -log(1.0 - p) / dist->params.exponential.rate;
        default: return NAN;
    }
}

void sci_dist_sample(const SCI_Distribution *dist, int n,
                      double *samples, unsigned int seed) {
    if (!dist || !samples || n <= 0) return;
    static unsigned int lcg = 12345;
    if (seed != 0) lcg = seed;

    for (int i = 0; i < n; i++) {
        lcg = 1103515245 * lcg + 12345;
        double u1 = (double)(lcg & 0x7FFFFFFF) / 2147483648.0;
        lcg = 1103515245 * lcg + 12345;
        double u2 = (double)(lcg & 0x7FFFFFFF) / 2147483648.0;

        switch (dist->dist) {
            case SCI_DIST_NORMAL: {
                /* Box-Muller transform */
                double r = sqrt(-2.0 * log(u1));
                double theta = 2.0 * M_PI * u2;
                samples[i] = dist->params.normal.mean
                           + dist->params.normal.sd * r * cos(theta);
                break;
            }
            case SCI_DIST_UNIFORM:
                samples[i] = dist->params.uniform.a
                           + u1 * (dist->params.uniform.b - dist->params.uniform.a);
                break;
            case SCI_DIST_EXPONENTIAL:
                samples[i] = -log(u1) / dist->params.exponential.rate;
                break;
            default:
                samples[i] = 0.0;
        }
    }
}

/* ================================================================
 * L5: Hypothesis Testing — t-tests
 *
 * One-sample: t = (mean - mu0) / (sd / sqrt(n))
 * Two-sample (Welch): t = (m1-m2) / sqrt(var1/n1 + var2/n2)
 *   df = (v1+v2)^2 / (v1^2/(n1-1) + v2^2/(n2-1))  (Welch-Satterthwaite)
 * Paired: t = mean(diff) / (sd(diff) / sqrt(n))
 * ================================================================ */

static double ttest_pvalue(double t_stat, double df, SCI_TestTail tail) {
    double p = sci_student_t_cdf(t_stat, df);
    switch (tail) {
        case SCI_TEST_TWO_TAILED: return 2.0 * fmin(p, 1.0 - p);
        case SCI_TEST_LEFT_TAIL:  return p;
        case SCI_TEST_RIGHT_TAIL: return 1.0 - p;
    }
    return p;
}

static double ttest_critical(double df, double alpha, SCI_TestTail tail) {
    double p = (tail == SCI_TEST_TWO_TAILED) ? 1.0 - alpha/2.0 : 1.0 - alpha;
    /* Approximate via normal quantile for simplicity
     * (exact would require inverse t-CDF) */
    (void)df;
    return sci_normal_quantile(p);
}

SCI_TestResult sci_ttest_1sample(const double *data, int n, double mu0,
                                  double alpha, SCI_TestTail tail) {
    SCI_TestResult res;
    memset(&res, 0, sizeof(res));
    res.test_type = SCI_HTEST_T_ONESAMPLE;
    res.alpha = alpha;
    res.tail = tail;

    if (!data || n < 2) { res.p_value = 1.0; return res; }

    SCI_SummaryStats stats = sci_compute_summary(data, n);
    double se = stats.sd / sqrt((double)n);
    if (se < 1e-15) {
        res.statistic = (stats.mean == mu0) ? 0.0 : INFINITY;
        res.p_value = (stats.mean == mu0) ? 1.0 : 0.0;
        res.reject_h0 = (stats.mean != mu0);
        return res;
    }

    res.statistic = (stats.mean - mu0) / se;
    res.df = n - 1;
    res.p_value = ttest_pvalue(res.statistic, (double)res.df, tail);
    res.critical_val = ttest_critical((double)res.df, alpha, tail);
    res.reject_h0 = (res.p_value < alpha);

    /* Effect size: Cohen d for one-sample */
    if (stats.sd > 0.0)
        res.effect_size = fabs(stats.mean - mu0) / stats.sd;

    /* Confidence interval for mean */
    double t_crit = sci_normal_quantile(1.0 - alpha/2.0);
    res.conf_lo = stats.mean - t_crit * se;
    res.conf_hi = stats.mean + t_crit * se;
    return res;
}

SCI_TestResult sci_ttest_2sample(const double *data1, int n1,
                                  const double *data2, int n2,
                                  double alpha, SCI_TestTail tail) {
    SCI_TestResult res;
    memset(&res, 0, sizeof(res));
    res.test_type = SCI_HTEST_T_TWOSAMPLE;
    res.alpha = alpha;
    res.tail = tail;

    if (!data1 || !data2 || n1 < 2 || n2 < 2) { res.p_value = 1.0; return res; }

    SCI_SummaryStats s1 = sci_compute_summary(data1, n1);
    SCI_SummaryStats s2 = sci_compute_summary(data2, n2);

    double v1 = s1.variance / n1;
    double v2 = s2.variance / n2;
    double se = sqrt(v1 + v2);
    if (se < 1e-15) {
        res.statistic = (s1.mean == s2.mean) ? 0.0 : INFINITY;
        res.p_value = (s1.mean == s2.mean) ? 1.0 : 0.0;
        res.reject_h0 = (s1.mean != s2.mean);
        return res;
    }

    res.statistic = (s1.mean - s2.mean) / se;
    /* Welch-Satterthwaite df */
    double num = (v1 + v2) * (v1 + v2);
    double den = v1*v1/(n1-1) + v2*v2/(n2-1);
    res.df = (int)(num / den);
    if (res.df < 1) res.df = 1;

    res.p_value = ttest_pvalue(res.statistic, (double)res.df, tail);
    res.critical_val = ttest_critical((double)res.df, alpha, tail);
    res.reject_h0 = (res.p_value < alpha);

    /* Cohen d for independent groups */
    double sp = sqrt(((n1-1)*s1.variance + (n2-1)*s2.variance) / (n1+n2-2));
    if (sp > 0.0) res.effect_size = fabs(s1.mean - s2.mean) / sp;

    double t_crit = sci_normal_quantile(1.0 - alpha/2.0);
    res.conf_lo = (s1.mean - s2.mean) - t_crit * se;
    res.conf_hi = (s1.mean - s2.mean) + t_crit * se;
    return res;
}

SCI_TestResult sci_ttest_paired(const double *data1, const double *data2,
                                 int n, double alpha, SCI_TestTail tail) {
    SCI_TestResult res;
    memset(&res, 0, sizeof(res));
    res.test_type = SCI_HTEST_T_PAIRED;
    res.alpha = alpha;
    res.tail = tail;

    if (!data1 || !data2 || n < 2) { res.p_value = 1.0; return res; }

    double *diff = (double*)malloc(n * sizeof(double));
    if (!diff) return res;
    for (int i = 0; i < n; i++) diff[i] = data1[i] - data2[i];

    res = sci_ttest_1sample(diff, n, 0.0, alpha, tail);
    res.test_type = SCI_HTEST_T_PAIRED;
    free(diff);
    return res;
}

/* ================================================================
 * L5: Chi-Squared Test for Independence
 *
 * chi2 = sum((O_ij - E_ij)^2 / E_ij)
 * E_ij = row_i_total * col_j_total / grand_total
 * df = (rows-1)*(cols-1)
 *
 * Pearson (1900): Under H0, chi2 ~ chi2(df) asymptotically.
 * ================================================================ */

SCI_TestResult sci_chisq_test(const int *observed, int rows, int cols,
                               double alpha) {
    SCI_TestResult res;
    memset(&res, 0, sizeof(res));
    res.test_type = SCI_HTEST_CHI_SQUARED;
    res.alpha = alpha;
    res.tail = SCI_TEST_RIGHT_TAIL;

    if (!observed || rows < 2 || cols < 1) { res.p_value = 1.0; return res; }

    double total = 0.0;
    double *row_tot = (double*)calloc(rows, sizeof(double));
    double *col_tot = (double*)calloc(cols, sizeof(double));
    if (!row_tot || !col_tot) { free(row_tot); free(col_tot); return res; }

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            double o = observed[i * cols + j];
            total += o;
            row_tot[i] += o;
            col_tot[j] += o;
        }
    }

    double chi2 = 0.0;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (row_tot[i] == 0.0 || col_tot[j] == 0.0) continue;
            double expected = row_tot[i] * col_tot[j] / total;
            if (expected > 0.0) {
                double o = observed[i * cols + j];
                chi2 += (o - expected) * (o - expected) / expected;
            }
        }
    }

    res.statistic = chi2;
    res.df = (rows - 1) * (cols - 1);
    if (res.df < 1) res.df = 1;
    res.p_value = 1.0 - sci_chisq_cdf(chi2, (double)res.df);
    res.reject_h0 = (res.p_value < alpha);
    res.effect_size = sqrt(chi2 / total) / sqrt(fmin(rows-1, cols-1));

    free(row_tot); free(col_tot);
    return res;
}

/* ================================================================
 * L5: Kolmogorov-Smirnov One-Sample Test
 *
 * D_n = max_i |F_n(x_i) - F(x_i)|
 * Asymptotic: P(sqrt(n) D_n <= x) -> K(x) (Kolmogorov distribution).
 * ================================================================ */

SCI_TestResult sci_ks_test(const double *data, int n,
                            const SCI_Distribution *dist, double alpha) {
    SCI_TestResult res;
    memset(&res, 0, sizeof(res));
    res.test_type = SCI_HTEST_KOLMOGOROV_S;
    res.alpha = alpha;

    if (!data || !dist || n < 5) { res.p_value = 1.0; return res; }

    double *sorted = (double*)malloc(n * sizeof(double));
    if (!sorted) return res;
    memcpy(sorted, data, n * sizeof(double));
    sci_quicksort(sorted, n);

    double d_max = 0.0;
    for (int i = 0; i < n; i++) {
        double Fn = (double)(i + 1) / n;
        double Fn_prev = (double)i / n;
        double Fx = sci_dist_cdf(dist, sorted[i]);
        double d1 = fabs(Fn - Fx);
        double d2 = fabs(Fx - Fn_prev);
        if (d1 > d_max) d_max = d1;
        if (d2 > d_max) d_max = d2;
    }

    res.statistic = d_max;
    /* Kolmogorov asymptotic approximation */
    double lambda = (sqrt((double)n) + 0.12 + 0.11 / sqrt((double)n)) * d_max;
    res.p_value = 2.0 * exp(-2.0 * lambda * lambda);
    if (res.p_value > 1.0) res.p_value = 1.0;
    res.reject_h0 = (res.p_value < alpha);

    free(sorted);
    return res;
}

/* ================================================================
 * L5: Mann-Whitney U Test
 *
 * Nonparametric alternative to independent t-test.
 * Uses normal approximation for large samples.
 * ================================================================ */

SCI_TestResult sci_mann_whitney_u(const double *data1, int n1,
                                   const double *data2, int n2,
                                   double alpha, SCI_TestTail tail) {
    SCI_TestResult res;
    memset(&res, 0, sizeof(res));
    res.test_type = SCI_HTEST_MANN_WHITNEY;
    res.alpha = alpha;
    res.tail = tail;

    if (!data1 || !data2 || n1 < 3 || n2 < 3) { res.p_value = 1.0; return res; }

    int N = n1 + n2;
    double *combined = (double*)malloc(N * sizeof(double));
    int *group = (int*)malloc(N * sizeof(int));
    if (!combined || !group) { free(combined); free(group); return res; }

    for (int i = 0; i < n1; i++) { combined[i] = data1[i]; group[i] = 0; }
    for (int i = 0; i < n2; i++) { combined[n1+i] = data2[i]; group[n1+i] = 1; }

    /* Sort by value (simple bubble for correctness, quicksort would need parallel) */
    for (int i = 0; i < N - 1; i++) {
        for (int j = 0; j < N - 1 - i; j++) {
            if (combined[j] > combined[j+1]) {
                double t = combined[j]; combined[j]=combined[j+1]; combined[j+1]=t;
                int tg = group[j]; group[j]=group[j+1]; group[j+1]=tg;
            }
        }
    }

    /* Assign ranks (average for ties) */
    double *ranks = (double*)malloc(N * sizeof(double));
    if (!ranks) { free(combined); free(group); return res; }

    int pos = 0;
    while (pos < N) {
        int run_end = pos;
        while (run_end + 1 < N && fabs(combined[run_end+1]-combined[pos]) < 1e-12)
            run_end++;
        double avg_rank = (pos + run_end + 2) / 2.0; /* 1-indexed ranks */
        for (int i = pos; i <= run_end; i++) ranks[i] = avg_rank;
        pos = run_end + 1;
    }

    double R1 = 0.0;
    for (int i = 0; i < N; i++)
        if (group[i] == 0) R1 += ranks[i];

    double U1 = R1 - n1 * (n1 + 1.0) / 2.0;
    double U2 = (double)n1 * n2 - U1;
    double U = fmin(U1, U2);

    double mu = n1 * n2 / 2.0;
    double sigma = sqrt(n1 * n2 * (N + 1.0) / 12.0);

    res.statistic = U;
    double z = (U - mu) / sigma;
    res.p_value = 2.0 * (1.0 - sci_normal_cdf(fabs(z)));
    if (tail == SCI_TEST_LEFT_TAIL) res.p_value = sci_normal_cdf(z);
    if (tail == SCI_TEST_RIGHT_TAIL) res.p_value = 1.0 - sci_normal_cdf(z);
    res.reject_h0 = (res.p_value < alpha);
    res.effect_size = fabs(z) / sqrt((double)N);

    free(combined); free(group); free(ranks);
    return res;
}

/* ================================================================
 * L5: One-Way ANOVA (Fisher 1921)
 * ================================================================ */

SCI_AnovaResult sci_anova_oneway(const double *groups[], const int sizes[],
                                  int k, double alpha) {
    SCI_AnovaResult res;
    memset(&res, 0, sizeof(res));
    res.type = SCI_ANOVA_ONEWAY;
    res.significant = false;

    if (!groups || !sizes || k < 2) return res;

    int N = 0;
    double grand_sum = 0.0, grand_sum_sq = 0.0;
    for (int j = 0; j < k; j++) {
        if (!groups[j] || sizes[j] <= 0) return res;
        N += sizes[j];
        for (int i = 0; i < sizes[j]; i++) {
            grand_sum += groups[j][i];
            grand_sum_sq += groups[j][i] * groups[j][i];
        }
    }

    double grand_mean = grand_sum / N;
    double ss_total = grand_sum_sq - grand_sum * grand_sum / N;

    res.ss_between = 0.0;
    for (int j = 0; j < k; j++) {
        double group_sum = 0.0;
        for (int i = 0; i < sizes[j]; i++)
            group_sum += groups[j][i];
        double group_mean = group_sum / sizes[j];
        res.ss_between += sizes[j] * (group_mean - grand_mean)
                        * (group_mean - grand_mean);
    }

    res.ss_within = ss_total - res.ss_between;
    res.ss_total = ss_total;
    res.df_between = k - 1;
    res.df_within = N - k;
    res.ms_between = res.ss_between / res.df_between;
    res.ms_within = (res.df_within > 0) ? res.ss_within / res.df_within : 0.0;

    if (res.ms_within > 0.0) {
        res.f_statistic = res.ms_between / res.ms_within;
        res.p_value = 1.0 - sci_f_dist_cdf(res.f_statistic,
                        (double)res.df_between, (double)res.df_within);
    }
    res.eta_squared = (res.ss_total > 0.0) ? res.ss_between / res.ss_total : 0.0;
    res.significant = (res.p_value < alpha);
    return res;
}

void sci_anova_twoway(const double *data, int rows, int cols,
                       double alpha,
                       SCI_AnovaResult *result_a,
                       SCI_AnovaResult *result_b,
                       SCI_AnovaResult *result_ab) {
    memset(result_a, 0, sizeof(SCI_AnovaResult));
    memset(result_b, 0, sizeof(SCI_AnovaResult));
    memset(result_ab, 0, sizeof(SCI_AnovaResult));
    if (!data || rows < 2 || cols < 2) return;

    int N = rows * cols;
    double total = 0.0;
    for (int i = 0; i < N; i++) total += data[i];
    double gm = total / N;

    /* SS_A (rows) */
    double ss_a = 0.0;
    for (int i = 0; i < rows; i++) {
        double rs = 0.0;
        for (int j = 0; j < cols; j++) rs += data[i*cols+j];
        ss_a += cols * (rs/cols - gm) * (rs/cols - gm);
    }
    /* SS_B (cols) */
    double ss_b = 0.0;
    for (int j = 0; j < cols; j++) {
        double cs = 0.0;
        for (int i = 0; i < rows; i++) cs += data[i*cols+j];
        ss_b += rows * (cs/rows - gm) * (cs/rows - gm);
    }
    /* SS_cells */
    double ss_cells = 0.0;
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            ss_cells += (data[i*cols+j] - gm) * (data[i*cols+j] - gm);
    /* SS_AB = SS_cells - SS_A - SS_B */
    double ss_ab = ss_cells - ss_a - ss_b;
    if (ss_ab < 0.0) ss_ab = 0.0;

    result_a->f_statistic = (ss_a/(rows-1)) / (ss_ab/((rows-1)*(cols-1)));
    result_a->p_value = 1.0 - sci_f_dist_cdf(result_a->f_statistic,
        (double)(rows-1), (double)((rows-1)*(cols-1)));
    result_a->significant = (result_a->p_value < alpha);

    result_b->f_statistic = (ss_b/(cols-1)) / (ss_ab/((rows-1)*(cols-1)));
    result_b->p_value = 1.0 - sci_f_dist_cdf(result_b->f_statistic,
        (double)(cols-1), (double)((rows-1)*(cols-1)));
    result_b->significant = (result_b->p_value < alpha);
}

/* ================================================================
 * L5: OLS Regression via Normal Equations
 *
 * beta = (X^T X)^{-1} X^T y
 * Uses Cholesky decomposition for solving through X^T X.
 * ================================================================ */

bool sci_regression_ols(const double *X, const double *y,
                         int n_obs, int n_pred,
                         SCI_RegressionResult *result) {
    if (!X || !y || !result || n_obs <= n_pred || n_pred < 1) return false;
    memset(result, 0, sizeof(SCI_RegressionResult));
    result->type = SCI_REG_OLS;
    result->n_obs = n_obs;
    result->n_predictors = n_pred;

    int p = n_pred + 1; /* coefficients including intercept */
    result->coefficients = (double*)calloc(p, sizeof(double));
    result->std_errors = (double*)calloc(p, sizeof(double));
    result->t_stats = (double*)calloc(p, sizeof(double));
    result->p_values = (double*)calloc(p, sizeof(double));
    if (!result->coefficients || !result->std_errors ||
        !result->t_stats || !result->p_values) {
        sci_regression_free(result); return false;
    }

    /* Build X^T X and X^T y (with intercept column of 1s) */
    /* For simplicity, handle small dimensions with explicit solve */
    double *XTX = (double*)calloc(p * p, sizeof(double));
    double *XTy = (double*)calloc(p, sizeof(double));
    if (!XTX || !XTy) { free(XTX); free(XTy); sci_regression_free(result); return false; }

    for (int i = 0; i < n_obs; i++) {
        double *xi = (double*)&X[i * n_pred];
        /* Intercept column (virtual) */
        XTX[0] += 1.0;
        for (int j = 0; j < n_pred; j++)
            XTX[j + 1] += xi[j];
        for (int j = 0; j < n_pred; j++)
            XTX[(j+1)*p] += xi[j];
        for (int j = 0; j < n_pred; j++)
            for (int k = 0; k < n_pred; k++)
                XTX[(j+1)*p + (k+1)] += xi[j] * xi[k];
        XTy[0] += y[i];
        for (int j = 0; j < n_pred; j++)
            XTy[j+1] += xi[j] * y[i];
    }

    /* Fill symmetric part */
    for (int j = 0; j < p; j++)
        for (int k = 0; k < j; k++)
            XTX[k*p + j] = XTX[j*p + k];

    /* Solve XTX * beta = XTy via Gaussian elimination */
    double *A = (double*)malloc(p * p * sizeof(double));
    double *B = (double*)malloc(p * sizeof(double));
    if (!A || !B) { free(A); free(B); free(XTX); free(XTy); sci_regression_free(result); return false; }
    memcpy(A, XTX, p*p*sizeof(double));
    memcpy(B, XTy, p*sizeof(double));

    for (int k = 0; k < p; k++) {
        int max_row = k;
        for (int i = k+1; i < p; i++)
            if (fabs(A[i*p + k]) > fabs(A[max_row*p + k])) max_row = i;
        if (max_row != k) {
            for (int j = 0; j < p; j++) {
                double t = A[k*p+j]; A[k*p+j] = A[max_row*p+j]; A[max_row*p+j] = t;
            }
            double t = B[k]; B[k] = B[max_row]; B[max_row] = t;
        }
        if (fabs(A[k*p+k]) < 1e-15) { free(A); free(B); free(XTX); free(XTy); sci_regression_free(result); return false; }
        for (int i = k+1; i < p; i++) {
            double factor = A[i*p+k] / A[k*p+k];
            for (int j = k; j < p; j++) A[i*p+j] -= factor * A[k*p+j];
            B[i] -= factor * B[k];
        }
    }

    for (int i = p-1; i >= 0; i--) {
        double sum = B[i];
        for (int j = i+1; j < p; j++) sum -= A[i*p+j] * result->coefficients[j];
        result->coefficients[i] = sum / A[i*p+i];
    }

    /* Compute predicted values and residuals */
    double *y_hat = (double*)calloc(n_obs, sizeof(double));
    double ss_res = 0.0, ss_tot = 0.0;
    double y_mean = 0.0;
    for (int i = 0; i < n_obs; i++) y_mean += y[i];
    y_mean /= n_obs;

    for (int i = 0; i < n_obs; i++) {
        y_hat[i] = result->coefficients[0];
        for (int j = 0; j < n_pred; j++)
            y_hat[i] += result->coefficients[j+1] * X[i*n_pred+j];
        ss_res += (y[i] - y_hat[i]) * (y[i] - y_hat[i]);
        ss_tot += (y[i] - y_mean) * (y[i] - y_mean);
    }

    result->r_squared = (ss_tot > 0.0) ? 1.0 - ss_res/ss_tot : 0.0;
    result->adj_r_squared = 1.0 - (1.0-result->r_squared)
                          * (n_obs-1)/(n_obs-p);
    result->rmse = sqrt(ss_res / (n_obs - p));

    /* Standard errors from diagonal of sigma^2 * (X^T X)^{-1} */
    double sigma2 = ss_res / (n_obs - p);
    for (int j = 0; j < p; j++) {
        result->std_errors[j] = sqrt(sigma2 * fabs(XTX[j*p+j]));
        if (result->std_errors[j] > 1e-15) {
            result->t_stats[j] = result->coefficients[j]
                                / result->std_errors[j];
            result->p_values[j] = 2.0 * (1.0 - sci_normal_cdf(
                fabs(result->t_stats[j])));
        }
    }

    /* Overall F-test */
    if (ss_res > 0.0 && n_pred > 0) {
        result->f_statistic = ((ss_tot - ss_res)/n_pred)
                             / (ss_res/(n_obs - p));
        result->f_p_value = 1.0 - sci_f_dist_cdf(
            result->f_statistic, (double)n_pred, (double)(n_obs-p));
    }

    /* AIC = n*log(SSE/n) + 2p */
    result->aic = n_obs * log(ss_res/n_obs) + 2.0 * p;
    /* BIC = n*log(SSE/n) + p*log(n) */
    result->bic = n_obs * log(ss_res/n_obs) + p * log((double)n_obs);

    free(y_hat); free(A); free(B); free(XTX); free(XTy);
    return true;
}

void sci_regression_free(SCI_RegressionResult *result) {
    if (!result) return;
    free(result->coefficients);
    free(result->std_errors);
    free(result->t_stats);
    free(result->p_values);
    memset(result, 0, sizeof(SCI_RegressionResult));
}

double sci_regression_predict(const SCI_RegressionResult *result,
                               const double *x_new) {
    if (!result || !result->coefficients || !x_new) return NAN;
    double y = result->coefficients[0]; /* intercept */
    for (int j = 0; j < result->n_predictors; j++)
        y += result->coefficients[j+1] * x_new[j];
    return y;
}

/* ================================================================
 * L7-L8: Bootstrap CI, Multiple Testing Correction
 * ================================================================ */

void sci_bootstrap_mean(const double *data, int n, int B, double alpha,
                         unsigned int seed, double *conf_lo, double *conf_hi) {
    if (!data || n <= 0 || B <= 0 || !conf_lo || !conf_hi) return;

    double *boot_means = (double*)malloc(B * sizeof(double));
    double *resample = (double*)malloc(n * sizeof(double));
    if (!boot_means || !resample) { free(boot_means); free(resample); return; }

    unsigned int lcg = (seed != 0) ? seed : 12345;

    for (int b = 0; b < B; b++) {
        for (int i = 0; i < n; i++) {
            lcg = 1103515245 * lcg + 12345;
            int idx = ((lcg >> 16) & 0x7FFF) % n;
            resample[i] = data[idx];
        }
        double sum = 0.0;
        for (int i = 0; i < n; i++) sum += resample[i];
        boot_means[b] = sum / n;
    }

    sci_quicksort(boot_means, B);
    int lo_idx = (int)(B * alpha / 2.0);
    int hi_idx = (int)(B * (1.0 - alpha / 2.0));
    if (lo_idx < 0) lo_idx = 0;
    if (hi_idx >= B) hi_idx = B - 1;
    *conf_lo = boot_means[lo_idx];
    *conf_hi = boot_means[hi_idx];

    free(boot_means); free(resample);
}

void sci_holm_bonferroni(const double *p_values, int m, double alpha,
                          bool *adjusted_out) {
    if (!p_values || !adjusted_out || m <= 0) return;

    /* Create sorted indices */
    int *indices = (int*)malloc(m * sizeof(int));
    double *sorted_p = (double*)malloc(m * sizeof(double));
    if (!indices || !sorted_p) { free(indices); free(sorted_p); return; }

    for (int i = 0; i < m; i++) { indices[i] = i; sorted_p[i] = p_values[i]; }

    /* Sort p-values (simple bubble sort for small m) */
    for (int i = 0; i < m-1; i++)
        for (int j = 0; j < m-1-i; j++)
            if (sorted_p[j] > sorted_p[j+1]) {
                double tp = sorted_p[j]; sorted_p[j]=sorted_p[j+1]; sorted_p[j+1]=tp;
                int ti = indices[j]; indices[j]=indices[j+1]; indices[j+1]=ti;
            }

    /* Holm step-down */
    for (int i = 0; i < m; i++) {
        double adj_alpha = alpha / (m - i);
        adjusted_out[indices[i]] = (sorted_p[i] < adj_alpha);
        /* Once we fail to reject, all remaining fail */
        if (!adjusted_out[indices[i]]) {
            for (int j = i+1; j < m; j++)
                adjusted_out[indices[j]] = false;
            break;
        }
    }

    free(indices); free(sorted_p);
}

void sci_benjamini_hochberg(const double *p_values, int m, double q,
                             bool *significant) {
    if (!p_values || !significant || m <= 0) return;

    int *indices = (int*)malloc(m * sizeof(int));
    double *sorted_p = (double*)malloc(m * sizeof(double));
    if (!indices || !sorted_p) { free(indices); free(sorted_p); return; }

    for (int i = 0; i < m; i++) { indices[i] = i; sorted_p[i] = p_values[i]; }

    for (int i = 0; i < m-1; i++)
        for (int j = 0; j < m-1-i; j++)
            if (sorted_p[j] > sorted_p[j+1]) {
                double tp = sorted_p[j]; sorted_p[j]=sorted_p[j+1]; sorted_p[j+1]=tp;
                int ti = indices[j]; indices[j]=indices[j+1]; indices[j+1]=ti;
            }

    for (int i = 0; i < m; i++)
        significant[i] = false;

    for (int i = m-1; i >= 0; i--) {
        double threshold = q * (i + 1) / m;
        if (sorted_p[i] <= threshold) {
            for (int j = 0; j <= i; j++)
                significant[indices[j]] = true;
            break;
        }
    }

    free(indices); free(sorted_p);
}