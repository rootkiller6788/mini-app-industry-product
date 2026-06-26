/* oee_monitor.c - OEE, TPM & Predictive Maintenance
 * L2: Overall Equipment Effectiveness (OEE = A*P*Q), TPM pillars
 * L4: ISO 22400 OEE KPIs, ISO 55000 Asset Management
 * L5: Weibull reliability analysis, FMEA RPN scoring
 * L6: OEE dashboard, PM scheduling, Six Big Losses analysis
 */
#include "oee_monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define M_EPS 0.001f
#define M_PI_F 3.14159265358979323846f

/* ---- Utility ---- */

const char* smf_event_type_name(SMF_EventType type) {
    switch (type) {
        case SMF_EVENT_STARTUP:       return "Startup";
        case SMF_EVENT_SHUTDOWN:      return "Shutdown";
        case SMF_EVENT_BREAKDOWN:     return "Breakdown";
        case SMF_EVENT_REPAIR_DONE:   return "Repair Complete";
        case SMF_EVENT_SETUP_START:   return "Setup Start";
        case SMF_EVENT_SETUP_END:     return "Setup End";
        case SMF_EVENT_SPEED_LOSS:    return "Speed Loss";
        case SMF_EVENT_IDLE_MINOR:    return "Idle/Minor Stop";
        case SMF_EVENT_QUALITY_ISSUE: return "Quality Issue";
        case SMF_EVENT_SCHED_MAINT:   return "Scheduled Maintenance";
        case SMF_EVENT_UNSCHED_MAINT: return "Unscheduled Maintenance";
        case SMF_EVENT_CHANGEOVER:    return "Changeover";
        case SMF_EVENT_MATERIAL_WAIT: return "Material Wait";
        case SMF_EVENT_POWER_OUTAGE:  return "Power Outage";
        case SMF_EVENT_SAFETY_STOP:   return "Safety Stop";
        default:                      return "Unknown";
    }
}

const char* smf_maint_type_name(SMF_MaintenanceType type) {
    switch (type) {
        case SMF_MAINT_PREVENTIVE:      return "Preventive";
        case SMF_MAINT_PREDICTIVE:      return "Predictive";
        case SMF_MAINT_CORRECTIVE:      return "Corrective";
        case SMF_MAINT_CONDITION_BASED: return "Condition-Based";
        case SMF_MAINT_OVERHAUL:        return "Overhaul";
        case SMF_MAINT_CALIBRATION:     return "Calibration";
        default:                        return "Unknown";
    }
}

const char* smf_loss_category_name(int category) {
    switch (category) {
        case SMF_LOSS_BREAKDOWN:        return "Breakdown Losses";
        case SMF_LOSS_SETUP_ADJUST:     return "Setup & Adjustment";
        case SMF_LOSS_IDLE_MINOR:       return "Idling & Minor Stoppages";
        case SMF_LOSS_REDUCED_SPEED:    return "Reduced Speed";
        case SMF_LOSS_STARTUP_DEFECT:   return "Startup Defects";
        case SMF_LOSS_PRODUCTION_DEFECT: return "Production Defects";
        default:                        return "Unknown";
    }
}

/* ================================================================
 *  OEE Calculation (ISO 22400)
 *
 *  OEE = Availability * Performance * Quality
 *
 *  A = Operating Time / Planned Production Time
 *  P = (Ideal Cycle Time * Total Pieces) / Operating Time
 *  Q = Good Pieces / Total Pieces
 *
 *  World-class: A>=90%, P>=95%, Q>=99.9%, OEE>=85%
 * ================================================================ */

int smf_oee_calculate(SMF_OEEMetrics *oee, float planned_time,
                       float operating_time, float ideal_cycle,
                       int total_pieces, int good_pieces) {
    if (!oee) return -1;
    memset(oee, 0, sizeof(SMF_OEEMetrics));

    oee->planned_prod_time = planned_time;
    oee->operating_time = operating_time;
    oee->downtime = planned_time - operating_time;
    if (oee->downtime < 0.0f) oee->downtime = 0.0f;

    oee->ideal_cycle_time = ideal_cycle;
    oee->total_pieces = (float)total_pieces;
    oee->good_pieces = (float)good_pieces;
    oee->reject_pieces = (float)(total_pieces - good_pieces);
    if (oee->reject_pieces < 0.0f) oee->reject_pieces = 0.0f;

    /* Availability */
    if (planned_time > 0.0f) {
        oee->availability = operating_time / planned_time;
    }

    /* Performance */
    if (operating_time > 0.0f) {
        float theoretical_output = operating_time / ideal_cycle;
        oee->performance = (theoretical_output > 0.0f)
            ? (float)total_pieces / theoretical_output : 0.0f;
    }

    /* Quality */
    if (total_pieces > 0) {
        oee->quality = (float)good_pieces / (float)total_pieces;
    }

    oee->oee = oee->availability * oee->performance * oee->quality;

    /* World-class targets */
    oee->availability_target = 0.90f;
    oee->performance_target = 0.95f;
    oee->quality_target = 0.999f;
    oee->oee_target = 0.85f;

    oee->period_start = time(NULL);
    oee->period_end = oee->period_start;

    return 0;
}

int smf_oee_calculate_from_events(SMF_OEEMetrics *oee,
                                   const SMF_MachineEvent *events,
                                   int num_events, int machine_idx,
                                   time_t start, time_t end) {
    if (!oee || !events) return -1;
    memset(oee, 0, sizeof(SMF_OEEMetrics));

    float planned = (float)difftime(end, start) / 3600.0f;
    float downtime = 0.0f;
    int total_pcs = 0, good_pcs = 0;

    for (int i = 0; i < num_events; i++) {
        if (events[i].machine_idx != machine_idx) continue;
        if (events[i].timestamp < start || events[i].timestamp > end) continue;

        switch (events[i].type) {
            case SMF_EVENT_BREAKDOWN:
            case SMF_EVENT_SETUP_START:
            case SMF_EVENT_UNSCHED_MAINT:
            case SMF_EVENT_POWER_OUTAGE:
                downtime += events[i].duration_minutes / 60.0f;
                break;
            case SMF_EVENT_QUALITY_ISSUE:
                /* Quality event: assume some rejected parts */
                break;
            default:
                break;
        }
    }

    float operating = planned - downtime;
    if (operating < 0.0f) operating = 0.0f;

    return smf_oee_calculate(oee, planned, operating, 0.0167f,
                              total_pcs, good_pcs);
}

int smf_oee_benchmark(const SMF_OEEMetrics *oee,
                       bool *avail_ok, bool *perf_ok, bool *qual_ok) {
    if (!oee) return -1;
    if (avail_ok) *avail_ok = oee->availability >= oee->availability_target;
    if (perf_ok) *perf_ok = oee->performance >= oee->performance_target;
    if (qual_ok) *qual_ok = oee->quality >= oee->quality_target;
    return 0;
}

float smf_oee_gap(const SMF_OEEMetrics *oee) {
    if (!oee) return 0.0f;
    float gap = oee->oee_target - oee->oee;
    return gap > 0.0f ? gap : 0.0f;
}

/* ================================================================
 *  Machine Event Logging
 * ================================================================ */

static SMF_MachineEvent _events[16384];
static int _event_count = 0;

int smf_event_log(SMF_Factory *factory, int machine_idx,
                   SMF_EventType type, float duration_min,
                   const char *description, int severity) {
    if (!factory || _event_count >= 16384) return -1;

    int id = _event_count++;
    SMF_MachineEvent *evt = &_events[id];
    memset(evt, 0, sizeof(SMF_MachineEvent));
    evt->event_id = id;
    evt->machine_idx = machine_idx;
    evt->type = type;
    evt->timestamp = time(NULL);
    evt->duration_minutes = duration_min;
    if (description) strncpy(evt->description, description, SMF_DESC_LEN - 1);
    evt->severity = severity;
    evt->is_resolved = false;

    return id;
}

int smf_event_query(const SMF_Factory *factory, int machine_idx,
                     time_t start, time_t end,
                     SMF_MachineEvent *results, int max_results) {
    (void)factory;
    int count = 0;
    for (int i = _event_count - 1; i >= 0 && count < max_results; i--) {
        if (_events[i].machine_idx != machine_idx) continue;
        if (_events[i].timestamp < start || _events[i].timestamp > end) continue;
        results[count++] = _events[i];
    }
    return count;
}

float smf_event_total_downtime(const SMF_MachineEvent *events,
                                int num_events, time_t start, time_t end) {
    float total = 0.0f;
    for (int i = 0; i < num_events; i++) {
        if (events[i].timestamp < start || events[i].timestamp > end) continue;
        switch (events[i].type) {
            case SMF_EVENT_BREAKDOWN:
            case SMF_EVENT_UNSCHED_MAINT:
            case SMF_EVENT_POWER_OUTAGE:
                total += events[i].duration_minutes;
                break;
            default:
                break;
        }
    }
    return total;
}

/* ================================================================
 *  Six Big Losses Analysis (TPM - Nakajima 1988)
 *
 *  1. Breakdown losses
 *  2. Setup and adjustment losses
 *  3. Idling and minor stoppages
 *  4. Reduced speed losses
 *  5. Quality defects and rework (startup)
 *  6. Quality defects and rework (production)
 * ================================================================ */

int smf_loss_classify(const SMF_MachineEvent *events, int num_events,
                       int machine_idx, time_t start, time_t end,
                       SMF_LossAnalysis *analysis) {
    if (!events || !analysis) return -1;
    memset(analysis, 0, sizeof(SMF_LossAnalysis));
    analysis->period_start = start;
    analysis->period_end = end;

    for (int i = 0; i < num_events; i++) {
        if (events[i].machine_idx != machine_idx) continue;
        if (events[i].timestamp < start || events[i].timestamp > end) continue;

        float hours = events[i].duration_minutes / 60.0f;
        switch (events[i].type) {
            case SMF_EVENT_BREAKDOWN:
                analysis->loss_hours[SMF_LOSS_BREAKDOWN] += hours;
                break;
            case SMF_EVENT_SETUP_START:
            case SMF_EVENT_CHANGEOVER:
                analysis->loss_hours[SMF_LOSS_SETUP_ADJUST] += hours;
                break;
            case SMF_EVENT_IDLE_MINOR:
            case SMF_EVENT_MATERIAL_WAIT:
                analysis->loss_hours[SMF_LOSS_IDLE_MINOR] += hours;
                break;
            case SMF_EVENT_SPEED_LOSS:
                analysis->loss_hours[SMF_LOSS_REDUCED_SPEED] += hours;
                break;
            case SMF_EVENT_QUALITY_ISSUE:
                analysis->loss_hours[SMF_LOSS_PRODUCTION_DEFECT] += hours;
                break;
            default:
                break;
        }
    }

    /* Compute totals and percentages */
    for (int i = 0; i < 6; i++) {
        analysis->total_loss_hours += analysis->loss_hours[i];
    }

    if (analysis->total_loss_hours > 0.0f) {
        analysis->breakdown_pct = analysis->loss_hours[0] / analysis->total_loss_hours * 100.0f;
        analysis->setup_pct = analysis->loss_hours[1] / analysis->total_loss_hours * 100.0f;
        analysis->speed_pct = analysis->loss_hours[3] / analysis->total_loss_hours * 100.0f;
        analysis->defect_pct = (analysis->loss_hours[4] + analysis->loss_hours[5]) / analysis->total_loss_hours * 100.0f;
    }

    return 0;
}

int smf_loss_top_category(const SMF_LossAnalysis *analysis) {
    if (!analysis) return -1;
    int top = 0;
    for (int i = 1; i < 6; i++) {
        if (analysis->loss_hours[i] > analysis->loss_hours[top]) {
            top = i;
        }
    }
    return top;
}

/* ================================================================
 *  Weibull Reliability Analysis
 *
 *  Weibull CDF: F(t) = 1 - exp(-(t/eta)^beta)
 *  MTBF = eta * Gamma(1 + 1/beta)
 *
 *  Parameter estimation via Rank Regression (Median Rank):
 *  F_i = (i - 0.3) / (N + 0.4)   [Benard's approximation]
 *  Linear regression on ln(ln(1/(1-F))) vs ln(t)
 * ================================================================ */

int smf_weibull_fit(const float *time_to_failure, int num_failures,
                     float *shape, float *scale) {
    if (!time_to_failure || num_failures < 3) return -1;
    if (!shape || !scale) return -1;

    /* Sort failure times */
    float *sorted = (float*)malloc(num_failures * sizeof(float));
    if (!sorted) return -1;
    memcpy(sorted, time_to_failure, num_failures * sizeof(float));

    for (int i = 0; i < num_failures - 1; i++) {
        for (int j = i + 1; j < num_failures; j++) {
            if (sorted[i] > sorted[j]) {
                float t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t;
            }
        }
    }

    /* Compute rank regression */
    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_xx = 0.0f;
    float n_float = (float)num_failures;

    for (int i = 0; i < num_failures; i++) {
        float rank = ((float)(i + 1) - 0.3f) / (n_float + 0.4f);
        if (rank >= 1.0f) rank = 0.999f;
        if (rank <= 0.0f) rank = 0.001f;

        float x = logf(sorted[i]);
        float y = logf(-logf(1.0f - rank));

        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }

    float beta = (n_float * sum_xy - sum_x * sum_y)
                 / (n_float * sum_xx - sum_x * sum_x);
    float alpha = (sum_y - beta * sum_x) / n_float;

    *shape = beta;
    *scale = expf(-alpha / beta);

    free(sorted);
    return 0;
}

float smf_weibull_mtbf(float shape, float scale) {
    if (shape <= 0.0f || scale <= 0.0f) return 0.0f;

    /* Gamma function approximation using Stirling:
     * Gamma(1 + 1/beta) ~ sqrt(2*pi) * x^(x-0.5) * exp(-x)
     * where x = 1 + 1/beta */
    float x = 1.0f + 1.0f / shape;
    float gamma = sqrtf(2.0f * M_PI_F) * powf(x, (x - 0.5f)) * expf(-x);

    return scale * gamma;
}

float smf_weibull_reliability(float shape, float scale, float t) {
    if (shape <= 0.0f || scale <= 0.0f || t < 0.0f) return 1.0f;
    return expf(-powf(t / scale, shape));
}

/* ================================================================
 *  FMEA - Failure Mode and Effects Analysis
 *
 *  RPN = Severity * Occurrence * Detection
 *
 *  S: 1=no effect, 10=hazardous without warning
 *  O: 1=almost never, 10=almost inevitable
 *  D: 1=certain detection, 10=no detection possible
 *
 *  RPN > 100: action required
 *  RPN 50-100: action recommended
 *  RPN < 50: acceptable risk
 * ================================================================ */

int smf_fmea_rpn(int severity, int occurrence, int detection) {
    return severity * occurrence * detection;
}

int smf_fmea_evaluate(SMF_FMEARecord *fmea) {
    if (!fmea) return -1;
    fmea->rpn = smf_fmea_rpn(fmea->severity, fmea->occurrence, fmea->detection);
    return fmea->rpn > 100 ? 1 : (fmea->rpn > 50 ? 0 : -1);
}

int smf_fmea_sort_by_rpn(SMF_FMEARecord *records, int num_records) {
    if (!records || num_records <= 0) return -1;

    /* Update RPNs, then sort descending */
    for (int i = 0; i < num_records; i++) {
        records[i].rpn = smf_fmea_rpn(records[i].severity,
                                        records[i].occurrence,
                                        records[i].detection);
    }

    for (int i = 0; i < num_records - 1; i++) {
        for (int j = i + 1; j < num_records; j++) {
            if (records[i].rpn < records[j].rpn) {
                SMF_FMEARecord tmp = records[i];
                records[i] = records[j];
                records[j] = tmp;
            }
        }
    }
    return 0;
}

/* ================================================================
 *  Maintenance Management
 * ================================================================ */

static SMF_MaintenanceJob _maint_jobs[4096];
static int _maint_count = 0;

int smf_maint_schedule(SMF_Factory *factory, int machine_idx,
                        SMF_MaintenanceType type, time_t scheduled_date,
                        float estimated_hours, int interval_hours) {
    if (!factory || _maint_count >= 4096) return -1;

    int id = _maint_count++;
    SMF_MaintenanceJob *mj = &_maint_jobs[id];
    memset(mj, 0, sizeof(SMF_MaintenanceJob));
    mj->maint_id = id;
    mj->machine_idx = machine_idx;
    mj->type = type;
    mj->scheduled_date = scheduled_date;
    mj->estimated_hours = estimated_hours;
    mj->interval_hours = interval_hours;
    mj->is_completed = false;
    mj->is_overdue = false;
    return id;
}

int smf_maint_complete(SMF_Factory *factory, int maint_id,
                        float actual_hours, float cost) {
    if (!factory || maint_id < 0 || maint_id >= _maint_count) return -1;
    SMF_MaintenanceJob *mj = &_maint_jobs[maint_id];
    mj->completed_date = time(NULL);
    mj->actual_hours = actual_hours;
    mj->cost = cost;
    mj->is_completed = true;
    mj->is_overdue = false;
    return 0;
}

int smf_maint_overdue(const SMF_Factory *factory,
                       int *maint_indices, int max_results) {
    if (!factory || !maint_indices || max_results <= 0) return 0;
    time_t now = time(NULL);
    int count = 0;
    for (int i = 0; i < _maint_count && count < max_results; i++) {
        if (!_maint_jobs[i].is_completed
            && _maint_jobs[i].scheduled_date < now) {
            maint_indices[count++] = i;
        }
    }
    return count;
}

float smf_maint_pm_compliance(const SMF_Factory *factory,
                               time_t from, time_t to) {
    if (!factory) return 0.0f;
    int scheduled = 0, completed = 0;
    for (int i = 0; i < _maint_count; i++) {
        if (_maint_jobs[i].scheduled_date < from
            || _maint_jobs[i].scheduled_date > to) continue;
        if (_maint_jobs[i].type == SMF_MAINT_PREVENTIVE) {
            scheduled++;
            if (_maint_jobs[i].is_completed) completed++;
        }
    }
    return scheduled > 0 ? (float)completed / (float)scheduled * 100.0f : 100.0f;
}