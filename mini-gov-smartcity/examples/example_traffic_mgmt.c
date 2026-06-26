/* Example: Smart Traffic Management
 * Demonstrates Webster cycle optimization, intersection control,
 * road congestion tracking, parking management, and shortest path.
 * L6: Canonical Problem - Adaptive Traffic Signal Control */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "smartcity_traffic.h"

int main(void) {
    printf("=== Smart Traffic Management Demo ===\n\n");
    SC_TrafficSystem *traf = sc_traffic_create();

    /* Create intersections */
    SC_Intersection i1; memset(&i1, 0, sizeof(i1));
    i1.intersection_id = 1;
    strncpy(i1.name, "Main & 1st", 128);
    i1.latitude = 40.7128; i1.longitude = -74.0060;
    sc_traffic_intersection_add(traf, &i1);

    SC_Intersection i2; memset(&i2, 0, sizeof(i2));
    i2.intersection_id = 2;
    strncpy(i2.name, "Main & 2nd", 128);
    i2.latitude = 40.7135; i2.longitude = -74.0050;
    sc_traffic_intersection_add(traf, &i2);

    SC_Intersection i3; memset(&i3, 0, sizeof(i3));
    i3.intersection_id = 3;
    strncpy(i3.name, "Oak & 1st", 128);
    i3.latitude = 40.7140; i3.longitude = -74.0065;
    sc_traffic_intersection_add(traf, &i3);

    printf("Added %d intersections\n", sc_traffic_intersection_count(traf));

    /* Webster optimal cycle calculation */
    int flows[] = {600, 500};
    int cycle = sc_traffic_webster_cycle(2, flows, 1800, 4);
    printf("Webster optimal cycle: %d seconds\n", cycle);

    /* Set signal cycle for intersection 1 */
    sc_traffic_intersection_set_cycle(traf, 1, 30, 25, 3, 2);
    printf("Intersection 1 cycle: %d sec\n", cycle);

    /* Add road segments */
    SC_RoadSegment r1; memset(&r1, 0, sizeof(r1));
    r1.road_id = 101; r1.from_intersection = 1; r1.to_intersection = 2;
    r1.distance_km = 0.3; r1.speed_limit_kmh = 50; r1.num_lanes = 2;
    sc_traffic_road_add(traf, &r1);

    SC_RoadSegment r2; memset(&r2, 0, sizeof(r2));
    r2.road_id = 102; r2.from_intersection = 2; r2.to_intersection = 3;
    r2.distance_km = 0.4; r2.speed_limit_kmh = 40; r2.num_lanes = 1;
    sc_traffic_road_add(traf, &r2);

    SC_RoadSegment r3; memset(&r3, 0, sizeof(r3));
    r3.road_id = 103; r3.from_intersection = 1; r3.to_intersection = 3;
    r3.distance_km = 0.6; r3.speed_limit_kmh = 60; r3.num_lanes = 3;
    sc_traffic_road_add(traf, &r3);

    /* Update congestion */
    sc_traffic_road_update_congestion(traf, 101, SC_CONGESTION_HEAVY, 45, 25.0f);
    sc_traffic_road_update_congestion(traf, 102, SC_CONGESTION_LIGHT, 10, 38.0f);
    sc_traffic_road_update_congestion(traf, 103, SC_CONGESTION_MODERATE, 25, 45.0f);

    printf("Avg congestion level: %.2f\n", sc_traffic_avg_congestion(traf));

    /* Shortest path with congestion weights */
    int path[32];
    int plen = sc_traffic_shortest_path(traf, 1, 3, path, 32);
    printf("Shortest path from 1 to 3: ");
    for (int i = 0; i < plen; i++) printf("%d ", path[i]);
    printf("(length=%d, congestion=%.2f)\n", plen,
           sc_traffic_path_congestion_score(traf, path, plen));

    /* Parking */
    SC_ParkingLot lot; memset(&lot, 0, sizeof(lot));
    lot.lot_id = 1; lot.total_spaces = 200; lot.is_open = true;
    lot.latitude = 40.7130; lot.longitude = -74.0055;
    strncpy(lot.name, "Main St Garage", 128);
    sc_traffic_parking_add(traf, &lot);
    sc_traffic_parking_update_occupancy(traf, 1, 120);

    int near[5];
    int found = sc_traffic_parking_find_nearest(traf, 40.7128, -74.0060, 5, near);
    printf("\nNearest parking: %d lots found\n", found);
    for (int i = 0; i < found; i++) {
        SC_ParkingLot *pl = sc_traffic_parking_lookup(traf, near[i]);
        if (pl) printf("  Lot %d: %s (%d/%d spaces)\n",
                        pl->lot_id, pl->name, pl->occupied_spaces, pl->total_spaces);
    }

    /* Traffic hotspots */
    int hotspots[32];
    int nh = sc_traffic_congestion_hotspots(traf, hotspots, 32);
    printf("\nCongestion hotspots: %d\n", nh);

    /* Flow prediction */
    int pred = sc_traffic_flow_prediction(traf, 101, 2);
    printf("Predicted vehicles on road 101: %d\n", pred);

    sc_traffic_destroy(traf);
    printf("\nDemo complete.\n");
    return 0;
}