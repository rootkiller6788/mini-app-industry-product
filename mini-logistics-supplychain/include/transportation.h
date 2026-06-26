/**
 * transportation.h — Transportation & Routing
 *
 * L2-L5: Transportation logistics, vehicle routing, and fleet management.
 *
 * Core Concepts (L2):
 *   - Vehicle Routing Problem (VRP) and its variants
 *   - Traveling Salesman Problem (TSP) for route optimization
 *   - Mode selection (FTL, LTL, intermodal, parcel)
 *   - Load consolidation and optimization
 *
 * Algorithms (L5):
 *   - Dijkstra's shortest path (graph-based routing)
 *   - Clarke-Wright Savings Algorithm for VRP
 *   - Nearest Neighbor heuristic for TSP
 *   - 2-opt local search for route improvement
 *   - Knapsack-based load optimization
 *
 * Standards (L4):
 *   - Haversine formula for geo-distance (spherical Earth)
 *   - Dantzig-Fulkerson-Johnson formulation for TSP (1954)
 *   - Clarke-Wright Savings (Clarke & Wright, 1964)
 *
 * Course Alignment:
 *   - MIT 15.053 Optimization Methods in Business Analytics
 *   - MIT CTL.SC2x Supply Chain Design
 *   - Georgia Tech ISYE 6333 Supply Chain Engineering
 *   - CMU 45-871 Logistics and Transportation
 *   - ETH 363-0541 Computational Optimization
 */

#ifndef TRANSPORTATION_H
#define TRANSPORTATION_H

#include "logistics_core.h"
#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * L1 — Transportation-Specific Structures
 * ============================================================ */

/** Vehicle in the fleet */
typedef struct {
    char    vehicle_id[32];
    char    carrier_id[32];
    double  capacity_kg;         /* Weight capacity */
    double  capacity_m3;         /* Volume capacity */
    double  cost_per_km;         /* Operating cost per kilometer */
    double  avg_speed_kmh;       /* Average speed */
    double  fuel_efficiency_kml; /* km per liter */
    double  co2_per_km;          /* CO2 emissions per km */
    int     is_refrigerated;     /* Cold chain capable */
    int     is_available;        /* Currently available for dispatch */
    geo_location_t current_location;
} vehicle_t;

/** A directed edge in the transportation graph */
typedef struct {
    int     to_node;             /* Destination node index */
    double  distance_km;         /* Distance */
    double  time_min;            /* Travel time */
    double  cost;                /* Monetary cost */
    double  co2;                 /* Carbon cost */
    shipment_mode_t allowed_mode;
} transport_edge_t;

/** Transportation network as adjacency list */
typedef struct {
    transport_edge_t **adj;      /* adj[u] = array of outgoing edges */
    int               *degree;   /* degree[u] = number of outgoing edges */
    int               *capacity; /* capacity[u] = allocated array size */
    int                node_count;
    facility_t       **facilities; /* Facility at each node */
    geo_location_t    *locations;  /* Coordinates for each node */
} transport_graph_t;

/**
 * Clarke-Wright Savings algorithm state.
 *
 * Savings S(i,j) = d(0,i) + d(0,j) - d(i,j)
 *
 * Where 0 is the depot. Higher savings → better to merge routes i and j.
 */
typedef struct {
    int     i;          /* Route endpoint i */
    int     j;          /* Route endpoint j */
    double  savings;    /* S(i,j) savings value */
} cw_saving_t;

/** A single vehicle route in VRP solution */
typedef struct {
    int     *node_sequence;   /* Ordered list of node indices visited */
    int      seq_length;
    int      seq_capacity;
    double   total_distance;
    double   total_load;
    int      is_valid;        /* Within vehicle capacity constraints */
} vrp_route_t;

/** Complete VRP solution */
typedef struct {
    vrp_route_t *routes;
    int          route_count;
    int          depot_index;
    double       total_distance;
    int          total_nodes_served;
} vrp_solution_t;

/** Load optimization item (for knapsack-based loading) */
typedef struct {
    char    item_id[32];
    double  weight_kg;
    double  volume_m3;
    double  value;          /* Priority or revenue */
    int     is_loaded;      /* Whether this item is loaded */
} load_item_t;

/** Load plan result */
typedef struct {
    load_item_t *items;
    int          item_count;
    double       total_weight;
    double       total_volume;
    double       total_value;
    double       weight_capacity;
    double       volume_capacity;
} load_plan_t;

/* ============================================================
 * L4/L5 — Distance & Path Algorithms
 * ============================================================ */

/**
 * Haversine distance between two geographic points (WGS84).
 *
 * Theorem: Spherical law of haversines
 *   hav(Δσ) = hav(Δlat) + cos(lat1)·cos(lat2)·hav(Δlon)
 *   hav(θ) = sin²(θ/2)
 *
 * Earth radius R = 6371.0 km (mean volumetric radius)
 *
 * Complexity: O(1)
 */
double haversine_km(double lat1, double lon1, double lat2, double lon2);

/**
 * Dijkstra's shortest path algorithm on the transport graph.
 *
 * Theorem: Dijkstra (1959) — optimal for non-negative edge weights.
 *
 * Complexity: O((V + E) log V) using binary heap
 *
 * @param graph     Transportation graph
 * @param source    Source node index
 * @param target    Target node index
 * @param path_out  Output array for node sequence (caller allocates V ints)
 * @param path_len  Output: length of the path
 * @return Total distance of shortest path, or INFINITY if unreachable
 */
double dijkstra_shortest_path(const transport_graph_t *graph,
                              int source, int target,
                              int *path_out, int *path_len);

/**
 * Nearest Neighbor heuristic for TSP.
 * Starts at depot, repeatedly visits nearest unvisited node.
 *
 * Theorem: NN is a O(n²) heuristic with approximation ratio
 *   bounded by 0.5·log₂(n) + 0.5 (Rosenkrantz, Stearns & Lewis, 1977).
 *
 * Complexity: O(n²)
 *
 * @param graph     Transport graph
 * @param nodes     Array of node indices to visit
 * @param n         Number of nodes
 * @param tour_out  Output: node sequence of length n+1 (return to start)
 * @return Total tour distance
 */
double tsp_nearest_neighbor(const transport_graph_t *graph,
                            const int *nodes, int n,
                            int *tour_out);

/**
 * 2-opt local search improvement for a TSP tour.
 *
 * Theorem: 2-opt removes crossing edges in a tour, improving it.
 *   The algorithm terminates at a 2-optimal tour.
 *   Repeated iterations yield a local optimum.
 *
 * Complexity: O(n²) per iteration
 *
 * @param graph     Transport graph
 * @param tour      Tour array of length n+1 (modified in-place)
 * @param n         Number of nodes (tour length - 1)
 * @return Improvement in total distance (negative = no improvement)
 */
double tsp_two_opt(const transport_graph_t *graph, int *tour, int n);

/* ============================================================
 * L5 — Clarke-Wright Savings Algorithm (VRP)
 * ============================================================ */

/**
 * Solve Vehicle Routing Problem using Clarke-Wright Savings algorithm.
 *
 * Algorithm (Clarke & Wright, 1964):
 * 1. Compute savings S(i,j) = d(0,i) + d(j,0) - d(i,j) for all i, j
 * 2. Sort savings in descending order
 * 3. Merge routes greedily, respecting capacity constraints
 *
 * Theorem: CW Savings is a constructive heuristic for VRP with
 * O(n² log n) complexity. It generally produces solutions within
 * 10-15% of optimal for standard instances.
 *
 * Complexity: O(n² log n) where n = number of customer nodes
 *
 * @param graph             Transport graph
 * @param demand             Demand quantity at each node (indexed by node)
 * @param customer_count     Number of customer nodes
 * @param depot_index        Index of the depot node
 * @param vehicle_capacity   Max load per vehicle
 * @param max_vehicles       Maximum number of vehicles available
 * @return VRP solution (caller must free with vrp_solution_destroy)
 */
vrp_solution_t *clarke_wright_vrp(const transport_graph_t *graph,
                                  const double *demand,
                                  int customer_count,
                                  int depot_index,
                                  double vehicle_capacity,
                                  int max_vehicles);

/**
 * Free a VRP solution.
 * Complexity: O(routes).
 */
void vrp_solution_destroy(vrp_solution_t *sol);

/* ============================================================
 * L5 — Load Optimization (Knapsack-based)
 * ============================================================ */

/**
 * 0/1 Knapsack load optimization with two constraints (weight, volume).
 *
 * Maximizes total value subject to weight ≤ W and volume ≤ V.
 * Uses dynamic programming on weight dimension with volume check.
 *
 * Theorem: 0/1 Knapsack solved optimally by DP in O(n·W) time.
 * Two-constraint version adds O(n·W·V) in naive implementation;
 * this implementation uses weight-DP + volume feasibility pruning.
 *
 * Complexity: O(n · weight_capacity) time, O(weight_capacity) space
 *
 * @param plan   Load plan with items (value, weight, volume filled in)
 * @param w_cap  Weight capacity
 * @param v_cap  Volume capacity
 * @return Total value loaded
 */
double load_knapsack_optimize(load_plan_t *plan, double w_cap, double v_cap);

/* ============================================================
 * L1/L2 — Fleet & Mode Operations
 * ============================================================ */

/**
 * Initialize a transport graph with a given number of nodes.
 * Returns 0 on success, -1 on allocation failure.
 * Complexity: O(V) allocation
 */
int  transport_graph_init(transport_graph_t *graph, int node_count);

/**
 * Add a directed edge to the transport graph.
 * Complexity: O(1) amortized
 */
int  transport_graph_add_edge(transport_graph_t *graph, int from, int to,
                              double distance_km, double time_min,
                              double cost, shipment_mode_t mode);

/**
 * Add a facility at a specific graph node.
 * Complexity: O(1)
 */
void transport_graph_set_facility(transport_graph_t *graph, int node_idx,
                                  const facility_t *fac);

/**
 * Free the transport graph memory.
 * Complexity: O(V + E)
 */
void transport_graph_destroy(transport_graph_t *graph);

/**
 * Initialize a vehicle record.
 * Complexity: O(1)
 */
void vehicle_init(vehicle_t *v, const char *vid, double cap_kg, double cap_m3);

/**
 * Select optimal shipment mode given distance, weight, volume, and urgency.
 *
 * Decision matrix based on:
 *   - Parcel: < 30 kg, < 0.1 m³
 *   - LTL: < 2000 kg, < 15 m³
 *   - FTL: ≥ 2000 kg or ≥ 15 m³
 *   - Air: urgent + high value, distance > 500 km
 *   - Ocean: non-urgent + international, heavy
 *   - Intermodal: distance > 800 km, non-urgent
 *
 * Complexity: O(1)
 */
shipment_mode_t select_shipment_mode(double weight_kg, double volume_m3,
                                     double distance_km, int is_urgent,
                                     int is_international);

/**
 * Estimate freight cost using mode-specific rate structures.
 * Complexity: O(1)
 */
double estimate_freight_cost(shipment_mode_t mode, double weight_kg,
                             double distance_km, double volume_m3);

/**
 * Compute the linehaul distance matrix between all facility pairs
 * in the graph using repeated Dijkstra calls (or Floyd-Warshall
 * for dense graphs).
 *
 * Complexity: O(V³) using Floyd-Warshall
 *
 * @param graph    Transport graph
 * @param dist_out Output: V×V matrix (caller allocates V*V doubles)
 *                  dist_out[i*V + j] = distance from i to j
 */
void all_pairs_shortest_path(const transport_graph_t *graph, double *dist_out);

/**
 * Find the nearest facility of a given type to a location.
 * Uses linear scan with Haversine distance.
 * Complexity: O(n) where n = number of facilities checked
 */
int find_nearest_facility(const facility_t *facilities, int count,
                          facility_type_t type_filter,
                          geo_location_t target,
                          double *distance_out);

#endif /* TRANSPORTATION_H */
