/* example_oee.c - OEE Dashboard Simulation
 * Demonstrates: factory setup, machine monitoring, OEE calculation,
 * Six Big Losses analysis, and production reporting.
 *
 * Usage: gcc -I../include example_oee.c -L../build -lsmartmfg -lm -o oee_demo && ./oee_demo
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "smart_manufacturing.h"
#include "oee_monitor.h"

int main(void) {
    printf("=== OEE Dashboard Demo ===\n\n");

    /* Create factory */
    SMF_Factory *plant = smf_factory_create("OEE Demo Plant", "Building A");
    plant->shifts_per_day = 3;
    plant->hours_per_shift = 8.0f;
    plant->days_per_week = 5.0f;

    /* Add machines */
    smf_add_machine(plant, "CNC01", "CNC Mill #1", SMF_MACH_CNC);
    smf_add_machine(plant, "CNC02", "CNC Mill #2", SMF_MACH_CNC);
    smf_add_machine(plant, "LATHE01", "Lathe #1", SMF_MACH_LATHE);
    smf_add_machine(plant, "ROBOT01", "Welding Robot", SMF_MACH_WELDING);

    /* Set cycle times and start machines */
    for (int i = 0; i < 4; i++) {
        plant->machines[i].cycle_time_minutes = 1.0f + i * 0.5f;
        smf_machine_set_state(plant, i, SMF_MACH_RUNNING);
    }

    /* Simulate production: record parts + events */
    int parts_per_hour[] = {55, 52, 48, 40};
    for (int i = 0; i < 4; i++) {
        smf_machine_record_production(plant, i, parts_per_hour[i], 2);
    }

    /* Log some events */
    smf_event_log(plant, 0, SMF_EVENT_BREAKDOWN, 30.0f, "Spindle bearing failure", 3);
    smf_event_log(plant, 2, SMF_EVENT_SETUP_START, 15.0f, "Tool change", 1);
    smf_event_log(plant, 0, SMF_EVENT_REPAIR_DONE, 0.0f, "Spindle repaired", 3);

    /* Calculate OEE for each machine */
    printf("Machine OEE Summary:\n");
    printf("%-20s %8s %8s %8s %8s\n", "Machine", "Avail%", "Perf%", "Qual%", "OEE%");
    printf("-----------------------------------------------\n");

    for (int i = 0; i < 4; i++) {
        SMF_OEEMetrics oee;
        /* Simulate: 8hr shift, 7hr operating, various pieces */
        smf_oee_calculate(&oee, 8.0f, 7.0f - i * 0.5f,
                          1.0f / 60.0f, parts_per_hour[i] * 8,
                          parts_per_hour[i] * 8 - i);

        bool a, p, q;
        smf_oee_benchmark(&oee, &a, &p, &q);

        printf("%-20s %7.1f%% %7.1f%% %7.1f%% %7.1f%% %s\n",
               plant->machines[i].name,
               oee.availability * 100, oee.performance * 100,
               oee.quality * 100, oee.oee * 100,
               (oee.oee >= oee.oee_target) ? "[PASS]" : "[GAP]");
    }

    /* Six Big Losses Analysis */
    printf("\nSix Big Losses Analysis (CNC01):\n");
    SMF_MachineEvent events[10];
    int n = smf_event_query(plant, 0, 0, time(NULL) + 86400, events, 10);

    SMF_LossAnalysis loss;
    smf_loss_classify(events, n, 0, 0, time(NULL) + 86400, &loss);

    for (int i = 0; i < 6; i++) {
        printf("  %-30s: %5.1f hrs (%.1f%%)\n",
               smf_loss_category_name(i),
               loss.loss_hours[i],
               loss.total_loss_hours > 0
                   ? loss.loss_hours[i] / loss.total_loss_hours * 100 : 0);
    }

    printf("\nTotal Loss: %.1f hours\n", loss.total_loss_hours);
    int top = smf_loss_top_category(&loss);
    printf("Top Loss Category: %s\n", smf_loss_category_name(top));

    float gap = smf_oee_gap(&plant->machines[0].oee_overall > 0.85f
                             ? &(SMF_OEEMetrics){.oee=plant->machines[0].oee_overall}
                             : &(SMF_OEEMetrics){.oee=0.72f});
    printf("\nOEE Gap to World-Class (85%%): %.1f%%\n", gap * 100);

    smf_factory_destroy(plant);
    printf("\n=== Demo Complete ===\n");
    return 0;
}
