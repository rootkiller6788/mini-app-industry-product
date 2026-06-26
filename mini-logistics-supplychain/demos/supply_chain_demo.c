/**
 * supply_chain_demo.c — End-to-End Supply Chain Demo
 *
 * L6/L7: Demonstrates a complete supply chain flow:
 *   Forecasting → Inventory Planning → Order Fulfillment →
 *   Route Optimization → Delivery → KPI Dashboard
 *
 * Application: Regional distribution network with demand
 * forecasting, optimal inventory replenishment, route planning,
 * and sustainability metrics.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "logistics_core.h"
#include "inventory.h"
#include "transportation.h"
#include "demand_forecast.h"
#include "scm_optimize.h"

int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║     SUPPLY CHAIN END-TO-END DEMO                            ║\n");
    printf("║     Forecast → Plan → Execute → Monitor                     ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    /* ══════════════════════════════════════════════════════════
     * PHASE 1: DEMAND FORECASTING
     * ══════════════════════════════════════════════════════════ */
    printf("┌─ PHASE 1: Demand Forecasting ──────────────────────────┐\n");

    /* Historical demand for a seasonal product */
    double history[] = {
        120, 135, 142, 138,  /* Q1 */
        150, 165, 180, 190,  /* Q2 peak season */
        170, 155, 145, 140,  /* Q3 */
        125, 118, 110, 108   /* Q4 */
    };

    /* Fit Holt-Winters with quarterly seasonality (mult) */
    holt_winters_model_t hw;
    holt_winters_init(&hw, 0.2, 0.1, 0.1, 4, 1);
    holt_winters_fit_seasonal(&hw, history, 16);

    /* Update with all history */
    for (int i = 0; i < 16; i++) {
        holt_winters_update(&hw, history[i]);
    }

    printf("  Historical Demand (quarterly seasonal):\n  ");
    for (int i = 0; i < 16; i++) {
        printf("%4.0f ", history[i]);
        if ((i + 1) % 4 == 0) printf("\n  ");
    }
    printf("\n");

    printf("  Holt-Winters Forecast (next 4 periods):\n");
    double total_forecast = 0.0;
    for (int k = 1; k <= 4; k++) {
        double f = holt_winters_forecast_k(&hw, k);
        total_forecast += f;
        printf("    Period %d: %.0f units\n", k, f);
    }
    printf("  Total next quarter forecast: %.0f units\n\n", total_forecast);

    holt_winters_destroy(&hw);
    printf("└────────────────────────────────────────────────────────┘\n\n");

    /* ══════════════════════════════════════════════════════════
     * PHASE 2: INVENTORY PLANNING
     * ══════════════════════════════════════════════════════════ */
    printf("┌─ PHASE 2: Inventory Planning ──────────────────────────┐\n");

    /* EOQ for a key SKU */
    eoq_params_t eoq_p;
    memset(&eoq_p, 0, sizeof(eoq_p));
    eoq_p.annual_demand = 48000.0;
    eoq_p.ordering_cost = 120.0;
    eoq_p.holding_cost_per_unit = 4.0;
    eoq_p.unit_cost = 35.0;
    eoq_p.lead_time_days = 7.0;
    eoq_p.working_days_per_year = 300;

    eoq_result_t eoq_r = eoq_compute(&eoq_p);

    printf("  Product: Premium Widget\n");
    printf("  EOQ (Q*):           %.0f units\n", eoq_r.eoq);
    printf("  Avg Inventory:      %.0f units\n", eoq_r.avg_inventory);
    printf("  Orders/year:        %.1f\n", eoq_r.orders_per_year);
    printf("  Total Annual Cost:  $%.0f\n", eoq_r.total_annual_cost);

    /* Safety stock */
    safety_stock_params_t ss_p;
    memset(&ss_p, 0, sizeof(ss_p));
    ss_p.avg_daily_demand = eoq_p.annual_demand / eoq_p.working_days_per_year;
    ss_p.stddev_daily_demand = 25.0;
    ss_p.avg_lead_time_days = eoq_p.lead_time_days;
    ss_p.service_level = 0.975;

    int ss = safety_stock_compute(&ss_p);
    int rop = reorder_point_compute(ss_p.avg_daily_demand,
                                     ss_p.avg_lead_time_days, ss);

    printf("  Safety Stock (97.5%%): %d units\n", ss);
    printf("  Reorder Point:       %d units\n", rop);
    printf("  Order Policy: When OH ≤ %d, order %d units\n\n",
           rop, (int)eoq_r.eoq);
    printf("└────────────────────────────────────────────────────────┘\n\n");

    /* ══════════════════════════════════════════════════════════
     * PHASE 3: ORDER FULFILLMENT
     * ══════════════════════════════════════════════════════════ */
    printf("┌─ PHASE 3: Order Fulfillment ───────────────────────────┐\n");

    order_t order;
    order_create(&order, "ORD-2024-001", "CUST-ACME");
    order_add_line(&order, "SKU-WIDGET", 50, 99.99);
    order_add_line(&order, "SKU-GADGET", 30, 149.99);
    order_add_line(&order, "SKU-ACCESSORY", 100, 19.99);

    printf("  Order ID:    %s\n", order.order_id);
    printf("  Customer:    %s\n", order.customer_id);
    printf("  Lines:       %d\n", order.line_count);
    for (int i = 0; i < order.line_count; i++) {
        printf("    %-16s ×%4d  @ $%7.2f  = $%8.2f\n",
               order.lines[i].sku_id,
               order.lines[i].quantity_ordered,
               order.lines[i].unit_price,
               order.lines[i].line_total);
    }
    printf("  Subtotal:    $%.2f\n", order.subtotal);
    printf("  Status:      ORDER_CREATED\n\n");

    order_destroy(&order);
    printf("└────────────────────────────────────────────────────────┘\n\n");

    /* ══════════════════════════════════════════════════════════
     * PHASE 4: TRANSPORTATION & ROUTE PLANNING
     * ══════════════════════════════════════════════════════════ */
    printf("┌─ PHASE 4: Route Planning ──────────────────────────────┐\n");

    facility_t dc;
    facility_init(&dc, "DC-01", "Central DC", FACILITY_DISTRIBUTION,
                  40.7128, -74.0060);

    transport_graph_t net;
    transport_graph_init(&net, 5);
    transport_graph_set_facility(&net, 0, &dc);

    facility_t dests[4];
    facility_init(&dests[0], "STORE-A", "Store Alpha", FACILITY_RETAIL,
                  40.7580, -73.9855);
    facility_init(&dests[1], "STORE-B", "Store Beta", FACILITY_RETAIL,
                  40.6892, -74.0445);
    facility_init(&dests[2], "STORE-C", "Store Gamma", FACILITY_RETAIL,
                  40.7831, -73.9712);
    facility_init(&dests[3], "STORE-D", "Store Delta", FACILITY_RETAIL,
                  40.7282, -73.7949);

    for (int i = 0; i < 4; i++) {
        transport_graph_set_facility(&net, i + 1, &dests[i]);
    }

    /* Route distances from DC */
    for (int i = 0; i < 4; i++) {
        double d = haversine_km(dc.location.latitude, dc.location.longitude,
                                dests[i].location.latitude,
                                dests[i].location.longitude);
        transport_graph_add_edge(&net, 0, i + 1, d, d / 40.0 * 60.0, d * 1.5,
                                 SHIP_MODE_TRUCKLOAD);
        transport_graph_add_edge(&net, i + 1, 0, d, d / 40.0 * 60.0, d * 1.5,
                                 SHIP_MODE_TRUCKLOAD);
    }

    printf("  Delivery Route (Nearest Neighbor TSP from DC):\n");

    int nodes[] = {0, 1, 2, 3, 4};
    int tour[6];
    double tour_dist = tsp_nearest_neighbor(&net, nodes, 5, tour);

    printf("  Route: ");
    for (int i = 0; i < 6; i++) {
        const char *name = (tour[i] == 0) ? "DC" :
            dests[tour[i] - 1].name;
        printf("%s%s", name, (i < 5) ? " → " : "");
    }
    printf("\n  Total distance: %.1f km\n", tour_dist);

    /* 2-opt improvement */
    double improved = tsp_two_opt(&net, tour, 5);
    double improved_dist = 0.0;
    for (int i = 0; i < 5; i++) {
        geo_location_t *a = &net.locations[tour[i]];
        geo_location_t *b = &net.locations[tour[i + 1]];
        improved_dist += haversine_km(a->latitude, a->longitude,
                                       b->latitude, b->longitude);
    }
    printf("  After 2-opt:  %.1f km (saved %.1f km)\n\n", improved_dist, improved);

    transport_graph_destroy(&net);
    printf("└────────────────────────────────────────────────────────┘\n\n");

    /* ══════════════════════════════════════════════════════════
     * PHASE 5: RISK & SUSTAINABILITY
     * ══════════════════════════════════════════════════════════ */
    printf("┌─ PHASE 5: Risk Pooling & Sustainability ───────────────┐\n");

    double ss_consolidated;
    double savings = risk_pooling_savings(9, 500.0, &ss_consolidated);

    printf("  Before: 9 regional DCs × 500 units = 4500 units total SS\n");
    printf("  After:  1 central DC → %.0f units total SS\n", ss_consolidated);
    printf("  Square Root Law savings: %.1f%%\n\n", savings * 100.0);

    /* CO2 comparison */
    printf("  CO2 Comparison (10 tonnes, 500 km):\n");
    printf("    Truck:  %.0f kg CO2\n",
           co2_emissions(SHIP_MODE_TRUCKLOAD, 10.0, 500.0));
    printf("    Rail:   %.0f kg CO2\n",
           co2_emissions(SHIP_MODE_INTERMODAL, 10.0, 500.0));
    printf("    Ocean:  %.0f kg CO2\n\n",
           co2_emissions(SHIP_MODE_OCEAN_FREIGHT, 10.0, 500.0));

    printf("└────────────────────────────────────────────────────────┘\n\n");

    /* ══════════════════════════════════════════════════════════
     * PHASE 6: KPI DASHBOARD
     * ══════════════════════════════════════════════════════════ */
    printf("┌─ PHASE 6: Supply Chain KPI Dashboard ──────────────────┐\n");

    sc_kpi_t kpi;
    sc_kpi_init(&kpi);
    kpi.otif_rate            = 94.5;
    kpi.fill_rate            = 97.2;
    kpi.inventory_turnover   = 8.5;
    kpi.days_inventory_oh    = 42.9;
    kpi.on_time_delivery_pct = 96.1;
    kpi.cost_per_shipment    = 245.0;
    kpi.utilization_pct      = 82.3;
    kpi.cost_per_km          = 1.45;
    kpi.total_co2_tonnes     = 1520.0;

    printf("  Service Level:\n");
    printf("    OTIF Rate:            %5.1f%%\n", kpi.otif_rate);
    printf("    Fill Rate:            %5.1f%%\n", kpi.fill_rate);
    printf("    On-Time Delivery:     %5.1f%%\n", kpi.on_time_delivery_pct);
    printf("  Inventory:\n");
    printf("    Inventory Turnover:   %5.1f turns/yr\n", kpi.inventory_turnover);
    printf("    Days Inventory OH:    %5.1f days\n", kpi.days_inventory_oh);
    printf("  Transportation:\n");
    printf("    Cost per Shipment:    $%6.2f\n", kpi.cost_per_shipment);
    printf("    Cost per KM:          $%6.2f\n", kpi.cost_per_km);
    printf("    Vehicle Utilization:  %5.1f%%\n", kpi.utilization_pct);
    printf("  Sustainability:\n");
    printf("    CO2 Emissions:        %.0f tonnes\n\n", kpi.total_co2_tonnes);

    printf("└────────────────────────────────────────────────────────┘\n\n");

    printf("══════════════════════════════════════════════════════════════\n");
    printf("  Supply Chain Demo Complete — All Phases Executed\n");
    printf("══════════════════════════════════════════════════════════════\n\n");

    return 0;
}
