/* example_full_erp_cycle.c - End-to-End ERP: Sales Order -> GL -> Financial Statements */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "erp_core.h"
#include "fin_report.h"

int main(void) {
  printf("=== Full ERP Accounting Cycle Demo ===\n\n");

  /* 1. Setup company */
  MENT_ERPInstance *erp = ment_erp_create("Acme Manufacturing Inc.", "ACME");
  ment_erp_setup_fiscal_year(erp, 2025, 1735689600);
  printf("[1] Company '%s' created with fiscal year 2025\n", erp->company_name);

  /* 2. Build Chart of Accounts */
  int cash = ment_erp_add_account(erp, "1001", "Cash", MENT_ACCTYPE_ASSET, 0);
  int ar   = ment_erp_add_account(erp, "1100", "Accounts Receivable", MENT_ACCTYPE_ASSET, 0);
  int inv  = ment_erp_add_account(erp, "1200", "Inventory", MENT_ACCTYPE_ASSET, 0);
  int ap   = ment_erp_add_account(erp, "2000", "Accounts Payable", MENT_ACCTYPE_LIABILITY, 1);
  int cap  = ment_erp_add_account(erp, "3000", "Share Capital", MENT_ACCTYPE_EQUITY, 2);
  int sales= ment_erp_add_account(erp, "4000", "Product Sales", MENT_ACCTYPE_REVENUE, 3);
  int cogs = ment_erp_add_account(erp, "5000", "COGS", MENT_ACCTYPE_EXPENSE, 4);
  int rent = ment_erp_add_account(erp, "5500", "Rent Expense", MENT_ACCTYPE_EXPENSE, 4);
  printf("[2] Chart of Accounts: %d accounts\n", erp->num_accounts);

  /* 3. Initial investment */
  MENT_JournalEntry *j_invest = ment_erp_create_journal(erp, time(NULL), "INVEST-001");
  ment_erp_journal_add_line(j_invest, cash, 500000.0f, MENT_DEBIT, "Initial capital");
  ment_erp_journal_add_line(j_invest, cap, 500000.0f, MENT_CREDIT, "Share capital");
  ment_erp_post_journal(erp, j_invest);
  printf("[3] Initial investment: $500,000 posted\n");

  /* 4. Sales transaction */
  MENT_JournalEntry *j_sale = ment_erp_create_journal(erp, time(NULL), "SALE-001");
  ment_erp_journal_add_line(j_sale, cash, 150000.0f, MENT_DEBIT, "Cash sale");
  ment_erp_journal_add_line(j_sale, sales, 150000.0f, MENT_CREDIT, "Product revenue");
  ment_erp_post_journal(erp, j_sale);
  printf("[4] Sales: $150,000 revenue (cash)\n");

  /* 5. Expense: rent */
  MENT_JournalEntry *j_rent = ment_erp_create_journal(erp, time(NULL), "EXP-001");
  ment_erp_journal_add_line(j_rent, rent, 12000.0f, MENT_DEBIT, "Monthly rent");
  ment_erp_journal_add_line(j_rent, cash, 12000.0f, MENT_CREDIT, "Rent paid");
  ment_erp_post_journal(erp, j_rent);
  printf("[5] Rent expense: $12,000 paid\n");

  /* 6. Financial Statements */
  float assets, liabilities, equity;
  ment_erp_balance_sheet(erp, 1, &assets, &liabilities, &equity);
  printf("\n[6] Balance Sheet (Period 1):\n");
  printf("    Assets:      $%.2f\n", assets);
  printf("    Liabilities: $%.2f\n", liabilities);
  printf("    Equity:      $%.2f\n", equity);
  printf("    A = L + E?   %s\n\n", fabsf(assets - (liabilities + equity)) < 0.01f ? "YES" : "NO");

  float rev, exp, ni;
  ment_erp_income_statement(erp, 1, 1, &rev, &exp, &ni);
  printf("[7] Income Statement (Period 1):\n");
  printf("    Revenue:     $%.2f\n", rev);
  printf("    Expenses:    $%.2f\n", exp);
  printf("    Net Income:  $%.2f\n\n", ni);

  /* 8. Financial Ratios */
  MENT_FinReportInstance *fin = ment_fin_create();
  ment_fin_compute_ratios(fin, erp, 1);
  const MENT_RatioReport *rr = ment_fin_get_ratios(fin, 1);
  if (rr) {
    printf("[8] Key Financial Ratios:\n");
    for (int i = 0; i < rr->num_ratios; i++) {
      printf("    %-20s = %.2f  [%s]\n", rr->ratios[i].name,
             rr->ratios[i].value, rr->ratios[i].assessment);
    }
  }

  ment_fin_destroy(fin);
  ment_erp_destroy(erp);
  printf("\n=== ERP Demo Complete ===\n");
  return 0;
}
