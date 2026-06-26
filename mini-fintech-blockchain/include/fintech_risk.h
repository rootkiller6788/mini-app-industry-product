#ifndef FINTECH_RISK_H
#define FINTECH_RISK_H

#define RISK_MAX_RETURNS 4096
#define RISK_MAX_SIMULATIONS 100000

typedef enum {
    VAR_HISTORICAL,
    VAR_PARAMETRIC,
    VAR_MONTE_CARLO,
    VAR_TYPE_COUNT
} VaRMethod;

typedef struct {
    VaRMethod method;
    double    confidence_level;
    double    var_value;
    double    var_percent;
    double    expected_shortfall;
    double    portfolio_value;
    double    mean_return;
    double    stddev;
    int       horizon_days;
} RiskMetrics;

typedef struct {
    double var_60day_avg;
    double var_previous_day;
    double stressed_var;
    double multiplier;
    double market_risk_capital;
    double credit_risk_capital;
    double operational_risk_capital;
    double total_risk_capital;
    double tier1_capital;
    double cet1_ratio;
} BaselCapital;

double random_normal_box_muller(void);
void   sort_doubles(double* arr, int n);
int    var_historical(RiskMetrics* rm, const double* returns, int n);
int    var_parametric(RiskMetrics* rm, double mean, double stddev);
int    var_monte_carlo(RiskMetrics* rm, double mean, double stddev, int n_sim);
double expected_shortfall_historical(const double* returns, int n, double var_threshold);
void   basel_capital_calculate(BaselCapital* bc);
double portfolio_var_from_returns(const double* returns, int n, double conf, double pv);
void   risk_metrics_print(const RiskMetrics* rm);
void   basel_capital_print(const BaselCapital* bc);

#endif