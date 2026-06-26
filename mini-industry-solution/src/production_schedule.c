/**
 * mini-industry-solution: production_schedule.c
 * Job shop scheduling, Johnson's Rule, NEH heuristic, dispatch rules
 * Ref: Johnson (1954), NEH (Nawaz, Enscore, Ham 1983),
 *      Pinedo (2016) Scheduling, Goldratt (1984) The Goal
 */
#include "production_schedule.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* === Schedule Management (L3) === */

void schedule_init(Schedule *sched, Job *jobs, int n_jobs,
                    Machine *machines, int n_machines) {
    if (!sched) return;
    memset(sched, 0, sizeof(*sched));
    sched->jobs = jobs; sched->n_jobs = n_jobs;
    sched->machines = machines; sched->n_machines = n_machines;
    for (int i = 0; i < n_machines; i++) {
        machines[i].total_busy_time = 0.0;
        machines[i].total_idle_time = 0.0;
        machines[i].jobs_processed = 0;
        machines[i].status = MACHINE_IDLE;
    }
}

void schedule_clear(Schedule *sched) {
    if (!sched) return;
    sched->makespan = 0.0;
    sched->total_tardiness = 0.0;
    sched->n_tardy_jobs = 0;
    sched->machine_utilization = 0.0;
    sched->flow_time_mean = 0.0;
}

/* === Johnson's Rule (L5) === */
/*
 * Johnson's Rule for 2-machine flow shop: optimal sequence
 * minimizing makespan. Proven optimal by Johnson (1954).
 *
 * Algorithm:
 * 1. Partition jobs: Set A = {j | p1j < p2j}, Set B = {j | p1j >= p2j}
 * 2. Sort A by p1j ascending (SPT on machine 1)
 * 3. Sort B by p2j descending (LPT on machine 2)
 * 4. Sequence = A followed by B
 *
 * Complexity: O(n log n)
 * Ref: Johnson, S.M. (1954) "Optimal two- and three-stage
 *      production schedules", Naval Research Logistics Quarterly
 */

int johnsons_rule_2machine(Job *jobs, int n_jobs,
                            double *m1_times, double *m2_times,
                            int *optimal_sequence) {
    (void)jobs;
    if (!m1_times || !m2_times || !optimal_sequence
        || n_jobs <= 0) return -1;

    typedef struct { int id; double p1, p2; } JobPair;
    JobPair *pairs = (JobPair *)malloc((size_t)n_jobs * sizeof(JobPair));
    if (!pairs) return -1;

    for (int i = 0; i < n_jobs; i++) {
        pairs[i].id = i;
        pairs[i].p1 = m1_times[i];
        pairs[i].p2 = m2_times[i];
    }

    /* Sort by min(p1, p2) ascending, tie-break by p1 */
    for (int i = 0; i < n_jobs - 1; i++) {
        for (int j = 0; j < n_jobs - i - 1; j++) {
            double min_j  = (pairs[j].p1 < pairs[j].p2)
                            ? pairs[j].p1 : pairs[j].p2;
            double min_j1 = (pairs[j+1].p1 < pairs[j+1].p2)
                            ? pairs[j+1].p1 : pairs[j+1].p2;
            if (min_j > min_j1
                || (min_j == min_j1 && pairs[j].p1 > pairs[j+1].p1)) {
                JobPair tmp = pairs[j];
                pairs[j] = pairs[j+1];
                pairs[j+1] = tmp;
            }
        }
    }

    int *A = (int *)malloc((size_t)n_jobs * sizeof(int));
    int *B = (int *)malloc((size_t)n_jobs * sizeof(int));
    int na = 0, nb = 0;
    for (int i = 0; i < n_jobs; i++) {
        if (pairs[i].p1 < pairs[i].p2)
            A[na++] = pairs[i].id;
        else
            B[nb++] = pairs[i].id;
    }

    int idx = 0;
    for (int i = 0; i < na; i++) optimal_sequence[idx++] = A[i];
    for (int i = nb - 1; i >= 0; i--) optimal_sequence[idx++] = B[i];

    free(pairs); free(A); free(B);
    return 0;
}

/* === NEH Heuristic (L5) === */
/*
 * NEH heuristic for permutation flow shop scheduling.
 * The most effective constructive heuristic for makespan minimization.
 *
 * Steps:
 * 1. Sort jobs by total processing time descending
 * 2. Take first two jobs, evaluate both sequences, keep best
 * 3. For k=3 to n: insert job k at position that minimizes
 *    partial makespan among k possible positions
 *
 * Complexity: O(n^3 * m)
 * Ref: Nawaz, Enscore, Ham (1983) "A heuristic algorithm for the
 *      m-machine, n-job flow-shop sequencing problem", OMEGA
 */

int neh_heuristic_flowshop(int n_jobs, int n_machines,
                            const double * const *processing_times,
                            int *out_sequence, double *out_makespan) {
    if (!processing_times || !out_sequence || !out_makespan
        || n_jobs <= 0 || n_machines <= 0) return -1;

    /* Sort jobs by total processing time descending */
    typedef struct { int id; double total; } JobTotal;
    JobTotal *jt = (JobTotal *)malloc((size_t)n_jobs * sizeof(JobTotal));
    if (!jt) return -1;
    for (int i = 0; i < n_jobs; i++) {
        jt[i].id = i;
        jt[i].total = 0.0;
        for (int m = 0; m < n_machines; m++)
            jt[i].total += processing_times[i][m];
    }
    for (int i = 0; i < n_jobs - 1; i++)
        for (int j = 0; j < n_jobs - i - 1; j++)
            if (jt[j].total < jt[j+1].total) {
                JobTotal tmp = jt[j]; jt[j] = jt[j+1]; jt[j+1] = tmp;
            }

    /* Build sequence incrementally */
    int *seq = (int *)malloc((size_t)n_jobs * sizeof(int));
    int n_seq = 0;
    seq[n_seq++] = jt[0].id;

    double best_makespan = 1e18;
    for (int k = 1; k < n_jobs; k++) {
        int new_job = jt[k].id;
        int best_pos = n_seq;
        best_makespan = 1e18;

        for (int pos = n_seq; pos >= 0; pos--) {
            /* Compute makespan with new job inserted at 'pos' */
            double *machine_end = (double *)calloc((size_t)n_machines,
                                                    sizeof(double));
            for (int i = 0; i <= n_seq; i++) {
                int job_id;
                if (i < pos) job_id = seq[i];
                else if (i == pos) job_id = new_job;
                else job_id = seq[i-1];

                for (int m = 0; m < n_machines; m++) {
                    double prev_m = (m > 0) ? machine_end[m-1] : 0.0;
                    double start = (machine_end[m] > prev_m)
                                   ? machine_end[m] : prev_m;
                    machine_end[m] = start + processing_times[job_id][m];
                }
            }
            double mk = machine_end[n_machines - 1];
            free(machine_end);

            if (mk < best_makespan) {
                best_makespan = mk;
                best_pos = pos;
            }
        }

        /* Insert job at best position */
        for (int i = n_seq; i > best_pos; i--) seq[i] = seq[i-1];
        seq[best_pos] = new_job;
        n_seq++;
    }

    for (int i = 0; i < n_jobs; i++) out_sequence[i] = seq[i];
    *out_makespan = best_makespan;

    free(jt); free(seq);
    return 0;
}

/* === Priority Dispatching (L5) === */
/*
 * Dispatching rules for job shop scheduling.
 * Different rules optimize different objectives.
 *
 * SPT: minimizes mean flow time
 * EDD: minimizes maximum tardiness
 * CR:  balances urgency
 * SLACK: dynamic priority based on remaining slack
 */

int priority_dispatch_schedule(const Job *jobs, int n_jobs,
                                DispatchRule rule, int *out_sequence) {
    if (!jobs || !out_sequence || n_jobs <= 0) return -1;

    typedef struct { int id; double key; } JobKey;
    JobKey *keys = (JobKey *)malloc((size_t)n_jobs * sizeof(JobKey));
    if (!keys) return -1;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    for (int i = 0; i < n_jobs; i++) {
        keys[i].id = i;
        switch (rule) {
        case DISPATCH_SPT:
            keys[i].key = jobs[i].total_processing_time;
            break;
        case DISPATCH_LPT:
            keys[i].key = -jobs[i].total_processing_time;
            break;
        case DISPATCH_EDD: {
            double diff = (double)(jobs[i].due_date.tv_sec - now.tv_sec);
            keys[i].key = diff;
            break;
        }
        case DISPATCH_FIFO: {
            double diff = (double)(jobs[i].release_date.tv_sec - now.tv_sec);
            keys[i].key = diff;
            break;
        }
        case DISPATCH_CR: {
            double remain = jobs[i].remaining_time;
            double time_left = (double)(jobs[i].due_date.tv_sec
                                        - now.tv_sec);
            keys[i].key = (remain > 0.0) ? time_left / remain : 1e9;
            break;
        }
        case DISPATCH_SLACK: {
            double time_left = (double)(jobs[i].due_date.tv_sec
                                        - now.tv_sec);
            keys[i].key = time_left - jobs[i].remaining_time;
            break;
        }
        default:
            keys[i].key = (double)i;
        }
    }

    /* Sort by key ascending */
    for (int i = 0; i < n_jobs - 1; i++)
        for (int j = 0; j < n_jobs - i - 1; j++)
            if (keys[j].key > keys[j+1].key) {
                JobKey tmp = keys[j];
                keys[j] = keys[j+1];
                keys[j+1] = tmp;
            }

    for (int i = 0; i < n_jobs; i++) out_sequence[i] = keys[i].id;
    free(keys);
    return 0;
}

/* === CDS Heuristic (L5) === */
/*
 * Campbell-Dudek-Smith heuristic for m-machine flow shop.
 * Reduces m-machine problem to (m-1) 2-machine problems
 * solved by Johnson's Rule, then picks the best.
 * Ref: Campbell, Dudek, Smith (1970) Management Science
 */
int cds_heuristic(int n_jobs, int n_machines,
                   const double * const *processing_times,
                   int *out_sequence, double *out_makespan) {
    if (!processing_times || !out_sequence || !out_makespan
        || n_jobs <= 0 || n_machines < 2) return -1;

    double best_mk = 1e18;
    int *best_seq = (int *)malloc((size_t)n_jobs * sizeof(int));
    int *cur_seq  = (int *)malloc((size_t)n_jobs * sizeof(int));
    double *m1 = (double *)malloc((size_t)n_jobs * sizeof(double));
    double *m2 = (double *)malloc((size_t)n_jobs * sizeof(double));

    for (int k = 1; k < n_machines; k++) {
        for (int j = 0; j < n_jobs; j++) {
            m1[j] = 0.0; m2[j] = 0.0;
            for (int m = 0; m < k; m++) m1[j] += processing_times[j][m];
            for (int m = n_machines - k; m < n_machines; m++)
                m2[j] += processing_times[j][m];
        }
        johnsons_rule_2machine(NULL, n_jobs, m1, m2, cur_seq);
        double mk = makespan_calculate(NULL, n_jobs, n_machines,
                                        cur_seq);
        if (mk < best_mk) {
            best_mk = mk;
            for (int i = 0; i < n_jobs; i++) best_seq[i] = cur_seq[i];
        }
    }

    for (int i = 0; i < n_jobs; i++) out_sequence[i] = best_seq[i];
    *out_makespan = best_mk;
    free(best_seq); free(cur_seq); free(m1); free(m2);
    return 0;
}

/* === Schedule Evaluation (L3) === */

double makespan_calculate(const Job *jobs, int n_jobs, int n_machines,
                           const int *sequence) {
    if (!sequence || n_jobs <= 0 || n_machines <= 0) return 0.0;
    double *machine_end = (double *)calloc((size_t)n_machines,
                                            sizeof(double));
    if (!machine_end) return 0.0;

    for (int i = 0; i < n_jobs; i++) {
        int jid = sequence[i];
        for (int m = 0; m < n_machines; m++) {
            double proc_time = jobs ? jobs[jid].operations[m].processing_time
                                    : 1.0;
            double prev_m = (m > 0) ? machine_end[m-1] : 0.0;
            double start = (machine_end[m] > prev_m) ? machine_end[m] : prev_m;
            machine_end[m] = start + proc_time;
        }
    }
    double mk = machine_end[n_machines - 1];
    free(machine_end);
    return mk;
}

double total_tardiness_calculate(const Job *jobs, int n_jobs,
                                   const int *sequence) {
    if (!jobs || !sequence || n_jobs <= 0) return 0.0;
    double tardiness = 0.0;
    double current_time = 0.0;
    for (int i = 0; i < n_jobs; i++) {
        int jid = sequence[i];
        current_time += jobs[jid].total_processing_time;
        double due = (double)jobs[jid].due_date.tv_sec;
        if (current_time > due)
            tardiness += (current_time - due);
    }
    return tardiness;
}

double machine_utilization_calculate(const Schedule *sched) {
    if (!sched || sched->n_machines <= 0) return 0.0;
    double total_util = 0.0;
    for (int i = 0; i < sched->n_machines; i++) {
        double total = sched->machines[i].total_busy_time
                       + sched->machines[i].total_idle_time;
        if (total > 0.0)
            total_util += sched->machines[i].total_busy_time / total;
    }
    return total_util / (double)sched->n_machines * 100.0;
}

double mean_flow_time_calculate(const Job *jobs, int n_jobs,
                                  const int *sequence) {
    if (!jobs || !sequence || n_jobs <= 0) return 0.0;
    double total_flow = 0.0, cum_time = 0.0;
    for (int i = 0; i < n_jobs; i++) {
        cum_time += jobs[sequence[i]].total_processing_time;
        total_flow += cum_time;
    }
    return total_flow / (double)n_jobs;
}

/* === Feasibility & Release (L3) === */

int schedule_feasibility_check(const Schedule *sched) {
    if (!sched) return 0;
    for (int i = 0; i < sched->n_jobs; i++) {
        if (sched->jobs[i].status == JOB_CANCELLED) continue;
        if (sched->jobs[i].total_processing_time <= 0.0) return 0;
    }
    return 1;
}

int job_release_schedule(Job *jobs, int n_jobs,
                          struct timespec now, int *out_released) {
    if (!jobs || !out_released || n_jobs <= 0) return 0;
    int count = 0;
    for (int i = 0; i < n_jobs; i++) {
        if (jobs[i].status == JOB_RELEASED
            && jobs[i].release_date.tv_sec <= now.tv_sec) {
            out_released[count++] = i;
            jobs[i].status = JOB_QUEUED;
        }
    }
    return count;
}

/* === Critical Ratio & Slack (L4) === */

double critical_ratio_calculate(const Job *job, struct timespec now) {
    if (!job) return 1e9;
    double time_left = (double)(job->due_date.tv_sec - now.tv_sec);
    double remain = job->remaining_time;
    if (remain <= 0.0) {
        return (time_left > 0.0) ? 1e9 : 0.0;
    }
    double cr = time_left / remain;
    if (cr < 0.0) return 0.0;
    return cr;
}

double slack_time_calculate(const Job *job, struct timespec now) {
    if (!job) return 1e9;
    double time_left = (double)(job->due_date.tv_sec - now.tv_sec);
    return time_left - job->remaining_time;
}

/* === Work Center Load Analysis (L6) === */

int work_center_load_analysis(const Machine *machines, int n_machines,
                                const Job *jobs, int n_jobs,
                                double available_time,
                                WorkCenterLoad *out_loads) {
    if (!machines || !jobs || !out_loads
        || n_machines <= 0 || n_jobs <= 0) return -1;

    for (int m = 0; m < n_machines; m++) {
        out_loads[m].machine_id = machines[m].machine_id;
        out_loads[m].total_load = 0.0;

        /* Sum processing time for all jobs assigned to this machine */
        for (int j = 0; j < n_jobs; j++) {
            for (int o = 0; o < jobs[j].n_operations; o++) {
                if (jobs[j].operations[o].machine_id == machines[m].machine_id)
                    out_loads[m].total_load +=
                        jobs[j].operations[o].processing_time;
            }
        }

        out_loads[m].available_time = available_time;
        out_loads[m].load_pct = (available_time > 0.0)
            ? out_loads[m].total_load / available_time * 100.0 : 0.0;
        out_loads[m].overloaded = (out_loads[m].load_pct > 100.0) ? 1 : 0;
    }
    return n_machines;
}

/* === Batch Formation (L7) === */

int batch_formation(Job *jobs, int n_jobs, int max_batch_size,
                     BatchJob *out_batches, int max_batches,
                     int *out_n_batches) {
    if (!jobs || !out_batches || !out_n_batches
        || n_jobs <= 0 || max_batch_size <= 0
        || max_batches <= 0) return -1;

    int batch_idx = 0, job_idx = 0;
    while (job_idx < n_jobs && batch_idx < max_batches) {
        BatchJob *bj = &out_batches[batch_idx];
        bj->batch_jobs = &jobs[job_idx];
        bj->max_batch_size = max_batch_size;
        int count = 0;
        double total_time = 0.0;

        while (job_idx < n_jobs && count < max_batch_size) {
            total_time += jobs[job_idx].total_processing_time;
            job_idx++; count++;
        }

        bj->batch_size = count;
        bj->processing_time = total_time;
        batch_idx++;
    }
    *out_n_batches = batch_idx;
    return batch_idx;
}

/* === Bottleneck & DBR (L4-L7) === */
/*
 * Theory of Constraints (Goldratt, 1984)
 * Bottleneck: resource with highest utilization.
 * DBR scheduling: schedule bottleneck first, then
 * release materials at rope speed.
 */

int bottleneck_identify(const Machine *machines, int n_machines,
                          const Job *jobs, int n_jobs) {
    if (!machines || n_machines <= 0) return -1;
    WorkCenterLoad *loads = (WorkCenterLoad *)malloc(
        (size_t)n_machines * sizeof(WorkCenterLoad));
    if (!loads) return -1;

    work_center_load_analysis(machines, n_machines, jobs, n_jobs,
                               100.0, loads);

    int bottleneck = 0;
    double max_load = 0.0;
    for (int i = 0; i < n_machines; i++) {
        if (loads[i].total_load > max_load) {
            max_load = loads[i].total_load;
            bottleneck = i;
        }
    }

    free(loads);
    return bottleneck;
}

double drum_buffer_rope_schedule(Job *jobs, int n_jobs,
                                   Machine *machines, int n_machines,
                                   int bottleneck_id) {
    if (!jobs || !machines || bottleneck_id < 0
        || bottleneck_id >= n_machines) return 0.0;

    /* Simple DBR: sequence bottleneck jobs by SPT */
    int *bottleneck_jobs = (int *)malloc((size_t)n_jobs * sizeof(int));
    if (!bottleneck_jobs) return 0.0;

    int bn = 0;
    for (int j = 0; j < n_jobs; j++) {
        for (int o = 0; o < jobs[j].n_operations; o++) {
            if (jobs[j].operations[o].machine_id == bottleneck_id) {
                bottleneck_jobs[bn++] = j;
                break;
            }
        }
    }

    /* Sort bottleneck jobs by processing time on bottleneck */
    for (int i = 0; i < bn - 1; i++)
        for (int j = 0; j < bn - i - 1; j++) {
            double t1 = 0.0, t2 = 0.0;
            for (int o = 0; o < jobs[bottleneck_jobs[j]].n_operations; o++)
                if (jobs[bottleneck_jobs[j]].operations[o].machine_id
                    == bottleneck_id)
                    t1 = jobs[bottleneck_jobs[j]].operations[o].processing_time;
            for (int o = 0; o < jobs[bottleneck_jobs[j+1]].n_operations; o++)
                if (jobs[bottleneck_jobs[j+1]].operations[o].machine_id
                    == bottleneck_id)
                    t2 = jobs[bottleneck_jobs[j+1]].operations[o].processing_time;
            if (t1 > t2) {
                int tmp = bottleneck_jobs[j];
                bottleneck_jobs[j] = bottleneck_jobs[j+1];
                bottleneck_jobs[j+1] = tmp;
            }
        }

    /* Compute makespan for bottleneck-sequenced DBR schedule */
    double makespan = 0.0;
    for (int i = 0; i < bn; i++) {
        int jid = bottleneck_jobs[i];
        for (int o = 0; o < jobs[jid].n_operations; o++)
            makespan += jobs[jid].operations[o].processing_time;
    }

    free(bottleneck_jobs);
    return makespan;
}
