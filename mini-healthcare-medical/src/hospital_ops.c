#include "hospital_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

HospitalSystem* hosp_system_create(void) {
    HospitalSystem *hs = malloc(sizeof(HospitalSystem));
    if (hs) memset(hs, 0, sizeof(HospitalSystem));
    return hs;
}

void hosp_system_destroy(HospitalSystem *hs) { free(hs); }

void hosp_system_init(HospitalSystem *hs) {
    if (hs) memset(hs, 0, sizeof(HospitalSystem));
}

int hosp_add_ward(HospitalSystem *hs, const char *name, HospWardType type,
                   int total_beds, bool isolation, int phone) {
    if (!hs || !name || hs->ward_count >= HOSP_MAX_WARDS) return -1;
    HospitalWard *w = &hs->wards[hs->ward_count];
    w->id = hs->ward_count + 1;
    snprintf(w->name, sizeof(w->name), "%s", name);
    w->type = type;
    w->total_beds = total_beds;
    w->available_beds = total_beds;
    w->nurse_station_phone = phone;
    w->has_isolation_capability = isolation;
    w->active = true;
    hs->ward_count++;
    return w->id;
}

int hosp_add_bed(HospitalSystem *hs, int ward_id, const char *label,
                  bool bariatric, bool telemetry, bool neg_pressure) {
    if (!hs || !label || hs->bed_count >= HOSP_MAX_BEDS) return -1;
    HospitalBed *b = &hs->beds[hs->bed_count];
    b->id = hs->bed_count + 1;
    b->ward_id = ward_id;
    snprintf(b->bed_label, sizeof(b->bed_label), "%s", label);
    b->status = HOSP_BED_AVAILABLE;
    b->bariatric = bariatric;
    b->telemetry_capable = telemetry;
    b->negative_pressure = neg_pressure;
    b->current_patient_id = 0;
    hs->bed_count++;
    return b->id;
}

HospitalBed* hosp_find_bed(HospitalSystem *hs, int bed_id) {
    if (!hs || bed_id <= 0 || bed_id > hs->bed_count) return NULL;
    return &hs->beds[bed_id - 1];
}

bool hosp_occupy_bed(HospitalSystem *hs, int bed_id, int patient_id) {
    HospitalBed *b = hosp_find_bed(hs, bed_id);
    if (!b || b->status != HOSP_BED_AVAILABLE) return false;
    b->status = HOSP_BED_OCCUPIED;
    b->current_patient_id = patient_id;
    b->occupied_since = time(NULL);
    for (int i = 0; i < hs->ward_count; i++) {
        if (hs->wards[i].id == b->ward_id) {
            hs->wards[i].available_beds--;
            break;
        }
    }
    return true;
}

bool hosp_release_bed(HospitalSystem *hs, int bed_id) {
    HospitalBed *b = hosp_find_bed(hs, bed_id);
    if (!b || b->status != HOSP_BED_OCCUPIED) return false;
    b->status = HOSP_BED_CLEANING;
    b->current_patient_id = 0;
    b->cleaning_since = time(NULL);
    for (int i = 0; i < hs->ward_count; i++) {
        if (hs->wards[i].id == b->ward_id) {
            hs->wards[i].available_beds++;
            break;
        }
    }
    return true;
}

bool hosp_set_bed_status(HospitalSystem *hs, int bed_id, HospBedStatus status) {
    HospitalBed *b = hosp_find_bed(hs, bed_id);
    if (!b) return false;
    b->status = status;
    return true;
}

HospTriageLevel hosp_triage_assess(const char *chief_complaint, int age, int hr,
                                    int rr, int sbp, int spo2, double temp) {
    int danger = 0;
    if (hr <= 40 || hr >= 140) danger++;
    if (rr <= 8 || rr >= 30) danger++;
    if (sbp <= 80) danger++;
    if (spo2 <= 88) danger++;
    if (temp >= 41.0 || temp <= 33.0) danger++;
    if (age >= 80) danger++;
    (void)chief_complaint;
    if (danger >= 2) return HOSP_TRIAGE_RESUSCITATION;
    if (danger >= 1) return HOSP_TRIAGE_EMERGENT;
    if (age >= 65 || spo2 <= 92) return HOSP_TRIAGE_URGENT;
    if (hr >= 110 || sbp >= 180) return HOSP_TRIAGE_SEMI_URGENT;
    return HOSP_TRIAGE_NON_URGENT;
}

const char* hosp_triage_label(HospTriageLevel level) {
    switch (level) {
        case HOSP_TRIAGE_RESUSCITATION: return "RESUSCITATION (Immediate)";
        case HOSP_TRIAGE_EMERGENT: return "EMERGENT (Within 10 min)";
        case HOSP_TRIAGE_URGENT: return "URGENT (Within 60 min)";
        case HOSP_TRIAGE_SEMI_URGENT: return "SEMI-URGENT (Within 120 min)";
        case HOSP_TRIAGE_NON_URGENT: return "NON-URGENT (Within 240 min)";
        default: return "UNKNOWN";
    }
}

int hosp_triage_wait_time_minutes(HospTriageLevel level) {
    switch (level) {
        case HOSP_TRIAGE_RESUSCITATION: return 0;
        case HOSP_TRIAGE_EMERGENT: return 10;
        case HOSP_TRIAGE_URGENT: return 60;
        case HOSP_TRIAGE_SEMI_URGENT: return 120;
        case HOSP_TRIAGE_NON_URGENT: return 240;
        default: return 999;
    }
}

HospitalBed* hosp_find_available_bed(HospitalSystem *hs, HospWardType preferred_type,
                                      bool needs_isolation, bool needs_telemetry) {
    if (!hs) return NULL;
    for (int i = 0; i < hs->bed_count; i++) {
        HospitalBed *b = &hs->beds[i];
        if (b->status != HOSP_BED_AVAILABLE) continue;
        if (needs_isolation && !b->negative_pressure) continue;
        if (needs_telemetry && !b->telemetry_capable) continue;
        for (int j = 0; j < hs->ward_count; j++) {
            if (hs->wards[j].id == b->ward_id) {
                if (preferred_type == hs->wards[j].type || preferred_type == HOSP_WARD_GENERAL) {
                    return b;
                }
                break;
            }
        }
    }
    for (int i = 0; i < hs->bed_count; i++) {
        if (hs->beds[i].status == HOSP_BED_AVAILABLE &&
            (!needs_isolation || hs->beds[i].negative_pressure) &&
            (!needs_telemetry || hs->beds[i].telemetry_capable)) {
            return &hs->beds[i];
        }
    }
    return NULL;
}

int hosp_available_beds_count(const HospitalSystem *hs, HospWardType type) {
    if (!hs) return 0;
    int count = 0;
    for (int i = 0; i < hs->ward_count; i++) {
        if (hs->wards[i].type == type || type == HOSP_WARD_GENERAL) {
            count += hs->wards[i].available_beds;
        }
    }
    return count;
}

int hosp_ward_occupancy_pct(const HospitalSystem *hs, int ward_id) {
    if (!hs || ward_id <= 0 || ward_id > hs->ward_count) return 0;
    const HospitalWard *w = &hs->wards[ward_id - 1];
    if (w->total_beds == 0) return 0;
    int occupied = w->total_beds - w->available_beds;
    return (occupied * 100) / w->total_beds;
}

int hosp_admit_patient(HospitalSystem *hs, int patient_id, const char *name,
                        const char *complaint, int age, int hr, int rr,
                        int sbp, int spo2, double temp) {
    if (!hs || !name || hs->admission_count >= HOSP_MAX_PATIENTS) return -1;
    AdmissionRecord *adm = &hs->admissions[hs->admission_count];
    adm->id = hs->admission_count + 1;
    adm->patient_id = patient_id;
    snprintf(adm->patient_name, sizeof(adm->patient_name), "%s", name);
    snprintf(adm->chief_complaint, sizeof(adm->chief_complaint), "%s", complaint ? complaint : "");
    adm->triage = hosp_triage_assess(complaint, age, hr, rr, sbp, spo2, temp);
    adm->arrival_time = time(NULL);
    adm->admitted = false;
    adm->assigned_bed_id = 0;
    hs->admission_count++;
    return adm->id;
}

bool hosp_discharge_patient(HospitalSystem *hs, int admission_id) {
    if (!hs || admission_id <= 0 || admission_id > hs->admission_count) return false;
    AdmissionRecord *adm = &hs->admissions[admission_id - 1];
    if (adm->assigned_bed_id > 0) {
        hosp_release_bed(hs, adm->assigned_bed_id);
    }
    adm->admitted = false;
    return true;
}

bool hosp_transfer_patient(HospitalSystem *hs, int admission_id, int to_ward_id) {
    if (!hs) return false;
    AdmissionRecord *adm = &hs->admissions[admission_id - 1];
    /* Try preferred ward first, then fall back to general */
    HospitalBed *new_bed = hosp_find_available_bed(hs,
        (to_ward_id > 0 && to_ward_id <= hs->ward_count) ? hs->wards[to_ward_id - 1].type : HOSP_WARD_GENERAL,
        false, false);
    if (!new_bed) return false;
    if (adm->assigned_bed_id > 0) hosp_release_bed(hs, adm->assigned_bed_id);
    hosp_occupy_bed(hs, new_bed->id, adm->patient_id);
    adm->assigned_bed_id = new_bed->id;
    return true;
}

AdmissionRecord* hosp_find_admission(const HospitalSystem *hs, int admission_id) {
    if (!hs || admission_id <= 0 || admission_id > hs->admission_count) return NULL;
    return (AdmissionRecord*)&hs->admissions[admission_id - 1];
}

int hosp_schedule_appointment(HospitalSystem *hs, int patient_id, int doctor_id,
                               time_t when, int duration_min, const char *dept,
                               const char *reason) {
    if (!hs || hs->appointment_count >= HOSP_MAX_APPOINTMENTS) return -1;
    Appointment *a = &hs->appointments[hs->appointment_count];
    a->id = hs->appointment_count + 1;
    a->patient_id = patient_id;
    a->doctor_id = doctor_id;
    a->scheduled_time = when;
    a->duration_min = duration_min;
    snprintf(a->department, sizeof(a->department), "%s", dept ? dept : "");
    snprintf(a->reason, sizeof(a->reason), "%s", reason ? reason : "");
    a->confirmed = false;
    a->completed = false;
    a->no_show = false;
    hs->appointment_count++;
    return a->id;
}

bool hosp_confirm_appointment(HospitalSystem *hs, int appt_id) {
    if (!hs || appt_id <= 0 || appt_id > hs->appointment_count) return false;
    hs->appointments[appt_id - 1].confirmed = true;
    return true;
}

bool hosp_check_in_appointment(HospitalSystem *hs, int appt_id) {
    if (!hs || appt_id <= 0 || appt_id > hs->appointment_count) return false;
    hs->appointments[appt_id - 1].completed = true;
    return true;
}

int hosp_get_daily_appointments(const HospitalSystem *hs, time_t day_start,
                                 int *appt_ids, int max_count) {
    if (!hs || !appt_ids) return 0;
    time_t day_end = day_start + 86400;
    int count = 0;
    for (int i = 0; i < hs->appointment_count && count < max_count; i++) {
        if (hs->appointments[i].scheduled_time >= day_start &&
            hs->appointments[i].scheduled_time < day_end) {
            appt_ids[count++] = hs->appointments[i].id;
        }
    }
    return count;
}

int hosp_add_staff(HospitalSystem *hs, const char *name, const char *role,
                    const char *dept) {
    if (!hs || !name || hs->staff_count >= HOSP_MAX_STAFF) return -1;
    StaffMember *s = &hs->staff[hs->staff_count];
    s->id = hs->staff_count + 1;
    snprintf(s->name, sizeof(s->name), "%s", name);
    snprintf(s->role, sizeof(s->role), "%s", role ? role : "");
    snprintf(s->department, sizeof(s->department), "%s", dept ? dept : "");
    s->active = true;
    s->current_patient_load = 0;
    hs->staff_count++;
    return s->id;
}

int hosp_assign_patient_to_staff(HospitalSystem *hs, int staff_id, int patient_id) {
    if (!hs || staff_id <= 0 || staff_id > hs->staff_count) return -1;
    StaffMember *s = &hs->staff[staff_id - 1];
    s->current_patient_load++;
    (void)patient_id;
    return s->current_patient_load;
}

double hosp_staff_utilization(const HospitalSystem *hs, int staff_id,
                               int max_patients) {
    if (!hs || staff_id <= 0 || staff_id > hs->staff_count || max_patients <= 0)
        return 0.0;
    const StaffMember *s = &hs->staff[staff_id - 1];
    return (double)s->current_patient_load / (double)max_patients;
}

int hosp_find_available_staff(const HospitalSystem *hs, const char *role,
                               const char *dept) {
    if (!hs) return 0;
    for (int i = 0; i < hs->staff_count; i++) {
        if (hs->staff[i].active &&
            (!role || strstr(hs->staff[i].role, role)) &&
            (!dept || strstr(hs->staff[i].department, dept))) {
            return hs->staff[i].id;
        }
    }
    return 0;
}

void hosp_remote_monitor_init(RemoteMonitor *rm, int patient_id) {
    if (!rm) return;
    memset(rm, 0, sizeof(RemoteMonitor));
    rm->patient_id = patient_id;
    rm->last_reading = time(NULL);
}

bool hosp_remote_check_alerts(RemoteMonitor *rm) {
    if (!rm) return false;
    rm->alert_triggered = false;
    if (rm->spo2 <= 90) {
        rm->alert_triggered = true;
        snprintf(rm->alert_reason, sizeof(rm->alert_reason),
                 "SpO2 critical: %d%%", rm->spo2);
    }
    if (rm->heart_rate >= 130 || rm->heart_rate <= 45) {
        rm->alert_triggered = true;
        snprintf(rm->alert_reason, sizeof(rm->alert_reason),
                 "Heart rate abnormal: %d bpm", rm->heart_rate);
    }
    if (rm->systolic_bp >= 180 || rm->systolic_bp <= 85) {
        rm->alert_triggered = true;
        snprintf(rm->alert_reason, sizeof(rm->alert_reason),
                 "Blood pressure critical: %d mmHg", rm->systolic_bp);
    }
    if (rm->temperature >= 39.0 || rm->temperature <= 35.0) {
        rm->alert_triggered = true;
        snprintf(rm->alert_reason, sizeof(rm->alert_reason),
                 "Temperature abnormal: %.1f C", rm->temperature);
    }
    return rm->alert_triggered;
}

int hosp_remote_escalation_level(const RemoteMonitor *rm) {
    if (!rm || !rm->alert_triggered) return 0;
    if (rm->spo2 <= 85 || rm->heart_rate <= 40 || rm->systolic_bp <= 80)
        return 3;
    if (rm->spo2 <= 90 || rm->heart_rate >= 130 || rm->systolic_bp >= 180)
        return 2;
    return 1;
}
