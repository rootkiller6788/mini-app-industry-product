#ifndef MINI_ENT_MRP_ENGINE_H
#define MINI_ENT_MRP_ENGINE_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* ================================================================
 *  MRP Engine — Material Requirements Planning
 *
 *  L1: Core types — Item Master, BOM, work order, inventory
 *  L2: Core concepts — dependent vs independent demand, lead time,
 *      lot sizing, safety stock, MRP II (Manufacturing Resource Planning)
 *  L3: Engineering structures — BOM tree (Gozinto graph),
 *      routing/work center model, low-level coding
 *  L4: Standards — APICS body of knowledge, ISO 10303 (STEP)
 *      Just-in-Time (Toyota Production System), Theory of Constraints
 *      (Goldratt, "The Goal" 1984)
 *  L5: Algorithms — MRP explosion, lot-sizing (EOQ, L4L, POQ, PPB),
 *      backward/forward scheduling, capacity requirements planning
 *  L6: Canonical problems — Net requirements calculation, pegging
 *  L7: Applications — Discrete manufacturing, process manufacturing
 *  L8: Advanced — APS (Advanced Planning & Scheduling), DDMRP
 *  L9: Industry — SAP PP, Oracle Manufacturing, Plex
 *
 *  References:
 *  - Orlicky, "Material Requirements Planning" (1975)
 *  - Vollmann et al., "Manufacturing Planning & Control for SCM" (2005)
 *  - Goldratt & Cox, "The Goal" (1984)
 * ================================================================ */

#define MENT_MRP_MAX_ITEMS      16384
#define MENT_MRP_MAX_BOM_LINES  65536
#define MENT_MRP_MAX_WORKORDERS 32768
#define MENT_MRP_MAX_ROUTINGS   4096
#define MENT_MRP_MAX_WORKCENTERS 128
#define MENT_MRP_HORIZON_WEEKS  52

/* ---- L1: Item Master ---- */

typedef enum {
    MENT_ITEM_RAW_MATERIAL  = 0,
    MENT_ITEM_SUBASSEMBLY   = 1,
    MENT_ITEM_FINISHED_GOOD = 2,
    MENT_ITEM_PHANTOM       = 3,
    MENT_ITEM_EXPENSE       = 4,
    MENT_ITEM_TOOLING       = 5
} MENT_ItemType;

typedef enum {
    MENT_LOTSIZE_LOT_FOR_LOT    = 0,
    MENT_LOTSIZE_FIXED          = 1,
    MENT_LOTSIZE_EOQ            = 2,  /* Economic Order Quantity */
    MENT_LOTSIZE_PERIOD_ORDER   = 3,  /* POQ */
    MENT_LOTSIZE_PART_PERIOD    = 4   /* PPB */
} MENT_LotSizeMethod;

typedef struct {
    int      id;
    char     item_code[32];
    char     description[128];
    MENT_ItemType type;
    char     uom[8];            /* unit of measure */
    float    unit_cost;
    int      lead_time_days;
    int      safety_stock;
    int      min_order_qty;
    int      max_order_qty;
    MENT_LotSizeMethod lot_size_method;
    int      lot_size_qty;
    float    scrap_factor;      /* 0.0 - 1.0 */
    int      low_level_code;   /* for MRP explosion */
    bool     is_mps_item;      /* Master Production Schedule item */
    bool     is_purchased;
    bool     is_manufactured;
} MENT_Item;

/* ---- L1: Bill of Materials ---- */

typedef struct {
    int      id;
    int      parent_item_id;
    int      component_item_id;
    float    quantity_per;
    float    scrap_factor;
    int      effective_from;    /* period */
    int      effective_to;      /* period (-1 = indefinite) */
    int      operation_seq;     /* routing sequence */
    bool     is_phantom;
    bool     is_critical;
} MENT_BOMLine;

/* ---- L1: Work Center & Routing ---- */

typedef struct {
    int      id;
    char     code[16];
    char     description[64];
    float    capacity_per_day;  /* in standard hours */
    int      machines;
    int      workers;
    float    efficiency;        /* 0.0 - 1.0 */
    float    utilization;       /* 0.0 - 1.0 */
    float    cost_per_hour;
    int      queue_time_days;
} MENT_WorkCenter;

typedef struct {
    int      id;
    int      item_id;
    int      operation_seq;
    int      workcenter_id;
    float    setup_hours;
    float    run_hours_per_unit;
    float    queue_hours;
    char     instruction[128];
    bool     is_external;       /* outsourced */
} MENT_RoutingStep;

/* ---- L1: Demand & Supply ---- */

typedef struct {
    int      item_id;
    int      period;            /* week number */
    int      year;
    int      quantity;
    char     source[32];        /* "MPS", "SalesOrder#123", "Forecast" */
} MENT_Demand;

typedef struct {
    int      item_id;
    int      period;
    int      year;
    int      quantity;
    int      source_order_id;   /* work order or purchase order */
    char     type[16];          /* "WO", "PO", "OnHand", "InTransit" */
} MENT_Supply;

/* ---- L1: Work Order ---- */

typedef enum {
    MENT_WO_PLANNED     = 0,
    MENT_WO_RELEASED    = 1,
    MENT_WO_IN_PROGRESS = 2,
    MENT_WO_COMPLETED   = 3,
    MENT_WO_CLOSED      = 4
} MENT_WOStatus;

typedef struct {
    int      id;
    int      item_id;
    int      quantity;
    int      completed_qty;
    int      scrap_qty;
    time_t   release_date;
    time_t   due_date;
    time_t   start_date;
    time_t   completion_date;
    MENT_WOStatus status;
    int      priority;
    bool     is_firmed;
    char     notes[128];
} MENT_WorkOrder;

/* ---- L1: MRP Run Result ---- */

typedef struct {
    int      item_id;
    int      period;
    int      gross_requirements;
    int      scheduled_receipts;
    int      projected_on_hand;
    int      net_requirements;
    int      planned_order_receipts;
    int      planned_order_releases;
} MENT_MRPPegging;

/* ---- L1: MRP Instance ---- */

typedef struct {
    MENT_Item          items[MENT_MRP_MAX_ITEMS];
    int                num_items;
    MENT_BOMLine       bom[MENT_MRP_MAX_BOM_LINES];
    int                num_bom_lines;
    MENT_WorkOrder     work_orders[MENT_MRP_MAX_WORKORDERS];
    int                num_work_orders;
    MENT_WorkCenter    workcenters[MENT_MRP_MAX_WORKCENTERS];
    int                num_workcenters;
    MENT_RoutingStep   routings[MENT_MRP_MAX_ROUTINGS];
    int                num_routings;
    MENT_Demand        *demands;
    int                num_demands;
    MENT_Supply        *supplies;
    int                num_supplies;
    MENT_MRPPegging    *pegging;
    int                num_pegging;
    int                horizon_weeks;
    int                current_week;
    int                current_year;
} MENT_MRPInstance;

/* ---- API ---- */

MENT_MRPInstance* ment_mrp_create(int horizon_weeks);
void ment_mrp_destroy(MENT_MRPInstance *mrp);

/** Add item to master */
int  ment_mrp_add_item(MENT_MRPInstance *mrp, const char *code,
                        const char *desc, MENT_ItemType type,
                        int lead_time, int safety_stock);

/** Add BOM line */
int  ment_mrp_add_bom(MENT_MRPInstance *mrp, int parent_id,
                       int component_id, float qty_per);

/** Add work center */
int  ment_mrp_add_workcenter(MENT_MRPInstance *mrp, const char *code,
                              float capacity_per_day);

/** Add routing step */
int  ment_mrp_add_routing(MENT_MRPInstance *mrp, int item_id,
                           int seq, int wc_id, float run_hours);

/** Add demand (forecast or sales order) */
int  ment_mrp_add_demand(MENT_MRPInstance *mrp, int item_id,
                          int period, int qty);

/** Add supply (on-hand, PO, WO) */
int  ment_mrp_add_supply(MENT_MRPInstance *mrp, int item_id,
                          int period, int qty, const char *type);

/**
 * MRP Explosion (L5: Core MRP Algorithm)
 *
 * Processes all items by low-level code (0=finished goods first).
 * For each item, in each period:
 *   NetReq[t] = max(0, GrossReq[t] - ProjOnHand[t-1] - SchedRec[t])
 *   PlannedOrders[t - lead_time] = NetReq[t] (lot-sized)
 *   ProjOnHand[t] = ProjOnHand[t-1] + SchedRec[t] + PlannedRec[t] - GrossReq[t]
 *
 * Then explodes planned orders into gross requirements for components.
 *
 * Reference: Orlicky (1975), Vollmann et al. (2005)
 * Complexity: O(I * H * B) where I=items, H=horizon, B=BOM depth
 */
int  ment_mrp_explode(MENT_MRPInstance *mrp);

/** Get pegging for an item (trace up to parent demand) */
int  ment_mrp_get_pegging(const MENT_MRPInstance *mrp, int item_id,
                           MENT_MRPPegging *pegging, int max_periods);

/** Create planned work orders from MRP output */
int  ment_mrp_generate_work_orders(MENT_MRPInstance *mrp);

/**
 * Lot Sizing Algorithms (L5):
 * - EOQ: sqrt(2*D*S/H) where D=annual demand, S=setup cost, H=holding cost
 * - L4L: order exactly net requirements
 * - POQ: combine N periods of requirements
 * - PPB: combine periods until setup cost = holding cost
 */
int  ment_mrp_lot_size(MENT_LotSizeMethod method, int *net_req, int periods,
                        float setup_cost, float holding_cost,
                        int *planned_orders, int lead_time);

/**
 * Capacity Requirements Planning (CRP).
 * For each work center, compares load (planned order hours) to capacity.
 * Returns overloaded periods.
 */
int  ment_mrp_capacity_check(const MENT_MRPInstance *mrp,
                              int wc_id, float *load, float *capacity,
                              int max_periods);

/** Compute total inventory value */
float ment_mrp_inventory_value(const MENT_MRPInstance *mrp);

/** Compute total WIP value */
float ment_mrp_wip_value(const MENT_MRPInstance *mrp);

/** Low-Level Code assignment (pre-processing for MRP) */
void ment_mrp_assign_low_level_codes(MENT_MRPInstance *mrp);

#endif /* MINI_ENT_MRP_ENGINE_H */
