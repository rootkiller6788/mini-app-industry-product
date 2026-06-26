/**
 * example_warehouse.c — Warehouse Operations Example
 *
 * L6: Demonstrates warehouse layout design, ABC slotting,
 *     pick path optimization, batch picking, receiving,
 *     and Little's Law for warehouse WIP analysis.
 *
 * Scenario: A 5-aisle warehouse with 20 storage locations
 * per aisle, serving both retail and e-commerce channels.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "logistics_core.h"
#include "warehouse.h"

int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║     WAREHOUSE OPERATIONS EXAMPLE                         ║\n");
    printf("║     Slotting · Pick Path · Batch · Receiving · KPI       ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* ── Part 1: Warehouse Layout ── */
    printf("─── Part 1: Warehouse Layout Design ───\n\n");

    int aisle_lengths[] = {10, 12, 12, 10, 8};
    warehouse_layout_t layout;
    warehouse_init(&layout, 5, aisle_lengths);

    /* Add storage locations */
    for (int a = 0; a < layout.aisle_count; a++) {
        for (int p = 0; p < layout.aisle_lengths[a]; p++) {
            storage_location_t loc;
            memset(&loc, 0, sizeof(loc));
            snprintf(loc.location_id, sizeof(loc.location_id),
                     "A%02dB%02dL01", a + 1, p + 1);
            loc.type = (p < 2) ? LOCATION_PICK_FACE :
                       (p < 6) ? LOCATION_PALLET_RACK : LOCATION_SHELF;
            loc.aisle = a;
            loc.bay = p;
            loc.level = 1;
            loc.dist_to_dock_m = 5.0 + fabs((double)(a - layout.dock_aisle)) * 25.0;
            loc.volume_capacity_m3 = 2.0;
            loc.weight_capacity_kg = 500.0;
            loc.volume_used_m3 = 0.0;
            loc.is_occupied = 0;

            warehouse_add_location(&layout, a, &loc);
        }
    }

    printf("  Warehouse Layout:\n");
    printf("    Aisles:      %d\n", layout.aisle_count);
    printf("    Total Bays:  %d\n",
           aisle_lengths[0] + aisle_lengths[1] + aisle_lengths[2] +
           aisle_lengths[3] + aisle_lengths[4]);
    printf("    Dock Aisle:  %d\n", layout.dock_aisle);
    printf("\n");

    /* ── Part 2: ABC Slotting ── */
    printf("─── Part 2: ABC Slotting Optimization ───\n\n");

    char sku_ids[15][32];
    double sku_freq[15];

    /* Create 15 SKUs with varying pick frequencies */
    const char *sku_names[] = {
        "SKU-AAA", "SKU-AAB", "SKU-AAC", "SKU-AAD", "SKU-AAE",
        "SKU-BBA", "SKU-BBB", "SKU-BBC", "SKU-BBD", "SKU-BBE",
        "SKU-CCA", "SKU-CCB", "SKU-CCC", "SKU-CCD", "SKU-CCE"
    };
    double freqs[] = {
        150.0, 120.0, 100.0,   /* A items: high velocity */
        45.0,  40.0,  35.0,  30.0,  25.0,   /* B items: medium */
        8.0,   6.0,   5.0,   4.0,   3.0,   2.0,  1.0   /* C items: slow */
    };

    for (int i = 0; i < 15; i++) {
        strncpy(sku_ids[i], sku_names[i], 31);
        sku_freq[i] = freqs[i];
    }

    abc_slotting(&layout, sku_ids, sku_freq, 15);

    printf("  ABC Slotting Results:\n");
    printf("  %-12s %8s %10s %8s\n", "SKU", "Freq/day", "Distance m", "Zone");
    printf("  %-12s %8s %10s %8s\n", "────────", "────────", "──────────", "────────");

    /* Show first few slotted items */
    int shown = 0;
    for (int a = 0; a < layout.aisle_count && shown < 12; a++) {
        for (int p = 0; p < layout.aisle_lengths[a] && shown < 12; p++) {
            storage_location_t *loc = &layout.locations[a][p];
            if (loc->is_occupied && loc->sku_id[0] != '\0') {
                const char *zone = (loc->abc_zone == ABC_CLASS_A) ? "A (Gold)" :
                                   (loc->abc_zone == ABC_CLASS_B) ? "B (Silver)" : "C (Bronze)";
                printf("  %-12s %8.0f %9.1fm %8s\n",
                       loc->sku_id, loc->pick_frequency,
                       loc->dist_to_dock_m, zone);
                shown++;
            }
        }
    }
    printf("\n  ✓ High-velocity A-items placed closest to shipping dock\n\n");

    /* ── Part 3: Pick Path Optimization ── */
    printf("─── Part 3: Pick Path Optimization (S-Shape) ───\n\n");

    pick_list_t pick_list;
    pick_list_init(&pick_list, PICK_WAVE);

    /* Add diverse pick items across warehouse */
    const char *pick_locs[] = {"A01B01L01", "A01B03L01", "A02B05L01",
                                "A03B02L01", "A04B07L01", "A05B01L01"};
    const char *pick_skus[] = {"SKU-AAA", "SKU-AAB", "SKU-BBA",
                                "SKU-AAC", "SKU-CCB", "SKU-CCC"};
    for (int i = 0; i < 6; i++) {
        pick_list_add_item(&pick_list, pick_skus[i], pick_locs[i],
                           (i + 1) * 2, 0.0);
    }

    double distance = s_shape_pick_path(&layout, &pick_list);

    printf("  Pick List: %d items across %d aisles\n",
           pick_list.item_count, layout.aisle_count);
    printf("  Strategy: S-Shape (traversal)\n");
    printf("  Estimated travel distance: %.1f meters\n", distance);
    printf("\n");

    pick_list_destroy(&pick_list);

    /* ── Part 4: Little's Law ── */
    printf("─── Part 4: Little's Law for Warehouse Queueing ───\n\n");
    printf("  Little's Law: L = λ × W\n\n");

    double scenarios[][2] = {
        {20.0, 2.0},    /* Peak: 20 orders/hr, 2hr cycle */
        {15.0, 1.5},    /* Normal */
        {8.0,  1.0},    /* Off-peak */
    };
    const char *scene_names[] = {"Peak", "Normal", "Off-peak"};

    for (int i = 0; i < 3; i++) {
        double wip = littles_law_warehouse(scenarios[i][0], scenarios[i][1]);
        printf("  %-10s: λ=%.0f orders/hr, W=%.1f hrs → L=%.0f orders in process\n",
               scene_names[i], scenarios[i][0], scenarios[i][1], wip);
    }
    printf("\n");

    /* ── Part 5: Receiving & Put-away ── */
    printf("─── Part 5: Warehouse KPIs ───\n\n");

    warehouse_kpi_t kpi;
    warehouse_kpi_init(&kpi);
    kpi.space_utilization_pct = 72.5;
    kpi.pick_accuracy_pct = 99.2;
    kpi.order_cycle_time_min = 45.0;
    kpi.picks_per_hour = 120.0;
    kpi.inventory_accuracy_pct = 98.7;
    kpi.damage_rate_pct = 0.15;

    printf("  Warehouse Performance Dashboard:\n");
    printf("    Space Utilization:     %5.1f%%\n", kpi.space_utilization_pct);
    printf("    Pick Accuracy:         %5.1f%%\n", kpi.pick_accuracy_pct);
    printf("    Order Cycle Time:      %5.0f min\n", kpi.order_cycle_time_min);
    printf("    Picks Per Hour:        %5.0f\n", kpi.picks_per_hour);
    printf("    Inventory Accuracy:    %5.1f%%\n", kpi.inventory_accuracy_pct);
    printf("    Damage Rate:           %5.2f%%\n\n", kpi.damage_rate_pct);

    warehouse_destroy(&layout);

    printf("══════════════════════════════════════════════════════════════\n");
    printf("  Warehouse Operations Example Complete\n");
    printf("══════════════════════════════════════════════════════════════\n\n");

    return 0;
}
