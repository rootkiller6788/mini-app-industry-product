/* Smart City Emergency Management System */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "smartcity_ems.h"

struct SC_EMSystem {
    SC_EmergencyIncident incidents[SC_EMS_MAX_INCIDENTS];
    int incident_count;
    int next_incident_id;
    SC_ResponseUnit units[SC_EMS_MAX_RESPONSE_UNITS];
    int unit_count;
    SC_Shelter shelters[SC_EMS_MAX_SHELTERS];
    int shelter_count;
    SC_EvacuationZone evac_zones[SC_EMS_MAX_EVAC_ZONES];
    int evac_count;
    SC_CitizenAlert alerts[SC_EMS_MAX_INCIDENTS];
    int alert_count;
    int next_alert_id;
};

SC_EMSystem* sc_ems_create(void) {
    SC_EMSystem *ems = (SC_EMSystem*)calloc(1, sizeof(SC_EMSystem));
    if (!ems) return NULL;
    ems->next_incident_id = 10000;
    ems->next_alert_id = 1;
    return ems;
}

void sc_ems_destroy(SC_EMSystem *ems) { free(ems); }

/* ---- L2: Incident Management ---- */
int64_t sc_ems_incident_report(SC_EMSystem *ems, const SC_EmergencyIncident *incident) {
    if (!ems || !incident || ems->incident_count >= SC_EMS_MAX_INCIDENTS) return -1;
    int idx = ems->incident_count;
    memcpy(&ems->incidents[idx], incident, sizeof(SC_EmergencyIncident));
    ems->incidents[idx].incident_id = ems->next_incident_id++;
    ems->incidents[idx].status = SC_INC_STATUS_REPORTED;
    ems->incidents[idx].reported_at = time(NULL);
    ems->incidents[idx].num_units_dispatched = 0;
    memset(ems->incidents[idx].dispatched_unit_ids, -1, sizeof(ems->incidents[idx].dispatched_unit_ids));
    ems->incident_count++;
    return ems->incidents[idx].incident_id;
}

SC_EmergencyIncident* sc_ems_incident_lookup(SC_EMSystem *ems, int64_t incident_id) {
    if (!ems) return NULL;
    for (int i = 0; i < ems->incident_count; i++)
        if (ems->incidents[i].incident_id == incident_id) return &ems->incidents[i];
    return NULL;
}

int sc_ems_incident_update_status(SC_EMSystem *ems, int64_t incident_id, SC_IncidentStatus status) {
    SC_EmergencyIncident *inc = sc_ems_incident_lookup(ems, incident_id);
    if (!inc) return -1;
    inc->status = status;
    time_t now = time(NULL);
    switch (status) {
        case SC_INC_STATUS_VERIFIED: inc->verified_at = now; break;
        case SC_INC_STATUS_DISPATCHED: inc->dispatched_at = now; break;
        case SC_INC_STATUS_CONTAINED: inc->contained_at = now; break;
        case SC_INC_STATUS_RESOLVED: inc->resolved_at = now; break;
        case SC_INC_STATUS_CLOSED: inc->closed_at = now; break;
        default: break;
    }
    return 0;
}

int sc_ems_incident_assign_severity(SC_EMSystem *ems, int64_t incident_id, SC_IncidentSeverity sev) {
    SC_EmergencyIncident *inc = sc_ems_incident_lookup(ems, incident_id);
    if (!inc) return -1;
    inc->severity = sev;
    return 0;
}

int sc_ems_incident_count_active(const SC_EMSystem *ems) {
    if (!ems) return 0;
    int c = 0;
    for (int i = 0; i < ems->incident_count; i++)
        if (ems->incidents[i].status != SC_INC_STATUS_CLOSED) c++;
    return c;
}

int sc_ems_incident_list_by_type(const SC_EMSystem *ems, SC_IncidentType type, int64_t *results, int max) {
    if (!ems || !results) return 0;
    int count = 0;
    for (int i = 0; i < ems->incident_count && count < max; i++)
        if (ems->incidents[i].type == type)
            results[count++] = ems->incidents[i].incident_id;
    return count;
}

/* ---- L5: START Triage Priority Algorithm ---- */
int sc_ems_triage_priority(SC_IncidentType type, int affected_population, int casualties, SC_IncidentSeverity sev) {
    int score = 0;
    switch (sev) {
        case SC_SEV_CATASTROPHIC: score += 100; break;
        case SC_SEV_SEVERE: score += 75; break;
        case SC_SEV_MAJOR: score += 50; break;
        case SC_SEV_MODERATE: score += 25; break;
        default: score += 10;
    }
    score += (affected_population / 100) * 5;
    score += casualties * 20;
    switch (type) {
        case SC_INC_FIRE: case SC_INC_HAZMAT: case SC_INC_TERRORISM:
            score += 30; break;
        case SC_INC_EARTHQUAKE: case SC_INC_HURRICANE: case SC_INC_TORNADO:
            score += 40; break;
        default: break;
    }
    if (score >= 120) return 1;
    if (score >= 80) return 2;
    if (score >= 40) return 3;
    return 4;
}

int sc_ems_triage_sort_incidents(SC_EMSystem *ems, int64_t *sorted_ids, int max) {
    if (!ems || !sorted_ids || max <= 0) return 0;
    int n = ems->incident_count;
    if (n > max) n = max;
    int *priorities = (int*)malloc((size_t)n * sizeof(int));
    int *indices = (int*)malloc((size_t)n * sizeof(int));
    if (!priorities || !indices) { free(priorities); free(indices); return 0; }
    for (int i = 0; i < n; i++) {
        indices[i] = i;
        priorities[i] = sc_ems_triage_priority(ems->incidents[i].type, ems->incidents[i].affected_population, ems->incidents[i].casualties, ems->incidents[i].severity);
    }
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (priorities[j] > priorities[i]) {
                int tp = priorities[i]; priorities[i] = priorities[j]; priorities[j] = tp;
                int ti = indices[i]; indices[i] = indices[j]; indices[j] = ti;
            }
    for (int i = 0; i < n; i++)
        sorted_ids[i] = ems->incidents[indices[i]].incident_id;
    free(priorities); free(indices);
    return n;
}

/* ---- L3: Response Unit Management ---- */
int sc_ems_unit_register(SC_EMSystem *ems, const SC_ResponseUnit *unit) {
    if (!ems || !unit || ems->unit_count >= SC_EMS_MAX_RESPONSE_UNITS) return -1;
    int idx = ems->unit_count;
    memcpy(&ems->units[idx], unit, sizeof(SC_ResponseUnit));
    ems->units[idx].is_available = true;
    ems->units[idx].current_incident_id = -1;
    ems->unit_count++;
    return idx;
}

SC_ResponseUnit* sc_ems_unit_lookup(SC_EMSystem *ems, int unit_id) {
    if (!ems) return NULL;
    for (int i = 0; i < ems->unit_count; i++)
        if (ems->units[i].unit_id == unit_id) return &ems->units[i];
    return NULL;
}

int sc_ems_unit_dispatch(SC_EMSystem *ems, int unit_id, int64_t incident_id) {
    SC_ResponseUnit *unit = sc_ems_unit_lookup(ems, unit_id);
    if (!unit || !unit->is_available) return -1;
    SC_EmergencyIncident *inc = sc_ems_incident_lookup(ems, incident_id);
    if (!inc) return -1;
    unit->is_available = false;
    unit->current_incident_id = incident_id;
    unit->last_dispatched = time(NULL);
    unit->total_responses++;
    if (inc->num_units_dispatched < 16)
        inc->dispatched_unit_ids[inc->num_units_dispatched++] = unit_id;
    inc->status = SC_INC_STATUS_DISPATCHED;
    inc->dispatched_at = time(NULL);
    return 0;
}

int sc_ems_unit_release(SC_EMSystem *ems, int unit_id) {
    SC_ResponseUnit *unit = sc_ems_unit_lookup(ems, unit_id);
    if (!unit) return -1;
    unit->is_available = true;
    unit->current_incident_id = -1;
    return 0;
}

int sc_ems_unit_list_available(const SC_EMSystem *ems, SC_ResponseUnitType type, int *results, int max) {
    if (!ems || !results) return 0;
    int count = 0;
    for (int i = 0; i < ems->unit_count && count < max; i++)
        if (ems->units[i].type == type && ems->units[i].is_available)
            results[count++] = ems->units[i].unit_id;
    return count;
}

/* ---- L5: Greedy Resource Allocation ---- */
int sc_ems_allocate_resources(SC_EMSystem *ems, int64_t incident_id, int num_units_needed, SC_ResponseUnitType *types, int *assigned_units) {
    if (!ems || !types || !assigned_units || num_units_needed <= 0) return 0;
    int assigned = 0;
    for (int t = 0; t < num_units_needed; t++) {
        int best_unit = -1;
        double best_dist = 1e9;
        SC_EmergencyIncident *inc = sc_ems_incident_lookup(ems, incident_id);
        if (!inc) break;
        for (int i = 0; i < ems->unit_count; i++) {
            if (ems->units[i].type == types[t] && ems->units[i].is_available) {
                double dlat = ems->units[i].station_latitude - inc->latitude;
                double dlon = ems->units[i].station_longitude - inc->longitude;
                double dist = sqrt(dlat*dlat + dlon*dlon);
                if (dist < best_dist) { best_dist = dist; best_unit = ems->units[i].unit_id; }
            }
        }
        if (best_unit >= 0) {
            sc_ems_unit_dispatch(ems, best_unit, incident_id);
            assigned_units[assigned++] = best_unit;
        }
    }
    return assigned;
}

float sc_ems_response_time_estimate(const SC_EMSystem *ems, int unit_id, double incident_lat, double incident_lon) {
    SC_ResponseUnit *unit = sc_ems_unit_lookup((SC_EMSystem*)ems, unit_id);
    if (!unit) return -1.0f;
    double dlat = unit->station_latitude - incident_lat;
    double dlon = unit->station_longitude - incident_lon;
    double dist_km = sqrt(dlat*dlat + dlon*dlon) * 111.32;
    float travel_min = (float)(dist_km / 40.0 * 60.0);
    return travel_min + (float)unit->response_time_minutes_avg;
}

/* ---- Nearest Unit Search ---- */
int sc_ems_find_nearest_unit(const SC_EMSystem *ems, double lat, double lon, SC_ResponseUnitType type) {
    if (!ems) return -1;
    double best_dist = 1e9;
    int best_id = -1;
    for (int i = 0; i < ems->unit_count; i++) {
        if (ems->units[i].type != type || !ems->units[i].is_available) continue;
        double dlat = ems->units[i].station_latitude - lat;
        double dlon = ems->units[i].station_longitude - lon;
        double dist = sqrt(dlat*dlat + dlon*dlon);
        if (dist < best_dist) { best_dist = dist; best_id = ems->units[i].unit_id; }
    }
    return best_id;
}

int sc_ems_find_nearest_units(const SC_EMSystem *ems, double lat, double lon, int count, int *unit_ids, int max) {
    if (!ems || !unit_ids || count <= 0 || max <= 0) return 0;
    int n = 0;
    struct { int id; double dist; } candidates[SC_EMS_MAX_RESPONSE_UNITS];
    for (int i = 0; i < ems->unit_count; i++) {
        if (!ems->units[i].is_available) continue;
        double dlat = ems->units[i].station_latitude - lat;
        double dlon = ems->units[i].station_longitude - lon;
        candidates[n].id = ems->units[i].unit_id;
        candidates[n].dist = sqrt(dlat*dlat + dlon*dlon);
        n++;
    }
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (candidates[j].dist < candidates[i].dist) {
                int ti = candidates[i].id; double td = candidates[i].dist;
                candidates[i].id = candidates[j].id; candidates[i].dist = candidates[j].dist;
                candidates[j].id = ti; candidates[j].dist = td;
            }
    int result = n < count ? n : count;
    if (result > max) result = max;
    for (int i = 0; i < result; i++) unit_ids[i] = candidates[i].id;
    return result;
}

/* ---- L6: Shelter Management ---- */
int sc_ems_shelter_register(SC_EMSystem *ems, const SC_Shelter *shelter) {
    if (!ems || !shelter || ems->shelter_count >= SC_EMS_MAX_SHELTERS) return -1;
    int idx = ems->shelter_count;
    memcpy(&ems->shelters[idx], shelter, sizeof(SC_Shelter));
    ems->shelter_count++;
    return idx;
}

SC_Shelter* sc_ems_shelter_lookup(SC_EMSystem *ems, int shelter_id) {
    if (!ems) return NULL;
    for (int i = 0; i < ems->shelter_count; i++)
        if (ems->shelters[i].shelter_id == shelter_id) return &ems->shelters[i];
    return NULL;
}

int sc_ems_shelter_open(SC_EMSystem *ems, int shelter_id) {
    SC_Shelter *s = sc_ems_shelter_lookup(ems, shelter_id);
    if (!s) return -1;
    s->is_open = true;
    s->opened_at = time(NULL);
    return 0;
}

int sc_ems_shelter_close(SC_EMSystem *ems, int shelter_id) {
    SC_Shelter *s = sc_ems_shelter_lookup(ems, shelter_id);
    if (!s) return -1;
    s->is_open = false;
    s->current_occupancy = 0;
    return 0;
}

int sc_ems_shelter_update_occupancy(SC_EMSystem *ems, int shelter_id, int occupancy) {
    SC_Shelter *s = sc_ems_shelter_lookup(ems, shelter_id);
    if (!s) return -1;
    s->current_occupancy = occupancy;
    return 0;
}

int sc_ems_shelter_find_nearest(SC_EMSystem *ems, double lat, double lon, int *results, int max) {
    if (!ems || !results || max <= 0) return 0;
    int n = 0;
    struct { int id; double dist; } candidates[SC_EMS_MAX_SHELTERS];
    for (int i = 0; i < ems->shelter_count; i++) {
        if (!ems->shelters[i].is_open) continue;
        double dlat = ems->shelters[i].latitude - lat;
        double dlon = ems->shelters[i].longitude - lon;
        candidates[n].id = ems->shelters[i].shelter_id;
        candidates[n].dist = sqrt(dlat*dlat + dlon*dlon);
        n++;
    }
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (candidates[j].dist < candidates[i].dist) {
                int ti = candidates[i].id; double td = candidates[i].dist;
                candidates[i].id = candidates[j].id; candidates[i].dist = candidates[j].dist;
                candidates[j].id = ti; candidates[j].dist = td;
            }
    int result = n < max ? n : max;
    for (int i = 0; i < result; i++) results[i] = candidates[i].id;
    return result;
}

int sc_ems_shelter_total_capacity(const SC_EMSystem *ems) {
    if (!ems) return 0;
    int total = 0;
    for (int i = 0; i < ems->shelter_count; i++)
        if (ems->shelters[i].is_open) total += ems->shelters[i].capacity;
    return total;
}

/* ---- L6: Evacuation Management ---- */
int sc_ems_evac_zone_add(SC_EMSystem *ems, const SC_EvacuationZone *zone) {
    if (!ems || !zone || ems->evac_count >= SC_EMS_MAX_EVAC_ZONES) return -1;
    int idx = ems->evac_count;
    memcpy(&ems->evac_zones[idx], zone, sizeof(SC_EvacuationZone));
    ems->evac_count++;
    return idx;
}

int sc_ems_evac_order(SC_EMSystem *ems, int zone_id) {
    if (!ems) return -1;
    for (int i = 0; i < ems->evac_count; i++) {
        if (ems->evac_zones[i].zone_id == zone_id) {
            ems->evac_zones[i].evacuation_ordered = true;
            ems->evac_zones[i].order_time = time(NULL);
            return 0;
        }
    }
    return -1;
}

int sc_ems_evac_lift(SC_EMSystem *ems, int zone_id) {
    if (!ems) return -1;
    for (int i = 0; i < ems->evac_count; i++) {
        if (ems->evac_zones[i].zone_id == zone_id) {
            ems->evac_zones[i].evacuation_ordered = false;
            return 0;
        }
    }
    return -1;
}

int sc_ems_evac_plan_route(SC_EMSystem *ems, int zone_id, int *shelter_ids, int max) {
    if (!ems || !shelter_ids || max <= 0) return 0;
    for (int i = 0; i < ems->evac_count; i++) {
        if (ems->evac_zones[i].zone_id == zone_id) {
            SC_EvacuationZone *zone = &ems->evac_zones[i];
            return sc_ems_shelter_find_nearest(ems, zone->centroid_lat, zone->centroid_lon, shelter_ids, max);
        }
    }
    return 0;
}

/* ---- L6: Alert System ---- */
int sc_ems_alert_send(SC_EMSystem *ems, const SC_CitizenAlert *alert) {
    if (!ems || !alert || ems->alert_count >= SC_EMS_MAX_INCIDENTS) return -1;
    int idx = ems->alert_count;
    memcpy(&ems->alerts[idx], alert, sizeof(SC_CitizenAlert));
    ems->alerts[idx].alert_id = ems->next_alert_id++;
    ems->alerts[idx].issued_at = time(NULL);
    ems->alert_count++;
    return ems->alerts[idx].alert_id;
}

int sc_ems_alert_active_count(const SC_EMSystem *ems) {
    if (!ems) return 0;
    int count = 0;
    time_t now = time(NULL);
    for (int i = 0; i < ems->alert_count; i++)
        if (ems->alerts[i].expires_at > now) count++;
    return count;
}

SC_CitizenAlert* sc_ems_alert_lookup(SC_EMSystem *ems, int64_t alert_id) {
    if (!ems) return NULL;
    for (int i = 0; i < ems->alert_count; i++)
        if (ems->alerts[i].alert_id == alert_id) return &ems->alerts[i];
    return NULL;
}

/* ---- L7: EMS Dashboard ---- */
int sc_ems_summary_report(const SC_EMSystem *ems, char *report, int buf_size) {
    if (!ems || !report || buf_size <= 0) return -1;
    int active_incidents = sc_ems_incident_count_active(ems);
    int available_units = 0;
    for (int i = 0; i < ems->unit_count; i++)
        if (ems->units[i].is_available) available_units++;
    int open_shelters = 0;
    for (int i = 0; i < ems->shelter_count; i++)
        if (ems->shelters[i].is_open) open_shelters++;
    int active_alerts = sc_ems_alert_active_count(ems);
    return snprintf(report, (size_t)buf_size,
        "EMS Status: ActiveIncidents=%d AvailableUnits=%d OpenShelters=%d ActiveAlerts=%d TotalShelterCapacity=%d",
        active_incidents, available_units, open_shelters, active_alerts, sc_ems_shelter_total_capacity(ems));
}

int sc_ems_resource_status(const SC_EMSystem *ems, char *report, int buf_size) {
    if (!ems || !report || buf_size <= 0) return -1;
    int fire = 0, ambulance = 0, police = 0, hazmat = 0, rescue = 0;
    for (int i = 0; i < ems->unit_count; i++) {
        if (!ems->units[i].is_available) continue;
        switch (ems->units[i].type) {
            case SC_UNIT_FIRE_ENGINE: fire++; break;
            case SC_UNIT_AMBULANCE: ambulance++; break;
            case SC_UNIT_POLICE: police++; break;
            case SC_UNIT_HAZMAT_TEAM: hazmat++; break;
            case SC_UNIT_SEARCH_RESCUE: rescue++; break;
            default: break;
        }
    }
    return snprintf(report, (size_t)buf_size,
        "Available: Fire=%d Ambulance=%d Police=%d Hazmat=%d Rescue=%d",
        fire, ambulance, police, hazmat, rescue);
}