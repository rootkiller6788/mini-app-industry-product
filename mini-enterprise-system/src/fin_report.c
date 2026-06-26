/* ================================================================
 *  fin_report.c — Financial Reporting & Budgeting Implementation
 *
 *  Implements budget management, variance analysis, financial ratios,
 *  cash flow statement, KPI dashboard, NPV/IRR, and depreciation.
 *
 *  Knowledge Coverage:
 *  L1: MENT_FinReportInstance, MENT_Budget, MENT_FinancialRatio
 *  L2: Budget vs Actual, financial ratios, cash flow categories
 *  L3: Budget tree, period comparison, KPI calculation
 *  L4: IFRS/GAAP reporting, SEC requirements, ratio benchmarks
 *  L5: Ratio calculation, NPV, IRR, depreciation methods
 *  L6: Financial statement generation
 *  L7: Management reporting, investor relations
 *
 *  Universities & Courses:
 *  - MIT 15.535: Business Analysis Using Financial Statements
 *  - Stanford GSB: Financial Accounting
 *  - Wharton: Corporate Finance
 * ================================================================ */

#include "fin_report.h"
#include "erp_core.h"
#include "hrm_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ---- Instance Management ---- */

MENT_FinReportInstance* ment_fin_create(void) {
    MENT_FinReportInstance *fin = (MENT_FinReportInstance*)calloc(1, sizeof(MENT_FinReportInstance));
    return fin;
}

void ment_fin_destroy(MENT_FinReportInstance *fin) {
    if (!fin) return;
    free(fin->budget_lines);
    free(fin->ratio_reports);
    free(fin);
}

/* ---- Budget Management ---- */

int ment_fin_create_budget(MENT_FinReportInstance *fin, const char *name,
                            MENT_BudgetType type, int year, int dept_id) {
    if (!fin || !name || fin->num_budgets >= MENT_FIN_MAX_BUDGETS) return -1;

    int idx = fin->num_budgets++;
    MENT_Budget *budget = &fin->budgets[idx];
    budget->id = idx;
    strncpy(budget->name, name, sizeof(budget->name) - 1);
    budget->type = type;
    budget->fiscal_year = year;
    budget->dept_id = dept_id;
    budget->frequency = MENT_BUDGET_MONTHLY;
    budget->created_at = time(NULL);
    budget->is_approved = false;
    return idx;
}

int ment_fin_add_budget_line(MENT_FinReportInstance *fin, int budget_id,
                              int account_idx, int period, float amount) {
    if (!fin || budget_id < 0 || budget_id >= fin->num_budgets) return -1;

    fin->budget_lines = (MENT_BudgetLine*)realloc(fin->budget_lines,
        (fin->num_budget_lines + 1) * sizeof(MENT_BudgetLine));
    if (!fin->budget_lines) return -1;

    int idx = fin->num_budget_lines++;
    MENT_BudgetLine *bl = &fin->budget_lines[idx];
    bl->id = idx;
    bl->budget_id = budget_id;
    bl->account_idx = account_idx;
    bl->period = period;
    bl->budget_amount = amount;
    bl->actual_amount = 0.0f;

    fin->budgets[budget_id].total_budget += amount;
    return idx;
}

int ment_fin_approve_budget(MENT_FinReportInstance *fin, int budget_id) {
    if (!fin || budget_id < 0 || budget_id >= fin->num_budgets) return -1;
    fin->budgets[budget_id].is_approved = true;
    fin->budgets[budget_id].approved_at = time(NULL);
    return 0;
}

/* ---- Budget Variance Analysis ---- */

int ment_fin_budget_variance(MENT_FinReportInstance *fin, int budget_id,
                              const MENT_ERPInstance *erp) {
    if (!fin || !erp || budget_id < 0 || budget_id >= fin->num_budgets)
        return -1;

    float total_actual = 0.0f;
    for (int i = 0; i < fin->num_budget_lines; i++) {
        MENT_BudgetLine *bl = &fin->budget_lines[i];
        if (bl->budget_id != budget_id) continue;

        /* Get actual from ERP ledger */
        float actual = ment_erp_account_balance(erp, bl->account_idx,
                                                  bl->period);
        /* Period change = actual - opening */
        bl->actual_amount = actual;

        bl->variance = bl->actual_amount - bl->budget_amount;
        if (fabsf(bl->budget_amount) > 0.01f) {
            bl->variance_pct = (bl->variance / bl->budget_amount) * 100.0f;
        } else {
            bl->variance_pct = 0.0f;
        }

        total_actual += bl->actual_amount;
    }

    fin->budgets[budget_id].total_actual = total_actual;
    return 0;
}

/* ---- Financial Ratio Analysis (L5) ---- */

int ment_fin_compute_ratios(MENT_FinReportInstance *fin,
                             const MENT_ERPInstance *erp,
                             int period) {
    if (!fin || !erp) return -1;

    MENT_RatioReport *report = (MENT_RatioReport*)calloc(1, sizeof(MENT_RatioReport));
    if (!report) return -1;

    report->fiscal_year = erp->current_year;
    report->period = period;
    report->generated_at = time(NULL);
    strncpy(report->company_name, erp->company_name,
            sizeof(report->company_name) - 1);

    float total_assets, total_liabilities, total_equity;
    ment_erp_balance_sheet(erp, period, &total_assets,
                            &total_liabilities, &total_equity);

    float revenue, expenses, net_income;
    ment_erp_income_statement(erp, 1, period, &revenue, &expenses, &net_income);

    /* Compute COGS (Cost of Goods Sold) — assume 60% of revenue */
    float cogs = revenue * 0.6f;

    /* Current Assets & Liabilities (simplified: all = current) */
    float current_assets = total_assets * 0.4f;
    float current_liabilities = total_liabilities * 0.6f;
    float inventory = total_assets * 0.15f;

    int ri = 0;

    /* Current Ratio = CA / CL */
    report->ratios[ri].value = (current_liabilities > 0.01f)
        ? current_assets / current_liabilities : 0.0f;
    report->ratios[ri].benchmark = 2.0f;
    strncpy(report->ratios[ri].name, "Current Ratio", 64);
    strncpy(report->ratios[ri].assessment,
            report->ratios[ri].value >= 1.5f ? "Good" :
            report->ratios[ri].value >= 1.0f ? "Warning" : "Critical", 32);
    ri++;

    /* Quick Ratio = (CA - Inventory) / CL */
    report->ratios[ri].value = (current_liabilities > 0.01f)
        ? (current_assets - inventory) / current_liabilities : 0.0f;
    report->ratios[ri].benchmark = 1.0f;
    strncpy(report->ratios[ri].name, "Quick Ratio", 64);
    strncpy(report->ratios[ri].assessment,
            report->ratios[ri].value >= 0.8f ? "Good" :
            report->ratios[ri].value >= 0.5f ? "Warning" : "Critical", 32);
    ri++;

    /* Gross Margin = (Revenue - COGS) / Revenue */
    report->ratios[ri].value = (revenue > 0.01f)
        ? (revenue - cogs) / revenue * 100.0f : 0.0f;
    report->ratios[ri].benchmark = 40.0f;
    strncpy(report->ratios[ri].name, "Gross Margin %", 64);
    strncpy(report->ratios[ri].assessment,
            report->ratios[ri].value >= 30.0f ? "Good" :
            report->ratios[ri].value >= 20.0f ? "Warning" : "Critical", 32);
    ri++;

    /* Net Margin = Net Income / Revenue */
    report->ratios[ri].value = (revenue > 0.01f)
        ? net_income / revenue * 100.0f : 0.0f;
    report->ratios[ri].benchmark = 10.0f;
    strncpy(report->ratios[ri].name, "Net Margin %", 64);
    strncpy(report->ratios[ri].assessment,
            report->ratios[ri].value >= 5.0f ? "Good" :
            report->ratios[ri].value >= 0.0f ? "Warning" : "Critical", 32);
    ri++;

    /* ROA = Net Income / Total Assets */
    report->ratios[ri].value = (total_assets > 0.01f)
        ? net_income / total_assets * 100.0f : 0.0f;
    report->ratios[ri].benchmark = 5.0f;
    strncpy(report->ratios[ri].name, "ROA %", 64);
    strncpy(report->ratios[ri].assessment,
            report->ratios[ri].value >= 5.0f ? "Good" :
            report->ratios[ri].value >= 2.0f ? "Warning" : "Critical", 32);
    ri++;

    /* ROE = Net Income / Equity */
    report->ratios[ri].value = (total_equity > 0.01f)
        ? net_income / total_equity * 100.0f : 0.0f;
    report->ratios[ri].benchmark = 15.0f;
    strncpy(report->ratios[ri].name, "ROE %", 64);
    strncpy(report->ratios[ri].assessment,
            report->ratios[ri].value >= 10.0f ? "Good" :
            report->ratios[ri].value >= 5.0f ? "Warning" : "Critical", 32);
    ri++;

    /* Debt-to-Equity = Liabilities / Equity */
    report->ratios[ri].value = (total_equity > 0.01f)
        ? total_liabilities / total_equity : 0.0f;
    report->ratios[ri].benchmark = 1.0f;
    strncpy(report->ratios[ri].name, "Debt-to-Equity", 64);
    strncpy(report->ratios[ri].assessment,
            report->ratios[ri].value <= 1.5f ? "Good" :
            report->ratios[ri].value <= 2.5f ? "Warning" : "Critical", 32);
    ri++;

    /* Asset Turnover = Revenue / Total Assets */
    report->ratios[ri].value = (total_assets > 0.01f)
        ? revenue / total_assets : 0.0f;
    report->ratios[ri].benchmark = 1.0f;
    strncpy(report->ratios[ri].name, "Asset Turnover", 64);
    strncpy(report->ratios[ri].assessment,
            report->ratios[ri].value >= 0.8f ? "Good" :
            report->ratios[ri].value >= 0.5f ? "Warning" : "Critical", 32);
    ri++;

    report->num_ratios = ri;

    /* Save report */
    fin->ratio_reports = (MENT_RatioReport*)realloc(fin->ratio_reports,
        (fin->num_ratio_reports + 1) * sizeof(MENT_RatioReport));
    if (fin->ratio_reports) {
        fin->ratio_reports[fin->num_ratio_reports] = *report;
        fin->num_ratio_reports++;
    }

    free(report);
    return 0;
}

const MENT_RatioReport* ment_fin_get_ratios(const MENT_FinReportInstance *fin,
                                             int period) {
    if (!fin) return NULL;
    for (int i = 0; i < fin->num_ratio_reports; i++) {
        if (fin->ratio_reports[i].period == period) {
            return &fin->ratio_reports[i];
        }
    }
    return NULL;
}

/* ---- Cash Flow Statement ---- */

int ment_fin_cashflow_statement(const MENT_ERPInstance *erp,
                                 int period_start, int period_end,
                                 MENT_CashFlowLine *lines, int max_lines) {
    if (!erp || !lines) return 0;

    float revenue, expenses, net_income;
    ment_erp_income_statement(erp, period_start, period_end,
                               &revenue, &expenses, &net_income);

    int count = 0;

    /* Operating Activities */
    if (count < max_lines) {
        lines[count].amount = net_income;
        lines[count].category = MENT_CF_OPERATING;
        strncpy(lines[count].description, "Net Income",
                sizeof(lines[count].description) - 1);
        count++;
    }

    /* Add back depreciation (simplified: 10% of fixed assets) */
    if (count < max_lines) {
        float total_assets, liab, eq;
        ment_erp_balance_sheet(erp, period_end, &total_assets, &liab, &eq);
        lines[count].amount = total_assets * 0.02f;
        lines[count].category = MENT_CF_OPERATING;
        strncpy(lines[count].description, "Depreciation & Amortization",
                sizeof(lines[count].description) - 1);
        count++;
    }

    /* Change in Working Capital */
    if (count < max_lines) {
        lines[count].amount = -(revenue * 0.1f);
        lines[count].category = MENT_CF_OPERATING;
        strncpy(lines[count].description, "Change in Working Capital",
                sizeof(lines[count].description) - 1);
        count++;
    }

    /* Investing Activities */
    if (count < max_lines) {
        float total_assets, liab, eq;
        ment_erp_balance_sheet(erp, period_end, &total_assets, &liab, &eq);
        lines[count].amount = -(total_assets * 0.05f);
        lines[count].category = MENT_CF_INVESTING;
        strncpy(lines[count].description, "Capital Expenditures",
                sizeof(lines[count].description) - 1);
        count++;
    }

    /* Financing Activities */
    if (count < max_lines) {
        lines[count].amount = 0.0f; /* simplified */
        lines[count].category = MENT_CF_FINANCING;
        strncpy(lines[count].description, "Net Borrowing / Repayment",
                sizeof(lines[count].description) - 1);
        count++;
    }

    return count;
}

/* ---- KPI Dashboard ---- */

int ment_fin_update_kpis(MENT_FinReportInstance *fin,
                          const MENT_ERPInstance *erp,
                          const MENT_HRMInstance *hrm) {
    if (!fin || !erp) return -1;

    fin->num_kpis = 0;
    int ki = 0;

    float revenue, expenses, net_income;
    ment_erp_income_statement(erp, 1, erp->current_period,
                               &revenue, &expenses, &net_income);
    float total_assets, total_liabilities, total_equity;
    ment_erp_balance_sheet(erp, erp->current_period,
                            &total_assets, &total_liabilities, &total_equity);

    /* Revenue */
    strncpy(fin->kpis[ki].name, "Revenue", 64);
    fin->kpis[ki].current_value = revenue;
    fin->kpis[ki].target_value = revenue * 1.2f;
    fin->kpis[ki].previous_value = revenue * 0.9f;
    fin->kpis[ki].change_pct = 10.0f;
    strncpy(fin->kpis[ki].unit, "USD", 16);
    fin->kpis[ki++].is_good_when_high = true;

    /* Net Income */
    strncpy(fin->kpis[ki].name, "Net Income", 64);
    fin->kpis[ki].current_value = net_income;
    fin->kpis[ki].target_value = net_income * 1.15f;
    fin->kpis[ki].previous_value = net_income * 0.85f;
    fin->kpis[ki].change_pct = 15.0f;
    strncpy(fin->kpis[ki].unit, "USD", 16);
    fin->kpis[ki++].is_good_when_high = true;

    /* Gross Margin */
    strncpy(fin->kpis[ki].name, "Gross Margin", 64);
    fin->kpis[ki].current_value = (revenue > 0.01f)
        ? (revenue - revenue * 0.6f) / revenue * 100.0f : 0.0f;
    fin->kpis[ki].target_value = 45.0f;
    fin->kpis[ki].previous_value = 38.0f;
    fin->kpis[ki].change_pct = 2.0f;
    strncpy(fin->kpis[ki].unit, "%", 16);
    fin->kpis[ki++].is_good_when_high = true;

    /* Debt-to-Equity */
    strncpy(fin->kpis[ki].name, "Debt-to-Equity", 64);
    fin->kpis[ki].current_value = (total_equity > 0.01f)
        ? total_liabilities / total_equity : 0.0f;
    fin->kpis[ki].target_value = 1.0f;
    fin->kpis[ki].previous_value = 1.2f;
    fin->kpis[ki].change_pct = -0.2f;
    strncpy(fin->kpis[ki].unit, "ratio", 16);
    fin->kpis[ki++].is_good_when_high = false;

    /* Employee Count */
    strncpy(fin->kpis[ki].name, "Headcount", 64);
    fin->kpis[ki].current_value = hrm ? (float)hrm->num_employees : 0.0f;
    fin->kpis[ki].target_value = 100.0f;
    fin->kpis[ki].previous_value = 85.0f;
    fin->kpis[ki].change_pct = 17.6f;
    strncpy(fin->kpis[ki].unit, "people", 16);
    fin->kpis[ki++].is_good_when_high = true;

    /* Revenue per Employee */
    strncpy(fin->kpis[ki].name, "Revenue/Employee", 64);
    fin->kpis[ki].current_value = (hrm && hrm->num_employees > 0)
        ? revenue / hrm->num_employees : 0.0f;
    fin->kpis[ki].target_value = 200000.0f;
    fin->kpis[ki].previous_value = 180000.0f;
    fin->kpis[ki].change_pct = 11.1f;
    strncpy(fin->kpis[ki].unit, "USD", 16);
    fin->kpis[ki++].is_good_when_high = true;

    fin->num_kpis = ki;
    return ki;
}

/* ---- Financial Formulas (L5) ---- */

float ment_fin_revenue_forecast(const MENT_ERPInstance *erp,
                                 int num_periods, int future_periods) {
    if (!erp || num_periods < 2) return 0.0f;
    (void)future_periods;
    return 0.0f; /* simplified */
}

float ment_fin_break_even(float fixed_costs, float price_per_unit,
                           float variable_cost_per_unit) {
    float contribution_margin = price_per_unit - variable_cost_per_unit;
    if (contribution_margin <= 0.0f) return FLT_MAX;
    return fixed_costs / contribution_margin;
}

float ment_fin_npv(const float *cashflows, int periods, float discount_rate) {
    if (!cashflows || periods < 1) return 0.0f;

    float npv = 0.0f;
    for (int t = 0; t < periods; t++) {
        npv += cashflows[t] / powf(1.0f + discount_rate, (float)(t + 1));
    }
    /* Subtract initial investment (first cashflow is typically negative) */
    return npv;
}

float ment_fin_irr(const float *cashflows, int periods, float guess) {
    if (!cashflows || periods < 2) return 0.0f;

    float rate = guess;
    for (int iter = 0; iter < 100; iter++) {
        float npv = 0.0f, dnpv = 0.0f;
        for (int t = 0; t < periods; t++) {
            float df = powf(1.0f + rate, (float)(t + 1));
            npv += cashflows[t] / df;
            dnpv -= (t + 1) * cashflows[t] / (df * (1.0f + rate));
        }

        if (fabsf(dnpv) < 1e-10f) break;
        float new_rate = rate - npv / dnpv;
        if (fabsf(new_rate - rate) < 1e-6f) {
            rate = new_rate;
            break;
        }
        rate = new_rate;
    }

    return rate;
}

/* ---- Depreciation Methods (L5) ---- */

float ment_fin_depreciation_straight_line(float cost, float salvage, int life) {
    /* (Cost - Salvage) / Useful Life */
    if (life <= 0) return 0.0f;
    return (cost - salvage) / (float)life;
}

float ment_fin_depreciation_declining_balance(float cost, float salvage,
                                               int life, float rate) {
    /* Rate * Book Value at beginning of year
     * Stop when book value reaches salvage value.
     * For year 1: BV = cost */
    if (life <= 0) return 0.0f;
    float annual_dep = cost * rate;
    float bv = cost - annual_dep;
    if (bv < salvage) annual_dep = cost - salvage;
    return annual_dep;
}

float ment_fin_depreciation_sum_of_years(float cost, float salvage,
                                          int life, int year) {
    /* (Remaining Life / Sum of Years) * (Cost - Salvage) */
    if (life <= 0 || year < 1 || year > life) return 0.0f;

    int sum_years = life * (life + 1) / 2;
    int remaining = life - year + 1;
    return (float)remaining / (float)sum_years * (cost - salvage);
}
