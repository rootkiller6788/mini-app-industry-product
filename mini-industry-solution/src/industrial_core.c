/**
 * mini-industry-solution: industrial_core.c
 * Implementation: Sensor processing, PID control, Kalman filter, data logging
 * Ref: Astrom & Hagglund (1995), Kalman (1960), NIST Monograph 175,
 *      IEC 60751, ISA-18.2, EEMUA 191
 */
#include "industrial_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

static struct timespec clock_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts;
}

static double timespec_diff_sec(const struct timespec *a,
                                 const struct timespec *b) {
    double da = (double)a->tv_sec + (double)a->tv_nsec / 1e9;
    double db = (double)b->tv_sec + (double)b->tv_nsec / 1e9;
    return da - db;
}

/* === Sensor Reading & Validation (L2) === */

void sensor_reading_init(SensorReading *s, const char *tag, SignalType type) {
    if (!s || !tag) return;
    memset(s, 0, sizeof(*s));
    strncpy(s->tag_name, tag, sizeof(s->tag_name) - 1);
    s->tag_name[sizeof(s->tag_name) - 1] = '\0';
    s->signal_type = type;
    s->quality = PVQ_GOOD;
    s->health = SENSOR_OK;
    s->timestamp = clock_now();
    s->sequence = 0;
}

int sensor_validate_range(const SensorReading *s,
                           double low_limit, double high_limit) {
    if (!s) return 0;
    if (low_limit > high_limit) return 0;
    if (s->signal_type == SIG_ANALOG_4_20MA) {
        if (s->raw_value < 3.6 || s->raw_value > 21.0) return 0;
    }
    if (s->value < low_limit || s->value > high_limit) return 0;
    return 1;
}

double sensor_deadband_filter(double current_value,
                               double new_value, double deadband) {
    double delta = new_value - current_value;
    if (delta < 0) delta = -delta;
    if (delta >= deadband) return new_value;
    return current_value;
}

/* === Analog Signal Processing (L5) === */

double thermocouple_k_mv_to_celsius(double millivolts) {
    /* ITS-90 Type K, range 0-1372C (NIST Monograph 175) */
    static const double c[11] = {
        -1.7600413686e-2,  3.8921204975e-2,  1.8558770032e-5,
        -9.9457592874e-8,  3.1840945719e-10, -5.6072844889e-13,
         5.6075059059e-16, -3.2020720003e-19,  9.7151147152e-23,
        -1.2104721275e-26, 0.0
    };
    /* Exponential correction coefficients (ITS-90) */
    static const double a0 = 1.185976e-1;
    static const double a1 = -1.183432e-4;
    static const double a2 = 1.269686e3;
    double T = millivolts / 0.041;
    for (int iter = 0; iter < 10; iter++) {
        double E = 0.0, dE = 0.0, Tp = 1.0;
        for (int j = 1; j < 10; j++) {
            E += c[j] * Tp * T;
            dE += c[j] * (j + 1) * Tp;
            Tp *= T;
        }
        double residual = E - millivolts;
        if (fabs(residual) < 1e-6) break;
        if (fabs(dE) < 1e-15) break;
        T -= residual / dE;
    }
    return T;
}

double thermocouple_j_mv_to_celsius(double millivolts) {
    static const double c1 = 1.9528268e-2, c2 = -1.2286185e-6;
    static const double c3 = -1.0752178e-9, c4 = -5.9086933e-13;
    static const double c5 = -1.7256713e-16;
    double T = millivolts / 0.054;
    for (int iter = 0; iter < 10; iter++) {
        double T2 = T * T, T3 = T2 * T, T4 = T3 * T, T5 = T4 * T;
        double E = c1*T + c2*T2 + c3*T3 + c4*T4 + c5*T5;
        double dE = c1 + 2*c2*T + 3*c3*T2 + 4*c4*T3 + 5*c5*T4;
        double residual = E - millivolts;
        if (fabs(residual) < 1e-6) break;
        if (fabs(dE) < 1e-15) break;
        T -= residual / dE;
    }
    return T;
}

double rtd_pt100_to_celsius(double resistance_ohms) {
    static const double R0 = 100.0, A = 3.9083e-3;
    static const double B = -5.775e-7, C = -4.183e-12;
    if (resistance_ohms < R0) {
        double T = (resistance_ohms - R0) / (R0 * A);
        for (int iter = 0; iter < 20; iter++) {
            double T2 = T*T, T3 = T2*T;
            double Rc = R0*(1.0+A*T+B*T2+C*(T-100.0)*T3);
            double dR = R0*(A+2*B*T+C*(4*T3-300*T2));
            double residual = Rc - resistance_ohms;
            if (fabs(residual) < 1e-8) break;
            if (fabs(dR) < 1e-15) break;
            T -= residual / dR;
        }
        return T;
    }
    double a = B, b = A, c = 1.0 - resistance_ohms/R0;
    double disc = b*b - 4.0*a*c;
    if (a == 0.0) return -c/b;
    if (disc < 0.0) return 0.0;
    return (-b + sqrt(disc)) / (2.0 * a);
}

double analog_normalize(double raw_value, double raw_min, double raw_max,
                         double eu_min, double eu_max) {
    if (raw_max == raw_min) return eu_min;
    double fraction = (raw_value - raw_min) / (raw_max - raw_min);
    return eu_min + fraction * (eu_max - eu_min);
}

double analog_raw_from_signal(double raw, SignalType type) {
    switch (type) {
    case SIG_ANALOG_4_20MA:
        if (raw < 4.0) raw = 4.0;
        if (raw > 20.0) raw = 20.0;
        return (raw - 4.0) / 16.0 * 100.0;
    case SIG_ANALOG_0_10V:
        if (raw < 0.0) raw = 0.0;
        if (raw > 10.0) raw = 10.0;
        return raw / 10.0 * 100.0;
    case SIG_DIGITAL_ON_OFF:
        return (raw > 0.5) ? 100.0 : 0.0;
    default:
        return raw;
    }
}

int signal_linearity_check(const double *inputs, const double *outputs,
                            size_t n, double *out_r_squared) {
    if (!inputs || !outputs || !out_r_squared || n < 3) return -1;
    double slope, intercept, r2;
    int ret = stats_linear_regression(inputs, outputs, n,
                                       &slope, &intercept, &r2);
    if (ret != 0) return -1;
    *out_r_squared = r2;
    return 0;
}

/* === Filtering & Smoothing (L5) === */

double sma_filter(double new_sample, double *history, int *index,
                   int window_size, double *running_sum) {
    if (!history || !index || !running_sum || window_size < 1)
        return new_sample;
    if (*index < window_size) {
        history[*index] = new_sample;
        *running_sum += new_sample;
        (*index)++;
        return *running_sum / *index;
    }
    int old_idx = *index % window_size;
    *running_sum -= history[old_idx];
    history[old_idx] = new_sample;
    *running_sum += new_sample;
    (*index)++;
    return *running_sum / window_size;
}

double wma_filter(const double *history, int window_size) {
    if (!history || window_size < 1) return 0.0;
    double weighted_sum = 0.0, weight_sum = 0.0;
    for (int i = 0; i < window_size; i++) {
        double w = (double)(i + 1);
        weighted_sum += history[i] * w;
        weight_sum += w;
    }
    return (weight_sum > 0.0) ? weighted_sum / weight_sum : 0.0;
}

double ema_filter(double new_sample, double prev_ema, double alpha) {
    if (alpha <= 0.0) return prev_ema;
    if (alpha > 1.0) alpha = 1.0;
    return alpha * new_sample + (1.0 - alpha) * prev_ema;
}

static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

double median_filter(const double *window, int window_size) {
    if (!window || window_size < 1) return 0.0;
    if (window_size == 1) return window[0];
    double *sorted = (double *)malloc((size_t)window_size * sizeof(double));
    if (!sorted) return window[0];
    memcpy(sorted, window, (size_t)window_size * sizeof(double));
    qsort(sorted, (size_t)window_size, sizeof(double), cmp_double);
    double med = (window_size % 2 == 0)
        ? (sorted[window_size/2-1]+sorted[window_size/2])/2.0
        : sorted[window_size/2];
    free(sorted);
    return med;
}

/* === Kalman Filter (L5) === */

void kalman1d_init(KalmanFilter1D *kf, double initial_estimate,
                    double initial_uncertainty,
                    double process_noise, double measurement_noise) {
    if (!kf) return;
    kf->x_est = initial_estimate;
    kf->p_est = initial_uncertainty;
    kf->process_noise = process_noise;
    kf->measurement_noise = measurement_noise;
}

double kalman1d_update(KalmanFilter1D *kf, double measurement) {
    if (!kf) return 0.0;
    double x_pred = kf->x_est;
    double p_pred = kf->p_est + kf->process_noise;
    double K = p_pred / (p_pred + kf->measurement_noise);
    kf->x_est = x_pred + K * (measurement - x_pred);
    kf->p_est = (1.0 - K) * p_pred;
    return kf->x_est;
}

/* === Alarm Management (L6) === */

int alarm_evaluate(AlarmCondition *alarm, double current_value,
                    AlarmPriority *out_priority) {
    if (!alarm) return 0;
    alarm->current_value = current_value;
    if (alarm->is_suppressed) {
        if (out_priority) *out_priority = alarm->priority;
        return alarm->is_active;
    }
    if (!alarm->is_active) {
        if (current_value >= alarm->trip_point) {
            alarm->is_active = 1;
            alarm->on_time = clock_now();
            alarm->is_acknowledged = 0;
            alarm->occurrence_count++;
        }
    } else {
        double reset_point = alarm->trip_point - alarm->deadband;
        if (current_value <= reset_point) alarm->is_active = 0;
    }
    if (out_priority) *out_priority = alarm->priority;
    return alarm->is_active;
}

int alarm_nuisance_check(const AlarmCondition *alarm,
                          double max_rate_per_hour) {
    if (!alarm || !alarm->is_active) return 0;
    struct timespec _now = clock_now(); double elapsed = timespec_diff_sec(&_now, &alarm->on_time);
    if (elapsed <= 0.0) return 0;
    double rate = (double)alarm->occurrence_count / (elapsed / 3600.0);
    return (rate > max_rate_per_hour) ? 1 : 0;
}

int alarm_flood_detect(const AlarmCondition *alarms, int count,
                        double time_window_sec) {
    if (!alarms || count < 2) return 0;
    int active = 0;
    struct timespec now = clock_now();
    for (int i = 0; i < count; i++) {
        if (alarms[i].is_active) {
            double dt = timespec_diff_sec(&now, &alarms[i].on_time);
            if (dt < time_window_sec) active++;
        }
    }
    return (active >= 10) ? 1 : 0;
}

/* === Process Variable Operations (L3) === */

void pv_init(ProcessVariable *pv, const char *name,
              double min_range, double max_range,
              const char *engineering_unit) {
    if (!pv) return;
    memset(pv, 0, sizeof(*pv));
    if (name) {
        strncpy(pv->name, name, sizeof(pv->name) - 1);
        pv->name[sizeof(pv->name) - 1] = '\0';
    }
    pv->min_range = min_range;
    pv->max_range = max_range;
    if (engineering_unit) {
        strncpy(pv->engineering_unit, engineering_unit,
                sizeof(pv->engineering_unit) - 1);
        pv->engineering_unit[sizeof(pv->engineering_unit) - 1] = '\0';
    }
    pv->mode = LOOP_MANUAL;
}

void pv_update(ProcessVariable *pv, const SensorReading *reading) {
    if (!pv || !reading) return;
    pv->value = reading->value;
    pv->error = pv->setpoint - pv->value;
    memcpy(&pv->last_reading, reading, sizeof(SensorReading));
}

int pv_alarm_check(const ProcessVariable *pv, double hihi, double hi,
                    double lo, double lolo, AlarmCondition *alarms) {
    if (!pv || !alarms) return 0;
    int count = 0;
    if (pv->value >= hihi) {
        alarms[count].priority = ALARM_CRITICAL;
        snprintf(alarms[count].tag, sizeof(alarms[count].tag),
                 "%s_HIHI", pv->name);
        snprintf(alarms[count].message, sizeof(alarms[count].message),
                 "%s: %.2f >= HIHI %.2f %s",
                 pv->name, pv->value, hihi, pv->engineering_unit);
        alarms[count].trip_point = hihi;
        alarms[count].deadband = (hihi - hi) * 0.02;
        alarms[count].is_active = 1;
        count++;
    } else if (pv->value >= hi) {
        alarms[count].priority = ALARM_HIGH;
        snprintf(alarms[count].tag, sizeof(alarms[count].tag),
                 "%s_HI", pv->name);
        snprintf(alarms[count].message, sizeof(alarms[count].message),
                 "%s: %.2f >= HI %.2f %s",
                 pv->name, pv->value, hi, pv->engineering_unit);
        alarms[count].trip_point = hi;
        alarms[count].deadband = (hi - pv->min_range) * 0.02;
        alarms[count].is_active = 1;
        count++;
    } else if (pv->value <= lolo) {
        alarms[count].priority = ALARM_CRITICAL;
        snprintf(alarms[count].tag, sizeof(alarms[count].tag),
                 "%s_LOLO", pv->name);
        snprintf(alarms[count].message, sizeof(alarms[count].message),
                 "%s: %.2f <= LOLO %.2f %s",
                 pv->name, pv->value, lolo, pv->engineering_unit);
        alarms[count].trip_point = lolo;
        alarms[count].deadband = (lo - lolo) * 0.02;
        alarms[count].is_active = 1;
        count++;
    } else if (pv->value <= lo) {
        alarms[count].priority = ALARM_HIGH;
        snprintf(alarms[count].tag, sizeof(alarms[count].tag),
                 "%s_LO", pv->name);
        snprintf(alarms[count].message, sizeof(alarms[count].message),
                 "%s: %.2f <= LO %.2f %s",
                 pv->name, pv->value, lo, pv->engineering_unit);
        alarms[count].trip_point = lo;
        alarms[count].deadband = (pv->max_range - lo) * 0.02;
        alarms[count].is_active = 1;
        count++;
    }
    return count;
}

/* === Control Loop (L3-L5) === */

void control_loop_init(ControlLoop *loop, const char *pv_name,
                        double min_range, double max_range,
                        const char *units,
                        double kp, double ki, double kd,
                        double sample_time, double integral_limit) {
    if (!loop) return;
    memset(loop, 0, sizeof(*loop));
    pv_init(&loop->pv, pv_name, min_range, max_range, units);
    loop->kp = kp; loop->ki = ki; loop->kd = kd;
    loop->sample_time = sample_time;
    loop->integral_limit = integral_limit;
    loop->derivative_filter = 0.1;
    loop->last_update = clock_now();
}

double control_loop_step(ControlLoop *loop,
                          double setpoint, double measured_value) {
    if (!loop || loop->sample_time <= 0.0) return loop ? loop->pv.output : 0.0;
    struct timespec now = clock_now();
    double dt = timespec_diff_sec(&now, &loop->last_update);
    if (loop->cycle_count > 0 && dt < loop->sample_time * 0.99) return loop->pv.output;
    loop->last_update = now;
    loop->pv.value = measured_value;
    loop->pv.setpoint = setpoint;
    double error = setpoint - measured_value;
    loop->pv.error = error;
    loop->cycle_count++;

    double p_term = loop->kp * error;
    if (loop->ki > 0.0) {
        loop->integral_accum += loop->ki * error * dt;
        if (loop->integral_accum > loop->integral_limit)
            loop->integral_accum = loop->integral_limit;
        if (loop->integral_accum < -loop->integral_limit)
            loop->integral_accum = -loop->integral_limit;
    }
    double i_term = loop->integral_accum;
    double d_term = 0.0;
    if (loop->kd > 0.0 && dt > 1e-9) {
        double raw_deriv = (error - loop->prev_error) / dt;
        loop->derivative_filter = 0.9*loop->derivative_filter + 0.1*raw_deriv;
        d_term = loop->kd * loop->derivative_filter;
    }
    double output = p_term + i_term + d_term;
    if (output > 100.0) output = 100.0;
    if (output < 0.0) output = 0.0;
    if (error > loop->overshoot_max && error > 0)
        loop->overshoot_max = error;
    loop->steady_state_error = error;
    loop->prev_error = error;
    loop->pv.output = output;
    return output;
}

void control_loop_reset(ControlLoop *loop) {
    if (!loop) return;
    loop->integral_accum = 0.0; loop->prev_error = 0.0;
    loop->derivative_filter = 0.1; loop->cycle_count = 0;
    loop->overshoot_max = 0.0;
}

int control_loop_performance(const ControlLoop *loop,
                              double *out_overshoot,
                              double *out_settling,
                              double *out_steady_err) {
    if (!loop) return -1;
    if (out_overshoot) *out_overshoot = loop->overshoot_max;
    if (out_settling) *out_settling = loop->settling_time;
    if (out_steady_err) *out_steady_err = loop->steady_state_error;
    return 0;
}

/* === PID Auto-Tuning (L5) === */

int ziegler_nichols_open_loop(double K, double L, double T,
                               double *out_kp, double *out_ki,
                               double *out_kd) {
    if (K <= 0.0 || L <= 0.0 || T <= 0.0 || !out_kp) return -1;
    *out_kp = (1.0/K) * 1.2 * (T/L);
    if (out_ki) *out_ki = *out_kp / (2.0*L);
    if (out_kd) *out_kd = *out_kp * 0.5 * L;
    return 0;
}

int cohen_coon_tuning(double K, double L, double T,
                       double *out_kp, double *out_ki, double *out_kd) {
    if (K <= 0.0 || L <= 0.0 || T <= 0.0 || !out_kp) return -1;
    double ratio = L/T;
    *out_kp = (1.0/K)*(T/L)*(4.0/3.0 + ratio/4.0);
    if (out_ki) {
        double num = 32.0 + 6.0*ratio;
        double den = 13.0 + 8.0*ratio;
        *out_ki = (den > 0.0) ? *out_kp/(L*num/den) : 0.0;
    }
    if (out_kd) *out_kd = *out_kp * L * (4.0/(11.0+2.0*ratio));
    return 0;
}

int lambda_tuning(double K, double L, double T, double lambda,
                   double *out_kp, double *out_ki) {
    if (K <= 0.0 || T <= 0.0 || lambda <= 0.0 || !out_kp) return -1;
    *out_kp = T / (K * (L + lambda));
    if (out_ki) *out_ki = *out_kp / T;
    return 0;
}

/* === Data Logger (L3) === */

void datalogger_init(DataLogger *log, SensorReading *buffer,
                      size_t capacity, int overwrite) {
    if (!log || !buffer || capacity == 0) return;
    log->buffer = buffer; log->capacity = capacity;
    log->head = 0; log->count = 0;
    log->overwrite_oldest = overwrite;
}

int datalogger_append(DataLogger *log, const SensorReading *reading) {
    if (!log || !reading || log->capacity == 0) return -1;
    if (log->count < log->capacity) {
        memcpy(&log->buffer[log->head], reading, sizeof(SensorReading));
        log->head = (log->head + 1) % log->capacity;
        log->count++;
        return 0;
    }
    if (!log->overwrite_oldest) return -1;
    memcpy(&log->buffer[log->head], reading, sizeof(SensorReading));
    log->head = (log->head + 1) % log->capacity;
    return 0;
}

const SensorReading *datalogger_get(const DataLogger *log, size_t i) {
    if (!log || i >= log->count) return NULL;
    size_t idx = (log->head + log->capacity - 1 - i) % log->capacity;
    return &log->buffer[idx];
}

size_t datalogger_count(const DataLogger *log) {
    return log ? log->count : 0;
}

void datalogger_clear(DataLogger *log) {
    if (!log) return;
    log->head = 0; log->count = 0;
}

double datalogger_average(const DataLogger *log, size_t n) {
    if (!log || n == 0 || n > log->count) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        const SensorReading *r = datalogger_get(log, i);
        if (r) sum += r->value;
    }
    return sum / (double)n;
}

double datalogger_min(const DataLogger *log, size_t n) {
    if (!log || n == 0 || n > log->count) return 0.0;
    double v = datalogger_get(log, 0)->value;
    for (size_t i = 1; i < n; i++) {
        double x = datalogger_get(log, i)->value;
        if (x < v) v = x;
    }
    return v;
}

double datalogger_max(const DataLogger *log, size_t n) {
    if (!log || n == 0 || n > log->count) return 0.0;
    double v = datalogger_get(log, 0)->value;
    for (size_t i = 1; i < n; i++) {
        double x = datalogger_get(log, i)->value;
        if (x > v) v = x;
    }
    return v;
}

int datalogger_variance(const DataLogger *log, size_t n,
                         double *out_mean, double *out_variance) {
    if (!log || n < 2 || n > log->count) return -1;
    double mean = datalogger_average(log, n);
    double s = 0.0;
    for (size_t i = 0; i < n; i++) {
        double d = datalogger_get(log, i)->value - mean;
        s += d * d;
    }
    if (out_mean) *out_mean = mean;
    if (out_variance) *out_variance = s/(double)(n-1);
    return 0;
}

/* === Digital Twin (L8) === */

void digital_twin_init(DigitalTwin *twin, const char *asset_id) {
    if (!twin) return;
    memset(twin, 0, sizeof(*twin));
    if (asset_id) {
        strncpy(twin->asset_id, asset_id, sizeof(twin->asset_id) - 1);
        twin->asset_id[sizeof(twin->asset_id) - 1] = '\0';
    }
}

void digital_twin_sync(DigitalTwin *twin, double pos, double vel,
                        double temp, double vib) {
    if (!twin) return;
    if (twin->is_running) {
        struct timespec _now2 = clock_now(); double dt = timespec_diff_sec(&_now2, &twin->last_sync);
        if (dt > 0.0) twin->acceleration = (vel - twin->velocity) / dt;
    }
    twin->position = pos; twin->velocity = vel;
    twin->temperature = temp; twin->vibration_rms = vib;
    twin->last_sync = clock_now();
}

int digital_twin_degradation_index(const DigitalTwin *twin,
                                    double bl, double fl) {
    if (!twin) return 0;
    double r = fl - bl;
    if (r <= 0.0) return 0;
    double ratio = (twin->vibration_rms - bl) / r * 100.0;
    if (ratio < 0) ratio = 0;
    if (ratio > 100) ratio = 100;
    return (int)(ratio + 0.5);
}

void digital_twin_predict_health(const DigitalTwin *twin,
                                  double *hp, double *rh) {
    if (!twin) return;
    if (hp) *hp = 100.0 - (double)twin->degradation_index;
    if (rh) {
        if (twin->degradation_index <= 0) *rh = 87600.0;
        else if (twin->degradation_index >= 100) *rh = 0.0;
        else {
            double rate = (double)twin->degradation_index /
                (twin->runtime_hours > 0 ? twin->runtime_hours : 1.0);
            *rh = (rate > 0) ? (100.0 - twin->degradation_index)/rate : 87600.0;
        }
    }
}

/* === Statistical Utilities === */

double stats_mean(const double *data, size_t n) {
    if (!data || n == 0) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) sum += data[i];
    return sum / (double)n;
}

double stats_stddev(const double *data, size_t n) {
    if (!data || n < 2) return 0.0;
    double m = stats_mean(data, n), s = 0.0;
    for (size_t i = 0; i < n; i++) { double d = data[i]-m; s += d*d; }
    return sqrt(s/(double)(n-1));
}

double stats_median(double *data, size_t n) {
    if (!data || n == 0) return 0.0;
    if (n == 1) return data[0];
    double *cpy = (double *)malloc(n*sizeof(double));
    if (!cpy) return data[0];
    memcpy(cpy, data, n*sizeof(double));
    qsort(cpy, n, sizeof(double), cmp_double);
    double m = (n%2==0) ? (cpy[n/2-1]+cpy[n/2])/2.0 : cpy[n/2];
    free(cpy);
    return m;
}

double stats_pearson_r(const double *x, const double *y, size_t n) {
    if (!x || !y || n < 2) return 0.0;
    double mx = stats_mean(x,n), my = stats_mean(y,n);
    double cov=0.0, sx=0.0, sy=0.0;
    for (size_t i=0; i<n; i++) {
        double dx=x[i]-mx, dy=y[i]-my;
        cov+=dx*dy; sx+=dx*dx; sy+=dy*dy;
    }
    double d = sqrt(sx)*sqrt(sy);
    return (d>0) ? cov/d : 0.0;
}

int stats_linear_regression(const double *x, const double *y, size_t n,
                             double *out_slope, double *out_intercept,
                             double *out_r_squared) {
    if (!x || !y || n < 2) return -1;
    double mx = stats_mean(x,n), my = stats_mean(y,n);
    double num=0.0, den=0.0;
    for (size_t i=0; i<n; i++) {
        double dx = x[i]-mx;
        num += dx*(y[i]-my);
        den += dx*dx;
    }
    if (den == 0.0) return -1;
    double slope = num/den, inter = my - slope*mx;
    if (out_slope) *out_slope = slope;
    if (out_intercept) *out_intercept = inter;
    if (out_r_squared) {
        double ssr=0.0, sst=0.0;
        for (size_t i=0; i<n; i++) {
            double pred = slope*x[i]+inter;
            ssr += (y[i]-pred)*(y[i]-pred);
            sst += (y[i]-my)*(y[i]-my);
        }
        *out_r_squared = (sst>0)? 1.0-ssr/sst : 0.0;
    }
    return 0;
}
