#ifndef HOSPITAL_OPS_H
#define HOSPITAL_OPS_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define HOSP_MAX_WARDS 64
#define HOSP_MAX_BEDS 1024
#define HOSP_MAX_PATIENTS 2048
#define HOSP_MAX_APPOINTMENTS 4096
#define HOSP_MAX_STAFF 512
#define HOSP_MAX_NAME 128

typedef enum {
    HOSP_TRIAGE_RESUSCITATION,
    HOSP_TRIAGE_EMERGENT,
    HOSP_TRIAGE_URGENT,
    HOSP_TRIAGE_SEMI_URGENT,
    HOSP_TRIAGE_NON_URGENT
} HospTriageLevel;

typedef enum {
    HOSP_BED_AVAILABLE,
    HOSP_BED_OCCUPIED,
    HOSP_BED_RESERVED,
    HOSP_BED_CLEANING,
    HOSP_BED_MAINTENANCE,
    HOSP_BED_ISOLATION
} HospBedStatus;

typedef enum {
    HOSP_WARD_GENERAL,
    HOSP_WARD_ICU,
    HOSP_WARD_EMERGENCY,
    HOSP_WARD_PEDIATRIC,
    HOSP_WARD_MATERNITY,
    HOSP_WARD_SURGICAL,
    HOSP_WARD_PSYCHIATRIC,
    HOSP_WARD_ISOLATION
} HospWardType;

typedef struct {
    int      id;
    char     name[HOSP_MAX_NAME];
    HospWardType type;
    int      total_beds;
    int      available_beds;
    int      nurse_station_phone;
    bool     has_isolation_capability;
    bool     active;
} HospitalWard;

typedef struct {
    int      id;
    int      ward_id;
    char     bed_label[16];
    HospBedStatus status;
    int      current_patient_id;
    time_t   occupied_since;
    time_t   cleaning_since;
    bool     bariatric;
    bool     telemetry_capable;
    bool     negative_pressure;
} HospitalBed;

typedef struct {
    int      id;
    int      patient_id;
    char     patient_name[HOSP_MAX_NAME];
    HospTriageLevel triage;
    time_t   arrival_time;
    time_t   seen_time;
    int      assigned_bed_id;
    bool     admitted;
    char     chief_complaint[256];
} AdmissionRecord;

typedef struct {
    int      id;
    int      patient_id;
    int      doctor_id;
    time_t   scheduled_time;
    time_t   duration_min;
    char     department[64];
    char     reason[256];
    bool     confirmed;
    bool     completed;
    bool     no_show;
} Appointment;

typedef struct {
    int      id;
    char     name[HOSP_MAX_NAME];
    char     role[64];
    char     license_id[32];
    char     department[64];
    bool     active;
    int      current_patient_load;
} StaffMember;

typedef struct {
    HospitalWard wards[HOSP_MAX_WARDS];
    int      ward_count;
    HospitalBed beds[HOSP_MAX_BEDS];
    int      bed_count;
    AdmissionRecord admissions[HOSP_MAX_PATIENTS];
    int      admission_count;
    Appointment appointments[HOSP_MAX_APPOINTMENTS];
    int      appointment_count;
    StaffMember staff[HOSP_MAX_STAFF];
    int      staff_count;
} HospitalSystem;

/* Core API */
HospitalSystem* hosp_system_create(void);
void            hosp_system_destroy(HospitalSystem *hs);
void hosp_system_init(HospitalSystem *hs);
int  hosp_add_ward(HospitalSystem *hs, const char *name, HospWardType type, int total_beds, bool isolation, int phone);
int  hosp_add_bed(HospitalSystem *hs, int ward_id, const char *label, bool bariatric, bool telemetry, bool neg_pressure);
HospitalBed* hosp_find_bed(HospitalSystem *hs, int bed_id);
bool hosp_occupy_bed(HospitalSystem *hs, int bed_id, int patient_id);
bool hosp_release_bed(HospitalSystem *hs, int bed_id);
bool hosp_set_bed_status(HospitalSystem *hs, int bed_id, HospBedStatus status);

/* L5: Triage system (Manchester Triage Scale) */
HospTriageLevel hosp_triage_assess(const char *chief_complaint, int age, int hr, int rr, int sbp, int spo2, double temp);
const char* hosp_triage_label(HospTriageLevel level);
int  hosp_triage_wait_time_minutes(HospTriageLevel level);

/* L6: Bed management ? find available bed */
HospitalBed* hosp_find_available_bed(HospitalSystem *hs, HospWardType preferred_type, bool needs_isolation, bool needs_telemetry);
int  hosp_available_beds_count(const HospitalSystem *hs, HospWardType type);
int  hosp_ward_occupancy_pct(const HospitalSystem *hs, int ward_id);

/* L6: Admission-discharge-transfer (ADT) workflow */
int  hosp_admit_patient(HospitalSystem *hs, int patient_id, const char *name, const char *complaint, int age, int hr, int rr, int sbp, int spo2, double temp);
bool hosp_discharge_patient(HospitalSystem *hs, int admission_id);
bool hosp_transfer_patient(HospitalSystem *hs, int admission_id, int to_ward_id);
AdmissionRecord* hosp_find_admission(const HospitalSystem *hs, int admission_id);

/* L7: Appointment scheduling */
int  hosp_schedule_appointment(HospitalSystem *hs, int patient_id, int doctor_id, time_t when, int duration_min, const char *dept, const char *reason);
bool hosp_confirm_appointment(HospitalSystem *hs, int appt_id);
bool hosp_check_in_appointment(HospitalSystem *hs, int appt_id);
int  hosp_get_daily_appointments(const HospitalSystem *hs, time_t day_start, int *appt_ids, int max_count);

/* L8: Staff workload management */
int  hosp_add_staff(HospitalSystem *hs, const char *name, const char *role, const char *dept);
int  hosp_assign_patient_to_staff(HospitalSystem *hs, int staff_id, int patient_id);
double hosp_staff_utilization(const HospitalSystem *hs, int staff_id, int max_patients);
int  hosp_find_available_staff(const HospitalSystem *hs, const char *role, const char *dept);

/* L9: Hospital-at-Home / Remote monitoring (frontier) */
typedef struct {
    int      patient_id;
    time_t   last_reading;
    int      heart_rate;
    int      spo2;
    double   temperature;
    int      systolic_bp;
    bool     alert_triggered;
    char     alert_reason[256];
} RemoteMonitor;

void hosp_remote_monitor_init(RemoteMonitor *rm, int patient_id);
bool hosp_remote_check_alerts(RemoteMonitor *rm);
int  hosp_remote_escalation_level(const RemoteMonitor *rm);

#endif
