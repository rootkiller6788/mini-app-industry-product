#ifndef FINTECH_BONDS_H
#define FINTECH_BONDS_H

#define BOND_MAX_CASHFLOWS 240

typedef enum {
    BOND_ZERO_COUPON,
    BOND_FIXED_COUPON,
    BOND_FLOATING_RATE,
    BOND_CONVERTIBLE,
    BOND_TYPE_COUNT
} BondType;

typedef struct {
    double amount;
    double time_years;
    int    period;
} CashFlow;

typedef struct {
    BondType  type;
    char      isin[16];
    double    face_value;
    double    coupon_rate;
    int       coupon_frequency;
    double    maturity_years;
    double    yield_to_maturity;
    double    market_price;
    double    accrued_interest;
    double    dirty_price;
    double    macaulay_duration;
    double    modified_duration;
    double    convexity;
    double    dv01;
    CashFlow  cashflows[BOND_MAX_CASHFLOWS];
    int       cashflow_count;
} Bond;

typedef struct {
    double beta0;
    double beta1;
    double beta2;
    double tau;
} NelsonSiegelCurve;

Bond* bond_create(BondType type, double face_value, double coupon_rate,
                  int frequency, double maturity_years);
int   bond_generate_cashflows(Bond* b);
double bond_price_from_ytm(const Bond* b, double ytm);
double bond_ytm_newton(const Bond* b, double market_price,
                       double tolerance, int max_iter);
double bond_duration_macaulay(const Bond* b);
double bond_duration_modified(const Bond* b);
double bond_convexity(const Bond* b);
double bond_dv01(const Bond* b);
double bond_zero_coupon_price(double face_value, double ytm, double years);
void   nelson_siegel_curve(NelsonSiegelCurve* ns, double beta0, double beta1,
                            double beta2, double tau);
double nelson_siegel_yield(const NelsonSiegelCurve* ns, double t_years);
double nelson_siegel_forward(const NelsonSiegelCurve* ns, double t_years);
double immunization_hedge_ratio(double liability_duration,
                                 double asset_duration,
                                 double liability_pv, double asset_pv);
void   bond_print(const Bond* b);
void   bond_destroy(Bond* b);

#endif
