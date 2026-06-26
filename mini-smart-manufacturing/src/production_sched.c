/* production_sched.c - Job init, dispatch comparators, single machine sched */
#include "production_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#define M_EPS 0.001f

const char* smf_dispatch_rule_name(SMF_DispatchRule r) {
    switch(r){
        case SMF_DISPATCH_FIFO:return"FIFO";case SMF_DISPATCH_SPT:return"SPT";
        case SMF_DISPATCH_LPT:return"LPT";case SMF_DISPATCH_EDD:return"EDD";
        case SMF_DISPATCH_MS:return"Min Slack";case SMF_DISPATCH_CR:return"Critical Ratio";
        case SMF_DISPATCH_LWR:return"Least Work Remaining";case SMF_DISPATCH_MWR:return"Most Work Remaining";
        case SMF_DISPATCH_RANDOM:return"Random";case SMF_DISPATCH_COVERT:return"COVERT";
        default:return"Unknown";
    }
}

void smf_sched_job_init(SMF_SchedulingJob *j, int id, const char *nm, int no) {
    if (!j) return;
    memset(j, 0, sizeof(SMF_SchedulingJob));
    j->job_id = id;
    if (nm) strncpy(j->name, nm, SMF_NAME_LEN - 1);
    j->num_operations = (no > SMF_SCHED_MAX_MACHINES) ? SMF_SCHED_MAX_MACHINES : no;
    j->priority = 50; j->weight = 1.0f; j->release_time = 0.0f; j->due_date = 100.0f;
    for (int i = 0; i < SMF_SCHED_MAX_MACHINES; i++) {
        j->processing_times[i] = 0.0f; j->machine_sequence[i] = i;
    }
}

void smf_sched_job_set_op(SMF_SchedulingJob *j, int o, int m, float p) {
    if (!j || o < 0 || o >= SMF_SCHED_MAX_MACHINES) return;
    if (m < 0 || m >= SMF_SCHED_MAX_MACHINES) return;
    j->processing_times[o] = p; j->machine_sequence[o] = m;
}

void smf_sched_job_set_route(SMF_SchedulingJob *j, const int *ms, int no) {
    if (!j || !ms) return;
    int n = (no < SMF_SCHED_MAX_MACHINES) ? no : SMF_SCHED_MAX_MACHINES;
    j->num_operations = n;
    for (int i = 0; i < n; i++) j->machine_sequence[i] = ms[i];
}

typedef int(*cmf)(const void*, const void*);
static float _sct = 0.0f;
static int _cf(const void*a,const void*b){return((const SMF_SchedulingJob*)a)->release_time>((const SMF_SchedulingJob*)b)->release_time?1:-1;}
static int _cs(const void*a,const void*b){float p=((const SMF_SchedulingJob*)a)->processing_times[0],q=((const SMF_SchedulingJob*)b)->processing_times[0];return p>q?1:(p<q?-1:0);}
static int _cl(const void*a,const void*b){float p=((const SMF_SchedulingJob*)a)->processing_times[0],q=((const SMF_SchedulingJob*)b)->processing_times[0];return p<q?1:(p>q?-1:0);}
static int _ce(const void*a,const void*b){return((const SMF_SchedulingJob*)a)->due_date>((const SMF_SchedulingJob*)b)->due_date?1:-1;}
static int _cm(const void*a,const void*b){const SMF_SchedulingJob*ja=a,*jb=b;float sa=ja->due_date-_sct-ja->remaining_work,sb=jb->due_date-_sct-jb->remaining_work;return sa>sb?1:-1;}
static int _cc(const void*a,const void*b){const SMF_SchedulingJob*ja=a,*jb=b;float ca=ja->remaining_work>0.0f?(ja->due_date-_sct)/ja->remaining_work:FLT_MAX,cb=jb->remaining_work>0.0f?(jb->due_date-_sct)/jb->remaining_work:FLT_MAX;return ca>cb?1:-1;}

SMF_Schedule smf_schedule_single_machine(SMF_SchedulingJob *jobs, int n,
                                          SMF_DispatchRule r, int mi) {
    SMF_Schedule s; memset(&s, 0, sizeof(s)); s.num_machines = 1;
    if (!jobs || n <= 0) return s;
    SMF_SchedulingJob *sr = malloc(n * sizeof(SMF_SchedulingJob));
    if (!sr) return s;
    memcpy(sr, jobs, n * sizeof(SMF_SchedulingJob));
    for (int i = 0; i < n; i++) sr[i].remaining_work = sr[i].processing_times[0];
    cmf cmp = _cf;
    switch (r) {
        case SMF_DISPATCH_SPT: cmp = _cs; break;
        case SMF_DISPATCH_LPT: cmp = _cl; break;
        case SMF_DISPATCH_EDD: cmp = _ce; break;
        case SMF_DISPATCH_MS:  cmp = _cm; break;
        case SMF_DISPATCH_CR:  cmp = _cc; break;
        default: break;
    }
    qsort(sr, n, sizeof(SMF_SchedulingJob), cmp);
    float t = 0.0f;
    for (int i = 0; i < n; i++) {
        float p = sr[i].processing_times[0];
        s.entries[i].job_idx = sr[i].job_id;
        s.entries[i].machine_idx = mi;
        s.entries[i].operation_no = 0;
        s.entries[i].start_time = t;
        t += p;
        s.entries[i].end_time = t;
        s.num_entries++;
    }
    s.makespan = t; s.total_flow_time = t;
    s.machine_utilization[0] = (t > 0.0f) ? 1.0f : 0.0f;
    free(sr); return s;
}