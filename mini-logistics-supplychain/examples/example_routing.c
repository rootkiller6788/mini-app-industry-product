/**
 * example_routing.c — Transportation & Routing Example
 *
 * L6: Demonstrates Dijkstra shortest path, route optimization,
 *     mode selection, freight cost estimation, and load planning
 *     in a realistic logistics scenario.
 *
 * Scenario: Regional distribution from a central DC to 5 retail
 * locations across a metropolitan area.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "logistics_core.h"
#include "transportation.h"
#include "scm_optimize.h"

int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║     TRANSPORTATION & ROUTING EXAMPLE                     ║\n");
    printf("║     Dijkstra · TSP · VRP · Mode Selection · Load Plan    ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* ── Part 1: Build Transportation Network ── */
    printf("─── Part 1: Transportation Network ───\n\n");

    /*
     * Network topology:
     *
     *   DC (0) — North Hub (1) — Customer A (2)
     *    |           |              |
     *   South (3) — East (4) — Customer B (5)
     *
     * Coordinates (lat, lon):
     *   0: DC        (40.7128, -74.0060)  NYC
     *   1: North Hub (40.7580, -73.9855)  Midtown
     *   2: Cust A    (40.7831, -73.9712)  Upper East
     *   3: South     (40.6892, -74.0445)  Jersey City
     *   4: East Hub  (40.7282, -73.7949)  Queens
     *   5: Cust B    (40.7589, -73.7651)  Bayside
     */

    facility_t facilities[6];
    facility_init(&facilities[0], "DC-NYC", "NYC Distribution Center",
                  FACILITY_DISTRIBUTION, 40.7128, -74.0060);
    facility_init(&facilities[1], "HUB-NORTH", "North Transit Hub",
                  FACILITY_HUB, 40.7580, -73.9855);
    facility_init(&facilities[2], "CUST-A", "Customer A Store",
                  FACILITY_RETAIL, 40.7831, -73.9712);
    facility_init(&facilities[3], "SOUTH", "South Facility",
                  FACILITY_CROSSDOCK, 40.6892, -74.0445);
    facility_init(&facilities[4], "HUB-EAST", "East Transit Hub",
                  FACILITY_HUB, 40.7282, -73.7949);
    facility_init(&facilities[5], "CUST-B", "Customer B Store",
                  FACILITY_RETAIL, 40.7589, -73.7651);

    transport_graph_t graph;
    transport_graph_init(&graph, 6);
    for (int i = 0; i < 6; i++) {
        transport_graph_set_facility(&graph, i, &facilities[i]);
    }

    /* Add edges with distances */
    transport_graph_add_edge(&graph, 0, 1,
        haversine_km(40.7128, -74.0060, 40.7580, -73.9855),
        15.0, 5.0, SHIP_MODE_TRUCKLOAD);
    transport_graph_add_edge(&graph, 0, 3,
        haversine_km(40.7128, -74.0060, 40.6892, -74.0445),
        12.0, 4.0, SHIP_MODE_TRUCKLOAD);
    transport_graph_add_edge(&graph, 1, 0,
        haversine_km(40.7580, -73.9855, 40.7128, -74.0060),
        15.0, 5.0, SHIP_MODE_TRUCKLOAD);
    transport_graph_add_edge(&graph, 1, 2,
        haversine_km(40.7580, -73.9855, 40.7831, -73.9712),
        8.0, 3.0, SHIP_MODE_LTL);
    transport_graph_add_edge(&graph, 1, 4,
        haversine_km(40.7580, -73.9855, 40.7282, -73.7949),
        25.0, 8.0, SHIP_MODE_TRUCKLOAD);
    transport_graph_add_edge(&graph, 2, 1,
        haversine_km(40.7831, -73.9712, 40.7580, -73.9855),
        8.0, 3.0, SHIP_MODE_LTL);
    transport_graph_add_edge(&graph, 3, 0,
        haversine_km(40.6892, -74.0445, 40.7128, -74.0060),
        12.0, 4.0, SHIP_MODE_TRUCKLOAD);
    transport_graph_add_edge(&graph, 3, 4,
        haversine_km(40.6892, -74.0445, 40.7282, -73.7949),
        30.0, 10.0, SHIP_MODE_TRUCKLOAD);
    transport_graph_add_edge(&graph, 4, 1,
        haversine_km(40.7282, -73.7949, 40.7580, -73.9855),
        25.0, 8.0, SHIP_MODE_TRUCKLOAD);
    transport_graph_add_edge(&graph, 4, 3,
        haversine_km(40.7282, -73.7949, 40.6892, -74.0445),
        30.0, 10.0, SHIP_MODE_TRUCKLOAD);
    transport_graph_add_edge(&graph, 4, 5,
        haversine_km(40.7282, -73.7949, 40.7589, -73.7651),
        10.0, 4.0, SHIP_MODE_LTL);
    transport_graph_add_edge(&graph, 5, 4,
        haversine_km(40.7589, -73.7651, 40.7282, -73.7949),
        10.0, 4.0, SHIP_MODE_LTL);

    printf("  Network: %d nodes, %d edges\n", graph.node_count,
           graph.degree[0] + graph.degree[1] + graph.degree[2] +
           graph.degree[3] + graph.degree[4] + graph.degree[5]);

    for (int i = 0; i < 6; i++) {
        printf("  Node %d: %s (%s)\n", i, facilities[i].name,
               (facilities[i].type == FACILITY_DISTRIBUTION) ? "DC" :
               (facilities[i].type == FACILITY_HUB) ? "Hub" :
               (facilities[i].type == FACILITY_RETAIL) ? "Store" : "Other");
    }
    printf("\n");

    /* ── Part 2: Dijkstra Shortest Path ── */
    printf("─── Part 2: Dijkstra Shortest Path ───\n\n");

    int path[10], path_len;
    double dist = dijkstra_shortest_path(&graph, 0, 5, path, &path_len);

    printf("  Shortest path from DC (0) to Customer B (5):\n");
    printf("  Distance: %.1f km\n", dist);
    printf("  Path: ");
    for (int i = 0; i < path_len; i++) {
        printf("%s (%d)%s", facilities[path[i]].name, path[i],
               (i < path_len - 1) ? " → " : "\n");
    }
    printf("\n");

    /* ── Part 3: Mode Selection & Cost ── */
    printf("─── Part 3: Shipment Mode & Freight Cost ───\n\n");

    double scenarios[][4] = {
        /* {weight_kg, volume_m3, distance_km, urgency} */
        {10.0,    0.05,  50.0,  0},   /* Small local → parcel */
        {500.0,   2.0,   300.0, 1},   /* Medium urgent → LTL */
        {8000.0,  20.0,  500.0, 0},   /* Large non-urgent → FTL */
        {20000.0, 40.0,  3000.0, 0},  /* Int'l heavy → Ocean */
        {50.0,    0.2,   1000.0, 1},  /* Urgent far → Air */
    };
    const char *desc[] = {
        "Small parcel, local",
        "Medium freight, urgent",
        "Heavy bulk, regional",
        "Overseas container",
        "Express air freight"
    };

    for (int s = 0; s < 5; s++) {
        shipment_mode_t mode = select_shipment_mode(
            scenarios[s][0], scenarios[s][1],
            scenarios[s][2], (int)scenarios[s][3], 0);
        double cost = estimate_freight_cost(mode, scenarios[s][0],
                                            scenarios[s][2], scenarios[s][1]);

        const char *mode_str =
            (mode == SHIP_MODE_PARCEL) ? "PARCEL" :
            (mode == SHIP_MODE_LTL) ? "LTL" :
            (mode == SHIP_MODE_TRUCKLOAD) ? "FTL" :
            (mode == SHIP_MODE_AIR_FREIGHT) ? "AIR" :
            (mode == SHIP_MODE_OCEAN_FREIGHT) ? "OCEAN" : "OTHER";

        printf("  %-24s → %-6s  $%8.2f\n", desc[s], mode_str, cost);
    }
    printf("\n");

    /* ── Part 4: Load Optimization ── */
    printf("─── Part 4: Vehicle Load Optimization (Knapsack) ───\n\n");

    load_plan_t plan;
    memset(&plan, 0, sizeof(plan));
    plan.weight_capacity = 1000.0;   /* 1000 kg */
    plan.volume_capacity = 10.0;     /* 10 m³ */

    load_item_t load_items[] = {
        {"Item-A", 200.0, 2.0, 500.0, 0},
        {"Item-B", 300.0, 3.0, 700.0, 0},
        {"Item-C", 150.0, 1.5, 300.0, 0},
        {"Item-D", 400.0, 4.0, 900.0, 0},
        {"Item-E", 250.0, 2.5, 600.0, 0},
        {"Item-F", 100.0, 1.0, 200.0, 0},
    };
    plan.items = load_items;
    plan.item_count = 6;

    double total_val = load_knapsack_optimize(&plan, 1000.0, 10.0);

    printf("  Vehicle capacity: 1000 kg, 10 m³\n");
    printf("  Items loaded:\n");
    printf("  %-10s %8s %8s %8s %s\n", "Item", "Weight", "Volume", "Value", "Loaded");
    for (int i = 0; i < 6; i++) {
        printf("  %-10s %7.0fkg %7.1fm³ $%7.0f  %s\n",
               load_items[i].item_id,
               load_items[i].weight_kg,
               load_items[i].volume_m3,
               load_items[i].value,
               load_items[i].is_loaded ? "✓ YES" : "✗ NO");
    }
    printf("  Total loaded: %.0f kg, %.1f m³, Value: $%.0f\n\n",
           plan.total_weight, plan.total_volume, total_val);

    /* ── Part 5: Fleet Vehicle Management ── */
    printf("─── Part 5: CO2 Emissions Estimation ───\n\n");
    printf("  GLEC Framework emission factors:\n");
    printf("  %-15s %10s %s\n", "Mode", "kg CO2/t·km", "Example (10t, 500km)");
    printf("  Truck:     %9.3f   %10.0f kg\n", 0.062,
           co2_emissions(SHIP_MODE_TRUCKLOAD, 10.0, 500.0));
    printf("  Rail:      %9.3f   %10.0f kg\n", 0.022,
           co2_emissions(SHIP_MODE_INTERMODAL, 10.0, 500.0));
    printf("  Air:       %9.3f   %10.0f kg\n", 0.602,
           co2_emissions(SHIP_MODE_AIR_FREIGHT, 10.0, 500.0));
    printf("  Ocean:     %9.3f   %10.0f kg\n", 0.008,
           co2_emissions(SHIP_MODE_OCEAN_FREIGHT, 10.0, 500.0));
    printf("\n");

    transport_graph_destroy(&graph);

    printf("══════════════════════════════════════════════════════════════\n");
    printf("  Transportation & Routing Example Complete\n");
    printf("══════════════════════════════════════════════════════════════\n\n");

    return 0;
}
