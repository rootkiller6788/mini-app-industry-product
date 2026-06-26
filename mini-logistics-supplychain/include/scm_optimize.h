/**
 * scm_optimize.h — SCM Optimization & Advanced Analytics
 *
 * L5-L8: Multi-echelon optimization, MRP, stochastic models,
 * Bullwhip Effect, and supply chain network design.
 *
 * L8 (Advanced Topics):
 *   - Multi-echelon inventory optimization
 *   - Bullwhip Effect quantification and mitigation
 *   - Green logistics optimization (cost-CO2 Pareto frontier)
 *   - Stochastic demand models with service-level constraints
 *
 * L5 Algorithms:
 *   - MRP explosion (Bill of Materials processing)
 *   - Multi-echelon base-stock policy computation
 *   - Lead time pooling and risk pooling
 *
 * L4 Standards/Theorems:
 *   - Bullwhip Effect (Forrester, 1958; Lee, Padmanabhan & Whang, 1997)
 *   - Risk Pooling Principle (Eppen, 1979)
 *   - Square Root Law for inventory consolidation
 *
 * Course Alignment:
 *   - MIT 15.762 Supply Chain Planning (Bullwhip, MRP)
 *   - MIT CTL.SC4x Supply Chain Dynamics
 *   - Stanford MS&E 262 Supply Chain Management
 *   - Georgia Tech ISYE 6333 Supply Chain Engineering
 *   - CMU 45-875 Supply Chain Optimization
 */

#ifndef SCM_OPTIMIZE_H
#define SCM_OPTIMIZE_H

#include "logistics_core.h"
#include "inventory.h"
#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * L1 — BOM (Bill of Materials) and MRP Structures
 * ============================================================ */

/** One component in a BOM */
typedef struct {
    char    component_sku[32];
    int     quantity_per_parent;   /* Qty needed per unit of parent */
    int     lead_time_days;        /* Procurement/production lead time */
    int     scrap_rate_pct;        /* Expected scrap percentage */
    int     is_critical;           /* 1 = critical path component */
} bom_component_t;

/** Bill of Materials for one product */
typedef struct {
    char            product_sku[32];
    char            description[256];
    int             lot_size;           /* Production lot size */
    int             lead_time_days;     /* Assembly lead time */
    bom_component_t *components;
    int              component_count;
    int              component_capacity;
    int              level;             /* 0 = finished good, 1 = subassembly, ... */
} bom_t;

/** MRP planned order release */
typedef struct {
    char    sku_id[32];
    int     period;                 /* Time bucket index */
    int     gross_requirements;
    int     scheduled_receipts;
    int     projected_on_hand;
    int     net_requirements;
    int     planned_order_receipt;
    int     planned_order_release;
} mrp_record_t;

/** MRP plan for one SKU across multiple periods */
typedef struct {
    char        sku_id[32];
    int         lead_time;          /* Lead time in periods */
    int         lot_size;           /* Order quantity (or 0 for lot-for-lot) */
    int         safety_stock;
    int         initial_inventory;
    mrp_record_t *periods;
    int          period_count;
} mrp_plan_t;

/* ============================================================
 * L1 — Network Optimization Structures
 * ============================================================ */

/** A candidate facility for network design */
typedef struct {
    char    facility_id[32];
    double  fixed_cost;          /* Annual fixed cost */
    double  variable_cost_per_unit;
    double  capacity_units;      /* Max annual throughput */
    geo_location_t location;
    int     is_open;             /* Decision variable */
    double  assigned_demand;     /* Demand assigned to this facility */
} network_facility_t;

/** A demand node (customer region) */
typedef struct {
    char    demand_id[32];
    double  annual_demand;       /* Units per year */
    geo_location_t location;
} demand_node_t;

/** Network design problem instance */
typedef struct {
    network_facility_t *facilities;
    int                  facility_count;
    demand_node_t       *demand_nodes;
    int                  demand_count;
    double             **transport_cost;  /* cost[i][j]: facility i → demand j */
} network_design_t;

/* ============================================================
 * L1 — Bullwhip Effect Analysis
 * ============================================================ */

/**
 * Echelon demand data for Bullwhip analysis.
 * Tracks orders placed vs demand received at each echelon.
 */
typedef struct {
    char    echelon_name[64];
    int     echelon_level;        /* 0 = retailer, n-1 = raw material supplier */
    double  *demand_received;     /* Demand seen by this echelon (time series) */
    double  *orders_placed;       /* Orders placed upstream (time series) */
    int     period_count;
    double  demand_variance;
    double  order_variance;
    double  bullwhip_ratio;       /* Var(orders) / Var(demand) */
} bullwhip_echelon_t;

/* ============================================================
 * L5 — MRP Explosion
 * ============================================================ */

/**
 * Perform MRP explosion for a finished good across time periods.
 *
 * Algorithm:
 * 1. For each period t, compute:
 *    - Gross requirements (from MPS or parent planned order releases)
 *    - Projected on-hand = prior on-hand + scheduled receipts - gross req
 *    - Net requirements = max(0, gross req - projected on-hand - safety stock)
 *    - Planned order receipt (lot-sized if lot_size > 0, else lot-for-lot)
 *    - Planned order release = planned receipt offset by lead time
 *
 * Theorem: MRP is a backward scheduling algorithm that ensures
 * material availability. It provides feasible but not necessarily
 * optimal production plans.
 *
 * Complexity: O(T) where T = number of time periods
 *
 * @param plan       Pre-allocated MRP plan with lead_time, lot_size,
 *                   safety_stock, initial_inventory, period_count set.
 * @param gross_req  Array of gross requirements per period (length = period_count)
 * @return 0 on success
 */
int  mrp_explode(mrp_plan_t *plan, const int *gross_req);

/**
 * Initialize a BOM structure.
 * Complexity: O(1)
 */
int  bom_init(bom_t *bom, const char *product_sku, const char *description,
              int lead_time_days, int lot_size, int level);

/**
 * Add a component to a BOM.
 * Complexity: O(1) amortized
 */
int  bom_add_component(bom_t *bom, const char *component_sku,
                       int qty_per_parent, int lead_time_days);

/**
 * Free BOM memory.
 * Complexity: O(1)
 */
void bom_destroy(bom_t *bom);

/**
 * Initialize an MRP plan.
 * Complexity: O(period_count) for allocation
 */
int  mrp_plan_init(mrp_plan_t *plan, const char *sku_id,
                   int lead_time, int lot_size, int safety_stock,
                   int initial_inventory, int period_count);

/**
 * Free MRP plan memory.
 * Complexity: O(1)
 */
void mrp_plan_destroy(mrp_plan_t *plan);

/* ============================================================
 * L8 — Multi-Echelon Inventory Optimization
 * ============================================================ */

/**
 * Compute guaranteed service time for a serial multi-echelon system.
 *
 * Given a chain of facilities e0 → e1 → ... → e_{n-1} (retailer to
 * raw material), each with deterministic processing time, compute
 * the guaranteed service time to the end customer.
 *
 *   GST(e_i) = processing_time(e_i) + max(0, GST(e_{i-1}) - coverage(e_i))
 *
 * Where coverage time is the inventory coverage at each echelon.
 *
 * Complexity: O(n) where n = number of echelons
 *
 * @param processing_times  Processing time at each echelon (days)
 * @param coverage_times    Inventory coverage at each echelon (days)
 * @param n                 Number of echelons
 * @return Guaranteed service time to end customer (days)
 */
double guaranteed_service_time(const double *processing_times,
                               const double *coverage_times, int n);

/**
 * Compute risk pooling benefit using the Square Root Law.
 *
 * Square Root Law (Maister, 1976; Eppen, 1979):
 *   When N identical warehouses are consolidated into 1,
 *   total safety stock reduces by factor 1/√N.
 *
 *   SS_consolidated = SS_individual × √N / 1 = SS_individual · √N
 *   Actually: SS_total_after = SS_per_facility_before × √N
 *
 * More precisely, for N facilities each holding SS with same
 * demand std σ and service level:
 *   SS_each = Z·σ·√LT
 *   SS_centralized = Z·√(N·σ²)·√LT = Z·σ·√(N·LT)
 *   Reduction = (SS_each·N - SS_centralized) / (SS_each·N)
 *              = (N - √N) / N = 1 - 1/√N
 *
 * Complexity: O(1)
 *
 * @param n_facilities        Number of pre-consolidation facilities
 * @param ss_per_facility     Safety stock at each facility
 * @param ss_consolidated_out Output: total safety stock after consolidation
 * @return Savings ratio (0.0 to 1.0)
 */
double risk_pooling_savings(int n_facilities, double ss_per_facility,
                            double *ss_consolidated_out);

/* ============================================================
 * L4/L8 — Bullwhip Effect
 * ============================================================ */

/**
 * Compute the Bullwhip Effect ratio for each echelon.
 *
 * The Bullwhip Effect (Forrester 1958; Lee, Padmanabhan & Whang, 1997)
 * describes how demand variability amplifies as it moves upstream
 * in a supply chain:
 *
 *   Bullwhip Ratio = Var(Orders) / Var(Demand)
 *
 * Causes (Lee et al., 1997):
 *   1. Demand signal processing (forecast updating)
 *   2. Order batching (periodic review)
 *   3. Price fluctuations (forward buying)
 *   4. Rationing game (shortage gaming)
 *
 * A ratio > 1.0 indicates amplification.
 *
 * Complexity: O(n·T) where n = number of echelons, T = periods
 *
 * @param echelons       Array of echelon structures with data filled
 * @param n_echelons     Number of echelons
 * @param periods        Number of time periods
 * @param fill_ratios    If 1, fills bullwhip_ratio for each echelon
 */
void bullwhip_analyze(bullwhip_echelon_t *echelons, int n_echelons,
                      int periods, int fill_ratios);

/**
 * Compute demand variance from an array of observations.
 * Complexity: O(n)
 */
double compute_variance(const double *data, int n);

/* ============================================================
 * L8 — Green Logistics Optimization
 * ============================================================ */

/**
 * Compute the Pareto-optimal trade-off between cost and CO2 for
 * a set of transportation alternatives.
 *
 * Each alternative has (cost, co2). The Pareto frontier consists
 * of non-dominated alternatives: no other alternative has both
 * lower cost AND lower CO2.
 *
 * Complexity: O(n²) for Pareto filter, O(n log n) for sorting
 *
 * @param costs      Array of costs for each alternative
 * @param co2        Array of CO2 emissions for each alternative
 * @param n          Number of alternatives
 * @param pareto_idx Output: indices of Pareto-optimal alternatives
 *                   (caller allocates n ints)
 * @param pareto_count Output: number of Pareto-optimal alternatives
 */
void green_pareto_frontier(const double *costs, const double *co2,
                           int n, int *pareto_idx, int *pareto_count);

/**
 * Facility location optimization: Greedy ADD algorithm.
 *
 * For uncapacitated facility location, iteratively adds the
 * facility that gives the greatest cost reduction until no
 * improvement is possible.
 *
 * Complexity: O(F²·D) where F = candidate facilities, D = demand nodes
 *
 * @param network     Network design problem
 * @param max_facilities  Maximum facilities to open
 * @param total_cost_out  Output: total annual cost
 * @return Number of facilities opened
 */
int  facility_location_greedy(network_design_t *network,
                               int max_facilities,
                               double *total_cost_out);

/**
 * Initialize a network design problem.
 * Complexity: O(F·D) for transport cost matrix
 */
int  network_design_init(network_design_t *network,
                         int facility_count, int demand_count);

/**
 * Free network design memory.
 * Complexity: O(F·D)
 */
void network_design_destroy(network_design_t *network);

/**
 * Compute total CO2 emissions for a shipment.
 *
 * Uses GLEC Framework emission factors:
 *   Truck: 0.062 kg CO2/ton-km
 *   Rail:  0.022 kg CO2/ton-km
 *   Air:   0.602 kg CO2/ton-km
 *   Ocean: 0.008 kg CO2/ton-km
 *
 * Complexity: O(1)
 */
double co2_emissions(shipment_mode_t mode, double weight_tonnes,
                     double distance_km);

/**
 * Lead time pooling: compute safety stock reduction when
 * moving from N suppliers to 1 with pooled demand.
 *
 * Complexity: O(1)
 */
double lead_time_pooling_savings(int n_suppliers, double demand_per_supplier,
                                 double stddev_per_supplier,
                                 double lead_time_days,
                                 double service_level);

#endif /* SCM_OPTIMIZE_H */
