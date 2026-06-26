/* demo_enterprise.c - Full Enterprise System Demo: ERP + MRP + HRM + CRM + Finance */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "erp_core.h"
#include "mrp_engine.h"
#include "hrm_system.h"
#include "crm_pipeline.h"
#include "fin_report.h"

int main(void) {
  printf("\n");
  printf("╔══════════════════════════════════════════════════════╗\n");
  printf("║     Mini Enterprise System — Full Stack Demo        ║\n");
  printf("║     ERP + MRP + HRM + CRM + Financial Reporting     ║\n");
  printf("╚══════════════════════════════════════════════════════╝\n\n");

  /* ======== 1. ERP: Company Setup ======== */
  printf("━━━ 1. ERP Core — Company Setup ━━━\n");
  MENT_ERPInstance *erp = ment_erp_create("Nano Manufacturing Co.", "NANO");
  ment_erp_setup_fiscal_year(erp, 2025, 1735689600);
  printf("  Company: %s (%s), Fiscal Year: 2025\n\n", erp->company_name, erp->company_code);

  /* ======== 2. ERP: Chart of Accounts & Transactions ======== */
  printf("━━━ 2. ERP — Chart of Accounts & Transactions ━━━\n");
  int cash   = ment_erp_add_account(erp, "1000", "Cash", MENT_ACCTYPE_ASSET, 0);
  int ar     = ment_erp_add_account(erp, "1100", "A/R", MENT_ACCTYPE_ASSET, 0);
  int inv    = ment_erp_add_account(erp, "1200", "Inventory", MENT_ACCTYPE_ASSET, 0);
  int ap     = ment_erp_add_account(erp, "2000", "A/P", MENT_ACCTYPE_LIABILITY, 1);
  int cap    = ment_erp_add_account(erp, "3000", "Capital", MENT_ACCTYPE_EQUITY, 2);
  int re     = ment_erp_add_account(erp, "3100", "Retained Earnings", MENT_ACCTYPE_EQUITY, 2);
  int sales  = ment_erp_add_account(erp, "4000", "Product Sales", MENT_ACCTYPE_REVENUE, 3);
  int cogs   = ment_erp_add_account(erp, "5000", "COGS", MENT_ACCTYPE_EXPENSE, 4);
  int salary = ment_erp_add_account(erp, "5100", "Salary Expense", MENT_ACCTYPE_EXPENSE, 4);
  printf("  %d accounts configured\n", erp->num_accounts);

  MENT_JournalEntry *j1 = ment_erp_create_journal(erp, time(NULL), "INV-001");
  ment_erp_journal_add_line(j1, cash, 1000000.0f, MENT_DEBIT, "Initial capital");
  ment_erp_journal_add_line(j1, cap, 1000000.0f, MENT_CREDIT, "Share capital");
  ment_erp_post_journal(erp, j1);
  printf("  Capital injected: $1,000,000\n");

  MENT_JournalEntry *j2 = ment_erp_create_journal(erp, time(NULL), "SALE-001");
  ment_erp_journal_add_line(j2, cash, 500000.0f, MENT_DEBIT, "Cash sale");
  ment_erp_journal_add_line(j2, sales, 500000.0f, MENT_CREDIT, "Product revenue");
  ment_erp_post_journal(erp, j2);
  printf("  Revenue: $500,000\n\n");

  /* ======== 3. MRP: Manufacturing Planning ======== */
  printf("━━━ 3. MRP — Manufacturing Planning ━━━\n");
  MENT_MRPInstance *mrp = ment_mrp_create(26);
  int fg  = ment_mrp_add_item(mrp, "FG-001", "Widget Pro", MENT_ITEM_FINISHED_GOOD, 7, 50);
  int sa  = ment_mrp_add_item(mrp, "SA-001", "Sub-Assembly A", MENT_ITEM_SUBASSEMBLY, 3, 100);
  int rm1 = ment_mrp_add_item(mrp, "RM-001", "Steel Rod", MENT_ITEM_RAW_MATERIAL, 2, 500);
  ment_mrp_add_bom(mrp, fg, sa, 3.0f);
  ment_mrp_add_bom(mrp, sa, rm1, 5.0f);
  ment_mrp_add_demand(mrp, fg, 5, 500);
  ment_mrp_add_supply(mrp, fg, 1, 80, "OnHand");
  ment_mrp_add_workcenter(mrp, "ASSY01", 40.0f);
  ment_mrp_add_routing(mrp, fg, 1, 1, 1.5f);
  ment_mrp_explode(mrp);
  int n_wo = ment_mrp_generate_work_orders(mrp);
  printf("  Items: 3, BOM depth: 2, Work Orders: %d generated\n\n", n_wo);

  /* ======== 4. HRM: Workforce Management ======== */
  printf("━━━ 4. HRM — Workforce Management ━━━\n");
  MENT_HRMInstance *hrm = ment_hrm_create();
  int eng = ment_hrm_add_department(hrm, "ENG", "Engineering", 0);
  int ops = ment_hrm_add_department(hrm, "OPS", "Operations", 0);
  int e1  = ment_hrm_add_employee(hrm, "Alice", "Chen", eng, 150000.0f, 1700000000);
  int e2  = ment_hrm_add_employee(hrm, "Bob", "Patel", eng, 120000.0f, 1710000000);
  int e3  = ment_hrm_add_employee(hrm, "Carol", "Kim", ops, 100000.0f, 1720000000);
  int e4  = ment_hrm_add_employee(hrm, "Dave", "Liu", ops, 95000.0f, 1705000000);
  ment_hrm_run_payroll(hrm, 2025, 3);
  printf("  Departments: 2, Employees: 4, Payroll processed: %d records\n",
         hrm->num_payrolls);
  printf("  Total compensation: $%.0f\n\n", ment_hrm_total_compensation(hrm));

  /* ======== 5. CRM: Customer Pipeline ======== */
  printf("━━━ 5. CRM — Customer Pipeline ━━━\n");
  MENT_CRMInstance *crm = ment_crm_create();
  int acct = ment_crm_add_account(crm, "Industrial Corp", "Manufacturing");
  ment_crm_add_contact(crm, acct, "John", "Smith", "john@indcorp.com", true);
  int lead = ment_crm_add_lead(crm, "MegaBuild", "CEO", "ceo@megabuild.com", "Referral");
  ment_crm_score_lead(crm, lead);
  int opp = ment_crm_add_opportunity(crm, "$1M Contract", acct, 1000000.0f, time(NULL)+86400*90);
  ment_crm_update_stage(crm, opp, MENT_OPP_PROPOSAL);
  float pipeline = ment_crm_pipeline_value(crm);
  printf("  Accounts: %d, Leads: 1, Pipeline: $%.0f\n\n", crm->num_accounts, pipeline);

  /* ======== 6. Finance: Reporting & Ratios ======== */
  printf("━━━ 6. Finance — Reporting & Analysis ━━━\n");
  MENT_FinReportInstance *fin = ment_fin_create();
  ment_fin_compute_ratios(fin, erp, 1);
  ment_fin_update_kpis(fin, erp, hrm);
  const MENT_RatioReport *rr = ment_fin_get_ratios(fin, 1);
  if (rr) {
    printf("  Financial Ratios (%d computed):\n", rr->num_ratios);
    for (int i = 0; i < rr->num_ratios && i < 5; i++) {
      printf("    %-20s = %7.2f  [%s]\n",
             rr->ratios[i].name, rr->ratios[i].value, rr->ratios[i].assessment);
    }
  }

  /* ======== 7. KPIs ======== */
  printf("\n  KPIs (%d tracked):\n", fin->num_kpis);
  for (int i = 0; i < fin->num_kpis && i < 5; i++) {
    printf("    %-20s = %10.2f %s\n", fin->kpis[i].name,
           fin->kpis[i].current_value, fin->kpis[i].unit);
  }

  /* ======== Summary ======== */
  printf("\n╔══════════════════════════════════════════════════════╗\n");
  printf("║              Enterprise System Demo Complete         ║\n");
  printf("║   ERP: %d accounts, %d journals                   ║\n",
         erp->num_accounts, erp->num_journals);
  printf("║   MRP: %d items, %d BOM lines, %d work orders    ║\n",
         mrp->num_items, mrp->num_bom_lines, mrp->num_work_orders);
  printf("║   HRM: %d employees, %d payroll records          ║\n",
         hrm->num_employees, hrm->num_payrolls);
  printf("║   CRM: %d accounts, %d leads, Pipeline: $%.0f  ║\n",
         crm->num_accounts, crm->num_leads, pipeline);
  printf("║   FIN: %d ratios, %d KPIs                       ║\n",
         rr ? rr->num_ratios : 0, fin->num_kpis);
  printf("╚══════════════════════════════════════════════════════╝\n\n");

  ment_fin_destroy(fin);
  ment_crm_destroy(crm);
  ment_hrm_destroy(hrm);
  ment_mrp_destroy(mrp);
  ment_erp_destroy(erp);
  return 0;
}
