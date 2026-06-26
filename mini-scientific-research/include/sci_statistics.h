#ifndef MINI_SCI_STATISTICS_H
#define MINI_SCI_STATISTICS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* ================================================================
 *  sci_statistics -- Statistical Computing for Scientific Research
 *
 *  L1: Core types -- distributions, hypothesis test configs, ANOVA
 *      tables, regression models
 *  L2: Core concepts -- probability distributions (PDF/CDF), Type I/II
 *      error, p-value, confidence intervals, effect size
 *  L3: Engineering structures -- streaming statistics (Welford), test
 *      pipelines, bootstrap resampling, power analysis
 *  L4: Standards/Theorems -- Central Limit Theorem (Laplace 1810, Lyapunov
 *      1901), Law of Large Numbers, Neyman-Pearson Lemma, Gauss-Markov
 *      Theorem, Bayes Theorem, Cramer-Rao bound
 *  L5: Algorithms -- MLE estimation, t-test (Student/Gosset 1908),
 *      ANOVA (Fisher 1921), OLS regression, K-S test, chi-squared test,
 *      bootstrap confidence intervals (Efron 1979)
 *  L6: Canonical problems -- Hypothesis testing, linear regression,
 *      distribution fitting, ANOVA analysis
 *  L7: Applications -- Medical trials analysis, psychometric evaluation,
 *      econometric modeling, quality control
 *  L8: Advanced -- Mixed-effects models, Bayesian hierarchical,
 *      nonparametric statistics (Mann-Whitney, Kruskal-Wallis),
 *      multiple testing correction (Bonferroni, FDR/Benjamini-Hochberg)
 *  L9: Industry -- R language, SciPy.stats, JASP, SAS, SPSS
 *
 *  References:
 *  - Casella & Berger, "Statistical Inference" (2nd ed, 2002)
 *  - Wasserman, "All of Statistics" (2004)
 *  - Fisher, "Statistical Methods for Research Workers" (1925)
 *  - Efron & Tibshirani, "An Introduction to the Bootstrap" (1993)
 * ================================================================ */

/* -----------------------------------------------------------------
 * L1: Core Type Definitions
 * ----------------------------------------------------------------- */

typedef enum {
    SCI_DIST_NORMAL       = 0,
    SCI_DIST_UNIFORM      = 1,
    SCI_DIST_EXPONENTIAL  = 2,
    SCI_DIST_POISSON      = 3,
    SCI_DIST_BINOMIAL     = 4,
    SCI_DIST_CHI_SQUARED  = 5,
    SCI_DIST_STUDENT_T    = 6,
    SCI_DIST_F            = 7,
    SCI_DIST_GAMMA        = 8,
    SCI_DIST_BETA         = 9,
    SCI_DIST_CAUCHY       = 10,
    SCI_DIST_LOG_NORMAL   = 11,
    SCI_DIST_WEIBULL      = 12,
} SCI_DistributionType;

typedef enum {
    SCI_TEST_TWO_TAILED = 0,
    SCI_TEST_LEFT_TAIL  = 1,
    SCI_TEST_RIGHT_TAIL = 2,
} SCI_TestTail;

typedef enum {
    SCI_HTEST_T_ONESAMPLE   = 0,
    SCI_HTEST_T_TWOSAMPLE   = 1,
    SCI_HTEST_T_PAIRED      = 2,
    SCI_HTEST_Z_PROPORTION  = 3,
    SCI_HTEST_CHI_SQUARED   = 4,
    SCI_HTEST_F_VARIANCE    = 5,
    SCI_HTEST_KOLMOGOROV_S  = 6,
    SCI_HTEST_MANN_WHITNEY  = 7,
    SCI_HTEST_WILCOXON      = 8,
} SCI_HTestType;

typedef enum {
    SCI_ANOVA_ONEWAY    = 0,
    SCI_ANOVA_TWOWAY    = 1,
    SCI_ANOVA_REPEATED  = 2,
} SCI_AnovaType;

typedef enum {
    SCI_REG_OLS         = 0,  /* Ordinary Least Squares */
    SCI_REG_WLS         = 1,  /* Weighted Least Squares */
    SCI_REG_RIDGE       = 2,  /* Ridge regression (L2) */
    SCI_REG_LASSO       = 3,  /* LASSO (L1) */
} SCI_RegType;

typedef struct {
    SCI_DistributionType dist;
    union {
        struct { double mean; double sd; } normal;
        struct { double a; double b; } uniform;
        struct { double rate; } exponential;
        struct { double lambda; } poisson;
        struct { int n; double p; } binomial;
        struct { double df; } chisq;
        struct { double df; } student_t;
        struct { double df1; double df2; } f_dist;
        struct { double shape; double scale; } gamma;
        struct { double alpha; double beta; } beta_dist;
        struct { double location; double scale; } cauchy;
        struct { double mu; double sigma; } lognormal;
        struct { double shape; double scale; } weibull;
    } params;
} SCI_Distribution;

typedef struct {
    int     n;               /* Sample size */
    double  mean;            /* Sample mean */
    double  variance;        /* Sample variance (unbiased) */
    double  sd;              /* Sample standard deviation */
    double  skewness;        /* Fisher-Pearson skewness */
    double  kurtosis;        /* Excess kurtosis */
    double  min, max;        /* Range */
    double  median;          /* 50th percentile */
    double  q1, q3;          /* 25th, 75th percentiles */
    double  sum, sum_sq;     /* Running sums */
} SCI_SummaryStats;

typedef struct {
    SCI_HTestType test_type;
    double        statistic;    /* Test statistic */
    double        p_value;      /* p-value */
    double        critical_val; /* Critical value at given alpha */
    double        alpha;        /* Significance level */
    int           df;           /* Degrees of freedom */
    double        effect_size;  /* Cohen d or equivalent */
    bool          reject_h0;    /* Null hypothesis rejected? */
    double        conf_lo;      /* Confidence interval lower bound */
    double        conf_hi;      /* Confidence interval upper bound */
    SCI_TestTail  tail;
} SCI_TestResult;

typedef struct {
    SCI_AnovaType type;
    double  ss_between;     /* Between-group sum of squares */
    double  ss_within;      /* Within-group sum of squares */
    double  ss_total;       /* Total sum of squares */
    int     df_between;     /* Between-group df */
    int     df_within;      /* Within-group df */
    double  ms_between;     /* Between-group mean square */
    double  ms_within;      /* Within-group mean square */
    double  f_statistic;    /* F = MS_between / MS_within */
    double  p_value;
    double  eta_squared;    /* Effect size: SS_between / SS_total */
    bool    significant;    /* p < alpha */
} SCI_AnovaResult;

typedef struct {
    SCI_RegType type;
    int      n_predictors;  /* Number of predictor variables */
    int      n_obs;         /* Number of observations */
    double  *coefficients;  /* Beta coefficients (size n_predictors+1) */
    double  *std_errors;    /* Standard errors of coefficients */
    double  *t_stats;       /* t-statistics for coefficients */
    double  *p_values;      /* p-values for coefficients */
    double   r_squared;     /* Coefficient of determination */
    double   adj_r_squared; /* Adjusted R-squared */
    double   f_statistic;   /* Overall F-test */
    double   f_p_value;
    double   rmse;          /* Root mean squared error */
    double   aic;           /* Akaike Information Criterion */
    double   bic;           /* Bayesian Information Criterion */
} SCI_RegressionResult;

/* -----------------------------------------------------------------
 * L2-L5: Descriptive Statistics
 * ----------------------------------------------------------------- */

/**
 * Compute summary statistics (mean, variance, skewness, etc.).
 * Uses Welford algorithm for numerically stable variance computation.
 * Welford (1962): M_k = M_{k-1} + (x_k - M_{k-1})/k
 *                S_k = S_{k-1} + (x_k - M_{k-1})(x_k - M_k)
 * Complexity: O(n).
 */
SCI_SummaryStats sci_compute_summary(const double *data, int n);

/**
 * Streaming update of running statistics (Welford online algorithm).
 * Allows computing mean/variance without storing all data.
 * Complexity: O(1) per update.
 *
 * @param stats   In/out statistics accumulator
 * @param x       New data point
 * @param n       Current count before this observation
 */
void sci_stats_update(SCI_SummaryStats *stats, double x, int n);

/**
 * Merges two streaming statistic accumulators.
 * Chan, Golub & LeVeque (1983) parallel variance algorithm.
 * Complexity: O(1).
 */
SCI_SummaryStats sci_stats_merge(const SCI_SummaryStats *a,
                                  const SCI_SummaryStats *b);

/**
 * Sort an array in place using quicksort.
 * Complexity: O(n log n) average, O(n^2) worst-case.
 *
 * @param data  Array to sort
 * @param n     Number of elements
 */
void sci_quicksort(double *data, int n);

/**
 * Compute percentile of sorted data using linear interpolation.
 * Uses the Hyndman-Fan R7 definition (default in R, NumPy).
 *
 * @param sorted_data  Sorted array
 * @param n            Size of array
 * @param p            Percentile in [0, 100]
 * @return             Percentile value
 */
double sci_percentile(const double *sorted_data, int n, double p);

/* -----------------------------------------------------------------
 * L2: Probability Distributions
 * ----------------------------------------------------------------- */

/**
 * Probability density / mass function for supported distributions.
 * Complexity: O(1).
 */
double sci_dist_pdf(const SCI_Distribution *dist, double x);
double sci_dist_cdf(const SCI_Distribution *dist, double x);
double sci_dist_quantile(const SCI_Distribution *dist, double p);

/**
 * Normal distribution CDF via error function.
 * Phi(x) = 0.5 * (1 + erf(x/sqrt(2)))
 * Uses Abramowitz & Stegun rational approximation (accurate to ~1.5e-7).
 */
double sci_normal_cdf(double x);

/**
 * Inverse normal CDF (quantile function) via rational approximation.
 * Method of Beasley-Springer (1977) / Moro (1995).
 *
 * @param p  Probability in (0,1)
 * @return   Phi^{-1}(p)
 */
double sci_normal_quantile(double p);

/**
 * Student t-distribution CDF using incomplete beta function.
 *
 * @param t   Test statistic
 * @param df  Degrees of freedom
 * @return    P(T <= t)
 */
double sci_student_t_cdf(double t, double df);

/**
 * Chi-squared CDF using regularized incomplete gamma function.
 *
 * @param x   Test statistic
 * @param df  Degrees of freedom
 * @return    P(chi2 <= x)
 */
double sci_chisq_cdf(double x, double df);

/**
 * F-distribution CDF via regularized incomplete beta.
 *
 * @param f    Test statistic
 * @param df1  Numerator degrees of freedom
 * @param df2  Denominator degrees of freedom
 * @return     P(F <= f)
 */
double sci_f_dist_cdf(double f, double df1, double df2);

/**
 * Generate a random sample from a distribution.
 * Uses Box-Muller for normal, inversion for others.
 *
 * @param dist    Distribution parameters
 * @param n       Number of samples requested
 * @param samples Output array (caller-allocated, size n)
 * @param seed    Random seed (0 for time-based)
 */
void sci_dist_sample(const SCI_Distribution *dist, int n,
                      double *samples, unsigned int seed);

/* -----------------------------------------------------------------
 * L5: Hypothesis Testing
 * ----------------------------------------------------------------- */

/**
 * One-sample t-test: H0: mu = mu0.
 *
 * t = (mean - mu0) / (sd / sqrt(n))
 * Under H0, t ~ Student_t(n-1).
 *
 * @param data  Sample data
 * @param n     Sample size
 * @param mu0   Hypothesized mean
 * @param alpha Significance level (e.g., 0.05)
 * @param tail  Two-tailed / one-tailed
 * @return      Test result
 */
SCI_TestResult sci_ttest_1sample(const double *data, int n, double mu0,
                                  double alpha, SCI_TestTail tail);

/**
 * Two-sample independent t-test (Welch approximation).
 * Does NOT assume equal variances. Welch-Satterthwaite df.
 *
 * t = (mean1 - mean2) / sqrt(var1/n1 + var2/n2)
 * df = (var1/n1 + var2/n2)^2 / ((var1/n1)^2/(n1-1) + (var2/n2)^2/(n2-1))
 *
 * Reference: Welch (1947)
 */
SCI_TestResult sci_ttest_2sample(const double *data1, int n1,
                                  const double *data2, int n2,
                                  double alpha, SCI_TestTail tail);

/**
 * Paired t-test: comparison of two related samples.
 * d_i = x_{1i} - x_{2i}, H0: mu_d = 0.
 */
SCI_TestResult sci_ttest_paired(const double *data1, const double *data2,
                                 int n, double alpha, SCI_TestTail tail);

/**
 * Chi-squared test for independence/categorical data.
 *
 * For a 2x2 contingency table or general r x c.
 * chi2 = sum(sum((O_ij - E_ij)^2 / E_ij))
 * E_ij = (row_i_total * col_j_total) / grand_total
 * df = (r-1)(c-1)
 *
 * @param observed Contingency table (row-major, size rows*cols)
 * @param rows     Number of rows
 * @param cols     Number of columns
 * @param alpha    Significance level
 * @return         Test result
 */
SCI_TestResult sci_chisq_test(const int *observed, int rows, int cols,
                               double alpha);

/**
 * Kolmogorov-Smirnov one-sample test.
 * Tests H0: data follows specified distribution.
 *
 * D_n = sup_x |F_n(x) - F(x)| where F_n is empirical CDF.
 * Uses Kolmogorov-Smirnov limit distribution.
 *
 * @param data  Sample data
 * @param n     Sample size
 * @param dist  Hypothesized distribution
 * @param alpha Significance level
 * @return      Test result
 */
SCI_TestResult sci_ks_test(const double *data, int n,
                            const SCI_Distribution *dist, double alpha);

/**
 * Mann-Whitney U test (Wilcoxon rank-sum).
 * Nonparametric alternative to independent t-test.
 * Uses normal approximation for large samples.
 */
SCI_TestResult sci_mann_whitney_u(const double *data1, int n1,
                                   const double *data2, int n2,
                                   double alpha, SCI_TestTail tail);

/* -----------------------------------------------------------------
 * L5: Analysis of Variance (ANOVA)
 * ----------------------------------------------------------------- */

/**
 * One-way ANOVA (Fisher 1921).
 *
 * Tests H0: mu_1 = mu_2 = ... = mu_k vs H1: at least one differs.
 *
 * F = MS_between / MS_within
 * Under H0, F ~ F(k-1, N-k) (assuming normality + homoscedasticity).
 *
 * @param groups Array of pointers to group data arrays
 * @param sizes  Size of each group
 * @param k      Number of groups
 * @param alpha  Significance level
 * @return       ANOVA result table
 */
SCI_AnovaResult sci_anova_oneway(const double *groups[], const int sizes[],
                                  int k, double alpha);

/**
 * Two-way ANOVA with interaction.
 * Tests main effects of factor A, factor B, and A*B interaction.
 *
 * @param data   Row-major matrix (size rows*cols)
 * @param rows   Number of levels of factor A
 * @param cols   Number of levels of factor B
 * @param alpha  Significance level
 * @param result_a  Output: factor A result
 * @param result_b  Output: factor B result
 * @param result_ab Output: interaction result
 */
void sci_anova_twoway(const double *data, int rows, int cols,
                       double alpha,
                       SCI_AnovaResult *result_a,
                       SCI_AnovaResult *result_b,
                       SCI_AnovaResult *result_ab);

/* -----------------------------------------------------------------
 * L5: Regression Analysis
 * ----------------------------------------------------------------- */

/**
 * Ordinary Least Squares regression.
 *
 * y = X * beta + epsilon  (where beta[0] is the intercept)
 * beta_hat = (X^T X)^{-1} X^T y
 *
 * Uses Cholesky decomposition for solving normal equations.
 * Implements the Gauss-Markov theorem: OLS is BLUE under
 * homoscedasticity and uncorrelated errors.
 *
 * R^2 = 1 - SS_res / SS_tot
 * adj R^2 = 1 - (1-R^2)(n-1)/(n-p-1)
 *
 * @param X       Design matrix (size n_obs x n_predictors, row-major)
 * @param y       Response vector (size n_obs)
 * @param n_obs   Number of observations
 * @param n_pred  Number of predictors
 * @param result  Output: regression results (allocated by caller)
 * @return        true if computation succeeded
 */
bool sci_regression_ols(const double *X, const double *y,
                         int n_obs, int n_pred,
                         SCI_RegressionResult *result);

/**
 * Free resources allocated within regression result.
 */
void sci_regression_free(SCI_RegressionResult *result);

/**
 * Predict response for new data using fitted regression model.
 *
 * @param result  Fitted regression model
 * @param x_new   New predictor values (size n_predictors)
 * @return        Predicted y value
 */
double sci_regression_predict(const SCI_RegressionResult *result,
                               const double *x_new);

/* -----------------------------------------------------------------
 * L7-L8: Advanced Methods
 * ----------------------------------------------------------------- */

/**
 * Bootstrap confidence interval for the mean.
 * Resamples with replacement B times, computes mean for each,
 * uses percentile method for CI.
 *
 * Efron (1979): "Bootstrap Methods: Another Look at the Jackknife"
 *
 * @param data    Original sample
 * @param n       Sample size
 * @param B       Number of bootstrap replicates (e.g., 1000)
 * @param alpha   (1-alpha) confidence level
 * @param seed    Random seed
 * @param conf_lo Output: lower confidence bound
 * @param conf_hi Output: upper confidence bound
 */
void sci_bootstrap_mean(const double *data, int n, int B, double alpha,
                         unsigned int seed, double *conf_lo, double *conf_hi);

/**
 * Holm-Bonferroni correction for multiple comparisons.
 * Controls family-wise error rate (FWER).
 *
 * @param p_values     Array of raw p-values (size m)
 * @param m            Number of tests
 * @param alpha        Family-wise significance level
 * @param adjusted_out Output: adjusted flags (true = reject H0 for this test)
 */
void sci_holm_bonferroni(const double *p_values, int m, double alpha,
                          bool *adjusted_out);

/**
 * Benjamini-Hochberg procedure for controlling false discovery rate (FDR).
 * (Benjamini & Hochberg, 1995)
 *
 * @param p_values     Array of raw p-values (size m)
 * @param m            Number of tests
 * @param q            Desired FDR level (e.g., 0.05)
 * @param significant  Output: true for significant tests
 */
void sci_benjamini_hochberg(const double *p_values, int m, double q,
                             bool *significant);

#endif /* MINI_SCI_STATISTICS_H */