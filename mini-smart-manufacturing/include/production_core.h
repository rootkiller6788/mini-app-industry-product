#ifndef MINI_SMF_PRODUCTION_CORE_H
#define MINI_SMF_PRODUCTION_CORE_H

#include "smart_manufacturing.h"

/* ================================================================
 *  production_core.h — Production Scheduling & Execution
 *
 *  L2: Core concepts — Production planning (MPS/MRP), Shop floor
 *      control, Capacity planning (RCCP/CRP), Lean pull systems
 *  L3: Engineering structures — Backward/forward scheduling,
 *      Gantt chart operations, dispatching rules engine
 *  L4: Standards — APICS CPIM Body of Knowledge, ISO 22400 KPIs
 *  L5: Algorithms — Johnson's rule (2-machine flow shop), EDD/SPT/
 *      LPT/Moore-Hodgson dispatching, Giffler-Thompson for job shop,
 *      MRP explosion (Orlicky 1975)
 *  L6: Canonical problems — n-job m-machine scheduling, production
 *      line balancing (COMSOAL/Hoffmann), work order dispatching
 *  L7: Applications — APS (Advanced Planning & Scheduling), MES
 *      production dispatching dashboard
 *  L8: Advanced — Constraint-based scheduling, Genetic algorithm
 *      for job shop, Kanban CONWIP pull system
 *  L9: Industry — Siemens Opcenter, SAP PP/DS, Oracle ASCP
 *
 *  References:
 *  - Pinedo, "Scheduling: Theory, Algorithms, and Systems" (2016)
 *  - Johnson, "Optimal two- and three-stage production schedules"
 *    (Naval Research Logistics Quarterly, 1954)
 *  - Orlicky, "Material Requirements Planning" (1975)
 *  - Goldratt, "The Goal" — Theory of Constraints (1984)
 * ================================================================ */

#define SMF_SCHED_MAX_JOBS       256
#define SMF_SCHED_MAX_MACHINES    64
#define SMF_SCHED_MAX_SEQUENCE   512

/* ---- L1: Job/Operation for Scheduling ---- */

typedef struct {
    int     job_id;
    int     product_idx;
    int     quantity;
    int     priority;           /* 0=highest */
    float   release_time;       /* earliest start time (hours from now) */
    float   due_date;           /* hours from now */
    float   processing_times[SMF_SCHED_MAX_MACHINES];  /* per machine */
    int     machine_sequence[SMF_SCHED_MAX_MACHINES];  /* route */
    int     num_operations;
    float   weight;             /* job importance weight */
    float   remaining_work;
    float   slack_time;
    float   critical_ratio;
    char    name[SMF_NAME_LEN];
} SMF_SchedulingJob;

/* ---- L1: Scheduling Result (Gantt entry) ---- */

typedef struct {
    int     job_idx;
    int     machine_idx;
    int     operation_no;
    float   start_time;
    float   end_time;
    float   setup_time;
    bool    is_idle;            /* idle period for machine */
} SMF_ScheduleEntry;

typedef struct {
    SMF_ScheduleEntry entries[SMF_SCHED_MAX_SEQUENCE];
    int               num_entries;
    float             makespan;         /* C_max: completion time of last job */
    float             total_flow_time;
    float             avg_tardiness;
    float             max_tardiness;
    int               tardy_jobs;
    float             machine_utilization[SMF_SCHED_MAX_MACHINES];
    int               num_machines;
} SMF_Schedule;

/* ---- L1: Dispatching Rule Enum ---- */

typedef enum {
    SMF_DISPATCH_FIFO   = 0,  /* First In First Out */
    SMF_DISPATCH_SPT    = 1,  /* Shortest Processing Time */
    SMF_DISPATCH_LPT    = 2,  /* Longest Processing Time */
    SMF_DISPATCH_EDD    = 3,  /* Earliest Due Date */
    SMF_DISPATCH_MS     = 4,  /* Minimum Slack */
    SMF_DISPATCH_CR     = 5,  /* Critical Ratio */
    SMF_DISPATCH_LWR    = 6,  /* Least Work Remaining */
    SMF_DISPATCH_MWR    = 7,  /* Most Work Remaining */
    SMF_DISPATCH_RANDOM = 8,  /* Random */
    SMF_DISPATCH_COVERT = 9   /* Cost OVER Time */
} SMF_DispatchRule;

/* ---- L1: Capacity Planning ---- */

typedef struct {
    int     workstation_idx;
    float   available_hours_per_week;
    float   required_hours_per_week;
    float   load_percent;        /* required / available * 100 */
    float   overload_hours;
    float   underload_hours;
} SMF_CapacityLoad;

/* ---- L1: Work Order Dispatch Result ---- */

typedef struct {
    int     work_order_idx;
    int     machine_idx;
    int     step_idx;
    int     sequence_position;
    float   estimated_start;
    float   estimated_end;
    bool    is_dispatched;
} SMF_DispatchResult;

/* ================================================================
 *  API: Job Shop Scheduling
 * ================================================================ */

/** Create a scheduling job */
void smf_sched_job_init(SMF_SchedulingJob *job, int job_id,
                        const char *name, int num_ops);

/** Set processing time for an operation on a machine */
void smf_sched_job_set_op(SMF_SchedulingJob *job, int op_idx,
                           int machine_idx, float proc_time);

/** Set machine sequence (routing) for the job */
void smf_sched_job_set_route(SMF_SchedulingJob *job,
                              const int *machine_seq, int num_ops);

/**
 * Single-machine dispatching.
 * Sorts jobs by the given rule and schedules them sequentially.
 * Complexity: O(n log n) for sorting.
 */
SMF_Schedule smf_schedule_single_machine(SMF_SchedulingJob *jobs,
                                          int num_jobs,
                                          SMF_DispatchRule rule,
                                          int machine_idx);

/**
 * Johnson's Rule for 2-machine flow shop (optimal).
 * Minimizes makespan for n jobs on 2 machines in series.
 * Each job: process on M1 then M2. No preemption.
 *
 * Theorem (Johnson 1954): For the 2-machine flow shop problem F2||C_max,
 * sequencing jobs so that min(A_i, B_j) <= min(A_j, B_i) for i < j
 * yields optimal makespan.
 *
 * Complexity: O(n log n)
 */
SMF_Schedule smf_schedule_johnson_2machine(SMF_SchedulingJob *jobs,
                                            int num_jobs);

/**
 * Generic job shop dispatching with priority rules.
 * Implements Giffler-Thompson active schedule generation.
 * Complexity: O(n * m^2 * n) approximate
 */
SMF_Schedule smf_schedule_job_shop(SMF_SchedulingJob *jobs, int num_jobs,
                                    int num_machines, SMF_DispatchRule rule);

/* ================================================================
 *  API: Schedule Analysis (L4: ISO 22400 KPIs)
 * ================================================================ */

/** Calculate makespan (C_max) of a schedule */
float smf_schedule_makespan(const SMF_Schedule *schedule);

/** Calculate average flow time */
float smf_schedule_avg_flow_time(const SMF_Schedule *schedule);

/** Calculate number of tardy jobs */
int  smf_schedule_tardy_count(const SMF_Schedule *schedule,
                               const SMF_SchedulingJob *jobs, int num_jobs);

/** Calculate maximum tardiness */
float smf_schedule_max_tardiness(const SMF_Schedule *schedule,
                                  const SMF_SchedulingJob *jobs, int num_jobs);

/** Calculate machine utilization */
float smf_schedule_machine_util(const SMF_Schedule *schedule, int machine_idx);

/* ================================================================
 *  API: Work Order Management
 * ================================================================ */

/** Create a new work order */
SMF_WorkOrder* smf_wo_create(SMF_Factory *factory, int product_idx,
                              int quantity, time_t planned_start,
                              time_t planned_end, int priority);

/** Release a work order to the shop floor */
int  smf_wo_release(SMF_Factory *factory, int wo_idx);

/** Record completion of a routing step */
int  smf_wo_complete_step(SMF_Factory *factory, int wo_idx,
                           int step_idx, int qty_good, int qty_scrap);

/** Close a completed work order */
int  smf_wo_close(SMF_Factory *factory, int wo_idx);

/** Find next work order to dispatch (based on priority + rule) */
int  smf_wo_find_next(const SMF_Factory *factory, int workstation_idx,
                       SMF_DispatchRule rule);

/** Calculate work order progress percentage */
float smf_wo_progress(const SMF_WorkOrder *wo);

/* ================================================================
 *  API: Capacity Planning & CRP
 * ================================================================ */

/** Calculate capacity load for each workstation (CRP) */
int  smf_capacity_load_profile(const SMF_Factory *factory,
                                SMF_CapacityLoad *loads, int max_ws);

/** Calculate overload level for a specific work center */
float smf_capacity_overload(const SMF_CapacityLoad *load);

/** Identify overloaded work centers (> 100% load) */
int  smf_capacity_bottlenecks(const SMF_CapacityLoad *loads, int count,
                               int *bottleneck_indices, int max_results);

/* ================================================================
 *  API: Takt Time & Line Balancing
 * ================================================================ */

/**
 * Calculate takt time:
 *   Takt Time = Available Production Time / Customer Demand
 */
float smf_calc_takt_time(float available_time_min, float customer_demand);

/**
 * Calculate minimum number of workstations for given cycle time:
 *   N_min = ceil(Sum of task times / Cycle Time)
 */
int  smf_calc_min_workstations(const float *task_times, int num_tasks,
                                float cycle_time);

/** Balance a production line using Largest Candidate Rule (LCR) */
int  smf_line_balance_lcr(const float *task_times,
                           const int *precedence[2], int num_tasks,
                           int num_prec_edges, float cycle_time,
                           int *station_assignments);

/* Utility */
const char* smf_dispatch_rule_name(SMF_DispatchRule rule);

#endif