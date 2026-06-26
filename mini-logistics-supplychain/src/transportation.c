/**
 * transportation.c — Transportation & Routing Implementations
 *
 * L4: Haversine distance, Floyd-Warshall all-pairs shortest paths
 * L5: Dijkstra's algorithm, TSP (Nearest Neighbor + 2-opt),
 *     Clarke-Wright Savings VRP, Knapsack load optimization
 * L2: Shipment mode selection, freight cost estimation
 */

#include "transportation.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================
 * L4 — Haversine Distance
 * ============================================================ */
double haversine_km(double lat1, double lon1, double lat2, double lon2) {
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double rlat1 = lat1 * M_PI / 180.0;
    double rlat2 = lat2 * M_PI / 180.0;

    double a = sin(dlat / 2.0) * sin(dlat / 2.0) +
               cos(rlat1) * cos(rlat2) *
               sin(dlon / 2.0) * sin(dlon / 2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));

    return 6371.0 * c;
}

/* ============================================================
 * Binary Heap for Dijkstra (Min-Heap)
 * ============================================================ */
typedef struct {
    int     node;
    double  dist;
} heap_entry_t;

typedef struct {
    heap_entry_t *entries;
    int          *pos;    /* pos[node] = index in heap, -1 if not in heap */
    int            size;
    int            capacity;
} min_heap_t;

static void heap_init(min_heap_t *h, int capacity) {
    h->entries = (heap_entry_t *)malloc(capacity * sizeof(heap_entry_t));
    h->pos = (int *)malloc(capacity * sizeof(int));
    h->size = 0;
    h->capacity = capacity;
    for (int i = 0; i < capacity; i++) h->pos[i] = -1;
}

static void heap_destroy(min_heap_t *h) {
    free(h->entries);
    free(h->pos);
    h->entries = NULL;
    h->pos = NULL;
}

static void heap_swap(min_heap_t *h, int i, int j) {
    heap_entry_t tmp = h->entries[i];
    h->entries[i] = h->entries[j];
    h->entries[j] = tmp;
    h->pos[h->entries[i].node] = i;
    h->pos[h->entries[j].node] = j;
}

static void heap_sift_up(min_heap_t *h, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (h->entries[idx].dist < h->entries[parent].dist) {
            heap_swap(h, idx, parent);
            idx = parent;
        } else {
            break;
        }
    }
}

static void heap_sift_down(min_heap_t *h, int idx) {
    while (1) {
        int smallest = idx;
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;

        if (left < h->size &&
            h->entries[left].dist < h->entries[smallest].dist) {
            smallest = left;
        }
        if (right < h->size &&
            h->entries[right].dist < h->entries[smallest].dist) {
            smallest = right;
        }
        if (smallest != idx) {
            heap_swap(h, idx, smallest);
            idx = smallest;
        } else {
            break;
        }
    }
}

static void heap_push(min_heap_t *h, int node, double dist) {
    if (h->size >= h->capacity) return;
    if (h->pos[node] != -1) {
        /* Update existing entry */
        int idx = h->pos[node];
        if (dist < h->entries[idx].dist) {
            h->entries[idx].dist = dist;
            heap_sift_up(h, idx);
        }
        return;
    }
    int idx = h->size++;
    h->entries[idx].node = node;
    h->entries[idx].dist = dist;
    h->pos[node] = idx;
    heap_sift_up(h, idx);
}

static int heap_pop(min_heap_t *h, int *node, double *dist) {
    if (h->size == 0) return 0;
    *node = h->entries[0].node;
    *dist = h->entries[0].dist;
    h->pos[*node] = -1;

    h->size--;
    if (h->size > 0) {
        h->entries[0] = h->entries[h->size];
        h->pos[h->entries[0].node] = 0;
        heap_sift_down(h, 0);
    }
    return 1;
}

/* ============================================================
 * L5 — Dijkstra's Shortest Path
 * ============================================================
 *
 * Dijkstra, E.W. (1959). "A note on two problems in connexion
 * with graphs". Numerische Mathematik, 1, 269-271.
 *
 * Uses a binary min-heap for O((V+E) log V) complexity.
 *
 * Returns the shortest distance. The path is reconstructed
 * using parent pointers.
 */
double dijkstra_shortest_path(const transport_graph_t *graph,
                              int source, int target,
                              int *path_out, int *path_len) {
    if (!graph || !graph->adj || source < 0 || source >= graph->node_count ||
        target < 0 || target >= graph->node_count) {
        if (path_len) *path_len = 0;
        return INFINITY;
    }

    int V = graph->node_count;
    double *dist = (double *)malloc(V * sizeof(double));
    int    *parent = (int *)malloc(V * sizeof(int));
    int    *visited = (int *)calloc(V, sizeof(int));

    if (!dist || !parent || !visited) {
        free(dist); free(parent); free(visited);
        if (path_len) *path_len = 0;
        return INFINITY;
    }

    for (int i = 0; i < V; i++) {
        dist[i] = INFINITY;
        parent[i] = -1;
    }

    min_heap_t heap;
    heap_init(&heap, V);

    dist[source] = 0.0;
    heap_push(&heap, source, 0.0);

    while (heap.size > 0) {
        int u;
        double d;
        if (!heap_pop(&heap, &u, &d)) break;
        if (visited[u]) continue;
        visited[u] = 1;

        if (u == target) break;

        /* Relax all outgoing edges */
        for (int i = 0; i < graph->degree[u]; i++) {
            transport_edge_t *e = &graph->adj[u][i];
            int v = e->to_node;
            if (visited[v]) continue;

            double new_dist = dist[u] + e->distance_km;
            if (new_dist < dist[v]) {
                dist[v] = new_dist;
                parent[v] = u;
                heap_push(&heap, v, new_dist);
            }
        }
    }

    double result = dist[target];

    /* Reconstruct path */
    if (path_out && path_len && result < INFINITY) {
        int *rev = (int *)malloc(V * sizeof(int));
        int len = 0;
        int cur = target;
        while (cur != -1) {
            rev[len++] = cur;
            cur = parent[cur];
        }
        /* Reverse to get source→target order */
        for (int i = 0; i < len; i++) {
            path_out[i] = rev[len - 1 - i];
        }
        *path_len = len;
        free(rev);
    } else if (path_len) {
        *path_len = 0;
    }

    free(dist);
    free(parent);
    free(visited);
    heap_destroy(&heap);
    return result;
}

/* ============================================================
 * L5 — TSP: Nearest Neighbor Heuristic
 * ============================================================
 *
 * Algorithm:
 * 1. Start at node 0 (depot)
 * 2. Find nearest unvisited node
 * 3. Move there, mark visited
 * 4. Repeat until all nodes visited
 * 5. Return to depot
 *
 * Approximation ratio: O(log n) in worst case.
 */
double tsp_nearest_neighbor(const transport_graph_t *graph,
                            const int *nodes, int n,
                            int *tour_out) {
    if (!graph || !nodes || n <= 0 || !tour_out) return 0.0;

    int *visited = (int *)calloc(n, sizeof(int));
    if (!visited) return 0.0;

    tour_out[0] = 0;  /* Start at first node in list */
    visited[0] = 1;
    int current = 0;
    double total_dist = 0.0;

    for (int step = 1; step < n; step++) {
        int nearest = -1;
        double min_dist = INFINITY;

        for (int j = 0; j < n; j++) {
            if (visited[j]) continue;

            /* Compute distance using haversine between locations */
            const geo_location_t *loc_i = &graph->locations[nodes[current]];
            const geo_location_t *loc_j = &graph->locations[nodes[j]];
            double d = haversine_km(loc_i->latitude, loc_i->longitude,
                                    loc_j->latitude, loc_j->longitude);

            if (d < min_dist) {
                min_dist = d;
                nearest = j;
            }
        }

        if (nearest == -1) break;

        tour_out[step] = nearest;
        visited[nearest] = 1;
        total_dist += min_dist;
        current = nearest;
    }

    /* Return to start */
    const geo_location_t *loc_last = &graph->locations[nodes[current]];
    const geo_location_t *loc_start = &graph->locations[nodes[0]];
    total_dist += haversine_km(loc_last->latitude, loc_last->longitude,
                               loc_start->latitude, loc_start->longitude);
    tour_out[n] = 0;  /* Return to depot */

    free(visited);
    return total_dist;
}

/* ============================================================
 * L5 — 2-Opt Local Search Improvement
 * ============================================================
 *
 * 2-opt removes two edges and reconnects the tour in the only
 * alternative way that maintains a valid tour.
 *
 * For tour [v0, v1, ..., vi, vi+1, ..., vj, vj+1, ..., vn, v0]:
 *   Remove edges (vi, vi+1) and (vj, vj+1)
 *   Add edges (vi, vj) and (vi+1, vj+1)
 *   Reverse segment [vi+1 ... vj]
 *
 * If this reduces total length, apply the swap.
 * Repeat until no further improvement.
 */
static double tour_edge_dist(const transport_graph_t *graph,
                             const int *tour, int a, int b) {
    const geo_location_t *la = &graph->locations[tour[a]];
    const geo_location_t *lb = &graph->locations[tour[b]];
    return haversine_km(la->latitude, la->longitude,
                        lb->latitude, lb->longitude);
}

double tsp_two_opt(const transport_graph_t *graph, int *tour, int n) {
    if (!graph || !tour || n < 3) return 0.0;

    double total_improvement = 0.0;
    int improved = 1;

    while (improved) {
        improved = 0;
        for (int i = 0; i < n - 1; i++) {
            for (int j = i + 2; j < n + 1; j++) {
                int a = i, b = i + 1, c = j, d = j + 1;
                if (d > n) d = 0;  /* Wrap around */

                double dist_ab = tour_edge_dist(graph, tour, a, b);
                double dist_cd = tour_edge_dist(graph, tour, c, d);
                double dist_ac = tour_edge_dist(graph, tour, a, c);
                double dist_bd = tour_edge_dist(graph, tour, b, d);

                double delta = (dist_ac + dist_bd) - (dist_ab + dist_cd);

                if (delta < -1e-9) {
                    /* Apply 2-opt: reverse segment [b, c] */
                    int left = b, right = c;
                    while (left < right) {
                        int tmp = tour[left];
                        tour[left] = tour[right];
                        tour[right] = tmp;
                        left++;
                        right--;
                    }
                    total_improvement += (-delta);
                    improved = 1;
                }
            }
        }
    }

    return total_improvement;
}

/* ============================================================
 * L5 — Clarke-Wright Savings Algorithm for VRP
 * ============================================================
 *
 * Reference: Clarke, G. and Wright, J.W. (1964). "Scheduling
 * of vehicles from a central depot to a number of delivery
 * points". Operations Research, 12(4), 568-581.
 *
 * Savings S(i,j) = c(0,i) + c(0,j) - c(i,j)
 *
 * The algorithm greedily merges routes with highest savings.
 */
static int cw_savings_cmp(const void *a, const void *b) {
    const cw_saving_t *sa = (const cw_saving_t *)a;
    const cw_saving_t *sb = (const cw_saving_t *)b;
    if (sb->savings > sa->savings) return 1;
    if (sb->savings < sa->savings) return -1;
    return 0;
}

vrp_solution_t *clarke_wright_vrp(const transport_graph_t *graph,
                                  const double *demand,
                                  int customer_count,
                                  int depot_index,
                                  double vehicle_capacity,
                                  int max_vehicles) {
    if (!graph || !demand || customer_count <= 0 || max_vehicles <= 0) {
        return NULL;
    }

    /* Compute savings for all customer pairs */
    int max_savings = customer_count * (customer_count - 1) / 2;
    cw_saving_t *savings = (cw_saving_t *)malloc(max_savings * sizeof(cw_saving_t));
    if (!savings) return NULL;

    int s_count = 0;
    for (int i = 0; i < customer_count; i++) {
        if (i == depot_index) continue;
        for (int j = i + 1; j < customer_count; j++) {
            if (j == depot_index) continue;
            double d_0i = haversine_km(
                graph->locations[depot_index].latitude,
                graph->locations[depot_index].longitude,
                graph->locations[i].latitude,
                graph->locations[i].longitude);
            double d_0j = haversine_km(
                graph->locations[depot_index].latitude,
                graph->locations[depot_index].longitude,
                graph->locations[j].latitude,
                graph->locations[j].longitude);
            double d_ij = haversine_km(
                graph->locations[i].latitude,
                graph->locations[i].longitude,
                graph->locations[j].latitude,
                graph->locations[j].longitude);

            savings[s_count].i = i;
            savings[s_count].j = j;
            savings[s_count].savings = d_0i + d_0j - d_ij;
            s_count++;
        }
    }

    /* Sort savings descending */
    qsort(savings, s_count, sizeof(cw_saving_t), cw_savings_cmp);

    /* Initialize: each customer in its own route if demand ≤ capacity */
    vrp_solution_t *sol = (vrp_solution_t *)malloc(sizeof(vrp_solution_t));
    if (!sol) { free(savings); return NULL; }
    memset(sol, 0, sizeof(vrp_solution_t));
    sol->depot_index = depot_index;

    /* Track which route each node belongs to */
    int *route_of = (int *)malloc(graph->node_count * sizeof(int));
    int *route_end0 = (int *)malloc(graph->node_count * sizeof(int));
    int *route_end1 = (int *)malloc(graph->node_count * sizeof(int));
    if (!route_of || !route_end0 || !route_end1) {
        free(savings); free(sol);
        free(route_of); free(route_end0); free(route_end1);
        return NULL;
    }

    for (int i = 0; i < graph->node_count; i++) {
        route_of[i] = -1;
    }

    /* Create initial routes */
    int route_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (i == depot_index) continue;
        if (demand[i] <= 0.0) continue;
        if (route_count >= max_vehicles) break;
        if (demand[i] > vehicle_capacity) continue;

        route_of[i] = route_count;
        route_end0[route_count] = i;
        route_end1[route_count] = i;
        route_count++;
    }

    /* Greedy merge */
    for (int k = 0; k < s_count; k++) {
        int i = savings[k].i;
        int j = savings[k].j;
        int ri = route_of[i];
        int rj = route_of[j];

        if (ri < 0 || rj < 0) continue;  /* Not in any route */
        if (ri == rj) continue;           /* Already in same route */

        /* Check capacity */
        double load_i = 0.0, load_j = 0.0;
        for (int m = 0; m < graph->node_count; m++) {
            if (route_of[m] == ri) load_i += demand[m];
            if (route_of[m] == rj) load_j += demand[m];
        }

        if (load_i + load_j > vehicle_capacity) continue;

        /* Merge: i must be an endpoint of ri, j must be an endpoint of rj */
        int i_is_end = (i == route_end0[ri] || i == route_end1[ri]);
        int j_is_end = (j == route_end0[rj] || j == route_end1[rj]);

        if (!i_is_end || !j_is_end) continue;

        /* Transfer all nodes from rj to ri */
        for (int m = 0; m < graph->node_count; m++) {
            if (route_of[m] == rj) {
                route_of[m] = ri;
            }
        }

        /* Update endpoints of ri */
        if (i == route_end0[ri]) {
            if (j == route_end0[rj]) route_end0[ri] = route_end1[rj];
            else route_end0[ri] = route_end0[rj];
        } else {
            if (j == route_end0[rj]) route_end1[ri] = route_end1[rj];
            else route_end1[ri] = route_end0[rj];
        }

        /* Mark rj as empty */
        route_end0[rj] = -1;
        route_end1[rj] = -1;
    }

    /* Build result routes */
    sol->route_count = 0;
    for (int r = 0; r < route_count; r++) {
        if (route_end0[r] < 0) continue;  /* Empty route */
        sol->route_count++;
    }

    sol->routes = (vrp_route_t *)calloc(sol->route_count, sizeof(vrp_route_t));
    if (!sol->routes) {
        free(savings); free(route_of); free(route_end0); free(route_end1);
        free(sol);
        return NULL;
    }

    int route_idx = 0;
    for (int r = 0; r < route_count; r++) {
        if (route_end0[r] < 0) continue;
        /* Collect nodes in this route */
        int count = 0;
        for (int m = 0; m < graph->node_count; m++) {
            if (route_of[m] == r) count++;
        }
        sol->routes[route_idx].node_sequence = (int *)malloc(
            (count + 2) * sizeof(int));  /* +depot at start and end */
        if (!sol->routes[route_idx].node_sequence) {
            sol->routes[route_idx].seq_length = 0;
            continue;
        }
        sol->routes[route_idx].node_sequence[0] = depot_index;
        int pos = 1;
        for (int m = 0; m < graph->node_count; m++) {
            if (route_of[m] == r) {
                sol->routes[route_idx].node_sequence[pos++] = m;
                sol->routes[route_idx].total_load += demand[m];
            }
        }
        sol->routes[route_idx].node_sequence[pos] = depot_index;
        sol->routes[route_idx].seq_length = count + 2;
        sol->routes[route_idx].is_valid =
            (sol->routes[route_idx].total_load <= vehicle_capacity);

        /* Compute total distance */
        double d = 0.0;
        for (int s = 0; s < sol->routes[route_idx].seq_length - 1; s++) {
            int a = sol->routes[route_idx].node_sequence[s];
            int b = sol->routes[route_idx].node_sequence[s + 1];
            d += haversine_km(graph->locations[a].latitude,
                              graph->locations[a].longitude,
                              graph->locations[b].latitude,
                              graph->locations[b].longitude);
        }
        sol->routes[route_idx].total_distance = d;
        sol->total_distance += d;
        sol->total_nodes_served += count;
        route_idx++;
    }

    free(savings);
    free(route_of);
    free(route_end0);
    free(route_end1);
    return sol;
}

void vrp_solution_destroy(vrp_solution_t *sol) {
    if (!sol) return;
    if (sol->routes) {
        for (int i = 0; i < sol->route_count; i++) {
            free(sol->routes[i].node_sequence);
        }
        free(sol->routes);
    }
    free(sol);
}

/* ============================================================
 * L5 — Load Optimization (0/1 Knapsack with 2 constraints)
 * ============================================================
 *
 * Uses DP with weight as primary dimension and volume as
 * feasibility check. For each item, check if it fits within
 * remaining weight AND volume capacity.
 *
 * Recurrence:
 *   dp[w] = max(dp[w], dp[w - wi] + vi)  if w ≥ wi and vol ≤ V
 *
 * This is a practical simplification for logistics where
 * weight is often the binding constraint.
 */
double load_knapsack_optimize(load_plan_t *plan, double w_cap, double v_cap) {
    if (!plan || !plan->items || plan->item_count <= 0) return 0.0;

    plan->weight_capacity = w_cap;
    plan->volume_capacity = v_cap;

    int w_cap_int = (int)(w_cap * 100.0);  /* Scale to centigrams for precision */
    if (w_cap_int <= 0) return 0.0;

    double *dp = (double *)calloc(w_cap_int + 1, sizeof(double));
    double *dp_vol = (double *)calloc(w_cap_int + 1, sizeof(double));
    int **choice = (int **)malloc((w_cap_int + 1) * sizeof(int *));
    if (!dp || !dp_vol || !choice) {
        free(dp); free(dp_vol); free(choice);
        return 0.0;
    }
    for (int w = 0; w <= w_cap_int; w++) {
        choice[w] = (int *)calloc(plan->item_count, sizeof(int));
        if (!choice[w]) {
            /* Partial cleanup on failure */
            for (int i = 0; i < w; i++) free(choice[i]);
            free(choice); free(dp); free(dp_vol);
            return 0.0;
        }
    }

    for (int i = 0; i < plan->item_count; i++) {
        int wi = (int)(plan->items[i].weight_kg * 100.0);
        double vol_i = plan->items[i].volume_m3;
        double val_i = plan->items[i].value;

        for (int w = w_cap_int; w >= wi; w--) {
            double new_val = dp[w - wi] + val_i;
            double new_vol = dp_vol[w - wi] + vol_i;

            if (new_val > dp[w] && new_vol <= v_cap) {
                dp[w] = new_val;
                dp_vol[w] = new_vol;
                /* Copy choices from w-wi and add this item */
                for (int k = 0; k < plan->item_count; k++) {
                    choice[w][k] = choice[w - wi][k];
                }
                choice[w][i] = 1;
            }
        }
    }

    /* Find best feasible weight */
    double best_val = 0.0;
    int best_w = 0;
    for (int w = 0; w <= w_cap_int; w++) {
        if (dp[w] > best_val) {
            best_val = dp[w];
            best_w = w;
        }
    }

    /* Mark loaded items */
    for (int i = 0; i < plan->item_count; i++) {
        plan->items[i].is_loaded = choice[best_w][i];
    }

    plan->total_weight = (double)best_w / 100.0;
    plan->total_volume = dp_vol[best_w];
    plan->total_value = best_val;

    for (int w = 0; w <= w_cap_int; w++) free(choice[w]);
    free(choice);
    free(dp);
    free(dp_vol);
    return best_val;
}

/* ============================================================
 * L2 — Shipment Mode Selection
 * ============================================================
 *
 * Decision matrix for mode selection based on weight, volume,
 * distance, urgency, and international status.
 */
shipment_mode_t select_shipment_mode(double weight_kg, double volume_m3,
                                     double distance_km, int is_urgent,
                                     int is_international) {
    if (is_urgent && is_international && distance_km > 500.0) {
        return SHIP_MODE_AIR_FREIGHT;
    }
    if (is_international && !is_urgent && weight_kg > 1000.0) {
        return SHIP_MODE_OCEAN_FREIGHT;
    }
    if (!is_urgent && distance_km > 800.0 && weight_kg > 5000.0) {
        return SHIP_MODE_INTERMODAL;
    }
    if (weight_kg < 30.0 && volume_m3 < 0.1 && distance_km < 500.0) {
        return SHIP_MODE_PARCEL;
    }
    if (weight_kg >= 2000.0 || volume_m3 >= 15.0) {
        return SHIP_MODE_TRUCKLOAD;
    }
    if (is_urgent && distance_km < 200.0) {
        return SHIP_MODE_LAST_MILE;
    }
    return SHIP_MODE_LTL;
}

/* ============================================================
 * L2 — Freight Cost Estimation
 * ============================================================
 *
 * Uses mode-specific rate structures:
 *   - Parcel: base + per-kg
 *   - LTL: per-kg × distance bands
 *   - FTL: per-km for full truck
 *   - Air: per-kg × distance
 *   - Ocean: per-container approximation
 */
double estimate_freight_cost(shipment_mode_t mode, double weight_kg,
                             double distance_km, double volume_m3) {
    (void)volume_m3;  /* Used for some modes below */
    switch (mode) {
        case SHIP_MODE_PARCEL:
            /* $5 base + $0.50/kg + $0.10/km */
            return 5.0 + 0.50 * weight_kg + 0.10 * distance_km;
        case SHIP_MODE_LTL:
            /* $0.15/kg + $0.05/km */
            return 0.15 * weight_kg * (1.0 + distance_km / 100.0);
        case SHIP_MODE_TRUCKLOAD:
            /* $1.50/km + $0.02/kg */
            return 1.50 * distance_km + 0.02 * weight_kg;
        case SHIP_MODE_INTERMODAL:
            /* $0.80/km + $0.01/kg */
            return 0.80 * distance_km + 0.01 * weight_kg;
        case SHIP_MODE_AIR_FREIGHT:
            /* $2.50/kg + $0.005/km per kg */
            return weight_kg * (2.50 + 0.005 * distance_km);
        case SHIP_MODE_OCEAN_FREIGHT:
            /* $1500 per 20ft container (approx 10000 kg) */
            return (weight_kg / 10000.0) * 1500.0 + 0.001 * distance_km;
        case SHIP_MODE_LAST_MILE:
            /* $8.00 flat + per-stop */
            return 8.00 + 0.30 * distance_km;
        default:
            return 0.15 * weight_kg * distance_km;  /* Generic */
    }
}

/* ============================================================
 * L2 — Transport Graph Operations
 * ============================================================ */
int transport_graph_init(transport_graph_t *graph, int node_count) {
    if (!graph || node_count <= 0) return -1;
    memset(graph, 0, sizeof(transport_graph_t));

    graph->node_count = node_count;
    graph->adj = (transport_edge_t **)calloc(node_count, sizeof(transport_edge_t *));
    graph->degree = (int *)calloc(node_count, sizeof(int));
    graph->capacity = (int *)calloc(node_count, sizeof(int));
    graph->facilities = (facility_t **)calloc(node_count, sizeof(facility_t *));
    graph->locations = (geo_location_t *)calloc(node_count, sizeof(geo_location_t));

    if (!graph->adj || !graph->degree || !graph->capacity ||
        !graph->facilities || !graph->locations) {
        transport_graph_destroy(graph);
        return -1;
    }
    return 0;
}

int transport_graph_add_edge(transport_graph_t *graph, int from, int to,
                              double distance_km, double time_min,
                              double cost, shipment_mode_t mode) {
    if (!graph || !graph->adj || from < 0 || from >= graph->node_count ||
        to < 0 || to >= graph->node_count) return -1;

    /* Expand capacity if needed */
    if (graph->degree[from] >= graph->capacity[from]) {
        int new_cap = graph->capacity[from] == 0 ? 4 : graph->capacity[from] * 2;
        transport_edge_t *new_edges = (transport_edge_t *)realloc(
            graph->adj[from], new_cap * sizeof(transport_edge_t));
        if (!new_edges) return -1;
        graph->adj[from] = new_edges;
        graph->capacity[from] = new_cap;
    }

    transport_edge_t *e = &graph->adj[from][graph->degree[from]];
    e->to_node = to;
    e->distance_km = distance_km;
    e->time_min = time_min;
    e->cost = cost;
    e->co2 = 0.0;
    e->allowed_mode = mode;
    graph->degree[from]++;
    return 0;
}

void transport_graph_set_facility(transport_graph_t *graph, int node_idx,
                                  const facility_t *fac) {
    if (!graph || !graph->facilities || node_idx < 0 ||
        node_idx >= graph->node_count || !fac) return;

    if (!graph->facilities[node_idx]) {
        graph->facilities[node_idx] = (facility_t *)malloc(sizeof(facility_t));
    }
    if (graph->facilities[node_idx]) {
        memcpy(graph->facilities[node_idx], fac, sizeof(facility_t));
    }
    graph->locations[node_idx] = fac->location;
}

void transport_graph_destroy(transport_graph_t *graph) {
    if (!graph) return;
    if (graph->adj) {
        for (int i = 0; i < graph->node_count; i++) {
            free(graph->adj[i]);
        }
        free(graph->adj);
    }
    if (graph->facilities) {
        for (int i = 0; i < graph->node_count; i++) {
            free(graph->facilities[i]);
        }
        free(graph->facilities);
    }
    free(graph->degree);
    free(graph->capacity);
    free(graph->locations);
    memset(graph, 0, sizeof(transport_graph_t));
}

/* ============================================================
 * L4 — Floyd-Warshall All-Pairs Shortest Path
 * ============================================================
 *
 * Floyd, R.W. (1962). "Algorithm 97: Shortest Path".
 * Warshall, S. (1962). "A theorem on Boolean matrices".
 *
 * Complexity: O(V³)
 *
 * For dense graphs (where E ≈ V²), Floyd-Warshall is more
 * efficient than running Dijkstra V times.
 */
void all_pairs_shortest_path(const transport_graph_t *graph, double *dist_out) {
    if (!graph || !dist_out || graph->node_count <= 0) return;

    int V = graph->node_count;

    /* Initialize distances */
    for (int i = 0; i < V; i++) {
        for (int j = 0; j < V; j++) {
            dist_out[i * V + j] = (i == j) ? 0.0 : INFINITY;
        }
    }

    /* Direct edges */
    for (int u = 0; u < V; u++) {
        for (int e = 0; e < graph->degree[u]; e++) {
            int v = graph->adj[u][e].to_node;
            double d = graph->adj[u][e].distance_km;
            if (d < dist_out[u * V + v]) {
                dist_out[u * V + v] = d;
            }
        }
    }

    /* Floyd-Warshall DP */
    for (int k = 0; k < V; k++) {
        for (int i = 0; i < V; i++) {
            if (dist_out[i * V + k] == INFINITY) continue;
            for (int j = 0; j < V; j++) {
                if (dist_out[k * V + j] == INFINITY) continue;
                double through_k = dist_out[i * V + k] + dist_out[k * V + j];
                if (through_k < dist_out[i * V + j]) {
                    dist_out[i * V + j] = through_k;
                }
            }
        }
    }
}

/* ============================================================
 * L2 — Find Nearest Facility
 * ============================================================ */
int find_nearest_facility(const facility_t *facilities, int count,
                          facility_type_t type_filter,
                          geo_location_t target,
                          double *distance_out) {
    if (!facilities || count <= 0) {
        if (distance_out) *distance_out = INFINITY;
        return -1;
    }

    int best_idx = -1;
    double best_dist = INFINITY;

    for (int i = 0; i < count; i++) {
        if (facilities[i].type == type_filter || type_filter < 0) {
            double d = haversine_km(facilities[i].location.latitude,
                                    facilities[i].location.longitude,
                                    target.latitude, target.longitude);
            if (d < best_dist) {
                best_dist = d;
                best_idx = i;
            }
        }
    }

    if (distance_out) *distance_out = best_dist;
    return best_idx;
}

/* ============================================================
 * Vehicle Lifecycle
 * ============================================================ */
void vehicle_init(vehicle_t *v, const char *vid, double cap_kg, double cap_m3) {
    if (!v) return;
    memset(v, 0, sizeof(vehicle_t));
    if (vid) {
        strncpy(v->vehicle_id, vid, sizeof(v->vehicle_id) - 1);
        v->vehicle_id[sizeof(v->vehicle_id) - 1] = '\0';
    }
    v->capacity_kg = cap_kg;
    v->capacity_m3 = cap_m3;
    v->cost_per_km = 0.0;
    v->avg_speed_kmh = 60.0;
    v->fuel_efficiency_kml = 3.0;
    v->co2_per_km = 0.062 * cap_kg / 1000.0;
    v->is_refrigerated = 0;
    v->is_available = 1;
}
