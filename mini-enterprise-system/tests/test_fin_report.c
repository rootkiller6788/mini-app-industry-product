/* test_fin_report.c - Financial Reporting Test Suite (12 tests) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "fin_report.h"

#define AF(a,b) do{float _a=(a),_b=(b); if(fabsf(_a-_b)>0.01f){fprintf(stderr,"FAIL %.4f!=%.4f\n",_a,_b);assert(0);}}while(0)

int test_fin_suite(void) {
  printf("  Running Finance tests...\n");
  /*1: Instance */
  {MENT_FinReportInstance*f=ment_fin_create();assert(f);ment_fin_destroy(f);ment_fin_destroy(NULL);}
  /*2: Budget creation */
  {MENT_FinReportInstance*f=ment_fin_create();int b1=ment_fin_create_budget(f,"FY2025 Revenue",MENT_BUDGET_REVENUE,2025,-1);int b2=ment_fin_create_budget(f,"Q1 Marketing",MENT_BUDGET_EXPENSE,2025,3);assert(b1==0);assert(b2==1);assert(f->num_budgets==2);ment_fin_destroy(f);}
  /*3: Budget lines */
  {MENT_FinReportInstance*f=ment_fin_create();int b=ment_fin_create_budget(f,"Test Budget",MENT_BUDGET_REVENUE,2025,-1);int bl1=ment_fin_add_budget_line(f,b,0,1,100000.0f);int bl2=ment_fin_add_budget_line(f,b,0,2,120000.0f);assert(bl1>=0);assert(bl2>bl1);assert(f->num_budget_lines==2);AF(f->budgets[b].total_budget,220000.0f);ment_fin_destroy(f);}
  /*4: Budget approval */
  {MENT_FinReportInstance*f=ment_fin_create();int b=ment_fin_create_budget(f,"Test",MENT_BUDGET_REVENUE,2025,-1);assert(!f->budgets[b].is_approved);int r=ment_fin_approve_budget(f,b);assert(r==0);assert(f->budgets[b].is_approved);ment_fin_destroy(f);}
  /*5: Budget variance (needs ERP) */
  {MENT_ERPInstance*e=ment_erp_create("Test","TC");ment_erp_setup_fiscal_year(e,2025,1735689600);int cash=ment_erp_add_account(e,"1010","Cash",MENT_ACCTYPE_ASSET,0);int sales=ment_erp_add_account(e,"4010","Sales",MENT_ACCTYPE_REVENUE,3);MENT_JournalEntry*j=ment_erp_create_journal(e,time(0),"SALES");ment_erp_journal_add_line(j,cash,100000.0f,MENT_DEBIT,"Cash");ment_erp_journal_add_line(j,sales,100000.0f,MENT_CREDIT,"Sales");ment_erp_post_journal(e,j);MENT_FinReportInstance*f=ment_fin_create();int b=ment_fin_create_budget(f,"Revenue Budget",MENT_BUDGET_REVENUE,2025,-1);ment_fin_add_budget_line(f,b,3,1,80000.0f);int vr=ment_fin_budget_variance(f,b,e);assert(vr==0);assert(f->budget_lines[0].variance!=0);ment_fin_destroy(f);ment_erp_destroy(e);}
  /*6: Financial ratios */
  {MENT_ERPInstance*e=ment_erp_create("Test","TC");ment_erp_setup_fiscal_year(e,2025,1735689600);int cash=ment_erp_add_account(e,"1010","Cash",MENT_ACCTYPE_ASSET,0);int sales=ment_erp_add_account(e,"4010","Sales",MENT_ACCTYPE_REVENUE,3);int cogs=ment_erp_add_account(e,"5010","COGS",MENT_ACCTYPE_EXPENSE,4);MENT_JournalEntry*j1=ment_erp_create_journal(e,time(0),"S");ment_erp_journal_add_line(j1,cash,100000.0f,MENT_DEBIT,"Cash");ment_erp_journal_add_line(j1,sales,100000.0f,MENT_CREDIT,"Sales");ment_erp_post_journal(e,j1);MENT_FinReportInstance*f=ment_fin_create();int r=ment_fin_compute_ratios(f,e,1);assert(r==0);assert(f->num_ratio_reports==1);const MENT_RatioReport*rr=ment_fin_get_ratios(f,1);assert(rr);assert(rr->num_ratios>=6);ment_fin_destroy(f);ment_erp_destroy(e);}
  /*7: Cash flow statement */
  {MENT_ERPInstance*e=ment_erp_create("Test","TC");ment_erp_setup_fiscal_year(e,2025,1735689600);int cash=ment_erp_add_account(e,"1010","Cash",MENT_ACCTYPE_ASSET,0);int sales=ment_erp_add_account(e,"4010","Sales",MENT_ACCTYPE_REVENUE,3);MENT_JournalEntry*j=ment_erp_create_journal(e,time(0),"S");ment_erp_journal_add_line(j,cash,100000.0f,MENT_DEBIT,"Cash");ment_erp_journal_add_line(j,sales,100000.0f,MENT_CREDIT,"Sales");ment_erp_post_journal(e,j);MENT_CashFlowLine lines[16];int n=ment_fin_cashflow_statement(e,1,1,lines,16);assert(n>0);ment_erp_destroy(e);}
  /*8: KPI dashboard */
  {MENT_ERPInstance*e=ment_erp_create("Test","TC");ment_erp_setup_fiscal_year(e,2025,1735689600);int cash=ment_erp_add_account(e,"1010","Cash",MENT_ACCTYPE_ASSET,0);int sales=ment_erp_add_account(e,"4010","Sales",MENT_ACCTYPE_REVENUE,3);MENT_JournalEntry*j=ment_erp_create_journal(e,time(0),"S");ment_erp_journal_add_line(j,cash,100000.0f,MENT_DEBIT,"Cash");ment_erp_journal_add_line(j,sales,100000.0f,MENT_CREDIT,"Sales");ment_erp_post_journal(e,j);MENT_FinReportInstance*f=ment_fin_create();MENT_HRMInstance*h=ment_hrm_create();int d=ment_hrm_add_department(h,"D001","Eng",0);ment_hrm_add_employee(h,"A","B",d,120000.0f,time(0));int k=ment_fin_update_kpis(f,e,h);assert(k>0);assert(f->num_kpis>0);ment_hrm_destroy(h);ment_fin_destroy(f);ment_erp_destroy(e);}
  /*9: Break-even analysis */
  {float bep=ment_fin_break_even(100000.0f,50.0f,30.0f);AF(bep,5000.0f);float bep2=ment_fin_break_even(100000.0f,10.0f,10.0f);assert(bep2>999999.0f);}
  /*10: NPV calculation */
  {float cf[5]={-100000.0f,30000.0f,35000.0f,40000.0f,45000.0f};float npv=ment_fin_npv(cf,5,0.10f);assert(npv>0);float cf2[3]={-1000.0f,500.0f,500.0f};npv=ment_fin_npv(cf2,3,0.05f);AF(npv,(-1000.0f/1.05f+500.0f/1.1025f+500.0f/1.157625f));}
  /*11: IRR via Newton-Raphson */
  {float cf[4]={-1000.0f,300.0f,400.0f,500.0f};float irr=ment_fin_irr(cf,4,0.1f);assert(irr>0.0f);assert(irr<1.0f);}
  /*12: Depreciation methods */
  {float sl=ment_fin_depreciation_straight_line(100000.0f,10000.0f,5);AF(sl,18000.0f);float db=ment_fin_depreciation_declining_balance(100000.0f,10000.0f,5,0.4f);AF(db,40000.0f);float syd=ment_fin_depreciation_sum_of_years(100000.0f,10000.0f,5,1);AF(syd,30000.0f);}
  printf("  Finance: 12 tests passed\n");
  return 0;
}
