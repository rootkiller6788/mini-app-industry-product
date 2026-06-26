#ifndef MINI_SMARTCITY_ENV_H
#define MINI_SMARTCITY_ENV_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* ================================================================
 *  Smart City Environment -- Air, Water, Waste, Energy, Green
 *
 *  L1: Core definitions -- AQIMeasurement, WaterSample, WasteBin,
 *       EnergyUsage, GreenSpace, EmissionSource, NoiseLevel
 *  L2: Core concepts -- Air Quality Index (AQI) computation,
 *       waste collection route optimization, carbon footprint
 *       tracking, urban heat island effect monitoring
 *  L3: Engineering structures -- Time-series pollution data store,
 *       waste bin fill-level monitoring network, energy grid
 *       consumption aggregation
 *  L4: Standards -- WHO Air Quality Guidelines, US EPA AQI,
 *       ISO 14001 Environmental Management, Paris Agreement
 *       carbon accounting, EU Waste Framework Directive
 *  L5: Algorithms -- AQI sub-index computation (EPA method),
 *       vehicle routing problem for waste collection,
 *       time-series decomposition for seasonal pollution
 *  L6: Canonical problems -- Air quality monitoring network,
 *       smart waste management, energy efficiency tracking
 *  L7: Applications -- Pollution heatmap, carbon credit
 *       marketplace, green space accessibility analysis
 *  L8: Advanced -- Urban digital twin for pollution dispersion
 *       modeling, ML-based air quality forecasting
 *  L9: Industry -- BreezoMeter, AirVisual, Enevo waste sensors,
 *       Itron smart metering, Schneider Electric EcoStruxure
 *
 *  References:
 *  - US EPA AQI Technical Assistance Document (2018)
 *  - WHO Global Air Quality Guidelines (2021)
 * ================================================================ */

#define SC_ENV_MAX_STATIONS      8192
#define SC_ENV_MAX_WASTE_BINS    65536
#define SC_ENV_MAX_GREEN_SPACES  16384
#define SC_ENV_MAX_EMITTERS      32768

/* ---- L1: AQI Pollutant ---- */
typedef enum {
    SC_POLL_PM25     = 0,
    SC_POLL_PM10     = 1,
    SC_POLL_O3       = 2,
    SC_POLL_CO       = 3,
    SC_POLL_SO2      = 4,
    SC_POLL_NO2      = 5,
    SC_POLL_COUNT    = 6
} SC_Pollutant;

/* ---- L1: AQI Category ---- */
typedef enum {
    SC_AQI_GOOD                = 0,
    SC_AQI_MODERATE            = 1,
    SC_AQI_UNHEALTHY_SENSITIVE = 2,
    SC_AQI_UNHEALTHY           = 3,
    SC_AQI_VERY_UNHEALTHY      = 4,
    SC_AQI_HAZARDOUS           = 5
} SC_AQICategory;

/* ---- L1: Waste Bin Type ---- */
typedef enum {
    SC_WASTE_GENERAL    = 0,
    SC_WASTE_RECYCLABLE = 1,
    SC_WASTE_ORGANIC    = 2,
    SC_WASTE_HAZARDOUS  = 3,
    SC_WASTE_EWASTE     = 4,
    SC_WASTE_COUNT      = 5
} SC_WasteType;

/* ---- L1: Energy Source ---- */
typedef enum {
    SC_ENERGY_GRID      = 0,
    SC_ENERGY_SOLAR     = 1,
    SC_ENERGY_WIND      = 2,
    SC_ENERGY_HYDRO     = 3,
    SC_ENERGY_GEOTHERMAL = 4,
    SC_ENERGY_NUCLEAR   = 5,
    SC_ENERGY_COUNT     = 6
} SC_EnergySource;

/* ---- L1: Air Quality Measurement ---- */
typedef struct {
    int64_t  measurement_id;
    int      station_id;
    double   latitude;
    double   longitude;
    double   pm25_concentration;
    double   pm10_concentration;
    double   o3_concentration;
    double   co_concentration;
    double   so2_concentration;
    double   no2_concentration;
    int      overall_aqi;
    SC_AQICategory aqi_category;
    SC_Pollutant primary_pollutant;
    time_t   timestamp;
    double   temperature_celsius;
    double   humidity_pct;
    double   wind_speed_ms;
    int      wind_direction_deg;
} SC_AQIMeasurement;

/* ---- L1: Monitoring Station ---- */
typedef struct {
    int      station_id;
    char     name[128];
    double   latitude;
    double   longitude;
    int      district_id;
    bool     is_active;
    int      num_measurements;
    time_t   last_measurement;
    int      measurement_interval_minutes;
    double   avg_aqi_30day;
} SC_MonitoringStation;

/* ---- L1: Waste Bin ---- */
typedef struct {
    int      bin_id;
    char     location_name[128];
    double   latitude;
    double   longitude;
    SC_WasteType waste_type;
    int      capacity_liters;
    int      current_fill_liters;
    float    fill_percentage;
    time_t   last_collected;
    time_t   last_reading;
    int      collection_zone;
    bool     is_active;
    bool     needs_maintenance;
    int      days_since_collection;
} SC_WasteBin;

/* ---- L1: Green Space ---- */
typedef struct {
    int      space_id;
    char     name[128];
    double   latitude;
    double   longitude;
    double   area_sq_meters;
    int      district_id;
    int      tree_count;
    float    canopy_coverage_pct;
    bool     has_playground;
    bool     has_water_body;
    bool     has_walking_path;
    int      annual_visitors;
    bool     is_protected;
} SC_GreenSpace;

/* ---- L1: Emission Source ---- */
typedef struct {
    int      emitter_id;
    char     name[128];
    double   latitude;
    double   longitude;
    int      district_id;
    double   co2_emissions_tons_yr;
    double   so2_emissions_tons_yr;
    double   nox_emissions_tons_yr;
    double   pm_emissions_tons_yr;
    bool     has_carbon_capture;
    float    compliance_score;
    time_t   last_audit;
} SC_EmissionSource;

/* ---- L1: Energy Consumption Record ---- */
typedef struct {
    int64_t  record_id;
    int      district_id;
    SC_EnergySource source;
    double   consumption_kwh;
    double   renewable_kwh;
    double   cost;
    double   carbon_kg;
    time_t   timestamp;
} SC_EnergyRecord;

/* ---- Environment System ---- */
typedef struct SC_EnvSystem SC_EnvSystem;
SC_EnvSystem* sc_env_create(void);
void          sc_env_destroy(SC_EnvSystem *env);

/* Air Quality (L2) */
int  sc_env_station_register(SC_EnvSystem *env, const SC_MonitoringStation *station);
SC_MonitoringStation* sc_env_station_lookup(SC_EnvSystem *env, int station_id);
int  sc_env_aqi_record(SC_EnvSystem *env, const SC_AQIMeasurement *meas);

/* AQI Computation (L5) - EPA Breakpoint Method */
int  sc_env_aqi_compute_pm25(double concentration);
int  sc_env_aqi_compute_pm10(double concentration);
int  sc_env_aqi_compute_o3(double concentration);
int  sc_env_aqi_compute_co(double concentration);
int  sc_env_aqi_compute_so2(double concentration);
int  sc_env_aqi_compute_no2(double concentration);
int  sc_env_aqi_overall(double pm25, double pm10, double o3, double co, double so2, double no2);
SC_AQICategory sc_env_aqi_category(int aqi);
const char* sc_env_pollutant_name(SC_Pollutant p);

/* Waste Management (L6) */
int  sc_env_waste_bin_add(SC_EnvSystem *env, const SC_WasteBin *bin);
int  sc_env_waste_bin_update_fill(SC_EnvSystem *env, int bin_id, int fill_liters);
SC_WasteBin* sc_env_waste_bin_lookup(SC_EnvSystem *env, int bin_id);
int  sc_env_waste_bins_needing_collection(const SC_EnvSystem *env, int *bin_ids, int max, float threshold_pct);

/* Waste Collection Route (L5) - Nearest Neighbor Heuristic for VRP */
int  sc_env_waste_collection_route(SC_EnvSystem *env, const int *bin_ids, int num_bins, int *route, int max_route);
float sc_env_waste_collection_route_distance(SC_EnvSystem *env, const int *route, int route_len);

/* Green Space (L7) */
int  sc_env_greenspace_add(SC_EnvSystem *env, const SC_GreenSpace *space);
SC_GreenSpace* sc_env_greenspace_lookup(SC_EnvSystem *env, int space_id);
float sc_env_green_coverage_pct(const SC_EnvSystem *env, int district_id);
float sc_env_green_per_capita(const SC_EnvSystem *env, int district_id, int population);

/* Emissions (L4) */
int  sc_env_emitter_add(SC_EnvSystem *env, const SC_EmissionSource *emitter);
double sc_env_total_co2_emissions(const SC_EnvSystem *env);
double sc_env_emissions_per_capita(const SC_EnvSystem *env, int population);
int  sc_env_emitters_noncompliant(const SC_EnvSystem *env, int *emitter_ids, int max);

/* Energy (L6) */
int  sc_env_energy_record(SC_EnvSystem *env, const SC_EnergyRecord *rec);
double sc_env_energy_total_consumption(const SC_EnvSystem *env, int district_id);
double sc_env_energy_renewable_pct(const SC_EnvSystem *env);
double sc_env_energy_carbon_intensity(const SC_EnvSystem *env);

/* Environmental Dashboard (L7) */
int  sc_env_summary_report(const SC_EnvSystem *env, char *report, int buf_size);

#endif /* MINI_SMARTCITY_ENV_H */
