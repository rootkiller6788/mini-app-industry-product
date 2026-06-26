#include "hrm_system.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ================================================================
 * HRM System Implementation
 *
 * L2: Org hierarchy, compensation, performance, attendance
 * L3: Org tree, payroll pipeline
 * L5: Salary calc, leave accrual, attendance processing
 * L6: Payroll run, attendance reconciliation
 * L7: SME HR system, workforce planning
 * ================================================================ */

MENT_HRMInstance* ment_hrm_create(void) {
    MENT_HRMInstance *hrm = (MENT_HRMInstance*)malloc(sizeof(MENT_HRMInstance));
    if (!hrm) return NULL;
    memset(hrm, 0, sizeof(MENT_HRMInstance));
    hrm->attendances = (MENT_Attendance*)malloc(
        MENT_HRM_MAX_EMPLOYEES * 365 * sizeof(MENT_Attendance));
    hrm->payrolls = (MENT_Payroll*)malloc(
        MENT_HRM_MAX_EMPLOYEES * 12 * sizeof(MENT_Payroll));
    if (!hrm->attendances || !hrm->payrolls) {
        free(hrm->attendances); free(hrm->payrolls); free(hrm); return NULL;
    }
    memset(hrm->attendances, 0, MENT_HRM_MAX_EMPLOYEES * 365 * sizeof(MENT_Attendance));
    memset(hrm->payrolls, 0, MENT_HRM_MAX_EMPLOYEES * 12 * sizeof(MENT_Payroll));
    hrm->tax_rate = 0.15f;
    hrm->social_insurance_rate = 0.105f;
    hrm->overtime_rate = 1.5f;
    hrm->standard_hours_per_day = 8;
    return hrm;
}

void ment_hrm_destroy(MENT_HRMInstance *hrm) {
    if (!hrm) return;
    free(hrm->attendances);
    free(hrm->payrolls);
    free(hrm);
}

int ment_hrm_add_department(MENT_HRMInstance *hrm, const char *code,
                             const char *name, int parent_id) {
    if (hrm->num_departments >= MENT_HRM_MAX_DEPARTMENTS) return -1;
    int idx = hrm->num_departments++;
    MENT_Department *dept = &hrm->departments[idx];
    memset(dept, 0, sizeof(MENT_Department));
    dept->id = idx + 1;
    dept->parent_dept_id = parent_id;
    dept->is_active = true;
    /* Compute level from parent */
    if (parent_id > 0) {
        for (int i = 0; i < hrm->num_departments - 1; i++) {
            if (hrm->departments[i].id == parent_id) {
                dept->level = hrm->departments[i].level + 1;
                hrm->departments[i].headcount++;
                break;
            }
        }
    } else {
        dept->level = 0;
    }
    snprintf(dept->code, sizeof(dept->code), "%s", code);
    snprintf(dept->name, sizeof(dept->name), "%s", name);
    return dept->id;
}

int ment_hrm_find_department(const MENT_HRMInstance *hrm, const char *code) {
    for (int i = 0; i < hrm->num_departments; i++)
        if (strcmp(hrm->departments[i].code, code) == 0)
            return hrm->departments[i].id;
    return -1;
}

int ment_hrm_add_employee(MENT_HRMInstance *hrm, const char *first,
                           const char *last, int dept_id, float salary,
                           time_t hire_date) {
    if (hrm->num_employees >= MENT_HRM_MAX_EMPLOYEES) return -1;
    int idx = hrm->num_employees++;
    MENT_Employee *emp = &hrm->employees[idx];
    memset(emp, 0, sizeof(MENT_Employee));
    emp->id = idx + 1001;
    emp->status = MENT_EMP_ACTIVE;
    emp->emp_type = MENT_EMP_FULLTIME;
    emp->dept_id = dept_id;
    emp->salary = salary;
    emp->hire_date = hire_date;
    emp->hourly_rate = salary / 2080.0f; /* 40h * 52 weeks */
    for (int i = 0; i < MENT_HRM_MAX_LEAVE_TYPES; i++)
        emp->leave_balance[i] = 0;
    snprintf(emp->first_name, sizeof(emp->first_name), "%s", first);
    snprintf(emp->last_name, sizeof(emp->last_name), "%s", last);
    /* Generate email: first.last@company.com */
    snprintf(emp->email, sizeof(emp->email), "%s.%s@company.com", first, last);
    return emp->id;
}

int ment_hrm_find_employee(const MENT_HRMInstance *hrm, const char *email) {
    for (int i = 0; i < hrm->num_employees; i++)
        if (strcasecmp(hrm->employees[i].email, email) == 0)
            return hrm->employees[i].id;
    return -1;
}

int ment_hrm_org_chart(const MENT_HRMInstance *hrm,
                        int *hierarchy, int max_levels) {
    /* Build level distribution */
    int levels[32] = {0};
    int max_lvl = 0;
    for (int i = 0; i < hrm->num_departments; i++) {
        int lvl = hrm->departments[i].level;
        if (lvl < 32) levels[lvl]++;
        if (lvl > max_lvl) max_lvl = lvl;
    }
    for (int i = 0; i <= max_lvl && i < max_levels; i++)
        hierarchy[i] = levels[i];
    return (max_lvl + 1 < max_levels) ? max_lvl + 1 : max_levels;
}

int ment_hrm_record_attendance(MENT_HRMInstance *hrm, int emp_id,
                                time_t date, time_t check_in, time_t check_out) {
    int idx = -1;
    for (int i = 0; i < hrm->num_employees; i++)
        if (hrm->employees[i].id == emp_id) { idx = i; break; }
    if (idx < 0) return -1;

    int att = hrm->num_attendances++;
    MENT_Attendance *a = &hrm->attendances[att];
    a->emp_id = emp_id;
    a->date = date;
    a->check_in = check_in;
    a->check_out = check_out;

    double hours = difftime(check_out, check_in) / 3600.0;
    a->hours_worked = (float)hours;
    a->overtime_hours = 0.0f;

    /* Determine attendance type */
    struct tm tm_in;
#ifdef _WIN32
    localtime_s(&tm_in, &check_in);
#else
    localtime_r(&check_in, &tm_in);
#endif
    if (tm_in.tm_hour > 9) a->type = MENT_ATTEND_LATE;
    else if (hours < 4.0) a->type = MENT_ATTEND_HALF_DAY;
    else if (hours < 1.0) a->type = MENT_ATTEND_ABSENT;
    else a->type = MENT_ATTEND_PRESENT;

    /* Overtime calculation */
    if (hours > (float)hrm->standard_hours_per_day)
        a->overtime_hours = (float)(hours - (double)hrm->standard_hours_per_day);

    return 0;
}

int ment_hrm_request_leave(MENT_HRMInstance *hrm, int emp_id,
                            int leave_type, time_t start, time_t end) {
    int idx = -1;
    for (int i = 0; i < hrm->num_employees; i++)
        if (hrm->employees[i].id == emp_id) { idx = i; break; }
    if (idx < 0 || leave_type < 0 || leave_type >= MENT_HRM_MAX_LEAVE_TYPES)
        return -1;

    int days = (int)(difftime(end, start) / 86400.0) + 1;
    hrm->employees[idx].leave_balance[leave_type] -= days;
    return days;
}

/* Simplified progressive tax rate brackets */
static float calculate_tax(float taxable_monthly) {
    if (taxable_monthly <= 5000.0f) return 0.0f;
    if (taxable_monthly <= 12000.0f) return (taxable_monthly - 5000.0f) * 0.03f;
    if (taxable_monthly <= 25000.0f) return 7000.0f * 0.03f + (taxable_monthly - 12000.0f) * 0.10f;
    if (taxable_monthly <= 35000.0f) return 7000.0f * 0.03f + 13000.0f * 0.10f + (taxable_monthly - 25000.0f) * 0.20f;
    if (taxable_monthly <= 55000.0f) return 7000.0f * 0.03f + 13000.0f * 0.10f + 10000.0f * 0.20f + (taxable_monthly - 35000.0f) * 0.25f;
    if (taxable_monthly <= 80000.0f) return 7000.0f * 0.03f + 13000.0f * 0.10f + 10000.0f * 0.20f + 20000.0f * 0.25f + (taxable_monthly - 55000.0f) * 0.30f;
    return 7000.0f * 0.03f + 13000.0f * 0.10f + 10000.0f * 0.20f + 20000.0f * 0.25f + 25000.0f * 0.30f + (taxable_monthly - 80000.0f) * 0.35f;
}

int ment_hrm_run_payroll(MENT_HRMInstance *hrm, int year, int month) {
    for (int i = 0; i < hrm->num_employees; i++) {
        MENT_Employee *emp = &hrm->employees[i];
        if (emp->status != MENT_EMP_ACTIVE) continue;

        int idx = hrm->num_payrolls++;
        MENT_Payroll *pr = &hrm->payrolls[idx];
        memset(pr, 0, sizeof(MENT_Payroll));

        pr->emp_id = emp->id;
        pr->year = year;
        pr->month = month;
        pr->basic_salary = emp->salary / 12.0f;
        pr->overtime_pay = 0.0f;
        pr->bonus = 0.0f;
        pr->allowance = 0.0f;
        pr->leave_days = 0;
        pr->absent_days = 0;

        /* Calculate social insurance on basic salary */
        float si = pr->basic_salary * hrm->social_insurance_rate;
        /* Tax on (basic - threshold - SI) */
        float taxable = pr->basic_salary - 5000.0f - si;
        if (taxable < 0) taxable = 0;
        float tax = calculate_tax(pr->basic_salary);

        pr->deductions = si + tax;
        pr->net_pay = pr->basic_salary + pr->overtime_pay + pr->bonus +
                      pr->allowance - pr->deductions;
        pr->is_processed = true;
        pr->processed_at = time(NULL);
    }
    return hrm->num_payrolls;
}

int ment_hrm_get_payroll(const MENT_HRMInstance *hrm, int emp_id,
                          int year, int month, MENT_Payroll *result) {
    for (int i = 0; i < hrm->num_payrolls; i++) {
        const MENT_Payroll *pr = &hrm->payrolls[i];
        if (pr->emp_id == emp_id && pr->year == year && pr->month == month) {
            if (result) memcpy(result, pr, sizeof(MENT_Payroll));
            return 0;
        }
    }
    return -1;
}

int ment_hrm_headcount_by_dept(const MENT_HRMInstance *hrm, int dept_id) {
    int count = 0;
    for (int i = 0; i < hrm->num_employees; i++)
        if (hrm->employees[i].dept_id == dept_id) count++;
    return count;
}

float ment_hrm_total_compensation(const MENT_HRMInstance *hrm) {
    float total = 0.0f;
    for (int i = 0; i < hrm->num_employees; i++)
        total += hrm->employees[i].salary;
    return total;
}

float ment_hrm_avg_tenure(const MENT_HRMInstance *hrm) {
    if (hrm->num_employees == 0) return 0.0f;
    time_t now = time(NULL);
    double total_years = 0.0;
    for (int i = 0; i < hrm->num_employees; i++)
        total_years += difftime(now, hrm->employees[i].hire_date) / 31536000.0;
    return (float)(total_years / (double)hrm->num_employees);
}
