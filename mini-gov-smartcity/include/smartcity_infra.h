#ifndef MINI_SMARTCITY_INFRA_H
#define MINI_SMARTCITY_INFRA_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* ================================================================
 *  Smart City Infrastructure -- IoT, Utilities, Asset Management
 *
 *  L1: Core definitions -- Sensor, UtilityMeter, Infrastructure Asset,
 *       Maintenance Order, Utility Reading
 *  L2: Core concepts -- Condition-based maintenance, predictive
 *       asset management, utility consumption forecasting
 *  L3: Engineering structures -- Time-series sensor data ring buffer,
 *       asset hierarchy tree, maintenance scheduling queue
 *  L4: Standards -- ISO 55000 Asset Management, IEC 61850 Smart Grid,
 *       NIST Smart Grid Framework, FIWARE NGSI-LD
 *  L5: Algorithms -- Exponentially Weighted Moving Average (EWMA),
 *       anomaly detection via z-score, critical path maintenance,
 *       linear utility consumption forecasting
 *  L6: Canonical problems -- Smart grid monitoring, water distribution
 *       network management, street light maintenance
 *  L7: Applications -- Predictive maintenance dashboard, utility
 *       usage analytics, infrastructure health scoring
 *  L8: Advanced -- Digital twin for bridges/tunnels, vibration
 *       analysis for structural health monitoring
 *  L9: Industry -- Siemens MindSphere, GE Predix, FIWARE platform,
 *       Hitachi Lumada, AWS IoT TwinMaker
 *
 *  References:
 *  - ISO 55000:2014 Asset Management -- Overview, principles
 *  - IEC 61850 Communication networks for power utility automation
 * ================================================================ */

#define SC_INFRA_MAX_SENSORS         32768
#define SC_INFRA_MAX_ASSETS          65536
#define SC_INFRA_MAX_READINGS        1048576
#define SC_INFRA_MAX_MAINT_ORDERS    65536
#define SC_INFRA_SENSOR_ID_LEN       32
#define SC_INFRA_ASSET_ID_LEN        32
#define SC_INFRA_READING_UNIT_LEN    16

/* ---- L1: Sensor Type ---- */
typedef enum {
    SC_SENSOR_TEMPERATURE     = 0,
    SC_SENSOR_HUMIDITY        = 1,
    SC_SENSOR_PRESSURE        = 2,
    SC_SENSOR_VIBRATION       = 3,
    SC_SENSOR_FLOW_RATE       = 4,
    SC_SENSOR_WATER_QUALITY   = 5,
    SC_SENSOR_AIR_QUALITY     = 6,
    SC_SENSOR_NOISE           = 7,
    SC_SENSOR_POWER_QUALITY   = 8,
    SC_SENSOR_GAS_DETECTION   = 9,
    SC_SENSOR_SEISMIC         = 10,
    SC_SENSOR_MOTION          = 11,
    SC_SENSOR_LIGHT           = 12,
    SC_SENSOR_COUNT           = 13
} SC_SensorType;

/* ---- L1: Asset Type ---- */
typedef enum {
    SC_ASSET_ROAD_SEGMENT     = 0,
    SC_ASSET_BRIDGE           = 1,
    SC_ASSET_TUNNEL           = 2,
    SC_ASSET_WATER_PIPE       = 3,
    SC_ASSET_SEWER_LINE       = 4,
    SC_ASSET_POWER_LINE       = 5,
    SC_ASSET_TRANSFORMER      = 6,
    SC_ASSET_STREET_LIGHT     = 7,
    SC_ASSET_TRAFFIC_LIGHT    = 8,
    SC_ASSET_PUBLIC_BUILDING  = 9,
    SC_ASSET_PARK_FACILITY    = 10,
    SC_ASSET_WATER_TOWER      = 11,
    SC_ASSET_PUMP_STATION     = 12,
    SC_ASSET_SUBSTATION       = 13,
    SC_ASSET_COUNT            = 14
} SC_AssetType;

/* ---- L1: Asset Condition ---- */
typedef enum {
    SC_CONDITION_EXCELLENT  = 0,
    SC_CONDITION_GOOD       = 1,
    SC_CONDITION_FAIR       = 2,
    SC_CONDITION_POOR       = 3,
    SC_CONDITION_CRITICAL   = 4
} SC_AssetCondition;

/* ---- L1: Maintenance Order Status ---- */
typedef enum {
    SC_MAINT_PLANNED        = 0,
    SC_MAINT_SCHEDULED      = 1,
    SC_MAINT_IN_PROGRESS    = 2,
    SC_MAINT_COMPLETED      = 3,
    SC_MAINT_DEFERRED       = 4,
    SC_MAINT_CANCELLED      = 5
} SC_MaintenanceStatus;

/* ---- L1: Utility Type ---- */
typedef enum {
    SC_UTILITY_WATER        = 0,
    SC_UTILITY_ELECTRICITY  = 1,
    SC_UTILITY_GAS          = 2,
    SC_UTILITY_SEWER        = 3,
    SC_UTILITY_COUNT        = 4
} SC_UtilityType;

/* ---- L1: IoT Sensor ---- */
typedef struct {
    char     sensor_id[SC_INFRA_SENSOR_ID_LEN];
    SC_SensorType type;
    double   longitude;
    double   latitude;
    double   elevation;
    int      asset_id;
    int      district_id;
    int      ward_number;
    double   last_value;
    double   min_threshold;
    double   max_threshold;
    time_t   last_reading_time;
    time_t   installed_at;
    time_t   last_calibrated;
    int      calibration_interval_days;
    int      reading_interval_seconds;
    int      total_readings;
    int      anomaly_count;
    bool     is_active;
    bool     is_anomalous;
    char     unit[SC_INFRA_READING_UNIT_LEN];
} SC_Sensor;

/* ---- L1: Sensor Reading ---- */
typedef struct {
    int64_t  reading_id;
    char     sensor_id[SC_INFRA_SENSOR_ID_LEN];
    double   value;
    time_t   timestamp;
    double   z_score;
    bool     is_anomaly;
} SC_SensorReading;

/* ---- L1: Infrastructure Asset ---- */
typedef struct {
    char     asset_id[SC_INFRA_ASSET_ID_LEN];
    char     name[128];
    SC_AssetType type;
    SC_AssetCondition condition;
    double   longitude;
    double   latitude;
    int      district_id;
    int      construction_year;
    int      expected_lifetime_years;
    float    replacement_cost;
    float    annual_maintenance_budget;
    time_t   last_inspected;
    time_t   last_maintained;
    int      maintenance_count;
    float    health_score;
    bool     is_critical;
    int      parent_asset_idx;
    int      num_dependents;
} SC_InfraAsset;

/* ---- L1: Maintenance Order ---- */
typedef struct {
    int64_t  order_id;
    char     asset_id[SC_INFRA_ASSET_ID_LEN];
    char     description[512];
    SC_MaintenanceStatus status;
    int      priority;
    time_t   planned_date;
    time_t   started_at;
    time_t   completed_at;
    float    estimated_cost;
    float    actual_cost;
    int      assigned_crew;
    int      estimated_hours;
    int      actual_hours;
    char     notes[256];
} SC_MaintenanceOrder;

/* ---- L1: Utility Meter ---- */
typedef struct {
    char     meter_id[SC_INFRA_SENSOR_ID_LEN];
    SC_UtilityType utility_type;
    int      district_id;
    int      ward_number;
    double   longitude;
    double   latitude;
    double   current_reading;
    double   previous_reading;
    time_t   current_read_time;
    time_t   previous_read_time;
    int      num_connections;
    bool     is_smart_meter;
    bool     is_active;
    char     unit[SC_INFRA_READING_UNIT_LEN];
} SC_UtilityMeter;

/* ---- Infrastructure System ---- */
typedef struct SC_InfraSystem SC_InfraSystem;
SC_InfraSystem* sc_infra_create(void);
void            sc_infra_destroy(SC_InfraSystem *infra);

/* Sensor Management (L2) */
int  sc_infra_sensor_register(SC_InfraSystem *infra, const SC_Sensor *sensor);
SC_Sensor* sc_infra_sensor_lookup(SC_InfraSystem *infra, const char *sensor_id);
int  sc_infra_sensor_record_reading(SC_InfraSystem *infra, const char *sensor_id, double value, time_t ts);
int  sc_infra_sensor_get_readings(SC_InfraSystem *infra, const char *sensor_id, SC_SensorReading *buf, int max);
int  sc_infra_sensor_count_active(const SC_InfraSystem *infra);

/* Sensor Anomaly Detection (L5) - EWMA + Z-score */
double sc_infra_ewma_update(SC_InfraSystem *infra, const char *sensor_id, double new_value, double alpha);
double sc_infra_ewma_get(const SC_InfraSystem *infra, const char *sensor_id);
int    sc_infra_detect_anomaly_zscore(SC_InfraSystem *infra, const char *sensor_id, double z_threshold);

/* Asset Management (L2) */
int  sc_infra_asset_register(SC_InfraSystem *infra, const SC_InfraAsset *asset);
SC_InfraAsset* sc_infra_asset_lookup(SC_InfraSystem *infra, const char *asset_id);
int  sc_infra_asset_update_condition(SC_InfraSystem *infra, const char *asset_id, SC_AssetCondition cond);
int  sc_infra_asset_list_by_type(const SC_InfraSystem *infra, SC_AssetType type, char *results, int max);
int  sc_infra_asset_count_critical(const SC_InfraSystem *infra);
float sc_infra_asset_avg_health(const SC_InfraSystem *infra);

/* Health Score Computation (L5) */
float sc_infra_asset_compute_health(const SC_InfraAsset *asset, time_t now);
int   sc_infra_asset_rank_by_health(const SC_InfraSystem *infra, int *ranked_indices, int max);

/* Maintenance Management (L3) */
int  sc_infra_maintenance_create(SC_InfraSystem *infra, const SC_MaintenanceOrder *order);
int  sc_infra_maintenance_update_status(SC_InfraSystem *infra, int64_t order_id, SC_MaintenanceStatus status);
SC_MaintenanceOrder* sc_infra_maintenance_lookup(SC_InfraSystem *infra, int64_t order_id);
int  sc_infra_maintenance_list_pending(const SC_InfraSystem *infra, SC_MaintenanceOrder *orders, int max);

/* Utility Monitoring (L2) */
int  sc_infra_utility_meter_register(SC_InfraSystem *infra, const SC_UtilityMeter *meter);
int  sc_infra_utility_record_reading(SC_InfraSystem *infra, const char *meter_id, double reading, time_t ts);
double sc_infra_utility_consumption(const SC_InfraSystem *infra, const char *meter_id);
double sc_infra_utility_forecast(const SC_InfraSystem *infra, SC_UtilityType utype, int days_ahead);
double sc_infra_utility_total_consumption(const SC_InfraSystem *infra, SC_UtilityType utype);

/* Infrastructure Health Dashboard (L7) */
int  sc_infra_health_summary(const SC_InfraSystem *infra, char *report, int buf_size);
int  sc_infra_predictive_maintenance_alert(const SC_InfraSystem *infra, int *asset_indices, int max);

#endif /* MINI_SMARTCITY_INFRA_H */
