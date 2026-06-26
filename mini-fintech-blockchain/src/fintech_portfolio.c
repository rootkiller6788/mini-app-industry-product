#include "fintech_portfolio.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * L4: Modern Portfolio Theory ¡ª Markowitz (1952)
 * E(R_p) = sum(w_i * E(R_i))
 * Var(R_p) = sum_i sum_j w_i w_j Cov(R_i, R_j) = w^T Sigma w
 * Sharpe Ratio = (E(R_p) - R_f) / sigma_p
 * ================================================================ */

Portfolio* portfolio_create(double risk_free_rate) {
    Portfolio* pf = (Portfolio*)calloc(1, sizeof(Portfolio));
    if (!pf) return NULL;
    pf->risk_free_rate = risk_free_rate;
    return pf;
}

int portfolio_add_asset(Portfolio* pf, const char* ticker,
                        double expected_return, double volatility,
                        double weight, int asset_class) {
    if (!pf || pf->asset_count >= PORTFOLIO_MAX_ASSETS) return -1;
    PortfolioAsset* a = &pf->assets[pf->asset_count];
    snprintf(a->ticker, sizeof(a->ticker), "%s", ticker ? ticker : "UNKN");
    a->expected_return = expected_return;
    a->volatility = volatility;
    a->weight = weight;
    a->asset_class = asset_class;
    pf->asset_count++;
    return pf->asset_count - 1;
}

int portfolio_set_covariance(Portfolio* pf, int i, int j, double cov) {
    if (!pf || i < 0 || i >= pf->asset_count
           || j < 0 || j >= pf->asset_count) return -1;
    pf->cov_matrix[i][j] = cov;
    pf->cov_matrix[j][i] = cov;
    return 0;
}

double portfolio_compute_return(const Portfolio* pf) {
    if (!pf || pf->asset_count == 0) return 0.0;
    double er = 0.0;
    for (int i = 0; i < pf->asset_count; i++)
        er += pf->assets[i].weight * pf->assets[i].expected_return;
    return er;
}

double portfolio_compute_variance(Portfolio* pf) {
    if (!pf || pf->asset_count == 0) return 0.0;
    double var = 0.0;
    for (int i = 0; i < pf->asset_count; i++) {
        for (int j = 0; j < pf->asset_count; j++) {
            var += pf->assets[i].weight * pf->assets[j].weight
                   * pf->cov_matrix[i][j];
        }
    }
    pf->portfolio_variance = var;
    return var;
}

double portfolio_compute_stddev(Portfolio* pf) {
    double var = portfolio_compute_variance(pf);
    double sd = sqrt(var);
    if (pf) pf->portfolio_stddev = sd;
    return sd;
}

double portfolio_compute_sharpe_ratio(Portfolio* pf) {
    if (!pf || pf->asset_count == 0) return 0.0;
    double er = portfolio_compute_return(pf);
    double sd = portfolio_compute_stddev(pf);
    if (sd < 1e-12) return 0.0;
    pf->sharpe_ratio = (er - pf->risk_free_rate) / sd;
    return pf->sharpe_ratio;
}

double portfolio_diversification_ratio(const Portfolio* pf) {
    if (!pf || pf->asset_count == 0) return 1.0;
    double weighted_vol = 0.0;
    for (int i = 0; i < pf->asset_count; i++)
        weighted_vol += pf->assets[i].weight * pf->assets[i].volatility;
    double port_vol = pf->portfolio_stddev;
    if (port_vol < 1e-12) return 1.0;
    return weighted_vol / port_vol;
}

/* ================================================================
 * L4: Two-Asset Efficient Frontier (Closed Form)
 * Given 2 assets with known E(R), sigma, and correlation:
 * Port return = w*ER1 + (1-w)*ER2
 * Port variance = w^2*v1 + (1-w)^2*v2 + 2*w*(1-w)*cov12
 * Vary w from 0 to 1 to trace frontier.
 *
 * Max Sharpe ratio (tangency portfolio) for 2 assets has closed form:
 * w1* = ((ER1-Rf)*v2 - (ER2-Rf)*cov12)
 *      / ((ER1-Rf)*v2 + (ER2-Rf)*v1 - (ER1+ER2-2*Rf)*cov12)
 * ================================================================ */

void frontier_two_asset(double er1, double er2, double sd1, double sd2,
                        double corr, double rf, EfficientFrontier* ef) {
    if (!ef) return;
    double v1 = sd1 * sd1;
    double v2 = sd2 * sd2;
    double cov12 = corr * sd1 * sd2;
    memset(ef, 0, sizeof(EfficientFrontier));

    int steps = PORTFOLIO_MAX_FRONTIER_POINTS - 1;
    for (int i = 0; i <= steps && ef->point_count < PORTFOLIO_MAX_FRONTIER_POINTS; i++) {
        double w1 = (double)i / steps;
        double w2 = 1.0 - w1;
        double er = w1 * er1 + w2 * er2;
        double var = w1 * w1 * v1 + w2 * w2 * v2 + 2.0 * w1 * w2 * cov12;
        double sd = sqrt(var);
        double sr = (sd > 1e-12) ? (er - rf) / sd : 0.0;

        FrontierPoint* fp = &ef->points[ef->point_count];
        fp->expected_return = er;
        fp->stddev = sd;
        fp->sharpe_ratio = sr;
        fp->weights[0] = w1;
        fp->weights[1] = w2;
        ef->point_count++;

        if (i == 0 || sd < ef->min_variance_point.stddev) {
            ef->min_variance_point = *fp;
        }
        if (sr > ef->max_sharpe_point.sharpe_ratio) {
            ef->max_sharpe_point = *fp;
        }
    }

    /* Tangency portfolio closed form */
    double num = (er1 - rf) * v2 - (er2 - rf) * cov12;
    double den = (er1 - rf) * v2 + (er2 - rf) * v1
               - (er1 + er2 - 2.0 * rf) * cov12;
    if (fabs(den) < 1e-12) return;
    double wt = num / den;
    if (wt < 0.0) wt = 0.0;
    if (wt > 1.0) wt = 1.0;
    double w2t = 1.0 - wt;
    double ert = wt * er1 + w2t * er2;
    double vart = wt * wt * v1 + w2t * w2t * v2 + 2.0 * wt * w2t * cov12;
    double sdt = sqrt(vart);
    ef->max_sharpe_point.expected_return = ert;
    ef->max_sharpe_point.stddev = sdt;
    ef->max_sharpe_point.sharpe_ratio = (sdt > 1e-12) ? (ert - rf) / sdt : 0.0;
    ef->max_sharpe_point.weights[0] = wt;
    ef->max_sharpe_point.weights[1] = w2t;
}


/* ================================================================
 * L5: Tangency Portfolio (Maximum Sharpe Ratio)
 * For N assets, the tangency portfolio weights maximize:
 *   (w^T mu - rf) / sqrt(w^T Sigma w)
 * Subject to sum(w_i) = 1, w_i >= 0 (long-only)
 * Solved by quadratic programming / critical line algorithm.
 *
 * Here we use a simplified grid search for up to 4 assets
 * with step size 0.05 in weight space.
 * ================================================================ */

int portfolio_find_tangency(Portfolio* pf) {
    if (!pf || pf->asset_count < 1) return -1;
    int n = pf->asset_count;

    if (n == 1) {
        pf->assets[0].weight = 1.0;
        portfolio_compute_return(pf);
        portfolio_compute_sharpe_ratio(pf);
        return 0;
    }

    /* Two-asset closed form */
    if (n == 2) {
        double v1 = pf->cov_matrix[0][0];
        double v2 = pf->cov_matrix[1][1];
        double cv = pf->cov_matrix[0][1];
        double er1 = pf->assets[0].expected_return;
        double er2 = pf->assets[1].expected_return;
        double rf = pf->risk_free_rate;
        double num = (er1 - rf) * v2 - (er2 - rf) * cv;
        double den = (er1 - rf) * v2 + (er2 - rf) * v1
                   - (er1 + er2 - 2.0 * rf) * cv;
        double w1 = (fabs(den) > 1e-12) ? num / den : 0.5;
        if (w1 < 0.0) w1 = 0.0;
        if (w1 > 1.0) w1 = 1.0;
        pf->assets[0].weight = w1;
        pf->assets[1].weight = 1.0 - w1;
        portfolio_compute_return(pf);
        portfolio_compute_sharpe_ratio(pf);
        return 0;
    }

    /* Grid search for 3+ assets (simplified) */
    double best_sr = -1e10;
    double best_w[PORTFOLIO_MAX_ASSETS] = {0};
    int steps = 10; /* 10% increments */

    for (int w0 = 0; w0 <= 100; w0 += steps) {
        pf->assets[0].weight = w0 / 100.0;
        double remaining = 1.0 - pf->assets[0].weight;
        for (int w1 = 0; w1 <= (int)(remaining * 100); w1 += steps) {
            pf->assets[1].weight = w1 / 100.0;
            double rem2 = remaining - pf->assets[1].weight;
            for (int w2 = 0; w2 <= (int)(rem2 * 100); w2 += steps) {
                pf->assets[2].weight = w2 / 100.0;
                if (n > 3)
                    pf->assets[3].weight = rem2 - pf->assets[2].weight;
                else
                    pf->assets[2].weight = rem2;

                double er = portfolio_compute_return(pf);
                double sd = portfolio_compute_stddev(pf);
                double sr = (sd > 1e-12) ? (er - pf->risk_free_rate) / sd : 0.0;
                if (sr > best_sr) {
                    best_sr = sr;
                    for (int k = 0; k < n; k++) best_w[k] = pf->assets[k].weight;
                }
            }
        }
    }
    for (int k = 0; k < n; k++) pf->assets[k].weight = best_w[k];
    portfolio_compute_return(pf);
    portfolio_compute_sharpe_ratio(pf);
    return 0;
}

/* ================================================================
 * L4: CAPM ¡ª Capital Asset Pricing Model
 * Sharpe (1964), Lintner (1965), Mossin (1966)
 *
 * E(R_i) = R_f + beta_i * (E(R_m) - R_f)
 * beta_i = Cov(R_i, R_m) / Var(R_m)
 * alpha_i = E(R_i)_observed - E(R_i)_CAPM
 *
 * Security Market Line (SML): E(R) = R_f + beta * (E(R_m) - R_f)
 * ================================================================ */

CAPMModel* capm_create(double risk_free_rate, double market_return) {
    CAPMModel* capm = (CAPMModel*)calloc(1, sizeof(CAPMModel));
    if (!capm) return NULL;
    capm->risk_free_rate = risk_free_rate;
    capm->market_return = market_return;
    return capm;
}

void capm_compute_beta(CAPMModel* capm, const double* asset_returns,
                        const double* market_returns, int n) {
    if (!capm || !asset_returns || !market_returns || n < 2) return;

    double cov_am = covariance_compute(asset_returns, market_returns, n);
    double var_m = variance_compute(market_returns, n);

    if (var_m < 1e-12) { capm->beta = 0.0; return; }
    capm->beta = cov_am / var_m;

    /* R-squared: proportion of variance explained by market */
    double var_a = variance_compute(asset_returns, n);
    if (var_a > 1e-12)
        capm->r_squared = (cov_am * cov_am) / (var_m * var_a);

    /* CAPM expected return */
    capm->expected_return = capm->risk_free_rate
        + capm->beta * (capm->market_return - capm->risk_free_rate);

    /* Jensen alpha: actual average excess over CAPM prediction */
    double avg_asset = mean_compute(asset_returns, n);
    double avg_market = mean_compute(market_returns, n);
    capm->alpha = (avg_asset - capm->risk_free_rate)
        - capm->beta * (avg_market - capm->risk_free_rate);
}

double capm_expected_return(const CAPMModel* capm) {
    if (!capm) return 0.0;
    return capm->risk_free_rate
        + capm->beta * (capm->market_return - capm->risk_free_rate);
}

double capm_security_market_line(double risk_free_rate,
                                  double market_return, double beta) {
    return risk_free_rate + beta * (market_return - risk_free_rate);
}

void capm_print(const CAPMModel* capm) {
    if (!capm) return;
    printf("CAPM: Rf=%.2f%% Rm=%.2f%% Beta=%.3f Alpha=%.4f%% R^2=%.3f
",
           capm->risk_free_rate * 100.0, capm->market_return * 100.0,
           capm->beta, capm->alpha * 100.0, capm->r_squared);
    printf("  Expected: %.2f%% = %.2f%% + %.3f*(%.2f%% - %.2f%%)
",
           capm->expected_return * 100.0, capm->risk_free_rate * 100.0,
           capm->beta, capm->market_return * 100.0, capm->risk_free_rate * 100.0);
}

/* ================================================================
 * L5: Statistical Utilities
 * Cov(X,Y) = E[(X-E[X])(Y-E[Y])] = (1/(n-1))*sum((x_i-mean_x)*(y_i-mean_y))
 * Corr(X,Y) = Cov(X,Y) / (sigma_x * sigma_y)
 * Var(X) = E[(X-E[X])^2] = (1/(n-1))*sum((x_i-mean_x)^2)
 * ================================================================ */

double mean_compute(const double* x, int n) {
    if (!x || n <= 0) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < n; i++) sum += x[i];
    return sum / n;
}

double variance_compute(const double* x, int n) {
    if (!x || n < 2) return 0.0;
    double mean = mean_compute(x, n);
    double ssq = 0.0;
    for (int i = 0; i < n; i++) {
        double diff = x[i] - mean;
        ssq += diff * diff;
    }
    return ssq / (n - 1);
}

double covariance_compute(const double* x, const double* y, int n) {
    if (!x || !y || n < 2) return 0.0;
    double mx = mean_compute(x, n);
    double my = mean_compute(y, n);
    double sum = 0.0;
    for (int i = 0; i < n; i++)
        sum += (x[i] - mx) * (y[i] - my);
    return sum / (n - 1);
}

double correlation_compute(const double* x, const double* y, int n) {
    double cov = covariance_compute(x, y, n);
    double sx = sqrt(variance_compute(x, n));
    double sy = sqrt(variance_compute(y, n));
    if (sx < 1e-12 || sy < 1e-12) return 0.0;
    return cov / (sx * sy);
}

void portfolio_destroy(Portfolio* pf) { free(pf); }
