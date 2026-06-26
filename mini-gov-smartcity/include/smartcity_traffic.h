#ifndef MINI_SMARTCITY_TRAFFIC_H
#define MINI_SMARTCITY_TRAFFIC_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* ================================================================
 *  Smart City Traffic -- Signal Control, Transit, Congestion
 *
 *  L1: Core definitions -- TrafficLight, Intersection, TransitRoute,
 *       Vehicle, ParkingLot, CongestionEvent
 *  L2: Core concepts -- Traffic flow theory, signal phase timing,
 *       adaptive traffic control, public transit optimization
 *  L3: Engineering structures -- Intersection graph, signal phase
 *       ring, traffic flow time-series, route adjacency matrix
 *  L4: Standards -- HCM (Highway Capacity Manual), Webster's formula
 *       for optimal cycle time, TRANSYT traffic model
 *  L5: Algorithms -- Webster optimal cycle, green wave coordination,
 *       Dijkstra shortest path with congestion weights, traffic
 *       flow prediction using moving average
 *  L6: Canonical problems -- Adaptive traffic signal control,
 *       smart parking management, real-time traffic monitoring
 *  L7: Applications -- Traffic congestion heatmap, transit schedule
 *       optimization, EV charging station routing
 *  L8: Advanced -- V2X (Vehicle-to-Everything), reinforcement learning
 *       for signal timing, multi-modal transit optimization
 *  L9: Industry -- Siemens SCOOT, SCATS (Sydney), Google Green Light,
 *       Waze CCP, INRIX traffic analytics
 *
 *  References:
 *  - Webster, F.V. (1958) Traffic Signal Settings, Road Research
 *    Technical Paper No. 39
 *  - Highway Capacity Manual (HCM) 7th Edition
 * ================================================================ */

#define SC_TRAFFIC_MAX_INTERSECTIONS   65536
#define SC_TRAFFIC_MAX_TRANSIT_ROUTES  4096
#define SC_TRAFFIC_MAX_VEHICLES        524288
#define SC_TRAFFIC_MAX_PARKING_LOTS    16384
#define SC_TRAFFIC_MAX_ROADS           131072

/* ---- L1: Signal Phase Color ---- */
typedef enum {
    SC_SIG_RED     = 0,
    SC_SIG_YELLOW  = 1,
    SC_SIG_GREEN   = 2,
    SC_SIG_FLASHING_YELLOW = 3,
    SC_SIG_FLASHING_RED    = 4
} SC_SignalColor;

/* ---- L1: Transit Mode ---- */
typedef enum {
    SC_TRANSIT_BUS       = 0,
    SC_TRANSIT_METRO     = 1,
    SC_TRANSIT_LIGHT_RAIL = 2,
    SC_TRANSIT_BRT       = 3,
    SC_TRANSIT_FERRY     = 4,
    SC_TRANSIT_COUNT     = 5
} SC_TransitMode;

/* ---- L1: Congestion Level ---- */
typedef enum {
    SC_CONGESTION_FREE_FLOW  = 0,
    SC_CONGESTION_LIGHT      = 1,
    SC_CONGESTION_MODERATE   = 2,
    SC_CONGESTION_HEAVY      = 3,
    SC_CONGESTION_GRIDLOCK   = 4
} SC_CongestionLevel;

/* ---- L1: Intersection ---- */
typedef struct {
    int      intersection_id;
    char     name[128];
    double   longitude;
    double   latitude;
    int      num_approaches;
    int      approach_road_ids[4];
    int      current_phase;
    int      num_phases;
    int      phase_durations[8];
    int      cycle_time_seconds;
    int      green_time_ns;
    int      green_time_ew;
    int      yellow_time;
    int      all_red_time;
    bool     has_pedestrian_phase;
    bool     has_emergency_preemption;
    bool     is_adaptive;
    int      vehicle_count;
    float    congestion_index;
    time_t   last_cycle_change;
} SC_Intersection;

/* ---- L1: Traffic Light ---- */
typedef struct {
    int      light_id;
    int      intersection_id;
    int      approach_direction;
    SC_SignalColor current_color;
    int      time_in_current;
    int      max_green_time;
    int      min_green_time;
    bool     is_operational;
    bool     is_led;
    time_t   installed_at;
    int      failure_count;
} SC_TrafficLight;

/* ---- L1: Road Segment ---- */
typedef struct {
    int      road_id;
    char     name[128];
    int      from_intersection;
    int      to_intersection;
    double   distance_km;
    int      speed_limit_kmh;
    int      num_lanes;
    bool     is_one_way;
    SC_CongestionLevel current_congestion;
    int      current_vehicle_count;
    float    avg_speed;
    time_t   last_updated;
} SC_RoadSegment;

/* ---- L1: Transit Route ---- */
typedef struct {
    int      route_id;
    char     name[128];
    SC_TransitMode mode;
    int      num_stops;
    int      stop_intersection_ids[64];
    int      num_vehicles;
    int      headway_minutes;
    int      daily_ridership;
    float    on_time_performance;
    bool     is_active;
    time_t   first_departure;
    time_t   last_departure;
} SC_TransitRoute;

/* ---- L1: Parking Lot ---- */
typedef struct {
    int      lot_id;
    char     name[128];
    double   longitude;
    double   latitude;
    int      total_spaces;
    int      occupied_spaces;
    int      reserved_spaces;
    int      handicap_spaces;
    int      ev_charging_spaces;
    float    hourly_rate;
    bool     is_open;
    bool     has_ev_charging;
    time_t   last_updated;
} SC_ParkingLot;

/* ---- Traffic System ---- */
typedef struct SC_TrafficSystem SC_TrafficSystem;
SC_TrafficSystem* sc_traffic_create(void);
void              sc_traffic_destroy(SC_TrafficSystem *traf);

/* Intersection Management (L2) */
int  sc_traffic_intersection_add(SC_TrafficSystem *traf, const SC_Intersection *isec);
SC_Intersection* sc_traffic_intersection_lookup(SC_TrafficSystem *traf, int id);
int  sc_traffic_intersection_set_cycle(SC_TrafficSystem *traf, int id, int green_ns, int green_ew, int yellow, int all_red);
int  sc_traffic_intersection_count(const SC_TrafficSystem *traf);

/* Traffic Light Control (L2) */
int  sc_traffic_light_add(SC_TrafficSystem *traf, const SC_TrafficLight *light);
int  sc_traffic_light_set_color(SC_TrafficSystem *traf, int light_id, SC_SignalColor color);
SC_TrafficLight* sc_traffic_light_lookup(SC_TrafficSystem *traf, int light_id);

/* Road Segment (L2) */
int  sc_traffic_road_add(SC_TrafficSystem *traf, const SC_RoadSegment *road);
SC_RoadSegment* sc_traffic_road_lookup(SC_TrafficSystem *traf, int road_id);
int  sc_traffic_road_update_congestion(SC_TrafficSystem *traf, int road_id, SC_CongestionLevel level, int vehicle_count, float avg_speed);

/* Webster Optimal Cycle Time (L5) - Webster 1958 */
int  sc_traffic_webster_cycle(int num_phases, const int *critical_flows, int saturation_flow, int lost_time_per_phase);
int  sc_traffic_webster_green_split(int cycle_time, const int *critical_flows, int num_phases, int total_lost_time);

/* Green Wave Coordination (L5) */
int  sc_traffic_green_wave_plan(SC_TrafficSystem *traf, const int *intersection_ids, int count, int direction, int target_speed_kmh);

/* Transit (L6) */
int  sc_traffic_transit_route_add(SC_TrafficSystem *traf, const SC_TransitRoute *route);
SC_TransitRoute* sc_traffic_transit_lookup(SC_TrafficSystem *traf, int route_id);
int  sc_traffic_transit_optimize_headway(SC_TrafficSystem *traf, int route_id);

/* Parking (L6) */
int  sc_traffic_parking_add(SC_TrafficSystem *traf, const SC_ParkingLot *lot);
SC_ParkingLot* sc_traffic_parking_lookup(SC_TrafficSystem *traf, int lot_id);
int  sc_traffic_parking_update_occupancy(SC_TrafficSystem *traf, int lot_id, int occupied);
int  sc_traffic_parking_find_nearest(SC_TrafficSystem *traf, double lon, double lat, int max_results, int *results);

/* Shortest Path (L5) - Dijkstra with congestion weights */
int  sc_traffic_shortest_path(SC_TrafficSystem *traf, int from_intersection, int to_intersection, int *path, int max_path);
float sc_traffic_path_congestion_score(SC_TrafficSystem *traf, const int *path, int path_len);

/* Traffic Analytics (L7) */
float sc_traffic_avg_congestion(const SC_TrafficSystem *traf);
int   sc_traffic_congestion_hotspots(const SC_TrafficSystem *traf, int *road_ids, int max);
int   sc_traffic_flow_prediction(SC_TrafficSystem *traf, int road_id, int hours_ahead);

#endif /* MINI_SMARTCITY_TRAFFIC_H */
