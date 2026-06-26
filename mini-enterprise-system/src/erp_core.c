/* ================================================================
 *  erp_core.c — Enterprise Resource Planning Core Implementation
 *
 *  Implements double-entry bookkeeping, general ledger, chart of
 *  accounts, journal entry posting, period close, multi-currency,
 *  and audit trail.
 *
 *  Knowledge Coverage:
 *  L1: MENT_ERPInstance, MENT_Account, MENT_JournalEntry, MENT_LedgerBalance
 *  L2: Double-entry bookkeeping, COA hierarchy, fiscal period
 *  L3: Account tree, audit trail, B-tree indexing
 *  L4: GAAP/IFRS principles, SOX compliance (Sarbanes-Oxley 2002)
 *  L5: Period close algorithm, currency conversion, consolidation
 *  L6: General Ledger, Trial Balance, Balance Sheet, Income Statement
 *  L7: Multi-entity ERP, SME accounting
 *  L8: Real-time consolidation, IFRS 16
 *
 *  Accounting equation: Assets = Liabilities + Equity
 *  Extended: Assets + Expenses = Liabilities + Equity + Revenue
 *
 *  Universities & Courses:
 *  - Stanford GSB: Financial Accounting
 *  - MIT 15.501: Corporate Financial Accounting
 *  - Wharton: Accounting and Finance for Business
 * ================================================================ */

#include "erp_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MENT_EPS 0.001f

/* ---- Utility ---- */

const char* ment_acctype_name(MENT_AccountType type) {
    switch (type) {
        case MENT_ACCTYPE_ASSET:         return "Asset";
        case MENT_ACCTYPE_LIABILITY:     return "Liability";
        case MENT_ACCTYPE_EQUITY:        return "Equity";
        case MENT_ACCTYPE_REVENUE:       return "Revenue";
        case MENT_ACCTYPE_EXPENSE:       return "Expense";
        case MENT_ACCTYPE_OTHER_INCOME:  return "Other Income";
        case MENT_ACCTYPE_OTHER_EXPENSE: return "Other Expense";
        default: return "Unknown";
    }
}

MENT_DebitCredit ment_acctype_normal_balance(MENT_AccountType type) {
    switch (type) {
        case MENT_ACCTYPE_ASSET:
        case MENT_ACCTYPE_EXPENSE:
        case MENT_ACCTYPE_OTHER_EXPENSE:
            return MENT_DEBIT;
        case MENT_ACCTYPE_LIABILITY:
        case MENT_ACCTYPE_EQUITY:
        case MENT_ACCTYPE_REVENUE:
        case MENT_ACCTYPE_OTHER_INCOME:
            return MENT_CREDIT;
        default: return MENT_DEBIT;
    }
}

int ment_erp_get_next_voucher(MENT_ERPInstance *erp, char *buf, int bufsize) {
    if (!erp || !buf) return -1;
    erp->next_voucher_seq++;
    snprintf(buf, bufsize, "JV-%06d", erp->next_voucher_seq);
    return 0;
}

/* ---- ERP Instance Management ---- */

MENT_ERPInstance* ment_erp_create(const char *company_name,
                                   const char *company_code) {
    if (!company_name || !company_code) return NULL;
    MENT_ERPInstance *erp = (MENT_ERPInstance*)calloc(1, sizeof(MENT_ERPInstance));
    if (!erp) return NULL;

    strncpy(erp->company_name, company_name,
                               sizeof(erp->company_name) - 1);
    strncpy(erp->company_code, company_code,
            sizeof(erp->company_code) - 1);

    erp->next_voucher_seq = 0;
    erp->current_period = 1;
    erp->current_year = 2025;
    erp->multi_currency = false;
    erp->base_currency_idx = -1;

    /* Add default currency */
    ment_erp_add_currency(erp, "USD", "US Dollar", 1.0f, "$");
    erp->base_currency_idx = 0;

    /* Create default COA structure */
    const char *root_codes[] = {"1", "2", "3", "4", "5", "6", "7", NULL};
    const char *root_names[] = {"Assets", "Liabilities", "Equity",
                                 "Revenue", "Expenses", "Other Income",
                                 "Other Expense", NULL};
    MENT_AccountType root_types[] = {
        MENT_ACCTYPE_ASSET, MENT_ACCTYPE_LIABILITY, MENT_ACCTYPE_EQUITY,
        MENT_ACCTYPE_REVENUE, MENT_ACCTYPE_EXPENSE, MENT_ACCTYPE_OTHER_INCOME,
        MENT_ACCTYPE_OTHER_EXPENSE
    };

    for (int i = 0; root_codes[i]; i++) {
        ment_erp_add_account(erp, root_codes[i], root_names[i],
                              root_types[i], -1);
    }

    return erp;
}

void ment_erp_destroy(MENT_ERPInstance *erp) {
    if (!erp) return;
    for (int i = 0; i < erp->num_journals; i++) {
        free(erp->journals[i]->lines);
        free(erp->journals[i]);
    }
    free(erp->ledger);
    free(erp);
}

/* ---- Fiscal Period ---- */

int ment_erp_setup_fiscal_year(MENT_ERPInstance *erp, int year,
                                time_t start_date) {
    if (!erp) return -1;
    if (erp->num_periods + 12 > MENT_ERP_MAX_PERIODS) return -1;

    struct tm tm_start;
    localtime_s(&tm_start, &start_date);

    for (int p = 0; p < 12; p++) {
        MENT_FiscalPeriod *per = &erp->periods[erp->num_periods];
        per->year = year;
        per->period = p + 1;
        per->is_closed = false;
        per->is_adjustment_period = false;

        struct tm tm_p = tm_start;
        tm_p.tm_mon += p;
        tm_p.tm_mday = 1;
        per->start_date = mktime(&tm_p);

        tm_p.tm_mon += 1;
        tm_p.tm_mday = 0; /* last day of previous month */
        per->end_date = mktime(&tm_p);

        erp->num_periods++;
    }

    /* Adjustment period (13th) */
    MENT_FiscalPeriod *adj = &erp->periods[erp->num_periods++];
    adj->year = year;
    adj->period = 13;
    adj->is_closed = false;
    adj->is_adjustment_period = true;

    return 0;
}

int ment_erp_close_period(MENT_ERPInstance *erp, int period) {
    if (!erp) return -1;
    for (int i = 0; i < erp->num_periods; i++) {
        if (erp->periods[i].period == period &&
            erp->periods[i].year == erp->current_year) {
            erp->periods[i].is_closed = true;
            return 0;
        }
    }
    return -1;
}

/* ---- Chart of Accounts ---- */

int ment_erp_add_account(MENT_ERPInstance *erp, const char *code,
                          const char *name, MENT_AccountType type,
                          int parent_idx) {
    if (!erp || !code || !name) return -1;
    if (erp->num_accounts >= MENT_ERP_MAX_ACCOUNTS) return -1;

    int idx = erp->num_accounts++;
    MENT_Account *acct = &erp->accounts[idx];
    strncpy(acct->code, code, MENT_ERP_COA_CODE_LEN - 1);
    strncpy(acct->name, name, MENT_ERP_DESC_LEN - 1);
    acct->type = type;
    acct->parent_idx = parent_idx;
    acct->is_active = true;
    acct->allows_direct_posting = true;
    acct->normal_balance = ment_acctype_normal_balance(type);

    /* Determine level */
    if (parent_idx >= 0 && parent_idx < erp->num_accounts) {
        acct->level = erp->accounts[parent_idx].level + 1;
    } else {
        acct->level = 0;
    }

    return idx;
}

int ment_erp_find_account(const MENT_ERPInstance *erp, const char *code) {
    if (!erp || !code) return -1;
    for (int i = 0; i < erp->num_accounts; i++) {
        if (strcmp(erp->accounts[i].code, code) == 0) return i;
    }
    return -1;
}

int ment_erp_account_children(const MENT_ERPInstance *erp, int account_idx,
                               int *children, int max_children) {
    if (!erp || !children || account_idx < 0 ||
        account_idx >= erp->num_accounts) return -1;

    int count = 0;
    for (int i = 0; i < erp->num_accounts && count < max_children; i++) {
        if (erp->accounts[i].parent_idx == account_idx) {
            children[count++] = i;
        }
    }
    return count;
}

bool ment_erp_validate_coa(const MENT_ERPInstance *erp) {
    if (!erp) return false;
    /* Check accounting equation: Σ debit-normal = Σ credit-normal */
    float total_debit = 0.0f, total_credit = 0.0f;
    for (int i = 0; i < erp->num_accounts; i++) {
        if (erp->accounts[i].normal_balance == MENT_DEBIT) {
            total_debit += erp->accounts[i].opening_balance;
        } else {
            total_credit += erp->accounts[i].opening_balance;
        }
    }
    return fabsf(total_debit - total_credit) < MENT_EPS;
}

/* ---- Journal Entry & Posting ---- */

MENT_JournalEntry* ment_erp_create_journal(MENT_ERPInstance *erp,
                                             time_t doc_date,
                                             const char *reference) {
    if (!erp) return NULL;
    if (erp->num_journals >= MENT_ERP_MAX_JOURNALS) return NULL;

    MENT_JournalEntry *je = (MENT_JournalEntry*)calloc(1, sizeof(MENT_JournalEntry));
    if (!je) return NULL;

    ment_erp_get_next_voucher(erp, je->voucher_no, MENT_ERP_VOUCHER_LEN);
    je->posting_date = doc_date;
    je->document_date = doc_date;
    if (reference) strncpy(je->reference, reference, sizeof(je->reference) - 1);
    je->cap_lines = 16;
    je->lines = (MENT_JournalLine*)calloc(je->cap_lines, sizeof(MENT_JournalLine));
    je->is_posted = false;
    je->is_reversed = false;
    je->fiscal_period = erp->current_period;
    je->fiscal_year = erp->current_year;
    je->created_at = time(NULL);

    if (!je->lines) {
        free(je);
        return NULL;
    }

    erp->journals[erp->num_journals++] = je;
    return je;
}

int ment_erp_journal_add_line(MENT_JournalEntry *journal, int account_idx,
                               float amount, MENT_DebitCredit dc,
                               const char *description) {
    if (!journal || amount < 0.0f) return -1;

    /* Resize if needed */
    if (journal->num_lines >= journal->cap_lines) {
        int new_cap = journal->cap_lines * 2;
        MENT_JournalLine *new_lines = (MENT_JournalLine*)realloc(
            journal->lines, new_cap * sizeof(MENT_JournalLine));
        if (!new_lines) return -1;
        journal->lines = new_lines;
        journal->cap_lines = new_cap;
    }

    int idx = journal->num_lines++;
    journal->lines[idx].account_idx = account_idx;
    journal->lines[idx].amount = amount;
    journal->lines[idx].dc_flag = dc;
    journal->lines[idx].cost_center = -1;
    journal->lines[idx].project_id = -1;
    if (description) {
        strncpy(journal->lines[idx].description, description,
                sizeof(journal->lines[idx].description) - 1);
    }
    return idx;
}

bool ment_erp_validate_journal(const MENT_ERPInstance *erp,
                                const MENT_JournalEntry *journal) {
    if (!erp || !journal) return false;
    if (journal->num_lines < 2) return false;

    float total_debit = 0.0f, total_credit = 0.0f;

    for (int i = 0; i < journal->num_lines; i++) {
        int acct_idx = journal->lines[i].account_idx;
        if (acct_idx < 0 || acct_idx >= erp->num_accounts) return false;
        if (!erp->accounts[acct_idx].is_active) return false;

        if (journal->lines[i].dc_flag == MENT_DEBIT) {
            total_debit += journal->lines[i].amount;
        } else {
            total_credit += journal->lines[i].amount;
        }
    }

    /* Double-entry: debits must equal credits */
    return fabsf(total_debit - total_credit) < MENT_EPS;
}

int ment_erp_post_journal(MENT_ERPInstance *erp, MENT_JournalEntry *journal) {
    if (!erp || !journal) return -1;
    if (journal->is_posted) return -2;

    /* Check if period is closed */
    for (int i = 0; i < erp->num_periods; i++) {
        if (erp->periods[i].period == journal->fiscal_period &&
            erp->periods[i].year == journal->fiscal_year) {
            if (erp->periods[i].is_closed) return -3;
            break;
        }
    }

    /* Validate */
    if (!ment_erp_validate_journal(erp, journal)) return -4;

    /* Post to ledger: for each line, update the account's period balance */
    for (int i = 0; i < journal->num_lines; i++) {
        int acct_idx = journal->lines[i].account_idx;
        float amt = journal->lines[i].amount;
        MENT_DebitCredit dc = journal->lines[i].dc_flag;

        /* Find or create ledger entry for this account/period */
        int ledger_idx = -1;
        for (int j = 0; j < erp->num_ledger_entries; j++) {
            if (erp->ledger[j].account_idx == acct_idx &&
                erp->ledger[j].period == journal->fiscal_period &&
                erp->ledger[j].fiscal_year == journal->fiscal_year) {
                ledger_idx = j;
                break;
            }
        }

        if (ledger_idx < 0) {
            /* Create new ledger entry */
            erp->ledger = (MENT_LedgerBalance*)realloc(
                erp->ledger, (erp->num_ledger_entries + 1) * sizeof(MENT_LedgerBalance));
            if (!erp->ledger) return -5;

            ledger_idx = erp->num_ledger_entries++;
            MENT_LedgerBalance *lb = &erp->ledger[ledger_idx];
            memset(lb, 0, sizeof(MENT_LedgerBalance));
            lb->account_idx = acct_idx;
            lb->period = journal->fiscal_period;
            lb->fiscal_year = journal->fiscal_year;

            /* Carry forward opening balance from previous period */
            for (int j = 0; j < erp->num_ledger_entries; j++) {
                if (j == ledger_idx) continue;
                if (erp->ledger[j].account_idx == acct_idx &&
                    erp->ledger[j].period == journal->fiscal_period - 1 &&
                    erp->ledger[j].fiscal_year == journal->fiscal_year) {
                    lb->opening_balance = erp->ledger[j].closing_balance;
                    break;
                }
            }
            if (lb->opening_balance == 0.0f && journal->fiscal_period == 1) {
                lb->opening_balance = erp->accounts[acct_idx].opening_balance;
            }
        }

        /* Update balances */
        MENT_LedgerBalance *lb = &erp->ledger[ledger_idx];
        if (dc == MENT_DEBIT) {
            lb->debit_total += amt;
        } else {
            lb->credit_total += amt;
        }
        lb->closing_balance = lb->opening_balance + lb->debit_total - lb->credit_total;
        lb->transaction_count++;
    }

    journal->is_posted = true;
    ment_erp_audit_log(erp, "SYSTEM", "POST_JOURNAL",
                        journal->voucher_no, -1, -1);

    return 0;
}

MENT_JournalEntry* ment_erp_reverse_journal(MENT_ERPInstance *erp,
                                              const MENT_JournalEntry *original) {
    if (!erp || !original || !original->is_posted) return NULL;

    MENT_JournalEntry *rev = ment_erp_create_journal(erp,
        original->document_date, original->reference);
    if (!rev) return NULL;

    /* Reverse each line (swap debit/credit) */
    for (int i = 0; i < original->num_lines; i++) {
        MENT_DebitCredit rev_dc = (original->lines[i].dc_flag == MENT_DEBIT)
                                   ? MENT_CREDIT : MENT_DEBIT;
        ment_erp_journal_add_line(rev, original->lines[i].account_idx,
                                   original->lines[i].amount, rev_dc,
                                   original->lines[i].description);
    }

    rev->is_reversed = true;
    strncpy(rev->reversed_voucher, original->voucher_no,
            MENT_ERP_VOUCHER_LEN - 1);

    return rev;
}

/* ---- General Ledger Queries ---- */

float ment_erp_account_balance(const MENT_ERPInstance *erp,
                                int account_idx, int period) {
    if (!erp || account_idx < 0 || account_idx >= erp->num_accounts)
        return 0.0f;

    /* Get raw balance (opening + debits - credits) */
    float raw = erp->accounts[account_idx].opening_balance;
    for (int i = 0; i < erp->num_ledger_entries; i++) {
        if (erp->ledger[i].account_idx == account_idx &&
            erp->ledger[i].period == period) {
            raw = erp->ledger[i].closing_balance;
            break;
        }
    }
    /* Sign-adjust: debit-normal accounts stay positive;
     * credit-normal accounts negate to show positive balance */
    if (erp->accounts[account_idx].normal_balance == MENT_DEBIT) {
        return raw;   /* debit balance = positive */
    } else {
        return -raw;  /* credit balance = positive (negate stored negative) */
    }
}

int ment_erp_trial_balance(const MENT_ERPInstance *erp, int period,
                            float *debits, float *credits) {
    if (!erp) return -1;
    float total_db = 0.0f, total_cr = 0.0f;

    for (int i = 0; i < erp->num_accounts; i++) {
        float bal = ment_erp_account_balance(erp, i, period);
        if (fabsf(bal) < MENT_EPS) continue;

        /* Sign-adjusted: all balances positive.
         * Debit-normal accounts = debit balances.
         * Credit-normal accounts = credit balances. */
        if (erp->accounts[i].normal_balance == MENT_DEBIT) {
            total_db += bal;
        } else {
            total_cr += bal;
        }
    }

    if (debits) *debits = total_db;
    if (credits) *credits = total_cr;
    return 0;
}

int ment_erp_balance_sheet(const MENT_ERPInstance *erp, int period,
                            float *total_assets, float *total_liabilities,
                            float *total_equity) {
    if (!erp) return -1;
    float assets = 0.0f, liabilities = 0.0f, equity = 0.0f;

    for (int i = 0; i < erp->num_accounts; i++) {
        float bal = ment_erp_account_balance(erp, i, period);

        switch (erp->accounts[i].type) {
            case MENT_ACCTYPE_ASSET:
                assets += bal;
                break;
            case MENT_ACCTYPE_LIABILITY:
                liabilities += bal;
                break;
            case MENT_ACCTYPE_EQUITY:
                equity += bal;
                break;
            default:
                break;
        }
    }

    if (total_assets) *total_assets = assets;
    if (total_liabilities) *total_liabilities = liabilities;
    if (total_equity) *total_equity = equity;
    return 0;
}

int ment_erp_income_statement(const MENT_ERPInstance *erp,
                               int period_start, int period_end,
                               float *total_revenue, float *total_expenses,
                               float *net_income) {
    if (!erp) return -1;
    float revenue = 0.0f, expenses = 0.0f;

    for (int p = period_start; p <= period_end; p++) {
        for (int i = 0; i < erp->num_accounts; i++) {
            float bal = ment_erp_account_balance(erp, i, p);

            /* Period change = closing - opening, sign-adjusted */
            float opening_raw = 0.0f;
            for (int j = 0; j < erp->num_ledger_entries; j++) {
                if (erp->ledger[j].account_idx == i &&
                    erp->ledger[j].period == p) {
                    opening_raw = erp->ledger[j].opening_balance;
                    break;
                }
            }
            /* Sign-adjust opening to match sign-adjusted bal */
            float opening;
            if (erp->accounts[i].normal_balance == MENT_DEBIT) {
                opening = opening_raw;
            } else {
                opening = -opening_raw;
            }
            float change = bal - opening;

            switch (erp->accounts[i].type) {
                case MENT_ACCTYPE_REVENUE:
                case MENT_ACCTYPE_OTHER_INCOME:
                    revenue += change;
                    break;
                case MENT_ACCTYPE_EXPENSE:
                case MENT_ACCTYPE_OTHER_EXPENSE:
                    expenses += change;
                    break;
                default:
                    break;
            }
        }
    }

    float ni = revenue - expenses;
    if (total_revenue) *total_revenue = revenue;
    if (total_expenses) *total_expenses = expenses;
    if (net_income) *net_income = ni;
    return 0;
}

int ment_erp_account_transactions(const MENT_ERPInstance *erp,
                                   int account_idx, int period_start,
                                   int period_end,
                                   int *journal_indices, int max_results) {
    if (!erp || !journal_indices || account_idx < 0) return -1;

    int count = 0;
    for (int i = 0; i < erp->num_journals && count < max_results; i++) {
        MENT_JournalEntry *je = erp->journals[i];
        if (!je->is_posted) continue;
        if (je->fiscal_period < period_start || je->fiscal_period > period_end)
            continue;

        for (int j = 0; j < je->num_lines; j++) {
            if (je->lines[j].account_idx == account_idx) {
                journal_indices[count++] = i;
                break;
            }
        }
    }
    return count;
}

/* ---- Multi-Currency ---- */

int ment_erp_add_currency(MENT_ERPInstance *erp, const char *code,
                           const char *name, float rate, const char *symbol) {
    if (!erp || !code || erp->num_currencies >= MENT_ERP_MAX_CURRENCIES)
        return -1;

    int idx = erp->num_currencies++;
    strncpy(erp->currencies[idx].code, code, sizeof(erp->currencies[idx].code) - 1);
    strncpy(erp->currencies[idx].name, name, sizeof(erp->currencies[idx].name) - 1);
    erp->currencies[idx].exchange_rate = rate;
    erp->currencies[idx].decimal_places = 2;
    if (symbol) {
        strncpy(erp->currencies[idx].symbol, symbol,
                sizeof(erp->currencies[idx].symbol) - 1);
    }

    if (erp->num_currencies > 1) erp->multi_currency = true;
    return idx;
}

float ment_erp_convert_currency(const MENT_ERPInstance *erp,
                                 float amount, int src_currency) {
    if (!erp || src_currency < 0 || src_currency >= erp->num_currencies)
        return amount;
    if (src_currency == erp->base_currency_idx) return amount;
    return amount * erp->currencies[src_currency].exchange_rate;
}

int ment_erp_revalue_currencies(MENT_ERPInstance *erp, int period) {
    (void)period;
    if (!erp || !erp->multi_currency) return 0;
    /* Period-end revaluation: adjust foreign currency balances */
    /* Simplified: use current rate to revalue */
    return 0;
}

/* ---- Period Close ---- */

int ment_erp_period_close(MENT_ERPInstance *erp, int period) {
    if (!erp) return -1;

    /* 1. Post recurring entries */
    /* (simplified: depreciation, accruals would go here) */

    /* 2. Revalue currencies */
    ment_erp_revalue_currencies(erp, period);

    /* 3. Close revenue/expense to retained earnings */
    /* Revenue and expense accounts are temporary;
     * their balances are transferred to retained earnings.
     *
     * Close entries:
     *   Debit Revenue, Credit Income Summary
     *   Debit Income Summary, Credit Expenses
     *   Debit/Credit Income Summary → Retained Earnings
     */

    /* Find retained earnings account (usually under Equity) */
    int re_account = -1;
    for (int i = 0; i < erp->num_accounts; i++) {
        if (erp->accounts[i].type == MENT_ACCTYPE_EQUITY &&
            strstr(erp->accounts[i].name, "Retained") != NULL) {
            re_account = i;
            break;
        }
    }

    if (re_account >= 0 && period == 12) {
        /* Year-end close: transfer net income to retained earnings */
        float revenue = 0.0f, expenses = 0.0f;
        ment_erp_income_statement(erp, 1, 12, &revenue, &expenses, NULL);

        /* Create closing entries */
        MENT_JournalEntry *close_rev = ment_erp_create_journal(erp,
            erp->periods[period - 1].end_date, "Year-End Close - Revenue");
        MENT_JournalEntry *close_exp = ment_erp_create_journal(erp,
            erp->periods[period - 1].end_date, "Year-End Close - Expense");

        /* Close revenue accounts */
        for (int i = 0; i < erp->num_accounts; i++) {
            float bal = ment_erp_account_balance(erp, i, period);
            if (fabsf(bal) < MENT_EPS) continue;

            if (erp->accounts[i].type == MENT_ACCTYPE_REVENUE) {
                ment_erp_journal_add_line(close_rev, i, fabsf(bal),
                    (bal > 0) ? MENT_DEBIT : MENT_CREDIT, "Close Revenue");
                ment_erp_journal_add_line(close_rev, re_account, fabsf(bal),
                    (bal > 0) ? MENT_CREDIT : MENT_DEBIT, "To Retained Earnings");
            } else if (erp->accounts[i].type == MENT_ACCTYPE_EXPENSE) {
                ment_erp_journal_add_line(close_exp, re_account, fabsf(bal),
                    (bal > 0) ? MENT_DEBIT : MENT_CREDIT, "From Retained Earnings");
                ment_erp_journal_add_line(close_exp, i, fabsf(bal),
                    (bal > 0) ? MENT_CREDIT : MENT_DEBIT, "Close Expense");
            }
        }

        if (close_rev->num_lines > 0) ment_erp_post_journal(erp, close_rev);
        if (close_exp->num_lines > 0) ment_erp_post_journal(erp, close_exp);
    }

    /* 4. Mark period as closed */
    ment_erp_close_period(erp, period);

    return 0;
}

int ment_erp_consolidate(MENT_ERPInstance *parent,
                          MENT_ERPInstance **subsidiaries, int num_subs) {
    if (!parent || !subsidiaries || num_subs < 1) return -1;

    /* Consolidation:
     * 1. Sum all subsidiary balances into parent
     * 2. Eliminate inter-company transactions
     * 3. Adjust for minority interest
     *
     * Simplified: just sum balances
     */
    for (int s = 0; s < num_subs; s++) {
        MENT_ERPInstance *sub = subsidiaries[s];
        if (!sub) continue;

        for (int i = 0; i < sub->num_ledger_entries; i++) {
            /* Add subsidiary balance to parent's corresponding account */
            int sub_acct = sub->ledger[i].account_idx;
            /* Map subsidiary account to parent account (simplified: same index) */

            bool found = false;
            for (int j = 0; j < parent->num_ledger_entries; j++) {
                if (parent->ledger[j].account_idx == sub_acct &&
                    parent->ledger[j].period == sub->ledger[i].period) {
                    parent->ledger[j].debit_total += sub->ledger[i].debit_total;
                    parent->ledger[j].credit_total += sub->ledger[i].credit_total;
                    parent->ledger[j].closing_balance +=
                        sub->ledger[i].closing_balance;
                    found = true;
                    break;
                }
            }

            if (!found) {
                parent->ledger = (MENT_LedgerBalance*)realloc(
                    parent->ledger,
                    (parent->num_ledger_entries + 1) * sizeof(MENT_LedgerBalance));
                parent->ledger[parent->num_ledger_entries] = sub->ledger[i];
                parent->num_ledger_entries++;
            }
        }
    }

    return 0;
}

/* ---- Audit Trail ---- */

/* Internal audit storage - simplified as a basic log */
#define MENT_AUDIT_MAX_ENTRIES 32768

typedef struct {
    MENT_AuditEntry *entries;
    int count;
    int capacity;
} MENT_AuditLog;

static MENT_AuditLog *_audit_log = NULL;

static void _ensure_audit_log(void) {
    if (!_audit_log) {
        _audit_log = (MENT_AuditLog*)calloc(1, sizeof(MENT_AuditLog));
        _audit_log->capacity = 4096;
        _audit_log->entries = (MENT_AuditEntry*)calloc(
            _audit_log->capacity, sizeof(MENT_AuditEntry));
    }
}

void ment_erp_audit_log(MENT_ERPInstance *erp, const char *user,
                          const char *action, const char *detail,
                          int journal_idx, int account_idx) {
    (void)erp;
    _ensure_audit_log();

    if (_audit_log->count >= _audit_log->capacity) {
        _audit_log->capacity *= 2;
        _audit_log->entries = (MENT_AuditEntry*)realloc(
            _audit_log->entries,
            _audit_log->capacity * sizeof(MENT_AuditEntry));
    }

    MENT_AuditEntry *e = &_audit_log->entries[_audit_log->count++];
    e->timestamp = time(NULL);
    strncpy(e->user, user, sizeof(e->user) - 1);
    strncpy(e->action, action, sizeof(e->action) - 1);
    strncpy(e->detail, detail, sizeof(e->detail) - 1);
    e->journal_idx = journal_idx;
    e->account_idx = account_idx;
}

int ment_erp_audit_query(const MENT_ERPInstance *erp,
                          time_t from, time_t to,
                          MENT_AuditEntry *results, int max_results) {
    (void)erp;
    _ensure_audit_log();
    if (!results || !_audit_log) return 0;

    int count = 0;
    for (int i = _audit_log->count - 1; i >= 0 && count < max_results; i--) {
        if (_audit_log->entries[i].timestamp >= from &&
            _audit_log->entries[i].timestamp <= to) {
            results[count++] = _audit_log->entries[i];
        }
    }
    return count;
}

bool ment_erp_audit_verify(const MENT_ERPInstance *erp) {
    if (!erp) return false;

    /* Verify that all posted journals are in the audit log */
    for (int i = 0; i < erp->num_journals; i++) {
        if (!erp->journals[i]->is_posted) continue;
        /* Verify journal voucher appears in audit log */
        bool found = false;
        for (int j = 0; _audit_log && j < _audit_log->count; j++) {
            if (strcmp(_audit_log->entries[j].detail,
                       erp->journals[i]->voucher_no) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}
