/**
 * warehouse.c — Warehouse Operations Implementations
 *
 * L5: ABC slotting algorithm, S-shape pick path optimization,
 *     batch picking optimization
 * L2: Warehouse layout management, receiving, put-away
 * L4: Little's Law for warehouse queueing
 */

#include "warehouse.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================
 * L2 — Warehouse Layout Management
 * ============================================================ */
int warehouse_init(warehouse_layout_t *layout, int aisle_count,
                   const int *aisle_lengths) {
    if (!layout || aisle_count <= 0 || !aisle_lengths) return -1;
    memset(layout, 0, sizeof(warehouse_layout_t));

    layout->aisle_count = aisle_count;

    layout->aisle_lengths = (int *)malloc(aisle_count * sizeof(int));
    layout->locations = (storage_location_t **)malloc(
        aisle_count * sizeof(storage_location_t *));
    if (!layout->aisle_lengths || !layout->locations) {
        warehouse_destroy(layout);
        return -1;
    }

    for (int a = 0; a < aisle_count; a++) {
        layout->aisle_lengths[a] = aisle_lengths[a];
        layout->locations[a] = (storage_location_t *)calloc(
            aisle_lengths[a], sizeof(storage_location_t));
        if (!layout->locations[a]) {
            warehouse_destroy(layout);
            return -1;
        }
    }

    layout->total_capacity_m3 = 0.0;
    layout->total_used_m3 = 0.0;
    layout->dock_aisle = 0;
    layout->avg_picks_per_day = 0.0;
    layout->total_skus = 0;
    return 0;
}

int warehouse_add_location(warehouse_layout_t *layout,
                           int aisle, const storage_location_t *loc) {
    if (!layout || !layout->locations || aisle < 0 ||
        aisle >= layout->aisle_count || !loc) return -1;

    /* Find first empty slot in the aisle */
    for (int p = 0; p < layout->aisle_lengths[aisle]; p++) {
        if (layout->locations[aisle][p].location_id[0] == '\0') {
            memcpy(&layout->locations[aisle][p], loc, sizeof(storage_location_t));
            return 0;
        }
    }
    return -1;  /* Aisle is full */
}

int warehouse_find_empty_near(warehouse_layout_t *layout,
                               int near_aisle, location_type_t type,
                               char *location_id_out) {
    if (!layout || !layout->locations || !location_id_out) return 0;
    if (near_aisle < 0) near_aisle = 0;
    if (near_aisle >= layout->aisle_count) near_aisle = layout->aisle_count - 1;

    /* Search in expanding radius from near_aisle */
    for (int radius = 0; radius < layout->aisle_count; radius++) {
        for (int dir = 0; dir < 2; dir++) {
            int aisle = (dir == 0) ? near_aisle + radius : near_aisle - radius;
            if (aisle < 0 || aisle >= layout->aisle_count) continue;

            for (int p = 0; p < layout->aisle_lengths[aisle]; p++) {
                storage_location_t *loc = &layout->locations[aisle][p];
                if (loc->location_id[0] == '\0') continue;  /* Uninitialized */
                if (!loc->is_occupied &&
                    (type == loc->type || type == LOCATION_PALLET_RACK /* any */)) {
                    strncpy(location_id_out, loc->location_id,
                            sizeof(location_id_out) - 1);
                    location_id_out[31] = '\0';  /* Ensure null-termination */
                    return 1;
                }
            }
        }
    }
    return 0;
}

double warehouse_space_utilization(const warehouse_layout_t *layout) {
    if (!layout || layout->total_capacity_m3 <= 0.0) return 0.0;
    return (layout->total_used_m3 / layout->total_capacity_m3) * 100.0;
}

void warehouse_destroy(warehouse_layout_t *layout) {
    if (!layout) return;
    if (layout->locations) {
        for (int a = 0; a < layout->aisle_count; a++) {
            free(layout->locations[a]);
        }
        free(layout->locations);
        layout->locations = NULL;
    }
    free(layout->aisle_lengths);
    layout->aisle_lengths = NULL;
    layout->aisle_count = 0;
}

/* ============================================================
 * L5 — ABC Slotting Algorithm
 * ============================================================
 *
 * Places high-frequency (A-class) SKUs closest to the shipping
 * dock to minimize pick travel time.
 *
 * Pareto Principle: 20% of SKUs account for ~80% of picks.
 */
typedef struct {
    char   sku_id[32];
    double frequency;
} sku_freq_pair_t;

static int slot_freq_cmp_desc(const void *a, const void *b) {
    const sku_freq_pair_t *sa = (const sku_freq_pair_t *)a;
    const sku_freq_pair_t *sb = (const sku_freq_pair_t *)b;
    if (sb->frequency > sa->frequency) return 1;
    if (sb->frequency < sa->frequency) return -1;
    return 0;
}

static int slot_dist_cmp_asc(const void *a, const void *b) {
    const storage_location_t *la = *(const storage_location_t **)a;
    const storage_location_t *lb = *(const storage_location_t **)b;
    if (la->dist_to_dock_m > lb->dist_to_dock_m) return 1;
    if (la->dist_to_dock_m < lb->dist_to_dock_m) return -1;
    return 0;
}

void abc_slotting(warehouse_layout_t *layout,
                  const char (*sku_ids)[32],
                  const double *sku_freq,
                  int sku_count) {
    if (!layout || !layout->locations || !sku_ids || !sku_freq ||
        sku_count <= 0) return;

    /* Build sorted list of SKUs by frequency */
    sku_freq_pair_t *pairs = (sku_freq_pair_t *)malloc(
        sku_count * sizeof(sku_freq_pair_t));
    if (!pairs) return;

    for (int i = 0; i < sku_count; i++) {
        strncpy(pairs[i].sku_id, sku_ids[i], sizeof(pairs[i].sku_id) - 1);
        pairs[i].sku_id[sizeof(pairs[i].sku_id) - 1] = '\0';
        pairs[i].frequency = sku_freq[i];
    }

    qsort(pairs, sku_count, sizeof(sku_freq_pair_t), slot_freq_cmp_desc);

    /* Collect all empty locations sorted by distance to dock */
    int total_locs = 0;
    for (int a = 0; a < layout->aisle_count; a++) {
        for (int p = 0; p < layout->aisle_lengths[a]; p++) {
            if (layout->locations[a][p].location_id[0] != '\0') {
                total_locs++;
            }
        }
    }

    storage_location_t **sorted_locs = (storage_location_t **)malloc(
        total_locs * sizeof(storage_location_t *));
    if (!sorted_locs) { free(pairs); return; }

    int idx = 0;
    for (int a = 0; a < layout->aisle_count; a++) {
        for (int p = 0; p < layout->aisle_lengths[a]; p++) {
            if (layout->locations[a][p].location_id[0] != '\0') {
                sorted_locs[idx++] = &layout->locations[a][p];
            }
        }
    }

    qsort(sorted_locs, total_locs, sizeof(storage_location_t *),
          slot_dist_cmp_asc);

    /* Assign SKUs to locations (highest freq → nearest dock) */
    int assignments = (sku_count < total_locs) ? sku_count : total_locs;
    for (int i = 0; i < assignments; i++) {
        storage_location_t *loc = sorted_locs[i];
        strncpy(loc->sku_id, pairs[i].sku_id, sizeof(loc->sku_id) - 1);
        loc->sku_id[sizeof(loc->sku_id) - 1] = '\0';
        loc->is_occupied = 1;
        loc->pick_frequency = pairs[i].frequency;

        /* Assign ABC zone based on distance percentile */
        double pct = (double)i / (double)total_locs;
        if (pct < 0.20)      loc->abc_zone = ABC_CLASS_A;
        else if (pct < 0.50) loc->abc_zone = ABC_CLASS_B;
        else                  loc->abc_zone = ABC_CLASS_C;
    }

    free(pairs);
    free(sorted_locs);
}

/* ============================================================
 * L5 — S-Shape Pick Path Optimization
 * ============================================================
 *
 * S-shape (traversal) strategy for single-block warehouses:
 * - Start at depot (dock aisle, first position)
 * - Visit each aisle that has picks from left to right
 * - Enter each active aisle fully, exit at other end
 * - Alternate entry direction to form S-pattern
 *
 * This is optimal for warehouses with cross-aisles only at
 * the ends (Roodbergen & De Koster, 2001).
 */
double s_shape_pick_path(const warehouse_layout_t *layout,
                         pick_list_t *pick_list) {
    if (!layout || !pick_list || pick_list->item_count <= 0) return 0.0;

    /* Determine which aisles have picks */
    int *aisle_has_pick = (int *)calloc(layout->aisle_count, sizeof(int));
    if (!aisle_has_pick) return 0.0;

    for (int i = 0; i < pick_list->item_count; i++) {
        /* Extract aisle from location_id (format: A##B##L##) */
        /* Simple heuristic: search through locations to find which aisle
           contains this location_id */
        for (int a = 0; a < layout->aisle_count; a++) {
            for (int p = 0; p < layout->aisle_lengths[a]; p++) {
                if (strcmp(layout->locations[a][p].location_id,
                           pick_list->items[i].location_id) == 0) {
                    aisle_has_pick[a] = 1;
                    break;
                }
            }
        }
    }

    /* S-shape: traverse all aisles with picks */
    double total_distance = 0.0;
    int last_aisle = layout->dock_aisle;
    int direction_up = 1;  /* 1 = going up (increasing position), 0 = going down */

    double aisle_length_m = 50.0;   /* Typical aisle length */
    double cross_aisle_width_m = 3.0; /* Width between aisles */

    for (int a = 0; a < layout->aisle_count; a++) {
        if (!aisle_has_pick[a]) continue;

        /* Travel from previous aisle to this one */
        total_distance += fabs((double)(a - last_aisle)) * cross_aisle_width_m;

        /* Traverse the aisle */
        total_distance += aisle_length_m;

        direction_up = !direction_up;
        last_aisle = a;
    }

    /* Return to dock */
    total_distance += fabs((double)(last_aisle - layout->dock_aisle)) *
                      cross_aisle_width_m;

    pick_list->total_distance_m = total_distance;
    free(aisle_has_pick);
    return total_distance;
}

/* ============================================================
 * L5 — Batch Picking Optimization
 * ============================================================
 *
 * Groups single-line orders by SKU to minimize pick travel.
 * Orders for the same SKU can be batch-picked in one pass.
 *
 * Simple greedy: sort orders by SKU, group consecutive
 * matching SKUs into batches.
 */
typedef struct {
    int     order_index;
    char    sku_id[32];
} order_sku_t;

static int order_sku_cmp(const void *a, const void *b) {
    const order_sku_t *oa = (const order_sku_t *)a;
    const order_sku_t *ob = (const order_sku_t *)b;
    return strcmp(oa->sku_id, ob->sku_id);
}

void batch_picking_optimize(const warehouse_layout_t *layout,
                            const order_t *orders, int n_orders,
                            int *batches, int *n_batches) {
    if (!layout || !orders || !batches || !n_batches || n_orders <= 0) {
        if (n_batches) *n_batches = 0;
        return;
    }

    /* Extract SKU from first line of each order */
    order_sku_t *order_skus = (order_sku_t *)malloc(
        n_orders * sizeof(order_sku_t));
    if (!order_skus) {
        *n_batches = 0;
        return;
    }

    for (int i = 0; i < n_orders; i++) {
        order_skus[i].order_index = i;
        if (orders[i].line_count > 0) {
            size_t len = strlen(orders[i].lines[0].sku_id);
            if (len >= sizeof(order_skus[i].sku_id)) {
                len = sizeof(order_skus[i].sku_id) - 1;
            }
            memcpy(order_skus[i].sku_id, orders[i].lines[0].sku_id, len);
            order_skus[i].sku_id[len] = '\0';
        } else {
            order_skus[i].sku_id[0] = '\0';
        }
    }

    /* Sort by SKU */
    qsort(order_skus, n_orders, sizeof(order_sku_t), order_sku_cmp);

    /* Assign batches: consecutive same-SKU orders grouped together */
    int batch_id = 0;
    int batch_start = 0;

    for (int i = 1; i <= n_orders; i++) {
        int same_sku = (i < n_orders &&
                        strcmp(order_skus[i].sku_id,
                               order_skus[batch_start].sku_id) == 0);

        /* Limit batch size to 10 orders */
        int batch_full = (i - batch_start) >= 10;

        if (!same_sku || batch_full) {
            /* Finalize batch batch_id for orders batch_start..i-1 */
            for (int j = batch_start; j < i; j++) {
                batches[order_skus[j].order_index] = batch_id;
            }
            batch_id++;
            batch_start = i;
        }
    }

    *n_batches = batch_id;
    free(order_skus);
}

/* ============================================================
 * L2 — Receiving & Put-Away
 * ============================================================ */
void receiving_init(receiving_record_t *rec, const char *receiving_id,
                    const char *po_number, const char *supplier_id) {
    if (!rec) return;
    memset(rec, 0, sizeof(receiving_record_t));
    if (receiving_id) {
        strncpy(rec->receiving_id, receiving_id, sizeof(rec->receiving_id) - 1);
        rec->receiving_id[sizeof(rec->receiving_id) - 1] = '\0';
    }
    if (po_number) {
        strncpy(rec->po_number, po_number, sizeof(rec->po_number) - 1);
        rec->po_number[sizeof(rec->po_number) - 1] = '\0';
    }
    if (supplier_id) {
        strncpy(rec->supplier_id, supplier_id, sizeof(rec->supplier_id) - 1);
        rec->supplier_id[sizeof(rec->supplier_id) - 1] = '\0';
    }
    rec->arrival_time = time(NULL);
    rec->is_complete = 0;
}

void putaway_task_init(putaway_task_t *task, const char *task_id,
                       const char *sku_id, int quantity) {
    if (!task) return;
    memset(task, 0, sizeof(putaway_task_t));
    if (task_id) {
        strncpy(task->task_id, task_id, sizeof(task->task_id) - 1);
        task->task_id[sizeof(task->task_id) - 1] = '\0';
    }
    if (sku_id) {
        strncpy(task->sku_id, sku_id, sizeof(task->sku_id) - 1);
        task->sku_id[sizeof(task->sku_id) - 1] = '\0';
    }
    task->quantity = quantity;
    task->priority = PRIORITY_NORMAL;
    task->is_completed = 0;
    task->assigned_time = time(NULL);
}

void warehouse_kpi_init(warehouse_kpi_t *kpi) {
    if (!kpi) return;
    memset(kpi, 0, sizeof(warehouse_kpi_t));
}

/* ============================================================
 * L4 — Little's Law for Warehouse Queueing
 * ============================================================
 *
 * Little, J.D.C. (1954). "A Proof for the Queuing Formula:
 * L = λW". Operations Research, 9(3), 383-387.
 *
 * For a warehouse:
 *   L = average number of orders in process
 *   λ = order arrival rate (orders per hour)
 *   W = average order lead time (hours)
 */
double littles_law_warehouse(double arrival_rate_per_hour,
                             double avg_order_time_hours) {
    if (arrival_rate_per_hour < 0.0 || avg_order_time_hours < 0.0) return 0.0;
    return arrival_rate_per_hour * avg_order_time_hours;
}

/* ============================================================
 * L2 — Pick List Operations
 * ============================================================ */
#define PICK_INITIAL_CAP 16

void pick_list_init(pick_list_t *list, pick_strategy_t strategy) {
    if (!list) return;
    memset(list, 0, sizeof(pick_list_t));
    list->strategy = strategy;
    list->items = (pick_item_t *)malloc(PICK_INITIAL_CAP * sizeof(pick_item_t));
    if (list->items) {
        list->item_capacity = PICK_INITIAL_CAP;
    }
}

int pick_list_add_item(pick_list_t *list, const char *sku_id,
                       const char *location_id, int quantity,
                       double distance) {
    if (!list || !list->items) return -1;

    if (list->item_count >= list->item_capacity) {
        int new_cap = list->item_capacity * 2;
        pick_item_t *new_items = (pick_item_t *)realloc(list->items,
            new_cap * sizeof(pick_item_t));
        if (!new_items) return -1;
        list->items = new_items;
        list->item_capacity = new_cap;
    }

    pick_item_t *item = &list->items[list->item_count];
    memset(item, 0, sizeof(pick_item_t));
    if (sku_id) {
        strncpy(item->sku_id, sku_id, sizeof(item->sku_id) - 1);
        item->sku_id[sizeof(item->sku_id) - 1] = '\0';
    }
    if (location_id) {
        strncpy(item->location_id, location_id, sizeof(item->location_id) - 1);
        item->location_id[sizeof(item->location_id) - 1] = '\0';
    }
    item->quantity_needed = quantity;
    item->location_distance = distance;
    item->sequence = list->item_count;
    list->item_count++;
    return 0;
}

static int pick_location_cmp(const void *a, const void *b) {
    const pick_item_t *pa = (const pick_item_t *)a;
    const pick_item_t *pb = (const pick_item_t *)b;
    return strcmp(pa->location_id, pb->location_id);
}

void pick_list_sort_by_location(pick_list_t *list) {
    if (!list || !list->items) return;
    qsort(list->items, list->item_count, sizeof(pick_item_t),
          pick_location_cmp);
    for (int i = 0; i < list->item_count; i++) {
        list->items[i].sequence = i;
    }
}

void pick_list_destroy(pick_list_t *list) {
    if (!list) return;
    free(list->items);
    list->items = NULL;
    list->item_count = 0;
    list->item_capacity = 0;
}
