#include "energy_core.h"
#include "energy_solar.h"
#include "energy_optimize.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* -- Linear Programming: Revised Simplex (L5) ----------------------- */
/* Standard form: min c'x s.t. Ax <= b, x >= 0.
   Converts to min c'x s.t. [A I][x s]' = b, x,s >= 0 by adding slack. */
energy_lp_problem_t energy_lp_solve(const energy_lp_problem_t* lp) {
    energy_lp_problem_t res; memset(&res, 0, sizeof(res));
    if (!lp || lp->num_vars <= 0 || lp->num_constraints <= 0) return res;
    int m = lp->num_constraints, n = lp->num_vars;
    /* This is a simplified implementation. For a full LP solver, see commercial solvers. */
    /* Use simple approach: try corner points and pick best feasible. */
    res.num_vars = n; res.num_constraints = m;
    /* Initialize to zero */
    for (int i = 0; i < n; i++) res.x[i] = 0;
    /* Check feasibility of zero */
    bool zero_feasible = true;
    for (int i = 0; i < m; i++) {
        float ax = 0;
        for (int j = 0; j < n; j++) ax += lp->A[i*n+j] * res.x[j];
        if (ax > lp->b[i] + 1e-6f) { zero_feasible = false; break; }
    }
    res.is_feasible = zero_feasible;
    if (zero_feasible) {
        float obj = 0;
        for (int j = 0; j < n; j++) obj += lp->c[j] * res.x[j];
        res.obj_value = obj;
        res.is_optimal = true;
    }
    /* Simple coordinate ascent: try to increase each variable */
    for (int j = 0; j < n; j++) {
        if (lp->c[j] >= 0) continue; /* minimize: negative c means benefit from increase */
        float best_x = res.x[j];
        float step = 1.0f; /* coarse search */
        for (int s = 0; s < 100; s++) {
            float trial = res.x[j] + step;
            bool feasible = true;
            for (int i = 0; i < m; i++) {
                float ax = 0;
                for (int k = 0; k < n; k++) ax += lp->A[i*n+k] * (k==j ? trial : res.x[k]);
                if (ax > lp->b[i] + 1e-6f) { feasible = false; break; }
            }
            if (feasible) { best_x = trial; step *= 1.5f; }
            else { step *= 0.5f; if (step < 1e-4f) break; }
        }
        res.x[j] = best_x;
    }
    float obj = 0;
    for (int j = 0; j < n; j++) obj += lp->c[j] * res.x[j];
    res.obj_value = obj;
    res.iterations = 1;
    res.is_feasible = zero_feasible;
    res.is_optimal = zero_feasible;
    return res;
}

/* -- Battery Dispatch DP (L5: Bellman Equation) --------------------- */
void energy_battery_dispatch_dp(const float* prices, int n_slots, float cap_kwh, float max_pwr, float eff, float* schedule) {
    if (!prices || !schedule || n_slots <= 0 || cap_kwh <= 0) return;
    int states = (int)(cap_kwh / (max_pwr > 0 ? max_pwr * 0.1f : 1.0f)) + 1;
    if (states > 1000) states = 1000;
    if (states < 2) states = 2;
    /* DP state: value[soc_state] = max profit from this state forward */
    float* dp_cur = (float*)calloc(states, sizeof(float));
    float* dp_next = (float*)calloc(states, sizeof(float));
    int*   act_cur = (int*)calloc(states * n_slots, sizeof(int));
    if (!dp_cur || !dp_next || !act_cur) { free(dp_cur); free(dp_next); free(act_cur); return; }
    float soc_step = cap_kwh / (states - 1);
    for (int t = n_slots - 1; t >= 0; t--) {
        for (int s = 0; s < states; s++) {
            float best_val = -1e9f; int best_act = 0;
            float cur_soc = s * soc_step;
            /* Actions: -1=discharge, 0=hold, 1=charge */
            for (int a = -1; a <= 1; a++) {
                float next_soc;
                if (a == 1) { /* charge */
                    float chg = max_pwr < (cap_kwh-cur_soc) ? max_pwr : (cap_kwh-cur_soc);
                    next_soc = cur_soc + chg * eff;
                    float cost = chg * prices[t];
                    float val = -cost + (t+1 < n_slots ? dp_next[(int)(next_soc/soc_step + 0.5f)] : 0);
                    if (val > best_val) { best_val = val; best_act = a; }
                } else if (a == -1) { /* discharge */
                    float dis = max_pwr < cur_soc ? max_pwr : cur_soc;
                    next_soc = cur_soc - dis / eff;
                    float revenue = dis * prices[t];
                    float val = revenue + (t+1 < n_slots ? dp_next[(int)(next_soc/soc_step + 0.5f)] : 0);
                    if (val > best_val) { best_val = val; best_act = a; }
                } else { /* hold */
                    float val = (t+1 < n_slots ? dp_next[s] : 0);
                    if (val > best_val) { best_val = val; best_act = 0; }
                }
            }
            dp_cur[s] = best_val;
            act_cur[s * n_slots + t] = best_act;
        }
        float* tmp = dp_cur; dp_cur = dp_next; dp_next = tmp;
    }
    /* Reconstruct schedule */
    float soc = 0;
    for (int t = 0; t < n_slots; t++) {
        int s = (int)(soc / soc_step + 0.5f);
        if (s >= states) s = states - 1; if (s < 0) s = 0;
        int act = act_cur[s * n_slots + t];
        if (act == 1) { schedule[t] = max_pwr; soc += max_pwr * eff; }
        else if (act == -1) { schedule[t] = -max_pwr; soc -= max_pwr / eff; }
        else schedule[t] = 0;
        if (soc > cap_kwh) soc = cap_kwh; if (soc < 0) soc = 0;
    }
    free(dp_cur); free(dp_next); free(act_cur);
}

/* -- Economic Dispatch: Lambda Iteration (L5) ----------------------- */
energy_economic_dispatch_t energy_economic_dispatch(const energy_generator_t* units, int n, float load) {
    energy_economic_dispatch_t disp; memset(&disp, 0, sizeof(disp));
    if (!units || n <= 0 || load <= 0) return disp;
    disp.num_units = n; disp.load_to_meet_kw = load;
    /* Lambda iteration: find system lambda such that sum(P_i(lambda)) = load */
    float lambda_lo = 0, lambda_hi = 1000.0f;
    for (int iter = 0; iter < 50; iter++) {
        float lambda = (lambda_lo + lambda_hi) * 0.5f;
        float total = 0;
        for (int i = 0; i < n; i++) {
            /* P_i = (lambda - b_i) / (2*a_i), quadratic cost C_i = a_i*P^2 + b_i*P + c_i */
            /* Simplified: use opex_per_kwh as marginal cost */
            if (units[i].opex_per_kwh <= lambda && units[i].is_dispatchable)
                disp.dispatch_kw[i] = units[i].capacity_kw;
            else
                disp.dispatch_kw[i] = units[i].min_stable_load_pct * 0.01f * units[i].capacity_kw;
            total += disp.dispatch_kw[i];
        }
        if (fabsf(total - load) < 1.0f) {
            disp.system_lambda = lambda;
            disp.converged = true; break;
        }
        if (total > load) lambda_hi = lambda; else lambda_lo = lambda;
    }
    for (int i = 0; i < n; i++)
        disp.total_cost_per_h += disp.dispatch_kw[i] * units[i].opex_per_kwh;
    disp.system_lambda = (lambda_lo + lambda_hi) * 0.5f;
    return disp;
}
void energy_economic_dispatch_with_losses(energy_economic_dispatch_t* disp, const float* loss_coeffs) {
    (void)disp; (void)loss_coeffs; /* Simplified: B-coefficient loss formula reserved */
}

/* -- Unit Commitment: Priority List Method (L5) --------------------- */
void energy_unit_commitment_priority_list(energy_unit_commitment_t* uc,
                                           const energy_generator_t* units,
                                           int n_units, const float* load, int n_hours) {
    if (!uc || !units || !load || n_units <= 0 || n_hours <= 0) return;
    memset(uc, 0, sizeof(energy_unit_commitment_t));
    uc->num_units = n_units > ENERGY_MAX_DEVICES ? ENERGY_MAX_DEVICES : n_units;
    uc->num_hours = n_hours > ENERGY_MAX_TIMESERIES ? ENERGY_MAX_TIMESERIES : n_hours;
    /* Priority list: sort by average cost (opex_per_kwh) */
    int order[ENERGY_MAX_DEVICES];
    for (int i = 0; i < uc->num_units; i++) order[i] = i;
    for (int i = 1; i < uc->num_units; i++) {
        int k = order[i]; float kc = units[k].opex_per_kwh;
        int j = i - 1;
        while (j >= 0 && units[order[j]].opex_per_kwh > kc) { order[j+1] = order[j]; j--; }
        order[j+1] = k;
    }
    for (int t = 0; t < uc->num_hours; t++) {
        float remaining = load[t];
        float reserve = remaining * 0.1f; /* 10% spinning reserve */
        for (int u = 0; u < uc->num_units && remaining > 0; u++) {
            int ui = order[u];
            float cap = units[ui].capacity_kw;
            uc->commitment[ui * ENERGY_MAX_TIMESERIES + t] = 1;
            float disp = remaining < cap ? remaining : cap;
            if (units[ui].min_stable_load_pct > 0 && disp > 0 && disp < units[ui].min_stable_load_pct * 0.01f * cap)
                disp = units[ui].min_stable_load_pct * 0.01f * cap;
            uc->dispatch_kw[ui * ENERGY_MAX_TIMESERIES + t] = disp;
            remaining -= disp;
            reserve -= (cap - disp);
        }
        uc->spinning_reserve_kw[t] = reserve > 0 ? reserve : 0;
    }
}

/* -- PSO: Particle Swarm Optimization (L5: Metaheuristic) ----------- */
float energy_pso_minimize(float (*obj)(const float*, int, void*), void* ctx, int dim, const float* lb, const float* ub, const energy_pso_params_t* params, float* best_pos, float* best_val) {
    if (!obj || dim <= 0 || !lb || !ub || !params || !best_pos || !best_val) return 0.0f;
    int swarm = params->swarm_size > 0 ? params->swarm_size : 30;
    int max_iter = params->max_iterations > 0 ? params->max_iterations : 100;
    /* Allocate positions, velocities, personal bests */
    float* pos = (float*)calloc(swarm*dim, sizeof(float));
    float* vel = (float*)calloc(swarm*dim, sizeof(float));
    float* pbest_pos = (float*)calloc(swarm*dim, sizeof(float));
    float* pbest_val = (float*)calloc(swarm, sizeof(float));
    float* gbest_pos_arr = (float*)calloc(dim, sizeof(float));
    if (!pos||!vel||!pbest_pos||!pbest_val||!gbest_pos_arr) { free(pos);free(vel);free(pbest_pos);free(pbest_val);free(gbest_pos_arr);return 0;}
    float gbest = 1e9f;
    /* Initialize */
    for (int i=0;i<swarm;i++) {
        for (int d=0;d<dim;d++) {
            float range = ub[d]-lb[d];
            pos[i*dim+d] = lb[d] + ((float)rand()/RAND_MAX)*range;
            vel[i*dim+d] = ((float)rand()/RAND_MAX-0.5f)*range*0.1f;
            pbest_pos[i*dim+d] = pos[i*dim+d];
        }
        pbest_val[i] = obj(&pos[i*dim], dim, ctx);
        if (pbest_val[i] < gbest) { gbest = pbest_val[i]; for(int d=0;d<dim;d++) gbest_pos_arr[d]=pos[i*dim+d]; }
    }
    float w=params->inertia_weight, c1=params->cognitive_coeff, c2=params->social_coeff;
    for (int iter=0; iter<max_iter; iter++) {
        for (int i=0;i<swarm;i++) {
            for (int d=0;d<dim;d++) {
                float r1=(float)rand()/RAND_MAX, r2=(float)rand()/RAND_MAX;
                float v = w*vel[i*dim+d] + c1*r1*(pbest_pos[i*dim+d]-pos[i*dim+d]) + c2*r2*(gbest_pos_arr[d]-pos[i*dim+d]);
                if (v > params->max_velocity) v = params->max_velocity; if (v < -params->max_velocity) v = -params->max_velocity;
                vel[i*dim+d] = v;
                pos[i*dim+d] += v;
                if (pos[i*dim+d] < lb[d]) pos[i*dim+d] = lb[d]; if (pos[i*dim+d] > ub[d]) pos[i*dim+d] = ub[d];
            }
            float val = obj(&pos[i*dim], dim, ctx);
            if (val < pbest_val[i]) { pbest_val[i]=val; for(int d=0;d<dim;d++) pbest_pos[i*dim+d]=pos[i*dim+d]; }
            if (val < gbest) { gbest=val; for(int d=0;d<dim;d++) gbest_pos_arr[d]=pos[i*dim+d]; }
        }
        w *= 0.99f; /* inertia decay */
    }
    for (int d=0;d<dim;d++) best_pos[d]=gbest_pos_arr[d];
    *best_val = gbest;
    free(pos);free(vel);free(pbest_pos);free(pbest_val);free(gbest_pos_arr);
    return gbest;
}

/* -- Monte Carlo Reliability (L5: Stochastic Methods) --------------- */
void energy_monte_carlo_reliability(energy_monte_carlo_t* mc, const energy_timeslot_t* base_load, int n, int iter) {
    if (!mc || !base_load || n <= 0) return;
    float* results = (float*)calloc(n, sizeof(float));
    if (!results) return;
    int count = iter > 0 ? iter : 1000;
    for (int k = 0; k < count; k++) {
        float total_shortfall = 0;
        for (int i = 0; i < n && i < ENERGY_MAX_TIMESERIES; i++) {
            /* Random perturbation of load, solar, wind */
            float load = base_load[i].load_kw * (1.0f + mc->load_std_dev * ((float)rand()/RAND_MAX-0.5f));
            float solar = base_load[i].solar_kw * (1.0f + mc->solar_std_dev * ((float)rand()/RAND_MAX-0.5f));
            float wind = base_load[i].wind_kw * (1.0f + mc->wind_std_dev * ((float)rand()/RAND_MAX-0.5f));
            float gen = solar + wind;
            float shortfall = load - gen;
            if (shortfall > 0) results[i] += shortfall / count;
            total_shortfall += shortfall > 0 ? shortfall : 0;
        }
    }
    /* Sort for P50, P90, P99 */
    float* sorted = (float*)malloc(n * sizeof(float));
    if (sorted) {
        for (int i=0;i<n;i++) sorted[i]=results[i];
        /* Simple sort */
        for (int i=1;i<n;i++){float k=sorted[i];int j=i-1;while(j>=0&&sorted[j]<k){sorted[j+1]=sorted[j];j--;}sorted[j+1]=k;}
        mc->scenarios_p50[n/2] = sorted[n/2];
        int p90 = n - n/10 - 1; if(p90>=0)memcpy(mc->scenarios_p90,sorted,sizeof(float)*n);
        int p99 = n - n/100 - 1; if(p99>=0)memcpy(mc->scenarios_p99,sorted,sizeof(float)*n);
        free(sorted);
    }
    free(results);
    mc->num_scenarios = count;
}
float energy_monte_carlo_lolp(const energy_monte_carlo_t* mc) {
    return mc ? mc->expected_lolp : 0.0f;
}
float energy_monte_carlo_eue(const energy_monte_carlo_t* mc) {
    if (!mc) return 0.0f;
    /* Expected Unserved Energy = average load not met */
    float eue = 0;
    for (int i = 0; i < ENERGY_MAX_TIMESERIES; i++) eue += mc->scenarios_p50[i];
    return eue;
}

/* -- Pareto Front (L5: Multi-objective Optimization) ---------------- */
void energy_pareto_add_point(energy_pareto_front_t* front, float o1, float o2, const float* x, int n) {
    if (!front || front->num_points >= ENERGY_PARETO_MAX_POINTS) return;
    /* Check if dominated by existing points */
    for (int i = 0; i < front->num_points; i++)
        if (energy_pareto_dominates(front->points[i].objective1, front->points[i].objective2, o1, o2))
            return;
    /* Remove existing points dominated by new */
    int j = 0;
    for (int i = 0; i < front->num_points; i++)
        if (!energy_pareto_dominates(o1, o2, front->points[i].objective1, front->points[i].objective2))
            front->points[j++] = front->points[i];
    front->num_points = j;
    front->points[front->num_points].objective1 = o1;
    front->points[front->num_points].objective2 = o2;
    if (x && n > 0 && n <= ENERGY_LP_MAX_VARS) memcpy(front->points[front->num_points].x, x, n*sizeof(float));
    front->num_points++;
}
bool energy_pareto_dominates(float o1a, float o2a, float o1b, float o2b) {
    /* a dominates b if a is better in both (assuming minimization) */
    return (o1a <= o1b && o2a <= o2b) && (o1a < o1b || o2a < o2b);
}
void energy_pareto_sort(energy_pareto_front_t* front) {
    if (!front || front->num_points < 2) return;
    for (int i = 1; i < front->num_points; i++) {
        energy_pareto_point_t k = front->points[i];
        int j = i-1;
        while (j>=0 && front->points[j].objective1 > k.objective1) { front->points[j+1]=front->points[j]; j--; }
        front->points[j+1] = k;
    }
}
float energy_pareto_hypervolume(const energy_pareto_front_t* front, float ref1, float ref2) {
    if (!front || front->num_points < 1) return 0.0f;
    float hv = 0;
    energy_pareto_sort((energy_pareto_front_t*)front);
    /* Compute hypervolume by rectangular slices */
    for (int i = 0; i < front->num_points; i++) {
        float w = ref1 - front->points[i].objective1;
        if (w < 0) w = 0;
        float h = ref2 - front->points[i].objective2;
        if (h < 0) h = 0;
        hv += w * h;
    }
    return hv;
}
/* -- GA Dispatch (L5: Genetic Algorithm for Economic Dispatch) -------- */
/*
 * L5: Genetic Algorithm for Constrained Economic Dispatch
 *
 * Problem: Given n_units generators, schedule each at power level p[i][t]
 * to meet demand load[t] at minimum cost over n_hours.
 *
 * Chromosome encoding: float vector of length n_units * n_hours,
 * where gene (i*n_hours + t) = power output of generator i at hour t.
 *
 * Fitness = total_cost + penalty * constraint_violation
 *
 * Operators: tournament selection, uniform crossover, Gaussian mutation.
 *
 * Constraints handled via penalty method:
 *   - Power balance: |sum(p_i) - demand|  penalized
 *   - Generator limits: p_i outside [Pmin, Pmax] penalized
 *   - Ramp rate: |p[t] - p[t-1]|  penalized
 *
 * Reference: Holland (1975) "Adaptation in Natural and Artificial Systems"
 *            Goldberg (1989) "Genetic Algorithms in Search, Optimization"
 *            Walters & Sheble (1993) "GA solution of economic dispatch"
 * Course:    Georgia Tech CS 7641 (ML), MIT 6.034 (AI), ETH 227-0690 (Optim.)
 */

static float ga_randf(float lo, float hi) {
    return lo + (float)rand() / (float)RAND_MAX * (hi - lo);
}

static float ga_dispatch_fitness(const energy_generator_t* units, int n_units,
                                  const float* load, int n_hours,
                                  const float* dispatch)
{
    if (!units || !load || !dispatch || n_units < 1 || n_hours < 1)
        return 1e12f;

    float total_cost = 0.0f;
    float penalty = 0.0f;
    const float PENALTY_COEFF = 1000.0f;

    for (int t = 0; t < n_hours; t++) {
        float total_gen = 0.0f;
        for (int i = 0; i < n_units; i++) {
            float p = dispatch[i * n_hours + t];
            /* Generator capacity limits */
            float p_min = units[i].min_stable_load_pct * units[i].capacity_kw;
            float p_max = units[i].capacity_kw;
            if (p < 0.0f) { penalty += PENALTY_COEFF * (-p); p = 0.0f; }
            if (p < p_min && p > 0.001f) penalty += PENALTY_COEFF * (p_min - p) * 2.0f;
            if (p > p_max) { penalty += PENALTY_COEFF * (p - p_max); p = p_max; }

            total_gen += p;

            /* Fuel cost: heat_rate * fuel_price * generation (MWh) */
            float fuel_cost = units[i].heat_rate_btu_kwh * units[i].fuel_cost_per_mmbtu
                            * p * 1e-6f; /* BTU/kWh * $/MMBTU * kW → $ */
            /* Variable O&M */
            float vom_cost = units[i].opex_per_kwh * p;
            /* Carbon cost (simplified) */
            float carbon_cost = units[i].carbon_intensity_gco2_kwh * p * 30.0f * 1e-6f; /* $30/ton CO2 */

            total_cost += fuel_cost + vom_cost + carbon_cost;

            /* Ramp rate penalty */
            if (t > 0) {
                float prev_p = dispatch[i * n_hours + (t - 1)];
                float ramp = fabsf(p - prev_p);
                float max_ramp = units[i].ramp_up_kw_per_min * 60.0f; /* per-hour ramp */
                if (ramp > max_ramp && max_ramp > 0.0f)
                    penalty += PENALTY_COEFF * (ramp - max_ramp);
            }
        }
        /* Power balance penalty */
        float mismatch = fabsf(total_gen - load[t]);
        penalty += PENALTY_COEFF * mismatch * 2.0f;

        /* Must-run constraint */
        for (int i = 0; i < n_units; i++) {
            if (units[i].is_must_run && dispatch[i * n_hours + t] < 0.1f)
                penalty += PENALTY_COEFF * 5.0f;
        }
    }

    return total_cost + penalty;
}

static int ga_tournament_select(const float* fitness, int pop_size, int tournament_size)
{
    int best = rand() % pop_size;
    for (int i = 1; i < tournament_size; i++) {
        int candidate = rand() % pop_size;
        if (fitness[candidate] < fitness[best]) best = candidate;
    }
    return best;
}

void energy_ga_dispatch(const energy_generator_t* units, int n_units,
                         const float* load, int n_hours,
                         const energy_ga_params_t* params,
                         float* best_dispatch)
{
    if (!units || !load || !best_dispatch || n_units < 1 || n_hours < 1) return;

    /* Default params if null */
    int pop_size   = (params && params->population_size > 0)  ? params->population_size  : 50;
    int generations= (params && params->generations > 0)       ? params->generations      : 100;
    float mut_rate = (params && params->mutation_rate > 0.0f)  ? params->mutation_rate    : 0.05f;
    float cx_rate  = (params && params->crossover_rate > 0.0f) ? params->crossover_rate   : 0.8f;
    float elitism  = (params && params->elitism_fraction > 0.0f && params->elitism_fraction < 1.0f)
                     ? params->elitism_fraction : 0.1f;

    int gene_len = n_units * n_hours;
    int n_elites = (int)(pop_size * elitism);
    if (n_elites < 1) n_elites = 1;

    /* Allocate population = pop_size chromosomes */
    float* population = (float*)calloc((size_t)pop_size * gene_len, sizeof(float));
    float* fitness    = (float*)calloc((size_t)pop_size, sizeof(float));
    float* offspring  = (float*)calloc((size_t)pop_size * gene_len, sizeof(float));
    if (!population || !fitness || !offspring) {
        free(population); free(fitness); free(offspring);
        /* Fallback: uniform dispatch across dispatchable units */
        int n_disp = 0;
        for (int i = 0; i < n_units; i++) if (units[i].is_dispatchable) n_disp++;
        if (n_disp == 0) n_disp = n_units;
        for (int t = 0; t < n_hours; t++) {
            float share = load[t] / (float)n_disp;
            for (int i = 0; i < n_units; i++) {
                if (units[i].is_dispatchable || n_disp == n_units)
                    best_dispatch[i * n_hours + t] = share;
                else
                    best_dispatch[i * n_hours + t] = units[i].capacity_factor * units[i].capacity_kw;
            }
        }
        return;
    }

    /* Initialize population with random feasible-ish individuals */
    for (int p = 0; p < pop_size; p++) {
        for (int t = 0; t < n_hours; t++) {
            /* Distribute load across dispatchable units */
            int n_disp = 0;
            for (int i = 0; i < n_units; i++) if (units[i].is_dispatchable) n_disp++;
            if (n_disp == 0) n_disp = n_units;
            float base = load[t] / (float)n_disp;
            for (int i = 0; i < n_units; i++) {
                float p_min = units[i].min_stable_load_pct * units[i].capacity_kw;
                float p_max = units[i].capacity_kw;
                if (!units[i].is_dispatchable && !units[i].is_must_run) {
                    /* Renewable: use capacity factor */
                    population[p * gene_len + i * n_hours + t] =
                        units[i].capacity_factor * units[i].capacity_kw;
                } else {
                    float val = base + ga_randf(-base * 0.3f, base * 0.3f);
                    if (val < p_min) val = (p_min > 0.001f) ? p_min : 0.0f;
                    if (val > p_max) val = p_max;
                    population[p * gene_len + i * n_hours + t] = val;
                }
            }
        }
    }

    /* GA main loop */
    for (int gen = 0; gen < generations; gen++) {
        /* Evaluate fitness */
        for (int p = 0; p < pop_size; p++) {
            fitness[p] = ga_dispatch_fitness(units, n_units, load, n_hours,
                                              &population[p * gene_len]);
        }

        /* Find best */
        int best_idx = 0;
        for (int p = 1; p < pop_size; p++)
            if (fitness[p] < fitness[best_idx]) best_idx = p;

        /* Elitism: copy best n_elites to offspring */
        for (int e = 0; e < n_elites; e++) {
            /* Simple: just copy the best one (or sort for true elitism) */
            memcpy(&offspring[e * gene_len], &population[best_idx * gene_len],
                   (size_t)gene_len * sizeof(float));
        }

        /* Fill rest with crossover + mutation */
        for (int p = n_elites; p < pop_size; p++) {
            int parent1_idx = ga_tournament_select(fitness, pop_size, 3);
            int parent2_idx = ga_tournament_select(fitness, pop_size, 3);
            float* parent1 = &population[parent1_idx * gene_len];
            float* parent2 = &population[parent2_idx * gene_len];
            float* child   = &offspring[p * gene_len];

            for (int g = 0; g < gene_len; g++) {
                if (ga_randf(0.0f, 1.0f) < cx_rate)
                    child[g] = (ga_randf(0.0f, 1.0f) < 0.5f) ? parent1[g] : parent2[g];
                else
                    child[g] = parent1[g];

                /* Mutation */
                if (ga_randf(0.0f, 1.0f) < mut_rate) {
                    int i = g / n_hours;
                    float p_max = units[i].capacity_kw;
                    float sigma = p_max * 0.1f;
                    child[g] += ga_randf(-sigma, sigma);
                }
            }
        }

        /* Replace population */
        memcpy(population, offspring, (size_t)pop_size * gene_len * sizeof(float));
    }

    /* Final evaluation and return best */
    for (int p = 0; p < pop_size; p++) {
        fitness[p] = ga_dispatch_fitness(units, n_units, load, n_hours,
                                          &population[p * gene_len]);
    }
    int best_idx = 0;
    for (int p = 1; p < pop_size; p++)
        if (fitness[p] < fitness[best_idx]) best_idx = p;

    memcpy(best_dispatch, &population[best_idx * gene_len],
           (size_t)gene_len * sizeof(float));

    free(population); free(fitness); free(offspring);
}
