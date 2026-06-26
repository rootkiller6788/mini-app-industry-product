/* Smart City Traffic - Signals, Transit, Routing */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include "smartcity_traffic.h"

struct SC_TrafficSystem {
    SC_Intersection intersections[SC_TRAFFIC_MAX_INTERSECTIONS];
    int intersection_count;
    SC_TrafficLight lights[SC_TRAFFIC_MAX_INTERSECTIONS * 4];
    int light_count;
    SC_RoadSegment roads[SC_TRAFFIC_MAX_ROADS];
    int road_count;
    SC_TransitRoute transit_routes[SC_TRAFFIC_MAX_TRANSIT_ROUTES];
    int transit_count;
    SC_ParkingLot parking_lots[SC_TRAFFIC_MAX_PARKING_LOTS];
    int parking_count;
};

SC_TrafficSystem* sc_traffic_create(void) {
    SC_TrafficSystem *traf = (SC_TrafficSystem*)calloc(1, sizeof(SC_TrafficSystem));
    return traf;
}

void sc_traffic_destroy(SC_TrafficSystem *traf) { free(traf); }

/* ---- L2: Intersection Management ---- */
int sc_traffic_intersection_add(SC_TrafficSystem *traf, const SC_Intersection *isec) {
    if (!traf || !isec || traf->intersection_count >= SC_TRAFFIC_MAX_INTERSECTIONS) return -1;
    int idx = traf->intersection_count;
    memcpy(&traf->intersections[idx], isec, sizeof(SC_Intersection));
    traf->intersection_count++;
    return idx;
}

SC_Intersection* sc_traffic_intersection_lookup(SC_TrafficSystem *traf, int id) {
    if (!traf) return NULL;
    for (int i = 0; i < traf->intersection_count; i++)
        if (traf->intersections[i].intersection_id == id)
            return &traf->intersections[i];
    return NULL;
}

int sc_traffic_intersection_set_cycle(SC_TrafficSystem *traf, int id, int green_ns, int green_ew, int yellow, int all_red) {
    SC_Intersection *isec = sc_traffic_intersection_lookup(traf, id);
    if (!isec) return -1;
    isec->green_time_ns = green_ns;
    isec->green_time_ew = green_ew;
    isec->yellow_time = yellow;
    isec->all_red_time = all_red;
    isec->cycle_time_seconds = green_ns + yellow + all_red + green_ew + yellow + all_red;
    return 0;
}

int sc_traffic_intersection_count(const SC_TrafficSystem *traf) {
    return traf ? traf->intersection_count : 0;
}

/* ---- L2: Traffic Light Control ---- */
int sc_traffic_light_add(SC_TrafficSystem *traf, const SC_TrafficLight *light) {
    if (!traf || !light || traf->light_count >= SC_TRAFFIC_MAX_INTERSECTIONS * 4) return -1;
    int idx = traf->light_count;
    memcpy(&traf->lights[idx], light, sizeof(SC_TrafficLight));
    traf->light_count++;
    return idx;
}

int sc_traffic_light_set_color(SC_TrafficSystem *traf, int light_id, SC_SignalColor color) {
    if (!traf) return -1;
    for (int i = 0; i < traf->light_count; i++) {
        if (traf->lights[i].light_id == light_id) {
            traf->lights[i].current_color = color;
            traf->lights[i].time_in_current = 0;
            return 0;
        }
    }
    return -1;
}

SC_TrafficLight* sc_traffic_light_lookup(SC_TrafficSystem *traf, int light_id) {
    if (!traf) return NULL;
    for (int i = 0; i < traf->light_count; i++)
        if (traf->lights[i].light_id == light_id) return &traf->lights[i];
    return NULL;
}

/* ---- L2: Road Segments ---- */
int sc_traffic_road_add(SC_TrafficSystem *traf, const SC_RoadSegment *road) {
    if (!traf || !road || traf->road_count >= SC_TRAFFIC_MAX_ROADS) return -1;
    int idx = traf->road_count;
    memcpy(&traf->roads[idx], road, sizeof(SC_RoadSegment));
    traf->road_count++;
    return idx;
}

SC_RoadSegment* sc_traffic_road_lookup(SC_TrafficSystem *traf, int road_id) {
    if (!traf) return NULL;
    for (int i = 0; i < traf->road_count; i++)
        if (traf->roads[i].road_id == road_id) return &traf->roads[i];
    return NULL;
}

int sc_traffic_road_update_congestion(SC_TrafficSystem *traf, int road_id, SC_CongestionLevel level, int vehicle_count, float avg_speed) {
    SC_RoadSegment *r = sc_traffic_road_lookup(traf, road_id);
    if (!r) return -1;
    r->current_congestion = level;
    r->current_vehicle_count = vehicle_count;
    r->avg_speed = avg_speed;
    r->last_updated = time(NULL);
    return 0;
}

/* ---- L5: Webster Optimal Cycle Time (Webster 1958) ---- */
int sc_traffic_webster_cycle(int num_phases, const int *critical_flows, int saturation_flow, int lost_time_per_phase) {
    if (num_phases <= 0 || !critical_flows || saturation_flow <= 0) return 60;
    int total_lost_time = num_phases * lost_time_per_phase;
    double sum_y = 0.0;
    for (int i = 0; i < num_phases; i++)
        sum_y += (double)critical_flows[i] / (double)saturation_flow;
    if (sum_y >= 0.95) return 120;
    double C = (1.5 * total_lost_time + 5.0) / (1.0 - sum_y);
    if (C < 30.0) C = 30.0;
    if (C > 120.0) C = 120.0;
    return (int)(C + 0.5);
}

int sc_traffic_webster_green_split(int cycle_time, const int *critical_flows, int num_phases, int total_lost_time) {
    (void)critical_flows; (void)num_phases;
    return (cycle_time - total_lost_time) / 2;
}

/* ---- L5: Green Wave Coordination ---- */
int sc_traffic_green_wave_plan(SC_TrafficSystem *traf, const int *intersection_ids, int count, int direction, int target_speed_kmh) {
    if (!traf || !intersection_ids || count < 2 || target_speed_kmh <= 0) return -1;
    for (int i = 0; i < count; i++) {
        SC_Intersection *isec = sc_traffic_intersection_lookup(traf, intersection_ids[i]);
        if (!isec) continue;
        double dist_km = 0.0;
        if (i > 0) {
            SC_Intersection *prev = sc_traffic_intersection_lookup(traf, intersection_ids[i-1]);
            if (prev) {
                double dlat = isec->latitude - prev->latitude;
                double dlon = isec->longitude - prev->longitude;
                dist_km = sqrt(dlat*dlat + dlon*dlon) * 111.32;
            }
        }
        int offset_sec = (int)((dist_km / target_speed_kmh) * 3600.0);
        isec->cycle_time_seconds = 60;
        isec->green_time_ns = 25 + (direction % 2) * offset_sec;
    }
    return 0;
}

/* ---- L6: Transit Routes ---- */
int sc_traffic_transit_route_add(SC_TrafficSystem *traf, const SC_TransitRoute *route) {
    if (!traf || !route || traf->transit_count >= SC_TRAFFIC_MAX_TRANSIT_ROUTES) return -1;
    int idx = traf->transit_count;
    memcpy(&traf->transit_routes[idx], route, sizeof(SC_TransitRoute));
    traf->transit_count++;
    return idx;
}

SC_TransitRoute* sc_traffic_transit_lookup(SC_TrafficSystem *traf, int route_id) {
    if (!traf) return NULL;
    for (int i = 0; i < traf->transit_count; i++)
        if (traf->transit_routes[i].route_id == route_id) return &traf->transit_routes[i];
    return NULL;
}

int sc_traffic_transit_optimize_headway(SC_TrafficSystem *traf, int route_id) {
    SC_TransitRoute *route = sc_traffic_transit_lookup(traf, route_id);
    if (!route) return -1;
    if (route->daily_ridership > 10000) route->headway_minutes = 5;
    else if (route->daily_ridership > 5000) route->headway_minutes = 10;
    else if (route->daily_ridership > 2000) route->headway_minutes = 15;
    else route->headway_minutes = 20;
    return route->headway_minutes;
}

/* ---- L6: Parking Management ---- */
int sc_traffic_parking_add(SC_TrafficSystem *traf, const SC_ParkingLot *lot) {
    if (!traf || !lot || traf->parking_count >= SC_TRAFFIC_MAX_PARKING_LOTS) return -1;
    int idx = traf->parking_count;
    memcpy(&traf->parking_lots[idx], lot, sizeof(SC_ParkingLot));
    traf->parking_count++;
    return idx;
}

SC_ParkingLot* sc_traffic_parking_lookup(SC_TrafficSystem *traf, int lot_id) {
    if (!traf) return NULL;
    for (int i = 0; i < traf->parking_count; i++)
        if (traf->parking_lots[i].lot_id == lot_id) return &traf->parking_lots[i];
    return NULL;
}

int sc_traffic_parking_update_occupancy(SC_TrafficSystem *traf, int lot_id, int occupied) {
    SC_ParkingLot *lot = sc_traffic_parking_lookup(traf, lot_id);
    if (!lot) return -1;
    if (occupied < 0) occupied = 0;
    if (occupied > lot->total_spaces) occupied = lot->total_spaces;
    lot->occupied_spaces = occupied;
    lot->last_updated = time(NULL);
    return 0;
}

int sc_traffic_parking_find_nearest(SC_TrafficSystem *traf, double lon, double lat, int max_results, int *results) {
    if (!traf || !results || max_results <= 0) return 0;
    double *distances = (double*)malloc((size_t)traf->parking_count * sizeof(double));
    if (!distances) return 0;
    int *indices = (int*)malloc((size_t)traf->parking_count * sizeof(int));
    if (!indices) { free(distances); return 0; }
    for (int i = 0; i < traf->parking_count; i++) {
        double dlat = traf->parking_lots[i].latitude - lat;
        double dlon = traf->parking_lots[i].longitude - lon;
        distances[i] = sqrt(dlat*dlat + dlon*dlon);
        indices[i] = i;
    }
    for (int i = 0; i < traf->parking_count - 1; i++)
        for (int j = i + 1; j < traf->parking_count; j++)
            if (distances[j] < distances[i]) {
                double td = distances[i]; distances[i] = distances[j]; distances[j] = td;
                int ti = indices[i]; indices[i] = indices[j]; indices[j] = ti;
            }
    int n = traf->parking_count;
    if (n > max_results) n = max_results;
    for (int i = 0; i < n; i++) results[i] = indices[i];
    free(distances); free(indices);
    return n;
}

/* ---- L5: Shortest Path - Dijkstra with congestion weights ---- */
int sc_traffic_shortest_path(SC_TrafficSystem *traf, int from_intersection, int to_intersection, int *path, int max_path) {
    if (!traf || !path || max_path <= 0) return -1;
    if (from_intersection == to_intersection) { path[0] = from_intersection; return 1; }
    int n = traf->intersection_count;
    if (n == 0) return -1;
    double *dist = (double*)malloc((size_t)n * sizeof(double));
    int *prev = (int*)malloc((size_t)n * sizeof(int));
    bool *visited = (bool*)calloc((size_t)n, sizeof(bool));
    if (!dist || !prev || !visited) { free(dist); free(prev); free(visited); return -1; }
    for (int i = 0; i < n; i++) { dist[i] = DBL_MAX; prev[i] = -1; }
    dist[from_intersection] = 0.0;
    for (int iter = 0; iter < n; iter++) {
        int u = -1;
        double min_dist = DBL_MAX;
        for (int i = 0; i < n; i++)
            if (!visited[i] && dist[i] < min_dist) { min_dist = dist[i]; u = i; }
        if (u < 0 || u == to_intersection) break;
        visited[u] = true;
        for (int r = 0; r < traf->road_count; r++) {
            SC_RoadSegment *road = &traf->roads[r];
            int v = -1;
            if (road->from_intersection == u) v = road->to_intersection;
            else if (!road->is_one_way && road->to_intersection == u) v = road->from_intersection;
            if (v < 0 || v >= n) continue;
            if (visited[v]) continue;
            double weight = road->distance_km;
            if (road->current_congestion >= SC_CONGESTION_HEAVY) weight *= 3.0;
            else if (road->current_congestion >= SC_CONGESTION_MODERATE) weight *= 1.5;
            double alt = dist[u] + weight;
            if (alt < dist[v]) { dist[v] = alt; prev[v] = u; }
        }
    }
    if (dist[to_intersection] == DBL_MAX) { free(dist); free(prev); free(visited); return -1; }
    int path_len = 0;
    int *rev_path = (int*)malloc((size_t)n * sizeof(int));
    if (!rev_path) { free(dist); free(prev); free(visited); return -1; }
    for (int at = to_intersection; at != -1; at = prev[at]) rev_path[path_len++] = at;
    int result = path_len;
    if (result > max_path) result = max_path;
    for (int i = 0; i < result; i++) path[i] = rev_path[path_len - 1 - i];
    free(rev_path); free(dist); free(prev); free(visited);
    return result;
}

float sc_traffic_path_congestion_score(SC_TrafficSystem *traf, const int *path, int path_len) {
    if (!traf || !path || path_len <= 0) return 0.0f;
    float score = 0.0f;
    for (int i = 0; i < path_len - 1; i++) {
        for (int r = 0; r < traf->road_count; r++) {
            if ((traf->roads[r].from_intersection == path[i] && traf->roads[r].to_intersection == path[i+1]) ||
                (!traf->roads[r].is_one_way && traf->roads[r].to_intersection == path[i] && traf->roads[r].from_intersection == path[i+1])) {
                score += (float)(traf->roads[r].current_congestion);
                break;
            }
        }
    }
    return score / (float)(path_len - 1);
}

/* ---- L7: Traffic Analytics ---- */
float sc_traffic_avg_congestion(const SC_TrafficSystem *traf) {
    if (!traf || traf->road_count == 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < traf->road_count; i++)
        sum += (float)traf->roads[i].current_congestion;
    return sum / (float)traf->road_count;
}

int sc_traffic_congestion_hotspots(const SC_TrafficSystem *traf, int *road_ids, int max) {
    if (!traf || !road_ids) return 0;
    int count = 0;
    for (int i = 0; i < traf->road_count && count < max; i++)
        if (traf->roads[i].current_congestion >= SC_CONGESTION_HEAVY)
            road_ids[count++] = traf->roads[i].road_id;
    return count;
}

int sc_traffic_flow_prediction(SC_TrafficSystem *traf, int road_id, int hours_ahead) {
    (void)hours_ahead;
    SC_RoadSegment *r = sc_traffic_road_lookup(traf, road_id);
    if (!r) return -1;
    int base = r->current_vehicle_count;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    int hour = tm_info->tm_hour;
    if (hour >= 7 && hour <= 9) base = (int)(base * 1.5);
    else if (hour >= 16 && hour <= 19) base = (int)(base * 1.8);
    else if (hour >= 22 || hour <= 5) base = (int)(base * 0.3);
    return base;
}