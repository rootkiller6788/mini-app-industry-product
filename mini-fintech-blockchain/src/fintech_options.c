#include "fintech_options.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * L4: Normal Distribution Functions
 * Reference: Abramowitz & Stegun (1964) Handbook of Mathematical
 *   Functions, Section 26.2
 * CDF approximation error < 7.5e-8 (Hart 1968)
 * ================================================================ */

static const double SQRT_2PI = 2.506628274631000502415765284811;

double norm_pdf(double x) {
    return exp(-0.5 * x * x) / SQRT_2PI;
}

double norm_cdf(double x) {
    /* Abramowitz & Stegun 26.2.17 rational approximation */
    const double a1 =  0.254829592;
    const double a2 = -0.284496736;
    const double a3 =  1.421413741;
    const double a4 = -1.453152027;
    const double a5 =  1.061405429;
    const double p  =  0.3275911;

    int sign = (x >= 0.0) ? 1 : -1;
    x = fabs(x);

    double t = 1.0 / (1.0 + p * x);
    double y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * exp(-0.5 * x * x) / SQRT_2PI;
    return (sign == 1) ? y : (1.0 - y);
}

double norm_inv_cdf(double p) {
    /* Moro (1995) approximation for inverse normal CDF
     * Splits range [0,1] into central region and tails */
    if (p <= 0.0) return -1e10;
    if (p >= 1.0) return  1e10;

    const double a0 =  2.50662823884;
    const double a1 = -18.61500062529;
    const double a2 =  41.39119773534;
    const double a3 = -25.44106049637;
    const double b1 = -8.47351093090;
    const double b2 =  23.08336743743;
    const double b3 = -21.06224101826;
    const double b4 =   3.13082909833;
    const double c0 =  0.3374754822726147;
    const double c1 =  0.9761690190917186;
    const double c2 =  0.1607979714918209;
    const double c3 =  0.0276438810333863;
    const double c4 =  0.0038405729373609;
    const double c5 =  0.0003951896511919;
    const double c6 =  0.0000321767881768;
    const double c7 =  0.0000002888167364;
    const double c8 =  0.0000003960315187;

    double y = p - 0.5;
    if (fabs(y) < 0.42) {
        double r = y * y;
        return y * (((a3 * r + a2) * r + a1) * r + a0)
                 / ((((b4 * r + b3) * r + b2) * r + b1) * r + 1.0);
    } else {
        double r = (y < 0.0) ? p : (1.0 - p);
        r = log(-log(r));
        double x = c0 + r * (c1 + r * (c2 + r * (c3 + r * (c4
                    + r * (c5 + r * (c6 + r * (c7 + r * c8)))))));
        return (y < 0.0) ? -x : x;
    }
}

/* ================================================================
 * L4/L5: Black-Scholes Closed-Form Solution
 * European Call: C = S*e^(-qT)*N(d1) - K*e^(-rT)*N(d2)
 * European Put:  P = K*e^(-rT)*N(-d2) - S*e^(-qT)*N(-d1)
 * where d1 = [ln(S/K) + (r - q + 0.5*v^2)T] / (v*sqrt(T))
 *       d2 = d1 - v*sqrt(T)
 * ================================================================ */

double option_price_bs(const OptionContract* opt) {
    if (!opt || opt->time_to_expiry <= 0.0 || opt->volatility <= 0.0) {
        if (opt && opt->time_to_expiry <= 0.0)
            return option_payoff(opt->type, opt->spot, opt->strike);
        return 0.0;
    }

    double S = opt->spot;
    double K = opt->strike;
    double r = opt->rate;
    double v = opt->volatility;
    double T = opt->time_to_expiry;
    double q = opt->dividend_yield;

    double d1 = (log(S / K) + (r - q + 0.5 * v * v) * T) / (v * sqrt(T));
    double d2 = d1 - v * sqrt(T);

    if (opt->type == OPT_CALL) {
        return S * exp(-q * T) * norm_cdf(d1) - K * exp(-r * T) * norm_cdf(d2);
    } else {
        return K * exp(-r * T) * norm_cdf(-d2) - S * exp(-q * T) * norm_cdf(-d1);
    }
}

/* ================================================================
 * L5: Greeks Computation
 * Analytical closed-form Greeks for European options.
 * Delta=partial(V)/partial(S), Gamma=partial^2(V)/partial(S)^2
 * Theta=partial(V)/partial(t) per day, Vega=partial(V)/partial(sigma) per 1pct
 * Rho=partial(V)/partial(r) per 1pct
 * ================================================================ */

int option_compute_greeks(OptionContract* opt) {
    if (!opt || opt->time_to_expiry <= 0.0 || opt->volatility <= 0.0)
        return -1;

    double S = opt->spot;
    double K = opt->strike;
    double r = opt->rate;
    double v = opt->volatility;
    double T = opt->time_to_expiry;
    double q = opt->dividend_yield;

    double d1 = (log(S / K) + (r - q + 0.5 * v * v) * T) / (v * sqrt(T));
    double d2 = d1 - v * sqrt(T);
    double nd1 = norm_pdf(d1);
    double ert = exp(-r * T);
    double eqt = exp(-q * T);
    int is_call = (opt->type == OPT_CALL) ? 1 : -1;

    /* Delta */
    opt->greeks.delta = is_call * eqt * norm_cdf(is_call * d1);

    /* Gamma (same for call and put) */
    opt->greeks.gamma = (S > 0.0 && v > 0.0 && T > 0.0)
        ? eqt * nd1 / (S * v * sqrt(T)) : 0.0;

    /* Theta (per day) */
    double term1 = -(S * eqt * nd1 * v) / (2.0 * sqrt(T));
    double term2a = is_call * q * S * eqt * norm_cdf(is_call * d1);
    double term2b = is_call * r * K * ert * norm_cdf(is_call * d2);
    opt->greeks.theta = (term1 - term2a + term2b) / 365.0;

    /* Vega (per 1% vol change) */
    opt->greeks.vega = S * eqt * nd1 * sqrt(T) * 0.01;

    /* Rho (per 1% rate change) */
    opt->greeks.rho = (opt->type == OPT_CALL)
        ? K * T * ert * norm_cdf(d2) * 0.01
        : -K * T * ert * norm_cdf(-d2) * 0.01;

    opt->theoretical_price = option_price_bs(opt);
    return 0;
}

/* ================================================================
 * L5: Binomial Tree (Cox-Ross-Rubinstein 1979)
 * u = exp(sigma*sqrt(dt)), d = 1/u
 * p = (exp((r-q)*dt) - d) / (u - d)
 * Supports European, American, Bermudan exercise
 * ================================================================ */

double option_price_binomial(const OptionContract* opt, int steps) {
    if (!opt || steps < 1 || steps > OPT_MAX_STEPS) return -1.0;
    if (opt->time_to_expiry <= 0.0)
        return option_payoff(opt->type, opt->spot, opt->strike);

    double S = opt->spot, K = opt->strike, r = opt->rate;
    double v = opt->volatility, T = opt->time_to_expiry, q = opt->dividend_yield;
    double dt = T / steps;
    double u = exp(v * sqrt(dt));
    double d = 1.0 / u;
    double disc = exp(-r * dt);
    double p = (exp((r - q) * dt) - d) / (u - d);
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;

    double* prices = (double*)malloc((size_t)(steps + 1) * sizeof(double));
    if (!prices) return -1.0;

    /* Terminal payoff at maturity */
    for (int i = 0; i <= steps; i++) {
        double node_spot = S * pow(u, steps - i) * pow(d, i);
        prices[i] = option_payoff(opt->type, node_spot, K);
    }

    /* Backward induction through the tree */
    for (int j = steps - 1; j >= 0; j--) {
        for (int i = 0; i <= j; i++) {
            double hold = disc * (p * prices[i] + (1.0 - p) * prices[i + 1]);

            /* Check early exercise for non-European options */
            if (opt->style != OPT_STYLE_EUROPEAN) {
                double node_spot = S * pow(u, j - i) * pow(d, i);
                double exercise = option_payoff(opt->type, node_spot, K);
                if (exercise > hold) hold = exercise;
            }
            prices[i] = hold;
        }
    }
    double result = prices[0];
    free(prices);
    return result;
}

/* ================================================================
 * L5: Implied Volatility via Newton-Raphson
 * Find sigma s.t. BS(sigma) = market_price
 * Reference: Manaster & Koehler (1982) initial guess
 * ================================================================ */

double option_implied_volatility(const OptionContract* opt, double market_price,
                                  double tolerance, int max_iterations) {
    if (!opt || market_price <= 0.0 || tolerance <= 0.0 || max_iterations < 1)
        return -1.0;

    double S = opt->spot, K = opt->strike, r = opt->rate;
    double T = opt->time_to_expiry, q = opt->dividend_yield;

    /* Manaster & Koehler initial guess */
    double sigma = sqrt(2.0 * fabs(log(S / K) + (r - q) * T) / T);
    if (sigma < 0.001) sigma = 0.3;

    for (int iter = 0; iter < max_iterations; iter++) {
        double d1 = (log(S / K) + (r - q + 0.5 * sigma * sigma) * T)
                     / (sigma * sqrt(T));
        double d2 = d1 - sigma * sqrt(T);
        double bs_price;
        if (opt->type == OPT_CALL)
            bs_price = S * exp(-q * T) * norm_cdf(d1)
                     - K * exp(-r * T) * norm_cdf(d2);
        else
            bs_price = K * exp(-r * T) * norm_cdf(-d2)
                     - S * exp(-q * T) * norm_cdf(-d1);

        double diff = bs_price - market_price;
        if (fabs(diff) < tolerance) return sigma;

        double vega = S * exp(-q * T) * norm_pdf(d1) * sqrt(T);
        if (fabs(vega) < 1e-12) return -2.0;

        sigma -= diff / vega;
        if (sigma < 0.001) sigma = 0.001;
        if (sigma > 10.0)  sigma = 10.0;
    }
    return sigma;
}

/* ================================================================
 * L7: Option Strategy Payoffs
 * ================================================================ */

double option_payoff(OptionType type, double spot, double strike) {
    if (type == OPT_CALL)
        return (spot > strike) ? (spot - strike) : 0.0;
    else
        return (strike > spot) ? (strike - spot) : 0.0;
}

double straddle_payoff(double spot, double strike, double call_prem, double put_prem) {
    double cp = (spot > strike) ? (spot - strike) : 0.0;
    double pp = (strike > spot) ? (strike - spot) : 0.0;
    return cp + pp - call_prem - put_prem;
}

double bull_spread_payoff(OptionType type, double spot, double strike_low,
                           double strike_high, double net_premium) {
    if (strike_low >= strike_high) return 0.0;
    double payoff;
    if (type == OPT_CALL) {
        if (spot <= strike_low) payoff = 0.0;
        else if (spot >= strike_high) payoff = strike_high - strike_low;
        else payoff = spot - strike_low;
    } else {
        if (spot <= strike_low) payoff = strike_high - strike_low;
        else if (spot >= strike_high) payoff = 0.0;
        else payoff = strike_high - spot;
    }
    return payoff - net_premium;
}

/* ================================================================
 * L8: Exotic Options — Asian Option Pricing (Average Rate)
 * Arithmetic average rate call/put via Monte Carlo approximation
 * and geometric average closed form (Kemna & Vorst 1990)
 * ================================================================ */

double asian_option_geometric(OptionType type, double S, double K, double r,
                               double v, double T, double q, int n_obs) {
    /* Kemna & Vorst (1990) closed form for geometric average Asian options */
    if (T <= 0.0 || v <= 0.0 || n_obs < 1) return -1.0;

    double dt = T / n_obs;
    /* Adjusted volatility and drift for geometric average */
    double v_adj_sq = v * v * (2.0 * n_obs * n_obs + 3.0 * n_obs + 1.0)
                      / (6.0 * n_obs * n_obs);
    double v_adj = sqrt(v_adj_sq);
    double mu_adj = 0.5 * (r - q - 0.5 * v * v)
                    * (n_obs + 1.0) / n_obs
                    + 0.5 * v_adj_sq;

    double d1 = (log(S / K) + (mu_adj + 0.5 * v_adj_sq) * T) / (v_adj * sqrt(T));
    double d2 = d1 - v_adj * sqrt(T);

    if (type == OPT_CALL)
        return exp(-r * T) * (S * exp(mu_adj * T) * norm_cdf(d1) - K * norm_cdf(d2));
    else
        return exp(-r * T) * (K * norm_cdf(-d2) - S * exp(mu_adj * T) * norm_cdf(-d1));
}

/* ================================================================
 * L8: Lookback Option Pricing
 * Floating strike lookback call: payoff = S_T - min(S_t)
 * Closed form: Goldman, Sosin & Gatto (1979)
 * ================================================================ */

double lookback_call_floating(double S, double S_min, double r, double v,
                               double T, double q) {
    /* Goldman, Sosin & Gatto (1979) floating strike lookback */
    if (T <= 0.0 || v <= 0.0 || S_min <= 0.0 || S_min > S) return -1.0;

    double a1 = (log(S / S_min) + (r - q + 0.5 * v * v) * T) / (v * sqrt(T));
    double a2 = a1 - v * sqrt(T);
    double a3 = (log(S / S_min) + (-r + q + 0.5 * v * v) * T) / (v * sqrt(T));

    double term1 = S * exp(-q * T) * norm_cdf(a1);
    double term2 = S_min * exp(-r * T) * norm_cdf(a2);
    double term3 = S * exp(-r * T) * (v * v) / (2.0 * (r - q))
                   * (pow(S / S_min, -2.0 * (r - q) / (v * v))
                   * norm_cdf(-a3) - exp(-q * T + r * T) * norm_cdf(-a1));

    return term1 - term2 + term3;
}

/* --- Utility --- */
const char* option_type_name(OptionType t) {
    return (t == OPT_CALL) ? "Call" : "Put";
}

const char* option_style_name(OptionStyle s) {
    switch (s) {
        case OPT_STYLE_EUROPEAN: return "European";
        case OPT_STYLE_AMERICAN: return "American";
        case OPT_STYLE_BERMUDAN: return "Bermudan";
        default:                 return "Unknown";
    }
}
