#include "sci_optimize.h"
#include "sci_linearalgebra.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Example: Compare optimization algorithms on Rosenbrock function.
 * f(x,y) = (1-x)^2 + 100*(y-x^2)^2
 * Global minimum at (1,1) with f=0.
 */

static double rosenbrock(const double *x, int n) {
    (void)n;
    double a = 1.0 - x[0];
    double b = x[1] - x[0]*x[0];
    return a*a + 100.0 * b*b;
}

static void rosenbrock_grad(const double *x, int n, double *grad) {
    (void)n;
    grad[0] = -2.0*(1.0-x[0]) - 400.0*x[0]*(x[1]-x[0]*x[0]);
    grad[1] = 200.0*(x[1]-x[0]*x[0]);
}

/* Simple quadratic for testing */
static double quadratic(const double *x, int n) {
    (void)n;
    return (x[0]-3.0)*(x[0]-3.0) + (x[1]+2.0)*(x[1]+2.0);
}

static void quadratic_grad(const double *x, int n, double *grad) {
    (void)n;
    grad[0] = 2.0*(x[0]-3.0);
    grad[1] = 2.0*(x[1]+2.0);
}

int main(void) {
    printf("=== Example: Optimization Algorithms ===\n\n");

    /* Simple quadratic */
    printf("Quadratic: f = (x-3)^2 + (y+2)^2, min at (3,-2)\n");
    double x[] = {0.0, 0.0};

    SCI_OptConfig cfg = sci_opt_config_default(SCI_OPT_GRADIENT_DESCENT);
    cfg.max_iter = 500;
    cfg.gtol = 1e-6;

    SCI_OptResult gd = sci_minimize_gradient_descent(
        quadratic, quadratic_grad, x, 2, &cfg);
    printf("  Gradient Descent: x=(%.4f, %.4f), f=%.6f, iters=%d\n",
           x[0], x[1], gd.optimal_value, gd.iterations);
    sci_opt_result_free(&gd);

    x[0] = 0.0; x[1] = 0.0;
    cfg.method = SCI_OPT_NELDER_MEAD;
    cfg.max_iter = 200;
    SCI_OptResult nm = sci_minimize_nelder_mead(quadratic, x, 2, 1.0, &cfg);
    printf("  Nelder-Mead:      x=(%.4f, %.4f), f=%.6f, iters=%d\n",
           x[0], x[1], nm.optimal_value, nm.iterations);
    sci_opt_result_free(&nm);

    /* Rosenbrock */
    printf("\nRosenbrock: f = (1-x)^2 + 100*(y-x^2)^2, min at (1,1)\n");
    x[0] = -1.0; x[1] = 1.0;

    cfg.method = SCI_OPT_GRADIENT_DESCENT;
    cfg.max_iter = 2000;
    cfg.gtol = 1e-6;
    SCI_OptResult rgd = sci_minimize_gradient_descent(
        rosenbrock, rosenbrock_grad, x, 2, &cfg);
    printf("  Gradient Descent: x=(%.4f, %.4f), f=%.6f, iters=%d\n",
           x[0], x[1], rgd.optimal_value, rgd.iterations);
    sci_opt_result_free(&rgd);

    /* Linear programming example */
    printf("\nLinear Programming: min x+y s.t. x+y=1, x,y>=0\n");
    double c[] = {1.0, 1.0};
    double A[] = {1.0, 1.0};
    double b[] = {1.0};
    double xs[2], obj;
    if (sci_linprog_simplex(c, A, b, 1, 2, xs, &obj)) {
        printf("  Optimal: x=%.4f, y=%.4f, obj=%.4f\n", xs[0], xs[1], obj);
    }

    printf("\n=== Done ===\n");
    return 0;
}
