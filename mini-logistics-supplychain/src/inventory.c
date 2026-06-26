/**
 * inventory.c — Inventory Management Implementations
 *
 * L4: EOQ computation, safety stock, reorder point, newsvendor model
 * L5: ABC classification, FIFO/LIFO cost layer tracking, inventory KPIs
 */

#include "inventory.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* π constant */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================
 * L4 — Z-Score for Service Level
 * ============================================================
 *
 * Rational approximation of the inverse standard normal CDF.
 *
 * For p ∈ [0.5, 1.0), we compute Z = Φ⁻¹(p) using:
 *   t = sqrt(-2 · ln(1-p))
 *   Z = t - P(t) / Q(t)
 *
 * where P and Q are rational polynomials (Abramowitz & Stegun 26.2.23).
 *
 * Error |ε(p)| < 4.5×10⁻⁴ for all p.
 *
 * For p < 0.5, we use symmetry: Z(p) = -Z(1-p).
 *
 * For p ≥ 1.0 - ε (near 1), we cap at Z = 4.5 (~99.99997%).
 */
double z_score_for_service_level(double service_level) {
    /* Clamp to valid range */
    if (service_level <= 0.5) return 0.0;
    if (service_level >= 0.999999) return 4.5;

    double p = service_level;
    int sign = 1;
    if (p < 0.5) {
        p = 1.0 - p;
        sign = -1;
    }

    double t = sqrt(-2.0 * log(1.0 - p));

    /* Abramowitz & Stegun constants (formula 26.2.23) */
    const double c0 = 2.515517;
    const double c1 = 0.802853;
    const double c2 = 0.010328;
    const double d1 = 1.432788;
    const double d2 = 0.189269;
    const double d3 = 0.001308;

    double numerator = c0 + c1 * t + c2 * t * t;
    double denominator = 1.0 + d1 * t + d2 * t * t + d3 * t * t * t;

    double z = t - numerator / denominator;
    return sign * z;
}

/* ============================================================
 * L4 — EOQ Computation (Harris-Wilson Model)
 * ============================================================
 *
 * The classic EOQ formula minimizes:
 *   TC(Q) = (D/Q)·S + (Q/2)·H  + D·C
 *
 * First-order condition dTC/dQ = 0:
 *   Q* = sqrt(2·D·S / H)
 *
 * At Q*:
 *   TC* = sqrt(2·D·S·H) + D·C  (purchase cost not optimized by Q)
 *   N*  = D / Q*  (orders per year)
 *   T*  = Q* / D  (years per cycle)
 */
eoq_result_t eoq_compute(const eoq_params_t *params) {
    eoq_result_t result;
    memset(&result, 0, sizeof(result));

    if (!params || params->annual_demand <= 0.0 ||
        params->ordering_cost <= 0.0 || params->holding_cost_per_unit <= 0.0) {
        return result;
    }

    double D = params->annual_demand;
    double S = params->ordering_cost;
    double H = params->holding_cost_per_unit;
    double C = params->unit_cost;

    result.eoq = sqrt(2.0 * D * S / H);
    result.total_annual_cost = sqrt(2.0 * D * S * H) + D * C;
    result.orders_per_year = D / result.eoq;
    result.cycle_time_days = (result.eoq / D) *
        (double)(params->working_days_per_year > 0 ?
                 params->working_days_per_year : 250);
    result.avg_inventory = result.eoq / 2.0;
    result.max_inventory = result.eoq;

    /* Reorder point = daily demand × lead time */
    double daily_demand = D / (double)(params->working_days_per_year > 0 ?
                                       params->working_days_per_year : 250);
    result.reorder_point = daily_demand * params->lead_time_days;

    return result;
}

/* ============================================================
 * L4 — Safety Stock Computation
 * ============================================================
 *
 * Two cases:
 *   Case 1 (constant lead time, σ_LT = 0):
 *     SS = Z_α · σ_d · √(LT)
 *
 *   Case 2 (variable lead time):
 *     SS = Z_α · √(LT_avg · σ_d² + d_avg² · σ_LT²)
 *
 * Returns the computed safety stock level (integer units).
 */
int safety_stock_compute(const safety_stock_params_t *params) {
    if (!params || params->avg_daily_demand < 0.0 ||
        params->avg_lead_time_days <= 0.0) {
        return 0;
    }

    double Z = z_score_for_service_level(params->service_level);
    double d_avg = params->avg_daily_demand;
    double sigma_d = params->stddev_daily_demand;
    double LT_avg = params->avg_lead_time_days;
    double sigma_LT = params->stddev_lead_time_days;

    double variance;
    if (sigma_LT <= 0.0) {
        /* Case 1: Constant lead time */
        variance = sigma_d * sigma_d * LT_avg;
    } else {
        /* Case 2: Variable lead time */
        variance = LT_avg * sigma_d * sigma_d +
                   d_avg * d_avg * sigma_LT * sigma_LT;
    }

    double ss = Z * sqrt(variance);
    return (int)ceil(ss);
}

/* ============================================================
 * L4 — Reorder Point
 * ============================================================
 *
 *   ROP = d_avg · LT_avg + SS
 *
 * When inventory drops to ROP, place an order of size EOQ.
 */
int reorder_point_compute(double avg_daily_demand, double lead_time_days,
                          int safety_stock) {
    if (avg_daily_demand < 0.0 || lead_time_days < 0.0) return 0;
    double rop_d = avg_daily_demand * lead_time_days + (double)safety_stock;
    return (int)ceil(rop_d);
}

/* ============================================================
 * L5 — Cost Layer Stack (FIFO/LIFO)
 * ============================================================ */
void cost_layer_stack_init(cost_layer_stack_t *stack) {
    if (!stack) return;
    memset(stack, 0, sizeof(cost_layer_stack_t));
}

int cost_layer_push(cost_layer_stack_t *stack, int quantity,
                    double unit_cost, const char *lot_number) {
    if (!stack || quantity <= 0) return -1;

    /* Expand if needed */
    if (stack->count >= stack->capacity) {
        int new_cap = stack->capacity == 0 ? 8 : stack->capacity * 2;
        cost_layer_t *new_layers = (cost_layer_t *)realloc(stack->layers,
            new_cap * sizeof(cost_layer_t));
        if (!new_layers) return -1;
        stack->layers = new_layers;
        stack->capacity = new_cap;
    }

    cost_layer_t *layer = &stack->layers[stack->count];
    memset(layer, 0, sizeof(cost_layer_t));
    layer->quantity = quantity;
    layer->unit_cost = unit_cost;
    layer->received_date = time(NULL);
    if (lot_number) {
        strncpy(layer->lot_number, lot_number, sizeof(layer->lot_number) - 1);
        layer->lot_number[sizeof(layer->lot_number) - 1] = '\0';
    }

    stack->total_value += (double)quantity * unit_cost;
    stack->total_units += (double)quantity;
    stack->count++;
    return 0;
}

int cost_layer_draw_fifo(cost_layer_stack_t *stack, int quantity,
                         double *cost_of_goods_sold) {
    if (!stack || !stack->layers || quantity <= 0) {
        if (cost_of_goods_sold) *cost_of_goods_sold = 0.0;
        return 0;
    }

    int remaining = quantity;
    double total_cogs = 0.0;
    int layers_drawn = 0;

    /* FIFO: remove from front (oldest layers, index 0) */
    while (remaining > 0 && stack->count > 0) {
        cost_layer_t *first = &stack->layers[0];
        int draw = (remaining < first->quantity) ? remaining : first->quantity;

        total_cogs += (double)draw * first->unit_cost;
        first->quantity -= draw;
        stack->total_units -= (double)draw;
        stack->total_value -= (double)draw * first->unit_cost;
        remaining -= draw;
        layers_drawn++;

        if (first->quantity <= 0) {
            /* Remove empty layer; shift remaining layers down */
            if (stack->count > 1) {
                memmove(&stack->layers[0], &stack->layers[1],
                        (stack->count - 1) * sizeof(cost_layer_t));
            }
            stack->count--;
        }
    }

    if (cost_of_goods_sold) *cost_of_goods_sold = total_cogs;
    return quantity - remaining;
}

int cost_layer_draw_lifo(cost_layer_stack_t *stack, int quantity,
                         double *cost_of_goods_sold) {
    if (!stack || !stack->layers || quantity <= 0) {
        if (cost_of_goods_sold) *cost_of_goods_sold = 0.0;
        return 0;
    }

    int remaining = quantity;
    double total_cogs = 0.0;

    /* LIFO: remove from back (newest layers, index count-1) */
    while (remaining > 0 && stack->count > 0) {
        cost_layer_t *last = &stack->layers[stack->count - 1];
        int draw = (remaining < last->quantity) ? remaining : last->quantity;

        total_cogs += (double)draw * last->unit_cost;
        last->quantity -= draw;
        stack->total_units -= (double)draw;
        stack->total_value -= (double)draw * last->unit_cost;
        remaining -= draw;

        if (last->quantity <= 0) {
            stack->count--;
        }
    }

    if (cost_of_goods_sold) *cost_of_goods_sold = total_cogs;
    return quantity - remaining;
}

double cost_layer_avg_cost(const cost_layer_stack_t *stack) {
    if (!stack || stack->total_units <= 0.0) return 0.0;
    return stack->total_value / stack->total_units;
}

void cost_layer_stack_destroy(cost_layer_stack_t *stack) {
    if (!stack) return;
    free(stack->layers);
    stack->layers = NULL;
    stack->count = 0;
    stack->capacity = 0;
    stack->total_value = 0.0;
    stack->total_units = 0.0;
}

/* ============================================================
 * L5 — ABC Classification (Pareto Analysis)
 * ============================================================
 *
 * Sorts items by annual usage value descending, then computes
 * cumulative percentage and assigns A/B/C classes.
 *
 * Default thresholds:
 *   A: top 80% of cumulative value
 *   B: next 15% (80-95% cumulative)
 *   C: bottom 5%  (95-100% cumulative)
 */
static int abc_cmp_desc(const void *a, const void *b) {
    const abc_item_t *ia = (const abc_item_t *)a;
    const abc_item_t *ib = (const abc_item_t *)b;
    if (ib->annual_usage_value > ia->annual_usage_value) return 1;
    if (ib->annual_usage_value < ia->annual_usage_value) return -1;
    return 0;
}

void abc_classify(abc_item_t *items, int n,
                  double threshold_a, double threshold_b) {
    if (!items || n <= 0) return;

    /* Validate thresholds */
    if (threshold_a <= 0.0 || threshold_a >= 1.0) threshold_a = 0.80;
    if (threshold_b <= threshold_a || threshold_b >= 1.0) threshold_b = 0.95;

    /* Compute total value */
    double total_value = 0.0;
    for (int i = 0; i < n; i++) {
        total_value += items[i].annual_usage_value;
    }

    /* Sort by annual usage value descending */
    qsort(items, n, sizeof(abc_item_t), abc_cmp_desc);

    /* Compute cumulative percentages and classify */
    double cumulative = 0.0;
    for (int i = 0; i < n; i++) {
        cumulative += items[i].annual_usage_value;
        items[i].cumulative_pct = (total_value > 0.0) ?
            (cumulative / total_value) : 0.0;

        if (items[i].cumulative_pct <= threshold_a) {
            items[i].classification = ABC_CLASS_A;
        } else if (items[i].cumulative_pct <= threshold_b) {
            items[i].classification = ABC_CLASS_B;
        } else {
            items[i].classification = ABC_CLASS_C;
        }
    }
}

/* ============================================================
 * L2 — Echelon Node Operations
 * ============================================================ */
void echelon_node_init(echelon_node_t *node, const char *facility_id,
                       int level) {
    if (!node) return;
    memset(node, 0, sizeof(echelon_node_t));
    if (facility_id) {
        strncpy(node->facility_id, facility_id, sizeof(node->facility_id) - 1);
        node->facility_id[sizeof(node->facility_id) - 1] = '\0';
    }
    node->echelon_level = level;
}

double echelon_cost(const echelon_node_t *node) {
    if (!node) return 0.0;
    int net_inventory = node->on_hand - node->backorder;
    double holding, backorder;

    if (net_inventory > 0) {
        holding = (double)net_inventory * node->holding_cost;
        backorder = 0.0;
    } else {
        holding = 0.0;
        backorder = (double)(-net_inventory) * node->backorder_cost;
    }

    return holding + backorder;
}

/* ============================================================
 * Inventory KPIs
 * ============================================================ */
double inventory_turnover(double cogs, double avg_inventory) {
    if (avg_inventory <= 0.0) return 0.0;
    return cogs / avg_inventory;
}

double days_inventory_on_hand(double avg_inventory, double annual_cogs) {
    if (annual_cogs <= 0.0) return 0.0;
    return (avg_inventory / annual_cogs) * 365.0;
}

/* ============================================================
 * L4 — Newsvendor Model
 * ============================================================
 *
 * For a single-period inventory problem with stochastic demand
 * D ~ N(μ, σ²), the optimal order quantity Q* satisfies:
 *
 *   Φ((Q* - μ) / σ) = cu / (cu + co)
 *
 * where Φ is the standard normal CDF.
 *
 * Therefore: Q* = μ + σ · Φ⁻¹(cu / (cu + co))
 *
 * This is also called the "critical ratio" or "critical fractile".
 *
 * Reference: Arrow, Harris & Marschak (1951); Porteus (2002)
 */
double newsvendor_optimal_qty(double mean_demand, double stddev_demand,
                              double underage_cost, double overage_cost) {
    if (underage_cost <= 0.0 || overage_cost <= 0.0 ||
        stddev_demand < 0.0) {
        return mean_demand;
    }

    /* Critical ratio */
    double critical_ratio = underage_cost / (underage_cost + overage_cost);

    /* Z-score for the critical ratio */
    double z = z_score_for_service_level(critical_ratio);

    return mean_demand + z * stddev_demand;
}
