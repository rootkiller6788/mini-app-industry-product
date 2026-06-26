/* Smart City Infrastructure - IoT, Assets, Maintenance */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "smartcity_infra.h"

struct SC_InfraSystem {
    SC_Sensor sensors[SC_INFRA_MAX_SENSORS];
    int sensor_count;
    SC_SensorReading readings[SC_INFRA_MAX_READINGS];
    int reading_count;
    SC_InfraAsset assets[SC_INFRA_MAX_ASSETS];
    int asset_count;
    SC_MaintenanceOrder maint_orders[SC_INFRA_MAX_MAINT_ORDERS];
    int maint_count;
    int next_maint_id;
    SC_UtilityMeter meters[SC_INFRA_MAX_SENSORS];
    int meter_count;
    double ewma_values[SC_INFRA_MAX_SENSORS];
    double ewma_variances[SC_INFRA_MAX_SENSORS];
    int ewma_sensor_map[SC_INFRA_MAX_SENSORS];
    int ewma_count;
};

SC_InfraSystem* sc_infra_create(void) {
    SC_InfraSystem *infra = (SC_InfraSystem*)calloc(1, sizeof(SC_InfraSystem));
    if (!infra) return NULL;
    infra->next_maint_id = 5000;
    return infra;
}

void sc_infra_destroy(SC_InfraSystem *infra) { free(infra); }

/* ---- L2: Sensor Management ---- */
int sc_infra_sensor_register(SC_InfraSystem *infra, const SC_Sensor *sensor) {
    if (!infra || !sensor || infra->sensor_count >= SC_INFRA_MAX_SENSORS) return -1;
    int idx = infra->sensor_count;
    memcpy(&infra->sensors[idx], sensor, sizeof(SC_Sensor));
    infra->sensor_count++;
    return idx;
}

SC_Sensor* sc_infra_sensor_lookup(SC_InfraSystem *infra, const char *sensor_id) {
    if (!infra || !sensor_id) return NULL;
    for (int i = 0; i < infra->sensor_count; i++)
        if (strcmp(infra->sensors[i].sensor_id, sensor_id) == 0)
            return &infra->sensors[i];
    return NULL;
}

int sc_infra_sensor_record_reading(SC_InfraSystem *infra, const char *sensor_id, double value, time_t ts) {
    SC_Sensor *s = sc_infra_sensor_lookup(infra, sensor_id);
    if (!s || infra->reading_count >= SC_INFRA_MAX_READINGS) return -1;
    s->last_value = value;
    s->last_reading_time = ts;
    s->total_readings++;
    SC_SensorReading *r = &infra->readings[infra->reading_count++];
    r->reading_id = infra->reading_count;
    strncpy(r->sensor_id, sensor_id, SC_INFRA_SENSOR_ID_LEN - 1);
    r->value = value;
    r->timestamp = ts;
    r->is_anomaly = (value < s->min_threshold || value > s->max_threshold);
    if (r->is_anomaly) s->anomaly_count++;
    return 0;
}

int sc_infra_sensor_get_readings(SC_InfraSystem *infra, const char *sensor_id, SC_SensorReading *buf, int max) {
    if (!infra || !sensor_id || !buf) return 0;
    int count = 0;
    for (int i = infra->reading_count - 1; i >= 0 && count < max; i--) {
        if (strcmp(infra->readings[i].sensor_id, sensor_id) == 0)
            buf[count++] = infra->readings[i];
    }
    return count;
}

int sc_infra_sensor_count_active(const SC_InfraSystem *infra) {
    if (!infra) return 0;
    int c = 0;
    for (int i = 0; i < infra->sensor_count; i++)
        if (infra->sensors[i].is_active) c++;
    return c;
}

/* ---- L5: EWMA (Exponentially Weighted Moving Average) ---- */
static int sc_infra_find_ewma_idx(SC_InfraSystem *infra, const char *sensor_id) {
    for (int i = 0; i < infra->ewma_count; i++)
        if (strcmp(infra->sensors[infra->ewma_sensor_map[i]].sensor_id, sensor_id) == 0)
            return i;
    return -1;
}

double sc_infra_ewma_update(SC_InfraSystem *infra, const char *sensor_id, double new_value, double alpha) {
    if (!infra || !sensor_id || alpha <= 0.0 || alpha > 1.0) return 0.0;
    int idx = sc_infra_find_ewma_idx(infra, sensor_id);
    if (idx < 0) {
        if (infra->ewma_count >= SC_INFRA_MAX_SENSORS) return 0.0;
        int sidx = -1;
        for (int i = 0; i < infra->sensor_count; i++) {
            if (strcmp(infra->sensors[i].sensor_id, sensor_id) == 0) { sidx = i; break; }
        }
        if (sidx < 0) return 0.0;
        idx = infra->ewma_count;
        infra->ewma_sensor_map[idx] = sidx;
        infra->ewma_values[idx] = new_value;
        infra->ewma_variances[idx] = 0.0;
        infra->ewma_count++;
        return new_value;
    }
    double old_ewma = infra->ewma_values[idx];
    double new_ewma = alpha * new_value + (1.0 - alpha) * old_ewma;
    double delta = new_value - old_ewma;
    infra->ewma_variances[idx] = (1.0 - alpha) * (infra->ewma_variances[idx] + alpha * delta * delta);
    infra->ewma_values[idx] = new_ewma;
    return new_ewma;
}

double sc_infra_ewma_get(const SC_InfraSystem *infra, const char *sensor_id) {
    if (!infra || !sensor_id) return 0.0;
    for (int i = 0; i < infra->ewma_count; i++) {
        int sidx = infra->ewma_sensor_map[i];
        if (strcmp(infra->sensors[sidx].sensor_id, sensor_id) == 0)
            return infra->ewma_values[i];
    }
    return 0.0;
}

int sc_infra_detect_anomaly_zscore(SC_InfraSystem *infra, const char *sensor_id, double z_threshold) {
    if (!infra || !sensor_id || z_threshold <= 0.0) return -1;
    for (int i = 0; i < infra->ewma_count; i++) {
        int sidx = infra->ewma_sensor_map[i];
        if (strcmp(infra->sensors[sidx].sensor_id, sensor_id) == 0) {
            double variance = infra->ewma_variances[i];
            if (variance <= 0.0) return 0;
            double stddev = sqrt(variance);
            double latest = infra->sensors[sidx].last_value;
            double ewma = infra->ewma_values[i];
            double z = fabs(latest - ewma) / stddev;
            return (z > z_threshold) ? 1 : 0;
        }
    }
    return -1;
}

/* ---- L2: Asset Management ---- */
int sc_infra_asset_register(SC_InfraSystem *infra, const SC_InfraAsset *asset) {
    if (!infra || !asset || infra->asset_count >= SC_INFRA_MAX_ASSETS) return -1;
    int idx = infra->asset_count;
    memcpy(&infra->assets[idx], asset, sizeof(SC_InfraAsset));
    infra->asset_count++;
    return idx;
}

SC_InfraAsset* sc_infra_asset_lookup(SC_InfraSystem *infra, const char *asset_id) {
    if (!infra || !asset_id) return NULL;
    for (int i = 0; i < infra->asset_count; i++)
        if (strcmp(infra->assets[i].asset_id, asset_id) == 0)
            return &infra->assets[i];
    return NULL;
}

int sc_infra_asset_update_condition(SC_InfraSystem *infra, const char *asset_id, SC_AssetCondition cond) {
    SC_InfraAsset *a = sc_infra_asset_lookup(infra, asset_id);
    if (!a) return -1;
    a->condition = cond;
    a->last_inspected = time(NULL);
    return 0;
}

int sc_infra_asset_list_by_type(const SC_InfraSystem *infra, SC_AssetType type, char *results, int max) {
    if (!infra || !results) return 0;
    int count = 0;
    for (int i = 0; i < infra->asset_count && count < max; i++)
        if (infra->assets[i].type == type)
            results[count++] = (char)i;
    return count;
}

int sc_infra_asset_count_critical(const SC_InfraSystem *infra) {
    if (!infra) return 0;
    int c = 0;
    for (int i = 0; i < infra->asset_count; i++)
        if (infra->assets[i].condition == SC_CONDITION_CRITICAL) c++;
    return c;
}

float sc_infra_asset_avg_health(const SC_InfraSystem *infra) {
    if (!infra || infra->asset_count == 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < infra->asset_count; i++)
        sum += infra->assets[i].health_score;
    return sum / (float)infra->asset_count;
}

/* ---- L5: Asset Health Score ---- */
float sc_infra_asset_compute_health(const SC_InfraAsset *asset, time_t now) {
    if (!asset) return 0.0f;
    int age_years = (int)(difftime(now, 0) / 31536000.0) - asset->construction_year;
    if (age_years < 0) age_years = 0;
    float age_factor = 1.0f - (float)age_years / (float)asset->expected_lifetime_years;
    if (age_factor < 0.0f) age_factor = 0.0f;
    float cond_factor;
    switch (asset->condition) {
        case SC_CONDITION_EXCELLENT: cond_factor = 1.0f; break;
        case SC_CONDITION_GOOD: cond_factor = 0.8f; break;
        case SC_CONDITION_FAIR: cond_factor = 0.6f; break;
        case SC_CONDITION_POOR: cond_factor = 0.3f; break;
        case SC_CONDITION_CRITICAL: cond_factor = 0.1f; break;
        default: cond_factor = 0.5f;
    }
    time_t days_since_maint = (time_t)(difftime(now, asset->last_maintained) / 86400.0);
    float maint_factor = 1.0f - (float)days_since_maint / 365.0f;
    if (maint_factor < 0.2f) maint_factor = 0.2f;
    return (age_factor * 0.4f + cond_factor * 0.4f + maint_factor * 0.2f) * 100.0f;
}

int sc_infra_asset_rank_by_health(const SC_InfraSystem *infra, int *ranked_indices, int max) {
    if (!infra || !ranked_indices) return 0;
    int n = infra->asset_count;
    if (n > max) n = max;
    for (int i = 0; i < n; i++) ranked_indices[i] = i;
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (infra->assets[ranked_indices[j]].health_score < infra->assets[ranked_indices[i]].health_score) {
                int t = ranked_indices[i]; ranked_indices[i] = ranked_indices[j]; ranked_indices[j] = t;
            }
    return n;
}

/* ---- L3: Maintenance Management ---- */
int sc_infra_maintenance_create(SC_InfraSystem *infra, const SC_MaintenanceOrder *order) {
    if (!infra || !order || infra->maint_count >= SC_INFRA_MAX_MAINT_ORDERS) return -1;
    int idx = infra->maint_count;
    memcpy(&infra->maint_orders[idx], order, sizeof(SC_MaintenanceOrder));
    infra->maint_orders[idx].order_id = infra->next_maint_id++;
    infra->maint_count++;
    return infra->maint_orders[idx].order_id;
}

int sc_infra_maintenance_update_status(SC_InfraSystem *infra, int64_t order_id, SC_MaintenanceStatus status) {
    if (!infra) return -1;
    for (int i = 0; i < infra->maint_count; i++) {
        if (infra->maint_orders[i].order_id == order_id) {
            infra->maint_orders[i].status = status;
            if (status == SC_MAINT_COMPLETED) infra->maint_orders[i].completed_at = time(NULL);
            return 0;
        }
    }
    return -1;
}

SC_MaintenanceOrder* sc_infra_maintenance_lookup(SC_InfraSystem *infra, int64_t order_id) {
    if (!infra) return NULL;
    for (int i = 0; i < infra->maint_count; i++)
        if (infra->maint_orders[i].order_id == order_id)
            return &infra->maint_orders[i];
    return NULL;
}

int sc_infra_maintenance_list_pending(const SC_InfraSystem *infra, SC_MaintenanceOrder *orders, int max) {
    if (!infra || !orders) return 0;
    int count = 0;
    for (int i = 0; i < infra->maint_count && count < max; i++)
        if (infra->maint_orders[i].status == SC_MAINT_PLANNED || infra->maint_orders[i].status == SC_MAINT_SCHEDULED)
            orders[count++] = infra->maint_orders[i];
    return count;
}

/* ---- L2: Utility Monitoring ---- */
int sc_infra_utility_meter_register(SC_InfraSystem *infra, const SC_UtilityMeter *meter) {
    if (!infra || !meter || infra->meter_count >= SC_INFRA_MAX_SENSORS) return -1;
    int idx = infra->meter_count;
    memcpy(&infra->meters[idx], meter, sizeof(SC_UtilityMeter));
    infra->meter_count++;
    return idx;
}

int sc_infra_utility_record_reading(SC_InfraSystem *infra, const char *meter_id, double reading, time_t ts) {
    if (!infra || !meter_id) return -1;
    for (int i = 0; i < infra->meter_count; i++) {
        if (strcmp(infra->meters[i].meter_id, meter_id) == 0) {
            infra->meters[i].previous_reading = infra->meters[i].current_reading;
            infra->meters[i].previous_read_time = infra->meters[i].current_read_time;
            infra->meters[i].current_reading = reading;
            infra->meters[i].current_read_time = ts;
            return 0;
        }
    }
    return -1;
}

double sc_infra_utility_consumption(const SC_InfraSystem *infra, const char *meter_id) {
    if (!infra || !meter_id) return 0.0;
    for (int i = 0; i < infra->meter_count; i++)
        if (strcmp(infra->meters[i].meter_id, meter_id) == 0)
            return infra->meters[i].current_reading - infra->meters[i].previous_reading;
    return 0.0;
}

double sc_infra_utility_forecast(const SC_InfraSystem *infra, SC_UtilityType utype, int days_ahead) {
    if (!infra || days_ahead <= 0) return 0.0;
    double total_daily = 0.0;
    int count = 0;
    for (int i = 0; i < infra->meter_count; i++) {
        if (infra->meters[i].utility_type == utype && infra->meters[i].is_active) {
            double consumption = infra->meters[i].current_reading - infra->meters[i].previous_reading;
            if (consumption > 0) {
                total_daily += consumption;
                count++;
            }
        }
    }
    if (count == 0) return 0.0;
    return (total_daily / count) * days_ahead;
}

double sc_infra_utility_total_consumption(const SC_InfraSystem *infra, SC_UtilityType utype) {
    if (!infra) return 0.0;
    double total = 0.0;
    for (int i = 0; i < infra->meter_count; i++)
        if (infra->meters[i].utility_type == utype)
            total += infra->meters[i].current_reading;
    return total;
}

/* ---- L7: Infrastructure Health Dashboard ---- */
int sc_infra_health_summary(const SC_InfraSystem *infra, char *report, int buf_size) {
    if (!infra || !report || buf_size <= 0) return -1;
    int critical = sc_infra_asset_count_critical(infra);
    float avg_health = sc_infra_asset_avg_health(infra);
    int active_sensors = sc_infra_sensor_count_active(infra);
    return snprintf(report, (size_t)buf_size,
        "Infrastructure Health Report: Assets=%d Critical=%d AvgHealth=%.1f%% ActiveSensors=%d Sensors=%d",
        infra->asset_count, critical, avg_health, active_sensors, infra->sensor_count);
}

int sc_infra_predictive_maintenance_alert(const SC_InfraSystem *infra, int *asset_indices, int max) {
    if (!infra || !asset_indices) return 0;
    int count = 0;
    time_t now = time(NULL);
    for (int i = 0; i < infra->asset_count && count < max; i++) {
        float health = sc_infra_asset_compute_health(&infra->assets[i], now);
        if (health < 30.0f) asset_indices[count++] = i;
    }
    return count;
}