/**
 * warehouse.h — Warehouse Operations & Layout
 *
 * L2-L5: Warehouse management systems and slotting optimization.
 *
 * Core Concepts (L2):
 *   - Slotting: assigning SKUs to storage locations
 *   - Picking strategies: wave, batch, zone
 *   - Receiving and put-away processes
 *   - Cross-docking: direct flow-through without storage
 *
 * Algorithms (L5):
 *   - ABC slotting: high-velocity items nearest to pick/pack area
 *   - Batch picking optimization: minimize travel distance
 *   - Space utilization computation
 *   - Pick-path optimization (S-shape, return, midpoint)
 *
 * Standards (L4):
 *   - Little's Law for warehouse queueing: L = λ × W
 *   - Pareto Principle for slotting: 20% SKUs → 80% of picks
 *
 * Course Alignment:
 *   - MIT CTL.SC2x Warehouse Design and Operations
 *   - Georgia Tech ISYE 6203 Warehouse Systems
 *   - CMU 45-849 Warehousing and Distribution
 */

#ifndef WAREHOUSE_H
#define WAREHOUSE_H

#include "logistics_core.h"
#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * L1 — Warehouse Structures
 * ============================================================ */

/** Storage location type */
typedef enum {
    LOCATION_PALLET_RACK   = 0,  /* Full pallet racking */
    LOCATION_SHELF         = 1,  /* Shelving for small items */
    LOCATION_FLOW_RACK     = 2,  /* Gravity flow rack (FIFO) */
    LOCATION_BULK_FLOOR    = 3,  /* Bulk floor storage */
    LOCATION_MEZZANINE     = 4,  /* Mezzanine-level storage */
    LOCATION_COLD_STORAGE  = 5,  /* Refrigerated/frozen */
    LOCATION_HAZMAT        = 6,  /* Hazardous materials cage */
    LOCATION_CROSSDOCK     = 7,  /* Cross-dock staging lane */
    LOCATION_PICK_FACE     = 8   /* Forward pick location */
} location_type_t;

/** A single storage location (bin/slot) in the warehouse */
typedef struct {
    char            location_id[32];
    location_type_t type;
    int             aisle;             /* Aisle number */
    int             bay;               /* Bay number within aisle */
    int             level;             /* Vertical level */
    double          dist_to_dock_m;     /* Distance to shipping dock */
    double          volume_capacity_m3;
    double          weight_capacity_kg;
    double          volume_used_m3;
    int             is_occupied;
    char            sku_id[32];         /* Currently assigned SKU (empty if unoccupied) */
    int             quantity_stored;
    double          pick_frequency;     /* Average picks per day */
    abc_class_t     abc_zone;           /* Zone assignment */
} storage_location_t;

/** Picking strategy enumeration */
typedef enum {
    PICK_DISCRETE      = 0,  /* One order at a time */
    PICK_BATCH         = 1,  /* Multiple orders picked together */
    PICK_WAVE          = 2,  /* Orders grouped by time window */
    PICK_ZONE          = 3,  /* Orders picked within zones */
    PICK_CLUSTER       = 4   /* Multiple orders on one cart */
} pick_strategy_t;

/** A pick list entry */
typedef struct {
    char    sku_id[32];
    char    location_id[32];
    int     quantity_needed;
    int     quantity_picked;
    double  location_distance;  /* Distance from previous pick */
    int     sequence;           /* Order in pick path */
} pick_item_t;

/** A complete pick list */
typedef struct {
    pick_item_t     *items;
    int              item_count;
    int              item_capacity;
    pick_strategy_t  strategy;
    double           total_distance_m;  /* Estimated travel distance */
    int              order_count;       /* Number of orders in this batch */
} pick_list_t;

/** Warehouse receiving record */
typedef struct {
    char    receiving_id[32];
    char    po_number[32];        /* Purchase order reference */
    char    supplier_id[32];
    char    carrier_id[32];
    time_t  arrival_time;
    time_t  unload_start;
    time_t  unload_end;
    int     total_packages;
    int     packages_received;
    int     packages_damaged;
    int     is_complete;
    char    dock_location[32];
} receiving_record_t;

/** Put-away task */
typedef struct {
    char    task_id[32];
    char    sku_id[32];
    char    receiving_id[32];
    char    from_location[32];   /* Staging area */
    char    to_location[32];     /* Storage location */
    int     quantity;
    double  distance_m;
    int     priority;
    int     is_completed;
    time_t  assigned_time;
    time_t  completed_time;
} putaway_task_t;

/** Warehouse layout as a grid */
typedef struct {
    storage_location_t **locations;  /* 2D: locations[aisle][position] */
    int                 *aisle_lengths; /* Number of positions per aisle */
    int                  aisle_count;
    double               total_capacity_m3;
    double               total_used_m3;
    int                  dock_aisle;        /* Aisle nearest to shipping dock */
    double               avg_picks_per_day;
    int                  total_skus;        /* Distinct SKUs stored */
} warehouse_layout_t;

/* ============================================================
 * L1 — Warehouse KPIs
 * ============================================================ */

typedef struct {
    double  space_utilization_pct;     /* Used capacity / total capacity */
    double  labor_utilization_pct;     /* Productive hours / total hours */
    double  pick_accuracy_pct;         /* Correct picks / total picks */
    double  order_cycle_time_min;      /* Avg time order-to-ship */
    double  picks_per_hour;            /* Picking productivity */
    double  dock_to_stock_time_min;    /* Receipt-to-available time */
    double  perfect_order_pct;         /* Error-free order percentage */
    double  inventory_accuracy_pct;    /* System vs physical match */
    double  crossdock_pct;             /* Cross-docked vs total volume */
    double  damage_rate_pct;           /* Damaged items percentage */
} warehouse_kpi_t;

/* ============================================================
 * L5 — Slotting & Layout Algorithms
 * ============================================================ */

/**
 * Assign SKUs to storage locations using ABC slotting.
 *
 * Algorithm:
 * 1. Sort SKUs by pick frequency (descending)
 * 2. Sort locations by distance to dock (ascending)
 * 3. Assign highest-frequency SKUs to closest locations
 *
 * Pareto Principle: Place A-items (top 20% frequency →
 * 80% of picks) in the most accessible "golden zone".
 *
 * Complexity: O(n log n) where n = number of SKUs
 *
 * @param layout     Warehouse layout with locations
 * @param skus       Array of SKU IDs to assign
 * @param sku_freq   Pick frequency (picks/day) for each SKU
 * @param sku_count  Number of SKUs to assign
 */
void abc_slotting(warehouse_layout_t *layout,
                  const char (*sku_ids)[32],
                  const double *sku_freq,
                  int sku_count);

/**
 * Compute the optimal pick path using the S-shape (traversal) strategy.
 *
 * S-shape: Enter each aisle that has picks; traverse the entire aisle
 * from one end to the other for each visited aisle. The path forms
 * an S-pattern through the warehouse.
 *
 * Theorem: S-shape is optimal for single-block warehouses with
 * cross-aisles only at the ends. (Roodbergen & De Koster, 2001)
 *
 * Complexity: O(n log n) for sorting pick locations by aisle
 *
 * @param layout     Warehouse layout
 * @param pick_list  Pick list with items to pick
 * @return Estimated total travel distance in meters
 */
double s_shape_pick_path(const warehouse_layout_t *layout,
                         pick_list_t *pick_list);

/**
 * Optimize batch picking by grouping single-line orders.
 *
 * Groups orders by SKU to minimize travel — all orders needing
 * the same SKU are picked together in one pass.
 *
 * Complexity: O(n log n) where n = number of items
 *
 * @param layout    Warehouse layout
 * @param orders    Array of order IDs
 * @param n_orders  Number of orders
 * @param batches   Output: batch assignments (caller allocates n_orders ints)
 * @param n_batches Output: number of batches formed
 */
void batch_picking_optimize(const warehouse_layout_t *layout,
                            const order_t *orders, int n_orders,
                            int *batches, int *n_batches);

/* ============================================================
 * L2 — Warehouse Operations
 * ============================================================ */

/**
 * Initialize a warehouse layout with given aisle count.
 * Complexity: O(aisles * positions)
 */
int  warehouse_init(warehouse_layout_t *layout, int aisle_count,
                    const int *aisle_lengths);

/**
 * Add a storage location to the warehouse.
 * Complexity: O(1)
 */
int  warehouse_add_location(warehouse_layout_t *layout,
                            int aisle, const storage_location_t *loc);

/**
 * Find the best empty location for a SKU near a given aisle.
 * Returns 1 if found, 0 if no empty locations.
 * Complexity: O(L) where L = number of locations
 */
int  warehouse_find_empty_near(warehouse_layout_t *layout,
                               int near_aisle, location_type_t type,
                               char *location_id_out);

/**
 * Compute space utilization of the warehouse.
 * Complexity: O(n) where n = total locations
 */
double warehouse_space_utilization(const warehouse_layout_t *layout);

/**
 * Create a receiving record.
 * Complexity: O(1)
 */
void receiving_init(receiving_record_t *rec, const char *receiving_id,
                    const char *po_number, const char *supplier_id);

/**
 * Create a put-away task.
 * Complexity: O(1)
 */
void putaway_task_init(putaway_task_t *task, const char *task_id,
                       const char *sku_id, int quantity);

/**
 * Initialize warehouse KPIs to zero.
 * Complexity: O(1)
 */
void warehouse_kpi_init(warehouse_kpi_t *kpi);

/**
 * Apply Little's Law to estimate average work-in-process in the warehouse.
 *
 * Little's Law (John Little, 1954):
 *   L = λ × W
 *
 * Where:
 *   L = average number of orders in the system
 *   λ = arrival rate (orders per hour)
 *   W = average time an order spends in the system (hours)
 *
 * Complexity: O(1)
 */
double littles_law_warehouse(double arrival_rate_per_hour,
                             double avg_order_time_hours);

/**
 * Free the warehouse layout (internal location arrays).
 * Complexity: O(aisles)
 */
void warehouse_destroy(warehouse_layout_t *layout);

/**
 * Create and initialize a pick list.
 * Complexity: O(1)
 */
void pick_list_init(pick_list_t *list, pick_strategy_t strategy);

/**
 * Add an item to a pick list.
 * Complexity: O(1) amortized
 */
int  pick_list_add_item(pick_list_t *list, const char *sku_id,
                        const char *location_id, int quantity,
                        double distance);

/**
 * Sort pick list items by location for optimal pick path.
 * Complexity: O(n log n)
 */
void pick_list_sort_by_location(pick_list_t *list);

/**
 * Free pick list memory.
 * Complexity: O(1)
 */
void pick_list_destroy(pick_list_t *list);

#endif /* WAREHOUSE_H */
