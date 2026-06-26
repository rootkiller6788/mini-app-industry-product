/**
 * logistics_core.h — Core Definitions for Logistics & Supply Chain
 *
 * L1: Core data structures and type definitions for the entire
 * logistics and supply chain management system.
 *
 * Covers: SKU, Order, Shipment, Location, Facility, Carrier,
 * InventoryRecord, KPIs, and all foundational typedefs.
 *
 * Course Alignment:
 *   - MIT 15.762 Supply Chain Planning
 *   - MIT CTL.SC1x Supply Chain Fundamentals
 *   - Georgia Tech ISYE 6333 Supply Chain Engineering
 *
 * Standards:
 *   - GS1 standards for product identification
 *   - ISO 9001 quality management terminology
 *   - INCOTERMS for trade terms
 */

#ifndef LOGISTICS_CORE_H
#define LOGISTICS_CORE_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

/* ============================================================
 * L1 — Core Enumerations
 * ============================================================ */

/** Order status lifecycle: from placement to delivery confirmation */
typedef enum {
    ORDER_CREATED       = 0,  /* Order placed by customer */
    ORDER_CONFIRMED     = 1,  /* Order confirmed by system */
    ORDER_ALLOCATED     = 2,  /* Inventory allocated */
    ORDER_PICKING       = 3,  /* Being picked in warehouse */
    ORDER_PACKED        = 4,  /* Packed and labeled */
    ORDER_SHIPPED       = 5,  /* Handed to carrier */
    ORDER_IN_TRANSIT    = 6,  /* In transportation */
    ORDER_OUT_FOR_DELIVERY = 7,  /* Final mile delivery */
    ORDER_DELIVERED     = 8,  /* Successfully delivered */
    ORDER_RETURNED      = 9,  /* Returned by customer */
    ORDER_CANCELLED     = 10, /* Cancelled before shipment */
    ORDER_EXCEPTION     = 11  /* Exception state (delay, damage) */
} order_status_t;

/** Shipment mode — transportation method selection */
typedef enum {
    SHIP_MODE_TRUCKLOAD     = 0,  /* FTL: Full Truckload */
    SHIP_MODE_LTL           = 1,  /* Less Than Truckload */
    SHIP_MODE_PARCEL        = 2,  /* Small package / parcel */
    SHIP_MODE_INTERMODAL    = 3,  /* Rail + Truck combination */
    SHIP_MODE_AIR_FREIGHT   = 4,  /* Air cargo */
    SHIP_MODE_OCEAN_FREIGHT = 5,  /* Ocean container */
    SHIP_MODE_PIPELINE      = 6,  /* Liquid/gas pipeline */
    SHIP_MODE_LAST_MILE     = 7,  /* Final delivery to consumer */
    SHIP_MODE_DRONE         = 8,  /* Unmanned aerial delivery */
    SHIP_MODE_AUTONOMOUS    = 9   /* Self-driving vehicle */
} shipment_mode_t;

/** Facility type — nodes in the supply chain network */
typedef enum {
    FACILITY_SUPPLIER       = 0,  /* Raw material / component supplier */
    FACILITY_MANUFACTURING  = 1,  /* Factory / production plant */
    FACILITY_DISTRIBUTION   = 2,  /* Distribution center / warehouse */
    FACILITY_CROSSDOCK      = 3,  /* Cross-docking terminal */
    FACILITY_FULFILLMENT    = 4,  /* E-commerce fulfillment center */
    FACILITY_RETAIL         = 5,  /* Retail store / outlet */
    FACILITY_RETURN_CENTER  = 6,  /* Reverse logistics hub */
    FACILITY_PORT           = 7,  /* Sea/air port */
    FACILITY_HUB            = 8   /* Transportation hub */
} facility_type_t;

/** Inventory valuation method */
typedef enum {
    VALUATION_FIFO          = 0,  /* First-In First-Out */
    VALUATION_LIFO          = 1,  /* Last-In First-Out */
    VALUATION_WEIGHTED_AVG  = 2,  /* Weighted Average Cost */
    VALUATION_SPECIFIC_ID   = 3   /* Specific Identification */
} valuation_method_t;

/** Priority levels for orders and shipments */
typedef enum {
    PRIORITY_LOW        = 0,
    PRIORITY_NORMAL     = 1,
    PRIORITY_HIGH       = 2,
    PRIORITY_CRITICAL   = 3,
    PRIORITY_EMERGENCY  = 4
} priority_t;

/** ABC classification for inventory (Pareto principle) */
typedef enum {
    ABC_CLASS_A = 0,  /* Top 20% items, 80% of value */
    ABC_CLASS_B = 1,  /* Next 30% items, 15% of value */
    ABC_CLASS_C = 2   /* Bottom 50% items, 5% of value */
} abc_class_t;

/* ============================================================
 * L1 — Core Data Structures
 * ============================================================ */

/** GPS / geographic location using WGS84 */
typedef struct {
    double latitude;    /* Degrees, -90 to +90 */
    double longitude;   /* Degrees, -180 to +180 */
    double altitude;    /* Meters above sea level (optional) */
} geo_location_t;

/** Physical address */
typedef struct {
    char    line1[128];
    char    line2[128];
    char    city[64];
    char    state_province[64];
    char    postal_code[32];
    char    country_code[4];   /* ISO 3166-1 alpha-2 */
} address_t;

/** Stock Keeping Unit — the fundamental inventory item */
typedef struct {
    char        sku_id[32];        /* Unique identifier (GS1 GTIN compatible) */
    char        description[256];  /* Human-readable description */
    char        category[64];      /* Product category */
    char        uom[8];            /* Unit of measure (EA, KG, L, M, etc.) */
    double      unit_weight_kg;    /* Weight per unit in kilograms */
    double      unit_volume_m3;    /* Volume per unit in cubic meters */
    double      unit_cost;         /* Procurement cost per unit */
    double      unit_price;        /* Selling price per unit */
    abc_class_t abc_class;         /* ABC inventory classification */
    int         shelf_life_days;   /* Maximum shelf life (0 = non-perishable) */
    int         is_hazardous;      /* 1 = hazardous material */
    int         min_order_qty;     /* Minimum order quantity from supplier */
    int         lot_size;          /* Standard lot/batch size */
} sku_t;

/** A facility node in the supply chain network */
typedef struct {
    char            facility_id[32];
    char            name[128];
    facility_type_t type;
    geo_location_t  location;
    address_t       address;
    double          storage_capacity_m3;   /* Total storage volume */
    double          throughput_per_day;    /* Processing capacity units/day */
    int             dock_count;            /* Number of loading docks */
    double          fixed_cost_per_day;    /* Fixed operating cost */
    double          variable_cost_per_unit; /* Variable handling cost */
    double          labor_hours_per_day;   /* Available labor hours */
} facility_t;

/** An individual item line within an order */
typedef struct {
    char    sku_id[32];
    int     quantity_ordered;
    int     quantity_fulfilled;
    double  unit_price;
    double  line_total;
    char    lot_number[32];
} order_line_t;

/** Customer order */
typedef struct {
    char            order_id[32];
    char            customer_id[32];
    time_t          created_at;
    time_t          due_date;
    time_t          fulfilled_at;
    order_status_t  status;
    priority_t      priority;
    order_line_t    *lines;
    int             line_count;
    int             line_capacity;
    double          subtotal;
    double          tax;
    double          shipping_cost;
    double          total;
    address_t       shipping_address;
    char            notes[256];
} order_t;

/** A stop on a delivery route */
typedef struct {
    char            stop_id[32];
    geo_location_t  location;
    address_t       address;
    char            order_id[32];
    time_t          scheduled_arrival;
    time_t          actual_arrival;
    int             sequence_number;  /* Position in route sequence */
    double          service_time_min; /* Time spent at stop */
    int             is_completed;
} route_stop_t;

/** A delivery route with ordered stops */
typedef struct {
    char            route_id[32];
    char            vehicle_id[32];
    char            driver_id[32];
    route_stop_t    *stops;
    int             stop_count;
    int             stop_capacity;
    double          total_distance_km;
    double          total_duration_min;
    double          total_load_kg;
    double          total_volume_m3;
    time_t          departure_time;
    time_t          estimated_return;
    int             is_active;
} route_t;

/** Shipment — the physical movement of goods */
typedef struct {
    char            shipment_id[32];
    char            origin_facility_id[32];
    char            dest_facility_id[32];
    char            carrier_id[32];
    char            tracking_number[64];
    shipment_mode_t mode;
    order_status_t  status;         /* Reuse order_status for shipment */
    route_t         *assigned_route;
    int             package_count;
    double          total_weight_kg;
    double          total_volume_m3;
    double          freight_cost;
    time_t          pickup_time;
    time_t          estimated_delivery;
    time_t          actual_delivery;
} shipment_t;

/** Carrier profile */
typedef struct {
    char            carrier_id[32];
    char            name[128];
    shipment_mode_t supported_modes[4];
    int             mode_count;
    double          cost_per_km;
    double          cost_per_kg;
    double          cost_per_m3;
    double          reliability_score;  /* 0.0 - 1.0 */
    int             avg_transit_days;
    int             fleet_size;
} carrier_t;

/** Inventory record — tracks stock levels for a SKU at a facility */
typedef struct {
    char            sku_id[32];
    char            facility_id[32];
    int             quantity_on_hand;
    int             quantity_allocated;   /* Reserved for orders */
    int             quantity_available;   /* On hand - allocated */
    int             quantity_on_order;    /* Inbound from supplier */
    int             reorder_point;
    int             safety_stock;
    int             max_stock;
    int             eoq;                  /* Economic Order Quantity */
    time_t          last_received;
    time_t          last_issued;
    double          avg_daily_demand;
    double          holding_cost_per_unit_day;
    double          ordering_cost;
    valuation_method_t valuation;
    abc_class_t     abc_class;
} inventory_record_t;

/** Supply chain relationship — links between facilities */
typedef struct {
    char            link_id[32];
    char            from_facility_id[32];
    char            to_facility_id[32];
    double          distance_km;
    double          transit_time_days;
    double          cost_per_unit;
    double          capacity_units_per_day;
    double          co2_per_unit_km;      /* Carbon intensity */
    shipment_mode_t default_mode;
    int             is_active;
} sc_link_t;

/** Composite KPI dashboard for supply chain performance */
typedef struct {
    /* Service level */
    double  otif_rate;             /* On-Time In-Full percentage */
    double  fill_rate;             /* Order fill rate */
    double  perfect_order_rate;    /* Perfect order percentage */

    /* Inventory metrics */
    double  inventory_turnover;    /* Annual turns = COGS / avg inventory */
    double  days_inventory_oh;     /* Days of inventory on hand */
    double  stockout_rate;         /* Stockout frequency */
    double  obsolete_pct;          /* Obsolete/slow-moving inventory % */

    /* Transportation metrics */
    double  on_time_delivery_pct;  /* On-time delivery percentage */
    double  cost_per_shipment;     /* Average shipping cost */
    double  cost_per_km;           /* Transportation cost per km */
    double  utilization_pct;       /* Vehicle capacity utilization */

    /* Financial metrics */
    double  total_logistics_cost;           /* Total logistics spend */
    double  logistics_cost_pct_revenue;     /* Logistics cost as % of revenue */
    double  gross_margin_return_on_inv;     /* GMROI */

    /* Sustainability */
    double  total_co2_tonnes;      /* Total CO2 emissions */
    double  co2_per_order;         /* CO2 per order shipped */
} sc_kpi_t;

/* ============================================================
 * L1 — API: Core Lifecycle Functions
 * ============================================================ */

/**
 * Initialize a SKU with default values.
 * Complexity: O(1)
 */
void sku_init(sku_t *sku, const char *sku_id, const char *description);

/**
 * Initialize a facility with default values.
 * Complexity: O(1)
 */
void facility_init(facility_t *fac, const char *fac_id, const char *name,
                   facility_type_t type, double lat, double lon);

/**
 * Create a new order (dynamically allocates line items array).
 * Returns 0 on success, -1 on allocation failure.
 * Complexity: O(1)
 */
int  order_create(order_t *order, const char *order_id, const char *customer_id);

/**
 * Add a line item to an order. Resizes internal array if needed.
 * Returns 0 on success, -1 on failure.
 * Complexity: O(1) amortized
 */
int  order_add_line(order_t *order, const char *sku_id, int qty, double price);

/**
 * Recalculate order totals after line changes.
 * Complexity: O(n) where n = number of lines
 */
void order_recalc_totals(order_t *order);

/**
 * Free all internally allocated memory for an order.
 * Complexity: O(1)
 */
void order_destroy(order_t *order);

/**
 * Initialize an inventory record.
 * Complexity: O(1)
 */
void inv_record_init(inventory_record_t *rec, const char *sku_id,
                     const char *facility_id);

/**
 * Create a shipment record.
 * Complexity: O(1)
 */
void shipment_init(shipment_t *ship, const char *ship_id,
                   const char *origin, const char *dest, shipment_mode_t mode);

/**
 * Initialize route with empty stop list.
 * Complexity: O(1)
 */
int  route_create(route_t *route, const char *route_id, const char *vehicle_id);

/**
 * Add a stop to a route.
 * Complexity: O(1) amortized
 */
int  route_add_stop(route_t *route, const route_stop_t *stop);

/**
 * Free route's internal stop array.
 * Complexity: O(1)
 */
void route_destroy(route_t *route);

/**
 * Initialize a supply chain link.
 * Complexity: O(1)
 */
void sc_link_init(sc_link_t *link, const char *from, const char *to,
                  double distance_km, double transit_days);

/**
 * Initialize a KPI dashboard to zero.
 * Complexity: O(1)
 */
void sc_kpi_init(sc_kpi_t *kpi);

/**
 * Calculate great-circle distance between two geo points using
 * the Haversine formula.
 *
 * Theorem: Spherical law of haversines
 *   a = sin²(Δlat/2) + cos(lat1)·cos(lat2)·sin²(Δlon/2)
 *   c = 2·atan2(√a, √(1−a))
 *   d = R·c
 * where R = 6371 km (Earth's mean radius)
 *
 * Complexity: O(1)
 * Returns distance in kilometers.
 */
double geo_distance_km(const geo_location_t *a, const geo_location_t *b);

/**
 * Calculate travel time given distance and average speed.
 * Complexity: O(1)
 * Returns time in minutes.
 */
double travel_time_min(double distance_km, double avg_speed_kmh);

#endif /* LOGISTICS_CORE_H */
