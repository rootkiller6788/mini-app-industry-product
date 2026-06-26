#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "smartcity_core.h"
#include "smartcity_infra.h"
#include "smartcity_traffic.h"
#include "smartcity_env.h"
#include "smartcity_ems.h"

static int passed = 0, failed = 0;
#define T(n) do { printf("  %s ... ", n); } while(0)
#define P() do { printf("PASS\n"); passed++; } while(0)
#define F(m) do { printf("FAIL: %s\n", m); failed++; } while(0)
#define EQ(a,b) if((a)!=(b)){F("assert");return;}else{}
#define OK(c) if(!(c)){F("assert");return;}else{}
#define FEQ(a,b,e) if(fabs((a)-(b))>(e)){F("float");return;}else{}

static void test_city_create(void) {
    T("city_create");
    SC_CitySystem *city = sc_city_create("TestCity", 10);
    OK(city != NULL);
    EQ(sc_citizen_count(city), 0);
    sc_city_destroy(city);
    P();
}

static void test_citizen_register(void) {
    T("citizen_register");
    SC_CitySystem *city = sc_city_create("TestCity", 5);
    SC_Citizen c; memset(&c, 0, sizeof(c));
    strncpy(c.citizen_id, "CIT-001", SMARTCITY_CITIZEN_ID_LEN-1);
    strncpy(c.full_name, "John Doe", SMARTCITY_NAME_LEN-1);
    c.district_id = 2;
    int idx = sc_citizen_register(city, &c);
    OK(idx >= 0);
    EQ(sc_citizen_count(city), 1);
    SC_Citizen *f = sc_citizen_lookup(city, "CIT-001");
    OK(f != NULL);
    EQ(strcmp(f->full_name, "John Doe"), 0);
    sc_city_destroy(city);
    P();
}

static void test_service_request(void) {
    T("service_request");
    SC_CitySystem *city = sc_city_create("TestCity", 5);
    SC_Citizen c; memset(&c, 0, sizeof(c));
    strncpy(c.citizen_id, "CIT-001", SMARTCITY_CITIZEN_ID_LEN-1);
    sc_citizen_register(city, &c);
    SC_ServiceRequest req; memset(&req, 0, sizeof(req));
    strncpy(req.citizen_id, "CIT-001", SMARTCITY_CITIZEN_ID_LEN-1);
    req.category = SC_SVC_ROAD_POTHOLES;
    req.priority = SC_PRIORITY_MEDIUM;
    int64_t rid = sc_request_submit(city, &req);
    OK(rid >= 1000);
    SC_ServiceRequest *f = sc_request_lookup(city, rid);
    OK(f != NULL);
    EQ(f->status, SC_STATUS_SUBMITTED);
    sc_city_destroy(city);
    P();
}

static void test_workflow(void) {
    T("workflow");
    OK(sc_workflow_is_valid_transition(SC_STATUS_SUBMITTED, SC_STATUS_ACKNOWLEDGED));
    OK(sc_workflow_is_valid_transition(SC_STATUS_ACKNOWLEDGED, SC_STATUS_TRIAGED));
    OK(!sc_workflow_is_valid_transition(SC_STATUS_SUBMITTED, SC_STATUS_RESOLVED));
    OK(sc_workflow_is_valid_transition(SC_STATUS_CLOSED, SC_STATUS_REOPENED));
    EQ(sc_workflow_get_sla_hours(SC_STATUS_SUBMITTED, SC_STATUS_ACKNOWLEDGED), 4);
    EQ(sc_workflow_get_sla_hours(SC_STATUS_IN_PROGRESS, SC_STATUS_RESOLVED), 120);
    P();
}

static void test_pq(void) {
    T("priority_queue");
    SC_PriorityQueue *pq = sc_pq_create(16);
    OK(pq != NULL);
    OK(sc_pq_is_empty(pq));
    sc_pq_push(pq, 1001, SC_PRIORITY_LOW, time(NULL)+100);
    sc_pq_push(pq, 1002, SC_PRIORITY_CRITICAL, time(NULL)+50);
    sc_pq_push(pq, 1003, SC_PRIORITY_HIGH, time(NULL)+200);
    EQ(sc_pq_size(pq), 3);
    EQ(sc_pq_pop(pq), 1002);
    EQ(sc_pq_pop(pq), 1003);
    EQ(sc_pq_pop(pq), 1001);
    OK(sc_pq_is_empty(pq));
    sc_pq_destroy(pq);
    P();
}

static void test_sentiment(void) {
    T("sentiment");
    float s1 = sc_sentiment_analyze("great service, helpful staff");
    OK(s1 > 2.5f);
    float s2 = sc_sentiment_analyze("terrible service, rude staff");
    OK(s2 < 2.5f);
    float s3 = sc_sentiment_analyze("");
    FEQ(s3, 2.5f, 0.01f);
    P();
}

static void test_triage(void) {
    T("auto_triage");
    SC_CitySystem *city = sc_city_create("TestCity", 5);
    SC_Citizen c; memset(&c, 0, sizeof(c));
    strncpy(c.citizen_id, "CIT-001", SMARTCITY_CITIZEN_ID_LEN-1);
    sc_citizen_register(city, &c);
    SC_ServiceRequest req; memset(&req, 0, sizeof(req));
    strncpy(req.citizen_id, "CIT-001", SMARTCITY_CITIZEN_ID_LEN-1);
    req.category = SC_SVC_ROAD_POTHOLES;
    int64_t rid = sc_request_submit(city, &req);
    EQ(sc_auto_triage(city, rid), 0);
    SC_ServiceRequest *f = sc_request_lookup(city, rid);
    OK(f != NULL);
    EQ(f->assigned_department, SC_DEPT_TRANSPORTATION);
    sc_city_destroy(city);
    P();
}

static void test_infra_sensor(void) {
    T("infra_sensor");
    SC_InfraSystem *infra = sc_infra_create();
    OK(infra != NULL);
    SC_Sensor s; memset(&s, 0, sizeof(s));
    strncpy(s.sensor_id, "SENS-001", SC_INFRA_SENSOR_ID_LEN-1);
    s.type = SC_SENSOR_TEMPERATURE;
    s.is_active = true;
    s.min_threshold = -10.0; s.max_threshold = 50.0;
    OK(sc_infra_sensor_register(infra, &s) >= 0);
    sc_infra_sensor_record_reading(infra, "SENS-001", 25.0, time(NULL));
    EQ(sc_infra_sensor_count_active(infra), 1);
    sc_infra_ewma_update(infra, "SENS-001", 25.0, 0.3);
    sc_infra_destroy(infra);
    P();
}

static void test_infra_asset(void) {
    T("infra_asset");
    SC_InfraSystem *infra = sc_infra_create();
    SC_InfraAsset a; memset(&a, 0, sizeof(a));
    strncpy(a.asset_id, "AST-001", SC_INFRA_ASSET_ID_LEN-1);
    a.type = SC_ASSET_BRIDGE;
    a.condition = SC_CONDITION_GOOD;
    a.construction_year = 2000;
    a.expected_lifetime_years = 80;
    OK(sc_infra_asset_register(infra, &a) >= 0);
    SC_InfraAsset *f = sc_infra_asset_lookup(infra, "AST-001");
    OK(f != NULL);
    float health = sc_infra_asset_compute_health(f, time(NULL));
    OK(health > 0.0f && health <= 100.0f);
    sc_infra_destroy(infra);
    P();
}

static void test_traffic_webster(void) {
    T("traffic_webster");
    int flows[] = {600, 500};
    int cycle = sc_traffic_webster_cycle(2, flows, 1800, 4);
    OK(cycle >= 30 && cycle <= 120);
    P();
}

static void test_traffic_parking(void) {
    T("traffic_parking");
    SC_TrafficSystem *traf = sc_traffic_create();
    OK(traf != NULL);
    SC_ParkingLot lot; memset(&lot, 0, sizeof(lot));
    lot.lot_id = 1; lot.total_spaces = 100; lot.is_open = true;
    sc_traffic_parking_add(traf, &lot);
    sc_traffic_parking_update_occupancy(traf, 1, 45);
    SC_ParkingLot *f = sc_traffic_parking_lookup(traf, 1);
    OK(f != NULL);
    EQ(f->occupied_spaces, 45);
    sc_traffic_destroy(traf);
    P();
}

static void test_env_aqi(void) {
    T("env_aqi");
    int aqi = sc_env_aqi_compute_pm25(35.0);
    OK(aqi >= 0 && aqi <= 500);
    int overall = sc_env_aqi_overall(35.0, 100.0, 0.05, 5.0, 0.03, 0.04);
    OK(overall > 0);
    SC_AQICategory cat = sc_env_aqi_category(overall);
    OK(cat >= SC_AQI_GOOD && cat <= SC_AQI_HAZARDOUS);
    P();
}

static void test_env_waste(void) {
    T("env_waste");
    SC_EnvSystem *env = sc_env_create();
    OK(env != NULL);
    SC_WasteBin bin; memset(&bin, 0, sizeof(bin));
    bin.bin_id = 1; bin.capacity_liters = 240; bin.waste_type = SC_WASTE_GENERAL;
    sc_env_waste_bin_add(env, &bin);
    sc_env_waste_bin_update_fill(env, 1, 200);
    SC_WasteBin *f = sc_env_waste_bin_lookup(env, 1);
    OK(f != NULL && f->fill_percentage > 80.0f);
    sc_env_destroy(env);
    P();
}

static void test_ems_incident(void) {
    T("ems_incident");
    SC_EMSystem *ems = sc_ems_create();
    OK(ems != NULL);
    SC_EmergencyIncident inc; memset(&inc, 0, sizeof(inc));
    inc.type = SC_INC_FIRE; inc.severity = SC_SEV_MAJOR; inc.affected_population = 500;
    int64_t iid = sc_ems_incident_report(ems, &inc);
    OK(iid >= 10000);
    SC_EmergencyIncident *f = sc_ems_incident_lookup(ems, iid);
    OK(f != NULL);
    EQ(f->status, SC_INC_STATUS_REPORTED);
    EQ(sc_ems_incident_count_active(ems), 1);
    sc_ems_destroy(ems);
    P();
}

static void test_ems_dispatch(void) {
    T("ems_dispatch");
    SC_EMSystem *ems = sc_ems_create();
    SC_EmergencyIncident inc; memset(&inc, 0, sizeof(inc));
    inc.type = SC_INC_FIRE; inc.severity = SC_SEV_MAJOR;
    inc.latitude = 40.7128; inc.longitude = -74.0060;
    int64_t iid = sc_ems_incident_report(ems, &inc);
    SC_ResponseUnit unit; memset(&unit, 0, sizeof(unit));
    unit.unit_id = 100; unit.type = SC_UNIT_FIRE_ENGINE;
    unit.station_latitude = 40.7200; unit.station_longitude = -74.0100;
    sc_ems_unit_register(ems, &unit);
    EQ(sc_ems_unit_dispatch(ems, 100, iid), 0);
    SC_ResponseUnit *u = sc_ems_unit_lookup(ems, 100);
    OK(u != NULL && !u->is_available);
    SC_EmergencyIncident *f = sc_ems_incident_lookup(ems, iid);
    EQ(f->num_units_dispatched, 1);
    sc_ems_destroy(ems);
    P();
}

int main(void) {
    printf("=== Smart City Test Suite ===\n\nCore:\n");
    test_city_create();
    test_citizen_register();
    test_service_request();
    test_workflow();
    test_pq();
    test_sentiment();
    test_triage();
    printf("\nInfra:\n");
    test_infra_sensor();
    test_infra_asset();
    printf("\nTraffic:\n");
    test_traffic_webster();
    test_traffic_parking();
    printf("\nEnv:\n");
    test_env_aqi();
    test_env_waste();
    printf("\nEMS:\n");
    test_ems_incident();
    test_ems_dispatch();
    printf("\n=== %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}