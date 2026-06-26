/* production_johnson.c - Johnson's Rule & Job Shop Scheduling
 * L5: Johnson (1954) F2||C_max optimal O(n log n)
 *     Giffler-Thompson (1960) active schedule generation
 * L4: Scheduling theory optimality theorems
 */
#include "production_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#define M_EPS 0.001f

/* ================================================================
 *  Johnson's Rule - 2-Machine Flow Shop (F2||C_max)
 *
 *  Theorem (Johnson 1954): For n jobs processed on M1 then M2,
 *  the sequence where jobs with a_j <= b_j come first (sorted by
 *  ascending a_j), followed by jobs with a_j > b_j (sorted by
 *  descending b_j), minimizes makespan.
 *
 *  Proof sketch: Adjacent pairwise interchange shows that any
 *  violation of min(a_i,b_j) <= min(a_j,b_i) can be improved.
 *
 *  Complexity: O(n log n) time, O(n) space.
 * ================================================================ */

SMF_Schedule smf_schedule_johnson_2machine(SMF_SchedulingJob *jobs,
                                            int num_jobs) {
    SMF_Schedule sched;
    memset(&sched, 0, sizeof(SMF_Schedule));
    sched.num_machines = 2;
    if (!jobs || num_jobs <= 0) return sched;

    /* Allocate partition arrays */
    int *set_a = (int*)malloc(num_jobs * sizeof(int));
    int *set_b = (int*)malloc(num_jobs * sizeof(int));
    if (!set_a || !set_b) {
        free(set_a); free(set_b);
        return sched;
    }
    int na = 0, nb = 0;

    /* Partition: Set A (a <= b), Set B (a > b) */
    for (int i = 0; i < num_jobs; i++) {
        float a = jobs[i].processing_times[0]; /* M1 time */
        float b = jobs[i].processing_times[1]; /* M2 time */
        jobs[i].remaining_work = a + b;

        if (a <= b) {
            set_a[na++] = i;
        } else {
            set_b[nb++] = i;
        }
    }

    /* Sort Set A by ascending processing time on M1 */
    for (int i = 0; i < na - 1; i++) {
        for (int j = i + 1; j < na; j++) {
            if (jobs[set_a[i]].processing_times[0] >
                jobs[set_a[j]].processing_times[0]) {
                int tmp = set_a[i];
                set_a[i] = set_a[j];
                set_a[j] = tmp;
            }
        }
    }

    /* Sort Set B by descending processing time on M2 */
    for (int i = 0; i < nb - 1; i++) {
        for (int j = i + 1; j < nb; j++) {
            if (jobs[set_b[i]].processing_times[1] <
                jobs[set_b[j]].processing_times[1]) {
                int tmp = set_b[i];
                set_b[i] = set_b[j];
                set_b[j] = tmp;
            }
        }
    }

    /* Build schedule from combined sequence */
    float m1_end = 0.0f, m2_end = 0.0f;

    for (int i = 0; i < na; i++) {
        int job_i = set_a[i];
        float a = jobs[job_i].processing_times[0];
        float b = jobs[job_i].processing_times[1];

        /* Machine 1 */
        int e = sched.num_entries++;
        sched.entries[e].job_idx = jobs[job_i].job_id;
        sched.entries[e].machine_idx = 0;
        sched.entries[e].operation_no = 0;
        sched.entries[e].start_time = m1_end;
        m1_end += a;
        sched.entries[e].end_time = m1_end;
        sched.entries[e].setup_time = 0.0f;
        sched.entries[e].is_idle = false;

        /* Machine 2 */
        e = sched.num_entries++;
        float m2_start = (m1_end > m2_end) ? m1_end : m2_end;
        sched.entries[e].job_idx = jobs[job_i].job_id;
        sched.entries[e].machine_idx = 1;
        sched.entries[e].operation_no = 1;
        sched.entries[e].start_time = m2_start;
        m2_end = m2_start + b;
        sched.entries[e].end_time = m2_end;
        sched.entries[e].setup_time = 0.0f;
        sched.entries[e].is_idle = false;
    }

    for (int i = 0; i < nb; i++) {
        int job_i = set_b[i];
        float a = jobs[job_i].processing_times[0];
        float b = jobs[job_i].processing_times[1];

        int e = sched.num_entries++;
        sched.entries[e].job_idx = jobs[job_i].job_id;
        sched.entries[e].machine_idx = 0;
        sched.entries[e].operation_no = 0;
        sched.entries[e].start_time = m1_end;
        m1_end += a;
        sched.entries[e].end_time = m1_end;
        sched.entries[e].setup_time = 0.0f;
        sched.entries[e].is_idle = false;

        e = sched.num_entries++;
        float m2_start = (m1_end > m2_end) ? m1_end : m2_end;
        sched.entries[e].job_idx = jobs[job_i].job_id;
        sched.entries[e].machine_idx = 1;
        sched.entries[e].operation_no = 1;
        sched.entries[e].start_time = m2_start;
        m2_end = m2_start + b;
        sched.entries[e].end_time = m2_end;
        sched.entries[e].setup_time = 0.0f;
        sched.entries[e].is_idle = false;
    }

    sched.makespan = m2_end;
    sched.total_flow_time = 0.0f;
    for (int i = 0; i < sched.num_entries; i += 2) {
        sched.total_flow_time += sched.entries[i + 1].end_time;
    }
    sched.machine_utilization[0] = m1_end / m2_end;
    sched.machine_utilization[1] = m2_end / m2_end;

    free(set_a);
    free(set_b);
    return sched;
}

/* ================================================================
 *  Job Shop Scheduling - Giffler-Thompson (1960)
 *
 *  Generates non-delay schedules for J_m||C_max.
 *
 *  Algorithm:
 *  1. Initialize: all machines free at time 0.
 *  2. Among unscheduled operations whose predecessors are done,
 *     find the one with earliest possible completion time (ECT).
 *  3. Identify all operations on the same machine that can start
 *     before ECT (conflict set).
 *  4. Apply dispatch rule to select one from the conflict set.
 *  5. Schedule it and update machine/job ready times.
 *  6. Repeat until all operations scheduled.
 *
 *  Complexity: O(N^2 * m) for N operations, m machines.
 * ================================================================ */

SMF_Schedule smf_schedule_job_shop(SMF_SchedulingJob *jobs, int num_jobs,
                                    int num_machines, SMF_DispatchRule rule) {
    SMF_Schedule sched;
    memset(&sched, 0, sizeof(SMF_Schedule));
    if (!jobs || num_jobs <= 0 || num_machines <= 0) return sched;

    sched.num_machines = num_machines < SMF_SCHED_MAX_MACHINES
                         ? num_machines : SMF_SCHED_MAX_MACHINES;

    /* Machine completion times and job readiness */
    float machine_free[SMF_SCHED_MAX_MACHINES];
    float job_ready[SMF_SCHED_MAX_JOBS];
    int ops_done[SMF_SCHED_MAX_JOBS];
    bool job_done[SMF_SCHED_MAX_JOBS];

    for (int i = 0; i < sched.num_machines; i++) machine_free[i] = 0.0f;
    for (int i = 0; i < num_jobs; i++) {
        job_ready[i] = jobs[i].release_time;
        ops_done[i] = 0;
        job_done[i] = false;
    }

    /* Total number of operations to schedule */
    int total_ops = 0;
    for (int i = 0; i < num_jobs; i++) {
        total_ops += jobs[i].num_operations;
    }

    /* Giffler-Thompson main loop */
    for (int iter = 0; iter < total_ops; iter++) {
        /* Step 2: Find earliest completable operation */
        float earliest_complete = FLT_MAX;
        int earliest_machine = -1;

        for (int j = 0; j < num_jobs; j++) {
            if (job_done[j]) continue;
            int op = ops_done[j];
            if (op >= jobs[j].num_operations) continue;

            int m = jobs[j].machine_sequence[op];
            float p = jobs[j].processing_times[op];
            float start = (job_ready[j] > machine_free[m])
                          ? job_ready[j] : machine_free[m];
            float complete = start + p;

            if (complete < earliest_complete) {
                earliest_complete = complete;
                earliest_machine = m;
            }
        }

        if (earliest_machine < 0) break;

        /* Step 3-4: Find conflict set and apply dispatch rule */
        int selected_job = -1;
        float best_criterion = FLT_MAX;

        for (int j = 0; j < num_jobs; j++) {
            if (job_done[j]) continue;
            int op = ops_done[j];
            if (op >= jobs[j].num_operations) continue;

            int m = jobs[j].machine_sequence[op];
            if (m != earliest_machine) continue;

            float start = (job_ready[j] > machine_free[m])
                          ? job_ready[j] : machine_free[m];

            /* Only consider operations that can start before ECT */
            if (start < earliest_complete + M_EPS) {
                float criterion = 0.0f;

                switch (rule) {
                    case SMF_DISPATCH_SPT:
                        criterion = jobs[j].processing_times[op];
                        break;
                    case SMF_DISPATCH_LPT:
                        criterion = -jobs[j].processing_times[op];
                        break;
                    case SMF_DISPATCH_EDD:
                        criterion = jobs[j].due_date;
                        break;
                    case SMF_DISPATCH_MS: {
                        /* Min Slack = d - t - remaining_processing */
                        float remaining = 0.0f;
                        for (int k = op; k < jobs[j].num_operations; k++) {
                            remaining += jobs[j].processing_times[k];
                        }
                        criterion = jobs[j].due_date - start - remaining;
                        } break;
                    case SMF_DISPATCH_FIFO:
                    default:
                        criterion = jobs[j].release_time;
                        break;
                }

                if (criterion < best_criterion) {
                    best_criterion = criterion;
                    selected_job = j;
                }
            }
        }

        if (selected_job < 0) break;

        /* Step 5: Schedule the selected operation */
        int op = ops_done[selected_job];
        int m = jobs[selected_job].machine_sequence[op];
        float p = jobs[selected_job].processing_times[op];
        float start = (job_ready[selected_job] > machine_free[m])
                      ? job_ready[selected_job] : machine_free[m];
        float end = start + p;

        int ei = sched.num_entries++;
        sched.entries[ei].job_idx = jobs[selected_job].job_id;
        sched.entries[ei].machine_idx = m;
        sched.entries[ei].operation_no = op;
        sched.entries[ei].start_time = start;
        sched.entries[ei].end_time = end;
        sched.entries[ei].setup_time = 0.0f;
        sched.entries[ei].is_idle = false;

        /* Update */
        machine_free[m] = end;
        job_ready[selected_job] = end;
        ops_done[selected_job]++;

        if (ops_done[selected_job] >= jobs[selected_job].num_operations) {
            job_done[selected_job] = true;
        }
    }

    /* Compute makespan */
    sched.makespan = 0.0f;
    for (int i = 0; i < sched.num_entries; i++) {
        if (sched.entries[i].end_time > sched.makespan) {
            sched.makespan = sched.entries[i].end_time;
        }
    }

    /* Compute machine utilizations */
    if (sched.makespan > 0.0f) {
        for (int m = 0; m < sched.num_machines; m++) {
            float work = 0.0f;
            for (int i = 0; i < sched.num_entries; i++) {
                if (sched.entries[i].machine_idx == m) {
                    work += sched.entries[i].end_time
                            - sched.entries[i].start_time;
                }
            }
            sched.machine_utilization[m] = work / sched.makespan;
        }
    }

    return sched;
}