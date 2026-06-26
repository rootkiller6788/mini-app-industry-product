/**
 * scm_optimize.c — SCM Optimization Implementations
 *
 * L5: MRP explosion algorithm
 * L8: Multi-echelon optimization, Bullwhip Effect analysis,
 *     Green logistics Pareto frontier, facility location
 * L4: Risk Pooling Square Root Law, Lead Time Pooling
 *
 * References:
 *   - Forrester, J.W. (1958). Industrial Dynamics
 *   - Lee, Padmanabhan & Whang (1997). The Bullwhip Effect...
 *     Management Science, 43(4), 546-558.
 *   - Eppen, G.D. (1979). Effects of centralization on expected
 *     costs... Management Science, 25(5), 498-501.
 */

#include "scm_optimize.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ============================================================
 * L5 — MRP Explosion
 * ============================================================
 *
 * MRP (Material Requirements Planning) is a backward scheduling
 * algorithm. For each time period t:
 *
 * 1. Projected on-hand = prev_on_hand - gross_req[t] + scheduled_receipt[t]
 * 2. If projected_on_hand < safety_stock:
 *      net_req = max(0, gross_req[t] - projected_on_hand + safety_stock)
 * 3. Planned order receipt = lot-size adjusted net_req
 * 4. Planned order release = receipt offset by lead time
 *
 * Theorem: MRP provides a time-phased feasible schedule that
 * satisfies all gross requirements given lead time constraints.
 * It does NOT optimize any cost function — it is purely
 * a requirements explosion engine.
 */
int mrp_explode(mrp_plan_t *plan, const int *gross_req) {
    if (!plan || !gross_req || plan->period_count <= 0) return -1;

    int prev_on_hand = plan->initial_inventory;
    int ss = plan->safety_stock;
    int LT = plan->lead_time;
    int lot = plan->lot_size;

    for (int t = 0; t < plan->period_count; t++) {
        mrp_record_t *rec = &plan->periods[t];
        rec->period = t;
        rec->gross_requirements = gross_req[t];
        rec->scheduled_receipts = 0;  /* Filled externally */

        /* Projected on-hand = prev_on_hand - gross_req + scheduled_receipts */
        int projected = prev_on_hand - gross_req[t] + rec->scheduled_receipts;
        rec->projected_on_hand = projected;

        /* Net requirements = max(0, gross_req[t] + ss - prev_on_hand - scheduled_receipts) */
        int covered = prev_on_hand + rec->scheduled_receipts;
        int net = gross_req[t] + ss - covered;
        if (net < 0) net = 0;
        rec->net_requirements = net;

        /* Planned order receipt */
        int receipt = 0;
        if (lot > 0 && net > 0) {
            /* Lot-sized: round up to next lot multiple */
            receipt = ((net + lot - 1) / lot) * lot;
        } else if (lot == 0 && net > 0) {
            /* Lot-for-lot: order exact net requirements */
            receipt = net;
        }
        rec->planned_order_receipt = receipt;

        /* Planned order release: offset by lead time */
        int release_period = t - LT;
        if (release_period >= 0) {
            plan->periods[release_period].planned_order_release = receipt;
        } else if (t == 0 && receipt > 0) {
            /* Release needed before period 0 → past due */
            rec->planned_order_release = receipt;
        }

        /* Update projected on-hand with receipt */
        rec->projected_on_hand = projected + receipt;

        /* Set up for next period */
        prev_on_hand = rec->projected_on_hand;
    }

    return 0;
}

/* ============================================================
 * L5 — BOM & MRP Initialization
 * ============================================================ */
int bom_init(bom_t *bom, const char *product_sku, const char *description,
             int lead_time_days, int lot_size, int level) {
    if (!bom) return -1;
    memset(bom, 0, sizeof(bom_t));
    if (product_sku) {
        strncpy(bom->product_sku, product_sku, sizeof(bom->product_sku) - 1);
        bom->product_sku[sizeof(bom->product_sku) - 1] = '\0';
    }
    if (description) {
        strncpy(bom->description, description, sizeof(bom->description) - 1);
        bom->description[sizeof(bom->description) - 1] = '\0';
    }
    bom->lead_time_days = lead_time_days;
    bom->lot_size = lot_size;
    bom->level = level;
    return 0;
}

int bom_add_component(bom_t *bom, const char *component_sku,
                      int qty_per_parent, int lead_time_days) {
    if (!bom) return -1;

    if (bom->component_count >= bom->component_capacity) {
        int new_cap = bom->component_capacity == 0 ? 8 : bom->component_capacity * 2;
        bom_component_t *new_comps = (bom_component_t *)realloc(
            bom->components, new_cap * sizeof(bom_component_t));
        if (!new_comps) return -1;
        bom->components = new_comps;
        bom->component_capacity = new_cap;
    }

    bom_component_t *comp = &bom->components[bom->component_count];
    memset(comp, 0, sizeof(bom_component_t));
    if (component_sku) {
        strncpy(comp->component_sku, component_sku,
                sizeof(comp->component_sku) - 1);
        comp->component_sku[sizeof(comp->component_sku) - 1] = '\0';
    }
    comp->quantity_per_parent = qty_per_parent;
    comp->lead_time_days = lead_time_days;
    comp->scrap_rate_pct = 0;
    comp->is_critical = 0;
    bom->component_count++;
    return 0;
}

void bom_destroy(bom_t *bom) {
    if (!bom) return;
    free(bom->components);
    bom->components = NULL;
    bom->component_count = 0;
    bom->component_capacity = 0;
}

int mrp_plan_init(mrp_plan_t *plan, const char *sku_id,
                  int lead_time, int lot_size, int safety_stock,
                  int initial_inventory, int period_count) {
    if (!plan || period_count <= 0) return -1;
    memset(plan, 0, sizeof(mrp_plan_t));

    if (sku_id) {
        strncpy(plan->sku_id, sku_id, sizeof(plan->sku_id) - 1);
        plan->sku_id[sizeof(plan->sku_id) - 1] = '\0';
    }
    plan->lead_time = lead_time;
    plan->lot_size = lot_size;
    plan->safety_stock = safety_stock;
    plan->initial_inventory = initial_inventory;

    plan->periods = (mrp_record_t *)calloc(period_count, sizeof(mrp_record_t));
    if (!plan->periods) return -1;
    plan->period_count = period_count;
    return 0;
}

void mrp_plan_destroy(mrp_plan_t *plan) {
    if (!plan) return;
    free(plan->periods);
    plan->periods = NULL;
}

/* ============================================================
 * L8 — Guaranteed Service Time (Multi-Echelon)
 * ============================================================
 *
 * For a serial supply chain e₀ → e₁ → ... → e_{n-1}:
 *
 *   GST[i] = proc[i] + max(0, GST[i-1] - coverage[i])
 *
 * Where:
 *   proc[i]     = processing time at echelon i
 *   coverage[i] = inventory coverage time at echelon i
 *   GST[-1]     = 0 (no upstream echelon)
 *
 * The final GST to customer is GST[n-1].
 *
 * Reference: Graves, S.C. and Willems, S.P. (2000).
 * "Optimizing Strategic Safety Stock Placement in Supply Chains".
 * Manufacturing & Service Operations Management, 2(1), 68-83.
 */
double guaranteed_service_time(const double *processing_times,
                               const double *coverage_times, int n) {
    if (!processing_times || n <= 0) return 0.0;

    double gst_prev = 0.0;
    for (int i = 0; i < n; i++) {
        double cov = (coverage_times && i < n) ? coverage_times[i] : 0.0;
        double net_demand = gst_prev - cov;
        if (net_demand < 0.0) net_demand = 0.0;
        gst_prev = processing_times[i] + net_demand;
    }
    return gst_prev;
}

/* ============================================================
 * L4 — Risk Pooling (Square Root Law)
 * ============================================================
 *
 * When N identical decentralized facilities are consolidated:
 *   SS_centralized = SS_each × √N
 *
 * Savings ratio: (N·SS_each - √N·SS_each) / (N·SS_each)
 *              = 1 - 1/√N
 *
 * Example: 4 warehouses → savings = 1 - 1/2 = 50%
 *          9 warehouses → savings = 1 - 1/3 ≈ 66.7%
 */
double risk_pooling_savings(int n_facilities, double ss_per_facility,
                            double *ss_consolidated_out) {
    if (n_facilities <= 0) {
        if (ss_consolidated_out) *ss_consolidated_out = 0.0;
        return 0.0;
    }

    double ss_centralized = ss_per_facility * sqrt((double)n_facilities);
    if (ss_consolidated_out) *ss_consolidated_out = ss_centralized;

    double ss_total_before = ss_per_facility * (double)n_facilities;
    if (ss_total_before <= 0.0) return 0.0;

    return 1.0 - (ss_centralized / ss_total_before);
}

/* ============================================================
 * L4/L8 — Bullwhip Effect Analysis
 * ============================================================
 *
 * The Bullwhip Effect quantifies demand variability amplification
 * as it propagates upstream in a supply chain.
 *
 * Bullwhip Ratio = Var(Orders) / Var(Demand)
 *
 * For each echelon, we compute:
 *   demand_variance = Var(demand_received)
 *   order_variance  = Var(orders_placed)
 *   bullwhip_ratio  = order_variance / demand_variance
 *
 * A ratio > 1.0 indicates demand amplification.
 */
double compute_variance(const double *data, int n) {
    if (!data || n <= 1) return 0.0;

    double mean = 0.0;
    for (int i = 0; i < n; i++) {
        mean += data[i];
    }
    mean /= n;

    double var = 0.0;
    for (int i = 0; i < n; i++) {
        double diff = data[i] - mean;
        var += diff * diff;
    }
    return var / (double)(n - 1);  /* Sample variance, use n-1 */
}

void bullwhip_analyze(bullwhip_echelon_t *echelons, int n_echelons,
                      int periods, int fill_ratios) {
    if (!echelons || n_echelons <= 0 || periods <= 1) return;

    for (int e = 0; e < n_echelons; e++) {
        if (fill_ratios) {
            echelons[e].demand_variance =
                compute_variance(echelons[e].demand_received, periods);
            echelons[e].order_variance =
                compute_variance(echelons[e].orders_placed, periods);

            if (echelons[e].demand_variance > 1e-12) {
                echelons[e].bullwhip_ratio =
                    echelons[e].order_variance / echelons[e].demand_variance;
            } else {
                echelons[e].bullwhip_ratio = 1.0;
            }
        }
    }
}

/* ============================================================
 * L8 — Green Logistics Pareto Frontier
 * ============================================================
 *
 * A solution i dominates solution j if:
 *   cost[i] ≤ cost[j] AND co2[i] ≤ co2[j]
 *   AND (cost[i] < cost[j] OR co2[i] < co2[j])
 *
 * The Pareto frontier consists of all non-dominated solutions.
 */
void green_pareto_frontier(const double *costs, const double *co2,
                           int n, int *pareto_idx, int *pareto_count) {
    if (!costs || !co2 || !pareto_idx || !pareto_count || n <= 0) {
        if (pareto_count) *pareto_count = 0;
        return;
    }

    int *dominated = (int *)calloc(n, sizeof(int));
    if (!dominated) {
        *pareto_count = 0;
        return;
    }

    /* For each pair, check dominance */
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            /* Does j dominate i? */
            if (costs[j] <= costs[i] && co2[j] <= co2[i] &&
                (costs[j] < costs[i] || co2[j] < co2[i])) {
                dominated[i] = 1;
                break;
            }
        }
    }

    /* Collect non-dominated */
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (!dominated[i]) {
            pareto_idx[count++] = i;
        }
    }

    *pareto_count = count;
    free(dominated);
}

/* ============================================================
 * L8 — Facility Location: Greedy ADD Algorithm
 * ============================================================
 *
 * Kuehn-Hamburger (1963) greedy ADD heuristic for
 * uncapacitated facility location:
 *
 * 1. Start with no facilities open
 * 2. Iteratively add the facility that gives the greatest
 *    reduction in total cost (transport + fixed)
 * 3. Stop when no improvement or max_facilities reached
 *
 * Total cost for a given set of open facilities:
 *   Σ_j min_{i∈open} (transport_cost[i][j] + variable_cost[i])
 *   + Σ_{i∈open} fixed_cost[i]
 *
 * Complexity: O(F²·D) per iteration
 */
int facility_location_greedy(network_design_t *network,
                              int max_facilities,
                              double *total_cost_out) {
    if (!network || !network->facilities || !network->demand_nodes ||
        !network->transport_cost || network->facility_count <= 0 ||
        network->demand_count <= 0) {
        if (total_cost_out) *total_cost_out = 0.0;
        return 0;
    }

    int F = network->facility_count;
    int D = network->demand_count;

    /* Reset all facilities to closed */
    for (int i = 0; i < F; i++) {
        network->facilities[i].is_open = 0;
    }

    int opened = 0;
    double current_cost = INFINITY;

    for (int iter = 0; iter < max_facilities && iter < F; iter++) {
        int best_fac = -1;
        double best_delta = 0.0;

        for (int i = 0; i < F; i++) {
            if (network->facilities[i].is_open) continue;

            /* Temporarily open facility i */
            network->facilities[i].is_open = 1;

            /* Compute total cost with this facility open */
            double total = 0.0;
            for (int j = 0; j < D; j++) {
                double min_cost = INFINITY;
                for (int k = 0; k < F; k++) {
                    if (network->facilities[k].is_open) {
                        double c = network->transport_cost[k][j] +
                                   network->facilities[k].variable_cost_per_unit;
                        if (c < min_cost) min_cost = c;
                    }
                }
                if (min_cost < INFINITY) {
                    total += min_cost * network->demand_nodes[j].annual_demand;
                }
            }

            /* Add fixed costs */
            for (int k = 0; k < F; k++) {
                if (network->facilities[k].is_open) {
                    total += network->facilities[k].fixed_cost;
                }
            }

            /* Check improvement */
            double delta = current_cost - total;
            if (delta > best_delta) {
                best_delta = delta;
                best_fac = i;
            }

            /* Close facility i */
            network->facilities[i].is_open = 0;
        }

        if (best_fac < 0 || best_delta <= 0.0) break;

        /* Open the best facility */
        network->facilities[best_fac].is_open = 1;
        opened++;
        current_cost -= best_delta;
    }

    /* Recompute final cost */
    double final_cost = 0.0;
    for (int j = 0; j < D; j++) {
        double min_cost = INFINITY;
        for (int i = 0; i < F; i++) {
            if (network->facilities[i].is_open) {
                double c = network->transport_cost[i][j] +
                           network->facilities[i].variable_cost_per_unit;
                if (c < min_cost) min_cost = c;
            }
        }
        if (min_cost < INFINITY) {
            final_cost += min_cost * network->demand_nodes[j].annual_demand;
        }
    }
    for (int i = 0; i < F; i++) {
        if (network->facilities[i].is_open) {
            final_cost += network->facilities[i].fixed_cost;
        }
    }

    if (total_cost_out) *total_cost_out = final_cost;
    return opened;
}

/* ============================================================
 * Network Design Initialization / Destruction
 * ============================================================ */
int network_design_init(network_design_t *network,
                        int facility_count, int demand_count) {
    if (!network || facility_count <= 0 || demand_count <= 0) return -1;
    memset(network, 0, sizeof(network_design_t));

    network->facility_count = facility_count;
    network->demand_count = demand_count;

    network->facilities = (network_facility_t *)calloc(
        facility_count, sizeof(network_facility_t));
    network->demand_nodes = (demand_node_t *)calloc(
        demand_count, sizeof(demand_node_t));

    network->transport_cost = (double **)malloc(
        facility_count * sizeof(double *));
    if (!network->facilities || !network->demand_nodes ||
        !network->transport_cost) {
        network_design_destroy(network);
        return -1;
    }

    for (int i = 0; i < facility_count; i++) {
        network->transport_cost[i] = (double *)calloc(
            demand_count, sizeof(double));
        if (!network->transport_cost[i]) {
            network_design_destroy(network);
            return -1;
        }
    }

    return 0;
}

void network_design_destroy(network_design_t *network) {
    if (!network) return;
    if (network->transport_cost) {
        for (int i = 0; i < network->facility_count; i++) {
            free(network->transport_cost[i]);
        }
        free(network->transport_cost);
    }
    free(network->facilities);
    free(network->demand_nodes);
    memset(network, 0, sizeof(network_design_t));
}

/* ============================================================
 * L8 — CO2 Emissions Calculation (GLEC Framework)
 * ============================================================
 *
 * Emission factors (kg CO2 per tonne-km):
 *   - Truck: 0.062  (diesel, average load)
 *   - Rail:  0.022  (electric/diesel mix)
 *   - Air:   0.602  (cargo jet)
 *   - Ocean: 0.008  (container ship)
 *   - Last Mile: 0.085 (urban delivery, higher per-km)
 *
 * Reference: GLEC Framework v2.0, Smart Freight Centre (2019)
 */
double co2_emissions(shipment_mode_t mode, double weight_tonnes,
                     double distance_km) {
    double factor;
    switch (mode) {
        case SHIP_MODE_TRUCKLOAD:
        case SHIP_MODE_LTL:
            factor = 0.062;
            break;
        case SHIP_MODE_INTERMODAL:
            factor = 0.022;  /* Rail portion */
            break;
        case SHIP_MODE_AIR_FREIGHT:
            factor = 0.602;
            break;
        case SHIP_MODE_OCEAN_FREIGHT:
            factor = 0.008;
            break;
        case SHIP_MODE_LAST_MILE:
            factor = 0.085;
            break;
        case SHIP_MODE_PARCEL:
            factor = 0.100;  /* Parcel vans, less efficient */
            break;
        default:
            factor = 0.062;
    }
    return factor * weight_tonnes * distance_km;
}

/* ============================================================
 * L8 — Lead Time Pooling Savings
 * ============================================================
 *
 * When moving from N independent suppliers to 1 consolidated:
 *
 *   SS_before = N × Z × σ × √LT
 *   SS_after  = Z × √(N) × σ × √LT   (pooled demand variance = N·σ²)
 *
 * Savings = (SS_before - SS_after) / SS_before = 1 - 1/√N
 *
 * This is another manifestation of the Square Root Law.
 */
double lead_time_pooling_savings(int n_suppliers,
                                 double demand_per_supplier,
                                 double stddev_per_supplier,
                                 double lead_time_days,
                                 double service_level) {
    if (n_suppliers <= 1) return 0.0;
    (void)demand_per_supplier;  /* Used implicitly via square root law;
                                   savings ratio depends only on n_suppliers */

    /* Use the Z-score internally */
    double z = 0.0;
    if (service_level >= 0.999) z = 3.090;
    else if (service_level >= 0.990) z = 2.326;
    else if (service_level >= 0.975) z = 1.960;
    else if (service_level >= 0.950) z = 1.645;
    else if (service_level >= 0.900) z = 1.282;
    else z = 0.674;  /* 75% */

    double ss_each = z * stddev_per_supplier * sqrt(lead_time_days);
    double ss_before = ss_each * (double)n_suppliers;

    /* Pooled standard deviation */
    double pooled_std = sqrt((double)n_suppliers) * stddev_per_supplier;
    double ss_after = z * pooled_std * sqrt(lead_time_days);

    if (ss_before <= 0.0) return 0.0;
    return 1.0 - (ss_after / ss_before);
}
