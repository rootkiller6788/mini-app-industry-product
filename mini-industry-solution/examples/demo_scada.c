/**
 * demo_scada.c - Industrial SCADA Data Pipeline Demo
 *
 * Demonstrates: sensor acquisition, data logging, alarm management,
 * PID control loop, and digital twin synchronization.
 *
 * Build: make examples
 * Run:   ./build/demo_scada
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "industrial_core.h"
#include "plc_protocol.h"

int main(void) {
    printf("=== Industrial SCADA Data Pipeline Demo ===\n\n");

    /* 1. Sensor acquisition */
    printf("--- Sensor Acquisition ---\n");
    SensorReading temp_sensor;
    sensor_reading_init(&temp_sensor, "TIC-1001", SIG_THERMOCOUPLE_K);
    temp_sensor.raw_value = 4.096;
    temp_sensor.value = thermocouple_k_mv_to_celsius(4.096);
    printf("  TIC-1001: %.2f mV -> %.1f deg C\n",
           temp_sensor.raw_value, temp_sensor.value);

    SensorReading flow_sensor;
    sensor_reading_init(&flow_sensor, "FIC-1001", SIG_ANALOG_4_20MA);
    flow_sensor.raw_value = 12.0;
    flow_sensor.value = analog_normalize(12.0, 4.0, 20.0, 0.0, 100.0);
    printf("  FIC-1001: %.1f mA -> %.1f %%\n",
           flow_sensor.raw_value, flow_sensor.value);

    /* 2. PID Control Loop */
    printf("\n--- PID Control Loop ---\n");
    ControlLoop loop;
    control_loop_init(&loop, "FIC-1001", 0.0, 100.0, "%",
                       2.0, 0.5, 0.1, 0.1, 100.0);
    printf("  Controller: Kp=%.2f Ki=%.2f Kd=%.2f Ts=%.1fs\n",
           loop.kp, loop.ki, loop.kd, loop.sample_time);

    double measurement = 45.0;
    for (int cycle = 0; cycle < 5; cycle++) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        double output = control_loop_step(&loop, 50.0, measurement);
        printf("  Cycle %d: PV=%.1f SP=50.0 Error=%.1f Output=%.1f%%\n",
               cycle, measurement, loop.pv.error, output);
        measurement += 1.5;
    }

    /* 3. Alarm Management */
    printf("\n--- Alarm Management ---\n");
    AlarmCondition alarm;
    memset(&alarm, 0, sizeof(alarm));
    snprintf(alarm.tag, sizeof(alarm.tag), "TIC-1001_HI");
    alarm.trip_point = 100.0;
    alarm.deadband = 2.0;
    alarm.priority = ALARM_HIGH;
    snprintf(alarm.message, sizeof(alarm.message),
             "Temperature high alarm");

    double test_values[] = {95.0, 101.0, 102.0, 98.0};
    AlarmPriority prio;
    for (int i = 0; i < 4; i++) {
        int active = alarm_evaluate(&alarm, test_values[i], &prio);
        printf("  T=%.1f deg C: alarm=%s priority=%d\n",
               test_values[i], active ? "ACTIVE" : "CLEAR", prio);
    }

    /* 4. Data Logger */
    printf("\n--- Data Logger (Circular Buffer) ---\n");
    SensorReading buffer[10];
    DataLogger logger;
    datalogger_init(&logger, buffer, 10, 1);
    for (int i = 0; i < 12; i++) {
        SensorReading s;
        sensor_reading_init(&s, "TIC-1001", SIG_THERMOCOUPLE_K);
        s.value = 20.0 + (double)i * 2.0;
        datalogger_append(&logger, &s);
    }
    printf("  Stored: %zu readings\n", datalogger_count(&logger));
    printf("  Average (last 5): %.2f deg C\n",
           datalogger_average(&logger, 5));

    /* 5. Kalman Filter Sensor Fusion */
    printf("\n--- Kalman Filter Sensor Fusion ---\n");
    KalmanFilter1D kf;
    kalman1d_init(&kf, 25.0, 4.0, 0.01, 1.0);
    double noisy_readings[] = {25.1, 25.3, 24.8, 25.5, 24.9, 25.2};
    for (int i = 0; i < 6; i++) {
        double est = kalman1d_update(&kf, noisy_readings[i]);
        printf("  Reading: %.1f -> Filtered: %.2f\n",
               noisy_readings[i], est);
    }

    /* 6. Modbus Register Map */
    printf("\n--- Modbus Register Map ---\n");
    ModbusRegisterMap map[3] = {
        {"Temperature", 0, 1, 0.1, 0.0, "deg C", 0},
        {"Pressure",    1, 1, 0.01, 0.0, "bar", 0},
        {"Flow_Rate",   2, 1, 1.0, 0.0, "L/min", 0}
    };
    for (int i = 0; i < 3; i++) {
        uint16_t addr;
        double scale, offset;
        modbus_register_map_lookup(map, 3, map[i].name,
                                    &addr, &scale, &offset);
        printf("  %s: addr=%u scale=%.2f offset=%.2f [%s]\n",
               map[i].name, addr, scale, offset, map[i].unit);
    }

    /* 7. Digital Twin */
    printf("\n--- Digital Twin ---\n");
    DigitalTwin twin;
    digital_twin_init(&twin, "PUMP-A-01");
    digital_twin_sync(&twin, 0.75, 1450.0, 42.0, 2.3);
    twin.runtime_hours = 5000.0;
    twin.degradation_index = digital_twin_degradation_index(
        &twin, 1.0, 7.0);
    double health, remaining;
    digital_twin_predict_health(&twin, &health, &remaining);
    printf("  Asset: %s\n", twin.asset_id);
    printf("  Vibration: %.2f mm/s RMS\n", twin.vibration_rms);
    printf("  Degradation: %d%%\n", twin.degradation_index);
    printf("  Health: %.0f%%\n", health);
    printf("  Est. remaining life: %.0f hours\n", remaining);

    printf("\n=== SCADA Demo Complete ===\n");
    return 0;
}
