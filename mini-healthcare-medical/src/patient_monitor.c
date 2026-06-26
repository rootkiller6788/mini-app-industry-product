#include "patient_monitor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ================================================================
 * patient_monitor.c -- Patient Vital Signs Monitoring System
 *
 * L4: NEWS2 (National Early Warning Score 2) implementation
 * L5: Early warning score calculation, alarm escalation logic
 * L7: Clinical patient monitoring and shift reporting
 * ================================================================ */

/* ---- L1: System Management ---- */

MonitorSystem* monitor_create(void) {
    return (MonitorSystem*)calloc(1, sizeof(MonitorSystem));
}

void monitor_destroy(MonitorSystem *ms) {
    if (ms) free(ms);
}

int monitor_add_patient(MonitorSystem *ms, const char *id, const char *name,
                         int age, bool copd) {
    if (!ms || !id || ms->patient_count >= PM_MAX_PATIENTS) return -1;
    PatientVitals *pv = &ms->patients[ms->patient_count];
    strncpy(pv->patient_id, id, PM_MAX_ID_LEN - 1);
    if (name) strncpy(pv->name, name, PM_MAX_NAME_LEN - 1);
    pv->age_years = age;
    pv->has_copd = copd;
    pv->on_oxygen = false;
    return ms->patient_count++;
}

PatientVitals* monitor_find_patient(MonitorSystem *ms, const char *id) {
    if (!ms || !id) return NULL;
    for (int i = 0; i < ms->patient_count; i++)
        if (strcmp(ms->patients[i].patient_id, id) == 0)
            return &ms->patients[i];
    return NULL;
}

/* ---- L4: Vital Signs Recording ---- */

static const char* vitals_name(VitalsType t) {
    switch (t) {
        case PM_HR:   return "Heart Rate";
        case PM_SBP:  return "Systolic BP";
        case PM_DBP:  return "Diastolic BP";
        case PM_RR:   return "Respiratory Rate";
        case PM_SPO2: return "SpO2";
        case PM_TEMP: return "Temperature";
        case PM_AVPU: return "AVPU";
        case PM_O2_FLOW: return "O2 Flow";
        default: return "?";
    }
}

int monitor_record_vital(MonitorSystem *ms, const char *patient_id, VitalsType type,
                          double value) {
    if (!ms || !patient_id) return -1;
    PatientVitals *pv = monitor_find_patient(ms, patient_id);
    if (!pv) return -1;
    if (!monitor_validate_vital(type, value)) return -1;

    /* Shift history */
    if (pv->current_count >= PM_COUNT) {
        for (int h = PM_MAX_HISTORY - 1; h > 0; h--)
            memcpy(pv->history[h], pv->history[h - 1], sizeof(pv->history[0]));
        memcpy(pv->history[0], pv->current, sizeof(pv->current));
    }

    pv->current[type].type = type;
    pv->current[type].value = value;
    pv->current[type].timestamp = time(NULL);
    pv->current_count++;

    /* Track oxygen status */
    if (type == PM_O2_FLOW && value > 0) pv->on_oxygen = true;
    if (type == PM_O2_FLOW && value == 0) pv->on_oxygen = false;

    /* Auto-calculate NEWS2 */
    pv->last_news = news2_calculate(pv);

    /* Check for alarms */
    if (pv->last_news.risk_level >= NEWS_MEDIUM_RISK) {
        PatientAlarm alarm = monitor_check_alarms(ms, patient_id);
        if (alarm.level >= ALARM_MEDIUM && ms->alarm_count < PM_MAX_ALARMS)
            ms->alarms[ms->alarm_count++] = alarm;
    }

    return 0;
}

bool monitor_validate_vital(VitalsType type, double value) {
    if (value < 0) return false;
    switch (type) {
        case PM_HR:   return value >= 0 && value <= 350;
        case PM_SBP:  return value >= 0 && value <= 300;
        case PM_DBP:  return value >= 0 && value <= 200;
        case PM_RR:   return value >= 0 && value <= 80;
        case PM_SPO2: return value >= 0 && value <= 100;
        case PM_TEMP: return value >= 25.0 && value <= 45.0;
        case PM_AVPU: return value >= 0 && value <= 3;
        case PM_O2_FLOW: return value >= 0 && value <= 60;
        default: return false;
    }
}

/* ---- L5: NEWS2 Scoring Algorithm ---- */
/* Reference: Royal College of Physicians, National Early Warning Score
 * (NEWS) 2, December 2017.
 */

int news2_score_respiratory_rate(double rr) {
    if (rr <= 8)   return 3;
    if (rr <= 11)  return 1;
    if (rr <= 20)  return 0;
    if (rr <= 24)  return 2;
    return 3;
}

int news2_score_spo2(double spo2, bool on_o2) {
    if (on_o2) {
        if (spo2 <= 83) return 3;
        if (spo2 <= 85) return 2;
        if (spo2 <= 87) return 1;
        if (spo2 <= 92) return 0;  /* On O2, SpO2 88-92 = target range score 0 */
        if (spo2 <= 93) return 1;
        if (spo2 <= 95) return 2;
        return 3;
    } else {
        if (spo2 <= 91) return 3;
        if (spo2 <= 93) return 2;
        if (spo2 <= 95) return 1;
        return 0;
    }
}

int news2_score_systolic_bp(double sbp) {
    if (sbp <= 90)  return 3;
    if (sbp <= 100) return 2;
    if (sbp <= 110) return 1;
    if (sbp <= 219) return 0;
    return 3;
}

int news2_score_heart_rate(double hr) {
    if (hr <= 40)   return 3;
    if (hr <= 50)   return 1;
    if (hr <= 90)   return 0;
    if (hr <= 110)  return 1;
    if (hr <= 130)  return 2;
    return 3;
}

int news2_score_temperature(double temp) {
    if (temp <= 35.0) return 3;
    if (temp <= 36.0) return 1;
    if (temp <= 38.0) return 0;
    if (temp <= 39.0) return 1;
    return 2;
}

int news2_score_avpu(int avpu_level) {
    switch (avpu_level) {
        case 0: return 0;  /* Alert */
        case 1: return 0;  /* Voice */
        case 2: return 0;  /* Pain */
        case 3: return 3;  /* Unresponsive */
        default: return 0;
    }
}

NewsRisk news2_risk_level(int total_score) {
    if (total_score >= 7) return NEWS_HIGH_RISK;
    if (total_score >= 5) return NEWS_MEDIUM_RISK;
    return NEWS_LOW_RISK;
}

NewsScore news2_calculate(const PatientVitals *pv) {
    NewsScore ns;
    memset(&ns, 0, sizeof(ns));

    if (!pv) return ns;

    ns.news2_score = 0;
    for (int i = 0; i < PM_COUNT; i++) {
        VitalsType vt = (VitalsType)i;
        if (vt == PM_DBP || vt == PM_O2_FLOW) continue;

        double val = pv->current[vt].value;
        bool has_value = false;
        for (int j = 0; j < pv->current_count; j++) {
            if (pv->current[j].type == vt) { has_value = true; break; }
        }
        if (!has_value) continue;

        ns.readings[ns.reading_count++] = pv->current[vt];

        switch (vt) {
            case PM_RR:   ns.news2_score += news2_score_respiratory_rate(val); break;
            case PM_SPO2: ns.news2_score += news2_score_spo2(val, pv->on_oxygen); break;
            case PM_SBP:  ns.news2_score += news2_score_systolic_bp(val); break;
            case PM_HR:   ns.news2_score += news2_score_heart_rate(val); break;
            case PM_TEMP: ns.news2_score += news2_score_temperature(val); break;
            case PM_AVPU: ns.news2_score += news2_score_avpu((int)val); break;
            default: break;
        }
    }

    ns.risk_level = news2_risk_level(ns.news2_score);
    ns.calculated_at = time(NULL);
    return ns;
}

/* ---- L5: Alarm Generation ---- */

PatientAlarm monitor_check_alarms(MonitorSystem *ms, const char *patient_id) {
    PatientAlarm alarm;
    memset(&alarm, 0, sizeof(alarm));
    if (!ms || !patient_id) return alarm;

    PatientVitals *pv = monitor_find_patient(ms, patient_id);
    if (!pv) return alarm;

    strncpy(alarm.patient_id, patient_id, PM_MAX_ID_LEN - 1);
    alarm.triggered_at = time(NULL);

    NewsScore ns = pv->last_news;
    if (ns.news2_score >= 7) {
        alarm.level = ALARM_CRITICAL;
        snprintf(alarm.message, sizeof(alarm.message),
                 "NEWS2 score %d (HIGH RISK). Emergency assessment required.",
                 ns.news2_score);
    } else if (ns.news2_score >= 5) {
        alarm.level = ALARM_HIGH;
        snprintf(alarm.message, sizeof(alarm.message),
                 "NEWS2 score %d (MEDIUM RISK). Urgent review within 30 minutes.",
                 ns.news2_score);
    } else if (ns.news2_score >= 3) {
        alarm.level = ALARM_MEDIUM;
        snprintf(alarm.message, sizeof(alarm.message),
                 "NEWS2 score %d. Clinical review recommended.",
                 ns.news2_score);
    }

    /* Check for single-parameter score of 3 */
    for (int i = 0; i < PM_COUNT; i++) {
        VitalsType vt = (VitalsType)i;
        if (vt == PM_DBP || vt == PM_O2_FLOW) continue;
        int single_score = 0;
        double val = pv->current[vt].value;
        switch (vt) {
            case PM_RR:   single_score = news2_score_respiratory_rate(val); break;
            case PM_SPO2: single_score = news2_score_spo2(val, pv->on_oxygen); break;
            case PM_SBP:  single_score = news2_score_systolic_bp(val); break;
            case PM_HR:   single_score = news2_score_heart_rate(val); break;
            case PM_TEMP: single_score = news2_score_temperature(val); break;
            case PM_AVPU: single_score = news2_score_avpu((int)val); break;
            default: break;
        }
        if (single_score == 3 && alarm.level < ALARM_MEDIUM) {
            alarm.level = ALARM_MEDIUM;
            alarm.vitals_type = vt;
            snprintf(alarm.message, sizeof(alarm.message),
                     "Single parameter score 3: %s = %.1f. Clinical review required.",
                     vitals_name(vt), val);
        }
    }

    return alarm;
}

int monitor_acknowledge_alarm(MonitorSystem *ms, int alarm_index) {
    if (!ms || alarm_index < 0 || alarm_index >= ms->alarm_count) return -1;
    ms->alarms[alarm_index].acknowledged = true;
    ms->alarms[alarm_index].acknowledged_at = time(NULL);
    return 0;
}

int monitor_pending_alarms(const MonitorSystem *ms, PatientAlarm *out, int max_out) {
    if (!ms || !out) return 0;
    int cnt = 0;
    for (int i = 0; i < ms->alarm_count && cnt < max_out; i++)
        if (!ms->alarms[i].acknowledged)
            out[cnt++] = ms->alarms[i];
    return cnt;
}

const char* alarm_level_name(AlarmLevel lvl) {
    switch (lvl) {
        case ALARM_NONE:     return "None";
        case ALARM_LOW:      return "Low";
        case ALARM_MEDIUM:   return "Medium";
        case ALARM_HIGH:     return "High";
        case ALARM_CRITICAL: return "CRITICAL";
        default: return "?";
    }
}

/* ---- L7: Clinical Workflow ---- */

int monitor_shift_report(const MonitorSystem *ms, char *buf, int buf_sz) {
    if (!ms || !buf) return -1;
    int off = snprintf(buf, buf_sz, "=== Shift Report ===\nPatients monitored: %d\n\n", ms->patient_count);
    for (int i = 0; i < ms->patient_count && off < buf_sz; i++) {
        const PatientVitals *pv = &ms->patients[i];
        off += snprintf(buf + off, buf_sz - off, "Patient: %s (%s)  Age: %d  NEWS2: %d [%s]\n",
                        pv->name[0] ? pv->name : pv->patient_id, pv->patient_id,
                        pv->age_years, pv->last_news.news2_score,
                        pv->last_news.risk_level == NEWS_HIGH_RISK ? "HIGH" :
                        pv->last_news.risk_level == NEWS_MEDIUM_RISK ? "MEDIUM" : "LOW");
        for (int j = 0; j < PM_COUNT; j++) {
            if (j == PM_DBP || j == PM_O2_FLOW) continue;
            bool has_val = false;
            for (int k = 0; k < pv->current_count; k++)
                if (pv->current[k].type == (VitalsType)j) { has_val = true; break; }
            if (has_val) {
                off += snprintf(buf + off, buf_sz - off, "  %-20s %8.1f\n",
                                vitals_name((VitalsType)j), pv->current[j].value);
            }
        }
    }
    return off;
}

int monitor_escalation_protocol(const PatientVitals *pv, char *buf, int buf_sz) {
    if (!pv || !buf) return -1;
    int off = 0;
    NewsScore ns = pv->last_news;
    switch (ns.risk_level) {
        case NEWS_LOW_RISK:
            off += snprintf(buf + off, buf_sz - off,
                "Protocol: Continue 4-6 hourly observations. ");
            off += snprintf(buf + off, buf_sz - off,
                "Notify nurse-in-charge if score increases.");
            break;
        case NEWS_MEDIUM_RISK:
            off += snprintf(buf + off, buf_sz - off,
                "Protocol: Increase monitoring to hourly. ");
            off += snprintf(buf + off, buf_sz - off,
                "Notify covering physician. Consider escalation to ICU liaison.");
            break;
        case NEWS_HIGH_RISK:
            off += snprintf(buf + off, buf_sz - off,
                "Protocol: CONTINUOUS MONITORING. ");
            off += snprintf(buf + off, buf_sz - off,
                "Immediate assessment by critical care team. ");
            off += snprintf(buf + off, buf_sz - off,
                "Consider transfer to ICU/HDU. Inform attending NOW.");
            break;
    }
    return off;
}

bool monitor_needs_rapid_response(const NewsScore *ns) {
    return ns && (ns->news2_score >= 7 || ns->risk_level == NEWS_HIGH_RISK);
}
