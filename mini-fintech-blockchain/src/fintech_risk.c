#include "fintech_risk.h"
#include "fintech_options.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Box-Muller transform for N(0,1) random numbers */

double random_normal_box_muller(void) {
    static int has_spare = 0;
    static double spare;
    if (has_spare) { has_spare = 0; return spare; }
    double u1 = (double)rand() / RAND_MAX;
    double u2 = (double)rand() / RAND_MAX;
    if (u1 < 1e-12) u1 = 1e-12;
    double mag = sqrt(-2.0 * log(u1));
    double z1 = mag * cos(2.0 * M_PI * u2);
    double z2 = mag * sin(2.0 * M_PI * u2);
    spare = z2; has_spare = 1;
    return z1;
}

/* QuickSort (in-place, ascending) for double arrays.
 * Average O(n log n), worst-case O(n^2). */

static void quicksort_rec(double* arr, int low, int high) {
    if (low >= high) return;
    double pivot = arr[high];
    int i = low - 1;
    for (int j = low; j < high; j++) {
        if (arr[j] <= pivot) {
            i++;
            double t = arr[i]; arr[i] = arr[j]; arr[j] = t;
        }
    }
    double t = arr[i + 1]; arr[i + 1] = arr[high]; arr[high] = t;
    int pi = i + 1;
    quicksort_rec(arr, low, pi - 1);
    quicksort_rec(arr, pi + 1, high);
}

void sort_doubles(double* arr, int n) {
    if (!arr || n <= 1) return;
    quicksort_rec(arr, 0, n - 1);
}

/* Historical VaR: alpha-quantile of sorted historical returns.
 * Expected Shortfall: average loss beyond VaR threshold. */

int var_historical(RiskMetrics* rm, const double* returns, int n) {
    if (!rm || !returns || n < 10) return -1;
    double* sorted = (double*)malloc((size_t)n * sizeof(double));
    if (!sorted) return -1;
    memcpy(sorted, returns, (size_t)n * sizeof(double));
    sort_doubles(sorted, n);
    int idx = (int)((1.0 - rm->confidence_level) * n);
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    double var_ret = sorted[idx];
    rm->var_percent = -var_ret;
    rm->var_value = rm->portfolio_value * rm->var_percent;
    rm->method = VAR_HISTORICAL;
    rm->expected_shortfall = expected_shortfall_historical(returns, n, var_ret);
    if (rm->horizon_days > 1) {
        double sf = sqrt((double)rm->horizon_days);
        rm->var_value *= sf;
        rm->var_percent *= sf;
        rm->expected_shortfall *= sf;
    }
    free(sorted);
    return 0;
}

/* Parametric VaR (Variance-Covariance method).
 * Assumes normally distributed returns: VaR = -(mu + z*sigma)*V */

int var_parametric(RiskMetrics* rm, double mean, double stddev) {
    if (!rm) return -1;
    double alpha = rm->confidence_level;
    double z = norm_inv_cdf(1.0 - alpha);
    rm->mean_return = mean;
    rm->stddev = stddev;
    rm->var_percent = -(mean + z * stddev);
    if (rm->var_percent < 0.0) rm->var_percent = 0.0;
    rm->var_value = rm->portfolio_value * rm->var_percent;
    rm->method = VAR_PARAMETRIC;
    double phi_z = norm_pdf(z);
    double es_pct = -(mean + stddev * phi_z / (1.0 - alpha));
    if (es_pct < 0.0) es_pct = 0.0;
    rm->expected_shortfall = rm->portfolio_value * es_pct;
    if (rm->horizon_days > 1) {
        double sf = sqrt((double)rm->horizon_days);
        rm->var_value *= sf;
        rm->var_percent *= sf;
        rm->expected_shortfall *= sf;
    }
    return 0;
}

/* Monte Carlo VaR: simulate N(mu,sigma) paths, sort, take quantile. */

int var_monte_carlo(RiskMetrics* rm, double mean, double stddev, int n_sim) {
    if (!rm || n_sim < 100) return -1;
    if (n_sim > RISK_MAX_SIMULATIONS) n_sim = RISK_MAX_SIMULATIONS;
    double* sim = (double*)malloc((size_t)n_sim * sizeof(double));
    if (!sim) return -1;
    srand((unsigned int)time(NULL));
    for (int i = 0; i < n_sim; i++)
        sim[i] = mean + stddev * random_normal_box_muller();
    sort_doubles(sim, n_sim);
    int idx = (int)((1.0 - rm->confidence_level) * n_sim);
    if (idx < 0) idx = 0;
    if (idx >= n_sim) idx = n_sim - 1;
    double var_ret = sim[idx];
    rm->var_percent = -var_ret;
    if (rm->var_percent < 0.0) rm->var_percent = 0.0;
    rm->var_value = rm->portfolio_value * rm->var_percent;
    rm->method = VAR_MONTE_CARLO;
    rm->expected_shortfall = expected_shortfall_historical(sim, n_sim, var_ret);
    if (rm->horizon_days > 1) {
        double sf = sqrt((double)rm->horizon_days);
        rm->var_value *= sf;
        rm->var_percent *= sf;
        rm->expected_shortfall *= sf;
    }
    free(sim);
    return 0;
}

/* Expected Shortfall (CVaR): E[Loss | Loss > VaR] */

double expected_shortfall_historical(const double* returns, int n,
                                      double var_threshold) {
    if (!returns || n < 2) return 0.0;
    double sum = 0.0;
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (returns[i] < var_threshold) { sum += returns[i]; count++; }
    }
    if (count == 0) return -var_threshold;
    return -sum / count;
}

/* Basel III Market Risk Capital:
 * MRC = max(VaR_t-1, m_c * VaR_60day_avg) + SVaR
 * where m_c >= 3 (multiplier from backtesting exceptions) */

void basel_capital_calculate(BaselCapital* bc) {
    if (!bc) return;
    double m = (bc->multiplier < 3.0) ? 3.0 : bc->multiplier;
    double vc = bc->var_previous_day;
    double ac = m * bc->var_60day_avg;
    bc->market_risk_capital = (vc > ac ? vc : ac) + bc->stressed_var;
    bc->total_risk_capital = bc->market_risk_capital
                           + bc->credit_risk_capital
                           + bc->operational_risk_capital;
    double rwa = bc->total_risk_capital / 0.08;
    bc->cet1_ratio = (rwa > 0.0) ? bc->tier1_capital / rwa : 0.0;
}

/* Convenience: compute VaR from raw return series */

double portfolio_var_from_returns(const double* returns, int n,
                                   double conf, double pv) {
    RiskMetrics rm;
    memset(&rm, 0, sizeof(rm));
    rm.confidence_level = conf;
    rm.portfolio_value = pv;
    rm.horizon_days = 1;
    if (var_historical(&rm, returns, n) != 0) return 0.0;
    return rm.var_value;
}

void risk_metrics_print(const RiskMetrics* rm) {
    if (!rm) return;
    const char* names[] = {"Historical","Parametric","MonteCarlo"};
    printf("VaR(%.0f%%) [%s] Portfolio=%.0f VaR=%.2f (%.4f%%) ES=%.2f\\n",
           rm->confidence_level*100.0, names[rm->method],
           rm->portfolio_value, rm->var_value, rm->var_percent*100.0,
           rm->expected_shortfall);
}

void basel_capital_print(const BaselCapital* bc) {
    if (!bc) return;
    printf("Basel III: MktRisk=%.0f CrdRisk=%.0f OpRisk=%.0f Total=%.0f CET1=%.2f%%\\n",
           bc->market_risk_capital, bc->credit_risk_capital,
           bc->operational_risk_capital, bc->total_risk_capital,
           bc->cet1_ratio*100.0);
}
