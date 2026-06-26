#ifndef ENERGY_OPTIMIZE_H
#define ENERGY_OPTIMIZE_H

#include "energy_core.h"
#include "energy_solar.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Optimization Problem Types (L5: Algorithms) ──────────────────────── */

/* Linear Programming problem (standard form: min c'x s.t. Ax ≤ b, x ≥ 0) */
#define ENERGY_LP_MAX_VARS      256
#define ENERGY_LP_MAX_CONSTRAINTS 512

typedef struct {
    int     num_vars;
    int     num_constraints;
    float   c[ENERGY_LP_MAX_VARS];          /* objective coefficients */
    float   A[ENERGY_LP_MAX_CONSTRAINTS * ENERGY_LP_MAX_VARS]; /* constraint matrix */
    float   b[ENERGY_LP_MAX_CONSTRAINTS];    /* RHS */
    float   x[ENERGY_LP_MAX_VARS];           /* solution */
    float   obj_value;
    int     iterations;
    bool    is_optimal;
    bool    is_feasible;
    bool    is_unbounded;
} energy_lp_problem_t;

/* Microgrid sizing optimization */
typedef struct {
    float  pv_capacity_kw;       /* decision variables */
    float  wind_capacity_kw;
    float  battery_capacity_kwh;
    float  battery_power_kw;
    float  diesel_capacity_kw;
    float  lcoe;                 /* resulting LCOE */
    float  renewable_fraction;
    float  loss_of_load_pct;
    float  total_cost;
} energy_microgrid_sizing_t;

/* Unit commitment problem */
typedef struct {
    int    num_units;
    int    num_hours;
    float  load_kw[ENERGY_MAX_TIMESERIES];
    int    commitment[ENERGY_MAX_DEVICES * ENERGY_MAX_TIMESERIES]; /* unit x hour */
    float  dispatch_kw[ENERGY_MAX_DEVICES * ENERGY_MAX_TIMESERIES];
    float  total_cost;
    float  spinning_reserve_kw[ENERGY_MAX_TIMESERIES];
} energy_unit_commitment_t;

/* Economic dispatch (L5: Lambda Iteration) */
typedef struct {
    int    num_units;
    float  load_to_meet_kw;
    float  dispatch_kw[ENERGY_MAX_DEVICES];
    float  system_lambda;        /* incremental cost */
    float  total_cost_per_h;
    bool   converged;
} energy_economic_dispatch_t;

/* Particle Swarm Optimization parameters */
typedef struct {
    int    swarm_size;
    int    max_iterations;
    float  inertia_weight;       /* w */
    float  cognitive_coeff;      /* c1 */
    float  social_coeff;         /* c2 */
    float  min_velocity;
    float  max_velocity;
    float  convergence_tolerance;
} energy_pso_params_t;

/* Monte Carlo simulation */
typedef struct {
    int    num_scenarios;
    float  solar_capacity_kw;
    float  wind_capacity_kw;
    float  solar_std_dev;        /* uncertainty in solar output */
    float  wind_std_dev;         /* uncertainty in wind output */
    float  load_std_dev;         /* uncertainty in load */
    float  scenarios_p50[ENERGY_MAX_TIMESERIES];
    float  scenarios_p90[ENERGY_MAX_TIMESERIES];
    float  scenarios_p99[ENERGY_MAX_TIMESERIES];
    float  expected_lolp;        /* loss of load probability */
} energy_monte_carlo_t;

/* Multi-objective optimization Pareto front */
#define ENERGY_PARETO_MAX_POINTS 256

typedef struct {
    float  objective1;           /* e.g., cost */
    float  objective2;           /* e.g., emissions */
    float  x[ENERGY_LP_MAX_VARS];
    int    num_vars;
} energy_pareto_point_t;

typedef struct {
    energy_pareto_point_t points[ENERGY_PARETO_MAX_POINTS];
    int   num_points;
    float hypervolume;            /* dominated hypervolume */
} energy_pareto_front_t;

/* ── API: Linear Programming (L5: Simplex Method) ────────────────────── */

/* Solve LP using revised simplex */
energy_lp_problem_t energy_lp_solve(const energy_lp_problem_t* lp);

/* Battery dispatch DP (L5: Bellman Equation) */
void  energy_battery_dispatch_dp(const float* prices, int n_slots,
                                  float capacity_kwh, float max_power_kw,
                                  float efficiency, float* schedule);

/* ── API: Microgrid Sizing (L5: Heuristic + PSO) ──────────────────────── */

energy_microgrid_sizing_t energy_microgrid_size_pso(
    const energy_timeslot_t* load, int n_hours,
    const energy_solar_geometry_t* solar_geo,
    const float* wind_speeds,
    const energy_pso_params_t* pso);

float energy_microgrid_lcoe(const energy_microgrid_sizing_t* sizing,
                             const energy_timeslot_t* load, int n);

/* ── API: Unit Commitment (L5: Priority List) ─────────────────────────── */

void  energy_unit_commitment_priority_list(energy_unit_commitment_t* uc,
                                            const energy_generator_t* units,
                                            int num_units,
                                            const float* load, int n_hours);

/* ── API: Economic Dispatch (L5: Lambda Iteration) ───────────────────── */

energy_economic_dispatch_t energy_economic_dispatch(
    const energy_generator_t* units, int num_units, float load_kw);

void energy_economic_dispatch_with_losses(
    energy_economic_dispatch_t* disp, const float* loss_coeffs);

/* ── API: PSO (L5: Metaheuristic) ────────────────────────────────────── */

/* Generic PSO for minimizing f(x) with box constraints */
float energy_pso_minimize(
    float (*objective)(const float*, int, void*),
    void* context,
    int dimensions,
    const float* lower_bounds,
    const float* upper_bounds,
    const energy_pso_params_t* params,
    float* best_position,
    float* best_value);

/* ── API: Monte Carlo (L5: Stochastic Methods) ───────────────────────── */

void  energy_monte_carlo_reliability(
    energy_monte_carlo_t* mc,
    const energy_timeslot_t* base_load, int n_slots,
    int num_iterations);

float energy_monte_carlo_lolp(const energy_monte_carlo_t* mc);
float energy_monte_carlo_eue(const energy_monte_carlo_t* mc); /* expected unserved energy */

/* ── API: Multi-Objective (L5: Pareto Optimization) ──────────────────── */

void  energy_pareto_add_point(energy_pareto_front_t* front,
                               float obj1, float obj2,
                               const float* x, int n);
bool  energy_pareto_dominates(float obj1_a, float obj2_a,
                               float obj1_b, float obj2_b);
void  energy_pareto_sort(energy_pareto_front_t* front);
float energy_pareto_hypervolume(const energy_pareto_front_t* front,
                                 float ref1, float ref2);

/* ── Simple genetic algorithm for energy dispatch (L5) ───────────────── */

typedef struct {
    int    population_size;
    int    generations;
    float  mutation_rate;
    float  crossover_rate;
    float  elitism_fraction;
} energy_ga_params_t;

void energy_ga_dispatch(const energy_generator_t* units, int n_units,
                         const float* load, int n_hours,
                         const energy_ga_params_t* params,
                         float* best_dispatch);

#ifdef __cplusplus
}
#endif

#endif
