/**
 * mini-industry-solution: industrial_core.h
 *
 * Industrial Core - Process Control, Sensor Fusion & Automation Primitives
 * ========================================================================
 *
 * L1 Definitions: SensorReading, ProcessVariable, ActuatorCommand,
 *                  AlarmCondition, DataLogger, DigitalTwin, ControlLoop
 * L2 Core Concepts: Process control (PID), Sensor validation, Deadband
 * L3 Engineering: Sensor mesh topology, Actuator network, Multi-loop
 * L4 Standards: ISA-88, ISA-95, IEC 61131-3, NAMUR NE 43
 * L5 Algorithms: Kalman filter (1D), SMA/EMA/WMA, Thermocouple linearization
 * L6 Canonical Problems: Industrial I/O pipeline, Alarm management, PID loop
 * L7 Applications: SCADA data model, HMI tag database
 * L8 Advanced: Digital twin, Multi-rate sensor fusion
 * L9 Industry Frontiers: Industry 4.0 RAMI, OPC UA, AAS
 */

#ifndef INDUSTRIAL_CORE_H
#define INDUSTRIAL_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* ============================================================================
 * L1: Core Definitions
 * ============================================================================
 */

typedef enum {
    SIG_ANALOG_4_20MA,
    SIG_ANALOG_0_10V,
    SIG_DIGITAL_ON_OFF,
    SIG_DIGITAL_PULSE,
    SIG_THERMOCOUPLE_K,
    SIG_THERMOCOUPLE_J,
    SIG_RTD_PT100,
    SIG_MODBUS_REGISTER,
    SIG_HART_DIGITAL,
    SIG_FOUNDATION_FB
} SignalType;

typedef enum {
    SENSOR_OK,
    SENSOR_DRIFT,
    SENSOR_NOISE,
    SENSOR_STALE,
    SENSOR_FAULT_HI,
    SENSOR_FAULT_LO,
    SENSOR_FAILED
} SensorHealth;

typedef enum {
    PVQ_GOOD,
    PVQ_UNCERTAIN,
    PVQ_BAD,
    PVQ_MANUAL,
    PVQ_SUBSTITUTED
} PVQuality;

typedef enum {
    ALARM_DEBUG,
    ALARM_LOW,
    ALARM_MEDIUM,
    ALARM_HIGH,
    ALARM_CRITICAL,
    ALARM_SHELL
} AlarmPriority;

typedef enum {
    LOOP_MANUAL,
    LOOP_AUTO,
    LOOP_CASCADE,
    LOOP_RATIO,
    LOOP_FEEDFORWARD,
    LOOP_OVERRIDE
} LoopMode;

typedef struct {
    double value;
    double raw_value;
    SignalType signal_type;
    PVQuality quality;
    SensorHealth health;
    struct timespec timestamp;
    int64_t sequence;
    char tag_name[48];
} SensorReading;

typedef struct {
    char name[48];
    double value;
    double setpoint;
    double output;
    double error;
    double min_range;
    double max_range;
    char engineering_unit[16];
    LoopMode mode;
    SensorReading last_reading;
    int alarm_active;
} ProcessVariable;

typedef struct {
    char name[48];
    double requested_position;
    double current_position;
    double ramp_rate;
    int interlock_active;
    int feedback_healthy;
    struct timespec last_cmd;
    struct timespec last_fb;
} ActuatorCommand;

typedef struct {
    char tag[48];
    AlarmPriority priority;
    double trip_point;
    double deadband;
    double current_value;
    char message[128];
    struct timespec on_time;
    struct timespec ack_time;
    int is_active;
    int is_acknowledged;
    int is_suppressed;
    int occurrence_count;
} AlarmCondition;

typedef struct {
    ProcessVariable pv;
    ActuatorCommand actuator;
    double kp, ki, kd;
    double integral_limit;
    double integral_accum;
    double prev_error;
    double derivative_filter;
    double sample_time;
    struct timespec last_update;
    int cycle_count;
    double overshoot_max;
    double settling_time;
    double steady_state_error;
} ControlLoop;

typedef struct {
    SensorReading *buffer;
    size_t capacity;
    size_t head;
    size_t count;
    int overwrite_oldest;
} DataLogger;

typedef struct {
    char asset_id[48];
    double position;
    double velocity;
    double acceleration;
    double temperature;
    double vibration_rms;
    double power_consumption;
    double runtime_hours;
    int is_running;
    struct timespec last_sync;
    int degradation_index;
} DigitalTwin;

/* ============================================================================
 * L2-L5: API Declarations
 * ============================================================================
 */

void   sensor_reading_init(SensorReading *s, const char *tag, SignalType type);
int    sensor_validate_range(const SensorReading *s,
                             double low_limit, double high_limit);
double sensor_deadband_filter(double current_value,
                              double new_value, double deadband);

double thermocouple_k_mv_to_celsius(double millivolts);
double thermocouple_j_mv_to_celsius(double millivolts);
double rtd_pt100_to_celsius(double resistance_ohms);
double analog_normalize(double raw_value, double raw_min, double raw_max,
                        double eu_min, double eu_max);
double analog_raw_from_signal(double raw, SignalType type);
int    signal_linearity_check(const double *inputs, const double *outputs,
                              size_t n, double *out_r_squared);

double sma_filter(double new_sample, double *history, int *index,
                  int window_size, double *running_sum);
double wma_filter(const double *history, int window_size);
double ema_filter(double new_sample, double prev_ema, double alpha);
double median_filter(const double *window, int window_size);

typedef struct {
    double x_est;
    double p_est;
    double process_noise;
    double measurement_noise;
} KalmanFilter1D;

void   kalman1d_init(KalmanFilter1D *kf, double initial_estimate,
                     double initial_uncertainty,
                     double process_noise, double measurement_noise);
double kalman1d_update(KalmanFilter1D *kf, double measurement);

int alarm_evaluate(AlarmCondition *alarm, double current_value,
                   AlarmPriority *out_priority);
int alarm_nuisance_check(const AlarmCondition *alarm,
                         double max_rate_per_hour);
int alarm_flood_detect(const AlarmCondition *alarms, int count,
                       double time_window_sec);

void pv_init(ProcessVariable *pv, const char *name,
             double min_range, double max_range,
             const char *engineering_unit);
void pv_update(ProcessVariable *pv, const SensorReading *reading);
int  pv_alarm_check(const ProcessVariable *pv, double hihi, double hi,
                    double lo, double lolo, AlarmCondition *alarms);

void   control_loop_init(ControlLoop *loop, const char *pv_name,
                         double min_range, double max_range,
                         const char *units,
                         double kp, double ki, double kd,
                         double sample_time, double integral_limit);
double control_loop_step(ControlLoop *loop,
                         double setpoint, double measured_value);
void   control_loop_reset(ControlLoop *loop);
int    control_loop_performance(const ControlLoop *loop,
                                double *out_overshoot,
                                double *out_settling,
                                double *out_steady_err);

int ziegler_nichols_open_loop(double model_gain, double dead_time,
                              double time_constant,
                              double *out_kp, double *out_ki,
                              double *out_kd);
int cohen_coon_tuning(double model_gain, double dead_time,
                      double time_constant,
                      double *out_kp, double *out_ki, double *out_kd);
int lambda_tuning(double model_gain, double dead_time,
                  double time_constant, double lambda_factor,
                  double *out_kp, double *out_ki);

void   datalogger_init(DataLogger *log, SensorReading *buffer,
                       size_t capacity, int overwrite);
int    datalogger_append(DataLogger *log, const SensorReading *reading);
const SensorReading *datalogger_get(const DataLogger *log, size_t i);
size_t datalogger_count(const DataLogger *log);
void   datalogger_clear(DataLogger *log);
double datalogger_average(const DataLogger *log, size_t n);
double datalogger_min(const DataLogger *log, size_t n);
double datalogger_max(const DataLogger *log, size_t n);
int    datalogger_variance(const DataLogger *log, size_t n,
                           double *out_mean, double *out_variance);

void digital_twin_init(DigitalTwin *twin, const char *asset_id);
void digital_twin_sync(DigitalTwin *twin, double position, double velocity,
                       double temperature, double vibration);
int  digital_twin_degradation_index(const DigitalTwin *twin,
                                    double baseline_vibration,
                                    double failure_limit_vibration);
void digital_twin_predict_health(const DigitalTwin *twin,
                                 double *out_health_pct,
                                 double *out_remaining_hours_est);

double stats_mean(const double *data, size_t n);
double stats_stddev(const double *data, size_t n);
double stats_median(double *data, size_t n);
double stats_pearson_r(const double *x, const double *y, size_t n);
int    stats_linear_regression(const double *x, const double *y, size_t n,
                               double *out_slope, double *out_intercept,
                               double *out_r_squared);

#endif /* INDUSTRIAL_CORE_H */
