#ifndef MINI_SMF_SUPPLY_CHAIN_H
#define MINI_SMF_SUPPLY_CHAIN_H

#include "smart_manufacturing.h"

/* ================================================================
 *  supply_chain.h — Supply Chain & Inventory Management
 *
 *  L2: Core concepts — MRP II (Manufacturing Resource Planning),
 *      Just-In-Time (JIT), Kanban pull system, supplier quality,
 *      demand forecasting, safety stock
 *  L3: Engineering structures — MRP explosion engine, Kanban card
 *      loop, supplier scorecard, warehouse location system
 *  L4: Standards — APICS SCOR model (Plan-Source-Make-Deliver-Return),
 *      ISO 28000 Supply Chain Security, GS1 barcode standards
 *  L5: Algorithms — MRP net requirements (Orlicky 1975), EOQ
 *      (Harris 1913), Silver-Meal lot sizing, Wagner-Whitin DP,
 *      exponential smoothing forecast, ABC inventory classification
 *  L6: Canonical problems — Multi-level MRP, supplier evaluation,
 *      demand forecasting, safety stock optimization
 *  L7: Applications — Supply chain visibility dashboard, digital
 *      supply chain twin
 *  L8: Advanced — DDMRP (Demand Driven MRP), blockchain supply
 *      chain traceability, AI demand forecasting
 *  L9: Industry — SAP IBP, Oracle SCM Cloud, Kinaxis RapidResponse,
 *      Blue Yonder (JDA)
 *
 *  References:
 *  - Orlicky, "Material Requirements Planning" (1975)
 *  - Harris, "How Many Parts to Make at Once" (1913) — EOQ
 *  - Ptak & Smith, "Demand Driven MRP" (2016)
 *  - Simchi-Levi, "Designing and Managing the Supply Chain" (2008)
 * ================================================================ */

#define SMF_MRP_MAX_ITEMS         8192
#define SMF_MRP_MAX_LEVELS         16
#define SMF_MRP_MAX_PLANNED_ORDS  4096
#define SMF_SC_MAX_SUPPLIERS      1024
#define SMF_SC_MAX_KANBANS         512

/* ---- L1: MRP Item Master ---- */

typedef enum {
    SMF_ITEM_PURCHASED  = 0,
    SMF_ITEM_MANUFACTURED = 1,
    SMF_ITEM_INTERMEDIATE = 2,
    SMF_ITEM_PHANTOM    = 3
} SMF_ItemType;

typedef struct {
    int         item_idx;        /* material index in factory */
    SMF_ItemType type;
    float       gross_requirements;
    float       scheduled_receipts;
    float       projected_on_hand;
    float       net_requirements;
    float       planned_order_receipts;
    float       planned_order_releases;
    int         lead_time_periods;
    int         low_level_code;  /* lowest BOM level */
    int         lot_size;
    char        lot_sizing_rule; /* 'L'=L4L, 'F'=FOQ, 'P'=POQ, 'M'=min */
    float       safety_stock;
    float       allocated_qty;
    int         planning_periods;
    float       *time_phased_gross;    /* gross reqs per period */
    float       *time_phased_scheduled; /* scheduled receipts per period */
    float       *time_phased_on_hand;   /* projected on hand per period */
    float       *time_phased_net;       /* net reqs per period */
    float       *time_phased_por;       /* planned order receipts per period */
    float       *time_phased_porl;      /* planned order releases per period */
} SMF_MRPItem;

/* ---- L1: MRP Plan ---- */

typedef struct {
    SMF_MRPItem items[SMF_MRP_MAX_ITEMS];
    int         num_items;
    int         planning_horizon;
    int         time_bucket_days;  /* 1=day, 7=week */
    time_t      plan_start;
    time_t      plan_end;
    int         master_schedule_id;
    bool        is_regenerated;    /* full regenerate vs net change */
} SMF_MRPPlan;

/* ---- L1: Planned Order ---- */

typedef struct {
    int     planned_order_id;
    int     item_idx;
    float   quantity;
    time_t  release_date;
    time_t  receipt_date;
    char    source[32];        /* "MRP", "MPS", "Manual" */
    int     source_order_id;   /* parent planned order, -1 if top-level */
    int     level;             /* BOM level */
    bool    is_firmed;         /* firmed planned order */
    bool    is_released;       /* converted to work/purchase order */
} SMF_PlannedOrder;

/* ---- L1: Supplier ---- */

typedef struct {
    char    code[SMF_CODE_LEN];
    char    name[SMF_NAME_LEN];
    char    country[64];
    int     lead_time_days;
    float   on_time_delivery_pct;
    float   quality_acceptance_pct;
    float   cost_competitiveness;  /* 0-100 */
    float   response_time_days;
    int     total_orders;
    int     late_orders;
    int     rejected_lots;
    float   scorecard_total;       /* weighted supplier score */
    bool    is_preferred;
    bool    is_certified;          /* ISO 9001 certified */
    time_t  last_audit_date;
    char    payment_terms[32];     /* "Net30", "Net60", etc. */
    char    contact_name[SMF_NAME_LEN];
    char    contact_email[SMF_NAME_LEN];
} SMF_Supplier;

/* ---- L1: Kanban Card ---- */

typedef enum {
    SMF_KANBAN_PRODUCTION = 0,  /* P-Kanban: produce */
    SMF_KANBAN_WITHDRAWAL = 1,  /* W-Kanban: move/withdraw */
    SMF_KANBAN_SIGNAL     = 2,  /* Signal Kanban (batch) */
    SMF_KANBAN_E_2BIN     = 3   /* e-Kanban with two-bin */
} SMF_KanbanType;

typedef struct {
    int     kanban_id;
    SMF_KanbanType type;
    int     material_idx;
    int     source_ws_idx;    /* source workstation */
    int     dest_ws_idx;      /* destination workstation */
    int     container_qty;
    int     num_cards;        /* number of cards in loop */
    int     cards_in_circulation;
    int     cards_at_source;
    int     cards_at_dest;
    float   trigger_qty;      /* reorder trigger */
    float   safety_kanban;    /* additional safety cards */
    time_t  last_pull;
    int     pull_frequency_hours;
} SMF_Kanban;

/* ---- L1: Demand Forecast ---- */

typedef enum {
    SMF_FORECAST_NAIVE        = 0,  /* last period = next period */
    SMF_FORECAST_MA           = 1,  /* Moving Average */
    SMF_FORECAST_WMA          = 2,  /* Weighted Moving Average */
    SMF_FORECAST_SES          = 3,  /* Simple Exponential Smoothing */
    SMF_FORECAST_HOLT         = 4,  /* Holt's double exponential */
    SMF_FORECAST_HOLT_WINTERS = 5,  /* Holt-Winters triple exponential */
    SMF_FORECAST_LINEAR_REGRESSION = 6
} SMF_ForecastMethod;

typedef struct {
    float               *history;
    int                 history_len;
    float               *forecast;
    int                 forecast_horizon;
    SMF_ForecastMethod  method;
    float               alpha;      /* level smoothing (SES) */
    float               beta;       /* trend smoothing (Holt) */
    float               gamma;      /* seasonal smoothing (HW) */
    int                 season_period;
    float               mape;       /* Mean Absolute Percentage Error */
    float               mse;        /* Mean Squared Error */
    float               mae;        /* Mean Absolute Error */
    bool                has_seasonality;
    bool                has_trend;
} SMF_DemandForecast;

/* ---- L1: ABC Classification ---- */

typedef struct {
    int     item_idx;
    float   annual_usage_value;
    float   cumulative_pct;
    char    abc_class;         /* 'A', 'B', or 'C' */
    int     rank;
} SMF_ABCItem;

/* ================================================================
 *  API: MRP (Material Requirements Planning)
 * ================================================================ */

/**
 * Initialize an MRP plan for the factory.
 * Sets up the planning horizon and item master from factory data.
 */
int  smf_mrp_init(SMF_MRPPlan *plan, const SMF_Factory *factory,
                   int planning_horizon_days, int time_bucket_days);

/**
 * Execute full MRP regeneration (Orlicky 1975).
 *
 * Steps:
 * 1. Assign low-level codes via BOM traversal
 * 2. Compute gross requirements level-by-level (top-down)
 * 3. Compute net requirements: Net = Gross - OnHand - Scheduled + Safety
 * 4. Apply lot-sizing to determine planned order releases
 * 5. Offset by lead time to get planned order releases
 *
 * Complexity: O(N * T * L) where N=items, T=periods, L=levels
 */
int  smf_mrp_regenerate(SMF_MRPPlan *plan, const SMF_Factory *factory);

/** Net requirements calculation for a single item */
int  smf_mrp_net_requirements(SMF_MRPItem *item, int num_periods);

/** Assign low-level codes (LLC) to all items in a BOM structure */
int  smf_mrp_assign_llc(const SMF_Factory *factory, int *llc_array,
                          int max_items);

/* ================================================================
 *  API: Lot Sizing (L5: Multiple Methods)
 * ================================================================ */

/**
 * Lot-for-Lot (L4L): Order exactly what is needed each period.
 * Simplest method, minimizes inventory but may have high setup costs.
 */
int  smf_lotsize_l4l(SMF_MRPItem *item, int horizon);

/**
 * Fixed Order Quantity (FOQ): Always order a fixed quantity.
 * When net requirements trigger, order the full FOQ.
 */
int  smf_lotsize_foq(SMF_MRPItem *item, int horizon, float foq);

/**
 * Period Order Quantity (POQ): Order enough for N future periods.
 * POQ = EOQ / Avg Demand — expressed in periods.
 */
int  smf_lotsize_poq(SMF_MRPItem *item, int horizon, int periods);

/**
 * Economic Order Quantity (Harris 1913):
 * EOQ = sqrt(2 * D * S / H)
 * where D=annual demand, S=setup/order cost, H=holding cost/unit/year
 */
float smf_eoq_compute(float annual_demand, float order_cost,
                       float holding_cost);

/**
 * Silver-Meal Heuristic (1973):
 * Minimize total relevant cost per period.
 * K(m) = (A + h * sum_{t=2..m} (t-1)*D_t) / m
 * Add periods until K(m+1) > K(m).
 */
int  smf_lotsize_silver_meal(const float *demands, int horizon,
                              float setup_cost, float holding_cost_per_period,
                              int *order_periods);

/* ================================================================
 *  API: Kanban System (L3: Pull System)
 * ================================================================ */

/** Calculate number of Kanban cards needed:
 *  N = (D * L * (1 + SF)) / C
 *  where D=daily demand, L=lead time, SF=safety factor, C=container capacity
 */
int  smf_kanban_calc_cards(float daily_demand, float lead_time_days,
                            float safety_factor, int container_qty);

/** Pull from Kanban loop (consume from destination) */
int  smf_kanban_pull(SMF_Factory *factory, int kanban_idx, int quantity);

/** Replenish Kanban (produce/move to destination) */
int  smf_kanban_replenish(SMF_Factory *factory, int kanban_idx, int quantity);

/** Check if Kanban needs replenishment */
bool smf_kanban_needs_replenish(const SMF_Kanban *kanban);

/* ================================================================
 *  API: Demand Forecasting (L5: Time Series)
 * ================================================================ */

/** Simple Exponential Smoothing (SES):
 *  F_{t+1} = α * D_t + (1-α) * F_t
 */
int  smf_forecast_ses(SMF_DemandForecast *fc);

/** Moving Average forecast */
int  smf_forecast_moving_avg(SMF_DemandForecast *fc, int window);

/** Weighted Moving Average */
int  smf_forecast_weighted_ma(SMF_DemandForecast *fc,
                               const float *weights, int window);

/** Holt's Double Exponential Smoothing (trend) */
int  smf_forecast_holt(SMF_DemandForecast *fc);

/** Holt-Winters Triple Exponential Smoothing (trend + seasonality) */
int  smf_forecast_holt_winters(SMF_DemandForecast *fc);

/** Compute forecast accuracy metrics (MAPE, MSE, MAE) */
int  smf_forecast_accuracy(SMF_DemandForecast *fc, const float *actuals,
                            int num_actuals);

/* ================================================================
 *  API: Inventory Analysis
 * ================================================================ */

/** ABC Analysis (Pareto principle: 80/20 rule) */
int  smf_abc_analyze(const SMF_Factory *factory,
                      SMF_ABCItem *results, int max_items);

/** Safety stock calculation:
 *  SS = Z * σ * sqrt(LT)
 *  where Z=service level Z-score, σ=demand std dev, LT=lead time
 */
float smf_safety_stock(float z_score, float demand_stddev,
                        float lead_time_days);

/** Reorder point:
 *  ROP = d * LT + SS
 *  where d=average daily demand
 */
float smf_reorder_point(float avg_daily_demand, float lead_time_days,
                         float safety_stock);

/** Inventory days of supply */
float smf_inventory_days_of_supply(float on_hand_qty, float daily_demand);

/** Stockout risk probability */
float smf_stockout_probability(float demand_mean, float demand_stddev,
                                float on_hand_qty);

/* ================================================================
 *  API: Supplier Management
 * ================================================================ */

int  smf_supplier_add(SMF_Factory *factory, const char *code,
                       const char *name, const char *country,
                       int lead_time_days);

/** Compute supplier scorecard (weighted average):
 *  Score = 0.4 * Quality + 0.3 * Delivery + 0.2 * Cost + 0.1 * Responsiveness
 */
int  smf_supplier_evaluate(SMF_Supplier *supplier);

/** Rank suppliers by scorecard */
int  smf_supplier_rank(SMF_Supplier *suppliers, int num_suppliers,
                        int *ranked_indices);

/** Find best supplier for a material */
int  smf_supplier_best_for_material(const SMF_Factory *factory,
                                     int material_idx);

/* Utility */
const char* smf_item_type_name(SMF_ItemType type);
const char* smf_forecast_method_name(SMF_ForecastMethod method);
const char* smf_kanban_type_name(SMF_KanbanType type);

#endif