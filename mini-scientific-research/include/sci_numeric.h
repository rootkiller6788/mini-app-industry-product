#ifndef MINI_SCI_NUMERIC_H
#define MINI_SCI_NUMERIC_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* ================================================================
 *  sci_numeric -- Numerical Methods for Scientific Research
 *
 *  L1: Core types -- function types, result structs, solver configs
 *  L2: Core concepts -- convergence, error analysis, numerical stability,
 *      truncation error vs roundoff error, order of convergence
 *  L3: Engineering structures -- adaptive step control, Richardson
 *      extrapolation, deferred correction, quadrature selection
 *  L4: Standards/Theorems -- Taylor Theorem with Remainder, Mean Value
 *      Theorem, Lax Equivalence Theorem, Lipschitz conditions
 *  L5: Algorithms -- Newton-Raphson, Runge-Kutta (RK4/RKF45), adaptive
 *      quadrature, Brent method, Romberg integration
 *  L6: Canonical problems -- Root finding, ODE IVP, numerical integration
 *  L7: Applications -- Physics simulation, engineering analysis, data
 *      fitting, computational biology
 *  L8: Advanced -- Multigrid methods, spectral methods, symplectic
 *      integrators, stiff ODE solvers (BDF, implicit RK)
 *  L9: Industry -- Sundials, GSL, PETSc, deal.II, MATLAB/Julia numerical
 *
 *  References:
 *  - Burden & Faires, "Numerical Analysis" (10th ed, 2016)
 *  - Press et al., "Numerical Recipes" (3rd ed, 2007)
 *  - Hairer, Norsett & Wanner, "Solving Ordinary Differential Eqs"
 *  - Trefethen & Bau, "Numerical Linear Algebra" (1997)
 * ================================================================ */

typedef double (*SCI_ScalarFn)(double x);
typedef double (*SCI_MultiVarFn)(const double *x, int n);
typedef double (*SCI_ODEFn)(double t, double y);
typedef void (*SCI_ODE_SysFn)(double t, const double *y, int n, double *dydt);
typedef double (*SCI_ParamFn)(double x, const void *params);

typedef enum {
    SCI_CONVERGE_ABSOLUTE  = 0,
    SCI_CONVERGE_RELATIVE  = 1,
    SCI_CONVERGE_RESIDUAL  = 2,
    SCI_CONVERGE_INTERVAL  = 3,
} SCI_ConvergeType;

typedef enum {
    SCI_ODE_EULER          = 0,
    SCI_ODE_HEUN           = 1,
    SCI_ODE_RK4            = 2,
    SCI_ODE_RKF45          = 3,
    SCI_ODE_DORMAND_PRINCE = 4,
    SCI_ODE_IMPLICIT_EULER = 5,
} SCI_ODEMethod;

typedef enum {
    SCI_QUAD_TRAPEZOID   = 0,
    SCI_QUAD_SIMPSON     = 1,
    SCI_QUAD_GAUSS_LEG   = 2,
    SCI_QUAD_ADAPTIVE    = 3,
    SCI_QUAD_ROMBERG     = 4,
    SCI_QUAD_GAUSS_KRON  = 5,
} SCI_QuadMethod;

typedef enum {
    SCI_ROOT_BISECTION   = 0,
    SCI_ROOT_NEWTON      = 1,
    SCI_ROOT_SECANT      = 2,
    SCI_ROOT_BRENT       = 3,
    SCI_ROOT_RIDDERS     = 4,
} SCI_RootMethod;

typedef struct {
    double   root;
    double   residual;
    int      iterations;
    int      function_evals;
    bool     converged;
    SCI_ConvergeType criteria;
} SCI_RootResult;

typedef struct {
    double  *t;
    double  *y;
    int      num_points;
    double   error_estimate;
    int      accepted_steps;
    int      rejected_steps;
    bool     converged;
} SCI_ODEResult;

typedef struct {
    double   integral;
    double   error_estimate;
    int      eval_count;
    bool     converged;
} SCI_QuadResult;

typedef struct {
    SCI_RootMethod method;
    double         xtol;
    double         ftol;
    int            max_iter;
    bool           trace;
    SCI_ConvergeType criteria;
} SCI_RootConfig;

typedef struct {
    SCI_ODEMethod method;
    double        abstol;
    double        reltol;
    double        h0;
    double        hmin;
    double        hmax;
    int           max_steps;
    bool          dense_output;
} SCI_ODEConfig;

typedef struct {
    SCI_QuadMethod method;
    double         abstol;
    double         reltol;
    int            max_evals;
    int            n_points;
    int            max_depth;
} SCI_QuadConfig;

SCI_RootConfig sci_root_config_default(SCI_RootMethod method);
SCI_ODEConfig sci_ode_config_default(SCI_ODEMethod method);
SCI_QuadConfig sci_quad_config_default(SCI_QuadMethod method);

SCI_RootResult sci_find_root(SCI_ScalarFn f, double a, double b,
                              const SCI_RootConfig *config);
SCI_RootResult sci_newton_root(SCI_ScalarFn f, SCI_ScalarFn df,
                                double x0, double xtol, int maxit);
SCI_RootResult sci_brent_root(SCI_ScalarFn f, double a, double b,
                               double xtol, int maxit);
int sci_find_all_roots(SCI_ScalarFn f, double a, double b, int n_sub,
                        double xtol, double *roots_out, int max_roots);

double sci_derivative_first(SCI_ScalarFn f, double x, double h);
double sci_derivative_second(SCI_ScalarFn f, double x, double h);
void sci_gradient(SCI_MultiVarFn f, const double *x, int n,
                  double h, double *grad);
double sci_derivative_richardson(SCI_ScalarFn f, double x, double h);

SCI_ODEResult sci_ode_solve(SCI_ODEFn f, double t0, double y0,
                             double tend, const SCI_ODEConfig *config);
bool sci_ode_solve_system(SCI_ODE_SysFn f, int n, double t0,
                           const double *y0, double tend,
                           const SCI_ODEConfig *config,
                           double *t_out, double *y_out,
                           int max_pts, int *n_pts);
void sci_ode_result_free(SCI_ODEResult *result);

SCI_QuadResult sci_integrate(SCI_ScalarFn f, double a, double b,
                              const SCI_QuadConfig *config);
SCI_QuadResult sci_integrate_infinite(SCI_ScalarFn f, double a,
                                       const SCI_QuadConfig *config);

double sci_lagrange_interp(const double *x, const double *y, int n,
                            double xval);
bool sci_spline_coefficients(const double *x, const double *y, int n,
                              double *y2);
double sci_spline_eval(const double *x, const double *y, int n,
                        const double *y2, double xval);

int sci_find_minima(SCI_ScalarFn f, double a, double b, int n_scan,
                     double *minima, int max_min);
double sci_monte_carlo_integrate(SCI_ScalarFn f, double a, double b,
                                  int n_samples, unsigned int seed);
void sci_solve_bvp_1d(SCI_ScalarFn f, double alpha, double beta,
                       int N, double *u_out);

#endif /* MINI_SCI_NUMERIC_H */