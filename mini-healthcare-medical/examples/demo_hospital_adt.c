#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "hospital_ops.h"

int main(void) {
    printf("=== Hospital ADT Operations Demo ===\n");

    HospitalSystem *hs = hosp_system_create();
    assert(hs);

    int icu_id = hosp_add_ward(hs, "ICU", HOSP_WARD_ICU, 12, true, 555);
    int gen_id = hosp_add_ward(hs, "General Ward", HOSP_WARD_GENERAL, 30, false, 601);
    int er_id = hosp_add_ward(hs, "Emergency Dept", HOSP_WARD_EMERGENCY, 20, true, 911);
    printf("Wards: %d (ICU=%d, General=%d, ER=%d)\n",
           hs->ward_count, icu_id, gen_id, er_id);

    int b1 = hosp_add_bed(hs, icu_id, "ICU-1", false, true, true);
    int b2 = hosp_add_bed(hs, icu_id, "ICU-2", false, true, true);
    int b3 = hosp_add_bed(hs, gen_id, "GEN-1", true, false, false);
    int b4 = hosp_add_bed(hs, gen_id, "GEN-2", false, false, false);
    printf("Beds: %d\n", hs->bed_count);

    /* Bed management */
    assert(hosp_occupy_bed(hs, b1, 101));
    assert(hosp_occupy_bed(hs, b2, 102));
    printf("ICU occupancy: %d%%\n", hosp_ward_occupancy_pct(hs, icu_id));
    printf("Available ICU beds: %d\n",
           hosp_available_beds_count(hs, HOSP_WARD_ICU));

    HospitalBed *found = hosp_find_available_bed(hs, HOSP_WARD_GENERAL,
                                                  false, false);
    assert(found);
    printf("Found available bed: %s\n", found->bed_label);

    assert(hosp_release_bed(hs, b1));
    printf("Bed ICU-1 released (status: CLEANING)\n");

    /* Triage examples */
    printf("Triage assessments:\n");
    struct { const char *complaint; int age, hr, rr, sbp, spo2; double temp; }
    cases[] = {
        {"Cardiac arrest", 65, 0, 0, 60, 70, 35.0},
        {"Stroke symptoms", 70, 100, 22, 160, 94, 37.0},
        {"Chest pain", 55, 95, 20, 130, 95, 37.2},
        {"Ankle sprain", 30, 80, 18, 120, 98, 36.8},
        {"Medication refill", 45, 75, 16, 125, 99, 36.6},
    };
    for (int i = 0; i < 5; i++) {
        HospTriageLevel tl = hosp_triage_assess(cases[i].complaint,
            cases[i].age, cases[i].hr, cases[i].rr,
            cases[i].sbp, cases[i].spo2, cases[i].temp);
        printf("  %-20s -> %s (wait: %d min)\n",
               cases[i].complaint, hosp_triage_label(tl),
               hosp_triage_wait_time_minutes(tl));
    }

    /* ADT workflow */
    int adm1 = hosp_admit_patient(hs, 201, "John Smith", "chest pain",
                                   55, 95, 20, 130, 95, 37.2);
    assert(adm1 > 0);
    AdmissionRecord *ar = hosp_find_admission(hs, adm1);
    assert(ar);
    printf("Admitted: %s (#%d) triage=%s\n",
           ar->patient_name, adm1, hosp_triage_label(ar->triage));

    assert(hosp_occupy_bed(hs, b3, 201));
    ar->assigned_bed_id = b3;
    ar->admitted = true;

    assert(hosp_transfer_patient(hs, adm1, icu_id));
    printf("Transferred to ICU\n");

    assert(hosp_discharge_patient(hs, adm1));
    printf("Discharged John Smith\n");

    /* Appointments */
    time_t tomorrow = time(NULL) + 86400;
    int appt1 = hosp_schedule_appointment(hs, 301, 401, tomorrow, 30,
        "Cardiology", "Post-MI follow-up");
    assert(appt1 > 0);
    assert(hosp_confirm_appointment(hs, appt1));
    int appt2 = hosp_schedule_appointment(hs, 302, 402, tomorrow + 3600, 15,
        "Dermatology", "Skin check");
    assert(appt2 > 0);
    assert(hosp_check_in_appointment(hs, appt1));
    printf("Appointments: 2 scheduled, 1 checked in\n");

    time_t day_start = tomorrow - (tomorrow % 86400);
    int appt_ids[10];
    int daily_n = hosp_get_daily_appointments(hs, day_start, appt_ids, 10);
    printf("Daily appointments: %d\n", daily_n);

    /* Staff management */
    int s1 = hosp_add_staff(hs, "Dr. Chen", "Cardiologist", "Cardiology");
    int s2 = hosp_add_staff(hs, "Nurse Jones", "RN", "ICU");
    hosp_assign_patient_to_staff(hs, s1, 101);
    hosp_assign_patient_to_staff(hs, s1, 102);
    printf("Staff: %d members\n", hs->staff_count);
    printf("Dr. Chen utilization: %.0f%% (2/8)\n",
           hosp_staff_utilization(hs, s1, 8) * 100.0);
    printf("Available RN in ICU: staff #%d\n",
           hosp_find_available_staff(hs, "RN", "ICU"));

    /* Remote monitoring */
    RemoteMonitor rm;
    hosp_remote_monitor_init(&rm, 501);
    rm.heart_rate = 145;
    rm.spo2 = 88;
    rm.systolic_bp = 185;
    rm.temperature = 39.5;
    bool alert = hosp_remote_check_alerts(&rm);
    printf("Remote monitor alert: %s\n", alert ? "TRIGGERED" : "OK");
    if (alert) {
        printf("  Reason: %s\n", rm.alert_reason);
        printf("  Escalation level: %d\n", hosp_remote_escalation_level(&rm));
    }

    rm.heart_rate = 75;
    rm.spo2 = 98;
    rm.systolic_bp = 125;
    rm.temperature = 36.8;
    assert(!hosp_remote_check_alerts(&rm));
    printf("Normal vitals: no alert (escalation=%d)\n",
           hosp_remote_escalation_level(&rm));

    hosp_system_destroy(hs);
    printf("All hospital ADT demos passed.\n");
    return 0;
}
