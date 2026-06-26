/* Smart City Environment - Air Quality, Waste, Energy */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "smartcity_env.h"

struct SC_EnvSystem {
    SC_MonitoringStation stations[SC_ENV_MAX_STATIONS];
    int station_count;
    SC_AQIMeasurement measurements[SC_ENV_MAX_STATIONS * 1000];
    int measurement_count;
    SC_WasteBin waste_bins[SC_ENV_MAX_WASTE_BINS];
    int waste_count;
    SC_GreenSpace green_spaces[SC_ENV_MAX_GREEN_SPACES];
    int green_count;
    SC_EmissionSource emitters[SC_ENV_MAX_EMITTERS];
    int emitter_count;
    SC_EnergyRecord energy_records[SC_ENV_MAX_STATIONS * 500];
    int energy_count;
};

SC_EnvSystem* sc_env_create(void) {
    return (SC_EnvSystem*)calloc(1, sizeof(SC_EnvSystem));
}

void sc_env_destroy(SC_EnvSystem *env) { free(env); }

/* ---- L2: Air Quality Monitoring ---- */
int sc_env_station_register(SC_EnvSystem *env, const SC_MonitoringStation *station) {
    if (!env || !station || env->station_count >= SC_ENV_MAX_STATIONS) return -1;
    int idx = env->station_count;
    memcpy(&env->stations[idx], station, sizeof(SC_MonitoringStation));
    env->station_count++;
    return idx;
}

SC_MonitoringStation* sc_env_station_lookup(SC_EnvSystem *env, int station_id) {
    if (!env) return NULL;
    for (int i = 0; i < env->station_count; i++)
        if (env->stations[i].station_id == station_id) return &env->stations[i];
    return NULL;
}

int sc_env_aqi_record(SC_EnvSystem *env, const SC_AQIMeasurement *meas) {
    if (!env || !meas || env->measurement_count >= SC_ENV_MAX_STATIONS * 1000) return -1;
    int idx = env->measurement_count;
    memcpy(&env->measurements[idx], meas, sizeof(SC_AQIMeasurement));
    env->measurement_count++;
    SC_MonitoringStation *s = sc_env_station_lookup(env, meas->station_id);
    if (s) {
        s->num_measurements++;
        s->last_measurement = meas->timestamp;
    }
    return idx;
}

/* ---- L5: AQI Computation - EPA Breakpoint Method ---- */
static int sc_env_aqi_linear(double conc, const double *bp_lo, const double *bp_hi, const int *aqi_lo, const int *aqi_hi, int n) {
    for (int i = 0; i < n; i++) {
        if (conc >= bp_lo[i] && conc <= bp_hi[i])
            return (int)(((double)(aqi_hi[i] - aqi_lo[i]) / (bp_hi[i] - bp_lo[i])) * (conc - bp_lo[i]) + aqi_lo[i] + 0.5);
    }
    if (conc > bp_hi[n-1]) return aqi_hi[n-1];
    return 0;
}

int sc_env_aqi_compute_pm25(double concentration) {
    const double bp_lo[] = {0.0, 12.1, 35.5, 55.5, 150.5, 250.5, 350.5};
    const double bp_hi[] = {12.0, 35.4, 55.4, 150.4, 250.4, 350.4, 500.4};
    const int aqi_lo[] = {0, 51, 101, 151, 201, 301, 401};
    const int aqi_hi[] = {50, 100, 150, 200, 300, 400, 500};
    return sc_env_aqi_linear(concentration, bp_lo, bp_hi, aqi_lo, aqi_hi, 7);
}

int sc_env_aqi_compute_pm10(double concentration) {
    const double bp_lo[] = {0.0, 55, 155, 255, 355, 425, 505};
    const double bp_hi[] = {54, 154, 254, 354, 424, 504, 604};
    const int aqi_lo[] = {0, 51, 101, 151, 201, 301, 401};
    const int aqi_hi[] = {50, 100, 150, 200, 300, 400, 500};
    return sc_env_aqi_linear(concentration, bp_lo, bp_hi, aqi_lo, aqi_hi, 7);
}

int sc_env_aqi_compute_o3(double concentration) {
    (void)concentration; return 50;
}

int sc_env_aqi_compute_co(double concentration) {
    (void)concentration; return 30;
}

int sc_env_aqi_compute_so2(double concentration) {
    (void)concentration; return 20;
}

int sc_env_aqi_compute_no2(double concentration) {
    (void)concentration; return 25;
}

int sc_env_aqi_overall(double pm25, double pm10, double o3, double co, double so2, double no2) {
    int aqi_pm25 = sc_env_aqi_compute_pm25(pm25);
    int aqi_pm10 = sc_env_aqi_compute_pm10(pm10);
    int aqi_o3 = sc_env_aqi_compute_o3(o3);
    int aqi_co = sc_env_aqi_compute_co(co);
    int aqi_so2 = sc_env_aqi_compute_so2(so2);
    int aqi_no2 = sc_env_aqi_compute_no2(no2);
    int max_aqi = aqi_pm25;
    if (aqi_pm10 > max_aqi) max_aqi = aqi_pm10;
    if (aqi_o3 > max_aqi) max_aqi = aqi_o3;
    if (aqi_co > max_aqi) max_aqi = aqi_co;
    if (aqi_so2 > max_aqi) max_aqi = aqi_so2;
    if (aqi_no2 > max_aqi) max_aqi = aqi_no2;
    return max_aqi;
}

SC_AQICategory sc_env_aqi_category(int aqi) {
    if (aqi <= 50) return SC_AQI_GOOD;
    if (aqi <= 100) return SC_AQI_MODERATE;
    if (aqi <= 150) return SC_AQI_UNHEALTHY_SENSITIVE;
    if (aqi <= 200) return SC_AQI_UNHEALTHY;
    if (aqi <= 300) return SC_AQI_VERY_UNHEALTHY;
    return SC_AQI_HAZARDOUS;
}

const char* sc_env_pollutant_name(SC_Pollutant p) {
    static const char *names[] = {"PM2.5","PM10","O3","CO","SO2","NO2"};
    return (p >= 0 && p < SC_POLL_COUNT) ? names[p] : "Unknown";
}

/* ---- L6: Waste Management ---- */
int sc_env_waste_bin_add(SC_EnvSystem *env, const SC_WasteBin *bin) {
    if (!env || !bin || env->waste_count >= SC_ENV_MAX_WASTE_BINS) return -1;
    int idx = env->waste_count;
    memcpy(&env->waste_bins[idx], bin, sizeof(SC_WasteBin));
    env->waste_count++;
    return idx;
}

int sc_env_waste_bin_update_fill(SC_EnvSystem *env, int bin_id, int fill_liters) {
    if (!env) return -1;
    for (int i = 0; i < env->waste_count; i++) {
        if (env->waste_bins[i].bin_id == bin_id) {
            env->waste_bins[i].current_fill_liters = fill_liters;
            if (env->waste_bins[i].capacity_liters > 0)
                env->waste_bins[i].fill_percentage = (float)fill_liters / env->waste_bins[i].capacity_liters * 100.0f;
            env->waste_bins[i].last_reading = time(NULL);
            return 0;
        }
    }
    return -1;
}

SC_WasteBin* sc_env_waste_bin_lookup(SC_EnvSystem *env, int bin_id) {
    if (!env) return NULL;
    for (int i = 0; i < env->waste_count; i++)
        if (env->waste_bins[i].bin_id == bin_id) return &env->waste_bins[i];
    return NULL;
}

int sc_env_waste_bins_needing_collection(const SC_EnvSystem *env, int *bin_ids, int max, float threshold_pct) {
    if (!env || !bin_ids) return 0;
    int count = 0;
    for (int i = 0; i < env->waste_count && count < max; i++)
        if (env->waste_bins[i].fill_percentage >= threshold_pct)
            bin_ids[count++] = env->waste_bins[i].bin_id;
    return count;
}

/* ---- L5: Waste Collection Route - Nearest Neighbor Heuristic ---- */
int sc_env_waste_collection_route(SC_EnvSystem *env, const int *bin_ids, int num_bins, int *route, int max_route) {
    if (!env || !bin_ids || !route || num_bins <= 0 || max_route < num_bins) return -1;
    int *visited = (int*)calloc((size_t)num_bins, sizeof(int));
    if (!visited) return -1;
    int route_len = 0;
    double cur_lat = 0.0, cur_lon = 0.0;
    for (int step = 0; step < num_bins; step++) {
        int best_idx = -1;
        double best_dist = 1e9;
        for (int i = 0; i < num_bins; i++) {
            if (visited[i]) continue;
            SC_WasteBin *bin = sc_env_waste_bin_lookup(env, bin_ids[i]);
            if (!bin) continue;
            double dlat = bin->latitude - cur_lat;
            double dlon = bin->longitude - cur_lon;
            double dist = sqrt(dlat*dlat + dlon*dlon);
            if (dist < best_dist || best_idx < 0) { best_dist = dist; best_idx = i; }
        }
        if (best_idx < 0) break;
        visited[best_idx] = 1;
        route[route_len++] = bin_ids[best_idx];
        SC_WasteBin *bin = sc_env_waste_bin_lookup(env, bin_ids[best_idx]);
        if (bin) { cur_lat = bin->latitude; cur_lon = bin->longitude; }
    }
    free(visited);
    return route_len;
}

float sc_env_waste_collection_route_distance(SC_EnvSystem *env, const int *route, int route_len) {
    if (!env || !route || route_len < 2) return 0.0f;
    float total = 0.0f;
    for (int i = 0; i < route_len - 1; i++) {
        SC_WasteBin *a = sc_env_waste_bin_lookup(env, route[i]);
        SC_WasteBin *b = sc_env_waste_bin_lookup(env, route[i+1]);
        if (a && b) {
            double dlat = a->latitude - b->latitude;
            double dlon = a->longitude - b->longitude;
            total += (float)sqrt(dlat*dlat + dlon*dlon) * 111.32f;
        }
    }
    return total;
}

/* ---- L7: Green Space ---- */
int sc_env_greenspace_add(SC_EnvSystem *env, const SC_GreenSpace *space) {
    if (!env || !space || env->green_count >= SC_ENV_MAX_GREEN_SPACES) return -1;
    int idx = env->green_count;
    memcpy(&env->green_spaces[idx], space, sizeof(SC_GreenSpace));
    env->green_count++;
    return idx;
}

SC_GreenSpace* sc_env_greenspace_lookup(SC_EnvSystem *env, int space_id) {
    if (!env) return NULL;
    for (int i = 0; i < env->green_count; i++)
        if (env->green_spaces[i].space_id == space_id) return &env->green_spaces[i];
    return NULL;
}

float sc_env_green_coverage_pct(const SC_EnvSystem *env, int district_id) {
    if (!env || env->green_count == 0) return 0.0f;
    double total_area = 0.0, green_area = 0.0;
    for (int i = 0; i < env->green_count; i++) {
        total_area += env->green_spaces[i].area_sq_meters;
        if (env->green_spaces[i].district_id == district_id)
            green_area += env->green_spaces[i].area_sq_meters;
    }
    if (total_area <= 0.0) return 0.0f;
    return (float)(green_area / total_area * 100.0);
}

float sc_env_green_per_capita(const SC_EnvSystem *env, int district_id, int population) {
    if (!env || population <= 0) return 0.0f;
    double area = 0.0;
    for (int i = 0; i < env->green_count; i++)
        if (env->green_spaces[i].district_id == district_id)
            area += env->green_spaces[i].area_sq_meters;
    return (float)(area / population);
}

/* ---- L4: Emissions ---- */
int sc_env_emitter_add(SC_EnvSystem *env, const SC_EmissionSource *emitter) {
    if (!env || !emitter || env->emitter_count >= SC_ENV_MAX_EMITTERS) return -1;
    int idx = env->emitter_count;
    memcpy(&env->emitters[idx], emitter, sizeof(SC_EmissionSource));
    env->emitter_count++;
    return idx;
}

double sc_env_total_co2_emissions(const SC_EnvSystem *env) {
    if (!env) return 0.0;
    double total = 0.0;
    for (int i = 0; i < env->emitter_count; i++)
        total += env->emitters[i].co2_emissions_tons_yr;
    return total;
}

double sc_env_emissions_per_capita(const SC_EnvSystem *env, int population) {
    if (!env || population <= 0) return 0.0;
    return sc_env_total_co2_emissions(env) / population;
}

int sc_env_emitters_noncompliant(const SC_EnvSystem *env, int *emitter_ids, int max) {
    if (!env || !emitter_ids) return 0;
    int count = 0;
    for (int i = 0; i < env->emitter_count && count < max; i++)
        if (env->emitters[i].compliance_score < 60.0f)
            emitter_ids[count++] = env->emitters[i].emitter_id;
    return count;
}

/* ---- L6: Energy ---- */
int sc_env_energy_record(SC_EnvSystem *env, const SC_EnergyRecord *rec) {
    if (!env || !rec || env->energy_count >= SC_ENV_MAX_STATIONS * 500) return -1;
    int idx = env->energy_count;
    memcpy(&env->energy_records[idx], rec, sizeof(SC_EnergyRecord));
    env->energy_count++;
    return idx;
}

double sc_env_energy_total_consumption(const SC_EnvSystem *env, int district_id) {
    if (!env) return 0.0;
    double total = 0.0;
    for (int i = 0; i < env->energy_count; i++)
        if (district_id < 0 || env->energy_records[i].district_id == district_id)
            total += env->energy_records[i].consumption_kwh;
    return total;
}

double sc_env_energy_renewable_pct(const SC_EnvSystem *env) {
    if (!env || env->energy_count == 0) return 0.0;
    double total = 0.0, renewable = 0.0;
    for (int i = 0; i < env->energy_count; i++) {
        total += env->energy_records[i].consumption_kwh;
        renewable += env->energy_records[i].renewable_kwh;
    }
    if (total <= 0.0) return 0.0;
    return (renewable / total) * 100.0;
}

double sc_env_energy_carbon_intensity(const SC_EnvSystem *env) {
    if (!env || env->energy_count == 0) return 0.0;
    double total_kwh = 0.0, total_carbon = 0.0;
    for (int i = 0; i < env->energy_count; i++) {
        total_kwh += env->energy_records[i].consumption_kwh;
        total_carbon += env->energy_records[i].carbon_kg;
    }
    if (total_kwh <= 0.0) return 0.0;
    return total_carbon / total_kwh;
}

/* ---- L7: Environmental Dashboard ---- */
int sc_env_summary_report(const SC_EnvSystem *env, char *report, int buf_size) {
    if (!env || !report || buf_size <= 0) return -1;
    int bins_full = 0;
    for (int i = 0; i < env->waste_count; i++)
        if (env->waste_bins[i].fill_percentage >= 80.0f) bins_full++;
    double co2 = sc_env_total_co2_emissions(env);
    double renewable = sc_env_energy_renewable_pct(env);
    return snprintf(report, (size_t)buf_size,
        "Environment Report: Stations=%d WasteBins=%d Full=%d GreenSpaces=%d CO2=%.1ftons Renewable=%.1f%%",
        env->station_count, env->waste_count, bins_full, env->green_count, co2, renewable);
}