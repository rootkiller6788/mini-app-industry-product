/**
 * inventory.h — Inventory Management
 *
 * L2-L5: Inventory control theory and practice.
 *
 * Core Concepts (L2):
 *   - Economic Order Quantity (EOQ)
 *   - Safety Stock and Reorder Point
 *   - ABC Classification (Pareto)
 *   - Inventory Valuation (FIFO/LIFO/Weighted Avg)
 *
 * Standards/Theorems (L4):
 *   - Harris-Wilson EOQ Model: Q* = sqrt(2DS/H)
 *   - Newsvendor critical ratio: Φ(z) = cu/(cu+co)
 *   - Little's Law: L = λ * W
 *
 * Algorithms (L5):
 *   - EOQ computation with quantity discounts
 *   - Safety stock calculation with service level
 *   - ABC analysis with cumulative value curve
 *   - FIFO/LIFO cost layer tracking
 *
 * Course Alignment:
 *   - MIT 15.762 Supply Chain Planning (EOQ, newsvendor)
 *   - MIT CTL.SC1x Inventory Management
 *   - Georgia Tech ISYE 6201 Inventory and Supply Chain
 *   - CMU 45-870 Supply Chain Management
 */

#ifndef INVENTORY_H
#define INVENTORY_H

#include "logistics_core.h"
#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * L4 — EOQ Model (Harris, 1913; Wilson, 1934)
 * ============================================================
 *
 * The Economic Order Quantity minimizes total inventory cost:
 *   TC(Q) = (D/Q)·S + (Q/2)·H
 *
 * Where:
 *   D = annual demand (units)
 *   S = ordering cost per order
 *   H = holding cost per unit per year
 *
 * Taking derivative dTC/dQ = 0:
 *   Q* = sqrt(2·D·S / H)
 *
 * Number of orders per year: N = D / Q*
 * Cycle time: T = Q* / D (years)
 * Total annual cost at optimum: TC* = sqrt(2·D·S·H)
 */

/** EOQ computation parameters */
typedef struct {
    double annual_demand;         /* D — units per year */
    double ordering_cost;         /* S — cost per order placed */
    double holding_cost_per_unit; /* H — carrying cost per unit per year */
    double unit_cost;             /* C — purchase cost per unit */
    double lead_time_days;        /* LT — supplier lead time */
    int    working_days_per_year; /* Typically 250-365 */
} eoq_params_t;

/** EOQ result with all derived values */
typedef struct {
    double eoq;              /* Q* — optimal order quantity */
    double total_annual_cost;/* TC(Q*) — minimum total cost */
    double orders_per_year;  /* N — number of orders annually */
    double cycle_time_days;  /* Time between orders */
    double reorder_point;    /* ROP = d_daily * LT */
    double avg_inventory;    /* Q divided by 2 */
    double max_inventory;    /* Q (if instantaneous replenishment) */
} eoq_result_t;

/* ============================================================
 * L4 — Safety Stock Model
 * ============================================================
 *
 * Safety stock protects against demand and lead time variability.
 *
 *   SS = Z_α · σ_d · sqrt(LT)
 *
 * Where:
 *   Z_α  = Z-score for desired service level α
 *   σ_d  = standard deviation of daily demand
 *   LT   = lead time in days
 *
 * Common Z-scores:
 *   90% → 1.282, 95% → 1.645, 97.5% → 1.960
 *   99% → 2.326, 99.9% → 3.090
 *
 * If both demand and lead time are variable:
 *   SS = Z_α · sqrt(LT_avg · σ_d² + d_avg² · σ_LT²)
 */

/** Safety stock computation parameters */
typedef struct {
    double avg_daily_demand;        /* d_avg */
    double stddev_daily_demand;     /* σ_d */
    double avg_lead_time_days;      /* LT_avg */
    double stddev_lead_time_days;   /* σ_LT (0 if constant) */
    double service_level;           /* Target service level (0.90-0.999) */
} safety_stock_params_t;

/**
 * Compute the Z-score for a given service level using the
 * inverse standard normal CDF approximation (Abramowitz & Stegun).
 *
 *   Z ≈ t - (c0 + c1·t + c2·t²)/(1 + d1·t + d2·t² + d3·t³)
 *   where t = sqrt(-2·ln(1-p)) for p ≥ 0.5
 *
 * Error < 4.5×10⁻⁴ (Abramowitz and Stegun, 1964, formula 26.2.23)
 */
double z_score_for_service_level(double service_level);

/* ============================================================
 * L1 — Cost Layer (for FIFO/LIFO tracking)
 * ============================================================ */

/** A cost layer for inventory valuation */
typedef struct {
    int     quantity;       /* Units in this layer */
    double  unit_cost;      /* Cost per unit at acquisition */
    time_t  received_date;  /* When this batch was received */
    char    lot_number[32]; /* Lot/batch identifier */
} cost_layer_t;

/** Stack of cost layers for LIFO/FIFO tracking */
typedef struct {
    cost_layer_t *layers;
    int            count;
    int            capacity;
    double         total_value;  /* Sum of (qty * cost) across all layers */
    double         total_units;  /* Sum of quantity across all layers */
} cost_layer_stack_t;

/* ============================================================
 * L1 — ABC Analysis
 * ============================================================ */

/** One item in ABC analysis */
typedef struct {
    char    sku_id[32];
    double  annual_usage_value;  /* Annual demand × unit cost */
    double  cumulative_pct;      /* Cumulative percentage of total value */
    abc_class_t classification;
} abc_item_t;

/* ============================================================
 * L1 — Multi-Echelon Inventory Node
 * ============================================================ */

/** An echelon in a multi-tier supply chain */
typedef struct {
    char    facility_id[32];
    int     echelon_level;       /* 0 = retailer, 1 = DC, 2 = factory, ... */
    int     on_hand;
    int     on_order;
    int     backorder;
    int     echelon_inventory;   /* On hand at this echelon + all downstream */
    int     echelon_on_order;    /* On order at this echelon + all downstream */
    double  holding_cost;
    double  backorder_cost;
    double  lead_time_to_downstream; /* LT to next lower echelon */
    int     base_stock_level;    /* Base-stock policy target */
} echelon_node_t;

/* ============================================================
 * L2/L5 — API: Inventory Management Operations
 * ============================================================ */

/**
 * Compute the Economic Order Quantity.
 *
 * Theorem: Harris-Wilson EOQ (1913/1934)
 *   Q* = sqrt(2·D·S / H)
 *
 * Complexity: O(1)
 */
eoq_result_t eoq_compute(const eoq_params_t *params);

/**
 * Compute safety stock level.
 *
 * Theorem: Safety stock formula
 *   If σ_LT = 0: SS = Z_α · σ_d · sqrt(LT_avg)
 *   If σ_LT > 0: SS = Z_α · sqrt(LT_avg·σ_d² + d_avg²·σ_LT²)
 *
 * Complexity: O(1)
 */
int safety_stock_compute(const safety_stock_params_t *params);

/**
 * Compute reorder point.
 *
 *   ROP = d_avg × LT_avg + SS
 *
 * Complexity: O(1)
 */
int reorder_point_compute(double avg_daily_demand, double lead_time_days,
                          int safety_stock);

/**
 * Initialize a cost layer stack.
 * Complexity: O(1)
 */
void cost_layer_stack_init(cost_layer_stack_t *stack);

/**
 * Push a new cost layer (e.g., when receiving inventory).
 * Complexity: O(1) amortized
 */
int  cost_layer_push(cost_layer_stack_t *stack, int quantity, double unit_cost,
                     const char *lot_number);

/**
 * Pop layers using FIFO method (oldest first).
 * Returns actual quantity drawn (may be less than requested if stock insufficient).
 * Updates the stack by consuming from oldest layers.
 * Complexity: O(k) where k = number of layers consumed
 */
int  cost_layer_draw_fifo(cost_layer_stack_t *stack, int quantity,
                          double *cost_of_goods_sold);

/**
 * Pop layers using LIFO method (newest first).
 * Returns actual quantity drawn.
 * Complexity: O(k) where k = number of layers consumed
 */
int  cost_layer_draw_lifo(cost_layer_stack_t *stack, int quantity,
                          double *cost_of_goods_sold);

/**
 * Compute weighted average cost per unit across all layers.
 * Complexity: O(n) where n = number of layers
 */
double cost_layer_avg_cost(const cost_layer_stack_t *stack);

/**
 * Free all memory in a cost layer stack.
 * Complexity: O(1)
 */
void cost_layer_stack_destroy(cost_layer_stack_t *stack);

/**
 * Perform ABC classification on an array of items.
 * Sorts items by annual usage value (descending), then assigns
 * A (top ~80% cumulative), B (next ~15%), C (bottom ~5%).
 *
 * Pareto Principle: ~20% of items account for ~80% of value.
 *
 * Complexity: O(n log n) due to sorting
 *
 * @param items         Array of items with annual_usage_value filled
 * @param n             Number of items
 * @param threshold_a   Cumulative percentage cutoff for A class (e.g., 0.80)
 * @param threshold_b   Cumulative percentage cutoff for B class (e.g., 0.95)
 */
void abc_classify(abc_item_t *items, int n, double threshold_a,
                  double threshold_b);

/**
 * Initialize an echelon node.
 * Complexity: O(1)
 */
void echelon_node_init(echelon_node_t *node, const char *facility_id,
                       int level);

/**
 * Compute total cost for an echelon node (holding + backorder).
 *
 * Cost = holding_cost × max(0, on_hand - backorder) +
 *        backorder_cost × max(0, backorder - on_hand)
 *
 * Complexity: O(1)
 */
double echelon_cost(const echelon_node_t *node);

/**
 * Compute inventory turnover ratio.
 *
 * Turnover = Cost of Goods Sold / Average Inventory
 *
 * Higher turnover indicates more efficient inventory management.
 *
 * Complexity: O(1)
 */
double inventory_turnover(double cogs, double avg_inventory);

/**
 * Compute days of inventory on hand.
 *
 * DOH = Average Inventory / (COGS / 365)
 *
 * Complexity: O(1)
 */
double days_inventory_on_hand(double avg_inventory, double annual_cogs);

/**
 * Newsvendor model: optimal order quantity for single-period
 * stochastic demand (perishable goods, fashion items, etc.).
 *
 * Theorem: The optimal quantity Q* satisfies:
 *   F(Q*) = cu / (cu + co)
 *
 * Where:
 *   cu = underage cost (profit lost per unit short)
 *   co = overage cost (loss per unit unsold)
 *   F(x) = CDF of demand distribution (assumed normal here)
 *
 * Complexity: O(1)
 */
double newsvendor_optimal_qty(double mean_demand, double stddev_demand,
                              double underage_cost, double overage_cost);

#endif /* INVENTORY_H */
