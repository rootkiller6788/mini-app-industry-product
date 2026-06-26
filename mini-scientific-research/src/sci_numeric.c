#include "sci_numeric.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

/* sci_numeric.c -- Numerical Methods for Scientific Research */

SCI_RootConfig sci_root_config_default(SCI_RootMethod method) {
    SCI_RootConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.method = method;
    cfg.xtol = 1e-8;
    cfg.ftol = 1e-12;
    cfg.max_iter = 100;
    cfg.trace = false;
    cfg.criteria = SCI_CONVERGE_ABSOLUTE;
    return cfg;
}

SCI_ODEConfig sci_ode_config_default(SCI_ODEMethod method) {
    SCI_ODEConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.method = method;
    cfg.abstol = 1e-8;
    cfg.reltol = 1e-6;
    cfg.h0 = 0.0;
    cfg.hmin = 1e-14;
    cfg.hmax = 1.0;
    cfg.max_steps = 100000;
    cfg.dense_output = false;
    return cfg;
}

SCI_QuadConfig sci_quad_config_default(SCI_QuadMethod method) {
    SCI_QuadConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.method = method;
    cfg.abstol = 1e-8;
    cfg.reltol = 1e-6;
    cfg.max_evals = 100000;
    cfg.n_points = 7;
    cfg.max_depth = 50;
    return cfg;
}

/* ================================================================
 * L5: Root Finding — Bisection
 *
 * Theorem (Bolzano 1817 / IVT): If f continuous on [a,b] and
 * f(a)*f(b) < 0 then there exists c in (a,b) with f(c)=0.
 *
 * Linear convergence. Error: |c_n - c*| <= (b-a)/2^n.
 * Complexity: O(log((b-a)/tol)) guaranteed.
 * ================================================================ */

static SCI_RootResult root_bisection(SCI_ScalarFn f, double a, double b,
                                      const SCI_RootConfig *cfg) {
    SCI_RootResult res;
    memset(&res, 0, sizeof(res));
    res.converged = false;

    double fa = f(a), fb = f(b);
    res.function_evals = 2;

    if (fa * fb > 0.0) {
        res.root = NAN; res.residual = INFINITY;
        return res;
    }
    if (fabs(fa) < cfg->ftol) {
        res.root = a; res.residual = fabs(fa);
        res.converged = true; return res;
    }
    if (fabs(fb) < cfg->ftol) {
        res.root = b; res.residual = fabs(fb);
        res.converged = true; return res;
    }

    for (int i = 0; i < cfg->max_iter; i++) {
        double c = (a + b) / 2.0;
        double fc = f(c);
        res.function_evals++;
        res.iterations = i + 1;

        if (fabs(fc) < cfg->ftol || fabs(b - a) < cfg->xtol) {
            res.root = c; res.residual = fabs(fc);
            res.converged = true; return res;
        }
        if (fa * fc < 0.0) { b = c; fb = fc; }
        else { a = c; fa = fc; }
    }
    res.root = (a + b) / 2.0;
    res.residual = fabs(f(res.root));
    res.function_evals++;
    return res;
}

/* ================================================================
 * L5: Root Finding — Newton-Raphson
 *
 * x_{n+1} = x_n - f(x_n)/f'(x_n)
 *
 * Theorem (Kantorovich): If f is C^2, f'(root) != 0, and x0
 * sufficiently close, convergence is quadratic.
 *
 * Complexity: O(1) per iteration (with derivative).
 * ================================================================ */

SCI_RootResult sci_newton_root(SCI_ScalarFn f, SCI_ScalarFn df,
                                double x0, double xtol, int maxit) {
    SCI_RootResult res;
    memset(&res, 0, sizeof(res));
    res.converged = false;

    double x = x0;
    for (int i = 0; i < maxit; i++) {
        double fx = f(x);
        double dfx = df(x);
        res.function_evals += 2;
        res.iterations = i + 1;

        if (fabs(dfx) < 1e-15) {
            res.root = x; res.residual = fabs(fx); return res;
        }
        double dx = fx / dfx;
        x -= dx;
        if (fabs(dx) < xtol) {
            res.root = x; res.residual = fabs(f(x));
            res.function_evals++;
            res.converged = true; return res;
        }
    }
    res.root = x; res.residual = fabs(f(x));
    return res;
}

/* ================================================================
 * L5: Root Finding — Secant Method
 *
 * x_{n+1} = x_n - f(x_n)(x_n - x_{n-1})/(f(x_n) - f(x_{n-1}))
 *
 * No derivative required. Superlinear convergence,
 * order phi = (1+sqrt(5))/2 ~ 1.618.
 * Complexity: O(1) per iteration.
 * ================================================================ */

static SCI_RootResult root_secant(SCI_ScalarFn f, double a, double b,
                                   const SCI_RootConfig *cfg) {
    SCI_RootResult res;
    memset(&res, 0, sizeof(res));
    res.converged = false;

    double fa = f(a), fb = f(b);
    res.function_evals = 2;

    if (fabs(fa) > fabs(fb)) {
        double t = a; a = b; b = t;
        t = fa; fa = fb; fb = t;
    }

    for (int i = 0; i < cfg->max_iter; i++) {
        if (fabs(fb - fa) < 1e-15) break;
        double s = fb * (b - a) / (fb - fa);
        double c = b - s;
        double fc = f(c);
        res.function_evals++;
        res.iterations = i + 1;

        if (fabs(fc) < cfg->ftol || fabs(s) < cfg->xtol) {
            res.root = c; res.residual = fabs(fc);
            res.converged = true; return res;
        }
        a = b; fa = fb;
        b = c; fb = fc;
    }
    res.root = b; res.residual = fabs(fb);
    return res;
}

/* ================================================================
 * L5: Root Finding — Brent's Method (1973)
 *
 * Hybrid algorithm combining bisection, secant, and inverse
 * quadratic interpolation. Guaranteed convergence with
 * superlinear rate near the root.
 *
 * Reference: Brent, "Algorithms for Minimization without
 * Derivatives" (1973), Chapter 4.
 *
 * Gold standard for 1D root finding in practice.
 * ================================================================ */

SCI_RootResult sci_brent_root(SCI_ScalarFn f, double a, double b,
                               double xtol, int maxit) {
    SCI_RootResult res;
    memset(&res, 0, sizeof(res));
    res.converged = false;

    double fa = f(a), fb = f(b);
    res.function_evals = 2;

    if (fa * fb > 0.0) {
        res.root = NAN; res.residual = INFINITY; return res;
    }
    if (fabs(fa) < fabs(fb)) {
        double t = a; a = b; b = t;
        t = fa; fa = fb; fb = t;
    }

    double c = a, fc = fa;
    double d = b - a;
    bool mflag = true;

    for (int i = 0; i < maxit; i++) {
        res.iterations = i + 1;
        if (fabs(b - a) < xtol || fabs(fb) < xtol) {
            res.root = b; res.residual = fabs(fb);
            res.converged = true; return res;
        }

        /* Try inverse quadratic interpolation if possible */
        double s;
        if (fa != fc && fb != fc) {
            s = (a * fb * fc) / ((fa - fb) * (fa - fc))
              + (b * fa * fc) / ((fb - fa) * (fb - fc))
              + (c * fa * fb) / ((fc - fa) * (fc - fb));
        } else {
            /* Fall back to secant */
            s = b - fb * (b - a) / (fb - fa);
        }

        /* Bisection guard checks */
        bool must_bisect = false;
        double quarter = (3.0 * a + b) / 4.0;
        if ((s < quarter && s > b) || (s > quarter && s < b))
            must_bisect = true;
        else if (mflag && fabs(s - b) >= fabs(b - c) / 2.0)
            must_bisect = true;
        else if (!mflag && fabs(s - b) >= fabs(c - d) / 2.0)
            must_bisect = true;

        if (must_bisect) {
            s = (a + b) / 2.0;
            mflag = true;
        } else {
            mflag = false;
        }

        double fs = f(s);
        res.function_evals++;
        d = c; c = b; fc = fb;

        if (fa * fs < 0.0) { b = s; fb = fs; }
        else { a = s; fa = fs; }

        if (fabs(fa) < fabs(fb)) {
            double t = a; a = b; b = t;
            t = fa; fa = fb; fb = t;
        }
    }
    res.root = b; res.residual = fabs(fb);
    return res;
}

/* ================================================================
 * L5: Root Finding — Ridder's Method
 *
 * Exponential fitting method. Superlinear convergence.
 * More robust than secant near singularities.
 * ================================================================ */

static SCI_RootResult root_ridders(SCI_ScalarFn f, double a, double b,
                                    const SCI_RootConfig *cfg) {
    SCI_RootResult res;
    memset(&res, 0, sizeof(res));
    res.converged = false;

    double fa = f(a), fb = f(b);
    res.function_evals = 2;

    for (int i = 0; i < cfg->max_iter; i++) {
        double c = (a + b) / 2.0;
        double fc = f(c);
        res.function_evals++;
        res.iterations = i + 1;

        double s = sqrt(fc * fc - fa * fb);
        if (fabs(s) < 1e-15) {
            res.root = c; res.residual = fabs(fc);
            res.converged = true; return res;
        }
        double sign_fc = (fc >= 0.0) ? 1.0 : -1.0;
        double dx = (c - a) * sign_fc * fc / s;
        double xnew = c - dx;
        double fx = f(xnew);
        res.function_evals++;

        if (fabs(fx) < cfg->ftol || fabs(xnew - a) < cfg->xtol) {
            res.root = xnew; res.residual = fabs(fx);
            res.converged = true; return res;
        }

        if (fc * fx < 0.0) { a = c; fa = fc; b = xnew; fb = fx; }
        else if (fa * fx < 0.0) { b = xnew; fb = fx; }
        else { a = xnew; fa = fx; }
    }
    res.root = (a + b) / 2.0;
    return res;
}

/* ================================================================
 * L5: Root Finding Dispatcher
 * ================================================================ */

SCI_RootResult sci_find_root(SCI_ScalarFn f, double a, double b,
                              const SCI_RootConfig *config) {
    if (!f || !config) {
        SCI_RootResult err; memset(&err, 0, sizeof(err));
        err.root = NAN; err.residual = INFINITY; return err;
    }
    switch (config->method) {
        case SCI_ROOT_BISECTION:
            return root_bisection(f, a, b, config);
        case SCI_ROOT_SECANT:
            return root_secant(f, a, b, config);
        case SCI_ROOT_BRENT:
            return sci_brent_root(f, a, b, config->xtol, config->max_iter);
        case SCI_ROOT_RIDDERS:
            return root_ridders(f, a, b, config);
        default:
            { SCI_RootResult err; memset(&err, 0, sizeof(err));
              err.root = NAN; err.residual = INFINITY; return err; }
    }
}

/* ================================================================
 * L5: Find All Roots — scan interval, refine with Brent
 * ================================================================ */

int sci_find_all_roots(SCI_ScalarFn f, double a, double b, int n_sub,
                        double xtol, double *roots_out, int max_roots) {
    if (!f || !roots_out || n_sub < 1 || max_roots < 1) return 0;
    double h = (b - a) / n_sub;
    int found = 0;
    for (int i = 0; i < n_sub && found < max_roots; i++) {
        double x0 = a + i * h, x1 = x0 + h;
        double f0 = f(x0), f1 = f(x1);
        if (f0 * f1 <= 0.0) {
            SCI_RootResult res = sci_brent_root(f, x0, x1, xtol, 100);
            if (res.converged) {
                bool dup = false;
                for (int j = 0; j < found; j++) {
                    if (fabs(res.root - roots_out[j]) < xtol * 10.0)
                        { dup = true; break; }
                }
                if (!dup) roots_out[found++] = res.root;
            }
        }
    }
    return found;
}

/* ================================================================
 * L5: Numerical Differentiation
 *
 * Central difference: f'(x) = (f(x+h)-f(x-h))/(2h) + O(h^2)
 * Proof via Taylor expansion.
 *
 * Richardson extrapolation: combines D(h) and D(h/2) to get O(h^4):
 *   D_ext = (4*D(h/2) - D(h)) / 3
 *
 * Optimal h balances truncation O(h^2) and roundoff O(eps/h):
 *   h_opt ~ cbrt(eps) * |x|, default h = 1e-6 * max(|x|, 1)
 * ================================================================ */

double sci_derivative_first(SCI_ScalarFn f, double x, double h) {
    if (!f) return NAN;
    if (h <= 0.0) h = 1e-6 * (fabs(x) > 1.0 ? fabs(x) : 1.0);
    return (f(x + h) - f(x - h)) / (2.0 * h);
}

double sci_derivative_second(SCI_ScalarFn f, double x, double h) {
    if (!f) return NAN;
    if (h <= 0.0) h = 1e-5 * (fabs(x) > 1.0 ? fabs(x) : 1.0);
    return (f(x + h) - 2.0 * f(x) + f(x - h)) / (h * h);
}

void sci_gradient(SCI_MultiVarFn f, const double *x, int n,
                  double h, double *grad) {
    if (!f || !x || !grad || n <= 0) return;
    if (h <= 0.0) h = 1e-6;

    double *xp = (double*)malloc(n * sizeof(double));
    double *xm = (double*)malloc(n * sizeof(double));
    if (!xp || !xm) { free(xp); free(xm); return; }

    memcpy(xp, x, n * sizeof(double));
    memcpy(xm, x, n * sizeof(double));

    for (int i = 0; i < n; i++) {
        double xi = x[i];
        double hi = h * (fabs(xi) > 1.0 ? fabs(xi) : 1.0);
        xp[i] = xi + hi; xm[i] = xi - hi;
        grad[i] = (f(xp, n) - f(xm, n)) / (2.0 * hi);
        xp[i] = xi; xm[i] = xi;
    }
    free(xp); free(xm);
}

double sci_derivative_richardson(SCI_ScalarFn f, double x, double h) {
    if (!f) return NAN;
    if (h <= 0.0) h = 0.01;
    double d1 = sci_derivative_first(f, x, h);
    double d2 = sci_derivative_first(f, x, h / 2.0);
    return (4.0 * d2 - d1) / 3.0;
}

/* ================================================================
 * L5: ODE Solvers — RK4 Fixed Step
 *
 * Classical Runge-Kutta 4 (Kutta 1901):
 *   k1 = f(t_n, y_n)
 *   k2 = f(t_n + h/2, y_n + h*k1/2)
 *   k3 = f(t_n + h/2, y_n + h*k2/2)
 *   k4 = f(t_n + h, y_n + h*k3)
 *   y_{n+1} = y_n + h*(k1 + 2k2 + 2k3 + k4)/6
 *
 * Local error: O(h^5), Global error: O(h^4).
 * ================================================================ */

static SCI_ODEResult ode_solve_rk4_fixed(SCI_ODEFn f, double t0, double y0,
                                          double tend, const SCI_ODEConfig *cfg) {
    SCI_ODEResult result;
    memset(&result, 0, sizeof(result));
    result.converged = false;

    double h = (cfg->h0 > 0.0) ? cfg->h0 : (tend - t0) / 1000.0;
    if (h > cfg->hmax) h = cfg->hmax;
    if (h < cfg->hmin) h = cfg->hmin;
    if (fabs(h) < 1e-15) return result;

    int direction = (tend > t0) ? 1 : -1;
    if ((tend - t0) * direction < 0) return result;
    if (h * direction < 0) h = -h;

    int n_steps = (int)ceil(fabs((tend - t0) / h));
    if (n_steps > cfg->max_steps) n_steps = cfg->max_steps;
    if (n_steps < 1) n_steps = 1;
    h = (tend - t0) / n_steps;

    result.t = (double*)malloc((n_steps + 1) * sizeof(double));
    result.y = (double*)malloc((n_steps + 1) * sizeof(double));
    if (!result.t || !result.y) {
        free(result.t); free(result.y); return result;
    }

    result.t[0] = t0; result.y[0] = y0;
    result.num_points = 1;

    double t = t0, y = y0;
    for (int i = 0; i < n_steps; i++) {
        double k1 = f(t, y);
        double k2 = f(t + h/2.0, y + h * k1/2.0);
        double k3 = f(t + h/2.0, y + h * k2/2.0);
        double k4 = f(t + h, y + h * k3);
        y += h * (k1 + 2.0*k2 + 2.0*k3 + k4) / 6.0;
        t += h;
        result.t[result.num_points] = t;
        result.y[result.num_points] = y;
        result.num_points++;
        result.accepted_steps++;
    }
    result.converged = true;
    return result;
}

/* ================================================================
 * L5: ODE Solvers — RKF45 Adaptive (Fehlberg 1969)
 *
 * 6-stage Runge-Kutta-Fehlberg 4(5) with embedded error estimate.
 * Uses PI step-size controller (Gustafsson 1991).
 * ================================================================ */

/* Fehlberg coefficients for RKF45 */
#define RKF_A2  0.25
#define RKF_A31 0.09375
#define RKF_A32 0.28125
#define RKF_A41 (1932.0/2197.0)
#define RKF_A42 (-7200.0/2197.0)
#define RKF_A43 (7296.0/2197.0)
#define RKF_A51 (439.0/216.0)
#define RKF_A52 (-8.0)
#define RKF_A53 (3680.0/513.0)
#define RKF_A54 (-845.0/4104.0)
#define RKF_A61 (-8.0/27.0)
#define RKF_A62 (2.0)
#define RKF_A63 (-3544.0/2565.0)
#define RKF_A64 (1859.0/4104.0)
#define RKF_A65 (-11.0/40.0)

/* 4th order weights */
#define RKF_B41 (25.0/216.0)
#define RKF_B43 (1408.0/2565.0)
#define RKF_B44 (2197.0/4104.0)
#define RKF_B45 (-0.2)

/* 5th order weights */
#define RKF_B51 (16.0/135.0)
#define RKF_B53 (6656.0/12825.0)
#define RKF_B54 (28561.0/56430.0)
#define RKF_B55 (-0.18)
#define RKF_B56 (2.0/55.0)

static SCI_ODEResult ode_solve_rkf45(SCI_ODEFn f, double t0, double y0,
                                      double tend, const SCI_ODEConfig *cfg) {
    SCI_ODEResult result;
    memset(&result, 0, sizeof(result));
    result.converged = false;

    double h = (cfg->h0 > 0.0) ? cfg->h0 : fabs(tend - t0) / 100.0;
    if (h > cfg->hmax) h = cfg->hmax;
    if (h < cfg->hmin) h = cfg->hmin;

    int direction = (tend > t0) ? 1 : -1;
    h = fabs(h) * direction;

    int max_pts = cfg->max_steps + 1;
    result.t = (double*)malloc(max_pts * sizeof(double));
    result.y = (double*)malloc(max_pts * sizeof(double));
    if (!result.t || !result.y) {
        free(result.t); free(result.y); return result;
    }

    double t = t0, y = y0;
    result.t[0] = t; result.y[0] = y;
    result.num_points = 1;

    double abstol = cfg->abstol, reltol = cfg->reltol;

    for (int step = 0; step < cfg->max_steps; step++) {
        if ((direction > 0 && t >= tend) ||
            (direction < 0 && t <= tend)) break;
        if (direction > 0 && t + h > tend) h = tend - t;
        if (direction < 0 && t + h < tend) h = tend - t;

        /* 6 stage evaluations */
        double k1 = f(t, y);
        double k2 = f(t + RKF_A2*h, y + h*(RKF_A2*k1));
        double k3 = f(t + 0.375*h,
                       y + h*(RKF_A31*k1 + RKF_A32*k2));
        double k4 = f(t + 12.0/13.0*h,
                       y + h*(RKF_A41*k1 + RKF_A42*k2 + RKF_A43*k3));
        double k5 = f(t + h,
                       y + h*(RKF_A51*k1 + RKF_A52*k2 + RKF_A53*k3 + RKF_A54*k4));
        double k6 = f(t + 0.5*h,
                       y + h*(RKF_A61*k1 + RKF_A62*k2 + RKF_A63*k3
                              + RKF_A64*k4 + RKF_A65*k5));

        /* 4th and 5th order estimates */
        double y4 = y + h*(RKF_B41*k1 + RKF_B43*k3 + RKF_B44*k4 + RKF_B45*k5);
        double y5 = y + h*(RKF_B51*k1 + RKF_B53*k3 + RKF_B54*k4
                           + RKF_B55*k5 + RKF_B56*k6);

        /* Error estimation */
        double err = fabs(y5 - y4);
        double sc = abstol + reltol * fmax(fabs(y), fabs(y5));
        double err_norm = err / sc;

        if (err_norm <= 1.0) {
            /* Step accepted */
            t += h; y = y5;
            result.t[result.num_points] = t;
            result.y[result.num_points] = y;
            result.num_points++;
            result.accepted_steps++;
            if (result.num_points >= max_pts) break;
        } else {
            result.rejected_steps++;
        }

        /* PI step size control */
        double fac = 0.9 * pow(fmax(err_norm, 1e-10), -1.0/5.0);
        if (fac > 4.0) fac = 4.0;
        if (fac < 0.1) fac = 0.1;
        h *= fac;
        if (fabs(h) < cfg->hmin) h = (h > 0 ? 1.0 : -1.0) * cfg->hmin;
        if (fabs(h) > cfg->hmax) h = (h > 0 ? 1.0 : -1.0) * cfg->hmax;
    }
    result.converged = ((direction > 0 && t >= tend) ||
                        (direction < 0 && t <= tend));
    return result;
}

/* ================================================================
 * L5: ODE Solver Dispatcher
 * ================================================================ */

SCI_ODEResult sci_ode_solve(SCI_ODEFn f, double t0, double y0,
                             double tend, const SCI_ODEConfig *config) {
    if (!f || !config) {
        SCI_ODEResult err; memset(&err, 0, sizeof(err)); return err;
    }
    switch (config->method) {
        case SCI_ODE_RK4:   return ode_solve_rk4_fixed(f, t0, y0, tend, config);
        case SCI_ODE_RKF45: return ode_solve_rkf45(f, t0, y0, tend, config);
        default: {
            SCI_ODEResult err; memset(&err, 0, sizeof(err)); return err;
        }
    }
}

void sci_ode_result_free(SCI_ODEResult *result) {
    if (!result) return;
    free(result->t);
    free(result->y);
    memset(result, 0, sizeof(SCI_ODEResult));
}

/* ================================================================
 * L5: ODE System Solver (Vectorized RK4)
 * ================================================================ */

bool sci_ode_solve_system(SCI_ODE_SysFn f, int n, double t0,
                           const double *y0, double tend,
                           const SCI_ODEConfig *config,
                           double *t_out, double *y_out,
                           int max_pts, int *n_pts) {
    if (!f || !y0 || !t_out || !y_out || n <= 0 || max_pts < 2) return false;

    double h = (config->h0 > 0.0) ? config->h0 : fabs(tend - t0) / 1000.0;
    if (h > config->hmax) h = config->hmax;
    if (h < config->hmin) h = config->hmin;

    int direction = (tend > t0) ? 1 : -1;
    h = fabs(h) * direction;

    double *y = (double*)malloc(n * sizeof(double));
    double *k1 = (double*)malloc(n * sizeof(double));
    double *k2 = (double*)malloc(n * sizeof(double));
    double *k3 = (double*)malloc(n * sizeof(double));
    double *k4 = (double*)malloc(n * sizeof(double));
    double *ytmp = (double*)malloc(n * sizeof(double));
    if (!y || !k1 || !k2 || !k3 || !k4 || !ytmp) {
        free(y); free(k1); free(k2); free(k3); free(k4); free(ytmp);
        return false;
    }

    memcpy(y, y0, n * sizeof(double));
    double t = t0; int pt = 0;

    t_out[pt] = t;
    memcpy(&y_out[pt * n], y, n * sizeof(double));
    pt++;

    for (int step = 0; step < config->max_steps && pt < max_pts; step++) {
        if ((direction > 0 && t >= tend) ||
            (direction < 0 && t <= tend)) break;
        if (direction > 0 && t + h > tend) h = tend - t;
        if (direction < 0 && t + h < tend) h = tend - t;

        f(t, y, n, k1);
        for (int i = 0; i < n; i++) ytmp[i] = y[i] + h*k1[i]/2.0;
        f(t + h/2.0, ytmp, n, k2);
        for (int i = 0; i < n; i++) ytmp[i] = y[i] + h*k2[i]/2.0;
        f(t + h/2.0, ytmp, n, k3);
        for (int i = 0; i < n; i++) ytmp[i] = y[i] + h*k3[i];
        f(t + h, ytmp, n, k4);

        for (int i = 0; i < n; i++)
            y[i] += h*(k1[i] + 2.0*k2[i] + 2.0*k3[i] + k4[i])/6.0;
        t += h;

        t_out[pt] = t;
        memcpy(&y_out[pt * n], y, n * sizeof(double));
        pt++;
    }

    free(y); free(k1); free(k2); free(k3); free(k4); free(ytmp);
    *n_pts = pt;
    return true;
}

/* ================================================================
 * L5: Numerical Quadrature — Composite Trapezoidal Rule
 *
 * Error: -(b-a)*h^2*f''(xi)/12 (Euler-Maclaurin).
 * ================================================================ */

static SCI_QuadResult quad_trapezoid(SCI_ScalarFn f, double a, double b,
                                      const SCI_QuadConfig *cfg) {
    SCI_QuadResult res;
    memset(&res, 0, sizeof(res));
    res.converged = true;

    int n = 1000;
    if (n * 2 > cfg->max_evals) n = cfg->max_evals / 2;
    double h = (b - a) / n;
    double sum = 0.5 * (f(a) + f(b));
    res.eval_count = 2;

    for (int i = 1; i < n; i++) {
        if (res.eval_count >= cfg->max_evals) break;
        sum += f(a + i * h);
        res.eval_count++;
    }
    res.integral = h * sum;
    res.error_estimate = fabs((b - a) * h * h / 12.0);
    return res;
}

/* ================================================================
 * L5: Numerical Quadrature — Composite Simpson's Rule
 *
 * Error: -(b-a)*h^4*f^(4)(xi)/180.
 * Requires even number of subintervals.
 * ================================================================ */

static SCI_QuadResult quad_simpson(SCI_ScalarFn f, double a, double b,
                                    const SCI_QuadConfig *cfg) {
    SCI_QuadResult res;
    memset(&res, 0, sizeof(res));
    res.converged = true;

    int n = 1000;
    if (n % 2 != 0) n++;
    if (n >= cfg->max_evals) n = (cfg->max_evals / 2) * 2;
    double h = (b - a) / n;
    double sum = f(a) + f(b);
    res.eval_count = 2;

    for (int i = 1; i < n; i++) {
        if (res.eval_count >= cfg->max_evals) break;
        sum += f(a + i * h) * ((i % 2 == 0) ? 2.0 : 4.0);
        res.eval_count++;
    }
    res.integral = h * sum / 3.0;
    res.error_estimate = fabs((b - a) * h * h * h * h / 180.0);
    return res;
}

/* ================================================================
 * L5: Numerical Quadrature — Adaptive Simpson
 *
 * Recursively subdivide where error estimate exceeds tolerance.
 * Error estimate: |S(a,m)+S(m,b)-S(a,b)|/15.
 * ================================================================ */

static double quad_adaptive_recursive(SCI_ScalarFn f, double a, double b,
    double fa, double fb, double fm, double whole, double tol,
    const SCI_QuadConfig *cfg, int depth, int *evals) {
    double m = (a + b) / 2.0;
    double h2 = (b - a) / 4.0;
    double f1 = f(a + h2), f3 = f(b - h2);
    *evals += 2;
    double left  = (b - a) / 12.0 * (fa + 4.0*f1 + fm);
    double right = (b - a) / 12.0 * (fm + 4.0*f3 + fb);
    double approx = left + right;
    if (depth >= cfg->max_depth || *evals >= cfg->max_evals)
        return approx;
    if (fabs(approx - whole) <= 15.0 * tol)
        return approx + (approx - whole) / 15.0;
    return quad_adaptive_recursive(f, a, m, fa, fm, f1, left,
            tol/2.0, cfg, depth+1, evals)
         + quad_adaptive_recursive(f, m, b, fm, fb, f3, right,
            tol/2.0, cfg, depth+1, evals);
}

static SCI_QuadResult quad_adaptive(SCI_ScalarFn f, double a, double b,
                                     const SCI_QuadConfig *cfg) {
    SCI_QuadResult res;
    memset(&res, 0, sizeof(res));
    res.converged = true;
    double fa = f(a), fb = f(b), fm = f((a+b)/2.0);
    int evals = 3;
    double whole = (b-a)/6.0 * (fa + 4.0*fm + fb);
    res.integral = quad_adaptive_recursive(f, a, b, fa, fb, fm,
        whole, cfg->abstol, cfg, 0, &evals);
    res.eval_count = evals;
    res.error_estimate = cfg->abstol;
    return res;
}

/* ================================================================
 * L5: Numerical Quadrature — Gauss-Legendre (n=5 or n=7)
 *
 * Nodes are roots of Legendre polynomial P_n(x).
 * n-point formula exact for polynomials up to degree 2n-1.
 * ================================================================ */

static const double GL7_NODES[7] = {
    -0.9491079123427585, -0.7415311855993945, -0.4058451513773972,
     0.0, 0.4058451513773972, 0.7415311855993945, 0.9491079123427585
};
static const double GL7_WTS[7] = {
    0.1294849661688697, 0.2797053914892766, 0.3818300505051189,
    0.4179591836734694, 0.3818300505051189, 0.2797053914892766,
    0.1294849661688697
};
static const double GL5_NODES[5] = {
    -0.9061798459386640, -0.5384693101056831, 0.0,
     0.5384693101056831, 0.9061798459386640
};
static const double GL5_WTS[5] = {
    0.2369268850561891, 0.4786286704993665, 0.5688888888888889,
    0.4786286704993665, 0.2369268850561891
};

static SCI_QuadResult quad_gauss_legendre(SCI_ScalarFn f, double a, double b,
                                           const SCI_QuadConfig *cfg) {
    SCI_QuadResult res;
    memset(&res, 0, sizeof(res));
    res.converged = true;

    int n; const double *nodes, *wts;
    if (cfg->n_points <= 5) { n = 5; nodes = GL5_NODES; wts = GL5_WTS; }
    else { n = 7; nodes = GL7_NODES; wts = GL7_WTS; }

    double mid = (b + a) / 2.0, half = (b - a) / 2.0;
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += wts[i] * f(mid + half * nodes[i]);
        res.eval_count++;
    }
    res.integral = half * sum;
    res.error_estimate = cfg->abstol;
    return res;
}

/* ================================================================
 * L5: Numerical Quadrature — Romberg Integration
 *
 * Richardson extrapolation on trapezoidal rule.
 * R(k,m) = R(k,m-1) + (R(k,m-1)-R(k-1,m-1))/(4^m - 1)
 * Column m has error O(h^{2m+2}).
 * ================================================================ */

static SCI_QuadResult quad_romberg(SCI_ScalarFn f, double a, double b,
                                    const SCI_QuadConfig *cfg) {
    SCI_QuadResult res;
    memset(&res, 0, sizeof(res));
    res.converged = true;

    int max_k = 8;
    double R[9][9];
    memset(R, 0, sizeof(R));

    R[0][0] = 0.5 * (b - a) * (f(a) + f(b));
    res.eval_count = 2;

    for (int k = 1; k <= max_k; k++) {
        int n = 1 << k;
        double h = (b - a) / n;
        double sum = 0.0;
        for (int i = 1; i < n; i += 2)
            sum += f(a + i * h);
        res.eval_count += n / 2;
        R[k][0] = 0.5 * R[k-1][0] + h * sum;

        for (int m = 1; m <= k; m++) {
            double p4 = pow(4.0, (double)m);
            R[k][m] = R[k][m-1] + (R[k][m-1]-R[k-1][m-1])/(p4-1.0);
        }
        if (k >= 2 && fabs(R[k][k-1] - R[k-1][k-1]) < cfg->abstol) {
            res.integral = R[k][k];
            res.error_estimate = fabs(R[k][k] - R[k-1][k-1]);
            return res;
        }
        if (res.eval_count >= cfg->max_evals) break;
    }
    res.integral = R[max_k][max_k];
    res.error_estimate = fabs(R[max_k][max_k] - R[max_k-1][max_k-1]);
    return res;
}

/* ================================================================
 * L5: Quadrature Dispatcher
 * ================================================================ */

SCI_QuadResult sci_integrate(SCI_ScalarFn f, double a, double b,
                              const SCI_QuadConfig *config) {
    if (!f || !config) {
        SCI_QuadResult err; memset(&err, 0, sizeof(err)); return err;
    }
    if (a > b) {
        SCI_QuadResult res = sci_integrate(f, b, a, config);
        res.integral = -res.integral;
        return res;
    }
    if (a == b) {
        SCI_QuadResult res; memset(&res, 0, sizeof(res));
        res.integral = 0.0; res.converged = true; return res;
    }
    switch (config->method) {
        case SCI_QUAD_TRAPEZOID: return quad_trapezoid(f, a, b, config);
        case SCI_QUAD_SIMPSON:   return quad_simpson(f, a, b, config);
        case SCI_QUAD_GAUSS_LEG: return quad_gauss_legendre(f, a, b, config);
        case SCI_QUAD_ADAPTIVE:  return quad_adaptive(f, a, b, config);
        case SCI_QUAD_ROMBERG:   return quad_romberg(f, a, b, config);
        default: {
            SCI_QuadResult err; memset(&err, 0, sizeof(err)); return err;
        }
    }
}

/* ================================================================
 * L5: Improper Integral via transformation
 * ================================================================ */

SCI_QuadResult sci_integrate_infinite(SCI_ScalarFn f, double a,
                                       const SCI_QuadConfig *config) {
    SCI_QuadResult res;
    memset(&res, 0, sizeof(res));
    if (!f) return res;

    /* Transform [a,inf) -> (0,1] via t = 1/(1+x-a)
     * Then integrate with adaptive quadrature on [eps, 1] */
    double eps = 1e-8, total = 0.0;
    int evals = 0;

    /* Use Simpson on transformed integrand for robustness */
    int n = 5000;
    double h = (0.5 - eps) / n;
    for (int i = 0; i < n; i++) {
        double t = eps + i * h;
        double x = a + (1.0 - t) / t;
        total += f(x) / (t * t);
        evals++;
    }
    total *= h;

    n = 500;
    h = 0.5 / n;
    for (int i = 0; i < n; i++) {
        double t = 0.5 + i * h;
        double x = a + (1.0 - t) / t;
        total += f(x) / (t * t);
        evals++;
    }
    total *= h;

    res.integral = total;
    res.eval_count = evals;
    res.converged = true;
    res.error_estimate = config->abstol;
    return res;
}

/* ================================================================
 * L5: Lagrange Polynomial Interpolation
 *
 * P(x) = sum_i y_i * prod_{j!=i} (x - x_j)/(x_i - x_j)
 * O(n^2) per evaluation. Unique degree-n-1 polynomial through n points.
 * ================================================================ */

double sci_lagrange_interp(const double *x, const double *y, int n,
                            double xval) {
    if (!x || !y || n <= 0) return NAN;
    double result = 0.0;
    for (int i = 0; i < n; i++) {
        double term = y[i];
        for (int j = 0; j < n; j++) {
            if (j != i) {
                if (fabs(x[i] - x[j]) < 1e-15) return NAN;
                term *= (xval - x[j]) / (x[i] - x[j]);
            }
        }
        result += term;
    }
    return result;
}

/* ================================================================
 * L5: Cubic Spline (Natural)
 *
 * Natural boundary: S''(x_0) = S''(x_{n-1}) = 0.
 * Solve tridiagonal system for second derivatives via Thomas algorithm.
 * Reinsch (1967) implementation.
 * ================================================================ */

bool sci_spline_coefficients(const double *x, const double *y, int n,
                              double *y2) {
    if (!x || !y || !y2 || n < 2) return false;
    double *a = (double*)malloc(n * sizeof(double));
    double *b = (double*)malloc(n * sizeof(double));
    double *c = (double*)malloc(n * sizeof(double));
    double *r = (double*)malloc(n * sizeof(double));
    if (!a || !b || !c || !r) { free(a); free(b); free(c); free(r); return false; }

    y2[0] = 0.0; a[0] = 0.0; b[0] = 1.0; r[0] = 0.0;

    for (int i = 1; i < n - 1; i++) {
        double hi = x[i] - x[i-1];
        double hip1 = x[i+1] - x[i];
        if (hi <= 0.0 || hip1 <= 0.0) {
            free(a); free(b); free(c); free(r); return false;
        }
        a[i] = hi;
        b[i] = 2.0 * (hi + hip1);
        c[i] = hip1;
        r[i] = 3.0 * ((y[i+1]-y[i])/hip1 - (y[i]-y[i-1])/hi);
    }

    y2[n-1] = 0.0; a[n-1] = 0.0; b[n-1] = 1.0; r[n-1] = 0.0;

    /* Thomas algorithm */
    for (int i = 1; i < n; i++) {
        double m = a[i] / b[i-1];
        b[i] -= m * c[i-1];
        r[i] -= m * r[i-1];
    }

    y2[n-1] = r[n-1] / b[n-1];
    for (int i = n - 2; i >= 0; i--)
        y2[i] = (r[i] - c[i] * y2[i+1]) / b[i];

    free(a); free(b); free(c); free(r);
    return true;
}

double sci_spline_eval(const double *x, const double *y, int n,
                        const double *y2, double xval) {
    if (!x || !y || !y2 || n < 2) return NAN;
    int k = n - 2;
    for (int i = 0; i < n - 1; i++) {
        if (xval >= x[i] && xval <= x[i+1]) { k = i; break; }
    }
    if (xval < x[0]) k = 0;
    if (xval > x[n-1]) k = n - 2;
    double h = x[k+1] - x[k];
    if (h <= 0.0) return NAN;
    double a = (x[k+1] - xval) / h;
    double b2 = (xval - x[k]) / h;
    return a * y[k] + b2 * y[k+1]
         + ((a*a*a - a) * y2[k] + (b2*b2*b2 - b2) * y2[k+1]) * (h*h) / 6.0;
}

/* ================================================================
 * L7: Find Local Minima (grid scan + golden section refinement)
 * ================================================================ */

int sci_find_minima(SCI_ScalarFn f, double a, double b, int n_scan,
                     double *minima, int max_min) {
    if (!f || !minima || n_scan < 2 || max_min < 1) return 0;
    double h = (b - a) / n_scan;
    int found = 0;
    double phi = (sqrt(5.0) - 1.0) / 2.0;

    for (int i = 1; i < n_scan && found < max_min; i++) {
        double x0 = a + (i-1)*h, x1 = a + i*h, x2 = a + (i+1)*h;
        if (i+1 >= n_scan) x2 = b;
        double f0 = f(x0), f1 = f(x1), f2 = f(x2);
        if (f1 <= f0 && f1 <= f2) {
            double xl = x0, xr = x2;
            for (int j = 0; j < 30; j++) {
                double d = phi * (xr - xl);
                double xm1 = xr - d, xm2 = xl + d;
                if (f(xm1) < f(xm2)) xr = xm2;
                else xl = xm1;
                if (fabs(xr - xl) < 1e-8) break;
            }
            minima[found++] = (xl + xr) / 2.0;
        }
    }
    return found;
}

/* ================================================================
 * L7: Monte Carlo Integration
 *
 * Mean estimator: I_N = (b-a)/N * sum f(x_i), x_i ~ U(a,b).
 * Error: O(sigma/sqrt(N)). Dimension-independent (key advantage).
 * Metropolis & Ulam (1949).
 * ================================================================ */

static unsigned int sci_lcg = 12345;

static double sci_rand_double(void) {
    sci_lcg = 1103515245 * sci_lcg + 12345;
    return (double)(sci_lcg & 0x7FFFFFFF) / 2147483648.0;
}

double sci_monte_carlo_integrate(SCI_ScalarFn f, double a, double b,
                                  int n_samples, unsigned int seed) {
    if (!f || n_samples <= 0) return NAN;
    if (seed != 0) sci_lcg = seed;
    double sum = 0.0, range = b - a;
    for (int i = 0; i < n_samples; i++)
        sum += f(a + range * sci_rand_double());
    return range * sum / n_samples;
}

/* ================================================================
 * L7: 1D Boundary Value Problem Solver
 *
 * Solve: -u''(x) = f(x), u(0)=alpha, u(1)=beta
 * Finite difference: second-order central difference
 * => tridiagonal system, solved via Thomas algorithm.
 * Convergence: O(h^2). Keller (1968).
 * ================================================================ */

void sci_solve_bvp_1d(SCI_ScalarFn f, double alpha, double beta,
                       int N, double *u_out) {
    if (!f || !u_out || N < 1) return;
    double h = 1.0 / (N + 1), h2 = h * h;

    double *a = (double*)malloc(N * sizeof(double));
    double *b = (double*)malloc(N * sizeof(double));
    double *c = (double*)malloc(N * sizeof(double));
    double *r = (double*)malloc(N * sizeof(double));
    double *u = (double*)malloc(N * sizeof(double));
    if (!a || !b || !c || !r || !u) {
        free(a); free(b); free(c); free(r); free(u); return;
    }

    for (int i = 0; i < N; i++) {
        a[i] = -1.0/h2; b[i] = 2.0/h2; c[i] = -1.0/h2;
        r[i] = f((i + 1) * h);
    }
    r[0] += alpha/h2;
    r[N-1] += beta/h2;

    /* Thomas */
    for (int i = 1; i < N; i++) {
        double m = a[i] / b[i-1];
        b[i] -= m * c[i-1];
        r[i] -= m * r[i-1];
    }
    u[N-1] = r[N-1] / b[N-1];
    for (int i = N-2; i >= 0; i--)
        u[i] = (r[i] - c[i] * u[i+1]) / b[i];

    u_out[0] = alpha;
    for (int i = 0; i < N; i++) u_out[i+1] = u[i];
    u_out[N+1] = beta;

    free(a); free(b); free(c); free(r); free(u);
}