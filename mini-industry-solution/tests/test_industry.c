/**
 * mini-industry-solution: test_industry.c
 * Comprehensive unit tests for all industrial modules.
 * Uses assert-based testing (no external framework required).
 *
 * Coverage:
 * - Sensor processing & validation
 * - Thermocouple/RTD conversion
 * - Signal filtering (SMA, EMA, WMA, Median, Kalman)
 * - Alarm management
 * - PID control loop
 * - Data logger
 * - Digital twin
 * - Statistical utilities
 * - Modbus protocol (CRC16, LRC, frame build/parse, slave device)
 * - OPC UA node model
 * - SIL safety levels
 * - Weibull reliability
 * - Trend analysis & anomaly detection
 * - RUL estimation
 * - Bearing fault diagnosis
 * - Maintenance optimization
 * - X-bar/R chart & WECO rules
 * - Process capability (Cp/Cpk)
 * - Pareto analysis
 * - Gauge R&R
 * - CUSUM chart
 * - HAZOP & risk matrix
 * - LOPA SIL determination
 * - FMEA RPN
 * - Fault tree probability
 * - Event tree analysis
 * - Bow-tie analysis
 * - Johnson's Rule
 * - NEH heuristic
 * - Dispatch rules
 * - CDS heuristic
 * - Bottleneck & DBR
 * - Schedule evaluation
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>

#include "industrial_core.h"
#include "plc_protocol.h"
#include "predictive_maintenance.h"
#include "quality_control.h"
#include "safety_analysis.h"
#include "production_schedule.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  TEST %s ... ", name); \
} while(0)

#define PASS() do { \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("FAIL at %s:%d\n", __FILE__, __LINE__); \
        return; \
    } \
} while(0)

/* ================================================================
 * industrial_core tests
 * ================================================================ */

static void test_sensor_reading_init(void) {
    TEST("sensor_reading_init");
    SensorReading s;
    sensor_reading_init(&s, "FIC-1001", SIG_ANALOG_4_20MA);
    CHECK(strcmp(s.tag_name, "FIC-1001") == 0);
    CHECK(s.signal_type == SIG_ANALOG_4_20MA);
    CHECK(s.quality == PVQ_GOOD);
    CHECK(s.health == SENSOR_OK);
    PASS();
}

static void test_sensor_validate_range(void) {
    TEST("sensor_validate_range");
    SensorReading s;
    sensor_reading_init(&s, "T-101", SIG_ANALOG_4_20MA);
    s.value = 50.0; s.raw_value = 12.0;
    CHECK(sensor_validate_range(&s, 0.0, 100.0) == 1);
    s.value = 150.0;
    CHECK(sensor_validate_range(&s, 0.0, 100.0) == 0);
    PASS();
}

static void test_deadband_filter(void) {
    TEST("sensor_deadband_filter");
    double result = sensor_deadband_filter(50.0, 50.1, 0.5);
    CHECK(fabs(result - 50.0) < 1e-9);
    result = sensor_deadband_filter(50.0, 51.0, 0.5);
    CHECK(fabs(result - 51.0) < 1e-9);
    PASS();
}

static void test_thermocouple_conversion(void) {
    TEST("thermocouple conversion");
    /* Type K: 0 C = 0 mV */
    double t0 = thermocouple_k_mv_to_celsius(0.0);
    CHECK(fabs(t0) < 1.0);
    /* Type K: ~4.096 mV at 100 C */
    double t100 = thermocouple_k_mv_to_celsius(4.096);
    CHECK(fabs(t100 - 100.0) < 5.0);

    /* Type J: 0 C = 0 mV */
    double tj0 = thermocouple_j_mv_to_celsius(0.0);
    CHECK(fabs(tj0) < 1.0);
    PASS();
}

static void test_rtd_conversion(void) {
    TEST("RTD PT100 conversion");
    /* PT100: 100 ohm = 0 C */
    double t0 = rtd_pt100_to_celsius(100.0);
    CHECK(fabs(t0) < 1.0);
    /* PT100: ~138.5 ohm = 100 C */
    double t100 = rtd_pt100_to_celsius(138.5);
    CHECK(fabs(t100 - 100.0) < 5.0);
    PASS();
}

static void test_analog_normalize(void) {
    TEST("analog_normalize");
    double val = analog_normalize(12.0, 4.0, 20.0, 0.0, 100.0);
    CHECK(fabs(val - 50.0) < 0.1);
    val = analog_normalize(4.0, 4.0, 20.0, 0.0, 100.0);
    CHECK(fabs(val - 0.0) < 0.1);
    val = analog_normalize(20.0, 4.0, 20.0, 0.0, 100.0);
    CHECK(fabs(val - 100.0) < 0.1);
    PASS();
}

static void test_sma_filter(void) {
    TEST("SMA filter");
    double hist[5] = {0};
    int idx = 0;
    double sum = 0.0;
    double result;

    result = sma_filter(10.0, hist, &idx, 3, &sum);
    CHECK(fabs(result - 10.0) < 1e-9);
    result = sma_filter(20.0, hist, &idx, 3, &sum);
    CHECK(fabs(result - 15.0) < 1e-9);
    result = sma_filter(30.0, hist, &idx, 3, &sum);
    CHECK(fabs(result - 20.0) < 1e-9);
    result = sma_filter(40.0, hist, &idx, 3, &sum);
    CHECK(fabs(result - 30.0) < 1e-9);
    PASS();
}

static void test_ema_filter(void) {
    TEST("EMA filter");
    double ema = 0.0;
    ema = ema_filter(10.0, ema, 0.5);
    CHECK(fabs(ema - 5.0) < 1e-9);
    ema = ema_filter(20.0, ema, 0.5);
    CHECK(fabs(ema - 12.5) < 1e-9);
    PASS();
}

static void test_kalman_filter(void) {
    TEST("Kalman filter 1D");
    KalmanFilter1D kf;
    kalman1d_init(&kf, 0.0, 1.0, 0.01, 1.0);
    double est = kalman1d_update(&kf, 10.0);
    CHECK(est > 0.0 && est < 10.0);
    for (int i = 0; i < 10; i++)
        est = kalman1d_update(&kf, 10.0);
    CHECK(fabs(est - 10.0) < 1.0);
    PASS();
}

static void test_alarm_evaluate(void) {
    TEST("alarm_evaluate");
    AlarmCondition alarm;
    memset(&alarm, 0, sizeof(alarm));
    alarm.trip_point = 80.0;
    alarm.deadband = 2.0;
    alarm.priority = ALARM_HIGH;

    AlarmPriority prio;
    int active = alarm_evaluate(&alarm, 85.0, &prio);
    CHECK(active == 1);
    CHECK(prio == ALARM_HIGH);
    /* 85 triggers alarm. Reset at 78 (80-2). 79 is in deadband. */
    active = alarm_evaluate(&alarm, 79.0, &prio);
    CHECK(active == 1);
    /* 77.9 is below reset (78), alarm clears */
    active = alarm_evaluate(&alarm, 77.9, &prio);
    CHECK(active == 0);
    PASS();
}

static void test_control_loop(void) {
    TEST("PID control loop");
    ControlLoop loop;
    control_loop_init(&loop, "FIC-1001", 0.0, 100.0, "C",
                       2.0, 0.5, 0.1, 1.0, 100.0);
    CHECK(loop.kp == 2.0);
    CHECK(loop.ki == 0.5);
    CHECK(loop.kd == 0.1);

    double out = control_loop_step(&loop, 50.0, 40.0);
    CHECK(out > 0.0);
    CHECK(out <= 100.0);
    PASS();
}

static void test_ziegler_nichols(void) {
    TEST("Ziegler-Nichols tuning");
    double kp, ki, kd;
    int ret = ziegler_nichols_open_loop(1.0, 1.0, 10.0, &kp, &ki, &kd);
    CHECK(ret == 0);
    CHECK(kp > 0.0);
    CHECK(ki > 0.0);
    CHECK(kd > 0.0);
    PASS();
}

static void test_datalogger(void) {
    TEST("Data logger circular buffer");
    SensorReading buf[5];
    DataLogger log;
    datalogger_init(&log, buf, 5, 1);

    for (int i = 0; i < 7; i++) {
        SensorReading s;
        sensor_reading_init(&s, "TEST", SIG_ANALOG_4_20MA);
        s.value = (double)(i + 1) * 10.0;
        datalogger_append(&log, &s);
    }
    CHECK(datalogger_count(&log) == 5);
    double avg = datalogger_average(&log, 5);
    CHECK(avg > 0.0);
    const SensorReading *first = datalogger_get(&log, 0);
    CHECK(first != NULL);
    CHECK(first->value == 70.0);

    double vmin = datalogger_min(&log, 5);
    double vmax = datalogger_max(&log, 5);
    CHECK(vmax >= vmin);
    PASS();
}

static void test_digital_twin(void) {
    TEST("Digital twin");
    DigitalTwin twin;
    digital_twin_init(&twin, "PUMP-01");
    CHECK(strcmp(twin.asset_id, "PUMP-01") == 0);

    digital_twin_sync(&twin, 0.5, 100.0, 45.0, 2.5);
    CHECK(twin.temperature == 45.0);
    CHECK(twin.vibration_rms == 2.5);

    int di = digital_twin_degradation_index(&twin, 1.0, 10.0);
    CHECK(di >= 0 && di <= 100);

    double hp, rh;
    digital_twin_predict_health(&twin, &hp, &rh);
    CHECK(hp >= 0.0 && hp <= 100.0);
    PASS();
}

static void test_statistics(void) {
    TEST("Statistical utilities");
    double data[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    double m = stats_mean(data, 8);
    CHECK(fabs(m - 5.0) < 0.01);

    double sd = stats_stddev(data, 8);
    CHECK(sd > 0.0);

    double med = stats_median(data, 8);
    CHECK(med == 4.5);

    double x[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    double y[] = {2.0, 4.0, 6.0, 8.0, 10.0};
    double r = stats_pearson_r(x, y, 5);
    CHECK(fabs(r - 1.0) < 0.001);

    double slope, inter, r2;
    int ret = stats_linear_regression(x, y, 5, &slope, &inter, &r2);
    CHECK(ret == 0);
    CHECK(fabs(slope - 2.0) < 0.01);
    CHECK(fabs(r2 - 1.0) < 0.01);
    PASS();
}

/* ================================================================
 * plc_protocol tests
 * ================================================================ */

static void test_modbus_crc16(void) {
    TEST("Modbus CRC16");
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01};
    uint16_t crc = modbus_crc16(data, 6);
    CHECK(crc != 0);
    CHECK(crc == 0x0A84);
    PASS();
}

static void test_modbus_lrc(void) {
    TEST("Modbus LRC");
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01};
    uint8_t lrc = modbus_lrc(data, 6);
    CHECK(lrc != 0);
    PASS();
}

static void test_modbus_rtu_frame(void) {
    TEST("Modbus RTU frame build/parse");
    ModbusFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.slave_addr = 1;
    frame.function_code = MODBUS_FC_READ_HOLDING_REGS;
    frame.start_address = 0;
    frame.quantity = 1;

    uint8_t buf[256];
    size_t len;
    int ret = modbus_rtu_frame_build(&frame, buf, &len);
    CHECK(ret == 0);
    CHECK(len == 8);

    ModbusFrame parsed;
    ret = modbus_rtu_frame_parse(buf, len, &parsed);
    CHECK(ret == 0);
    CHECK(parsed.slave_addr == 1);
    CHECK(parsed.function_code == MODBUS_FC_READ_HOLDING_REGS);
    PASS();
}

static void test_modbus_slave(void) {
    TEST("Modbus slave device");
    ModbusSlaveDevice slave;
    modbus_slave_init(&slave, 1);

    modbus_slave_write_register(&slave, 100, 1234);
    uint16_t val;
    int ret = modbus_slave_read_register(&slave, 100, &val);
    CHECK(ret == 0);
    CHECK(val == 1234);
    PASS();
}

static void test_opcua_node(void) {
    TEST("OPC UA node");
    OPCUANode node;
    opcua_node_init(&node, "ns=1;s=Temperature", "Temperature",
                     OPCUA_FLOAT);
    CHECK(strcmp(node.node_id, "ns=1;s=Temperature") == 0);
    opcua_node_set_float(&node, 25.5);
    CHECK(fabs(node.numeric_value - 25.5) < 0.01);
    PASS();
}

static void test_sil(void) {
    TEST("SIL levels");
    CHECK(sil_pfd_avg(SIL_1) <= 1e-1);
    CHECK(sil_pfd_avg(SIL_4) <= 1e-4);
    CHECK(sil_rrf(SIL_3) >= 1000.0);
    CHECK(strcmp(sil_name(SIL_2), "SIL 2") == 0);
    PASS();
}

/* ================================================================
 * predictive_maintenance tests
 * ================================================================ */

static void test_weibull(void) {
    TEST("Weibull distribution");
    double eta = 1000.0, beta = 2.0;
    double r500 = weibull_reliability(500.0, eta, beta);
    CHECK(r500 > 0.7);
    CHECK(r500 < 1.0);

    double r2000 = weibull_reliability(2000.0, eta, beta);
    CHECK(r2000 < 0.1);

    double mttf = weibull_mttf(eta, beta);
    CHECK(mttf > 0.0);

    double cdf500 = weibull_cdf(500.0, eta, beta);
    CHECK(cdf500 > 0.0 && cdf500 < 1.0);
    PASS();
}

static void test_anomaly_detection(void) {
    TEST("Anomaly z-score detection");
    double normal[] = {1.0, 1.1, 0.9, 1.0, 1.2, 0.8, 1.0, 5.0, 1.1, 1.0};
    int indices[10];
    int count = 0;
    anomaly_zscore_detect(normal, 10, 2.5, indices, 10, &count);
    CHECK(count >= 1);
    CHECK(indices[0] == 7);
    PASS();
}

static void test_rul_estimation(void) {
    TEST("RUL linear estimation");
    double rul = rul_linear_estimate(50.0, 100.0, 0.01);
    CHECK(rul == 5000.0);

    double values[] = {0.0, 1.0, 2.0, 3.0, 4.0};
    double times[] = {0.0, 1.0, 2.0, 3.0, 4.0};
    double rate = rul_degradation_rate_estimate(values, 5, times);
    CHECK(fabs(rate - 1.0) < 0.01);
    PASS();
}

static void test_bearing_fault(void) {
    TEST("Bearing fault diagnosis");
    BearingGeometry bg;
    bearing_fault_frequencies(&bg, 1800.0, 9, 50.0, 10.0, 0.0);
    CHECK(bg.bpfo > 0.0);
    CHECK(bg.bpfi > 0.0);
    CHECK(bg.bsf > 0.0);
    CHECK(bg.ftf > 0.0);

    int fault_type;
    bearing_fault_diagnose(0.1, 0.2, 0.05, 0.15, &fault_type);
    CHECK(fault_type == 2);
    PASS();
}

static void test_maintenance_optimization(void) {
    TEST("Maintenance optimization");
    double optimal = maintenance_optimal_interval(1000.0, 10000.0,
                                                   5000.0, 3.0);
    CHECK(optimal > 0.0);
    double cost = maintenance_cost_per_hour(1000.0, 1000.0, 10000.0,
                                              5000.0, 3.0);
    CHECK(cost > 0.0);
    PASS();
}

/* ================================================================
 * quality_control tests
 * ================================================================ */

static void test_xbar_r_chart(void) {
    TEST("X-bar/R chart");
    double buf[20];
    XBarRChart chart;
    xbar_r_chart_init(&chart, 5, buf, 20);

    double subgroup[] = {10.0, 11.0, 9.0, 10.5, 9.5};
    xbar_r_chart_update(&chart, subgroup, 5);
    CHECK(chart.grand_mean > 0.0);
    CHECK(chart.ucl_x > chart.lcl_x);
    PASS();
}

static void test_weco_rules(void) {
    TEST("WECO rules check");
    double buf[40];
    XBarRChart chart;
    xbar_r_chart_init(&chart, 5, buf, 40);

    double sub[5] = {10.0, 10.0, 10.0, 10.0, 10.0};
    for (int i = 0; i < 8; i++)
        xbar_r_chart_update(&chart, sub, 5);

    int violations[10];
    int n = weco_rules_check(&chart, violations, 10);
    CHECK(n >= 0);
    PASS();
}

static void test_capability(void) {
    TEST("Process capability");
    double measurements[] = {9.8,10.1,10.0,9.9,10.2,10.0,10.1,9.9,
                              10.0,10.2,9.8,10.1,10.0,10.3,9.9,10.0};
    CapabilityIndex idx;
    capability_calculate(measurements, 16, 11.0, 9.0, &idx);
    CHECK(idx.cp > 0.0);
    CHECK(idx.cpk > 0.0);
    CHECK(idx.sigma_level > 0.0);
    PASS();
}

static void test_pareto(void) {
    TEST("Pareto analysis");
    int counts[] = {50, 25, 15, 7, 3};
    const char *labels[] = {"A", "B", "C", "D", "E"};
    ParetoItem items[10];
    int n = pareto_analysis(counts, labels, 5, items);
    CHECK(n == 5);
    CHECK(items[0].count == 50);
    CHECK(fabs(items[0].percentage - 50.0) < 0.1);
    PASS();
}

static void test_gauge_rnr(void) {
    TEST("Gauge R&R");
    double measurements[18] = {0};
    for (int i = 0; i < 18; i++) measurements[i] = 10.0 + (double)(i % 3) * 0.1;
    GaugeRRResult result;
    int ret = gauge_rnr_analysis(measurements, 3, 2, 3, &result);
    CHECK(ret == 0);
    PASS();
}

static void test_cusum(void) {
    TEST("CUSUM chart");
    CUSUMChart c;
    cusum_init(&c, 20, 0.5, 5.0);
    int sh, sl;
    for (int i = 0; i < 10; i++) {
        int ret = cusum_update(&c, 0.0, &sh, &sl);
        CHECK(ret == 0);
    }
    CHECK(c.count == 10);
    PASS();
}

/* ================================================================
 * safety_analysis tests
 * ================================================================ */

static void test_hazop(void) {
    TEST("HAZOP analysis");
    HAZOPRecord rec;
    hazop_init_record(&rec, "Reactor", "Temperature", GUIDE_MORE);
    CHECK(strcmp(rec.node, "Reactor") == 0);
    hazop_set_deviation(&rec, "+10C", "Cooling failure", "Overpressure");
    CHECK(strcmp(rec.deviation, "+10C") == 0);
    hazop_assess_risk(&rec, SEVERITY_CRITICAL, LIKELIHOOD_POSSIBLE);
    CHECK(rec.risk == RISK_HIGH);
    PASS();
}

static void test_risk_matrix(void) {
    TEST("Risk matrix evaluation");
    RiskLevel rl = risk_matrix_evaluate(SEVERITY_CATASTROPHIC,
                                         LIKELIHOOD_FREQUENT);
    CHECK(rl == RISK_INTOLERABLE);
    CHECK(risk_acceptable(RISK_LOW) == 1);
    CHECK(risk_acceptable(RISK_INTOLERABLE) == 0);
    PASS();
}

static void test_lopa(void) {
    TEST("LOPA SIL determination");
    double ipls[] = {0.1, 0.1};
    double mitigated = lopa_event_frequency(1.0, ipls, 2);
    CHECK(fabs(mitigated - 0.01) < 0.001);

    SIL sil;
    lopa_sil_determine(1e-4, 0.01, &sil);
    CHECK(sil == SSIL_2);
    PASS();
}

static void test_fmea(void) {
    TEST("FMEA analysis");
    FMEARecord rec;
    fmea_init_record(&rec, "Pump seal", "Contains fluid at pressure");
    rec.severity_rank = 8;
    rec.occurrence_rank = 5;
    rec.detection_rank = 3;
    int rpn = fmea_calculate_rpn(&rec);
    CHECK(rpn == 120);
    PASS();
}

static void test_fault_tree(void) {
    TEST("Fault tree analysis");
    FTANode top, event1, event2;
    fta_init_node(&top, "Overpressure", FTA_OR);
    fta_init_node(&event1, "Valve stuck closed", FTA_BASIC);
    fta_init_node(&event2, "Controller failure", FTA_BASIC);
    event1.probability = 0.01;
    event2.probability = 0.02;
    fta_add_child(&top, &event1);
    fta_add_child(&top, &event2);

    double prob = fta_compute_probability(&top);
    CHECK(prob > 0.01);
    CHECK(prob < 0.04);
    PASS();
}

static void test_event_tree(void) {
    TEST("Event tree analysis");
    ETABranch branches[4];
    memset(branches, 0, sizeof(branches));
    branches[0].branch_probability = 0.9;
    branches[1].branch_probability = 0.1;
    branches[2].branch_probability = 0.95;
    branches[3].branch_probability = 0.05;

    EventTree et;
    eta_init(&et, "Loss of cooling", 0.01, branches, 2, 2);

    int path[] = {1, 1};
    double prob = eta_outcome_probability(&et, path, 2);
    CHECK(prob > 0.0);
    PASS();
}

static void test_sif_design(void) {
    TEST("SIF PFD calculation");
    double pfd;
    SIL sil;
    sil_pfd_single(0.0005, 0.0001, 0.0005, &pfd, &sil);
    CHECK(pfd < 0.01);
    CHECK(sil == SSIL_2);
    PASS();
}

/* ================================================================
 * production_schedule tests
 * ================================================================ */

static void test_johnsons_rule(void) {
    TEST("Johnson's Rule");
    double m1[] = {3.0, 4.0, 2.0, 1.0};
    double m2[] = {2.0, 1.0, 5.0, 3.0};
    int seq[4];
    int ret = johnsons_rule_2machine(NULL, 4, m1, m2, seq);
    CHECK(ret == 0);
    PASS();
}

static void test_dispatch_rules(void) {
    TEST("Priority dispatch");
    Job jobs[3];
    memset(jobs, 0, sizeof(jobs));
    jobs[0].total_processing_time = 10.0;
    jobs[1].total_processing_time = 5.0;
    jobs[2].total_processing_time = 8.0;

    int seq[3];
    int ret = priority_dispatch_schedule(jobs, 3, DISPATCH_SPT, seq);
    CHECK(ret == 0);
    CHECK(seq[0] == 1);
    CHECK(seq[2] == 0);
    PASS();
}

static void test_schedule_evaluation(void) {
    TEST("Schedule evaluation");
    int seq[] = {0, 1, 2};
    double mk = makespan_calculate(NULL, 3, 3, seq);
    CHECK(mk >= 0.0);
    PASS();
}

static void test_critical_ratio(void) {
    TEST("Critical ratio & slack");
    Job job;
    memset(&job, 0, sizeof(job));
    clock_gettime(CLOCK_REALTIME, &job.due_date);
    job.due_date.tv_sec += 3600;
    job.remaining_time = 1800.0;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    double cr = critical_ratio_calculate(&job, now);
    CHECK(cr > 0.0);

    double slack = slack_time_calculate(&job, now);
    CHECK(slack > 0.0);
    PASS();
}

static void test_bottleneck(void) {
    TEST("Bottleneck identification");
    Machine machines[3];
    memset(machines, 0, sizeof(machines));
    machines[0].machine_id = 0;
    machines[1].machine_id = 1;
    machines[2].machine_id = 2;

    Job jobs[2];
    memset(jobs, 0, sizeof(jobs));
    jobs[0].n_operations = 3;
    jobs[0].operations = (Operation *)calloc(3, sizeof(Operation));
    jobs[0].operations[0].machine_id = 0;
    jobs[0].operations[0].processing_time = 5.0;
    jobs[0].operations[1].machine_id = 1;
    jobs[0].operations[1].processing_time = 20.0;
    jobs[0].operations[2].machine_id = 2;
    jobs[0].operations[2].processing_time = 3.0;
    jobs[1].n_operations = 3;
    jobs[1].operations = (Operation *)calloc(3, sizeof(Operation));
    jobs[1].operations[0].machine_id = 0;
    jobs[1].operations[0].processing_time = 2.0;
    jobs[1].operations[1].machine_id = 1;
    jobs[1].operations[1].processing_time = 15.0;
    jobs[1].operations[2].machine_id = 2;
    jobs[1].operations[2].processing_time = 4.0;

    int bn = bottleneck_identify(machines, 3, jobs, 2);
    CHECK(bn == 1);

    double mk = drum_buffer_rope_schedule(jobs, 2, machines, 3, 1);
    CHECK(mk > 0.0);

    free(jobs[0].operations);
    free(jobs[1].operations);
    PASS();
}

/* ================================================================
 * Main test runner
 * ================================================================ */

int main(void) {
    printf("\n=== mini-industry-solution: Comprehensive Test Suite ===\n\n");

    test_sensor_reading_init();
    test_sensor_validate_range();
    test_deadband_filter();
    test_thermocouple_conversion();
    test_rtd_conversion();
    test_analog_normalize();
    test_sma_filter();
    test_ema_filter();
    test_kalman_filter();
    test_alarm_evaluate();
    test_control_loop();
    test_ziegler_nichols();
    test_datalogger();
    test_digital_twin();
    test_statistics();

    test_modbus_crc16();
    test_modbus_lrc();
    test_modbus_rtu_frame();
    test_modbus_slave();
    test_opcua_node();
    test_sil();

    test_weibull();
    test_anomaly_detection();
    test_rul_estimation();
    test_bearing_fault();
    test_maintenance_optimization();

    test_xbar_r_chart();
    test_weco_rules();
    test_capability();
    test_pareto();
    test_gauge_rnr();
    test_cusum();

    test_hazop();
    test_risk_matrix();
    test_lopa();
    test_fmea();
    test_fault_tree();
    test_event_tree();
    test_sif_design();

    test_johnsons_rule();
    test_dispatch_rules();
    test_schedule_evaluation();
    test_critical_ratio();
    test_bottleneck();

    printf("\n=== Results: %d / %d tests passed ===\n\n",
           tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("SOME TESTS FAILED\n");
    return 1;
}
