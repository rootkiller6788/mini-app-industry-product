/* example_scheduling.c - Production Scheduling Demo
 * Demonstrates: Johnson's Rule, single machine dispatching,
 * job shop scheduling, work order management, capacity planning.
 *
 * Usage: gcc -I../include example_scheduling.c -L../build -lsmartmfg -lm -o sched_demo && ./sched_demo
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "smart_manufacturing.h"
#include "production_core.h"

int main(void) {
    printf("=== Production Scheduling Demo ===\n\n");

    /* ---- SINGLE MACHINE DISPATCHING ---- */
    printf("--- Single Machine Dispatching (5 jobs) ---\n");
    SMF_SchedulingJob jobs[5];
    float proc_times[] = {8, 3, 10, 4, 6};
    float due_dates[] = {15, 5, 25, 10, 20};

    for (int i = 0; i < 5; i++) {
        smf_sched_job_init(&jobs[i], i, "", 1);
        smf_sched_job_set_op(&jobs[i], 0, 0, proc_times[i]);
        jobs[i].due_date = due_dates[i];
    }

    SMF_DispatchRule rules[] = {
        SMF_DISPATCH_FIFO, SMF_DISPATCH_SPT, SMF_DISPATCH_EDD, SMF_DISPATCH_MS
    };
    const char *rule_names[] = {"FIFO", "SPT", "EDD", "Min Slack"};

    printf("%-15s %10s %10s %10s\n", "Rule", "Makespan", "FlowTime", "Tardy");
    printf("--------------------------------------------------\n");
    for (int r = 0; r < 4; r++) {
        SMF_Schedule s = smf_schedule_single_machine(jobs, 5, rules[r], 0);
        int tardy = smf_schedule_tardy_count(&s, jobs, 5);
        printf("%-15s %10.1f %10.1f %10d\n",
               rule_names[r], s.makespan,
               smf_schedule_avg_flow_time(&s), tardy);
    }

    /* ---- JOHNSON'S RULE ---- */
    printf("\n--- Johnson's Rule (2-Machine Flow Shop) ---\n");
    SMF_SchedulingJob fjobs[5];
    float fdata[5][2] = {{3,6},{7,2},{4,7},{5,3},{7,4}};

    for (int i = 0; i < 5; i++) {
        smf_sched_job_init(&fjobs[i], i, "", 2);
        smf_sched_job_set_op(&fjobs[i], 0, 0, fdata[i][0]);
        smf_sched_job_set_op(&fjobs[i], 1, 1, fdata[i][1]);
    }

    SMF_Schedule fs = smf_schedule_johnson_2machine(fjobs, 5);

    printf("Johnson Sequence (jobs on M1):\n");
    int last_job = -1;
    for (int i = 0; i < fs.num_entries; i++) {
        if (fs.entries[i].machine_idx == 0) {
            printf("  Job %d: M1[%.0f-%.0f]",
                   fs.entries[i].job_idx,
                   fs.entries[i].start_time, fs.entries[i].end_time);
            /* Find corresponding M2 entry */
            for (int j = 0; j < fs.num_entries; j++) {
                if (fs.entries[j].job_idx == fs.entries[i].job_idx
                    && fs.entries[j].machine_idx == 1) {
                    printf("  M2[%.0f-%.0f]\n",
                           fs.entries[j].start_time, fs.entries[j].end_time);
                    break;
                }
            }
        }
    }
    printf("Optimal Makespan: %.0f\n", fs.makespan);

    /* ---- WORK ORDER DEMO ---- */
    printf("\n--- Work Order Lifecycle ---\n");
    SMF_Factory *f = smf_factory_create("Sched Demo Plant", "Floor 2");

    smf_add_material(f, "AL-6061", "Aluminum 6061", SMF_MAT_RAW, "kg", 12.5f);
    smf_add_material(f, "SS-304", "Stainless 304", SMF_MAT_RAW, "kg", 25.0f);

    int p = smf_add_product(f, "BRACKET-A", "Mounting Bracket", "Rev.B");
    smf_add_bom(f, "BOM-BRACKET-A", p);
    smf_bom_add_item(f, 0, 0, 0.5f, 1);
    smf_bom_add_item(f, 0, 1, 0.2f, 1);

    smf_add_workstation(f, "WS-MILL", "Milling Station", "Machining");
    smf_add_workstation(f, "WS-ASM", "Assembly Station", "Assembly");

    smf_add_routing(f, "ROUTE-BRACKET-A", p);
    smf_routing_add_step(f, 0, SMF_OP_MACHINING, "Mill Profile", 0, 0.5f, 2.5f, 0);
    smf_routing_add_step(f, 0, SMF_OP_ASSEMBLY, "Assemble", 1, 0.2f, 1.0f, 1);
    smf_routing_add_step(f, 0, SMF_OP_QUALITY_GATE, "Final QC", -1, 0.1f, 0.5f, 2);

    time_t now = time(NULL);
    SMF_WorkOrder *wo = smf_wo_create(f, p, 1000, now, now + 7 * 86400, 5);
    printf("Created WO: %s (qty=%d, priority=%d)\n",
           wo->wo_number, wo->quantity_ordered, wo->priority);
    printf("Estimated cost: $%.2f\n", wo->estimated_cost);

    smf_wo_release(f, 0);
    printf("WO released, state: %s\n", smf_wo_state_name(wo->state));

    /* Capacity check */
    SMF_CapacityLoad loads[10];
    int nloads = smf_capacity_load_profile(f, loads, 10);
    printf("\nCapacity Load Profile:\n");
    for (int i = 0; i < nloads; i++) {
        printf("  WS%d: %.0f/%.0f hrs (%.0f%% load)%s\n",
               loads[i].workstation_idx,
               loads[i].required_hours_per_week,
               loads[i].available_hours_per_week,
               loads[i].load_percent,
               loads[i].load_percent > 100 ? " [OVERLOAD]" : "");
    }

    /* Takt time */
    float takt = smf_calc_takt_time(480, 240);
    printf("\nTakt Time: %.1f min/unit (480min/240units)\n", takt);

    smf_factory_destroy(f);
    printf("\n=== Scheduling Demo Complete ===\n");
    return 0;
}
