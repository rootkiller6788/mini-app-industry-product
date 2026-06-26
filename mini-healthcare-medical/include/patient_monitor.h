#ifndef PATIENT_MONITOR_H
#define PATIENT_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/**
 * patient_monitor.h -- Patient Vital Signs Monitoring System
 *
 * L4 Standards: NEWS2 (National Early Warning Score 2, Royal College of Physicians 2017)
 *               HL7 FHIR Observation resource mapping
 * L5 Algorithms: Early Warning Score calculation, alarm escalation,
 *                vital signs trend detection, threshold alarming
 * L7 Application: ICU/ward patient monitoring, clinical alarm systems
 *
 * NEWS2 scoring:
 *   Score 0-4: Low risk (routine monitoring)
 *   Score 5-6: Medium risk (urgent review)
 *   Score ≥7:  High risk (emergency assessment, consider ICU)
 *   Score of 3 in any single parameter: clinical review
 *
 * Course mapping:
 *   - MIT HST.542J (Medical Device Design)
 *   - Stanford MED 277 (AI in Healthcare)
 *   - Johns Hopkins EN.585.762 (Medical Sensors & Systems)
 */

#define PM_MAX_PATIENTS      64
#define PM_MAX_ID_LEN        32
#define PM_MAX_NAME_LEN     128
#define PM_MAX_ALARMS       256
#define PM_MAX_HISTORY       64

/* NEWS2 parameter thresholds (adult) */
#define NEWS_RR_LOW          8    /* Respiratory rate bpm */
#define NEWS_RR_HIGH         25
#define NEWS_SPO2_LOW        92   /* SpO2 % on air */
#define NEWS_SPO2_LOW_O2     88   /* SpO2 % on oxygen */
#define NEWS_SBP_LOW         100  /* Systolic BP mmHg */
#define NEWS_SBP_HIGH        220
#define NEWS_HR_LOW          50   /* Heart rate bpm */
#define NEWS_HR_HIGH         130
#define NEWS_TEMP_LOW        35.0 /* Temperature Celsius */
#define NEWS_TEMP_HIGH       39.1
#define NEWS_AVPU_ALERT      0
#define NEWS_AVPU_VOICE      1
#define NEWS_AVPU_PAIN       2
#define NEWS_AVPU_UNRESP     3

typedef enum {
    PM_HR,          /* Heart Rate (bpm) */
    PM_SBP,         /* Systolic Blood Pressure (mmHg) */
    PM_DBP,         /* Diastolic Blood Pressure (mmHg) */
    PM_RR,          /* Respiratory Rate (bpm) */
    PM_SPO2,        /* Oxygen Saturation (%) */
    PM_TEMP,        /* Temperature (Celsius) */
    PM_AVPU,        /* Consciousness level */
    PM_O2_FLOW,     /* Oxygen flow rate (L/min) */
    PM_COUNT
} VitalsType;

typedef enum {
    ALARM_NONE,
    ALARM_LOW,
    ALARM_MEDIUM,
    ALARM_HIGH,
    ALARM_CRITICAL
} AlarmLevel;

typedef enum {
    NEWS_LOW_RISK,
    NEWS_MEDIUM_RISK,
    NEWS_HIGH_RISK
} NewsRisk;

typedef struct {
    VitalsType type;
    double value;
    time_t timestamp;
} VitalReading;

typedef struct {
    VitalReading readings[PM_COUNT];
    int reading_count;
    int news2_score;
    NewsRisk risk_level;
    time_t calculated_at;
} NewsScore;

typedef struct {
    char patient_id[PM_MAX_ID_LEN];
    char name[PM_MAX_NAME_LEN];
    VitalReading current[PM_COUNT];
    int current_count;
    VitalReading history[PM_MAX_HISTORY][PM_COUNT];
    int history_count;
    NewsScore last_news;
    bool on_oxygen;
    bool has_copd;          /* For SpO2 target adjustment */
    int age_years;
} PatientVitals;

typedef struct {
    char patient_id[PM_MAX_ID_LEN];
    AlarmLevel level;
    VitalsType vitals_type;
    char message[256];
    time_t triggered_at;
    time_t acknowledged_at;
    bool acknowledged;
} PatientAlarm;

typedef struct {
    PatientVitals patients[PM_MAX_PATIENTS];
    int patient_count;
    PatientAlarm alarms[PM_MAX_ALARMS];
    int alarm_count;
} MonitorSystem;

/* L1: System management */
MonitorSystem* monitor_create(void);
void           monitor_destroy(MonitorSystem *ms);
int            monitor_add_patient(MonitorSystem *ms, const char *id, const char *name,
                                    int age, bool copd);
PatientVitals* monitor_find_patient(MonitorSystem *ms, const char *id);

/* L4: Vital signs recording and validation */
int  monitor_record_vital(MonitorSystem *ms, const char *patient_id, VitalsType type,
                           double value);
bool monitor_validate_vital(VitalsType type, double value);

/* L5: NEWS2 scoring algorithm */
NewsScore  news2_calculate(const PatientVitals *pv);
int        news2_score_respiratory_rate(double rr);
int        news2_score_spo2(double spo2, bool on_o2);
int        news2_score_systolic_bp(double sbp);
int        news2_score_heart_rate(double hr);
int        news2_score_temperature(double temp);
int        news2_score_avpu(int avpu_level);
NewsRisk   news2_risk_level(int total_score);

/* L5: Alarm generation and escalation */
PatientAlarm monitor_check_alarms(MonitorSystem *ms, const char *patient_id);
int          monitor_acknowledge_alarm(MonitorSystem *ms, int alarm_index);
int          monitor_pending_alarms(const MonitorSystem *ms, PatientAlarm *out, int max_out);
const char*  alarm_level_name(AlarmLevel lvl);

/* L7: Clinical workflow */
int  monitor_shift_report(const MonitorSystem *ms, char *buf, int buf_sz);
int  monitor_escalation_protocol(const PatientVitals *pv, char *buf, int buf_sz);
bool monitor_needs_rapid_response(const NewsScore *ns);

#endif
