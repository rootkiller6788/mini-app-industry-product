#ifndef MINI_SCI_OPTIMIZE_H
#define MINI_SCI_OPTIMIZE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* ================================================================
 *  sci_optimize -- Optimization Methods for Scientific Research
 *
 *  L1: Core types -- objective function types, optimizer configs,
 *      result structures, constraint representations
 *  L2: Core concepts -- local/global minima, convexity, KKT conditions,
 *      duality, Lagrange multipliers, constraint qualification
 *  L3: Engineering structures -- line search, trust region, barrier
 *      methods, multi-start, population-based search
 *  L4: Standards/Theorems -- Karush-Kuhn-Tucker (KKT) conditions
 *      (Kuhn & Tucker 1951), Wolfe conditions for line search,
 *      Rockafellar convex analysis, No Free Lunch Theorem
 *  L5: Algorithms -- Gradient descent (Cauchy 1847), Newton method,
 *      BFGS (Broyden-Fletcher-Goldfarb-Shanno 1970), Nelder-Mead simplex
 *      (1965), Genetic Algorithm (Holland 1975), Simulated Annealing
 *      (Kirkpatrick et al. 1983), Linear Programming via Simplex method
 *      (Dantzig 1947)
 *  L6: Canonical problems -- Function minimization, constrained
 *      optimization, parameter estimation, model fitting
 *  L7: Applications -- Machine learning hyperparameter tuning, engineering
 *      design optimization, portfolio optimization (Markowitz),
 *      molecular docking
 *  L8: Advanced -- SQP (Sequential Quadratic Programming), interior-point
 *      methods, ADMM, stochastic gradient descent with momentum, CMA-ES
 *  L9: Industry -- IPOPT, NLopt, SciPy.optimize, Ceres, Gurobi
 *
 *  References:
 *  - Nocedal & Wright, "Numerical Optimization" (2nd ed, 2006)
 *  - Boyd & Vandenberghe, "Convex Optimization" (2004)
 *  - Wolpert & Macready, "No Free Lunch Theorems" (1997)
 * ================================================================ */

/* -----------------------------------------------------------------
 * L1: Core Type Definitions
 * ----------------------------------------------------------------- */

/** Objective function f(x) for n-dimensional optimization */
typedef double (*SCI_ObjFn)(const double *x, int n);

/** Gradient of objective function: grad f(x) */
typedef void (*SCI_GradFn)(const double *x, int n, double *grad);

/** Constraint function g(x) <= 0 or g(x) = 0 */
typedef double (*SCI_ConstraintFn)(const double *x, int n);

/** Callback per iteration: called with current state */
typedef void (*SCI_OptCallback)(const double *x, int n, int iter,
                                 double fval, double grad_norm);

/** Optimizer method selection */
typedef enum {
    SCI_OPT_GRADIENT_DESCENT = 0,
    SCI_OPT_NEWTON           = 1,
    SCI_OPT_BFGS             = 2,
    SCI_OPT_NELDER_MEAD      = 3,
    SCI_OPT_GENETIC          = 4,
    SCI_OPT_SIMULATED_ANNEAL = 5,
    SCI_OPT_CONJUGATE_GRAD   = 6,
    SCI_OPT_L_BFGS           = 7,
} SCI_OptMethod;

/** Line search method */
typedef enum {
    SCI_LINESEARCH_BACKTRACK = 0,  /* Armijo backtracking */
    SCI_LINESEARCH_WOLFE     = 1,  /* Strong Wolfe conditions */
    SCI_LINESEARCH_NONE      = 2,  /* Fixed step */
} SCI_LineSearch;

/** Constraint type */
typedef enum {
    SCI_CONSTRAINT_EQ      = 0,  /* g(x) = 0 */
    SCI_CONSTRAINT_INEQ_LE = 1,  /* g(x) <= 0 */
    SCI_CONSTRAINT_BOX     = 2,  /* lb <= x <= ub */
} SCI_ConstraintType;

typedef struct {
    SCI_ConstraintType type;
    SCI_ConstraintFn   fn;
    double             rhs;       /* Right-hand side */
    double             tolerance; /* Constraint tolerance */
} SCI_Constraint;

typedef struct {
    SCI_OptMethod  method;
    double         ftol;          /* Function tolerance (default 1e-8) */
    double         gtol;          /* Gradient tolerance (default 1e-6) */
    double         xtol;          /* Parameter tolerance */
    int            max_iter;      /* Maximum iterations (default 1000) */
    int            max_evals;     /* Maximum function evaluations */
    double         step_size;     /* Initial step size / learning rate */
    SCI_LineSearch line_search;   /* Line search strategy */
    double         armijo_c;      /* Armijo constant (0 < c < 1, default 1e-4) */
    double         wolfe_c2;      /* Wolfe curvature (default 0.9) */
    bool           trace;         /* Print iteration info */
    SCI_OptCallback callback;     /* Per-iteration callback (or NULL) */
    /* Constraints */
    const SCI_Constraint *constraints;
    int             num_constraints;
    const double    *lower_bounds;  /* Box constraints lower (or NULL) */
    const double    *upper_bounds;  /* Box constraints upper (or NULL) */
    /* Genetic algorithm specific */
    int             pop_size;      /* Population size (default 50) */
    double          crossover_rate;/* Crossover probability (default 0.8) */
    double          mutation_rate; /* Mutation probability (default 0.01) */
    /* Simulated annealing specific */
    double          initial_temp;  /* Initial temperature (default 100) */
    double          cooling_rate;  /* Cooling multiplier (default 0.95) */
} SCI_OptConfig;

typedef struct {
    bool     converged;
    double   optimal_value;   /* f(x*) */
    double  *optimal_point;   /* x* (size n, allocated by solver) */
    int      n;               /* Dimension */
    int      iterations;      /* Iterations performed */
    int      func_evals;      /* Function evaluations */
    int      grad_evals;      /* Gradient evaluations */
    double   final_grad_norm; /* ||grad f(x*)|| */
    double   gradient_0_norm; /* Initial gradient norm for scaling */
    const char *termination;  /* Reason for termination */
} SCI_OptResult;

/* -----------------------------------------------------------------
 * L2-L3: Configuration & Initialization
 * ----------------------------------------------------------------- */

SCI_OptConfig sci_opt_config_default(SCI_OptMethod method);
void sci_opt_result_free(SCI_OptResult *result);

/* -----------------------------------------------------------------
 * L5: Unconstrained Optimization
 * ----------------------------------------------------------------- */

/**
 * Minimize f: R^n -> R using gradient descent with line search.
 *
 * x_{k+1} = x_k - alpha_k * grad f(x_k)
 *
 * Uses Armijo backtracking line search to ensure sufficient decrease:
 *   f(x_k + alpha * p_k) <= f(x_k) + c * alpha * grad f(x_k)^T p_k
 * where p_k = -grad f(x_k) and c in (0,1).
 *
 * Convergence: O(1/k) for convex functions, linear rate for strongly convex.
 *
 * Theorem (Armijo 1966): Under Lipschitz-continuous gradient, the
 * backtracking line search terminates in finite steps.
 *
 * @param f       Objective function
 * @param grad_f  Gradient function
 * @param x0      Initial guess (size n, modified in place for output)
 * @param n       Dimension
 * @param config  Optimizer configuration
 * @return        Optimization result
 */
SCI_OptResult sci_minimize_gradient_descent(SCI_ObjFn f, SCI_GradFn grad_f,
                                             double *x0, int n,
                                             const SCI_OptConfig *config);

/**
 * Minimize using Newton method.
 *
 * x_{k+1} = x_k - [H f(x_k)]^{-1} * grad f(x_k)
 *
 * Quadratic convergence near optimum (Kantorovich theorem).
 * Each iteration requires solving H * p = -grad for search direction p.
 * Uses finite difference Hessian approximation if no analytic Hessian.
 *
 * Complexity: O(n^3) per iteration for Hessian solve.
 */
SCI_OptResult sci_minimize_newton(SCI_ObjFn f, SCI_GradFn grad_f,
                                   double *x0, int n,
                                   const SCI_OptConfig *config);

/**
 * Minimize using BFGS quasi-Newton method.
 *
 * Maintains approximate inverse Hessian B_k, updated via:
 *   B_{k+1} = (I - rho_k s_k y_k^T) B_k (I - rho_k y_k s_k^T) + rho_k s_k s_k^T
 * where s_k = x_{k+1} - x_k, y_k = grad_{k+1} - grad_k, rho_k = 1/(y_k^T s_k)
 *
 * BFGS (Broyden 1970, Fletcher 1970, Goldfarb 1970, Shanno 1970):
 * Superlinear convergence, O(n^2) per iteration instead of O(n^3).
 * Most popular quasi-Newton method in practice.
 */
SCI_OptResult sci_minimize_bfgs(SCI_ObjFn f, SCI_GradFn grad_f,
                                 double *x0, int n,
                                 const SCI_OptConfig *config);

/**
 * Minimize using derivative-free Nelder-Mead simplex method.
 *
 * Maintains (n+1) vertices forming a simplex, applies:
 *   reflection, expansion, contraction, and shrinkage operations.
 *
 * Nelder & Mead (1965): "A Simplex Method for Function Minimization"
 * Simple and robust, though convergence is not guaranteed in theory.
 * Works well for low-dimensional problems (n < 20).
 *
 * @param f       Objective function (no gradient needed)
 * @param x0      Initial guess (size n, modified in place)
 * @param n       Dimension
 * @param simplex_scale Initial simplex size factor
 * @param config  Optimizer configuration
 */
SCI_OptResult sci_minimize_nelder_mead(SCI_ObjFn f, double *x0, int n,
                                        double simplex_scale,
                                        const SCI_OptConfig *config);

/**
 * Minimize using Conjugate Gradient (Fletcher-Reeves variant).
 *
 * p_0 = -grad_0
 * beta_k = (grad_k^T grad_k) / (grad_{k-1}^T grad_{k-1})
 * p_k = -grad_k + beta_k * p_{k-1}
 *
 * Fletcher & Reeves (1964): converges in at most n steps for
 * quadratic functions with exact line search.
 * Linear convergence in practice with inexact line search.
 */
SCI_OptResult sci_minimize_conjugate_gradient(SCI_ObjFn f, SCI_GradFn grad_f,
                                               double *x0, int n,
                                               const SCI_OptConfig *config);

/* -----------------------------------------------------------------
 * L5: Global & Stochastic Optimization
 * ----------------------------------------------------------------- */

/**
 * Genetic Algorithm for global optimization (Holland 1975).
 *
 * Population-based evolutionary algorithm:
 * 1. Initialize random population
 * 2. Evaluate fitness (negative objective)
 * 3. Selection (tournament selection, size 2)
 * 4. Crossover (uniform crossover)
 * 5. Mutation (Gaussian perturbation)
 * 6. Repeat for max_iter generations
 *
 * No gradient required. Handles multimodal, non-convex problems.
 * Computational cost: O(pop_size * max_iter * n).
 */
SCI_OptResult sci_minimize_genetic(SCI_ObjFn f, double *x0, int n,
                                    const double *lower_bounds,
                                    const double *upper_bounds,
                                    const SCI_OptConfig *config);

/**
 * Simulated Annealing (Kirkpatrick, Gelatt & Vecchi 1983).
 *
 * Metropolis criterion: accept worse solution with probability
 *   P = exp(-(f_new - f_cur) / T)
 * Temperature decreases: T_{k+1} = cooling_rate * T_k
 *
 * Converges to global optimum if cooling schedule is slow enough
 * (logarithmic). In practice, exponential cooling works well.
 *
 * @param f       Objective function
 * @param x0      Initial guess (size n, modified for best solution)
 * @param n       Dimension
 * @param config  Configuration (uses initial_temp, cooling_rate)
 */
SCI_OptResult sci_minimize_simulated_annealing(SCI_ObjFn f, double *x0,
                                                int n,
                                                const SCI_OptConfig *config);

/* -----------------------------------------------------------------
 * L5: Constrained Optimization
 * ----------------------------------------------------------------- */

/**
 * Linear Programming via Simplex Method (Dantzig 1947).
 *
 * Standard form: min c^T x  s.t. A x = b, x >= 0
 *
 * Implements the two-phase simplex method:
 * Phase 1: Find initial basic feasible solution
 * Phase 2: Pivot to optimality using reduced costs
 *
 * Bland's rule for anti-cycling.
 *
 * Complexity: Exponential worst-case (Klee-Minty), polynomial in practice.
 *
 * @param c        Cost vector (size n)
 * @param A        Constraint matrix (size m x n, row-major)
 * @param b        RHS vector (size m)
 * @param m        Number of equality constraints
 * @param n        Number of variables
 * @param x_out    Output: optimal solution (size n, caller allocated)
 * @param obj_out  Output: optimal objective value
 * @return         true if optimal solution found
 */
bool sci_linprog_simplex(const double *c, const double *A, const double *b,
                          int m, int n, double *x_out, double *obj_out);

/**
 * Penalty method for constrained optimization.
 * Converts constrained problem to unconstrained via:
 *   min f(x) + mu * sum(max(0, g_i(x))^2)  [inequality]
 *          + mu * sum(h_j(x)^2)             [equality]
 * where mu is the penalty parameter (increased in outer iterations).
 *
 * @param f          Objective function
 * @param grad_f     Gradient of objective
 * @param x0         Initial guess (size n, output)
 * @param n          Dimension
 * @param constraints Constraint array
 * @param num_cons   Number of constraints
 * @param config     Optimizer configuration
 */
SCI_OptResult sci_minimize_penalty(SCI_ObjFn f, SCI_GradFn grad_f,
                                    double *x0, int n,
                                    const SCI_Constraint *constraints,
                                    int num_cons,
                                    const SCI_OptConfig *config);

/* -----------------------------------------------------------------
 * L7-L8: Utilities
 * ----------------------------------------------------------------- */

/**
 * Multi-start optimization: restart from random initial points
 * and return the best result found. Helps escape local minima.
 *
 * @param f        Objective function
 * @param n        Dimension
 * @param bounds_l Lower bounds
 * @param bounds_u Upper bounds
 * @param n_starts Number of random restarts
 * @param seed     Random seed
 * @param config   Base optimizer config
 * @return         Best result across all starts (must free with sci_opt_result_free)
 */
SCI_OptResult sci_opt_multistart(SCI_ObjFn f, SCI_GradFn grad_f, int n,
                                  const double *bounds_l,
                                  const double *bounds_u,
                                  int n_starts, unsigned int seed,
                                  const SCI_OptConfig *config);

/**
 * Numerical gradient approximation using central differences.
 * grad_i = (f(x + h e_i) - f(x - h e_i)) / (2h)
 * Error: O(h^2). Useful when analytic gradient is unavailable.
 *
 * @param f     Objective function
 * @param x     Point to evaluate
 * @param n     Dimension
 * @param h     Step size (1e-6 default)
 * @param grad  Output gradient (caller-allocated, size n)
 */
void sci_numerical_gradient(SCI_ObjFn f, const double *x, int n,
                             double h, double *grad);

#endif /* MINI_SCI_OPTIMIZE_H */