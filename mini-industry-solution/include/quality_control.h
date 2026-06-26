/**
 * mini-industry-solution: quality_control.h
 *
 * Statistical Quality Control - SPC, Control Charts, Process Capability
 * =====================================================================
 *
 * L1 Definitions: QualityMetric, ControlChart, InspectionRecord, CapabilityIndex
 * L2 Core Concepts: Common vs special cause variation, Process stability
 * L3 Engineering: Control chart data pipeline, Inspection workflow
 * L4 Standards: Western Electric Rules (WECO), ISO 9001, Six Sigma,
 *               AIAG SPC Manual, ISO 22514 (Capability)
 * L5 Algorithms: X-bar/R chart, Cp/Cpk/Pp/Ppk, Pareto analysis, Gauge R&R
 * L6 Canonical Problems: Defect rate analysis, Out-of-control detection
 * L7 Applications: Supplier quality scorecard, Process monitoring dashboard
 * L8 Advanced: Multivariate SPC (Hotelling T2), CUSUM, EWMA control charts
 * L9 Industry Frontiers: AI visual quality inspection, Real-time SPC
 */

#ifndef QUALITY_CONTROL_H
#define QUALITY_CONTROL_H

#include <stddef.h>
#include <time.h>

/* === L1: Core Definitions === */

typedef enum { CHART_XBAR_R, CHART_XBAR_S, CHART_INDIVIDUALS,
               CHART_P, CHART_NP, CHART_C, CHART_U,
               CHART_CUSUM, CHART_EWMA } ChartType;

typedef enum { CHART_IN_CONTROL, CHART_WARNING, CHART_OUT_OF_CONTROL,
               CHART_INSUFFICIENT_DATA } ChartStatus;

typedef struct {
    ChartType type;
    int subgroup_size;
    int total_subgroups;
    double *subgroup_means;
    double *subgroup_ranges;
    double grand_mean;
    double mean_range;
    double ucl_x;
    double lcl_x;
    double ucl_r;
    double lcl_r;
    double a2, d3, d4;
    ChartStatus status_x;
    ChartStatus status_r;
} XBarRChart;

typedef struct {
    double cp;
    double cpk;
    double pp;
    double ppk;
    double cm;
    double cmk;
    double sigma_level;
    double dpm;
    int capable;
} CapabilityIndex;

typedef struct {
    char part_number[32];
    char characteristic[64];
    double nominal;
    double usl;
    double lsl;
    double *measurements;
    size_t n_measurements;
    int subgroup_size;
} QualityCharacteristic;

typedef enum { INSP_PASS, INSP_FAIL, INSP_REWORK, INSP_SCRAP,
               INSP_CONCESSION } InspectionResult;

typedef struct {
    char lot_number[32];
    char inspector[32];
    struct timespec timestamp;
    QualityCharacteristic characteristic;
    double measured_value;
    InspectionResult result;
} InspectionRecord;

typedef struct {
    char supplier_code[16];
    char supplier_name[64];
    int lots_received;
    int lots_accepted;
    int parts_inspected;
    int parts_defective;
    double dpmo;
    double quality_score;
} SupplierScorecard;

/* === L5: Control Chart Operations === */

void xbar_r_chart_init(XBarRChart *chart, int subgroup_size,
                        double *data_buffer, size_t n_measurements);
int  xbar_r_chart_update(XBarRChart *chart, const double *subgroup,
                          int subgroup_size);
int  weco_rules_check(const XBarRChart *chart, int *violated_rules,
                       int max_rules);
const char *weco_rule_description(int rule_number);

void control_chart_constants(int subgroup_size,
                               double *a2, double *d3, double *d4,
                               double *b3, double *b4, double *c4);

/* === L5: Process Capability === */

void capability_calculate(const double *measurements, size_t n,
                           double usl, double lsl,
                           CapabilityIndex *out_idx);
void capability_calculate_subgrouped(const double *measurements, size_t n,
                                      int subgroup_size,
                                      double usl, double lsl,
                                      CapabilityIndex *out_idx);
double process_sigma_level(double dpmo);
double dpmo_to_cpk(double dpmo);

/* === L5: Pareto Analysis === */

typedef struct {
    char category[48];
    int count;
    double percentage;
    double cumulative_pct;
} ParetoItem;

int pareto_analysis(const int *counts, const char **labels, int n_categories,
                     ParetoItem *out_items);

/* === L5: Gauge R&R === */

typedef struct {
    double repeatability;
    double reproducibility;
    double grr_total;
    double part_variation;
    double total_variation;
    double grr_pct;
    int acceptable;
} GaugeRRResult;

int gauge_rnr_analysis(const double *measurements,
                        int n_parts, int n_operators, int n_trials,
                        GaugeRRResult *out_result);

/* === L6: Defect Analysis === */

double defect_rate_calculate(int defect_count, int total_count);
double dpmo_calculate(int defect_count, int total_count,
                       int opportunity_count);
double rolled_throughput_yield(const double *yields, int n_stages);
double process_sigma_from_dpmo(double dpmo);

/* === L7: Quality Scorecard === */

void supplier_scorecard_init(SupplierScorecard *sc,
                               const char *code, const char *name);
void supplier_scorecard_update(SupplierScorecard *sc,
                                 int lots_received_delta,
                                 int lots_accepted_delta,
                                 int parts_inspected_delta,
                                 int parts_defective_delta);

/* === L8: CUSUM === */

typedef struct {
    double *cusum_high;
    double *cusum_low;
    double k;
    double h;
    int capacity;
    int count;
} CUSUMChart;

void cusum_init(CUSUMChart *c, int capacity, double k, double h);
int  cusum_update(CUSUMChart *c, double value,
                   int *out_signal_high, int *out_signal_low);

#endif /* QUALITY_CONTROL_H */
