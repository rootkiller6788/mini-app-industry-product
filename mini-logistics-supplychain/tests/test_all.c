/**
 * test_all.c — Comprehensive tests for mini-logistics-supplychain
 *
 * Tests cover all core APIs: L1-L5 functionality verification
 * using assert-based testing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "logistics_core.h"
#include "inventory.h"
#include "transportation.h"
#include "warehouse.h"
#include "demand_forecast.h"
#include "scm_optimize.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  TEST %s ... ", name); \
} while(0)

#define PASS() do { \
    printf("PASS\n"); \
    tests_passed++; \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
    tests_failed++; \
} while(0)

#define ASSERT_EQ_INT(a, b, msg) do { \
    if ((a) != (b)) { FAIL(msg); return; } \
} while(0)

#define ASSERT_DOUBLE_EQ(a, b, tol, msg) do { \
    if (fabs((a) - (b)) > (tol)) { \
        char buf[256]; \
        snprintf(buf, sizeof(buf), "%s (expected %.6f, got %.6f)", msg, b, a); \
        FAIL(buf); return; \
    } \
} while(0)

#define ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while(0)

/* ============================================================
 * L1 — Core Definitions Tests
 * ============================================================ */
static void test_sku_init(void) {
    TEST("sku_init");
    sku_t sku;
    sku_init(&sku, "SKU001", "Test Product");
    ASSERT_EQ_INT(0, strcmp(sku.sku_id, "SKU001"), "sku_id mismatch");
    ASSERT_EQ_INT(0, strcmp(sku.uom, "EA"), "uom default mismatch");
    ASSERT_EQ_INT(ABC_CLASS_C, sku.abc_class, "abc_class default");
    ASSERT_EQ_INT(1, sku.min_order_qty, "min_order_qty default");
    PASS();
}

static void test_order_lifecycle(void) {
    TEST("order_lifecycle");
    order_t order;
    int rc = order_create(&order, "ORD-001", "CUST-001");
    ASSERT_TRUE(rc == 0, "order_create failed");
    ASSERT_EQ_INT(ORDER_CREATED, order.status, "initial status");
    ASSERT_EQ_INT(0, order.line_count, "initial line count");

    rc = order_add_line(&order, "SKU-A", 5, 10.0);
    ASSERT_TRUE(rc == 0, "order_add_line failed");
    ASSERT_EQ_INT(1, order.line_count, "line count after add");
    ASSERT_DOUBLE_EQ(50.0, order.subtotal, 0.001, "subtotal");

    rc = order_add_line(&order, "SKU-B", 3, 20.0);
    ASSERT_TRUE(rc == 0, "second line add");
    ASSERT_EQ_INT(2, order.line_count, "two lines");
    ASSERT_DOUBLE_EQ(110.0, order.subtotal, 0.001, "subtotal2");

    order_destroy(&order);
    PASS();
}

static void test_facility_init(void) {
    TEST("facility_init");
    facility_t fac;
    facility_init(&fac, "DC-01", "Main Warehouse", FACILITY_DISTRIBUTION,
                  34.0522, -118.2437);
    ASSERT_EQ_INT(0, strcmp(fac.facility_id, "DC-01"), "facility_id");
    ASSERT_EQ_INT(FACILITY_DISTRIBUTION, fac.type, "facility type");
    ASSERT_DOUBLE_EQ(34.0522, fac.location.latitude, 0.0001, "latitude");
    PASS();
}

static void test_route_lifecycle(void) {
    TEST("route_lifecycle");
    route_t route;
    int rc = route_create(&route, "RTE-001", "TRK-001");
    ASSERT_TRUE(rc == 0, "route_create failed");
    ASSERT_EQ_INT(0, route.stop_count, "initial stop count");

    route_stop_t stop;
    memset(&stop, 0, sizeof(stop));
    strncpy(stop.stop_id, "STOP-1", sizeof(stop.stop_id) - 1);
    stop.sequence_number = 1;

    rc = route_add_stop(&route, &stop);
    ASSERT_TRUE(rc == 0, "route_add_stop failed");
    ASSERT_EQ_INT(1, route.stop_count, "stop count after add");

    route_destroy(&route);
    PASS();
}

/* ============================================================
 * L4 — Haversine Distance Test
 * ============================================================ */
static void test_haversine(void) {
    TEST("geo_distance_km (Haversine)");
    geo_location_t la = {34.0522, -118.2437, 0};  /* Los Angeles */
    geo_location_t sf = {37.7749, -122.4194, 0};   /* San Francisco */
    double d = geo_distance_km(&la, &sf);

    /* Approx 559 km */
    ASSERT_DOUBLE_EQ(559.0, d, 10.0, "LA to SF distance");
    PASS();
}

static void test_haversine_same_point(void) {
    TEST("geo_distance_km (same point)");
    geo_location_t p = {40.7128, -74.0060, 0};  /* NYC */
    double d = geo_distance_km(&p, &p);
    ASSERT_DOUBLE_EQ(0.0, d, 0.001, "same point distance");
    PASS();
}

/* ============================================================
 * L4/L5 — Inventory Tests
 * ============================================================ */
static void test_eoq(void) {
    TEST("EOQ computation");
    eoq_params_t params;
    memset(&params, 0, sizeof(params));
    params.annual_demand = 12000.0;      /* 12000 units/year */
    params.ordering_cost = 50.0;         /* $50 per order */
    params.holding_cost_per_unit = 2.0;  /* $2/unit/year */
    params.unit_cost = 25.0;
    params.lead_time_days = 5.0;
    params.working_days_per_year = 250;

    eoq_result_t result = eoq_compute(&params);

    /* Q* = sqrt(2*12000*50/2) = sqrt(600000) ≈ 774.6 */
    ASSERT_DOUBLE_EQ(774.6, result.eoq, 1.0, "EOQ value");
    ASSERT_TRUE(result.orders_per_year > 0, "orders per year");
    ASSERT_TRUE(result.reorder_point > 0, "reorder point");
    ASSERT_DOUBLE_EQ(387.3, result.avg_inventory, 1.0, "avg inventory");
    PASS();
}

static void test_safety_stock(void) {
    TEST("safety stock (constant LT)");
    safety_stock_params_t params;
    memset(&params, 0, sizeof(params));
    params.avg_daily_demand = 50.0;
    params.stddev_daily_demand = 10.0;
    params.avg_lead_time_days = 5.0;
    params.stddev_lead_time_days = 0.0;  /* Constant lead time */
    params.service_level = 0.95;          /* Z = 1.645 */

    int ss = safety_stock_compute(&params);
    /* SS = 1.645 * 10 * sqrt(5) = 1.645 * 10 * 2.236 = 36.78 → ceil = 37 */
    ASSERT_EQ_INT(37, ss, "95% safety stock");
    PASS();
}

static void test_cost_layer_fifo(void) {
    TEST("FIFO cost layering");
    cost_layer_stack_t stack;
    cost_layer_stack_init(&stack);

    /* Receive 100 units @ $10 each */
    int rc = cost_layer_push(&stack, 100, 10.0, "LOT-001");
    ASSERT_TRUE(rc == 0, "push 1");
    /* Receive 50 units @ $12 each */
    rc = cost_layer_push(&stack, 50, 12.0, "LOT-002");
    ASSERT_TRUE(rc == 0, "push 2");

    /* Draw 120 units FIFO: 100 @ $10 + 20 @ $12 = 1000 + 240 = 1240 */
    double cogs;
    int drawn = cost_layer_draw_fifo(&stack, 120, &cogs);
    ASSERT_EQ_INT(120, drawn, "FIFO drawn qty");
    ASSERT_DOUBLE_EQ(1240.0, cogs, 0.01, "FIFO COGS");

    /* 30 units remain @ $12 */
    double avg = cost_layer_avg_cost(&stack);
    ASSERT_DOUBLE_EQ(12.0, avg, 0.001, "remaining avg cost");

    cost_layer_stack_destroy(&stack);
    PASS();
}

static void test_cost_layer_lifo(void) {
    TEST("LIFO cost layering");
    cost_layer_stack_t stack;
    cost_layer_stack_init(&stack);

    cost_layer_push(&stack, 100, 10.0, "LOT-001");
    cost_layer_push(&stack, 50, 12.0, "LOT-002");

    /* Draw 40 units LIFO: 40 @ $12 = $480 */
    double cogs;
    int drawn = cost_layer_draw_lifo(&stack, 40, &cogs);
    ASSERT_EQ_INT(40, drawn, "LIFO drawn qty");
    ASSERT_DOUBLE_EQ(480.0, cogs, 0.01, "LIFO COGS");

    cost_layer_stack_destroy(&stack);
    PASS();
}

static void test_abc_classification(void) {
    TEST("ABC classification");
    abc_item_t items[10];
    const char *skus[] = {"A1","A2","B1","B2","C1","C2","C3","C4","C5","C6"};
    double values[] = {500,300,80,70,8,7,6,5,4,20};  /* sum = 1000 */

    for (int i = 0; i < 10; i++) {
        strncpy(items[i].sku_id, skus[i], sizeof(items[i].sku_id) - 1);
        items[i].annual_usage_value = values[i];
    }

    abc_classify(items, 10, 0.80, 0.95);

    /* Sorted: 500(50%) A, 300(80%) A, 80(88%) B, 70(95%) B, 20(97%) C... */
    ASSERT_EQ_INT(ABC_CLASS_A, items[0].classification, "top item A");
    ASSERT_EQ_INT(ABC_CLASS_A, items[1].classification, "second item A");

    /* Find the item with value 20 (should be C since it's after 95%) */
    int found_c = 0;
    for (int i = 0; i < 10; i++) {
        if (fabs(items[i].annual_usage_value - 20.0) < 0.001) {
            found_c = (items[i].classification == ABC_CLASS_C) ? 1 : -1;
            break;
        }
    }
    ASSERT_TRUE(found_c == 1, "value 20 should be C");
    PASS();
}

static void test_newsvendor(void) {
    TEST("newsvendor model");
    /* Demand ~ N(100, 20)
       cu = $5 (profit lost per unit short)
       co = $3 (loss per unit unsold)
       critical ratio = 5/(5+3) = 0.625
       z(0.625) ≈ 0.319
       Q* = 100 + 0.319 * 20 = 106.38 */
    double q = newsvendor_optimal_qty(100.0, 20.0, 5.0, 3.0);
    ASSERT_DOUBLE_EQ(106.38, q, 0.5, "newsvendor Q*");
    PASS();
}

/* ============================================================
 * L5 — Transportation Tests
 * ============================================================ */
static void test_transport_graph(void) {
    TEST("transport graph init/add_edge");
    transport_graph_t graph;
    int rc = transport_graph_init(&graph, 5);
    ASSERT_TRUE(rc == 0, "graph init");

    rc = transport_graph_add_edge(&graph, 0, 1, 100.0, 60.0, 10.0,
                                   SHIP_MODE_TRUCKLOAD);
    ASSERT_TRUE(rc == 0, "add edge");
    ASSERT_EQ_INT(1, graph.degree[0], "degree after add");

    transport_graph_destroy(&graph);
    PASS();
}

static void test_dijkstra(void) {
    TEST("dijkstra shortest path");
    transport_graph_t graph;
    transport_graph_init(&graph, 4);

    /* Simple diamond graph */
    transport_graph_add_edge(&graph, 0, 1, 4.0, 10.0, 5.0, SHIP_MODE_TRUCKLOAD);
    transport_graph_add_edge(&graph, 0, 2, 2.0, 5.0, 3.0, SHIP_MODE_TRUCKLOAD);
    transport_graph_add_edge(&graph, 1, 3, 3.0, 8.0, 4.0, SHIP_MODE_TRUCKLOAD);
    transport_graph_add_edge(&graph, 2, 3, 5.0, 12.0, 6.0, SHIP_MODE_TRUCKLOAD);
    transport_graph_add_edge(&graph, 2, 1, 1.0, 3.0, 2.0, SHIP_MODE_TRUCKLOAD);

    int path[10];
    int path_len;
    double d = dijkstra_shortest_path(&graph, 0, 3, path, &path_len);

    /* Shortest path: 0→2→1→3 = 2+1+3 = 6.0 */
    ASSERT_DOUBLE_EQ(6.0, d, 0.01, "shortest path distance");
    ASSERT_EQ_INT(4, path_len, "path length");
    ASSERT_EQ_INT(0, path[0], "path[0] = source");
    ASSERT_EQ_INT(3, path[3], "path[3] = target");

    transport_graph_destroy(&graph);
    PASS();
}

static void test_mode_selection(void) {
    TEST("shipment mode selection");
    shipment_mode_t m;

    m = select_shipment_mode(10.0, 0.05, 100.0, 0, 0);
    ASSERT_EQ_INT(SHIP_MODE_PARCEL, m, "small → parcel");

    m = select_shipment_mode(5000.0, 20.0, 300.0, 0, 0);
    ASSERT_EQ_INT(SHIP_MODE_TRUCKLOAD, m, "heavy bulk → FTL");

    m = select_shipment_mode(100.0, 0.5, 1000.0, 1, 1);
    ASSERT_EQ_INT(SHIP_MODE_AIR_FREIGHT, m, "urgent international → air");

    m = select_shipment_mode(5000.0, 20.0, 2000.0, 0, 1);
    ASSERT_EQ_INT(SHIP_MODE_OCEAN_FREIGHT, m, "heavy international → ocean");

    PASS();
}

static void test_freight_cost(void) {
    TEST("freight cost estimation");
    double cost = estimate_freight_cost(SHIP_MODE_PARCEL, 5.0, 100.0, 0.01);
    /* $5 base + 0.50*5 + 0.10*100 = 5 + 2.5 + 10 = 17.5 */
    ASSERT_DOUBLE_EQ(17.5, cost, 0.1, "parcel cost");

    double cost2 = estimate_freight_cost(SHIP_MODE_TRUCKLOAD, 1000.0, 500.0, 10.0);
    /* 1.50*500 + 0.02*1000 = 750 + 20 = 770 */
    ASSERT_DOUBLE_EQ(770.0, cost2, 1.0, "FTL cost");

    PASS();
}

/* ============================================================
 * L5 — Demand Forecast Tests
 * ============================================================ */
static void test_sma(void) {
    TEST("simple moving average");
    sma_model_t model;
    sma_init(&model, 3);

    sma_update(&model, 10.0);
    sma_update(&model, 20.0);
    sma_update(&model, 30.0);

    double f = sma_forecast(&model);
    ASSERT_DOUBLE_EQ(20.0, f, 0.001, "SMA of [10,20,30]");

    sma_update(&model, 40.0);  /* Window: [20,30,40] → avg = 30 */
    f = sma_forecast(&model);
    ASSERT_DOUBLE_EQ(30.0, f, 0.001, "SMA after 40");

    sma_destroy(&model);
    PASS();
}

static void test_ses(void) {
    TEST("single exponential smoothing");
    ses_model_t model;
    ses_init(&model, 0.3);

    ses_update(&model, 100.0);
    ses_update(&model, 110.0);
    /* level = 0.3*110 + 0.7*100 = 33 + 70 = 103 */
    double f = ses_forecast(&model);
    ASSERT_DOUBLE_EQ(103.0, f, 0.01, "SES level after [100,110]");

    ses_update(&model, 106.0);
    /* level = 0.3*106 + 0.7*103 = 31.8 + 72.1 = 103.9 */
    f = ses_forecast(&model);
    ASSERT_DOUBLE_EQ(103.9, f, 0.1, "SES level after [100,110,106]");

    PASS();
}

static void test_holt(void) {
    TEST("holt double exponential smoothing");
    holt_model_t model;
    holt_init(&model, 0.5, 0.3);

    holt_update(&model, 100.0);  /* Init: level=100, trend=0 */
    holt_update(&model, 105.0);
    /* level = 0.5*105 + 0.5*(100+0) = 52.5 + 50 = 102.5
       trend = 0.3*(102.5-100) + 0.7*0 = 0.75 */
    double f3 = holt_forecast_k(&model, 3);
    /* f3 = level + 3*trend = 102.5 + 3*0.75 = 104.75 */
    ASSERT_DOUBLE_EQ(104.75, f3, 0.5, "Holt 3-step forecast");

    PASS();
}

static void test_forecast_error_metrics(void) {
    TEST("forecast error metrics");
    double actual[] = {100, 120, 130, 110, 140};
    double forecast[] = {105, 115, 125, 115, 135};
    int n = 5;

    double mad = forecast_error(actual, forecast, n, ERROR_MAD);
    /* |100-105|+|120-115|+|130-125|+|110-115|+|140-135| = 5+5+5+5+5 = 25/5 = 5 */
    ASSERT_DOUBLE_EQ(5.0, mad, 0.01, "MAD");

    double mse = forecast_error(actual, forecast, n, ERROR_MSE);
    /* (25+25+25+25+25)/5 = 25 */
    ASSERT_DOUBLE_EQ(25.0, mse, 0.01, "MSE");

    double rmse = forecast_error(actual, forecast, n, ERROR_RMSE);
    ASSERT_DOUBLE_EQ(5.0, rmse, 0.01, "RMSE");

    double md = forecast_error(actual, forecast, n, ERROR_MD);
    /* e = actual - forecast: (-5+5+5-5+5)/5 = 5/5 = 1.0 */
    ASSERT_DOUBLE_EQ(1.0, md, 0.01, "MD bias");

    PASS();
}

static void test_linear_regression(void) {
    TEST("linear regression");
    double x[] = {1, 2, 3, 4, 5};
    double y[] = {2, 4, 6, 8, 10};
    double slope, intercept;
    linear_regression(x, y, 5, &slope, &intercept);
    ASSERT_DOUBLE_EQ(2.0, slope, 0.001, "slope");
    ASSERT_DOUBLE_EQ(0.0, intercept, 0.001, "intercept y=2x");
    PASS();
}

/* ============================================================
 * L5 — SCM Optimization Tests
 * ============================================================ */
static void test_mrp(void) {
    TEST("MRP explosion");
    mrp_plan_t plan;
    int rc = mrp_plan_init(&plan, "FG-001", 2, 100, 10, 50, 6);
    ASSERT_TRUE(rc == 0, "MRP plan init");

    int gross_req[] = {20, 30, 40, 25, 35, 20};
    rc = mrp_explode(&plan, gross_req);
    ASSERT_TRUE(rc == 0, "MRP explode");

    /* Period 0: projected = 50 - 20 = 30 (above SS=10, no net req) */
    ASSERT_EQ_INT(30, plan.periods[0].projected_on_hand, "period 0 projected");
    ASSERT_EQ_INT(0, plan.periods[0].net_requirements, "period 0 no net req");

    mrp_plan_destroy(&plan);
    PASS();
}

static void test_risk_pooling(void) {
    TEST("risk pooling (square root law)");
    double ss_consolidated;
    double savings = risk_pooling_savings(4, 100.0, &ss_consolidated);
    /* ss_each = 100, 4 facilities → total_before = 400
       ss_centralized = 100 * sqrt(4) = 200
       savings = 1 - 200/400 = 0.5 */
    ASSERT_DOUBLE_EQ(0.5, savings, 0.01, "4→1 savings ratio");
    ASSERT_DOUBLE_EQ(200.0, ss_consolidated, 0.01, "consolidated SS");

    double savings9 = risk_pooling_savings(9, 100.0, NULL);
    ASSERT_DOUBLE_EQ(1.0 - 1.0/3.0, savings9, 0.01, "9→1 savings ratio");
    PASS();
}

static void test_bullwhip(void) {
    TEST("bullwhip effect analysis");
    bullwhip_echelon_t echelons[3];

    /* Retailer data */
    echelons[0].period_count = 5;
    double retail_demand[] = {100, 105, 98, 102, 95};
    double retail_orders[] = {100, 105, 98, 102, 95};
    echelons[0].demand_received = retail_demand;
    echelons[0].orders_placed = retail_orders;

    /* Wholesaler amplifies */
    double whole_demand[] = {100, 105, 98, 102, 95};
    double whole_orders[] = {110, 115, 100, 110, 100};
    echelons[1].demand_received = whole_demand;
    echelons[1].orders_placed = whole_orders;

    /* Manufacturer amplifies more */
    double manu_demand[] = {110, 115, 100, 110, 100};
    double manu_orders[] = {130, 140, 120, 130, 110};
    echelons[2].demand_received = manu_demand;
    echelons[2].orders_placed = manu_orders;

    bullwhip_analyze(echelons, 3, 5, 1);

    ASSERT_TRUE(echelons[0].bullwhip_ratio >= 0.9, "retailer ratio ~1");
    ASSERT_TRUE(echelons[1].bullwhip_ratio > 1.0, "wholesaler ratio > 1");
    ASSERT_TRUE(echelons[2].bullwhip_ratio > 1.0, "manufacturer ratio > 1");

    PASS();
}

static void test_co2(void) {
    TEST("CO2 emissions");
    double co2 = co2_emissions(SHIP_MODE_TRUCKLOAD, 10.0, 500.0);
    /* 0.062 * 10 * 500 = 310 kg CO2 */
    ASSERT_DOUBLE_EQ(310.0, co2, 0.1, "truck CO2");

    double co2_air = co2_emissions(SHIP_MODE_AIR_FREIGHT, 1.0, 500.0);
    /* 0.602 * 1 * 500 = 301 kg CO2 */
    ASSERT_DOUBLE_EQ(301.0, co2_air, 0.1, "air CO2 for 1 ton");

    PASS();
}

static void test_green_pareto(void) {
    TEST("green Pareto frontier");
    double costs[] = {10, 8, 12, 7, 5};
    double co2[]   = {50, 60, 45, 55, 70};
    int pareto[10];
    int n_pareto;

    green_pareto_frontier(costs, co2, 5, pareto, &n_pareto);

    /* costs[4]=5 but co2[4]=70 → dominated by cost[3]=7 co2[3]=55
       The Pareto frontier should contain non-dominated solutions */
    ASSERT_TRUE(n_pareto >= 1, "at least one Pareto point");
    /* Verify that cost[0]=10 co2[0]=50 is on frontier (no one has both lower) */
    int has_10_50 = 0;
    for (int i = 0; i < n_pareto; i++) {
        int idx = pareto[i];
        if (fabs(costs[idx]-10.0)<0.01 && fabs(co2[idx]-50.0)<0.01) has_10_50=1;
    }
    /* Actually: cost[1]=8,co2[1]=60 dominates? No, higher CO2.
       cost[3]=7,co2[3]=55 also doesn't dominate 10,50 (higher CO2).
       cost[0]=10,co2[0]=50 IS Pareto-optimal. */
    ASSERT_TRUE(has_10_50, "cost=10,co2=50 is Pareto optimal");
    PASS();
}

static void test_guaranteed_service_time(void) {
    TEST("guaranteed service time");
    double proc[] = {1.0, 2.0, 3.0};    /* e0: 1 day, e1: 2 days, e2: 3 days */
    double cov[]  = {0.5, 1.0, 0.0};    /* e0 has 0.5 day coverage, e1 has 1.0 */
    /* e0: GST = 1 + max(0, 0-0.5) = 1
       e1: GST = 2 + max(0, 1-1.0) = 2
       e2: GST = 3 + max(0, 2-0.0) = 5 */
    double gst = guaranteed_service_time(proc, cov, 3);
    ASSERT_DOUBLE_EQ(5.0, gst, 0.001, "GST for 3-echelon chain");
    PASS();
}

/* ============================================================
 * L2 — Warehouse Tests
 * ============================================================ */
static void test_warehouse_init(void) {
    TEST("warehouse init");
    warehouse_layout_t layout;
    int aisle_lengths[] = {10, 10, 8};
    int rc = warehouse_init(&layout, 3, aisle_lengths);
    ASSERT_TRUE(rc == 0, "warehouse init");

    storage_location_t loc;
    memset(&loc, 0, sizeof(loc));
    strncpy(loc.location_id, "A01B01L01", sizeof(loc.location_id) - 1);
    loc.type = LOCATION_PALLET_RACK;
    loc.volume_capacity_m3 = 2.0;
    loc.dist_to_dock_m = 10.0;

    rc = warehouse_add_location(&layout, 0, &loc);
    ASSERT_TRUE(rc == 0, "add location");

    char found_id[32];
    int found = warehouse_find_empty_near(&layout, 0,
                                           LOCATION_PALLET_RACK, found_id);
    ASSERT_TRUE(found == 1, "find empty near dock");

    warehouse_destroy(&layout);
    PASS();
}

static void test_littles_law(void) {
    TEST("Little's Law");
    double L = littles_law_warehouse(20.0, 2.5);
    /* L = 20 orders/hour * 2.5 hours = 50 orders in system */
    ASSERT_DOUBLE_EQ(50.0, L, 0.01, "Little's Law WIP");
    PASS();
}

static void test_pick_list(void) {
    TEST("pick list operations");
    pick_list_t list;
    pick_list_init(&list, PICK_BATCH);

    int rc = pick_list_add_item(&list, "SKU-001", "A01B01L01", 5, 10.0);
    ASSERT_TRUE(rc == 0, "add pick item");
    ASSERT_EQ_INT(1, list.item_count, "item count");

    rc = pick_list_add_item(&list, "SKU-002", "A01B02L01", 3, 15.0);
    ASSERT_TRUE(rc == 0, "add pick item 2");
    ASSERT_EQ_INT(2, list.item_count, "two items");

    pick_list_sort_by_location(&list);
    /* Items should be sorted by location_id */
    ASSERT_TRUE(strcmp(list.items[0].location_id,
                       list.items[1].location_id) <= 0, "sorted by location");

    pick_list_destroy(&list);
    PASS();
}

/* ============================================================
 * Main Runner
 * ============================================================ */
int main(void) {
    printf("\n");
    printf("================================================================\n");
    printf("  mini-logistics-supplychain — Comprehensive Test Suite\n");
    printf("================================================================\n\n");

    /* L1 — Core Definitions */
    printf("[L1] Core Definitions\n");
    printf("---------------------\n");
    test_sku_init();
    test_order_lifecycle();
    test_facility_init();
    test_route_lifecycle();

    /* L4 — Standards/Theorems */
    printf("\n[L4] Standards & Theorems\n");
    printf("-------------------------\n");
    test_haversine();
    test_haversine_same_point();
    test_littles_law();

    /* L4/L5 — Inventory */
    printf("\n[L4/L5] Inventory Management\n");
    printf("-----------------------------\n");
    test_eoq();
    test_safety_stock();
    test_cost_layer_fifo();
    test_cost_layer_lifo();
    test_abc_classification();
    test_newsvendor();

    /* L5 — Transportation */
    printf("\n[L5] Transportation & Routing\n");
    printf("------------------------------\n");
    test_transport_graph();
    test_dijkstra();
    test_mode_selection();
    test_freight_cost();

    /* L5 — Demand Forecasting */
    printf("\n[L5] Demand Forecasting\n");
    printf("------------------------\n");
    test_sma();
    test_ses();
    test_holt();
    test_forecast_error_metrics();
    test_linear_regression();

    /* L5/L8 — SCM Optimization */
    printf("\n[L5/L8] SCM Optimization\n");
    printf("-------------------------\n");
    test_mrp();
    test_risk_pooling();
    test_bullwhip();
    test_co2();
    test_green_pareto();
    test_guaranteed_service_time();

    /* L2 — Warehouse */
    printf("\n[L2] Warehouse Operations\n");
    printf("--------------------------\n");
    test_warehouse_init();
    test_pick_list();

    /* Summary */
    printf("\n================================================================\n");
    printf("  RESULTS: %d tests run, %d passed, %d failed\n",
           tests_run, tests_passed, tests_failed);
    printf("================================================================\n\n");

    if (tests_failed > 0) {
        printf("❌ SOME TESTS FAILED\n");
        return 1;
    }
    printf("✅ ALL TESTS PASSED\n");
    return 0;
}
