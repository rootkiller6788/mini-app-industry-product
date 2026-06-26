/* example_workforce.c - HRM: Org chart, attendance, payroll run */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "hrm_system.h"

int main(void) {
  printf("=== Workforce HRM Demo ===\n\n");
  MENT_HRMInstance *hrm = ment_hrm_create();

  /* 1. Departments */
  int corp  = ment_hrm_add_department(hrm, "CORP", "Corporate", 0);
  int eng   = ment_hrm_add_department(hrm, "ENG", "Engineering", corp);
  int mkt   = ment_hrm_add_department(hrm, "MKT", "Marketing", corp);
  int qa    = ment_hrm_add_department(hrm, "QA", "Quality Assurance", eng);
  printf("[1] Departments: %d (CORP->ENG->QA, CORP->MKT)\n", hrm->num_departments);

  /* 2. Employees */
  int e1 = ment_hrm_add_employee(hrm, "Alice", "Johnson", eng, 150000.0f, 1700000000);
  int e2 = ment_hrm_add_employee(hrm, "Bob", "Smith", eng, 120000.0f, 1710000000);
  int e3 = ment_hrm_add_employee(hrm, "Carol", "Davis", qa, 100000.0f, 1720000000);
  int e4 = ment_hrm_add_employee(hrm, "Dave", "Wilson", mkt, 110000.0f, 1705000000);
  int e5 = ment_hrm_add_employee(hrm, "Eve", "Brown", eng, 130000.0f, 1730000000);
  printf("[2] Employees: %d hired\n", hrm->num_employees);
  printf("    Headcount by dept: ENG=%d, MKT=%d, QA=%d\n",
         ment_hrm_headcount_by_dept(hrm, eng),
         ment_hrm_headcount_by_dept(hrm, mkt),
         ment_hrm_headcount_by_dept(hrm, qa));

  /* 3. Org Chart */
  int hierarchy[32] = {0};
  int n = ment_hrm_org_chart(hrm, hierarchy, 32);
  printf("[3] Organization chart (%d levels):\n", n);
  for (int i = 0; i < n; i++) printf("    Level %d: %d departments\n", i, hierarchy[i]);

  /* 4. Attendance */
  time_t now = time(NULL);
  for (int i = 0; i < 5; i++) {
    time_t checkin = now - 3600 * 8;
    time_t checkout = now;
    ment_hrm_record_attendance(hrm, 1001 + i, now, checkin, checkout);
  }
  printf("[4] Attendance: %d records\n", hrm->num_attendances);

  /* 5. Leave */
  int leave_days = ment_hrm_request_leave(hrm, e1, 0, now, now + 86400 * 4);
  printf("[5] Leave: Alice took %d days\n", leave_days);

  /* 6. Payroll Run */
  printf("[6] Running March 2025 payroll...\n");
  int n_pay = ment_hrm_run_payroll(hrm, 2025, 3);
  printf("    %d payroll records generated\n", n_pay);

  /* 7. Pay Slips */
  printf("\n[7] March 2025 Pay Slips:\n");
  printf("    Employee         | Basic    | Overtime | Deductions | Net Pay\n");
  printf("    -----------------+----------+----------+------------+----------\n");
  for (int i = 0; i < hrm->num_employees; i++) {
    if (hrm->employees[i].status != MENT_EMP_ACTIVE) continue;
    MENT_Payroll pr;
    ment_hrm_get_payroll(hrm, hrm->employees[i].id, 2025, 3, &pr);
    printf("    %-16s | $%7.2f | $%7.2f | $%8.2f | $%7.2f\n",
           hrm->employees[i].first_name,
           pr.basic_salary, pr.overtime_pay, pr.deductions, pr.net_pay);
  }

  /* 8. Workforce Analytics */
  float total_comp = ment_hrm_total_compensation(hrm);
  float avg_tenure = ment_hrm_avg_tenure(hrm);
  printf("\n[8] Workforce Analytics:\n");
  printf("    Total Annual Compensation: $%.2f\n", total_comp);
  printf("    Average Tenure: %.2f years\n", avg_tenure);

  printf("\n=== HRM Demo Complete ===\n");
  ment_hrm_destroy(hrm);
  return 0;
}
