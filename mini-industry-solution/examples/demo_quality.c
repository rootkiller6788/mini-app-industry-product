/**
 * demo_quality.c - Statistical Quality Control Demo
 *
 * Demonstrates: X-bar/R chart, process capability, Pareto analysis,
 * CUSUM, supplier scorecard.
 *
 * Build: make examples
 * Run:   ./build/demo_quality
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "quality_control.h"

int main(void) {
    printf("=== Statistical Quality Control Demo ===\n\n");

    /* 1. X-bar/R Chart */
    printf("--- X-bar/R Control Chart ---\n");
    double buf[60];
    XBarRChart chart;
    xbar_r_chart_init(&chart, 5, buf, 60);

    double subgroups[][5] = {
        {10.0, 9.8, 10.2, 10.1, 9.9},
        {10.1, 10.0, 9.9, 10.2, 10.0},
        {10.0, 10.1, 10.0, 9.9, 10.1},
        {10.2, 10.0, 9.8, 10.1, 10.0},
        {10.0, 10.2, 10.1, 9.9, 10.0},
        {10.1, 9.9, 10.0, 10.2, 10.1},
        {9.9, 10.0, 10.1, 10.0, 10.2},
        {10.0, 10.1, 9.9, 10.1, 10.0}
    };

    for (int i = 0; i < 8; i++)
        xbar_r_chart_update(&chart, subgroups[i], 5);

    printf("  Grand mean: %.3f\n", chart.grand_mean);
    printf("  Mean range: %.3f\n", chart.mean_range);
    printf("  X-bar UCL: %.3f LCL: %.3f\n", chart.ucl_x, chart.lcl_x);
    printf("  R-chart UCL: %.3f LCL: %.3f\n", chart.ucl_r, chart.lcl_r);

    int violations[10];
    int nv = weco_rules_check(&chart, violations, 10);
    printf("  WECO violations: %d\n", nv);
    for (int i = 0; i < nv; i++)
        printf("    %s\n", weco_rule_description(violations[i]));

    /* 2. Process Capability */
    printf("\n--- Process Capability ---\n");
    double all_vals[40];
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 5; j++)
            all_vals[i*5+j] = subgroups[i][j];

    CapabilityIndex cpk;
    capability_calculate(all_vals, 40, 10.5, 9.5, &cpk);
    printf("  USL: 10.5, LSL: 9.5\n");
    printf("  Cp: %.3f\n", cpk.cp);
    printf("  Cpk: %.3f\n", cpk.cpk);
    printf("  Sigma level: %.2f\n", cpk.sigma_level);
    printf("  DPM: %.0f\n", cpk.dpm);
    printf("  %s\n", cpk.capable ? "CAPABLE (>=1.33)" : "NOT CAPABLE");

    /* 3. Pareto Analysis */
    printf("\n--- Pareto Analysis ---\n");
    int defect_counts[] = {42, 18, 12, 8, 3, 2};
    const char *defect_labels[] = {
        "Scratch", "Dent", "Burr", "Color", "Size", "Other"
    };
    ParetoItem items[10];
    pareto_analysis(defect_counts, defect_labels, 6, items);
    printf("  %-12s %6s %8s %12s\n", "Category", "Count", "%", "Cum %");
    for (int i = 0; i < 6; i++) {
        printf("  %-12s %6d %7.1f%% %10.1f%%\n",
               items[i].category, items[i].count,
               items[i].percentage, items[i].cumulative_pct);
    }

    /* 4. CUSUM Chart */
    printf("\n--- CUSUM Control Chart ---\n");
    CUSUMChart c;
    cusum_init(&c, 20, 1.0, 5.0);
    double cusum_data[] = {0.0,0.5,-0.2,0.3,0.1,2.0,2.5,3.0,2.8,3.2};
    for (int i = 0; i < 10; i++) {
        int sh, sl;
        cusum_update(&c, cusum_data[i], &sh, &sl);
        printf("  x[%d]=%.1f C+ = %.2f C- = %.2f %s%s\n",
               i, cusum_data[i], c.cusum_high[i], c.cusum_low[i],
               sh ? " [HI SIGNAL!]" : "",
               sl ? " [LO SIGNAL!]" : "");
    }

    /* 5. Supplier Scorecard */
    printf("\n--- Supplier Quality Scorecard ---\n");
    SupplierScorecard sc;
    supplier_scorecard_init(&sc, "SUP-001", "Precision Parts Inc.");
    supplier_scorecard_update(&sc, 50, 48, 5000, 25);
    printf("  Supplier: %s (%s)\n", sc.supplier_name, sc.supplier_code);
    printf("  Lots: %d received, %d accepted\n",
           sc.lots_received, sc.lots_accepted);
    printf("  Parts: %d inspected, %d defective\n",
           sc.parts_inspected, sc.parts_defective);
    printf("  DPMO: %.0f\n", sc.dpmo);
    printf("  Quality Score: %.1f%%\n", sc.quality_score);
    printf("  Sigma Level: %.2f\n",
           process_sigma_from_dpmo(sc.dpmo));

    printf("\n=== Quality Control Demo Complete ===\n");
    return 0;
}
