#ifndef MINI_SMF_QUALITY_CONTROL_H
#define MINI_SMF_QUALITY_CONTROL_H

#include "smart_manufacturing.h"

/* ================================================================
 *  quality_control.h — Statistical Process Control & Six Sigma
 *
 *  L2: Core concepts — Six Sigma (DMAIC/DMADV), SPC, process
 *      capability (Cp/Cpk), quality loss function (Taguchi)
 *  L3: Engineering structures — Control charts pipeline, sampling
 *      plan engine, defect classification tree
 *  L4: Standards — ISO 9001:2015 QMS, ISO 2859 (AQL sampling),
 *      ISO 3951 (variables sampling), ANSI/ASQ Z1.4
 *  L5: Algorithms — X-bar/R chart calculation, Control limit
 *      computation, Western Electric rules detection, Cp/Cpk/Pp/Ppk
 *  L6: Canonical problems — SPC chart generation, capability study,
 *      root cause analysis (Ishikawa/Fishbone)
 *  L7: Applications — Real-time SPC dashboard, Gage R&R analysis
 *  L8: Advanced — Multivariate SPC (Hotelling T²), EWMA/CUSUM charts
 *  L9: Industry — Minitab, JMP, Q-DAS, InfinityQS
 *
 *  References:
 *  - Shewhart, "Economic Control of Quality of Manufactured Product" (1931)
 *  - Montgomery, "Introduction to Statistical Quality Control" (2019)
 *  - Taguchi & Wu, "Introduction to Off-Line Quality Control" (1980)
 *  - ISO 9001:2015 "Quality Management Systems — Requirements"
 *  - Motorola, "Six Sigma Methodology" (1986)
 * ================================================================ */

#define SMF_SPC_MAX_SAMPLES      2048
#define SMF_SPC_MAX_SUBGROUPS     256
#define SMF_SPC_SAMPLE_SIZE        32
#define SMF_SPC_WECO_RULES         8

/* ---- L1: SPC Sample / Measurement ---- */

typedef struct {
    int     sample_id;
    time_t  timestamp;
    float   measurements[SMF_SPC_SAMPLE_SIZE];
    int     num_measurements;
    int     machine_idx;
    int     product_idx;
    int     char_idx;           /* quality characteristic index */
    char    operator_name[32];
    int     shift;
    bool    is_valid;
    bool    is_out_of_control;
    int     violated_rules;     /* bitmask of WE rules violated */
} SMF_SPCSample;

/* ---- L1: Subgroup (for X-bar/R/S charts) ---- */

typedef struct {
    int     subgroup_id;
    time_t  timestamp;
    float   x_bar;              /* subgroup mean */
    float   range;              /* subgroup range (R) */
    float   std_dev;            /* subgroup standard deviation (S) */
    int     sample_size;
    bool    x_bar_in_control;
    bool    range_in_control;
} SMF_SPCSubgroup;

/* ---- L1: Control Chart Definition ---- */

typedef enum {
    SMF_CHART_XBAR_R    = 0,  /* X-bar and R chart */
    SMF_CHART_XBAR_S    = 1,  /* X-bar and S chart */
    SMF_CHART_I_MR      = 2,  /* Individuals and Moving Range */
    SMF_CHART_P         = 3,  /* p-chart (fraction nonconforming) */
    SMF_CHART_NP        = 4,  /* np-chart (number nonconforming) */
    SMF_CHART_C         = 5,  /* c-chart (count of nonconformities) */
    SMF_CHART_U         = 6,  /* u-chart (nonconformities per unit) */
    SMF_CHART_EWMA      = 7,  /* Exponentially Weighted Moving Average */
    SMF_CHART_CUSUM     = 8   /* Cumulative Sum */
} SMF_ChartType;

typedef struct {
    SMF_ChartType   type;
    char            name[SMF_NAME_LEN];
    char            characteristic[SMF_DESC_LEN];
    float           target;         /* process target / nominal */
    float           usl;            /* upper specification limit */
    float           lsl;            /* lower specification limit */
    float           cl;             /* center line */
    float           ucl;            /* upper control limit */
    float           lcl;            /* lower control limit */
    float           ucl_r;          /* UCL for range (if applicable) */
    float           lcl_r;          /* LCL for range */
    float           sigma;          /* estimated process sigma */
    float           cp;             /* process capability */
    float           cpk;            /* process capability index */
    float           pp;             /* process performance */
    float           ppk;            /* process performance index */
    int             subgroup_size;  /* n for X-bar charts */
    float           d2;             /* control chart constant */
    float           d3;             /* control chart constant */
    float           d4;             /* control chart constant */
    float           a2;             /* control chart constant */
    float           a3;             /* control chart constant */
    int             num_subgroups_used;
    bool            is_stable;      /* all points within limits */
    bool            is_capable;     /* Cp/Cpk >= 1.33 */
    float           percent_nonconforming;  /* estimated */
} SMF_ControlChart;

/* ---- L1: Quality Inspection Record ---- */

typedef enum {
    SMF_INSP_VISUAL      = 0,
    SMF_INSP_DIMENSIONAL = 1,
    SMF_INSP_FUNCTIONAL  = 2,
    SMF_INSP_MATERIAL    = 3,
    SMF_INSP_NON_DESTRUCTIVE = 4,
    SMF_INSP_CMM         = 5
} SMF_InspectionType;

typedef enum {
    SMF_DEFECT_CRITICAL  = 0,  /* safety/regulatory */
    SMF_DEFECT_MAJOR     = 1,  /* product nonfunctional */
    SMF_DEFECT_MINOR     = 2   /* cosmetic / non-critical */
} SMF_DefectSeverity;

typedef struct {
    int     inspection_id;
    time_t  timestamp;
    int     work_order_idx;
    int     product_idx;
    int     operation_idx;
    int     machine_idx;
    SMF_InspectionType type;
    int     lot_size;
    int     sample_size;
    int     defects_found;
    int     defect_codes[8];
    SMF_DefectSeverity severities[8];
    int     num_defects;
    bool    lot_accepted;
    char    inspector[32];
    char    notes[SMF_DESC_LEN];
} SMF_InspectionRecord;

/* ---- L1: Pareto Analysis ---- */

typedef struct {
    int     defect_code;
    char    defect_name[SMF_NAME_LEN];
    int     frequency;
    float   percentage;
    float   cumulative_pct;
} SMF_ParetoItem;

/* ---- L1: Root Cause Analysis (5-Why + Fishbone) ---- */

typedef struct {
    char    problem_statement[SMF_DESC_LEN];
    char    why_chain[5][SMF_DESC_LEN];  /* 5-Why answers */
    int     num_whys;
    int     root_cause_category;  /* 0=man,1=machine,2=method,3=material,4=measure,5=environment */
    int     corrective_action_idx;
    bool    is_resolved;
    time_t  resolution_date;
} SMF_RootCauseAnalysis;

/* ================================================================
 *  API: SPC Chart Construction
 * ================================================================ */

/**
 * Initialize an X-bar/R control chart.
 * Sets up chart constants (A2, D3, D4, d2) based on subgroup size.
 *
 * Constants from ASTM E2587 / ISO 7870-2.
 * For n=5: A2=0.577, D3=0, D4=2.114, d2=2.326
 */
int  smf_spc_init_xbar_r(SMF_ControlChart *chart, const char *name,
                          const char *characteristic, float usl, float lsl,
                          float target, int subgroup_size);

/** Set control chart constants using known standard values */
int  smf_spc_set_standard(SMF_ControlChart *chart, float mu, float sigma);

/**
 * Compute control limits from subgroup data.
 *
 * Formulas (Shewhart 1931):
 *   CL = X_double_bar = Σ x_bar_i / k
 *   UCL_X = X_double_bar + A2 * R_bar
 *   LCL_X = X_double_bar - A2 * R_bar
 *   UCL_R = D4 * R_bar
 *   LCL_R = D3 * R_bar
 *   where R_bar = Σ R_i / k
 *
 * Returns number of subgroups processed.
 */
int  smf_spc_compute_limits(SMF_ControlChart *chart,
                             const SMF_SPCSubgroup *subgroups,
                             int num_subgroups);

/** Calculate process capability indices */
int  smf_spc_capability(SMF_ControlChart *chart);

/** Add subgroup data from raw measurements */
int  smf_spc_add_subgroup(SMF_SPCSubgroup *subgroup,
                           const float *measurements, int n);

/** Check a subgroup against control limits */
int  smf_spc_check_subgroup(const SMF_ControlChart *chart,
                             const SMF_SPCSubgroup *subgroup,
                             int *violated_rules);

/* ================================================================
 *  API: Western Electric Rules (L5: Pattern Detection)
 *
 *  WE Rule 1: One point beyond 3-sigma limits
 *  WE Rule 2: 2 of 3 consecutive points beyond 2-sigma (same side)
 *  WE Rule 3: 4 of 5 consecutive points beyond 1-sigma (same side)
 *  WE Rule 4: 8 consecutive points on same side of center line
 *  WE Rule 5: 6 consecutive points trending up or down
 *  WE Rule 6: 14 consecutive points alternating up/down
 *  WE Rule 7: 15 consecutive points within 1-sigma (stratification)
 *  WE Rule 8: 8 consecutive points beyond 1-sigma on both sides
 * ================================================================ */

/** Check WE rules for a sequence of points (returns bitmask) */
int  smf_weco_check_points(const float *values, const float *ranges,
                            int num_points, float cl, float ucl, float lcl,
                            float sigma);

/** Name of a Western Electric rule by bit index */
const char* smf_weco_rule_name(int rule_bit);

/* ================================================================
 *  API: Process Capability (L4: ISO 22514-4)
 *
 *  Cp  = (USL - LSL) / (6 * σ_within)
 *  Cpk = min[(USL - μ) / (3 * σ), (μ - LSL) / (3 * σ)]
 *  Pp  = (USL - LSL) / (6 * σ_overall)
 *  Ppk = min[(USL - μ) / (3 * σ_overall), (μ - LSL) / (3 * σ_overall)]
 * ================================================================ */

float smf_calc_cp(float usl, float lsl, float sigma_within);
float smf_calc_cpk(float usl, float lsl, float mean, float sigma);
float smf_calc_pp(float usl, float lsl, float sigma_overall);
float smf_calc_ppk(float usl, float lsl, float mean, float sigma_overall);

/** Six Sigma capability interpretation */
const char* smf_capability_rating(float cpk);

/* ================================================================
 *  API: Inspection Management
 * ================================================================ */

/** Create an inspection record */
int  smf_inspection_create(SMF_Factory *factory, int wo_idx,
                            int operation_idx, SMF_InspectionType type,
                            int lot_size);

/** Record inspection result */
int  smf_inspection_record_result(SMF_Factory *factory,
                                   int inspection_id, int defects,
                                   const int *defect_codes,
                                   const SMF_DefectSeverity *severities,
                                   int num_defects);

/** Accept/reject lot based on AQL sampling plan */
bool smf_inspection_accept_lot(const SMF_InspectionRecord *inspection,
                                float aql_percent, int sample_size);

/* ================================================================
 *  API: Pareto & Root Cause
 * ================================================================ */

/** Build Pareto chart from defect data */
int  smf_pareto_analyze(const int *defect_counts, const char **defect_names,
                         int num_categories, SMF_ParetoItem *result);

/** Root cause analysis using 5-Why method */
int  smf_rca_5why(SMF_RootCauseAnalysis *rca, const char *problem);

/** Ishikawa (Fishbone) 6M categories */
const char* smf_ishikawa_category(int category_id);

/* Utility */
const char* smf_chart_type_name(SMF_ChartType type);
const char* smf_inspection_type_name(SMF_InspectionType type);
const char* smf_defect_severity_name(SMF_DefectSeverity severity);

#endif