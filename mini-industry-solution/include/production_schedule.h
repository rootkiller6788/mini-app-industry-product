/**
 * mini-industry-solution: production_schedule.h
 *
 * Production Scheduling - Job Shop, Flow Shop, Dispatching, Optimization
 * ======================================================================
 *
 * L1 Definitions: Job, Machine, Schedule, Operation, WorkCenter
 * L2 Core Concepts: Make-to-stock vs make-to-order, Capacity planning
 * L3 Engineering: Job shop scheduling pipeline, Gantt chart data model
 * L4 Standards: Little's Law, Theory of Constraints (Goldratt),
 *               APICS scheduling framework, Lean manufacturing
 * L5 Algorithms: Johnson's Rule (2-machine), NEH heuristic, Priority dispatch
 *                (SPT, EDD, CR, SLACK, LPT), Makespan computation
 * L6 Canonical Problems: JSSP (Job Shop Scheduling Problem), PFSP
 *                (Permutation Flow Shop), Open Shop
 * L7 Applications: APS (Advanced Planning & Scheduling), MES integration
 * L8 Advanced: Constraint programming, Genetic algorithms, Simulated annealing
 * L9 Industry Frontiers: AI-based scheduling, Real-time rescheduling
 */

#ifndef PRODUCTION_SCHEDULE_H
#define PRODUCTION_SCHEDULE_H

#include <stddef.h>
#include <time.h>

/* === L1: Core Definitions === */

typedef enum {
    JOB_RELEASED,
    JOB_QUEUED,
    JOB_SETUP,
    JOB_PROCESSING,
    JOB_WAITING,
    JOB_COMPLETED,
    JOB_ON_HOLD,
    JOB_CANCELLED
} JobStatus;

typedef enum {
    MACHINE_IDLE,
    MACHINE_SETUP,
    MACHINE_RUNNING,
    MACHINE_BLOCKED,
    MACHINE_DOWN,
    MACHINE_MAINTENANCE
} MachineStatus;

typedef enum {
    DISPATCH_SPT,     /* Shortest Processing Time */
    DISPATCH_LPT,     /* Longest Processing Time */
    DISPATCH_EDD,     /* Earliest Due Date */
    DISPATCH_FIFO,    /* First In First Out */
    DISPATCH_LIFO,    /* Last In First Out */
    DISPATCH_CR,      /* Critical Ratio */
    DISPATCH_SLACK,   /* Minimum Slack */
    DISPATCH_WINQ,    /* Work In Next Queue */
    DISPATCH_COVERT   /* Cost Over Time */
} DispatchRule;

typedef struct {
    int job_id;
    int operation_seq;
    int machine_id;
    double processing_time;
    double setup_time;
    struct timespec start_time;
    struct timespec end_time;
    JobStatus status;
} Operation;

typedef struct {
    int job_id;
    char name[48];
    Operation *operations;
    int n_operations;
    struct timespec release_date;
    struct timespec due_date;
    double total_processing_time;
    double remaining_time;
    int priority;
    JobStatus status;
    double completion_time;
    double tardiness;
} Job;

typedef struct {
    int machine_id;
    char name[48];
    MachineStatus status;
    int current_job_id;
    double total_busy_time;
    double total_idle_time;
    int jobs_processed;
    double mttf;
    double mttr;
} Machine;

typedef struct {
    Job *jobs;
    int n_jobs;
    Machine *machines;
    int n_machines;
    double makespan;
    double total_tardiness;
    int n_tardy_jobs;
    double machine_utilization;
    double flow_time_mean;
} Schedule;

/* === L2-L5: Scheduling Algorithms === */

void schedule_init(Schedule *sched, Job *jobs, int n_jobs,
                    Machine *machines, int n_machines);
void schedule_clear(Schedule *sched);

/* Johnson's Rule for 2-machine flow shop (L5) */
int  johnsons_rule_2machine(Job *jobs, int n_jobs,
                              double *m1_times, double *m2_times,
                              int *optimal_sequence);

/* NEH heuristic for permutation flow shop (L5) */
int  neh_heuristic_flowshop(int n_jobs, int n_machines,
                              const double * const *processing_times,
                              int *out_sequence, double *out_makespan);

/* Priority dispatch sequencing (L5) */
int  priority_dispatch_schedule(const Job *jobs, int n_jobs,
                                  DispatchRule rule,
                                  int *out_sequence);

/* Campbell-Dudek-Smith heuristic (L5) */
int  cds_heuristic(int n_jobs, int n_machines,
                     const double * const *processing_times,
                     int *out_sequence, double *out_makespan);

/* Schedule evaluation (L3) */
double makespan_calculate(const Job *jobs, int n_jobs, int n_machines,
                           const int *sequence);
double total_tardiness_calculate(const Job *jobs, int n_jobs,
                                   const int *sequence);
double machine_utilization_calculate(const Schedule *sched);
double mean_flow_time_calculate(const Job *jobs, int n_jobs,
                                  const int *sequence);

/* Job release and feasibility checks (L3) */
int  schedule_feasibility_check(const Schedule *sched);
int  job_release_schedule(Job *jobs, int n_jobs,
                            struct timespec now, int *out_released);

/* Critical ratio calculation (L4) */
double critical_ratio_calculate(const Job *job, struct timespec now);
double slack_time_calculate(const Job *job, struct timespec now);

/* Work center load analysis (L6) */
typedef struct {
    int machine_id;
    double total_load;
    double available_time;
    double load_pct;
    int overloaded;
} WorkCenterLoad;

int work_center_load_analysis(const Machine *machines, int n_machines,
                                const Job *jobs, int n_jobs,
                                double available_time,
                                WorkCenterLoad *out_loads);

/* Batch scheduling (L7) */
typedef struct {
    Job *batch_jobs;
    int batch_size;
    int max_batch_size;
    double processing_time;
    struct timespec start_time;
} BatchJob;

int batch_formation(Job *jobs, int n_jobs, int max_batch_size,
                      BatchJob *out_batches, int max_batches,
                      int *out_n_batches);

/* Theory of Constraints bottleneck analysis (L4) */
int bottleneck_identify(const Machine *machines, int n_machines,
                          const Job *jobs, int n_jobs);
double drum_buffer_rope_schedule(Job *jobs, int n_jobs,
                                   Machine *machines, int n_machines,
                                   int bottleneck_id);

#endif /* PRODUCTION_SCHEDULE_H */
