/**
 * example_inventory.c — Inventory Management Example
 *
 * L6: Demonstrates EOQ computation, safety stock, ABC classification,
 *     FIFO/LIFO cost layering, and newsvendor model in a realistic
 *     inventory management scenario.
 *
 * Scenario: A distribution center managing 10 SKUs with varying
 * demand patterns. Shows how to determine optimal order quantities,
 * classify inventory, and track costs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "logistics_core.h"
#include "inventory.h"

int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║     INVENTORY MANAGEMENT EXAMPLE                         ║\n");
    printf("║     EOQ · Safety Stock · ABC · FIFO/LIFO · Newsvendor    ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* ── Part 1: Economic Order Quantity ── */
    printf("─── Part 1: Economic Order Quantity (EOQ) ───\n\n");

    eoq_params_t params;
    memset(&params, 0, sizeof(params));
    params.annual_demand = 12000.0;
    params.ordering_cost = 50.0;
    params.holding_cost_per_unit = 2.0;
    params.unit_cost = 25.0;
    params.lead_time_days = 5.0;
    params.working_days_per_year = 250;

    eoq_result_t result = eoq_compute(&params);

    printf("  Parameters:\n");
    printf("    Annual Demand (D):     %10.0f units\n", params.annual_demand);
    printf("    Ordering Cost (S):     $%9.2f\n", params.ordering_cost);
    printf("    Holding Cost (H):      $%9.2f /unit/yr\n", params.holding_cost_per_unit);
    printf("    Purchase Cost (C):     $%9.2f /unit\n", params.unit_cost);
    printf("    Lead Time:             %10.0f days\n", params.lead_time_days);
    printf("\n");

    printf("  EOQ Formula: Q* = sqrt(2·D·S / H)\n");
    printf("             = sqrt(2 × %.0f × %.0f / %.0f)\n",
           params.annual_demand, params.ordering_cost, params.holding_cost_per_unit);
    printf("             = sqrt(%.0f)\n", 2.0 * params.annual_demand * params.ordering_cost / params.holding_cost_per_unit);
    printf("\n");
    printf("  Results:\n");
    printf("    EOQ (Q*):              %10.1f units\n", result.eoq);
    printf("    Avg Inventory (Q*/2):  %10.1f units\n", result.avg_inventory);
    printf("    Orders per Year (N):  %10.2f\n", result.orders_per_year);
    printf("    Cycle Time:            %10.1f days\n", result.cycle_time_days);
    printf("    Total Annual Cost:     $%9.2f\n", result.total_annual_cost);
    printf("    Reorder Point (ROP):  %10.0f units\n\n", result.reorder_point);

    /* ── Part 2: Safety Stock ── */
    printf("─── Part 2: Safety Stock Calculation ───\n\n");

    safety_stock_params_t ss_params;
    memset(&ss_params, 0, sizeof(ss_params));
    ss_params.avg_daily_demand = 50.0;
    ss_params.stddev_daily_demand = 10.0;
    ss_params.avg_lead_time_days = 5.0;
    ss_params.stddev_lead_time_days = 0.0;
    ss_params.service_level = 0.95;

    int ss = safety_stock_compute(&ss_params);
    int rop = reorder_point_compute(50.0, 5.0, ss);

    printf("  Formula: SS = Z_α · σ_d · √(LT)\n");
    printf("    Z_α (95%%):             %.3f\n", z_score_for_service_level(0.95));
    printf("    σ_d (daily std dev):   %.0f units\n", ss_params.stddev_daily_demand);
    printf("    √(LT):                 %.3f\n", sqrt(5.0));
    printf("\n");
    printf("  Safety Stock:            %d units\n", ss);
    printf("  Reorder Point (ROP):     %d units\n", rop);
    printf("  (When inventory ≤ %d, reorder EOQ of %.0f units)\n\n", rop, result.eoq);

    /* ── Part 3: ABC Classification ── */
    printf("─── Part 3: ABC Classification (Pareto Principle) ───\n\n");

    abc_item_t items[10];
    const char *names[] = {
        "SKU-100 (Premium Widget)", "SKU-200 (Standard Widget)",
        "SKU-300 (Fastener Kit)",    "SKU-400 (Lubricant Gallon)",
        "SKU-500 (Seal Kit)",        "SKU-600 (Bearing Set)",
        "SKU-700 (O-ring Pack)",     "SKU-800 (Gasket Sheet)",
        "SKU-900 (Nut/Bolt Set)",    "SKU-010 (Spring Pack)"
    };
    double values[] = {12000, 8500, 3200, 2900, 1100, 980, 540, 320, 180, 80};

    for (int i = 0; i < 10; i++) {
        strncpy(items[i].sku_id, names[i], sizeof(items[i].sku_id) - 1);
        items[i].annual_usage_value = values[i];
    }

    abc_classify(items, 10, 0.80, 0.95);

    printf("  %-40s %12s %10s %s\n", "SKU", "Ann. Value", "Cumul %%", "Class");
    printf("  %-40s %12s %10s %s\n",
           "────────────────────────────────────────",
           "────────────", "──────────", "─────");
    for (int i = 0; i < 10; i++) {
        const char *class_str = (items[i].classification == ABC_CLASS_A) ? "A ⭐" :
                                (items[i].classification == ABC_CLASS_B) ? "B  " : "C  ";
        printf("  %-40s $%11.0f %9.1f%% %s\n",
               items[i].sku_id, items[i].annual_usage_value,
               items[i].cumulative_pct * 100.0, class_str);
    }
    printf("\n  ✓ Top 20%% of SKUs → ~80%% of inventory value (Pareto Principle)\n\n");

    /* ── Part 4: FIFO vs LIFO Cost Layering ── */
    printf("─── Part 4: FIFO vs LIFO Cost Layering ───\n\n");

    cost_layer_stack_t fifo_stack, lifo_stack;
    cost_layer_stack_init(&fifo_stack);
    cost_layer_stack_init(&lifo_stack);

    /* Simulate 3 purchase lots in a period of rising prices */
    printf("  Purchase History (rising prices):\n");
    printf("    Lot 1: 100 units @ $10.00\n");
    printf("    Lot 2:  80 units @ $12.00\n");
    printf("    Lot 3:  60 units @ $15.00\n");
    printf("    Total: 240 units, Avg Cost: $%.2f\n\n", 3600.0/240.0);

    cost_layer_push(&fifo_stack, 100, 10.0, "LOT-001");
    cost_layer_push(&fifo_stack, 80, 12.0, "LOT-002");
    cost_layer_push(&fifo_stack, 60, 15.0, "LOT-003");

    cost_layer_push(&lifo_stack, 100, 10.0, "LOT-001");
    cost_layer_push(&lifo_stack, 80, 12.0, "LOT-002");
    cost_layer_push(&lifo_stack, 60, 15.0, "LOT-003");

    /* Sell 150 units */
    double fifo_cogs, lifo_cogs;
    cost_layer_draw_fifo(&fifo_stack, 150, &fifo_cogs);
    cost_layer_draw_lifo(&lifo_stack, 150, &lifo_cogs);

    printf("  After selling 150 units:\n");
    printf("    FIFO COGS: $%.2f (oldest costs first)\n", fifo_cogs);
    printf("    LIFO COGS: $%.2f (newest costs first)\n", lifo_cogs);
    printf("    Difference: $%.2f (LIFO reflects current replacement cost)\n\n",
           lifo_cogs - fifo_cogs);

    printf("  Remaining Inventory:\n");
    printf("    FIFO avg cost: $%.2f (90 units, newer lots)\n",
           cost_layer_avg_cost(&fifo_stack));
    printf("    LIFO avg cost: $%.2f (90 units, older lots)\n\n",
           cost_layer_avg_cost(&lifo_stack));

    cost_layer_stack_destroy(&fifo_stack);
    cost_layer_stack_destroy(&lifo_stack);

    /* ── Part 5: Newsvendor Model ── */
    printf("─── Part 5: Newsvendor Model (Single-Period) ───\n\n");

    printf("  Scenario: Fashion retailer ordering seasonal jackets.\n");
    printf("    Demand ~ N(500, 100)\n");
    printf("    Selling price:  $120  |  Purchase cost: $50\n");
    printf("    Salvage value:  $20   (unsold items)\n\n");

    double cu = 120.0 - 50.0;   /* Underage: profit lost = 70 */
    double co = 50.0 - 20.0;    /* Overage: net loss = 30 */
    double q_opt = newsvendor_optimal_qty(500.0, 100.0, cu, co);

    printf("    Cu (underage) = $%.0f (profit per missed sale)\n", cu);
    printf("    Co (overage)  = $%.0f (loss per unsold item)\n", co);
    printf("    Critical ratio = %.3f\n", cu / (cu + co));
    printf("    Optimal order  = %.0f units\n", q_opt);
    printf("    (Covers ~%.1f%% of demand scenarios)\n\n", (cu/(cu+co))*100.0);

    printf("══════════════════════════════════════════════════════════════\n");
    printf("  Inventory Management Example Complete\n");
    printf("══════════════════════════════════════════════════════════════\n\n");

    return 0;
}
