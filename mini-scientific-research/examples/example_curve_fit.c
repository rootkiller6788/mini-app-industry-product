#include "sci_numeric.h"
#include "sci_statistics.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* Example: Fit exponential decay curve using ODE solver + regression.
 * Model: dy/dt = -k*y, y(0) = y0 => y(t) = y0*exp(-k*t)
 * Generate data with noise, then fit k using log-linear regression.
 */

static double decay(double t, double y) { (void)t; return -0.5 * y; }

int main(void) {
    printf("=== Example: Exponential Decay Curve Fitting ===\n\n");

    double k_true = 0.5, y0 = 10.0;
    printf("True parameters: k=%.3f, y0=%.1f\n", k_true, y0);

    /* Solve ODE analytically using our solver */
    SCI_ODEConfig cfg = sci_ode_config_default(SCI_ODE_RK4);
    cfg.h0 = 0.05;
    SCI_ODEResult res = sci_ode_solve(decay, 0.0, y0, 5.0, &cfg);

    printf("ODE solution at t=5: y=%.4f (true: %.4f)\n",
           res.y[res.num_points-1], y0 * exp(-k_true * 5.0));

    /* Generate noisy data points */
    int n = 20;
    double t[20], y[20];
    printf("\nData points with noise:\n");
    printf("   t       y_noisy    y_true\n");
    for (int i = 0; i < n; i++) {
        t[i] = 0.25 * (i + 1);
        y[i] = y0 * exp(-k_true * t[i]) + 0.2 * ((double)rand()/RAND_MAX - 0.5);
        printf("  %.2f   %8.4f   %8.4f\n", t[i], y[i], y0 * exp(-k_true * t[i]));
    }

    /* Log-transform for linear regression: ln(y) = ln(y0) - k*t */
    double ln_y[20];
    for (int i = 0; i < n; i++) ln_y[i] = log(y[i]);

    SCI_RegressionResult reg;
    memset(&reg, 0, sizeof(reg));
    if (sci_regression_ols(t, ln_y, n, 1, &reg)) {
        printf("\nRegression on ln(y):\n");
        printf("  ln(y0) = %.4f (true: %.4f)\n",
               reg.coefficients[0], log(y0));
        printf("  k      = %.4f (true: %.4f)\n",
               -reg.coefficients[1], k_true);
        printf("  R^2    = %.4f\n", reg.r_squared);
        sci_regression_free(&reg);
    }

    sci_ode_result_free(&res);
    printf("\n=== Done ===\n");
    return 0;
}
