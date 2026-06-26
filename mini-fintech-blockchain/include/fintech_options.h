#ifndef FINTECH_OPTIONS_H
#define FINTECH_OPTIONS_H

#include <stdbool.h>

/* ================================================================
 * L4: Options Pricing — Black-Scholes-Merton Model
 * Reference: Black & Scholes (1973) "The Pricing of Options and
 *   Corporate Liabilities", JPE Vol.81 No.3
 * Reference: Merton (1973) "Theory of Rational Option Pricing"
 *
 * European Call: C = S*e^(-qT)*N(d1) - K*e^(-rT)*N(d2)
 * European Put:  P = K*e^(-rT)*N(-d2) - S*e^(-qT)*N(-d1)
 * where d1 = [ln(S/K) + (r - q + σ²/2)T] / (σ√T)
 *       d2 = d1 - σ√T
 * N(x) = standard normal cumulative distribution function
 * ================================================================ */

#define OPT_MAX_STEPS 4096

typedef enum {
    OPT_CALL,
    OPT_PUT,
    OPT_TYPE_COUNT
} OptionType;

typedef enum {
    OPT_STYLE_EUROPEAN,
    OPT_STYLE_AMERICAN,
    OPT_STYLE_BERMUDAN,
    OPT_STYLE_COUNT
} OptionStyle;

/* L5: Greeks — partial derivatives of option price wrt parameters */
typedef struct {
    double delta;   /* ∂V/∂S: sensitivity to underlying price */
    double gamma;   /* ∂²V/∂S²: sensitivity of delta to price */
    double theta;   /* ∂V/∂t: time decay (per day) */
    double vega;    /* ∂V/∂σ: sensitivity to volatility (per 1%) */
    double rho;     /* ∂V/∂r: sensitivity to risk-free rate (per 1%) */
} OptionGreeks;

typedef struct {
    OptionType   type;
    OptionStyle  style;
    double       spot;            /* S: current underlying price */
    double       strike;          /* K: strike price */
    double       rate;            /* r: risk-free interest rate (annual) */
    double       volatility;      /* σ: implied volatility (annual) */
    double       time_to_expiry;  /* T: time to expiry in years */
    double       dividend_yield;  /* q: continuous dividend yield */
    double       theoretical_price;
    OptionGreeks greeks;
} OptionContract;

/* L5: Binomial tree node (Cox-Ross-Rubinstein 1979) */
typedef struct {
    double up_factor;       /* u = exp(σ√Δt) */
    double down_factor;     /* d = 1/u */
    double risk_neutral_p;  /* p = (e^(rΔt) - d) / (u - d) */
    int    steps;
} BinomialParams;

/* --- API: Cumulative Distribution Functions --- */
double norm_cdf(double x);        /* Standard normal CDF: Abramowitz & Stegun 26.2.17 */
double norm_pdf(double x);        /* Standard normal PDF: N'(x) = e^(-x²/2) / √(2π) */
double norm_inv_cdf(double p);    /* Inverse CDF: Moro (1995) approximation */

/* --- API: Black-Scholes --- */
double option_price_bs(const OptionContract* opt);
int    option_compute_greeks(OptionContract* opt);

/* --- API: Binomial Tree (CRR) --- */
double option_price_binomial(const OptionContract* opt, int steps);

/* --- API: Implied Volatility --- */
double option_implied_volatility(const OptionContract* opt, double market_price,
                                  double tolerance, int max_iterations);

/* --- API: Strategy Payoffs --- */
double option_payoff(OptionType type, double spot, double strike);
double straddle_payoff(double spot, double strike, double call_prem, double put_prem);
double bull_spread_payoff(OptionType type, double spot, double strike_low,
                           double strike_high, double net_premium);

/* --- Utility --- */
const char* option_type_name(OptionType t);
const char* option_style_name(OptionStyle s);

#endif
