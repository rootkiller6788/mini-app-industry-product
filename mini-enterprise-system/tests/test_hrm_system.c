/* test_hrm_system.c - HRM System Test Suite (10 tests) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "hrm_system.h"

#define AF(a,b) do{float _a=(a),_b=(b); if(fabsf(_a-_b)>0.01f){fprintf(stderr,"FAIL %.4f!=%.4f\n",_a,_b);assert(0);}}while(0)

int test_hrm_suite(void) {
  printf("  Running HRM tests...\n");
  /*1: Instance */
  {MENT_HRMInstance*h=ment_hrm_create();assert(h);assert(h->standard_hours_per_day==8);AF(h->tax_rate,0.15f);AF(h->overtime_rate,1.5f);ment_hrm_destroy(h);ment_hrm_destroy(NULL);}
  /*2: Departments */
  {MENT_HRMInstance*h=ment_hrm_create();int d1=ment_hrm_add_department(h,"D001","Engineering",0);int d2=ment_hrm_add_department(h,"D002","QA",d1);assert(d1==1);assert(d2==2);assert(ment_hrm_find_department(h,"D001")==1);assert(ment_hrm_find_department(h,"NOEXIST")==-1);assert(h->departments[0].level==0);assert(h->departments[1].level==1);ment_hrm_destroy(h);}
  /*3: Employees */
  {MENT_HRMInstance*h=ment_hrm_create();int d=ment_hrm_add_department(h,"D001","Engineering",0);int e1=ment_hrm_add_employee(h,"Alice","Smith",d,120000.0f,1700000000);int e2=ment_hrm_add_employee(h,"Bob","Jones",d,90000.0f,1710000000);assert(e1==1001);assert(e2==1002);assert(ment_hrm_find_employee(h,"alice.smith@company.com")==1001);assert(ment_hrm_find_employee(h,"none@test.com")==-1);assert(h->num_employees==2);assert(h->employees[0].hourly_rate>0);ment_hrm_destroy(h);}
  /*4: Org chart */
  {MENT_HRMInstance*h=ment_hrm_create();ment_hrm_add_department(h,"D001","Eng",0);ment_hrm_add_department(h,"D002","QA",1);ment_hrm_add_department(h,"D003","Dev",1);int hierarchy[32];int n=ment_hrm_org_chart(h,hierarchy,32);assert(n>=2);ment_hrm_destroy(h);}
  /*5: Attendance */
  {MENT_HRMInstance*h=ment_hrm_create();int d=ment_hrm_add_department(h,"D001","Eng",0);int e=ment_hrm_add_employee(h,"Alice","Smith",d,120000.0f,1700000000);time_t now=time(0);time_t checkin=now-3600*9;time_t checkout=now-3600;int r=ment_hrm_record_attendance(h,e,now,checkin,checkout);assert(r==0);assert(h->num_attendances==1);assert(h->attendances[0].hours_worked>0);ment_hrm_destroy(h);}
  /*6: Leave request */
  {MENT_HRMInstance*h=ment_hrm_create();int d=ment_hrm_add_department(h,"D001","Eng",0);int e=ment_hrm_add_employee(h,"Alice","Smith",d,120000.0f,1700000000);time_t start=time(0);time_t end=start+86400*4;int days=ment_hrm_request_leave(h,e,0,start,end);assert(days==5);ment_hrm_destroy(h);}
  /*7: Payroll */
  {MENT_HRMInstance*h=ment_hrm_create();int d=ment_hrm_add_department(h,"D001","Eng",0);ment_hrm_add_employee(h,"Alice","Smith",d,120000.0f,1700000000);ment_hrm_add_employee(h,"Bob","Jones",d,60000.0f,1710000000);int r=ment_hrm_run_payroll(h,2025,3);assert(r==2);assert(h->num_payrolls==2);assert(h->payrolls[0].is_processed);assert(h->payrolls[0].net_pay>0);ment_hrm_destroy(h);}
  /*8: Payroll query */
  {MENT_HRMInstance*h=ment_hrm_create();int d=ment_hrm_add_department(h,"D001","Eng",0);int e=ment_hrm_add_employee(h,"Alice","Smith",d,120000.0f,1700000000);ment_hrm_run_payroll(h,2025,3);MENT_Payroll pr;int r=ment_hrm_get_payroll(h,e,2025,3,&pr);assert(r==0);AF(pr.basic_salary,10000.0f);ment_hrm_destroy(h);}
  /*9: Analytics */
  {MENT_HRMInstance*h=ment_hrm_create();int d1=ment_hrm_add_department(h,"D001","Eng",0);int d2=ment_hrm_add_department(h,"D002","QA",0);ment_hrm_add_employee(h,"A","Smith",d1,120000.0f,1700000000);ment_hrm_add_employee(h,"B","Jones",d1,90000.0f,1710000000);ment_hrm_add_employee(h,"C","Brown",d2,80000.0f,1720000000);assert(ment_hrm_headcount_by_dept(h,d1)==2);assert(ment_hrm_headcount_by_dept(h,d2)==1);AF(ment_hrm_total_compensation(h),290000.0f);float t=ment_hrm_avg_tenure(h);assert(t==t);ment_hrm_destroy(h);}
  /*10: Employee status */
  {MENT_HRMInstance*h=ment_hrm_create();int d=ment_hrm_add_department(h,"D001","Eng",0);int e=ment_hrm_add_employee(h,"Alice","Smith",d,120000.0f,1700000000);assert(h->employees[0].status==MENT_EMP_ACTIVE);assert(h->employees[0].emp_type==MENT_EMP_FULLTIME);h->employees[0].status=MENT_EMP_TERMINATED;ment_hrm_run_payroll(h,2025,3);assert(h->num_payrolls==0);ment_hrm_destroy(h);}
  printf("  HRM: 10 tests passed\n");
  return 0;
}
