#ifndef MINI_ENT_HRM_SYSTEM_H
#define MINI_ENT_HRM_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* ================================================================
 *  HRM System — Human Resource Management
 *
 *  L1: Core types — Employee, Department, Position, Payroll
 *  L2: Core concepts — Organizational hierarchy, compensation,
 *      performance management, attendance
 *  L3: Engineering structures — Org tree, payroll calculation
 *      pipeline, approval workflow state machine
 *  L4: Standards — FLSA, ISO 30414 (Human Capital Reporting),
 *      local labor laws (China: 劳动合同法)
 *  L5: Algorithms — Salary calculation, leave accrual,
 *      attendance processing, org chart traversal
 *  L6: Canonical problems — Payroll run, attendance reconciliation
 *  L7: Applications — SME HR system, workforce planning
 *  L8: Advanced — People analytics, succession planning
 *  L9: Industry — Workday, SAP SuccessFactors, HR SaaS
 * ================================================================ */

#define MENT_HRM_MAX_EMPLOYEES   16384
#define MENT_HRM_MAX_DEPARTMENTS 512
#define MENT_HRM_MAX_POSITIONS   2048
#define MENT_HRM_MAX_LEAVE_TYPES 16

typedef enum {
    MENT_EMP_ACTIVE     = 0,
    MENT_EMP_ON_LEAVE   = 1,
    MENT_EMP_SUSPENDED  = 2,
    MENT_EMP_TERMINATED = 3,
    MENT_EMP_RETIRED    = 4
} MENT_EmployeeStatus;

typedef enum {
    MENT_EMP_FULLTIME   = 0,
    MENT_EMP_PARTTIME   = 1,
    MENT_EMP_CONTRACTOR = 2,
    MENT_EMP_INTERN     = 3
} MENT_EmploymentType;

typedef struct {
    int      id;
    char     code[16];
    char     name[64];
    int      parent_dept_id;
    int      manager_emp_id;
    int      level;
    int      headcount;
    bool     is_active;
} MENT_Department;

typedef struct {
    int      id;
    int      emp_id;
    char     first_name[32];
    char     last_name[32];
    char     email[64];
    char     phone[24];
    time_t   hire_date;
    time_t   termination_date;
    MENT_EmployeeStatus status;
    MENT_EmploymentType emp_type;
    int      dept_id;
    int      position_id;
    int      supervisor_id;
    float    salary;            /* annual base */
    float    hourly_rate;
    int      leave_balance[MENT_HRM_MAX_LEAVE_TYPES];
    char     bank_account[32];
    char     tax_id[24];
} MENT_Employee;

typedef struct {
    int      id;
    char     title[64];
    char     code[16];
    int      grade;
    float    min_salary;
    float    max_salary;
    int      dept_id;
} MENT_Position;

typedef struct {
    int      id;
    char     name[32];
    float    accrual_rate;      /* days per year */
    int      max_balance;
    bool     is_paid;
    bool     can_carryover;
} MENT_LeaveType;

typedef enum {
    MENT_ATTEND_PRESENT   = 0,
    MENT_ATTEND_ABSENT    = 1,
    MENT_ATTEND_LATE      = 2,
    MENT_ATTEND_HALF_DAY  = 3,
    MENT_ATTEND_HOLIDAY   = 4,
    MENT_ATTEND_OVERTIME  = 5
} MENT_AttendanceType;

typedef struct {
    int      emp_id;
    time_t   date;
    time_t   check_in;
    time_t   check_out;
    MENT_AttendanceType type;
    float    hours_worked;
    float    overtime_hours;
} MENT_Attendance;

typedef struct {
    int      emp_id;
    int      year;
    int      month;
    float    basic_salary;
    float    overtime_pay;
    float    bonus;
    float    allowance;
    float    deductions;       /* tax, social insurance */
    float    net_pay;
    int      leave_days;
    int      absent_days;
    bool     is_processed;
    time_t   processed_at;
} MENT_Payroll;

typedef struct {
    MENT_Employee     employees[MENT_HRM_MAX_EMPLOYEES];
    int               num_employees;
    MENT_Department   departments[MENT_HRM_MAX_DEPARTMENTS];
    int               num_departments;
    MENT_Position     positions[MENT_HRM_MAX_POSITIONS];
    int               num_positions;
    MENT_LeaveType    leave_types[MENT_HRM_MAX_LEAVE_TYPES];
    int               num_leave_types;
    MENT_Attendance   *attendances;
    int               num_attendances;
    MENT_Payroll      *payrolls;
    int               num_payrolls;
    float             tax_rate;
    float             social_insurance_rate;
    float             overtime_rate;     /* multiplier */
    int               standard_hours_per_day;
} MENT_HRMInstance;

/* ---- API ---- */

MENT_HRMInstance* ment_hrm_create(void);
void ment_hrm_destroy(MENT_HRMInstance *hrm);

/** Department management */
int  ment_hrm_add_department(MENT_HRMInstance *hrm, const char *code,
                              const char *name, int parent_id);
int  ment_hrm_find_department(const MENT_HRMInstance *hrm, const char *code);

/** Employee management */
int  ment_hrm_add_employee(MENT_HRMInstance *hrm,
                            const char *first, const char *last,
                            int dept_id, float salary, time_t hire_date);
int  ment_hrm_find_employee(const MENT_HRMInstance *hrm, const char *email);

/** Get department org chart (reporting hierarchy) */
int  ment_hrm_org_chart(const MENT_HRMInstance *hrm,
                         int *hierarchy, int max_levels);

/** Attendance recording */
int  ment_hrm_record_attendance(MENT_HRMInstance *hrm, int emp_id,
                                 time_t date, time_t check_in, time_t check_out);

/** Leave management */
int  ment_hrm_request_leave(MENT_HRMInstance *hrm, int emp_id,
                             int leave_type, time_t start, time_t end);

/**
 * Payroll calculation (L5):
 * Gross = Basic + OT + Bonus + Allowances
 * Deductions = Tax + Social Insurance
 * Tax brackets (simplified progressive):
 *   0-5000: 0%, 5000-12000: 3%, 12000-25000: 10%, 25000-35000: 20%,
 *   35000-55000: 25%, 55000-80000: 30%, 80000+: 35%
 */
int  ment_hrm_run_payroll(MENT_HRMInstance *hrm, int year, int month);

/** Get employee payroll slip */
int  ment_hrm_get_payroll(const MENT_HRMInstance *hrm, int emp_id,
                           int year, int month, MENT_Payroll *result);

/** Workforce analytics */
int  ment_hrm_headcount_by_dept(const MENT_HRMInstance *hrm,
                                 int dept_id);
float ment_hrm_total_compensation(const MENT_HRMInstance *hrm);
float ment_hrm_avg_tenure(const MENT_HRMInstance *hrm);  /* years */

#endif /* MINI_ENT_HRM_SYSTEM_H */
