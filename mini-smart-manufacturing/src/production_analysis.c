/* production_analysis.c - Schedule Analysis Functions
 * L4: ISO 22400 KPIs for manufacturing operations
 */
#include "production_core.h"
#include <string.h>
#include <math.h>
#define M_EPS 0.001f

float smf_schedule_makespan(const SMF_Schedule *sched) {
    return sched ? sched->makespan : 0.0f;
}

float smf_schedule_avg_flow_time(const SMF_Schedule *sched) {
    if (!sched || sched->num_entries <= 0) return 0.0f;
    float job_end[SMF_SCHED_MAX_JOBS];
    memset(job_end, 0, sizeof(job_end));
    for (int i = 0; i < sched->num_entries; i++) {
        int jid = sched->entries[i].job_idx;
        if (jid < 0 || jid >= SMF_SCHED_MAX_JOBS) continue;
        if (sched->entries[i].end_time > job_end[jid]) {
            job_end[jid] = sched->entries[i].end_time;
        }
    }
    float total = 0.0f;
    int count = 0;
    for (int j = 0; j < SMF_SCHED_MAX_JOBS; j++) {
        if (job_end[j] > 0.0f) {
            total += job_end[j];
            count++;
        }
    }
    return count > 0 ? total / (float)count : 0.0f;
}

int smf_schedule_tardy_count(const SMF_Schedule *sched,
                              const SMF_SchedulingJob *jobs, int num_jobs) {
    if (!sched || !jobs) return 0;
    float job_end[SMF_SCHED_MAX_JOBS];
    memset(job_end, 0, sizeof(job_end));
    for (int i = 0; i < sched->num_entries; i++) {
        int jid = sched->entries[i].job_idx;
        if (jid < 0 || jid >= SMF_SCHED_MAX_JOBS) continue;
        if (sched->entries[i].end_time > job_end[jid]) {
            job_end[jid] = sched->entries[i].end_time;
        }
    }
    int tardy = 0;
    for (int j = 0; j < num_jobs; j++) {
        int jid = jobs[j].job_id;
        if (jid < 0 || jid >= SMF_SCHED_MAX_JOBS) continue;
        if (job_end[jid] > jobs[j].due_date + M_EPS) {
            tardy++;
        }
    }
    return tardy;
}

float smf_schedule_max_tardiness(const SMF_Schedule *sched,
                                  const SMF_SchedulingJob *jobs, int num_jobs) {
    if (!sched || !jobs) return 0.0f;
    float job_end[SMF_SCHED_MAX_JOBS];
    memset(job_end, 0, sizeof(job_end));
    for (int i = 0; i < sched->num_entries; i++) {
        int jid = sched->entries[i].job_idx;
        if (jid < 0 || jid >= SMF_SCHED_MAX_JOBS) continue;
        if (sched->entries[i].end_time > job_end[jid]) {
            job_end[jid] = sched->entries[i].end_time;
        }
    }
    float max_tardy = 0.0f;
    for (int j = 0; j < num_jobs; j++) {
        int jid = jobs[j].job_id;
        if (jid < 0 || jid >= SMF_SCHED_MAX_JOBS) continue;
        float tardiness = job_end[jid] - jobs[j].due_date;
        if (tardiness > max_tardy) {
            max_tardy = tardiness;
        }
    }
    return max_tardy > 0.0f ? max_tardy : 0.0f;
}

float smf_schedule_machine_util(const SMF_Schedule *sched, int machine_idx) {
    if (!sched || machine_idx < 0 || machine_idx >= SMF_SCHED_MAX_MACHINES)
        return 0.0f;
    return sched->machine_utilization[machine_idx];
}