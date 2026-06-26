#ifndef MINI_SMF_OEE_MONITOR_H
#define MINI_SMF_OEE_MONITOR_H

#include "smart_manufacturing.h"

/* ================================================================
 *  oee_monitor.h — OEE, TPM & Predictive Maintenance
 *
 *  L2: Core concepts — OEE (Overall Equipment Effectiveness),
 *      TPM (Total Productive Maintenance), Nakajima's 8 pillars,
 *      JIPM certification
 *  L3: Engineering structures — OEE calculation pipeline, MTBF/
 *      MTTR tracking, Maintenance scheduling engine
 *  L4: Standards — ISO 22400 OEE KPIs, ISO 55000 Asset Management,
 *      ISO 13374 Condition Monitoring
 *  L5: Algorithms — OEE calculation (A*P*Q), Weibull reliability
 *      analysis, Failure Mode Effects Analysis (FMEA), RPN scoring
 *  L6: Canonical problems — OEE monitoring dashboard, preventive
 *      maintenance schedule optimization
 *  L7: Applications — Real-time OEE visualization, CMMS integration
 *  L8: Advanced — Predictive maintenance (ML-based), Digital twin
 *      for asset performance management, vibration analysis
 *  L9: Industry — PTC ThingWorx, Siemens MindSphere APM, ABB Ability
 *
 *  References:
 *  - Nakajima, "Introduction to TPM" (1988)
 *  - ISO 22400-2: OEE definition and calculation
 *  - ISO 55000:2014 "Asset Management"
 *  - Weibull, "A Statistical Distribution Function of Wide Applicability" (1951)
 * ================================================================ */

#define SMF_OEE_MAX_EVENTS  131072
#define SMF_OEE_MAX_ALERTS    1024

/* ---- L1: OEE Metrics ---- */

typedef struct {
    float   availability;      /* A = Operating Time / Planned Production Time */
    float   performance;       /* P = (Ideal Cycle Time * Total Pieces) / Operating Time */
    float   quality;           /* Q = Good Pieces / Total Pieces */
    float   oee;               /* OEE = A * P * Q */
    float   planned_prod_time; /* scheduled time (hours) */
    float   operating_time;    /* actual running time (hours) */
    float   downtime;          /* unplanned + planned downtime (hours) */
    float   ideal_cycle_time;  /* theoretical min per piece (hours) */
    float   total_pieces;
    float   good_pieces;
    float   reject_pieces;
    float   availability_target;  /* world-class: 90% */
    float   performance_target;   /* world-class: 95% */
    float   quality_target;       /* world-class: 99.9% */
    float   oee_target;           /* world-class: 85% */
    int     period;               /* measurement period */
    time_t  period_start;
    time_t  period_end;
} SMF_OEEMetrics;

/* ---- L1: Machine Event Log ---- */

typedef enum {
    SMF_EVENT_STARTUP      = 0,
    SMF_EVENT_SHUTDOWN     = 1,
    SMF_EVENT_BREAKDOWN    = 2,
    SMF_EVENT_REPAIR_DONE  = 3,
    SMF_EVENT_SETUP_START  = 4,
    SMF_EVENT_SETUP_END    = 5,
    SMF_EVENT_SPEED_LOSS   = 6,
    SMF_EVENT_IDLE_MINOR   = 7,
    SMF_EVENT_QUALITY_ISSUE = 8,
    SMF_EVENT_SCHED_MAINT  = 9,
    SMF_EVENT_UNSCHED_MAINT = 10,
    SMF_EVENT_CHANGEOVER   = 11,
    SMF_EVENT_MATERIAL_WAIT = 12,
    SMF_EVENT_POWER_OUTAGE = 13,
    SMF_EVENT_SAFETY_STOP  = 14
} SMF_EventType;

typedef struct {
    int         event_id;
    int         machine_idx;
    SMF_EventType type;
    time_t      timestamp;
    float       duration_minutes;
    char        description[SMF_DESC_LEN];
    int         severity;       /* 1=minor, 2=moderate, 3=major, 4=critical */
    char        reported_by[32];
    int         work_order_idx; /* -1 if unrelated */
    bool        is_resolved;
    float       cost_impact;    /* estimated cost of downtime */
} SMF_MachineEvent;

/* ---- L1: Six Big Losses (TPM classification) ---- */

typedef enum {
    SMF_LOSS_BREAKDOWN      = 0,  /* equipment failure */
    SMF_LOSS_SETUP_ADJUST   = 1,  /* setup and adjustment */
    SMF_LOSS_IDLE_MINOR     = 2,  /* idling and minor stoppages */
    SMF_LOSS_REDUCED_SPEED  = 3,  /* reduced speed operation */
    SMF_LOSS_STARTUP_DEFECT = 4,  /* process defects (startup) */
    SMF_LOSS_PRODUCTION_DEFECT = 5  /* production defects */
} SMF_SixBigLoss;

typedef struct {
    float   loss_hours[6];      /* one per loss category */
    float   total_loss_hours;
    float   breakdown_pct;
    float   setup_pct;
    float   speed_pct;
    float   defect_pct;
    int     period;
    time_t  period_start;
    time_t  period_end;
} SMF_LossAnalysis;

/* ---- L1: Maintenance Schedule ---- */

typedef enum {
    SMF_MAINT_PREVENTIVE      = 0,
    SMF_MAINT_PREDICTIVE      = 1,
    SMF_MAINT_CORRECTIVE      = 2,
    SMF_MAINT_CONDITION_BASED = 3,
    SMF_MAINT_OVERHAUL        = 4,
    SMF_MAINT_CALIBRATION     = 5
} SMF_MaintenanceType;

typedef struct {
    int         maint_id;
    int         machine_idx;
    SMF_MaintenanceType type;
    time_t      scheduled_date;
    time_t      completed_date; /* 0 if not yet completed */
    float       estimated_hours;
    float       actual_hours;
    int         interval_hours;  /* recurrence interval */
    char        checklist[SMF_DESC_LEN];
    char        technician[32];
    int         priority;        /* 0=highest */
    bool        is_completed;
    bool        is_overdue;
    float       cost;
} SMF_MaintenanceJob;

/* ---- L1: FMEA Record ---- */

typedef struct {
    int     fmea_id;
    int     machine_idx;
    char    failure_mode[SMF_DESC_LEN];
    char    effect[SMF_DESC_LEN];
    char    cause[SMF_DESC_LEN];
    int     severity;       /* S: 1-10 */
    int     occurrence;     /* O: 1-10 */
    int     detection;      /* D: 1-10 */
    int     rpn;            /* RPN = S * O * D */
    char    recommended_action[SMF_DESC_LEN];
    bool    action_taken;
    int     rpn_after;      /* RPN after corrective action */
} SMF_FMEARecord;

/* ================================================================
 *  API: OEE Calculation (L4: ISO 22400)
 *
 *  OEE = Availability × Performance × Quality
 *
 *  Availability = Operating Time / Planned Production Time
 *  Performance  = (Ideal Cycle Time × Total Pieces) / Operating Time
 *  Quality      = Good Pieces / Total Pieces
 *
 *  World-class benchmarks (Nakajima 1988):
 *    Availability ≥ 90%, Performance ≥ 95%, Quality ≥ 99.9% → OEE ≥ 85%
 * ================================================================ */

/** Calculate OEE for a machine from raw data */
int  smf_oee_calculate(SMF_OEEMetrics *oee, float planned_time,
                        float operating_time, float ideal_cycle,
                        int total_pieces, int good_pieces);

/** Calculate OEE over a period using event log */
int  smf_oee_calculate_from_events(SMF_OEEMetrics *oee,
                                    const SMF_MachineEvent *events,
                                    int num_events, int machine_idx,
                                    time_t start, time_t end);

/** Evaluate OEE against world-class benchmarks */
int  smf_oee_benchmark(const SMF_OEEMetrics *oee,
                        bool *avail_ok, bool *perf_ok, bool *qual_ok);

/** OEE world-class gap analysis */
float smf_oee_gap(const SMF_OEEMetrics *oee);

/* ================================================================
 *  API: Machine Event Logging
 * ================================================================ */

int  smf_event_log(SMF_Factory *factory, int machine_idx,
                    SMF_EventType type, float duration_min,
                    const char *description, int severity);

/** Query events for a machine within a time window */
int  smf_event_query(const SMF_Factory *factory, int machine_idx,
                      time_t start, time_t end,
                      SMF_MachineEvent *results, int max_results);

/** Calculate total downtime for a machine */
float smf_event_total_downtime(const SMF_MachineEvent *events,
                                int num_events, time_t start, time_t end);

/* ================================================================
 *  API: Six Big Losses Analysis (TPM)
 * ================================================================ */

/** Classify events into the 6 Big Losses categories */
int  smf_loss_classify(const SMF_MachineEvent *events, int num_events,
                        int machine_idx, time_t start, time_t end,
                        SMF_LossAnalysis *analysis);

/** Identify the top loss category */
int  smf_loss_top_category(const SMF_LossAnalysis *analysis);

const char* smf_loss_category_name(int category);

/* ================================================================
 *  API: Reliability Engineering (L5: Weibull Analysis)
 *
 *  Weibull CDF: F(t) = 1 - exp(-(t/η)^β)
 *  where η = scale parameter (characteristic life)
 *        β = shape parameter (β<1: infant mortality, β=1: random,
 *            β>1: wear-out, β=3.5: normal-like)
 *
 *  MTBF = η * Γ(1 + 1/β)
 * ================================================================ */

/**
 * Estimate Weibull parameters from time-to-failure data
 * using Rank Regression (Median Rank / Bernard's approximation).
 *
 * F_i = (i - 0.3) / (N + 0.4)  — Benard's median rank
 *
 * Returns: shape (beta > 0), scale (eta > 0)
 */
int  smf_weibull_fit(const float *time_to_failure, int num_failures,
                      float *shape, float *scale);

/** Compute MTBF from Weibull parameters */
float smf_weibull_mtbf(float shape, float scale);

/** Compute reliability at time t: R(t) = exp(-(t/eta)^beta) */
float smf_weibull_reliability(float shape, float scale, float t);

/* ================================================================
 *  API: FMEA (Failure Mode and Effects Analysis)
 * ================================================================ */

/** Compute Risk Priority Number: RPN = Severity × Occurrence × Detection */
int  smf_fmea_rpn(int severity, int occurrence, int detection);

/** Evaluate FMEA record and suggest priority action */
int  smf_fmea_evaluate(SMF_FMEARecord *fmea);

/** Sort FMEA records by RPN (descending) */
int  smf_fmea_sort_by_rpn(SMF_FMEARecord *records, int num_records);

/* ================================================================
 *  API: Maintenance Management
 * ================================================================ */

/** Schedule a maintenance job */
int  smf_maint_schedule(SMF_Factory *factory, int machine_idx,
                         SMF_MaintenanceType type, time_t scheduled_date,
                         float estimated_hours, int interval_hours);

/** Complete a maintenance job */
int  smf_maint_complete(SMF_Factory *factory, int maint_id,
                         float actual_hours, float cost);

/** Find overdue maintenance jobs */
int  smf_maint_overdue(const SMF_Factory *factory,
                        int *maint_indices, int max_results);

/** PM compliance rate */
float smf_maint_pm_compliance(const SMF_Factory *factory, time_t from, time_t to);

/* Utility */
const char* smf_event_type_name(SMF_EventType type);
const char* smf_maint_type_name(SMF_MaintenanceType type);

#endif