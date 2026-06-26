/* Example: Citizen 311 Service Portal
 * Demonstrates the complete citizen service request lifecycle:
 * registration, submission, auto-triage, assignment, resolution.
 * L6: Canonical Problem - Municipal 311 System */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "smartcity_core.h"

int main(void) {
    printf("=== Citizen 311 Portal Demo ===\n\n");
    SC_CitySystem *city = sc_city_create("Springfield", 8);
    /* Register citizens */
    SC_Citizen c1; memset(&c1, 0, sizeof(c1));
    strncpy(c1.citizen_id, "CIT-0001", SMARTCITY_CITIZEN_ID_LEN-1);
    strncpy(c1.full_name, "Alice Johnson", SMARTCITY_NAME_LEN-1);
    strncpy(c1.address, "123 Main St", SMARTCITY_ADDR_LEN-1);
    c1.district_id = 3; c1.latitude = 40.7128; c1.longitude = -74.0060;
    sc_citizen_register(city, &c1);

    SC_Citizen c2; memset(&c2, 0, sizeof(c2));
    strncpy(c2.citizen_id, "CIT-0002", SMARTCITY_CITIZEN_ID_LEN-1);
    strncpy(c2.full_name, "Bob Smith", SMARTCITY_NAME_LEN-1);
    c2.district_id = 5;
    sc_citizen_register(city, &c2);

    printf("Registered %d citizens\n", sc_citizen_count(city));

    /* Submit service requests */
    SC_ServiceRequest r1; memset(&r1, 0, sizeof(r1));
    strncpy(r1.citizen_id, "CIT-0001", SMARTCITY_CITIZEN_ID_LEN-1);
    r1.category = SC_SVC_ROAD_POTHOLES;
    r1.priority = SC_PRIORITY_HIGH;
    strncpy(r1.description, "Large pothole on Main St near 5th Ave", SMARTCITY_DESC_LEN-1);
    int64_t rid1 = sc_request_submit(city, &r1);
    printf("Request #%lld submitted: %s\n", (long long)rid1, r1.description);

    SC_ServiceRequest r2; memset(&r2, 0, sizeof(r2));
    strncpy(r2.citizen_id, "CIT-0002", SMARTCITY_CITIZEN_ID_LEN-1);
    r2.category = SC_SVC_STREET_LIGHTING;
    r2.priority = SC_PRIORITY_MEDIUM;
    strncpy(r2.description, "Street light out on Oak Ave", SMARTCITY_DESC_LEN-1);
    int64_t rid2 = sc_request_submit(city, &r2);
    printf("Request #%lld submitted: %s\n", (long long)rid2, r2.description);

    /* Auto-triage routes to correct department */
    sc_auto_triage(city, rid1);
    sc_auto_triage(city, rid2);

    SC_ServiceRequest *req1 = sc_request_lookup(city, rid1);
    SC_ServiceRequest *req2 = sc_request_lookup(city, rid2);
    printf("\nAfter triage:\n");
    printf("  #%lld -> Dept: %d, Status: %s\n", (long long)rid1,
           req1->assigned_department, sc_status_name(req1->status));
    printf("  #%lld -> Dept: %d, Status: %s\n", (long long)rid2,
           req2->assigned_department, sc_status_name(req2->status));

    /* Assign and resolve */
    sc_request_assign(city, rid1, SC_DEPT_TRANSPORTATION, 42);
    sc_request_update_status(city, rid1, SC_STATUS_IN_PROGRESS);
    sc_request_update_status(city, rid1, SC_STATUS_RESOLVED);
    sc_request_update_status(city, rid1, SC_STATUS_CLOSED);
    printf("\nRequest #%lld resolved and closed\n", (long long)rid1);

    /* Dashboard */
    SC_DashboardKPI kpi = sc_dashboard_compute(city);
    printf("\n=== City Dashboard ===\n");
    printf("Total citizens: %d\n", kpi.total_citizens);
    printf("Active requests: %d\n", kpi.active_requests);
    printf("Overdue: %d\n", kpi.overdue_requests);
    printf("Resolved today: %d\n", kpi.resolved_today);
    printf("Avg satisfaction: %.1f\n", kpi.avg_satisfaction);
    printf("Budget utilization: %.1f%%\n", kpi.budget_utilization_pct);

    /* Sentiment */
    float sentiment = sc_sentiment_analyze("The road was fixed quickly, great service!");
    printf("\nCitizen feedback sentiment: %.1f/5.0\n", sentiment);

    /* Digital twin snapshot */
    char json[1024];
    sc_digital_twin_snapshot(city, json, sizeof(json));
    printf("\nDigital Twin JSON: %s\n", json);

    sc_city_destroy(city);
    printf("\nDemo complete.\n");
    return 0;
}