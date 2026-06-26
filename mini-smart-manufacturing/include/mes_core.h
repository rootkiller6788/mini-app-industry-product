#ifndef MINI_SMF_MES_CORE_H
#define MINI_SMF_MES_CORE_H

#include "smart_manufacturing.h"

/* ================================================================
 *  mes_core.h 鈥?Manufacturing Execution System (MES/SCADA)
 *
 *  L2: Core concepts 鈥?MES 11 functions (MESA-11), SCADA
 *      supervisory control, Andon system, Poka-Yoke
 *  L3: Engineering structures 鈥?ISA-95 Level 3 (Manufacturing
 *      Operations), ISA-95 Level 2 (Control), OPC-UA data model
 *  L4: Standards 鈥?ISA-95 (IEC 62264) MES model, MESA-11
 *      functional model, PackML (ISA-TR88.00.02)
 *  L5: Algorithms 鈥?Real-time data collection, Andon escalation,
 *      Production dashboard aggregation, KPI computation
 *  L6: Canonical problems 鈥?Shop floor tracking, real-time
 *      production monitoring, genealogy/traceability
 *  L7: Applications 鈥?Digital twin synchronization, operator
 *      dashboard with real-time OEE
 *  L8: Advanced 鈥?OPC-UA server integration, IIoT edge gateway,
 *      Unified namespace (MQTT Sparkplug B)
 *  L9: Industry 鈥?Siemens Opcenter, GE Proficy, Rockwell FTPS,
 *      Aveva MES, SAP ME
 *
 *  References:
 *  - ISA-95 (IEC 62264): Enterprise-Control System Integration
 *  - MESA International, "MESA-11 Model" (1997)
 *  - OPC Foundation, "OPC UA Specification Part 1" (IEC 62541)
 *  - ISA-TR88.00.02, "PackML: Machine State Model"
 * ================================================================ */

#define SMF_MES_MAX_TAGS     16384
#define SMF_MES_MAX_SHIFTS     128
#define SMF_MES_MAX_ANDONS     256
#define SMF_MES_TAG_NAME_LEN    64
#define SMF_MES_TRACE_MAX      512

/* ---- L1: MES Data Tag (real-time variable) ---- */

typedef enum {
    SMF_TAG_TEMPERATURE   = 0,
    SMF_TAG_PRESSURE      = 1,
    SMF_TAG_FLOW_RATE     = 2,
    SMF_TAG_SPEED_RPM     = 3,
    SMF_TAG_VIBRATION     = 4,
    SMF_TAG_HUMIDITY      = 5,
    SMF_TAG_VOLTAGE       = 6,
    SMF_TAG_CURRENT       = 7,
    SMF_TAG_POWER         = 8,
    SMF_TAG_ENERGY_KWH    = 9,
    SMF_TAG_PART_COUNT    = 10,
    SMF_TAG_CYCLE_TIME    = 11,
    SMF_TAG_POSITION_MM   = 12,
    SMF_TAG_TORQUE        = 13,
    SMF_TAG_PH_LEVEL      = 14,
    SMF_TAG_WEIGHT        = 15,
    SMF_TAG_DIGITAL_IN    = 16,
    SMF_TAG_DIGITAL_OUT   = 17,
    SMF_TAG_ALARM         = 18,
    SMF_TAG_STATUS        = 19,
    SMF_TAG_CUSTOM        = 20
} SMF_TagType;

typedef struct {
    char        tag_name[SMF_MES_TAG_NAME_LEN];
    SMF_TagType type;
    int         machine_idx;
    float       value;
    float       min_value;
    float       max_value;
    float       avg_value;
    float       std_dev;
    time_t      last_update;
    int         update_count;
    bool        is_alarmed;
    float       alarm_high;
    float       alarm_low;
    float       alarm_hihi;     /* high-high alarm */
    float       alarm_lolo;     /* low-low alarm */
    char        unit[16];       /* e.g., "掳C", "bar", "RPM" */
    int         scan_rate_ms;   /* polling interval */
} SMF_MESTag;

/* ---- L1: Production Shift ---- */

typedef struct {
    int         shift_id;
    char        name[32];       /* "Day", "Swing", "Night" */
    int         shift_number;   /* 1, 2, 3 */
    time_t      start_time;     /* HH:MM (relative to day) */
    time_t      end_time;
    float       hours;
    int         operator_count;
    int         supervisor_id;
    bool        is_active;
    bool        is_weekend;
} SMF_ProductionShift;

/* ---- L1: Shift Production Report ---- */

typedef struct {
    int         shift_id;
    time_t      date;
    int         product_idx;
    int         planned_quantity;
    int         actual_quantity;
    int         good_quantity;
    int         scrap_quantity;
    float       planned_downtime_min;
    float       unplanned_downtime_min;
    float       setup_time_min;
    float       oee;
    float       utilization;
    int         num_incidents;
    char        notes[SMF_DESC_LEN];
    bool        is_verified;
} SMF_ShiftReport;

/* ---- L1: Andon System ---- */

typedef enum {
    SMF_ANDON_GREEN      = 0,  /* normal operation */
    SMF_ANDON_YELLOW     = 1,  /* warning / material call */
    SMF_ANDON_RED        = 2,  /* line stop / quality issue */
    SMF_ANDON_FLASHING   = 3,  /* emergency */
    SMF_ANDON_OFFLINE    = 4
} SMF_AndonState;

typedef struct {
    int             andon_id;
    int             workstation_idx;
    SMF_AndonState  state;
    time_t          state_changed;
    time_t          acknowledged_at;
    char            reason[SMF_DESC_LEN];
    int             escalation_level;  /* 0=not escalated, 1=team lead, 2=supervisor, 3=manager */
    int             acknowledged_by;
    float           response_time_min;  /* time from state change to ack */
    float           resolution_time_min;  /* time from state change to resolved */
    bool            is_resolved;
} SMF_Andon;

/* ---- L1: Product Genealogy / Traceability ---- */

typedef struct {
    char    serial_number[SMF_CODE_LEN];
    int     product_idx;
    int     work_order_idx;
    time_t  production_date;
    int     step_history[SMF_MES_TRACE_MAX];  /* routing steps completed */
    int     machine_history[SMF_MES_TRACE_MAX];
    time_t  timestamp_history[SMF_MES_TRACE_MAX];
    int     num_steps;
    char    batch_id[SMF_CODE_LEN];
    int     lot_number;
    int     material_lot_ids[SMF_MES_TRACE_MAX];  /* material traceability */
    int     num_material_lots;
    bool    passed_quality;
    int     final_inspection_id;
} SMF_ProductGenealogy;

/* ---- L1: Poka-Yoke (Error Proofing) Rule ---- */

typedef enum {
    SMF_POKA_CONTACT     = 0,  /* physical contact method */
    SMF_POKA_FIXED_VALUE = 1,  /* fixed-value method */
    SMF_POKA_MOTION_STEP = 2,  /* motion-step method */
    SMF_POKA_SENSOR      = 3   /* sensor-based detection */
} SMF_PokaYokeMethod;

typedef struct {
    int     poka_id;
    int     operation_idx;
    int     machine_idx;
    SMF_PokaYokeMethod method;
    char    rule_description[SMF_DESC_LEN];
    char    check_condition[SMF_DESC_LEN];
    int     violation_count;
    int     prevent_count;
    bool    is_active;
    bool    is_blocking;  /* true = stops machine, false = warns only */
} SMF_PokaYoke;

/* ---- L1: Andon Escalation Matrix ---- */

typedef struct {
    int     level;           /* 0-4 */
    float   max_response_time_min;
    char    role[64];        /* who gets alerted */
    char    notification_method[32]; /* "light", "buzzer", "sms", "app" */
    bool    requires_ack;
    bool    triggers_line_stop;
} SMF_AndonEscalation;

/* ================================================================
 *  API: MES Data Acquisition
 * ================================================================ */

/** Register a new data tag */
int  smf_mes_tag_register(SMF_Factory *factory, int machine_idx,
                           const char *tag_name, SMF_TagType type,
                           const char *unit, int scan_rate_ms);

/** Update tag value (from PLC/SCADA) */
int  smf_mes_tag_update(SMF_Factory *factory, const char *tag_name,
                         float value);

/** Get current tag value */
float smf_mes_tag_value(const SMF_Factory *factory, const char *tag_name);

/** Get tag statistics (min, max, avg, std) */
int  smf_mes_tag_stats(const SMF_Factory *factory, const char *tag_name,
                        float *min_val, float *max_val, float *avg_val,
                        float *std_dev);

/** Check if tag is in alarm state */
bool smf_mes_tag_in_alarm(const SMF_Factory *factory, const char *tag_name);

/** Query all tags for a machine */
int  smf_mes_tags_for_machine(const SMF_Factory *factory, int machine_idx,
                               const char **tag_names, int max_results);

/* ================================================================
 *  API: Shift Management
 * ================================================================ */

int  smf_shift_create(SMF_ProductionShift *shift, int number,
                       const char *name, int start_hour, int end_hour);

/** Generate a shift production report */
int  smf_shift_report_generate(const SMF_Factory *factory, int shift_id,
                                time_t date, SMF_ShiftReport *report);

/** Calculate shift efficiency */
float smf_shift_efficiency(const SMF_ShiftReport *report);

/** Aggregate multiple shift reports into daily summary */
int  smf_shift_daily_summary(const SMF_ShiftReport *shifts, int num_shifts,
                              float *total_good, float *total_scrap,
                              float *avg_oee);

/* ================================================================
 *  API: Andon System (L5: Escalation Algorithm)
 * ================================================================ */

/** Create an Andon for a workstation */
int  smf_andon_create(SMF_Factory *factory, int workstation_idx);

/** Trigger an Andon state change */
int  smf_andon_trigger(SMF_Factory *factory, int andon_id,
                        SMF_AndonState new_state, const char *reason);

/** Acknowledge an Andon alert */
int  smf_andon_acknowledge(SMF_Factory *factory, int andon_id,
                            int operator_id);

/** Resolve an Andon event */
int  smf_andon_resolve(SMF_Factory *factory, int andon_id);

/** Escalate Andon to next level */
int  smf_andon_escalate(SMF_Factory *factory, int andon_id);

/** Get Andon response statistics for a line */
int  smf_andon_stats(const SMF_Factory *factory, int line_idx,
                      float *avg_response, float *avg_resolution,
                      int *total_andons);

/** Check if escalation is needed based on response time matrix */
int  smf_andon_check_escalation(const SMF_Factory *factory, int andon_id,
                                 const SMF_AndonEscalation *matrix,
                                 int matrix_size);

/* ================================================================
 *  API: Traceability (L6: Product Genealogy)
 * ================================================================ */

/** Create product genealogy record */
int  smf_genealogy_create(SMF_Factory *factory, const char *serial,
                           int product_idx, int wo_idx);

/** Record a production step */
int  smf_genealogy_record_step(SMF_Factory *factory, const char *serial,
                                int step_no, int machine_idx);

/** Record material lot used */
int  smf_genealogy_add_material_lot(SMF_Factory *factory,
                                     const char *serial, int material_lot_id);

/** Query product genealogy by serial number */
int  smf_genealogy_query(const SMF_Factory *factory, const char *serial,
                          SMF_ProductGenealogy *result);

/** Forward trace: find all products using material lot X */
int  smf_genealogy_forward_trace(const SMF_Factory *factory,
                                  int material_lot_id,
                                  char serials[][SMF_CODE_LEN], int max_results);

/** Backward trace: find all material lots used in product serial Y */
int  smf_genealogy_backward_trace(const SMF_Factory *factory,
                                   const char *serial,
                                   int *material_lot_ids, int max_results);

/* ================================================================
 *  API: Poka-Yoke
 * ================================================================ */

int  smf_pokayoke_create(SMF_Factory *factory, int operation_idx,
                          int machine_idx, SMF_PokaYokeMethod method,
                          const char *rule, const char *condition,
                          bool is_blocking);

/** Check if poka-yoke condition is satisfied */
bool smf_pokayoke_check(const SMF_PokaYoke *poka, float measurement,
                         float nominal, float tolerance);

/** Record poka-yoke activation (prevented or detected error) */
int  smf_pokayoke_record(SMF_Factory *factory, int poka_id, bool prevented);

/* ================================================================
 *  API: Production Dashboard KPI Calculation
 * ================================================================ */

/** Calculate production attainment: actual / planned * 100 */
float smf_kpi_production_attainment(const SMF_ShiftReport *report);

/** Calculate first-pass yield: (good - rework) / total * 100 */
float smf_kpi_first_pass_yield(int good_qty, int rework_qty, int total_qty);

/** Calculate schedule adherence: on-time orders / total orders */
float smf_kpi_schedule_adherence(int on_time, int total);

/* Utility */
const char* smf_tag_type_name(SMF_TagType type);
const char* smf_andon_state_name(SMF_AndonState state);
const char* smf_pokayoke_method_name(SMF_PokaYokeMethod method);

#endif