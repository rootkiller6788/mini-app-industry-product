#ifndef FINTECH_PORTFOLIO_H
#define FINTECH_PORTFOLIO_H

#include <stdbool.h>

#define PORTFOLIO_MAX_ASSETS 256
#define PORTFOLIO_MAX_FRONTIER_POINTS 200

typedef struct {
    char   ticker[16];
    double expected_return;
    double volatility;
    double weight;
    int    asset_class;
} PortfolioAsset;

typedef struct {
    PortfolioAsset assets[PORTFOLIO_MAX_ASSETS];
    int            asset_count;
    double         cov_matrix[PORTFOLIO_MAX_ASSETS][PORTFOLIO_MAX_ASSETS];
    double         risk_free_rate;
    double         portfolio_return;
    double         portfolio_variance;
    double         portfolio_stddev;
    double         sharpe_ratio;
    double         diversification_ratio;
} Portfolio;

typedef struct {
    double expected_return;
    double stddev;
    double sharpe_ratio;
    double weights[PORTFOLIO_MAX_ASSETS];
} FrontierPoint;

typedef struct {
    FrontierPoint points[PORTFOLIO_MAX_FRONTIER_POINTS];
    int           point_count;
    FrontierPoint max_sharpe_point;
    FrontierPoint min_variance_point;
} EfficientFrontier;

typedef struct {
    double beta;
    double alpha;
    double r_squared;
    double expected_return;
    double market_return;
    double risk_free_rate;
} CAPMModel;

Portfolio* portfolio_create(double risk_free_rate);
int        portfolio_add_asset(Portfolio* pf, const char* ticker,
                               double expected_return, double volatility,
                               double weight, int asset_class);
int        portfolio_set_covariance(Portfolio* pf, int i, int j, double cov);
double     portfolio_compute_return(const Portfolio* pf);
double     portfolio_compute_variance(Portfolio* pf);
double     portfolio_compute_stddev(Portfolio* pf);
double     portfolio_compute_sharpe_ratio(Portfolio* pf);
double     portfolio_diversification_ratio(const Portfolio* pf);
void       frontier_two_asset(double er1, double er2, double sd1, double sd2,
                              double corr, double rf, EfficientFrontier* ef);
int        portfolio_find_tangency(Portfolio* pf);
CAPMModel* capm_create(double risk_free_rate, double market_return);
void       capm_compute_beta(CAPMModel* capm, const double* asset_returns,
                              const double* market_returns, int n);
double     capm_expected_return(const CAPMModel* capm);
double     capm_security_market_line(double risk_free_rate,
                                      double market_return, double beta);
void       capm_print(const CAPMModel* capm);
double     covariance_compute(const double* x, const double* y, int n);
double     correlation_compute(const double* x, const double* y, int n);
double     variance_compute(const double* x, int n);
double     mean_compute(const double* x, int n);
void       portfolio_destroy(Portfolio* pf);

#endif