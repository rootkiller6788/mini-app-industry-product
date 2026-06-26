#ifndef MINI_ENT_FIN_REPORT_H
#define MINI_ENT_FIN_REPORT_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* Cross-module type references */
#include "erp_core.h"
#include "hrm_system.h"

/* ================================================================
 *  Financial Reporting & Budgeting
 *
 *  L1: Core types — Budget, Cash Flow, Financial Ratio, KPI
 *  L2: Core concepts — Budget vs Actual, variance analysis,
 *      financial ratios (liquidity, profitability, solvency)
 *  L3: Engineering structures — Budget tree, period comparison
 *  L4: Standards — IFRS, GAAP reporting requirements, SEC filings
 *  L5: Algorithms — Ratio calculation, trend analysis, forecasting
 *  L6: Canonical problems — Financial statement generation
 *  L7: Applications — Management reporting, investor relations
 *  L8: Advanced — XBRL tagging, ESG reporting
 *
 *  References:
 *  - IFRS Foundation, "Conceptual Framework for Financial Reporting"
 *  - Penman, "Financial Statement Analysis and Security Valuation" (2013)
 * ================================================================ */

#define MENT_FIN_MAX_BUDGETS    256
#define MENT_FIN_MAX_BUDGETLINES 8192
#define MENT_FIN_MAX_RATIOS     32

/* ---- L1: Budget Types ---- */

typedef enum {
    MENT_BUDGET_REVENUE    = 0,
    MENT_BUDGET_EXPENSE    = 1,
    MENT_BUDGET_CAPITAL    = 2,
    MENT_BUDGET_CASHFLOW   = 3,
    MENT_BUDGET_PROJECT    = 4
} MENT_BudgetType;

typedef enum {
    MENT_BUDGET_MONTHLY   = 0,
    MENT_BUDGET_QUARTERLY = 1,
    MENT_BUDGET_ANNUAL    = 2
} MENT_BudgetFrequency;

typedef struct {
    int      id;
    char     name[128];
    MENT_BudgetType type;
    MENT_BudgetFrequency frequency;
    int      fiscal_year;
    int      dept_id;          /* -1 for company-wide */
    float    total_budget;
    float    total_actual;
    bool     is_approved;
    time_t   created_at;
    time_t   approved_at;
} MENT_Budget;

typedef struct {
    int      id;
    int      budget_id;
    int      account_idx;      /* GL account */
    int      period;           /* month (1-12) */
    float    budget_amount;
    float    actual_amount;
    float    variance;         /* actual - budget */
    float    variance_pct;     /* variance / budget * 100 */
    char     notes[128];
} MENT_BudgetLine;

/* ---- L1: Financial Ratios ---- */

typedef struct {
    char     name[64];
    float    value;
    float    benchmark;        /* industry average */
    char     assessment[32];   /* "Good", "Warning", "Critical" */
} MENT_FinancialRatio;

typedef struct {
    MENT_FinancialRatio ratios[MENT_FIN_MAX_RATIOS];
    int    num_ratios;
    int    fiscal_year;
    int    period;
    char   company_name[128];
    time_t generated_at;
} MENT_RatioReport;

/* ---- L1: Cash Flow Statement ---- */

typedef enum {
    MENT_CF_OPERATING  = 0,
    MENT_CF_INVESTING  = 1,
    MENT_CF_FINANCING  = 2
} MENT_CashFlowCategory;

typedef struct {
    int      account_idx;
    float    amount;
    MENT_CashFlowCategory category;
    char     description[128];
} MENT_CashFlowLine;

/* ---- L1: Management Dashboard KPI ---- */

typedef struct {
    char     name[64];
    float    current_value;
    float    target_value;
    float    previous_value;
    float    change_pct;
    char     unit[16];
    bool     is_good_when_high;  /* true=higher is better */
} MENT_KPI;

/* ---- L1: Financial Reporting Instance ---- */

typedef struct {
    MENT_Budget      budgets[MENT_FIN_MAX_BUDGETS];
    int              num_budgets;
    MENT_BudgetLine  *budget_lines;
    int              num_budget_lines;
    MENT_RatioReport *ratio_reports;
    int              num_ratio_reports;
    MENT_KPI         kpis[32];
    int              num_kpis;
} MENT_FinReportInstance;

/* ---- API ---- */

MENT_FinReportInstance* ment_fin_create(void);
void ment_fin_destroy(MENT_FinReportInstance *fin);

/** Budget Management */
int  ment_fin_create_budget(MENT_FinReportInstance *fin, const char *name,
                             MENT_BudgetType type, int year, int dept_id);
int  ment_fin_add_budget_line(MENT_FinReportInstance *fin, int budget_id,
                               int account_idx, int period, float amount);
int  ment_fin_approve_budget(MENT_FinReportInstance *fin, int budget_id);

/**
 * Budget vs Actual analysis.
 * Computes variance and variance percentage for each budget line.
 */
int  ment_fin_budget_variance(MENT_FinReportInstance *fin, int budget_id,
                               const MENT_ERPInstance *erp);

/**
 * Financial Ratio Analysis (L5).
 *
 * Liquidity Ratios:
 *   Current Ratio = Current Assets / Current Liabilities
 *   Quick Ratio = (Current Assets - Inventory) / Current Liabilities
 *
 * Profitability Ratios:
 *   Gross Margin = (Revenue - COGS) / Revenue
 *   Net Margin = Net Income / Revenue
 *   ROA = Net Income / Total Assets
 *   ROE = Net Income / Equity
 *
 * Solvency Ratios:
 *   Debt-to-Equity = Total Liabilities / Equity
 *   Interest Coverage = EBIT / Interest Expense
 *
 * Efficiency Ratios:
 *   Asset Turnover = Revenue / Total Assets
 *   Inventory Turnover = COGS / Avg Inventory
 *
 * Reference: Penman (2013)
 */
int  ment_fin_compute_ratios(MENT_FinReportInstance *fin,
                              const MENT_ERPInstance *erp,
                              int period);

/** Generate ratio report */
const MENT_RatioReport* ment_fin_get_ratios(const MENT_FinReportInstance *fin,
                                              int period);

/**
 * Cash Flow Statement (Indirect Method).
 * Operating: Net Income + Depreciation ± Working Capital changes
 * Investing: Capital expenditures, asset sales
 * Financing: Debt issuance/repayment, dividends, equity
 */
int  ment_fin_cashflow_statement(const MENT_ERPInstance *erp,
                                  int period_start, int period_end,
                                  MENT_CashFlowLine *lines, int max_lines);

/**
 * KPI Dashboard.
 * Revenue Growth, Gross Margin, Operating Margin, EBITDA,
 * Customer Acquisition Cost, Employee Productivity, etc.
 */
int  ment_fin_update_kpis(MENT_FinReportInstance *fin,
                           const MENT_ERPInstance *erp,
                           const MENT_HRMInstance *hrm);

/** Trend Analysis: linear regression for revenue forecast */
float ment_fin_revenue_forecast(const MENT_ERPInstance *erp,
                                 int num_periods, int future_periods);

/** Break-even Analysis:
 *  BEP = Fixed Costs / (Price per Unit - Variable Cost per Unit)
 */
float ment_fin_break_even(float fixed_costs, float price_per_unit,
                            float variable_cost_per_unit);

/** Net Present Value (NPV):
 *  NPV = Σ (CashFlow_t / (1 + r)^t) - Initial Investment
 */
float ment_fin_npv(const float *cashflows, int periods, float discount_rate);

/** Internal Rate of Return (IRR) via Newton-Raphson */
float ment_fin_irr(const float *cashflows, int periods, float guess);

/** Depreciation Methods */
float ment_fin_depreciation_straight_line(float cost, float salvage, int life);
float ment_fin_depreciation_declining_balance(float cost, float salvage,
                                               int life, float rate);
float ment_fin_depreciation_sum_of_years(float cost, float salvage, int life,
                                          int year);

#endif /* MINI_ENT_FIN_REPORT_H */
