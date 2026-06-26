/**
 * bench_routing.c — Routing Algorithm Benchmarks
 *
 * Measures performance of:
 *   - Dijkstra's shortest path (varying graph sizes)
 *   - Clarke-Wright VRP (varying customer counts)
 *   - TSP heuristics (NN + 2-opt)
 *   - Knapsack load optimization
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "logistics_core.h"
#include "transportation.h"

static double get_time_ms(clock_t start, clock_t end) {
    return (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;
}

static void bench_dijkstra(void) {
    printf("  ── Dijkstra Shortest Path ──\n");
    printf("  %-10s %-10s %s\n", "Nodes", "Edges", "Time (ms)");

    int sizes[] = {10, 50, 100, 200};
    for (int s = 0; s < 4; s++) {
        int V = sizes[s];
        int E = V * 3;  /* ~3 edges per node */

        transport_graph_t graph;
        transport_graph_init(&graph, V);

        /* Create a connected random graph */
        for (int i = 0; i < V; i++) {
            graph.locations[i].latitude = 40.0 + ((double)rand() / RAND_MAX) * 5.0;
            graph.locations[i].longitude = -74.0 + ((double)rand() / RAND_MAX) * 5.0;
        }

        for (int i = 0; i < V; i++) {
            for (int j = 0; j < 3; j++) {
                int to = rand() % V;
                if (to == i) continue;
                double d = haversine_km(graph.locations[i].latitude,
                                        graph.locations[i].longitude,
                                        graph.locations[to].latitude,
                                        graph.locations[to].longitude);
                transport_graph_add_edge(&graph, i, to, d, d/60.0*60.0,
                                         d*1.0, SHIP_MODE_TRUCKLOAD);
            }
        }

        int path[512];
        int path_len;

        clock_t start = clock();
        for (int iter = 0; iter < 100; iter++) {
            int src = rand() % V;
            int dst = rand() % V;
            dijkstra_shortest_path(&graph, src, dst, path, &path_len);
        }
        clock_t end = clock();

        printf("  %-10d %-10d %8.3f ms (100 runs)\n",
               V, E, get_time_ms(start, end));

        transport_graph_destroy(&graph);
    }
    printf("\n");
}

static void bench_knapsack(void) {
    printf("  ── Knapsack Load Optimization ──\n");
    printf("  %-10s %-10s %s\n", "Items", "Capacity", "Time (ms)");

    int sizes[] = {20, 50, 100};

    for (int s = 0; s < 3; s++) {
        int n = sizes[s];

        load_plan_t plan;
        memset(&plan, 0, sizeof(plan));
        plan.items = (load_item_t *)malloc(n * sizeof(load_item_t));

        for (int i = 0; i < n; i++) {
            snprintf(plan.items[i].item_id, sizeof(plan.items[i].item_id),
                     "ITEM-%d", i);
            plan.items[i].weight_kg = 10.0 + ((double)rand() / RAND_MAX) * 100.0;
            plan.items[i].volume_m3 = 0.1 + ((double)rand() / RAND_MAX) * 3.0;
            plan.items[i].value = 100.0 + ((double)rand() / RAND_MAX) * 1000.0;
        }
        plan.item_count = n;

        clock_t start = clock();
        load_knapsack_optimize(&plan, 5000.0, 20.0);
        clock_t end = clock();

        printf("  %-10d %-10s %8.3f ms\n",
               n, "5000kg", get_time_ms(start, end));

        free(plan.items);
    }
    printf("\n");
}

static void bench_tsp(void) {
    printf("  ── TSP Heuristics ──\n");
    printf("  %-10s %-12s %-12s %s\n", "Nodes", "NN Time(ms)", "2-opt(ms)", "Total(ms)");

    int sizes[] = {10, 20, 50, 100};

    for (int s = 0; s < 4; s++) {
        int n = sizes[s];

        transport_graph_t graph;
        transport_graph_init(&graph, n);

        for (int i = 0; i < n; i++) {
            graph.locations[i].latitude = 40.0 + ((double)rand() / RAND_MAX) * 5.0;
            graph.locations[i].longitude = -74.0 + ((double)rand() / RAND_MAX) * 5.0;
        }

        int *nodes = (int *)malloc(n * sizeof(int));
        int *tour = (int *)malloc((n + 1) * sizeof(int));
        for (int i = 0; i < n; i++) nodes[i] = i;

        clock_t t1 = clock();
        for (int iter = 0; iter < 100; iter++) {
            tsp_nearest_neighbor(&graph, nodes, n, tour);
        }
        clock_t t2 = clock();

        double nn_time = get_time_ms(t1, t2);

        /* Run 2-opt on the last tour */
        t1 = clock();
        for (int iter = 0; iter < 100; iter++) {
            tsp_nearest_neighbor(&graph, nodes, n, tour);
            tsp_two_opt(&graph, tour, n);
        }
        t2 = clock();

        double total_time = get_time_ms(t1, t2);

        printf("  %-10d %9.3f ms %9.3f ms %9.3f ms (100 runs)\n",
               n, nn_time, total_time - nn_time, total_time);

        free(nodes);
        free(tour);
        transport_graph_destroy(&graph);
    }
    printf("\n");
}

int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║     ROUTING ALGORITHM BENCHMARKS                          ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    srand(42);  /* Fixed seed for reproducibility */

    bench_dijkstra();
    bench_knapsack();
    bench_tsp();

    printf("══════════════════════════════════════════════════════════════\n");
    printf("  Benchmarks Complete\n");
    printf("══════════════════════════════════════════════════════════════\n\n");

    return 0;
}
