/**
 * mini-industry-solution: predictive_maintenance.h
 *
 * Predictive Maintenance — CBM, RCM, Weibull Analysis, Anomaly Detection
 * ======================================================================
 *
 * L1 Definitions: EquipmentProfile, FailureMode, MaintenanceTask, VibrationSample
 * L2 Core Concepts: CBM (Condition-Based), PdM vs Preventive, P-F Curve
 * L3 Engineering: Condition monitoring pipeline, Fleet management data model
 * L4 Standards: ISO 13374 CM&D, ISO 14224 RCM, Weibull distribution,
 *               SAE JA1011 RCM, MIL-STD-1629A FMECA
 * L5 Algorithms: Weibull reliability, Trend analysis, Z-score anomaly,
 *                Linear RUL estimation, FMEA RPN
 * L6 Canonical Problems: Bearing degradation prediction, Rotating equipment RUL
 * L7 Applications: Fleet maintenance scheduling, Vibration-based diagnostics
 * L8 Advanced: PCA feature extraction, Spectral analysis basics
 * L9 Industry Frontiers: Digital twin PdM, Federated learning for fleet
 */

#ifndef PREDICTIVE_MAINTENANCE_H
#define PREDICTIVE_MAINTENANCE_H

#include <stddef.h>
#include <time.h>

/* === L1: Core Definitions === */

typedef enum {
    EQUIP_PUMP,
    EQUIP_MOTOR,
    EQUIP_COMPRESSOR,
    EQUIP_TURBINE,
    EQUIP_CONVEYOR,
    EQUIP_BEARING,
    EQUIP_GEARBOX,
    EQUIP_HEAT_EXCHANGER,
    EQUIP_VALVE,
    EQUIP_FAN
} EquipmentType;

typedef enum {
    CRITICALITY_LOW,
    CRITICALITY_MEDIUM,
    CRITICALITY_HIGH,
    CRITICALITY_SAFETY
} EquipmentCriticality;

typedef enum {
    MAINT_CORRECTIVE,
    MAINT_PREVENTIVE_TIME,
    MAINT_PREVENTIVE_USAGE,
    MAINT_PREDICTIVE,
    MAINT_CONDITION_BASED,
    MAINT_RUN_TO_FAILURE
} MaintenanceStrategy;

typedef struct {
    char tag[48];
    char description[128];
    EquipmentType type;
    EquipmentCriticality criticality;
    double design_life_hours;
    double current_runtime;
    int starts_count;
    double last_vibration;
    double last_temperature;
    int health_score;
    struct timespec last_maintenance;
    double maintenance_interval_hours;
    MaintenanceStrategy strategy;
} EquipmentProfile;

typedef struct {
    char name[64];
    double severity;
    double occurrence;
    double detection;
    int rpn;
    char cause[128];
    char effect[128];
    char recommended_action[256];
} FailureMode;

typedef struct {
    char description[128];
    double estimated_hours;
    int priority;
    EquipmentProfile *equipment;
    struct timespec due_date;
    int completed;
} MaintenanceTask;

typedef struct {
    double *samples;
    size_t capacity;
    size_t count;
    double sample_rate_hz;
    struct timespec start_time;
} VibrationSampleBuffer;

typedef struct {
    double *values;
    double *timestamps;
    size_t count;
    size_t capacity;
    double mean;
    double stddev;
    double slope;
    int trend_direction;
} TrendSeries;

/* === L4: Weibull Reliability (ISO 14224) === */

double weibull_pdf(double t, double eta, double beta);
double weibull_cdf(double t, double eta, double beta);
double weibull_reliability(double t, double eta, double beta);
double weibull_failure_rate(double t, double eta, double beta);
double weibull_mttf(double eta, double beta);
int    weibull_parameter_estimate(const double *failure_times, size_t n,
                                   double *out_eta, double *out_beta);

/* === L5: Condition Monitoring & Diagnostics === */

void   trend_series_init(TrendSeries *ts, double *buffer, size_t capacity);
int    trend_series_append(TrendSeries *ts, double value, double timestamp);
double trend_series_forecast(const TrendSeries *ts, double future_time);
int    trend_series_detect_change(const TrendSeries *ts,
                                   double threshold, int *out_index);

int    anomaly_zscore_detect(const double *values, size_t n,
                              double threshold, int *out_indices,
                              int max_out, int *out_count);
int    anomaly_moving_average_detect(const double *values, size_t n,
                                      int window, double threshold,
                                      int *out_indices, int max_out,
                                      int *out_count);

/* === L5: Remaining Useful Life Estimation === */

double rul_linear_estimate(double current_value, double failure_threshold,
                            double degradation_rate);
double rul_degradation_rate_estimate(const double *values, size_t n,
                                      const double *timestamps);
double rul_exponential_estimate(double current_value, double initial_value,
                                 double failure_threshold, double lambda);

/* === L6: Bearing & Rotating Equipment === */

typedef struct {
    double bpfo;  /* Ball Pass Frequency Outer */
    double bpfi;  /* Ball Pass Frequency Inner */
    double bsf;   /* Ball Spin Frequency */
    double ftf;   /* Fundamental Train Frequency */
    double shaft_speed_rpm;
    int n_balls;
    double contact_angle_deg;
    double pitch_diameter;
    double ball_diameter;
} BearingGeometry;

void bearing_fault_frequencies(BearingGeometry *bg, double shaft_speed_rpm,
                                 int n_balls, double pitch_diameter,
                                 double ball_diameter, double contact_angle_deg);
int  bearing_fault_diagnose(double bpfo_amplitude, double bpfi_amplitude,
                              double bsf_amplitude, double threshold,
                              int *fault_type);

/* === L6: Maintenance Optimization === */

double maintenance_optimal_interval(double cost_preventive,
                                      double cost_corrective,
                                      double eta, double beta);
double maintenance_cost_per_hour(double interval, double cost_pm,
                                   double cost_cm,
                                   double eta, double beta);

/* === L7: Fleet Management === */

int maintenance_sort_by_priority(MaintenanceTask *tasks, int count);
int maintenance_sort_by_due_date(MaintenanceTask *tasks, int count);
int maintenance_filter_overdue(MaintenanceTask *tasks, int count,
                                 struct timespec now,
                                 MaintenanceTask *out_overdue,
                                 int max_out);

/* === L7: Condition Baseline === */

int condition_baseline_compute(const double *measurements, size_t n,
                                double *out_mean, double *out_stddev,
                                double *out_warning, double *out_alarm);
double health_score_compute(double current_value, double baseline,
                             double alarm_limit);

#endif /* PREDICTIVE_MAINTENANCE_H */
