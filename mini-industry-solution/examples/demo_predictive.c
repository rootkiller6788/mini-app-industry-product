/**
 * demo_predictive.c - Predictive Maintenance Demo
 *
 * Demonstrates: Weibull reliability analysis, bearing fault diagnosis,
 * anomaly detection, RUL estimation, and maintenance optimization.
 *
 * Build: make examples
 * Run:   ./build/demo_predictive
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "predictive_maintenance.h"

int main(void) {
    printf("=== Predictive Maintenance Demo ===\n\n");

    /* 1. Weibull Reliability Analysis */
    printf("--- Weibull Reliability ---\n");
    double eta = 5000.0, beta = 2.5;
    printf("  Parameters: eta=%.0f h, beta=%.1f\n", eta, beta);
    printf("  MTTF: %.0f hours\n", weibull_mttf(eta, beta));
    printf("  Reliability at 1000h: %.4f\n",
           weibull_reliability(1000.0, eta, beta));
    printf("  Reliability at 5000h: %.4f\n",
           weibull_reliability(5000.0, eta, beta));
    printf("  Failure rate at 5000h: %.6f /h\n",
           weibull_failure_rate(5000.0, eta, beta));

    /* Estimate parameters from failure data */
    printf("\n  Parameter estimation from failure data:\n");
    double failures[] = {1200.0, 2300.0, 3100.0, 4500.0, 5800.0};
    double est_eta, est_beta;
    weibull_parameter_estimate(failures, 5, &est_eta, &est_beta);
    printf("  Estimated: eta=%.0f h, beta=%.2f\n", est_eta, est_beta);

    /* 2. Bearing Fault Diagnosis */
    printf("\n--- Bearing Fault Diagnosis ---\n");
    BearingGeometry bg;
    bearing_fault_frequencies(&bg, 1800.0, 9, 50.0, 10.0, 0.0);
    printf("  Shaft speed: %.0f RPM\n", bg.shaft_speed_rpm);
    printf("  BPFO: %.2f Hz (outer race)\n", bg.bpfo);
    printf("  BPFI: %.2f Hz (inner race)\n", bg.bpfi);
    printf("  BSF:  %.2f Hz (ball spin)\n", bg.bsf);
    printf("  FTF:  %.2f Hz (cage)\n", bg.ftf);

    int fault;
    bearing_fault_diagnose(0.05, 0.25, 0.03, 0.15, &fault);
    printf("\n  Diagnosis: %s\n",
           fault == 0 ? "No fault" :
           fault == 1 ? "Outer race defect" :
           fault == 2 ? "Inner race defect" : "Rolling element defect");

    /* 3. Anomaly Detection */
    printf("\n--- Vibration Anomaly Detection ---\n");
    double vib_data[] = {1.2,1.1,1.3,1.2,1.4,1.1,1.2,1.3,5.8,1.2};
    int indices[10], count;
    anomaly_zscore_detect(vib_data, 10, 3.0, indices, 10, &count);
    printf("  Z-score (>3 sigma): %d anomalies found\n", count);
    for (int i = 0; i < count; i++)
        printf("    Index %d: %.2f mm/s\n", indices[i],
               vib_data[indices[i]]);

    /* 4. RUL Estimation */
    printf("\n--- Remaining Useful Life Estimation ---\n");
    double trend[] = {0.0,1.2,2.1,2.8,3.9,4.8,5.5,6.3,7.1,8.0};
    double timestamps[] = {0,100,200,300,400,500,600,700,800,900};
    double rate = rul_degradation_rate_estimate(trend, 10, timestamps);
    double rul = rul_linear_estimate(8.0, 25.0, rate);
    printf("  Degradation rate: %.4f units/hour\n", rate);
    printf("  Current: 8.0, Failure threshold: 25.0\n");
    printf("  RUL: %.0f hours\n", rul);

    /* 5. Maintenance Optimization */
    printf("\n--- Maintenance Cost Optimization ---\n");
    double optimal = maintenance_optimal_interval(
        500.0, 5000.0, 5000.0, 3.0);
    printf("  Preventive cost: $500, Corrective cost: $5000\n");
    printf("  Optimal PM interval: %.0f hours\n", optimal);
    printf("  Cost at 1000h: $%.2f/h\n",
           maintenance_cost_per_hour(1000.0, 500.0, 5000.0, 5000.0, 3.0));
    printf("  Cost at %.0fh: $%.2f/h\n",
           optimal, maintenance_cost_per_hour(
               optimal, 500.0, 5000.0, 5000.0, 3.0));

    /* 6. Condition Baseline */
    printf("\n--- Condition Baseline ---\n");
    double baseline_mean, baseline_std, warning, alarm_limit;
    condition_baseline_compute(vib_data, 10,
                                &baseline_mean, &baseline_std,
                                &warning, &alarm_limit);
    printf("  Baseline: %.2f +/- %.2f mm/s\n",
           baseline_mean, baseline_std);
    printf("  Warning:  %.2f mm/s (mean + 2*sigma)\n", warning);
    printf("  Alarm:    %.2f mm/s (mean + 3*sigma)\n", alarm_limit);
    printf("  Health score (5.8): %.0f%%\n",
           health_score_compute(5.8, baseline_mean, alarm_limit));

    printf("\n=== Predictive Maintenance Demo Complete ===\n");
    return 0;
}
