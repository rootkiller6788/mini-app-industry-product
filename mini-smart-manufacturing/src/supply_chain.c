/* supply_chain.c - Supply Chain & Inventory Management
 * L2: MRP II, JIT, Kanban pull system, demand forecasting
 * L4: APICS SCOR model, ISO 28000, GS1 standards
 * L5: MRP explosion (Orlicky 1975), EOQ (Harris 1913),
 *     Silver-Meal lot sizing, exponential smoothing forecast,
 *     ABC inventory classification
 * L6: Multi-level MRP, safety stock optimization
 */
#include "supply_chain.h"
#include "smart_manufacturing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define M_EPS 0.001f

/* ---- Utility ---- */

const char* smf_item_type_name(SMF_ItemType type) {
    switch (type) {
        case SMF_ITEM_PURCHASED:    return "Purchased";
        case SMF_ITEM_MANUFACTURED: return "Manufactured";
        case SMF_ITEM_INTERMEDIATE: return "Intermediate";
        case SMF_ITEM_PHANTOM:      return "Phantom";
        default:                    return "Unknown";
    }
}

const char* smf_forecast_method_name(SMF_ForecastMethod method) {
    switch (method) {
        case SMF_FORECAST_NAIVE:             return "Naive";
        case SMF_FORECAST_MA:                return "Moving Average";
        case SMF_FORECAST_WMA:               return "Weighted MA";
        case SMF_FORECAST_SES:               return "Simple Exponential Smoothing";
        case SMF_FORECAST_HOLT:              return "Holt (Double ES)";
        case SMF_FORECAST_HOLT_WINTERS:      return "Holt-Winters";
        case SMF_FORECAST_LINEAR_REGRESSION: return "Linear Regression";
        default:                             return "Unknown";
    }
}

const char* smf_kanban_type_name(SMF_KanbanType type) {
    switch (type) {
        case SMF_KANBAN_PRODUCTION: return "Production Kanban";
        case SMF_KANBAN_WITHDRAWAL: return "Withdrawal Kanban";
        case SMF_KANBAN_SIGNAL:     return "Signal Kanban";
        case SMF_KANBAN_E_2BIN:     return "e-Kanban (2-Bin)";
        default:                    return "Unknown";
    }
}

/* ================================================================
 *  MRP - Material Requirements Planning (Orlicky 1975)
 *
 *  MRP Net Requirements Logic:
 *  1. Gross Requirements = demand from parent items + independent demand
 *  2. Net Requirements = Gross - Projected On Hand - Scheduled Receipts
 *  3. If Net > 0, create Planned Order Release offset by lead time
 *  4. Apply lot-sizing rule to determine order quantity
 *  5. Explode BOM level by level (Low-Level Code ordering)
 * ================================================================ */

int smf_mrp_init(SMF_MRPPlan *plan, const SMF_Factory *factory,
                  int planning_horizon_days, int time_bucket_days) {
    if (!plan || !factory) return -1;
    memset(plan, 0, sizeof(SMF_MRPPlan));
    plan->planning_horizon = planning_horizon_days;
    plan->time_bucket_days = time_bucket_days;
    plan->plan_start = time(NULL);
    plan->plan_end = plan->plan_start + planning_horizon_days * 86400;
    plan->is_regenerated = true;

    /* Initialize MRP items from factory materials */
    plan->num_items = factory->num_materials < SMF_MRP_MAX_ITEMS
                      ? factory->num_materials : SMF_MRP_MAX_ITEMS;

    for (int i = 0; i < plan->num_items; i++) {
        SMF_MRPItem *item = &plan->items[i];
        memset(item, 0, sizeof(SMF_MRPItem));
        item->item_idx = i;
        item->type = SMF_ITEM_PURCHASED;
        item->projected_on_hand = factory->materials[i].quantity_on_hand;
        item->lead_time_periods = (int)factory->materials[i].lead_time_days
                                   / time_bucket_days;
        if (item->lead_time_periods < 1) item->lead_time_periods = 1;
        item->safety_stock = factory->materials[i].safety_stock;
        item->lot_sizing_rule = 'L'; /* Default: Lot-for-Lot */
        item->lot_size = 1;
    }

    return 0;
}

int smf_mrp_assign_llc(const SMF_Factory *factory, int *llc_array,
                         int max_items) {
    if (!factory || !llc_array) return -1;

    /* Initialize all LLC to 0 */
    for (int i = 0; i < max_items; i++) llc_array[i] = 0;

    /* BFS through BOM structure to assign low-level codes */
    bool changed = true;
    while (changed) {
        changed = false;
        for (int b = 0; b < factory->num_boms; b++) {
            for (int i = 0; i < factory->boms[b].num_items; i++) {
                int mat_idx = factory->boms[b].items[i].material_idx;
                if (mat_idx < 0 || mat_idx >= max_items) continue;

                int parent_level = factory->boms[b].items[i].level;
                if (parent_level + 1 > llc_array[mat_idx]) {
                    llc_array[mat_idx] = parent_level + 1;
                    changed = true;
                }
            }
        }
    }

    return 0;
}

int smf_mrp_net_requirements(SMF_MRPItem *item, int num_periods) {
    if (!item || num_periods <= 0) return -1;

    /* Allocate time-phased arrays */
    item->planning_periods = num_periods;
    item->time_phased_gross = (float*)calloc(num_periods, sizeof(float));
    item->time_phased_scheduled = (float*)calloc(num_periods, sizeof(float));
    item->time_phased_on_hand = (float*)calloc(num_periods, sizeof(float));
    item->time_phased_net = (float*)calloc(num_periods, sizeof(float));
    item->time_phased_por = (float*)calloc(num_periods, sizeof(float));
    item->time_phased_porl = (float*)calloc(num_periods, sizeof(float));

    float on_hand = item->projected_on_hand;

    for (int t = 0; t < num_periods; t++) {
        /* Projected on-hand at start of period */
        item->time_phased_on_hand[t] = on_hand;

        /* Gross requirements come from BOM explosion (set externally) */

        /* Add scheduled receipts */
        on_hand += item->time_phased_scheduled[t];

        /* Subtract gross requirements */
        float gross = item->time_phased_gross[t];

        /* Net = max(0, gross + safety - on_hand) */
        float net = gross + item->safety_stock - on_hand;
        if (net < 0.0f) net = 0.0f;
        item->time_phased_net[t] = net;

        /* If net > 0, create planned order receipt */
        if (net > M_EPS) {
            float order_qty = net; /* Default: Lot-for-Lot */

            switch (item->lot_sizing_rule) {
                case 'F': /* Fixed Order Quantity */
                    order_qty = (float)item->lot_size;
                    break;
                case 'P': /* Period Order Quantity */
                    /* Sum net requirements for POQ periods */
                    order_qty = 0.0f;
                    for (int k = t; k < t + item->lot_size && k < num_periods; k++) {
                        order_qty += item->time_phased_gross[k] - item->time_phased_scheduled[k];
                    }
                    if (order_qty < net) order_qty = net;
                    break;
                default: /* L4L */
                    order_qty = net;
                    break;
            }

            item->time_phased_por[t] = order_qty;
            on_hand += order_qty;

            /* Planned order release = POR offset by lead time */
            int release_period = t - item->lead_time_periods;
            if (release_period >= 0 && release_period < num_periods) {
                item->time_phased_porl[release_period] = order_qty;
            }
        }

        /* Update projected on-hand for end of period */
        item->projected_on_hand = on_hand;
        on_hand -= gross;
        if (on_hand < 0.0f) on_hand = 0.0f;
    }

    return 0;
}

int smf_mrp_regenerate(SMF_MRPPlan *plan, const SMF_Factory *factory) {
    if (!plan || !factory) return -1;

    /* Step 1: Assign low-level codes */
    int *llc = (int*)calloc(factory->num_materials, sizeof(int));
    if (!llc) return -1;
    smf_mrp_assign_llc(factory, llc, factory->num_materials);

    /* Steps 2-5: Process level by level (LLC 0 to max) */
    int max_llc = 0;
    for (int i = 0; i < factory->num_materials; i++) {
        if (llc[i] > max_llc) max_llc = llc[i];
    }

    int periods = plan->planning_horizon / plan->time_bucket_days;
    if (periods < 1) periods = 1;

    for (int level = 0; level <= max_llc; level++) {
        for (int i = 0; i < plan->num_items; i++) {
            if (llc[plan->items[i].item_idx] == level) {
                smf_mrp_net_requirements(&plan->items[i], periods);
            }
        }
    }

    plan->is_regenerated = true;
    free(llc);
    return 0;
}

/* ================================================================
 *  Lot Sizing Methods (L5)
 * ================================================================ */

float smf_eoq_compute(float annual_demand, float order_cost,
                       float holding_cost) {
    /* EOQ = sqrt(2 * D * S / H)  閳?Harris 1913 */
    if (holding_cost <= 0.0f || annual_demand <= 0.0f) return 0.0f;
    return sqrtf((2.0f * annual_demand * order_cost) / holding_cost);
}

int smf_lotsize_l4l(SMF_MRPItem *item, int horizon) {
    if (!item || horizon <= 0) return -1;
    item->lot_sizing_rule = 'L';
    item->lot_size = 1;
    return 0;
}

int smf_lotsize_foq(SMF_MRPItem *item, int horizon, float foq) {
    if (!item || foq <= 0.0f) return -1;
    item->lot_sizing_rule = 'F';
    item->lot_size = (int)foq;
    (void)horizon;
    return 0;
}

int smf_lotsize_poq(SMF_MRPItem *item, int horizon, int periods) {
    if (!item || periods <= 0) return -1;
    item->lot_sizing_rule = 'P';
    item->lot_size = periods;
    (void)horizon;
    return 0;
}

int smf_lotsize_silver_meal(const float *demands, int horizon,
                             float setup_cost, float holding_cost_per_period,
                             int *order_periods) {
    /* Silver-Meal Heuristic (1973):
     * Minimize total relevant cost per period.
     * K(m) = (A + h * sum_{t=2..m} (t-1)*D_t) / m
     * Add periods while K(m+1) <= K(m).
     */
    if (!demands || !order_periods || horizon <= 0) return 0;

    int t = 0;
    int num_orders = 0;
    memset(order_periods, 0, horizon * sizeof(int));

    while (t < horizon) {
        order_periods[num_orders++] = t;
        float cumulative_holding = 0.0f;
        int m = 1;

        for (int k = t + 1; k < horizon; k++) {
            cumulative_holding += (float)(k - t) * demands[k];
            float cost_m = (setup_cost + holding_cost_per_period * cumulative_holding) / (float)m;
            float cost_m1 = m > 1 ? (setup_cost + holding_cost_per_period * (cumulative_holding - (float)(k - t) * demands[k])) / (float)(m - 1) : cost_m + 1.0f;

            if (cost_m > cost_m1 && m > 1) {
                break;
            }
            m = k - t + 1;
        }
        t += m;
    }

    return num_orders;
}

/* ================================================================
 *  Kanban System (L3: Pull Production)
 *
 *  Kanban Formula: N = (D * L * (1 + SF)) / C
 *  where D = daily demand, L = lead time (days),
 *        SF = safety factor, C = container capacity
 * ================================================================ */

int smf_kanban_calc_cards(float daily_demand, float lead_time_days,
                           float safety_factor, int container_qty) {
    if (daily_demand <= 0.0f || container_qty <= 0) return 0;
    float numerator = daily_demand * lead_time_days * (1.0f + safety_factor);
    return (int)ceilf(numerator / (float)container_qty - 1e-6f);
}

int smf_kanban_pull(SMF_Factory *factory, int kanban_idx, int quantity) {
    if (!factory || kanban_idx < 0) return -1;
    /* Consume from destination - simplified */
    (void)quantity;
    return 0;
}

int smf_kanban_replenish(SMF_Factory *factory, int kanban_idx, int quantity) {
    if (!factory || kanban_idx < 0) return -1;
    /* Replenish from source - simplified */
    (void)quantity;
    return 0;
}

bool smf_kanban_needs_replenish(const SMF_Kanban *kanban) {
    if (!kanban) return false;
    /* Signal when cards at destination fall below trigger */
    return kanban->cards_at_dest <= ((float)kanban->num_cards * 0.3f);
}

/* ================================================================
 *  Demand Forecasting (L5: Time Series Methods)
 *
 *  SES: F_{t+1} = alpha * D_t + (1 - alpha) * F_t
 *  MA:  F_{t+1} = (D_t + D_{t-1} + ... + D_{t-n+1}) / n
 *  Holt: Level and trend components
 *  Holt-Winters: Level, trend, and seasonal components
 * ================================================================ */

int smf_forecast_ses(SMF_DemandForecast *fc) {
    if (!fc || !fc->history || fc->history_len < 2) return -1;
    if (fc->alpha <= 0.0f || fc->alpha >= 1.0f) fc->alpha = 0.3f;

    if (!fc->forecast) {
        fc->forecast = (float*)calloc(fc->forecast_horizon, sizeof(float));
        if (!fc->forecast) return -1;
    }

    float f = fc->history[0];
    for (int i = 1; i < fc->history_len; i++) {
        f = fc->alpha * fc->history[i] + (1.0f - fc->alpha) * f;
    }

    fc->forecast[0] = f;
    for (int i = 1; i < fc->forecast_horizon; i++) {
        fc->forecast[i] = f; /* Constant forecast for SES */
    }

    fc->method = SMF_FORECAST_SES;
    return 0;
}

int smf_forecast_moving_avg(SMF_DemandForecast *fc, int window) {
    if (!fc || !fc->history || fc->history_len < window) return -1;

    if (!fc->forecast) {
        fc->forecast = (float*)calloc(fc->forecast_horizon, sizeof(float));
        if (!fc->forecast) return -1;
    }

    float sum = 0.0f;
    for (int i = fc->history_len - window; i < fc->history_len; i++) {
        sum += fc->history[i];
    }
    float avg = sum / (float)window;

    for (int i = 0; i < fc->forecast_horizon; i++) {
        fc->forecast[i] = avg;
    }

    fc->method = SMF_FORECAST_MA;
    return 0;
}

int smf_forecast_weighted_ma(SMF_DemandForecast *fc,
                              const float *weights, int window) {
    if (!fc || !fc->history || !weights || fc->history_len < window) return -1;

    if (!fc->forecast) {
        fc->forecast = (float*)calloc(fc->forecast_horizon, sizeof(float));
        if (!fc->forecast) return -1;
    }

    float wsum = 0.0f, wval = 0.0f;
    for (int i = 0; i < window; i++) {
        float d = fc->history[fc->history_len - window + i];
        wval += weights[i] * d;
        wsum += weights[i];
    }

    float pred = (wsum > 0.0f) ? wval / wsum : 0.0f;
    for (int i = 0; i < fc->forecast_horizon; i++) {
        fc->forecast[i] = pred;
    }

    fc->method = SMF_FORECAST_WMA;
    return 0;
}

int smf_forecast_holt(SMF_DemandForecast *fc) {
    if (!fc || !fc->history || fc->history_len < 3) return -1;
    if (fc->alpha <= 0.0f || fc->alpha >= 1.0f) fc->alpha = 0.3f;
    if (fc->beta <= 0.0f || fc->beta >= 1.0f) fc->beta = 0.1f;

    if (!fc->forecast) {
        fc->forecast = (float*)calloc(fc->forecast_horizon, sizeof(float));
        if (!fc->forecast) return -1;
    }

    /* Initialize level and trend from first two data points */
    float level = fc->history[0];
    float trend = fc->history[1] - fc->history[0];

    /* Update */
    for (int i = 1; i < fc->history_len; i++) {
        float old_level = level;
        level = fc->alpha * fc->history[i] + (1.0f - fc->alpha) * (level + trend);
        trend = fc->beta * (level - old_level) + (1.0f - fc->beta) * trend;
    }

    /* Forecast */
    for (int i = 0; i < fc->forecast_horizon; i++) {
        fc->forecast[i] = level + (float)(i + 1) * trend;
    }

    fc->method = SMF_FORECAST_HOLT;
    fc->has_trend = true;
    return 0;
}

int smf_forecast_holt_winters(SMF_DemandForecast *fc) {
    if (!fc || !fc->history || fc->history_len < fc->season_period * 2) return -1;
    if (fc->alpha <= 0.0f || fc->alpha >= 1.0f) fc->alpha = 0.3f;
    if (fc->beta <= 0.0f || fc->beta >= 1.0f) fc->beta = 0.1f;
    if (fc->gamma <= 0.0f || fc->gamma >= 1.0f) fc->gamma = 0.1f;

    if (!fc->forecast) {
        fc->forecast = (float*)calloc(fc->forecast_horizon, sizeof(float));
        if (!fc->forecast) return -1;
    }

    /* Simplified HW: multiplicative seasonality */
    float level = fc->history[0];
    float trend = (fc->history[fc->season_period] - fc->history[0]) / (float)fc->season_period;
    float *season = (float*)calloc(fc->season_period, sizeof(float));

    /* Initialize seasonal indices */
    for (int i = 0; i < fc->season_period && i < fc->history_len; i++) {
        float avg = 0.0f;
        int n = 0;
        for (int j = i; j < fc->history_len; j += fc->season_period) {
            avg += fc->history[j];
            n++;
        }
        season[i] = n > 0 ? (avg / (float)n) / level : 1.0f;
    }

    /* Update */
    for (int t = 0; t < fc->history_len; t++) {
        int s = t % fc->season_period;
        float old_level = level;
        if (season[s] > 0.0f) {
            level = fc->alpha * (fc->history[t] / season[s])
                    + (1.0f - fc->alpha) * (level + trend);
        }
        trend = fc->beta * (level - old_level) + (1.0f - fc->beta) * trend;
        if (level > 0.0f) {
            season[s] = fc->gamma * (fc->history[t] / level)
                        + (1.0f - fc->gamma) * season[s];
        }
    }

    /* Forecast */
    for (int i = 0; i < fc->forecast_horizon; i++) {
        int s = (fc->history_len + i) % fc->season_period;
        fc->forecast[i] = (level + (float)(i + 1) * trend) * season[s];
    }

    fc->method = SMF_FORECAST_HOLT_WINTERS;
    fc->has_trend = true;
    fc->has_seasonality = true;
    free(season);
    return 0;
}

int smf_forecast_accuracy(SMF_DemandForecast *fc, const float *actuals,
                           int num_actuals) {
    if (!fc || !actuals || num_actuals <= 0) return -1;

    float sum_abs_err = 0.0f, sum_sq_err = 0.0f, sum_abs_pct = 0.0f;
    int n = fc->forecast_horizon < num_actuals
            ? fc->forecast_horizon : num_actuals;

    for (int i = 0; i < n; i++) {
        float err = fc->forecast[i] - actuals[i];
        sum_abs_err += fabsf(err);
        sum_sq_err += err * err;

        if (fabsf(actuals[i]) > M_EPS) {
            sum_abs_pct += fabsf(err / actuals[i]);
        }
    }

    fc->mae = sum_abs_err / (float)n;
    fc->mse = sum_sq_err / (float)n;
    fc->mape = (n > 0) ? (sum_abs_pct / (float)n) * 100.0f : 0.0f;
    return 0;
}

/* ================================================================
 *  ABC Inventory Analysis (Pareto Principle)
 *
 *  Class A: top 80% of annual usage value (~20% of items)
 *  Class B: next 15% (~30% of items)
 *  Class C: bottom 5% (~50% of items)
 * ================================================================ */

int smf_abc_analyze(const SMF_Factory *factory,
                     SMF_ABCItem *results, int max_items) {
    if (!factory || !results || max_items <= 0) return 0;

    int n = factory->num_materials < max_items
            ? factory->num_materials : max_items;

    /* Compute annual usage value for each item */
    for (int i = 0; i < n; i++) {
        results[i].item_idx = i;
        results[i].annual_usage_value = factory->materials[i].quantity_on_hand
                                         * factory->materials[i].unit_cost
                                         * 12.0f; /* Assume monthly turnover */
    }

    /* Sort by descending annual usage value */
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (results[i].annual_usage_value < results[j].annual_usage_value) {
                SMF_ABCItem tmp = results[i];
                results[i] = results[j];
                results[j] = tmp;
            }
        }
    }

    /* Compute cumulative percentages and classify */
    float total_value = 0.0f;
    for (int i = 0; i < n; i++) total_value += results[i].annual_usage_value;

    float cum = 0.0f;
    for (int i = 0; i < n; i++) {
        results[i].rank = i + 1;
        if (total_value > 0.0f) {
            cum += results[i].annual_usage_value;
            results[i].cumulative_pct = (cum / total_value) * 100.0f;
        }
        if (results[i].cumulative_pct <= 80.0f) results[i].abc_class = 'A';
        else if (results[i].cumulative_pct <= 95.0f) results[i].abc_class = 'B';
        else results[i].abc_class = 'C';
    }

    return n;
}

/* ================================================================
 *  Safety Stock & Inventory Formulas
 *
 *  Safety Stock: SS = Z * sigma * sqrt(LT)
 *  Reorder Point: ROP = d * LT + SS
 * ================================================================ */

float smf_safety_stock(float z_score, float demand_stddev,
                        float lead_time_days) {
    return z_score * demand_stddev * sqrtf(lead_time_days);
}

float smf_reorder_point(float avg_daily_demand, float lead_time_days,
                         float safety_stock) {
    return avg_daily_demand * lead_time_days + safety_stock;
}

float smf_inventory_days_of_supply(float on_hand_qty, float daily_demand) {
    if (daily_demand <= 0.0f) return 0.0f;
    return on_hand_qty / daily_demand;
}

float smf_stockout_probability(float demand_mean, float demand_stddev,
                                float on_hand_qty) {
    /* Z-score: how many standard deviations on_hand is from mean */
    if (demand_stddev <= 0.0f) return on_hand_qty < demand_mean ? 1.0f : 0.0f;
    float z = (on_hand_qty - demand_mean) / demand_stddev;
    /* Standard normal CDF approximation */
    float prob = 0.5f * (1.0f + erff(-z / sqrtf(2.0f)));
    return prob;
}

/* ================================================================
 *  Supplier Management
 * ================================================================ */

int smf_supplier_add(SMF_Factory *factory, const char *code,
                      const char *name, const char *country,
                      int lead_time_days) {
    if (!factory || !code || !name) return -1;
    /* Simplified: no dedicated supplier storage in factory,
     * return a token index for future use */
    (void)country; (void)lead_time_days;
    return 0;
}

int smf_supplier_evaluate(SMF_Supplier *supplier) {
    if (!supplier) return -1;
    /* Scorecard = 0.4*Quality + 0.3*Delivery + 0.2*Cost + 0.1*Responsiveness */
    supplier->scorecard_total = 0.4f * supplier->quality_acceptance_pct
        + 0.3f * supplier->on_time_delivery_pct
        + 0.2f * supplier->cost_competitiveness
        + 0.1f * (100.0f - supplier->response_time_days * 10.0f);
    if (supplier->scorecard_total > 100.0f) supplier->scorecard_total = 100.0f;
    if (supplier->scorecard_total < 0.0f) supplier->scorecard_total = 0.0f;
    return 0;
}

int smf_supplier_rank(SMF_Supplier *suppliers, int num_suppliers,
                       int *ranked_indices) {
    if (!suppliers || !ranked_indices || num_suppliers <= 0) return -1;

    /* Initialize indices */
    for (int i = 0; i < num_suppliers; i++) {
        smf_supplier_evaluate(&suppliers[i]);
        ranked_indices[i] = i;
    }

    /* Sort by descending scorecard */
    for (int i = 0; i < num_suppliers - 1; i++) {
        for (int j = i + 1; j < num_suppliers; j++) {
            if (suppliers[ranked_indices[i]].scorecard_total
                < suppliers[ranked_indices[j]].scorecard_total) {
                int tmp = ranked_indices[i];
                ranked_indices[i] = ranked_indices[j];
                ranked_indices[j] = tmp;
            }
        }
    }
    return 0;
}

int smf_supplier_best_for_material(const SMF_Factory *factory,
                                    int material_idx) {
    if (!factory || material_idx < 0) return -1;
    /* Simply return the supplier assigned to this material */
    if (material_idx < factory->num_materials) {
        return factory->materials[material_idx].supplier_idx;
    }
    return -1;
}