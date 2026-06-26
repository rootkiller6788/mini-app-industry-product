/* mes_core.c - Manufacturing Execution System (MES/SCADA)
 * L2: MESA-11 MES functions, Andon, Poka-Yoke
 * L4: ISA-95 (IEC 62264) Levels 2-3, OPC-UA data model
 * L5: Real-time data acquisition, Andon escalation algorithm
 * L6: Shop floor tracking, traceability/genealogy
 */
#include "mes_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ---- Utility ---- */

const char* smf_tag_type_name(SMF_TagType type) {
    switch (type) {
        case SMF_TAG_TEMPERATURE: return "Temperature";
        case SMF_TAG_PRESSURE:    return "Pressure";
        case SMF_TAG_FLOW_RATE:   return "Flow Rate";
        case SMF_TAG_SPEED_RPM:   return "Speed (RPM)";
        case SMF_TAG_VIBRATION:   return "Vibration";
        case SMF_TAG_HUMIDITY:    return "Humidity";
        case SMF_TAG_VOLTAGE:     return "Voltage";
        case SMF_TAG_CURRENT:     return "Current";
        case SMF_TAG_POWER:       return "Power";
        case SMF_TAG_ENERGY_KWH:  return "Energy (kWh)";
        case SMF_TAG_PART_COUNT:  return "Part Count";
        case SMF_TAG_CYCLE_TIME:  return "Cycle Time";
        case SMF_TAG_POSITION_MM: return "Position (mm)";
        case SMF_TAG_TORQUE:      return "Torque";
        case SMF_TAG_PH_LEVEL:    return "pH Level";
        case SMF_TAG_WEIGHT:      return "Weight";
        case SMF_TAG_DIGITAL_IN:  return "Digital Input";
        case SMF_TAG_DIGITAL_OUT: return "Digital Output";
        case SMF_TAG_ALARM:       return "Alarm";
        case SMF_TAG_STATUS:      return "Status";
        case SMF_TAG_CUSTOM:      return "Custom";
        default:                  return "Unknown";
    }
}

const char* smf_andon_state_name(SMF_AndonState state) {
    switch (state) {
        case SMF_ANDON_GREEN:    return "Green (Normal)";
        case SMF_ANDON_YELLOW:   return "Yellow (Warning)";
        case SMF_ANDON_RED:      return "Red (Stop)";
        case SMF_ANDON_FLASHING: return "Flashing (Emergency)";
        case SMF_ANDON_OFFLINE:  return "Offline";
        default:                 return "Unknown";
    }
}

const char* smf_pokayoke_method_name(SMF_PokaYokeMethod method) {
    switch (method) {
        case SMF_POKA_CONTACT:     return "Contact Method";
        case SMF_POKA_FIXED_VALUE: return "Fixed-Value Method";
        case SMF_POKA_MOTION_STEP: return "Motion-Step Method";
        case SMF_POKA_SENSOR:      return "Sensor-Based";
        default:                   return "Unknown";
    }
}

/* ================================================================
 *  MES Data Tags
 * ================================================================ */

static SMF_MESTag _tags[SMF_MES_MAX_TAGS];
static int _tag_count = 0;

int smf_mes_tag_register(SMF_Factory *factory, int machine_idx,
                          const char *tag_name, SMF_TagType type,
                          const char *unit, int scan_rate_ms) {
    if (!factory || !tag_name || _tag_count >= SMF_MES_MAX_TAGS) return -1;

    /* Check for duplicate */
    for (int i = 0; i < _tag_count; i++) {
        if (strcmp(_tags[i].tag_name, tag_name) == 0
            && _tags[i].machine_idx == machine_idx) {
            return i; /* Already registered */
        }
    }

    int idx = _tag_count++;
    SMF_MESTag *tag = &_tags[idx];
    memset(tag, 0, sizeof(SMF_MESTag));
    strncpy(tag->tag_name, tag_name, SMF_MES_TAG_NAME_LEN - 1);
    tag->type = type;
    tag->machine_idx = machine_idx;
    if (unit) strncpy(tag->unit, unit, sizeof(tag->unit) - 1);
    tag->scan_rate_ms = scan_rate_ms;
    tag->is_alarmed = false;
    tag->alarm_high = 1e6f;
    tag->alarm_low = -1e6f;
    tag->last_update = time(NULL);

    return idx;
}

int smf_mes_tag_update(SMF_Factory *factory, const char *tag_name,
                        float value) {
    if (!factory || !tag_name) return -1;

    for (int i = 0; i < _tag_count; i++) {
        if (strcmp(_tags[i].tag_name, tag_name) == 0) {
            SMF_MESTag *tag = &_tags[i];

            /* Update statistics */
            if (tag->update_count == 0) {
                tag->min_value = value;
                tag->max_value = value;
            } else {
                if (value < tag->min_value) tag->min_value = value;
                if (value > tag->max_value) tag->max_value = value;
            }

            /* Exponential moving average */
            float alpha = 0.1f;
            tag->avg_value = alpha * value + (1.0f - alpha) * tag->avg_value;

            tag->value = value;
            tag->last_update = time(NULL);
            tag->update_count++;

            /* Check alarms */
            tag->is_alarmed = (value > tag->alarm_high
                               || value < tag->alarm_low);

            return i;
        }
    }
    return -1;
}

float smf_mes_tag_value(const SMF_Factory *factory, const char *tag_name) {
    if (!factory || !tag_name) return 0.0f;
    for (int i = 0; i < _tag_count; i++) {
        if (strcmp(_tags[i].tag_name, tag_name) == 0) {
            return _tags[i].value;
        }
    }
    return 0.0f;
}

int smf_mes_tag_stats(const SMF_Factory *factory, const char *tag_name,
                       float *min_val, float *max_val, float *avg_val,
                       float *std_dev) {
    if (!factory || !tag_name) return -1;
    for (int i = 0; i < _tag_count; i++) {
        if (strcmp(_tags[i].tag_name, tag_name) == 0) {
            if (min_val) *min_val = _tags[i].min_value;
            if (max_val) *max_val = _tags[i].max_value;
            if (avg_val) *avg_val = _tags[i].avg_value;
            if (std_dev) *std_dev = _tags[i].std_dev;
            return i;
        }
    }
    return -1;
}

bool smf_mes_tag_in_alarm(const SMF_Factory *factory, const char *tag_name) {
    if (!factory || !tag_name) return false;
    for (int i = 0; i < _tag_count; i++) {
        if (strcmp(_tags[i].tag_name, tag_name) == 0) {
            return _tags[i].is_alarmed;
        }
    }
    return false;
}

int smf_mes_tags_for_machine(const SMF_Factory *factory, int machine_idx,
                              const char **tag_names, int max_results) {
    if (!factory || !tag_names) return 0;
    int count = 0;
    for (int i = 0; i < _tag_count && count < max_results; i++) {
        if (_tags[i].machine_idx == machine_idx) {
            tag_names[count++] = _tags[i].tag_name;
        }
    }
    return count;
}

/* ================================================================
 *  Shift Management
 * ================================================================ */

int smf_shift_create(SMF_ProductionShift *shift, int number,
                      const char *name, int start_hour, int end_hour) {
    if (!shift) return -1;
    memset(shift, 0, sizeof(SMF_ProductionShift));
    shift->shift_id = number;
    shift->shift_number = number;
    if (name) strncpy(shift->name, name, sizeof(shift->name) - 1);

    /* Convert hours to time_t (relative timestamps for today) */
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_s(&tm_info, &now);
    tm_info.tm_hour = start_hour;
    tm_info.tm_min = 0;
    tm_info.tm_sec = 0;
    shift->start_time = mktime(&tm_info);
    tm_info.tm_hour = end_hour;
    shift->end_time = mktime(&tm_info);

    shift->hours = (float)(end_hour - start_hour);
    shift->is_active = true;
    return 0;
}

int smf_shift_report_generate(const SMF_Factory *factory, int shift_id,
                               time_t date, SMF_ShiftReport *report) {
    if (!factory || !report) return -1;
    memset(report, 0, sizeof(SMF_ShiftReport));
    report->shift_id = shift_id;
    report->date = date;
    report->planned_quantity = 100;
    report->actual_quantity = 0;
    report->good_quantity = 0;
    report->scrap_quantity = 0;
    report->is_verified = false;
    return 0;
}

float smf_shift_efficiency(const SMF_ShiftReport *report) {
    if (!report || report->planned_quantity <= 0) return 0.0f;
    return (float)report->actual_quantity
           / (float)report->planned_quantity * 100.0f;
}

int smf_shift_daily_summary(const SMF_ShiftReport *shifts, int num_shifts,
                             float *total_good, float *total_scrap,
                             float *avg_oee) {
    if (!shifts || num_shifts <= 0) return -1;
    float good = 0.0f, scrap = 0.0f, oee = 0.0f;
    for (int i = 0; i < num_shifts; i++) {
        good += (float)shifts[i].good_quantity;
        scrap += (float)shifts[i].scrap_quantity;
        oee += shifts[i].oee;
    }
    if (total_good) *total_good = good;
    if (total_scrap) *total_scrap = scrap;
    if (avg_oee) *avg_oee = oee / (float)num_shifts;
    return 0;
}

/* ================================================================
 *  Andon System
 *
 *  Toyota Production System: Andon (lamp) provides visual status.
 *  Pulling the andon cord stops the line so problems are fixed
 *  immediately rather than passed downstream.
 * ================================================================ */

static SMF_Andon _andons[SMF_MES_MAX_ANDONS];
static int _andon_count = 0;

int smf_andon_create(SMF_Factory *factory, int workstation_idx) {
    if (!factory || _andon_count >= SMF_MES_MAX_ANDONS) return -1;
    int id = _andon_count++;
    SMF_Andon *a = &_andons[id];
    memset(a, 0, sizeof(SMF_Andon));
    a->andon_id = id;
    a->workstation_idx = workstation_idx;
    a->state = SMF_ANDON_GREEN;
    a->state_changed = time(NULL);
    a->escalation_level = 0;
    a->is_resolved = true;
    return id;
}

int smf_andon_trigger(SMF_Factory *factory, int andon_id,
                       SMF_AndonState new_state, const char *reason) {
    if (!factory || andon_id < 0 || andon_id >= _andon_count) return -1;
    SMF_Andon *a = &_andons[andon_id];
    a->state = new_state;
    a->state_changed = time(NULL);
    a->acknowledged_at = 0;
    if (reason) strncpy(a->reason, reason, SMF_DESC_LEN - 1);
    a->is_resolved = false;
    a->escalation_level = 0;
    return 0;
}

int smf_andon_acknowledge(SMF_Factory *factory, int andon_id,
                           int operator_id) {
    if (!factory || andon_id < 0 || andon_id >= _andon_count) return -1;
    SMF_Andon *a = &_andons[andon_id];
    a->acknowledged_at = time(NULL);
    a->acknowledged_by = operator_id;
    a->response_time_min = (float)difftime(a->acknowledged_at,
                                            a->state_changed) / 60.0f;
    return 0;
}

int smf_andon_resolve(SMF_Factory *factory, int andon_id) {
    if (!factory || andon_id < 0 || andon_id >= _andon_count) return -1;
    SMF_Andon *a = &_andons[andon_id];
    a->state = SMF_ANDON_GREEN;
    a->is_resolved = true;
    a->resolution_time_min = (float)difftime(time(NULL),
                                              a->state_changed) / 60.0f;
    return 0;
}

int smf_andon_escalate(SMF_Factory *factory, int andon_id) {
    if (!factory || andon_id < 0 || andon_id >= _andon_count) return -1;
    SMF_Andon *a = &_andons[andon_id];
    if (a->escalation_level < 3) {
        a->escalation_level++;
        /* Higher escalation = flashing */
        if (a->escalation_level >= 3) a->state = SMF_ANDON_FLASHING;
        else if (a->escalation_level >= 2) a->state = SMF_ANDON_RED;
    }
    return 0;
}

int smf_andon_stats(const SMF_Factory *factory, int line_idx,
                     float *avg_response, float *avg_resolution,
                     int *total_andons) {
    if (!factory) return -1;
    float sum_resp = 0.0f, sum_reso = 0.0f;
    int count = 0;
    for (int i = 0; i < _andon_count; i++) {
        if (_andons[i].response_time_min > 0.0f) {
            sum_resp += _andons[i].response_time_min;
            sum_reso += _andons[i].resolution_time_min;
            count++;
        }
    }
    if (avg_response) *avg_response = count > 0 ? sum_resp / count : 0.0f;
    if (avg_resolution) *avg_resolution = count > 0 ? sum_reso / count : 0.0f;
    (void)line_idx;
    if (total_andons) *total_andons = count;
    return 0;
}

int smf_andon_check_escalation(const SMF_Factory *factory, int andon_id,
                                const SMF_AndonEscalation *matrix,
                                int matrix_size) {
    if (!factory || andon_id < 0 || andon_id >= _andon_count) return -1;
    if (!matrix || matrix_size <= 0) return 0;

    SMF_Andon *a = &_andons[andon_id];
    if (a->is_resolved) return 0;
    if (a->acknowledged_at > 0) return 0;

    float elapsed = (float)difftime(time(NULL), a->state_changed) / 60.0f;
    int current = a->escalation_level;

    if (current < matrix_size - 1) {
        if (elapsed > matrix[current].max_response_time_min) {
            return current + 1; /* Should escalate */
        }
    }
    return -1; /* No escalation needed */
}

/* ================================================================
 *  Product Genealogy / Traceability (L6)
 *
 *  Track each product unit from raw material to finished good,
 *  including all process steps, machines used, and operators.
 *  Enables forward trace (material lot -> affected products) and
 *  backward trace (product -> all material lots used).
 * ================================================================ */

static SMF_ProductGenealogy _genealogies[SMF_MES_TRACE_MAX];
static int _genealogy_count = 0;

int smf_genealogy_create(SMF_Factory *factory, const char *serial,
                          int product_idx, int wo_idx) {
    if (!factory || !serial || _genealogy_count >= SMF_MES_TRACE_MAX)
        return -1;
    int id = _genealogy_count++;
    SMF_ProductGenealogy *g = &_genealogies[id];
    memset(g, 0, sizeof(SMF_ProductGenealogy));
    strncpy(g->serial_number, serial, SMF_CODE_LEN - 1);
    g->product_idx = product_idx;
    g->work_order_idx = wo_idx;
    g->production_date = time(NULL);
    g->num_steps = 0;
    g->num_material_lots = 0;
    g->passed_quality = false;
    return id;
}

int smf_genealogy_record_step(SMF_Factory *factory, const char *serial,
                               int step_no, int machine_idx) {
    if (!factory || !serial) return -1;
    for (int i = 0; i < _genealogy_count; i++) {
        if (strcmp(_genealogies[i].serial_number, serial) == 0) {
            SMF_ProductGenealogy *g = &_genealogies[i];
            if (g->num_steps >= SMF_MES_TRACE_MAX) return -2;
            int s = g->num_steps++;
            g->step_history[s] = step_no;
            g->machine_history[s] = machine_idx;
            g->timestamp_history[s] = time(NULL);
            return 0;
        }
    }
    return -1;
}

int smf_genealogy_add_material_lot(SMF_Factory *factory,
                                    const char *serial, int material_lot_id) {
    if (!factory || !serial) return -1;
    for (int i = 0; i < _genealogy_count; i++) {
        if (strcmp(_genealogies[i].serial_number, serial) == 0) {
            SMF_ProductGenealogy *g = &_genealogies[i];
            if (g->num_material_lots >= SMF_MES_TRACE_MAX) return -2;
            g->material_lot_ids[g->num_material_lots++] = material_lot_id;
            return 0;
        }
    }
    return -1;
}

int smf_genealogy_query(const SMF_Factory *factory, const char *serial,
                         SMF_ProductGenealogy *result) {
    if (!factory || !serial || !result) return -1;
    for (int i = 0; i < _genealogy_count; i++) {
        if (strcmp(_genealogies[i].serial_number, serial) == 0) {
            *result = _genealogies[i];
            return 0;
        }
    }
    return -1;
}

int smf_genealogy_forward_trace(const SMF_Factory *factory,
                                 int material_lot_id,
                                 char serials[][SMF_CODE_LEN], int max_results) {
    if (!factory || !serials || max_results <= 0) return 0;
    int count = 0;
    for (int i = 0; i < _genealogy_count && count < max_results; i++) {
        for (int j = 0; j < _genealogies[i].num_material_lots; j++) {
            if (_genealogies[i].material_lot_ids[j] == material_lot_id) {
                strncpy(serials[count], _genealogies[i].serial_number,
                        SMF_CODE_LEN - 1);
                count++;
                break;
            }
        }
    }
    return count;
}

int smf_genealogy_backward_trace(const SMF_Factory *factory,
                                  const char *serial,
                                  int *material_lot_ids, int max_results) {
    if (!factory || !serial || !material_lot_ids || max_results <= 0) return 0;
    for (int i = 0; i < _genealogy_count; i++) {
        if (strcmp(_genealogies[i].serial_number, serial) == 0) {
            int n = _genealogies[i].num_material_lots < max_results
                    ? _genealogies[i].num_material_lots : max_results;
            for (int j = 0; j < n; j++) {
                material_lot_ids[j] = _genealogies[i].material_lot_ids[j];
            }
            return n;
        }
    }
    return 0;
}

/* ================================================================
 *  Poka-Yoke (Error Proofing)
 *
 *  Invented by Shigeo Shingo at Toyota: prevent defects by designing
 *  processes so that errors are impossible or immediately detected.
 * ================================================================ */

static SMF_PokaYoke _pokas[512];
static int _poka_count = 0;

int smf_pokayoke_create(SMF_Factory *factory, int operation_idx,
                         int machine_idx, SMF_PokaYokeMethod method,
                         const char *rule, const char *condition,
                         bool is_blocking) {
    if (!factory || _poka_count >= 512) return -1;
    int id = _poka_count++;
    SMF_PokaYoke *p = &_pokas[id];
    memset(p, 0, sizeof(SMF_PokaYoke));
    p->poka_id = id;
    p->operation_idx = operation_idx;
    p->machine_idx = machine_idx;
    p->method = method;
    if (rule) strncpy(p->rule_description, rule, SMF_DESC_LEN - 1);
    if (condition) strncpy(p->check_condition, condition, SMF_DESC_LEN - 1);
    p->is_active = true;
    p->is_blocking = is_blocking;
    return id;
}

bool smf_pokayoke_check(const SMF_PokaYoke *poka, float measurement,
                         float nominal, float tolerance) {
    if (!poka || !poka->is_active) return true;
    return fabsf(measurement - nominal) <= tolerance;
}

int smf_pokayoke_record(SMF_Factory *factory, int poka_id, bool prevented) {
    if (!factory || poka_id < 0 || poka_id >= _poka_count) return -1;
    if (prevented) _pokas[poka_id].prevent_count++;
    else _pokas[poka_id].violation_count++;
    return 0;
}

/* ================================================================
 *  Production Dashboard KPIs
 * ================================================================ */

float smf_kpi_production_attainment(const SMF_ShiftReport *report) {
    if (!report || report->planned_quantity <= 0) return 0.0f;
    return (float)report->actual_quantity
           / (float)report->planned_quantity * 100.0f;
}

float smf_kpi_first_pass_yield(int good_qty, int rework_qty, int total_qty) {
    if (total_qty <= 0) return 0.0f;
    return (float)(good_qty - rework_qty) / (float)total_qty * 100.0f;
}

float smf_kpi_schedule_adherence(int on_time, int total) {
    if (total <= 0) return 0.0f;
    return (float)on_time / (float)total * 100.0f;
}