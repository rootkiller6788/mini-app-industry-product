/* example_spc.c - SPC & Quality Control Demo
 * Demonstrates: X-bar/R chart, process capability (Cp/Cpk),
 * Western Electric rules, Pareto analysis, 5-Why RCA.
 *
 * Usage: gcc -I../include example_spc.c -L../build -lsmartmfg -lm -o spc_demo && ./spc_demo
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "smart_manufacturing.h"
#include "quality_control.h"

int main(void) {
    printf("=== SPC & Quality Control Demo ===\n\n");

    /* ---- X-bar/R Chart ---- */
    printf("--- X-bar/R Control Chart: Shaft Diameter (mm) ---\n");
    SMF_ControlChart chart;
    smf_spc_init_xbar_r(&chart, "Shaft Diameter",
                         "Outer Diameter (mm) - Spec: 10.0 +/- 0.1",
                         10.10f, 9.90f, 10.00f, 5);

    /* Simulate 20 subgroups of measurements */
    SMF_SPCSubgroup subgroups[20];
    for (int i = 0; i < 20; i++) {
        /* Generate realistic measurements around nominal with some variation */
        float base = 10.00f + (float)(rand() % 100 - 50) / 1000.0f;
        float meas[5];
        for (int j = 0; j < 5; j++) {
            meas[j] = base + (float)(rand() % 40 - 20) / 1000.0f;
        }
        smf_spc_add_subgroup(&subgroups[i], meas, 5);
    }

    smf_spc_compute_limits(&chart, subgroups, 20);

    printf("Control Limits:\n");
    printf("  X-bar: CL=%.4f, UCL=%.4f, LCL=%.4f\n",
           chart.cl, chart.ucl, chart.lcl);
    printf("  R:     CL=%.4f, UCL=%.4f, LCL=%.4f\n",
           chart.sigma * chart.d2, chart.ucl_r, chart.lcl_r);

    /* Check WE rules */
    float xbar_values[20], range_values[20];
    for (int i = 0; i < 20; i++) {
        xbar_values[i] = subgroups[i].x_bar;
        range_values[i] = subgroups[i].range;
    }

    int we_rules = smf_weco_check_points(xbar_values, range_values, 20,
                                          chart.cl, chart.ucl, chart.lcl,
                                          chart.sigma);
    printf("\nWestern Electric Rule Violations:\n");
    for (int bit = 0; bit < 7; bit++) {
        if (we_rules & (1 << bit)) {
            printf("  %s\n", smf_weco_rule_name(bit));
        }
    }
    if (we_rules == 0) printf("  None - process is in control\n");

    /* ---- Process Capability ---- */
    printf("\n--- Process Capability ---\n");
    printf("  Cp  = %.3f  (Potential Capability)\n", chart.cp);
    printf("  Cpk = %.3f  (Actual Capability)\n", chart.cpk);
    printf("  Rating: %s\n", smf_capability_rating(chart.cpk));
    printf("  Status: %s\n", chart.is_capable ? "CAPABLE (Cpk >= 1.33)" : "NOT CAPABLE");

    /* Direct Cp/Cpk calculation */
    float cp_manual = smf_calc_cp(10.10f, 9.90f, 0.02f);
    float cpk_manual = smf_calc_cpk(10.10f, 9.90f, 10.01f, 0.02f);
    printf("\n  Manual Cp  (sigma=0.02): %.3f\n", cp_manual);
    printf("  Manual Cpk (mean=10.01): %.3f\n", cpk_manual);

    /* ---- Pareto Analysis ---- */
    printf("\n--- Pareto Analysis: Defect Categories ---\n");
    int defects[] = {45, 23, 12, 8, 5, 3, 2, 2};
    const char *names[] = {"Surface Scratch", "Dimensional", "Burr",
                           "Rust/Corrosion", "Thread Damage", "Weld Defect",
                           "Material Crack", "Missing Feature"};
    int ncat = 8;
    SMF_ParetoItem pareto[8];

    smf_pareto_analyze(defects, names, ncat, pareto);

    printf("%-25s %8s %8s %12s\n", "Defect", "Count", "%", "Cumulative%");
    printf("----------------------------------------------------\n");
    for (int i = 0; i < ncat; i++) {
        printf("%-25s %8d %7.1f%% %11.1f%%\n",
               pareto[i].defect_name, pareto[i].frequency,
               pareto[i].percentage, pareto[i].cumulative_pct);
    }

    /* Identify top 80% contributors */
    printf("\nTop contributors (80%% of defects):\n");
    for (int i = 0; i < ncat; i++) {
        if (pareto[i].cumulative_pct <= 80.01f) {
            printf("  - %s\n", pareto[i].defect_name);
        } else {
            break;
        }
    }

    /* ---- Root Cause Analysis ---- */
    printf("\n--- 5-Why Root Cause Analysis ---\n");
    SMF_RootCauseAnalysis rca;
    smf_rca_5why(&rca, "Surface scratch defects increased 300% in Q2");

    printf("Problem: %s\n", rca.problem_statement);
    printf("\nExample 5-Why Chain:\n");
    printf("  Why 1: Scratches occur during handling\n");
    printf("  Why 2: Parts contact metal trays\n");
    printf("  Why 3: Tray coating has worn off\n");
    printf("  Why 4: Tray replacement overdue\n");
    printf("  Why 5: No PM schedule for material handling equipment\n");
    printf("\nRoot Cause: %s\n", smf_ishikawa_category(1)); /* Machine/Equipment */
    printf("Corrective Action: Implement PM schedule for all handling equipment\n");

    printf("\n=== SPC Demo Complete ===\n");
    return 0;
}
