/**
 * logistics_core.c — Core Implementations for Logistics & Supply Chain
 *
 * Implements all core lifecycle functions: SKU, Order, Facility,
 * Shipment, Route, Inventory Record, Supply Chain Link operations,
 * and the Haversine distance formula.
 *
 * L1: Core type lifecycle functions
 * L4: Haversine distance formula (spherical geometry theorem)
 */

#include "logistics_core.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Earth's mean radius in kilometers (WGS84) */
#define EARTH_RADIUS_KM 6371.0

/* Initial capacity for order lines and route stops */
#define INITIAL_CAPACITY 8

/* π constant */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================
 * Helper: degrees to radians
 * ============================================================ */
static double deg2rad(double deg) {
    return deg * M_PI / 180.0;
}

/* ============================================================
 * SKU Lifecycle
 * ============================================================ */
void sku_init(sku_t *sku, const char *sku_id, const char *description) {
    if (!sku) return;
    memset(sku, 0, sizeof(sku_t));
    if (sku_id) {
        strncpy(sku->sku_id, sku_id, sizeof(sku->sku_id) - 1);
        sku->sku_id[sizeof(sku->sku_id) - 1] = '\0';
    }
    if (description) {
        strncpy(sku->description, description, sizeof(sku->description) - 1);
        sku->description[sizeof(sku->description) - 1] = '\0';
    }
    strncpy(sku->uom, "EA", sizeof(sku->uom) - 1);
    sku->abc_class = ABC_CLASS_C;
    sku->shelf_life_days = 0;
    sku->is_hazardous = 0;
    sku->min_order_qty = 1;
    sku->lot_size = 1;
    sku->unit_weight_kg = 0.0;
    sku->unit_volume_m3 = 0.0;
    sku->unit_cost = 0.0;
    sku->unit_price = 0.0;
}

/* ============================================================
 * Facility Lifecycle
 * ============================================================ */
void facility_init(facility_t *fac, const char *fac_id, const char *name,
                   facility_type_t type, double lat, double lon) {
    if (!fac) return;
    memset(fac, 0, sizeof(facility_t));
    if (fac_id) {
        strncpy(fac->facility_id, fac_id, sizeof(fac->facility_id) - 1);
        fac->facility_id[sizeof(fac->facility_id) - 1] = '\0';
    }
    if (name) {
        strncpy(fac->name, name, sizeof(fac->name) - 1);
        fac->name[sizeof(fac->name) - 1] = '\0';
    }
    fac->type = type;
    fac->location.latitude = lat;
    fac->location.longitude = lon;
    fac->location.altitude = 0.0;
    fac->dock_count = 0;
    fac->storage_capacity_m3 = 0.0;
    fac->throughput_per_day = 0.0;
    fac->fixed_cost_per_day = 0.0;
    fac->variable_cost_per_unit = 0.0;
    fac->labor_hours_per_day = 0.0;
}

/* ============================================================
 * Order Lifecycle
 * ============================================================ */
int order_create(order_t *order, const char *order_id, const char *customer_id) {
    if (!order) return -1;
    memset(order, 0, sizeof(order_t));
    if (order_id) {
        strncpy(order->order_id, order_id, sizeof(order->order_id) - 1);
        order->order_id[sizeof(order->order_id) - 1] = '\0';
    }
    if (customer_id) {
        strncpy(order->customer_id, customer_id, sizeof(order->customer_id) - 1);
        order->customer_id[sizeof(order->customer_id) - 1] = '\0';
    }
    order->lines = (order_line_t *)malloc(INITIAL_CAPACITY * sizeof(order_line_t));
    if (!order->lines) return -1;
    order->line_count = 0;
    order->line_capacity = INITIAL_CAPACITY;
    order->status = ORDER_CREATED;
    order->priority = PRIORITY_NORMAL;
    order->created_at = time(NULL);
    order->due_date = order->created_at + (7 * 86400); /* Default: 7 days */
    order->fulfilled_at = 0;
    order->subtotal = 0.0;
    order->tax = 0.0;
    order->shipping_cost = 0.0;
    order->total = 0.0;
    return 0;
}

int order_add_line(order_t *order, const char *sku_id, int qty, double price) {
    if (!order || !order->lines || qty <= 0) return -1;

    /* Resize if needed */
    if (order->line_count >= order->line_capacity) {
        int new_cap = order->line_capacity * 2;
        order_line_t *new_lines = (order_line_t *)realloc(order->lines,
            new_cap * sizeof(order_line_t));
        if (!new_lines) return -1;
        order->lines = new_lines;
        order->line_capacity = new_cap;
    }

    order_line_t *line = &order->lines[order->line_count];
    memset(line, 0, sizeof(order_line_t));
    if (sku_id) {
        strncpy(line->sku_id, sku_id, sizeof(line->sku_id) - 1);
        line->sku_id[sizeof(line->sku_id) - 1] = '\0';
    }
    line->quantity_ordered = qty;
    line->quantity_fulfilled = 0;
    line->unit_price = price;
    line->line_total = (double)qty * price;
    order->line_count++;
    order_recalc_totals(order);
    return 0;
}

void order_recalc_totals(order_t *order) {
    if (!order) return;
    double subtotal = 0.0;
    for (int i = 0; i < order->line_count; i++) {
        subtotal += order->lines[i].line_total;
    }
    order->subtotal = subtotal;
    order->tax = subtotal * 0.0; /* Tax is externally set */
    order->total = order->subtotal + order->tax + order->shipping_cost;
}

void order_destroy(order_t *order) {
    if (!order) return;
    free(order->lines);
    order->lines = NULL;
    order->line_count = 0;
    order->line_capacity = 0;
}

/* ============================================================
 * Inventory Record Lifecycle
 * ============================================================ */
void inv_record_init(inventory_record_t *rec, const char *sku_id,
                     const char *facility_id) {
    if (!rec) return;
    memset(rec, 0, sizeof(inventory_record_t));
    if (sku_id) {
        strncpy(rec->sku_id, sku_id, sizeof(rec->sku_id) - 1);
        rec->sku_id[sizeof(rec->sku_id) - 1] = '\0';
    }
    if (facility_id) {
        strncpy(rec->facility_id, facility_id, sizeof(rec->facility_id) - 1);
        rec->facility_id[sizeof(rec->facility_id) - 1] = '\0';
    }
    rec->quantity_on_hand = 0;
    rec->quantity_allocated = 0;
    rec->quantity_available = 0;
    rec->quantity_on_order = 0;
    rec->reorder_point = 0;
    rec->safety_stock = 0;
    rec->max_stock = 0;
    rec->eoq = 0;
    rec->last_received = 0;
    rec->last_issued = 0;
    rec->avg_daily_demand = 0.0;
    rec->holding_cost_per_unit_day = 0.0;
    rec->ordering_cost = 0.0;
    rec->valuation = VALUATION_FIFO;
    rec->abc_class = ABC_CLASS_C;
}

/* ============================================================
 * Shipment Lifecycle
 * ============================================================ */
void shipment_init(shipment_t *ship, const char *ship_id,
                   const char *origin, const char *dest, shipment_mode_t mode) {
    if (!ship) return;
    memset(ship, 0, sizeof(shipment_t));
    if (ship_id) {
        strncpy(ship->shipment_id, ship_id, sizeof(ship->shipment_id) - 1);
        ship->shipment_id[sizeof(ship->shipment_id) - 1] = '\0';
    }
    if (origin) {
        strncpy(ship->origin_facility_id, origin, sizeof(ship->origin_facility_id) - 1);
        ship->origin_facility_id[sizeof(ship->origin_facility_id) - 1] = '\0';
    }
    if (dest) {
        strncpy(ship->dest_facility_id, dest, sizeof(ship->dest_facility_id) - 1);
        ship->dest_facility_id[sizeof(ship->dest_facility_id) - 1] = '\0';
    }
    ship->mode = mode;
    ship->status = ORDER_CREATED;
    ship->assigned_route = NULL;
    ship->package_count = 0;
    ship->total_weight_kg = 0.0;
    ship->total_volume_m3 = 0.0;
    ship->freight_cost = 0.0;
    ship->pickup_time = 0;
    ship->estimated_delivery = 0;
    ship->actual_delivery = 0;
}

/* ============================================================
 * Route Lifecycle
 * ============================================================ */
int route_create(route_t *route, const char *route_id, const char *vehicle_id) {
    if (!route) return -1;
    memset(route, 0, sizeof(route_t));
    if (route_id) {
        strncpy(route->route_id, route_id, sizeof(route->route_id) - 1);
        route->route_id[sizeof(route->route_id) - 1] = '\0';
    }
    if (vehicle_id) {
        strncpy(route->vehicle_id, vehicle_id, sizeof(route->vehicle_id) - 1);
        route->vehicle_id[sizeof(route->vehicle_id) - 1] = '\0';
    }
    route->stops = (route_stop_t *)malloc(INITIAL_CAPACITY * sizeof(route_stop_t));
    if (!route->stops) return -1;
    route->stop_count = 0;
    route->stop_capacity = INITIAL_CAPACITY;
    route->total_distance_km = 0.0;
    route->total_duration_min = 0.0;
    route->total_load_kg = 0.0;
    route->total_volume_m3 = 0.0;
    route->departure_time = time(NULL);
    route->is_active = 0;
    return 0;
}

int route_add_stop(route_t *route, const route_stop_t *stop) {
    if (!route || !route->stops || !stop) return -1;

    if (route->stop_count >= route->stop_capacity) {
        int new_cap = route->stop_capacity * 2;
        route_stop_t *new_stops = (route_stop_t *)realloc(route->stops,
            new_cap * sizeof(route_stop_t));
        if (!new_stops) return -1;
        route->stops = new_stops;
        route->stop_capacity = new_cap;
    }

    memcpy(&route->stops[route->stop_count], stop, sizeof(route_stop_t));
    route->stop_count++;
    return 0;
}

void route_destroy(route_t *route) {
    if (!route) return;
    free(route->stops);
    route->stops = NULL;
    route->stop_count = 0;
    route->stop_capacity = 0;
}

/* ============================================================
 * Supply Chain Link Lifecycle
 * ============================================================ */
void sc_link_init(sc_link_t *link, const char *from, const char *to,
                  double distance_km, double transit_days) {
    if (!link) return;
    memset(link, 0, sizeof(sc_link_t));
    if (from) {
        strncpy(link->from_facility_id, from, sizeof(link->from_facility_id) - 1);
        link->from_facility_id[sizeof(link->from_facility_id) - 1] = '\0';
    }
    if (to) {
        strncpy(link->to_facility_id, to, sizeof(link->to_facility_id) - 1);
        link->to_facility_id[sizeof(link->to_facility_id) - 1] = '\0';
    }
    link->distance_km = distance_km;
    link->transit_time_days = transit_days;
    link->cost_per_unit = 0.0;
    link->capacity_units_per_day = 0.0;
    link->co2_per_unit_km = 0.0;
    link->default_mode = SHIP_MODE_TRUCKLOAD;
    link->is_active = 1;
}

/* ============================================================
 * KPI Lifecycle
 * ============================================================ */
void sc_kpi_init(sc_kpi_t *kpi) {
    if (!kpi) return;
    memset(kpi, 0, sizeof(sc_kpi_t));
}

/* ============================================================
 * L4 — Haversine Distance (Spherical Law of Haversines)
 * ============================================================
 *
 * For two points with latitudes φ1, φ2 and longitudes λ1, λ2
 * on a sphere of radius R:
 *
 *   a = sin²(Δφ/2) + cos(φ1)·cos(φ2)·sin²(Δλ/2)
 *   c = 2·atan2(√a, √(1−a))
 *   d = R·c
 *
 * This formula is well-conditioned for all distances including
 * very small ones (unlike the spherical law of cosines).
 *
 * Reference: Sinnott, R.W. (1984). "Virtues of the Haversine".
 * Sky and Telescope, 68(2), 159.
 */
double geo_distance_km(const geo_location_t *a, const geo_location_t *b) {
    if (!a || !b) return 0.0;

    double lat1 = deg2rad(a->latitude);
    double lon1 = deg2rad(a->longitude);
    double lat2 = deg2rad(b->latitude);
    double lon2 = deg2rad(b->longitude);

    double dlat = lat2 - lat1;
    double dlon = lon2 - lon1;

    double sin_dlat = sin(dlat / 2.0);
    double sin_dlon = sin(dlon / 2.0);

    double a_val = sin_dlat * sin_dlat +
                   cos(lat1) * cos(lat2) * sin_dlon * sin_dlon;

    double c = 2.0 * atan2(sqrt(a_val), sqrt(1.0 - a_val));

    return EARTH_RADIUS_KM * c;
}

double travel_time_min(double distance_km, double avg_speed_kmh) {
    if (avg_speed_kmh <= 0.0) return 0.0;
    return (distance_km / avg_speed_kmh) * 60.0;
}

/* ============================================================
 * Carrier Lifecycle
 * ============================================================ */
void carrier_init(carrier_t *carrier, const char *carrier_id, const char *name) {
    if (!carrier) return;
    memset(carrier, 0, sizeof(carrier_t));
    if (carrier_id) {
        strncpy(carrier->carrier_id, carrier_id, sizeof(carrier->carrier_id) - 1);
        carrier->carrier_id[sizeof(carrier->carrier_id) - 1] = '\0';
    }
    if (name) {
        strncpy(carrier->name, name, sizeof(carrier->name) - 1);
        carrier->name[sizeof(carrier->name) - 1] = '\0';
    }
    carrier->mode_count = 0;
    carrier->cost_per_km = 0.0;
    carrier->cost_per_kg = 0.0;
    carrier->cost_per_m3 = 0.0;
    carrier->reliability_score = 0.5;
    carrier->avg_transit_days = 3;
    carrier->fleet_size = 0;
}

/**
 * Set supported modes for a carrier.
 * Complexity: O(1)
 */
void carrier_add_mode(carrier_t *carrier, shipment_mode_t mode) {
    if (!carrier || carrier->mode_count >= 4) return;
    /* Check for duplicates */
    for (int i = 0; i < carrier->mode_count; i++) {
        if (carrier->supported_modes[i] == mode) return;
    }
    carrier->supported_modes[carrier->mode_count++] = mode;
}

/**
 * Check if a carrier supports a given mode.
 * Complexity: O(modes)
 */
int carrier_supports_mode(const carrier_t *carrier, shipment_mode_t mode) {
    if (!carrier) return 0;
    for (int i = 0; i < carrier->mode_count; i++) {
        if (carrier->supported_modes[i] == mode) return 1;
    }
    return 0;
}
