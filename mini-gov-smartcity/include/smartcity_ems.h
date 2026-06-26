#ifndef MINI_SMARTCITY_EMS_H
#define MINI_SMARTCITY_EMS_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* ================================================================
 *  Smart City Emergency Management System
 *
 *  L1: Core definitions -- EmergencyIncident, ResponseUnit,
 *       EvacuationZone, Alert, Shelter, ResourceAllocation
 *  L2: Core concepts -- Incident Command System (ICS), NIMS
 *       framework, multi-agency coordination, triage protocols
 *  L3: Engineering structures -- Incident priority queue with
 *       severity preemption, resource allocation matrix,
 *       evacuation zone graph, alert dissemination pipeline
 *  L4: Standards -- NFPA 1600 (Emergency Management), FEMA NIMS,
 *       ISO 22320 (Emergency management), Sendai Framework
 *       for Disaster Risk Reduction (2015-2030)
 *  L5: Algorithms -- Resource allocation via greedy optimization,
 *       evacuation route planning, incident severity triage
 *       using START protocol, shelter capacity optimization
 *  L6: Canonical problems -- Emergency response dispatch system,
 *       disaster recovery coordination, multi-agency resource
 *       pooling and deployment
 *  L7: Applications -- Real-time emergency dashboard, citizen
 *       alert system, shelter finder application
 *  L8: Advanced -- AI-assisted damage assessment using satellite
 *       imagery, predictive disaster impact modeling
 *  L9: Industry -- FEMA IPAWS, Everbridge, RapidSOS, OneConcern,
 *       Hexagon Safety & Infrastructure
 *
 *  References:
 *  - NFPA 1600 Standard on Disaster/Emergency Management (2019)
 *  - FEMA National Incident Management System (NIMS)
 *  - Sendai Framework for Disaster Risk Reduction 2015-2030 (UN)
 * ================================================================ */

#define SC_EMS_MAX_INCIDENTS      65536
#define SC_EMS_MAX_RESPONSE_UNITS 4096
#define SC_EMS_MAX_SHELTERS       2048
#define SC_EMS_MAX_EVAC_ZONES     4096

/* ---- L1: Incident Type ---- */
typedef enum {
    SC_INC_FIRE          = 0,
    SC_INC_FLOOD         = 1,
    SC_INC_EARTHQUAKE    = 2,
    SC_INC_HURRICANE     = 3,
    SC_INC_TORNADO       = 4,
    SC_INC_HAZMAT        = 5,
    SC_INC_TERRORISM     = 6,
    SC_INC_POWER_OUTAGE  = 7,
    SC_INC_WATER_MAIN    = 8,
    SC_INC_GAS_LEAK      = 9,
    SC_INC_PANDEMIC      = 10,
    SC_INC_CIVIL_UNREST  = 11,
    SC_INC_COUNT         = 12
} SC_IncidentType;

/* ---- L1: Incident Severity ---- */
typedef enum {
    SC_SEV_MINOR     = 0,
    SC_SEV_MODERATE  = 1,
    SC_SEV_MAJOR     = 2,
    SC_SEV_SEVERE    = 3,
    SC_SEV_CATASTROPHIC = 4
} SC_IncidentSeverity;

/* ---- L1: Response Unit Type ---- */
typedef enum {
    SC_UNIT_FIRE_ENGINE   = 0,
    SC_UNIT_AMBULANCE     = 1,
    SC_UNIT_POLICE        = 2,
    SC_UNIT_HAZMAT_TEAM   = 3,
    SC_UNIT_SEARCH_RESCUE = 4,
    SC_UNIT_WATER_RESCUE  = 5,
    SC_UNIT_UTILITY_CREW  = 6,
    SC_UNIT_MEDICAL_TEAM  = 7,
    SC_UNIT_COUNT         = 8
} SC_ResponseUnitType;

/* ---- L1: Incident Status ---- */
typedef enum {
    SC_INC_STATUS_REPORTED    = 0,
    SC_INC_STATUS_VERIFIED    = 1,
    SC_INC_STATUS_DISPATCHED  = 2,
    SC_INC_STATUS_RESPONDING  = 3,
    SC_INC_STATUS_CONTAINED   = 4,
    SC_INC_STATUS_RESOLVED    = 5,
    SC_INC_STATUS_RECOVERY    = 6,
    SC_INC_STATUS_CLOSED      = 7
} SC_IncidentStatus;

/* ---- L1: Emergency Incident ---- */
typedef struct {
    int64_t  incident_id;
    SC_IncidentType type;
    SC_IncidentSeverity severity;
    SC_IncidentStatus status;
    double   latitude;
    double   longitude;
    char     description[512];
    char     location_name[256];
    int      district_id;
    int      ward_number;
    int      affected_population;
    int      casualties;
    float    damage_estimate;
    int      num_units_dispatched;
    int      dispatched_unit_ids[16];
    time_t   reported_at;
    time_t   verified_at;
    time_t   dispatched_at;
    time_t   contained_at;
    time_t   resolved_at;
    time_t   closed_at;
    int      priority;
    bool     requires_evacuation;
    int      evacuation_zone_id;
    char     commander_name[128];
} SC_EmergencyIncident;

/* ---- L1: Response Unit ---- */
typedef struct {
    int      unit_id;
    SC_ResponseUnitType type;
    char     unit_name[128];
    char     station_name[128];
    double   station_latitude;
    double   station_longitude;
    int      personnel_count;
    bool     is_available;
    int      current_incident_id;
    double   current_latitude;
    double   current_longitude;
    int      response_time_minutes_avg;
    int      total_responses;
    time_t   last_dispatched;
} SC_ResponseUnit;

/* ---- L1: Shelter ---- */
typedef struct {
    int      shelter_id;
    char     name[128];
    char     address[256];
    double   latitude;
    double   longitude;
    int      capacity;
    int      current_occupancy;
    bool     is_open;
    bool     has_medical;
    bool     has_food_water;
    bool     pet_friendly;
    bool     wheelchair_accessible;
    int      staff_count;
    time_t   opened_at;
} SC_Shelter;

/* ---- L1: Evacuation Zone ---- */
typedef struct {
    int      zone_id;
    char     name[128];
    int      district_id;
    double   centroid_lat;
    double   centroid_lon;
    int      estimated_population;
    int      num_evac_routes;
    int      evac_route_destinations[8];
    bool     is_active;
    bool     evacuation_ordered;
    time_t   order_time;
} SC_EvacuationZone;

/* ---- L1: Citizen Alert ---- */
typedef struct {
    int64_t  alert_id;
    SC_IncidentType related_incident;
    char     message[512];
    int      target_district;
    int      target_ward;
    bool     is_emergency;
    bool     requires_action;
    char     action_instruction[256];
    time_t   issued_at;
    time_t   expires_at;
    int      delivery_count;
} SC_CitizenAlert;

/* ---- EMS System ---- */
typedef struct SC_EMSystem SC_EMSystem;
SC_EMSystem* sc_ems_create(void);
void         sc_ems_destroy(SC_EMSystem *ems);

/* Incident Management (L2) */
int64_t sc_ems_incident_report(SC_EMSystem *ems, const SC_EmergencyIncident *incident);
SC_EmergencyIncident* sc_ems_incident_lookup(SC_EMSystem *ems, int64_t incident_id);
int  sc_ems_incident_update_status(SC_EMSystem *ems, int64_t incident_id, SC_IncidentStatus status);
int  sc_ems_incident_assign_severity(SC_EMSystem *ems, int64_t incident_id, SC_IncidentSeverity sev);
int  sc_ems_incident_count_active(const SC_EMSystem *ems);
int  sc_ems_incident_list_by_type(const SC_EMSystem *ems, SC_IncidentType type, int64_t *results, int max);

/* START Triage Algorithm (L5) */
int  sc_ems_triage_priority(SC_IncidentType type, int affected_population, int casualties, SC_IncidentSeverity sev);
int  sc_ems_triage_sort_incidents(SC_EMSystem *ems, int64_t *sorted_ids, int max);

/* Response Unit Dispatch (L3) */
int  sc_ems_unit_register(SC_EMSystem *ems, const SC_ResponseUnit *unit);
SC_ResponseUnit* sc_ems_unit_lookup(SC_EMSystem *ems, int unit_id);
int  sc_ems_unit_dispatch(SC_EMSystem *ems, int unit_id, int64_t incident_id);
int  sc_ems_unit_release(SC_EMSystem *ems, int unit_id);
int  sc_ems_unit_list_available(const SC_EMSystem *ems, SC_ResponseUnitType type, int *results, int max);

/* Greedy Resource Allocation (L5) */
int  sc_ems_allocate_resources(SC_EMSystem *ems, int64_t incident_id, int num_units_needed, SC_ResponseUnitType *types, int *assigned_units);
float sc_ems_response_time_estimate(const SC_EMSystem *ems, int unit_id, double incident_lat, double incident_lon);

/* Nearest Unit Search (L5) */
int  sc_ems_find_nearest_unit(const SC_EMSystem *ems, double lat, double lon, SC_ResponseUnitType type);
int  sc_ems_find_nearest_units(const SC_EMSystem *ems, double lat, double lon, int count, int *unit_ids, int max);

/* Shelter Management (L6) */
int  sc_ems_shelter_register(SC_EMSystem *ems, const SC_Shelter *shelter);
SC_Shelter* sc_ems_shelter_lookup(SC_EMSystem *ems, int shelter_id);
int  sc_ems_shelter_open(SC_EMSystem *ems, int shelter_id);
int  sc_ems_shelter_close(SC_EMSystem *ems, int shelter_id);
int  sc_ems_shelter_update_occupancy(SC_EMSystem *ems, int shelter_id, int occupancy);
int  sc_ems_shelter_find_nearest(SC_EMSystem *ems, double lat, double lon, int *results, int max);
int  sc_ems_shelter_total_capacity(const SC_EMSystem *ems);

/* Evacuation Management (L6) */
int  sc_ems_evac_zone_add(SC_EMSystem *ems, const SC_EvacuationZone *zone);
int  sc_ems_evac_order(SC_EMSystem *ems, int zone_id);
int  sc_ems_evac_lift(SC_EMSystem *ems, int zone_id);
int  sc_ems_evac_plan_route(SC_EMSystem *ems, int zone_id, int *shelter_ids, int max);

/* Alert System (L6) */
int  sc_ems_alert_send(SC_EMSystem *ems, const SC_CitizenAlert *alert);
int  sc_ems_alert_active_count(const SC_EMSystem *ems);
SC_CitizenAlert* sc_ems_alert_lookup(SC_EMSystem *ems, int64_t alert_id);

/* EMS Dashboard (L7) */
int  sc_ems_summary_report(const SC_EMSystem *ems, char *report, int buf_size);
int  sc_ems_resource_status(const SC_EMSystem *ems, char *report, int buf_size);

#endif /* MINI_SMARTCITY_EMS_H */
