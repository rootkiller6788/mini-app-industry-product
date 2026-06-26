/* Example: Emergency Response System
 * Demonstrates incident reporting, triage, resource allocation,
 * unit dispatch, shelter management, and evacuation planning.
 * L6: Canonical Problem - Emergency Response Dispatch System */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "smartcity_ems.h"

int main(void) {
    printf("=== Emergency Response Demo ===\n\n");
    SC_EMSystem *ems = sc_ems_create();

    /* Register response units */
    SC_ResponseUnit u1; memset(&u1, 0, sizeof(u1));
    u1.unit_id = 1; u1.type = SC_UNIT_FIRE_ENGINE;
    strncpy(u1.unit_name, "Engine 42", 128);
    u1.station_latitude = 40.7150; u1.station_longitude = -74.0080;
    u1.personnel_count = 4;
    sc_ems_unit_register(ems, &u1);

    SC_ResponseUnit u2; memset(&u2, 0, sizeof(u2));
    u2.unit_id = 2; u2.type = SC_UNIT_AMBULANCE;
    strncpy(u2.unit_name, "Medic 7", 128);
    u2.station_latitude = 40.7140; u2.station_longitude = -74.0070;
    u2.personnel_count = 2;
    sc_ems_unit_register(ems, &u2);

    SC_ResponseUnit u3; memset(&u3, 0, sizeof(u3));
    u3.unit_id = 3; u3.type = SC_UNIT_FIRE_ENGINE;
    strncpy(u3.unit_name, "Engine 55", 128);
    u3.station_latitude = 40.7200; u3.station_longitude = -74.0120;
    u3.personnel_count = 5;
    sc_ems_unit_register(ems, &u3);

    printf("Registered %d units\n", 3);

    /* Report incident */
    SC_EmergencyIncident inc; memset(&inc, 0, sizeof(inc));
    inc.type = SC_INC_FIRE;
    inc.severity = SC_SEV_MAJOR;
    inc.latitude = 40.7128; inc.longitude = -74.0060;
    strncpy(inc.description, "Warehouse fire, 3-story building", 512);
    strncpy(inc.location_name, "550 Industrial Blvd", 256);
    inc.affected_population = 200;
    inc.casualties = 3;
    inc.district_id = 3;

    int64_t iid = sc_ems_incident_report(ems, &inc);
    printf("Incident #%lld reported: %s\n", (long long)iid, inc.description);

    /* Verify and assess severity */
    sc_ems_incident_update_status(ems, iid, SC_INC_STATUS_VERIFIED);
    printf("Incident verified\n");

    /* Triage priority */
    int triage = sc_ems_triage_priority(inc.type, inc.affected_population,
                                         inc.casualties, inc.severity);
    printf("Triage priority score: %d (1=highest)\n", triage);

    /* Find nearest fire engine */
    int nearest_fire = sc_ems_find_nearest_unit(ems, inc.latitude, inc.longitude,
                                                  SC_UNIT_FIRE_ENGINE);
    printf("Nearest fire engine: unit %d\n", nearest_fire);

    /* Allocate resources */
    SC_ResponseUnitType needed[] = {SC_UNIT_FIRE_ENGINE, SC_UNIT_AMBULANCE};
    int assigned[4];
    int n_assigned = sc_ems_allocate_resources(ems, iid, 2, needed, assigned);
    printf("Allocated %d units: ", n_assigned);
    for (int i = 0; i < n_assigned; i++) printf("unit %d ", assigned[i]);
    printf("\n");

    /* Shelter management */
    SC_Shelter s1; memset(&s1, 0, sizeof(s1));
    s1.shelter_id = 1; s1.capacity = 500;
    strncpy(s1.name, "Community Center", 128);
    s1.latitude = 40.7100; s1.longitude = -74.0040;
    s1.has_medical = true; s1.has_food_water = true;
    sc_ems_shelter_register(ems, &s1);
    sc_ems_shelter_open(ems, 1);
    sc_ems_shelter_update_occupancy(ems, 1, 150);

    SC_Shelter s2; memset(&s2, 0, sizeof(s2));
    s2.shelter_id = 2; s2.capacity = 300;
    strncpy(s2.name, "High School Gym", 128);
    s2.latitude = 40.7180; s2.longitude = -74.0090;
    s2.wheelchair_accessible = true;
    sc_ems_shelter_register(ems, &s2);

    printf("\nTotal shelter capacity open: %d\n", sc_ems_shelter_total_capacity(ems));

    /* Find nearest shelters to incident */
    int shelters[5];
    int nsh = sc_ems_shelter_find_nearest(ems, inc.latitude, inc.longitude, shelters, 5);
    printf("Nearest shelters: ");
    for (int i = 0; i < nsh; i++) {
        SC_Shelter *sh = sc_ems_shelter_lookup(ems, shelters[i]);
        if (sh) printf("%s(%d/%d) ", sh->name, sh->current_occupancy, sh->capacity);
    }
    printf("\n");

    /* Send alert */
    SC_CitizenAlert alert; memset(&alert, 0, sizeof(alert));
    alert.related_incident = SC_INC_FIRE;
    strncpy(alert.message, "EVACUATION ORDER: Warehouse fire. Leave area immediately.", 512);
    alert.is_emergency = true;
    alert.requires_action = true;
    alert.target_district = 3;
    alert.expires_at = time(NULL) + 86400;
    sc_ems_alert_send(ems, &alert);
    printf("\nAlert sent: %s\n", alert.message);

    /* Summary report */
    char report[512];
    sc_ems_summary_report(ems, report, sizeof(report));
    printf("\n=== EMS Summary ===\n%s\n", report);

    sc_ems_resource_status(ems, report, sizeof(report));
    printf("=== Resources ===\n%s\n", report);

    /* Resolve incident */
    sc_ems_incident_update_status(ems, iid, SC_INC_STATUS_CONTAINED);
    sc_ems_incident_update_status(ems, iid, SC_INC_STATUS_RESOLVED);
    sc_ems_incident_update_status(ems, iid, SC_INC_STATUS_CLOSED);
    printf("\nIncident resolved and closed\n");

    sc_ems_destroy(ems);
    printf("\nDemo complete.\n");
    return 0;
}