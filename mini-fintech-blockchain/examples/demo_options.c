#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "fintech_core.h"

/* Demo: Black-Scholes option pricing and Greeks
 * Showcases: Call/Put pricing, Greeks, put-call parity, implied volatility */

int main(void) {
    printf("=== Options Demo: Black-Scholes Pricing ===

");

    OptionParams opt;
    opt.type = OPTION_CALL;
    opt.spot = 100.0;
    opt.strike = 105.0;
    opt.time_to_expiry = 0.5;  /* 6 months */
    opt.risk_free_rate = 0.05;
    opt.volatility = 0.25;
    opt.dividend_yield = 0.02;

    double call_price = bs_price(&opt);
    double delta = bs_delta(&opt);
    double gamma = bs_gamma(&opt);
    double vega = bs_vega(&opt);
    double theta = bs_theta(&opt);

    printf("Call Option (S=100, K=105, T=0.5y, r=5%%, v=25%%, q=2%%):
");
    printf("  Price:  $%.4f
", call_price);
    printf("  Delta:  %.4f (option price moves %.4f per  spot move)
", delta, delta);
    printf("  Gamma:  %.4f (delta changes by %.4f per  spot move)
", gamma, gamma);
    printf("  Vega:   %.4f (price changes by $%.4f per 1%% vol increase)
", vega, vega);
    printf("  Theta:  %.4f/day
", theta);

    /* Put option for put-call parity check */
    opt.type = OPTION_PUT;
    double put_price = bs_price(&opt);
    printf("  Put:    $%.4f
", put_price);

    /* Put-call parity: C - P = S*e^(-qT) - K*e^(-rT) */
    double parity_lhs = call_price - put_price;
    double parity_rhs = opt.spot * exp(-opt.dividend_yield * opt.time_to_expiry)
                      - opt.strike * exp(-opt.risk_free_rate * opt.time_to_expiry);
    printf("
Put-Call Parity:
");
    printf("  C - P = %.4f
", parity_lhs);
    printf("  Se^(-qT) - Ke^(-rT) = %.4f
", parity_rhs);
    printf("  Difference: %.6f (should be ~0)
", fabs(parity_lhs - parity_rhs));

    /* Implied volatility from market price */
    double market_price = 3.50;
    opt.type = OPTION_CALL;
    double iv = bs_implied_volatility(&opt, market_price, 100, 1e-6);
    printf("
Implied Volatility for market price $%.2f: %.2f%%
",
           market_price, iv * 100.0);

    /* OTM, ATM, ITM comparison */
    printf("
Moneyness comparison (1-year call):
");
    opt.time_to_expiry = 1.0;
    double strikes[] = {80.0, 100.0, 120.0};
    const char* labels[] = {"ITM (K=80)", "ATM (K=100)", "OTM (K=120)"};
    for (int i = 0; i < 3; i++) {
        opt.strike = strikes[i];
        printf("  %s: Price=$%.2f Delta=%.3f
",
               labels[i], bs_price(&opt), bs_delta(&opt));
    }

    printf("
Demo complete.
");
    return 0;
}
