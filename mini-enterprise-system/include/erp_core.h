#ifndef MINI_ENT_ERP_CORE_H
#define MINI_ENT_ERP_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* ================================================================
 *  ERP Core — Enterprise Resource Planning Foundation
 *
 *  L1: Core definitions — Chart of Accounts, Journal Entry, Ledger
 *  L2: Core concepts — Double-entry bookkeeping, COA hierarchy,
 *      Fiscal period management
 *  L3: Engineering structures — B-tree account index, hash-based
 *      voucher lookup, audit trail
 *  L4: Standards — GAAP/IFRS accounting principles, SOX compliance
 *      (Sarbanes-Oxley Act 2002), COSO framework
 *  L5: Algorithms — Account tree traversal, period close, currency
 *      conversion, consolidation
 *  L6: Canonical problems — General Ledger, Accounts Payable/Receivable
 *  L7: Applications — SME ERP, multi-entity consolidation
 *  L8: Advanced — Real-time consolidation, IFRS 16 lease accounting
 *  L9: Industry — SAP S/4HANA, Oracle NetSuite, Odoo architecture
 *
 *  References:
 *  - IASB, "IFRS Standards" (International Accounting Standards Board)
 *  - COSO, "Internal Control — Integrated Framework" (2013)
 *  - McCarthy, "The REA Accounting Model" (The Accounting Review 1982)
 * ================================================================ */

#define MENT_ERP_MAX_ACCOUNTS    8192
#define MENT_ERP_MAX_JOURNALS    65536
#define MENT_ERP_MAX_PERIODS     64
#define MENT_ERP_MAX_CURRENCIES  32
#define MENT_ERP_COA_CODE_LEN    24
#define MENT_ERP_VOUCHER_LEN     32
#define MENT_ERP_DESC_LEN        256

/* ---- L1: Account Types (Standard COA) ---- */

typedef enum {
    MENT_ACCTYPE_ASSET       = 0,  /* 1xxx */
    MENT_ACCTYPE_LIABILITY   = 1,  /* 2xxx */
    MENT_ACCTYPE_EQUITY      = 2,  /* 3xxx */
    MENT_ACCTYPE_REVENUE     = 3,  /* 4xxx */
    MENT_ACCTYPE_EXPENSE     = 4,  /* 5xxx */
    MENT_ACCTYPE_OTHER_INCOME = 5, /* 6xxx */
    MENT_ACCTYPE_OTHER_EXPENSE = 6 /* 7xxx */
} MENT_AccountType;

typedef enum {
    MENT_DEBIT  = 0,
    MENT_CREDIT = 1
} MENT_DebitCredit;

/* ---- L1: Chart of Accounts Entry ---- */

typedef struct {
    char     code[MENT_ERP_COA_CODE_LEN];
    char     name[MENT_ERP_DESC_LEN];
    MENT_AccountType type;
    int      parent_idx;       /* hierarchical structure */
    int      level;            /* 0=root category, 1=sub-account, ... */
    bool     is_active;
    bool     is_control_account; /* e.g., AR control, AP control */
    bool     allows_direct_posting;
    float    opening_balance;
    MENT_DebitCredit normal_balance; /* debit normal or credit normal */
} MENT_Account;

/* ---- L1: Journal Entry Line ---- */

typedef struct {
    int      account_idx;      /* index into chart of accounts */
    float    amount;
    MENT_DebitCredit dc_flag;  /* debit or credit */
    char     description[MENT_ERP_DESC_LEN / 2];
    int      cost_center;      /* -1 if not applicable */
    int      project_id;       /* -1 if not applicable */
} MENT_JournalLine;

/* ---- L1: Journal Entry / Voucher ---- */

typedef struct {
    char     voucher_no[MENT_ERP_VOUCHER_LEN];
    time_t   posting_date;
    time_t   document_date;
    char     reference[64];
    MENT_JournalLine *lines;
    int      num_lines;
    int      cap_lines;
    bool     is_posted;        /* posted to GL */
    bool     is_reversed;      /* reversal entry */
    char     reversed_voucher[MENT_ERP_VOUCHER_LEN];
    int      fiscal_period;
    int      fiscal_year;
    char     created_by[32];
    time_t   created_at;
} MENT_JournalEntry;

/* ---- L1: Ledger Balance (per account per period) ---- */

typedef struct {
    int      account_idx;
    int      period;
    int      fiscal_year;
    float    opening_balance;
    float    debit_total;
    float    credit_total;
    float    closing_balance;
    int      transaction_count;
} MENT_LedgerBalance;

/* ---- L1: Fiscal Period ---- */

typedef struct {
    int      year;
    int      period;           /* 1-12 for monthly, 1-4 for quarterly */
    time_t   start_date;
    time_t   end_date;
    bool     is_closed;
    bool     is_adjustment_period; /* period 13 for adjustments */
} MENT_FiscalPeriod;

/* ---- L1: Currency ---- */

typedef struct {
    char     code[4];          /* ISO 4217 */
    char     name[32];
    float    exchange_rate;    /* to base currency */
    int      decimal_places;
    char     symbol[8];
} MENT_Currency;

/* ---- L1: ERP Instance ---- */

typedef struct {
    MENT_Account       accounts[MENT_ERP_MAX_ACCOUNTS];
    int                num_accounts;
    MENT_JournalEntry  *journals[MENT_ERP_MAX_JOURNALS];
    int                num_journals;
    MENT_LedgerBalance *ledger;       /* dynamically allocated per period */
    int                num_ledger_entries;
    MENT_FiscalPeriod  periods[MENT_ERP_MAX_PERIODS];
    int                num_periods;
    MENT_Currency      currencies[MENT_ERP_MAX_CURRENCIES];
    int                num_currencies;
    int                base_currency_idx;
    int                current_period;
    int                current_year;
    char               company_name[128];
    char               company_code[16];
    bool               multi_currency;
    int                next_voucher_seq;
} MENT_ERPInstance;

/* ================================================================
 *  API: ERP Instance Management
 * ================================================================ */

MENT_ERPInstance* ment_erp_create(const char *company_name,
                                   const char *company_code);
void ment_erp_destroy(MENT_ERPInstance *erp);

/** Set up fiscal year with 12 monthly periods */
int  ment_erp_setup_fiscal_year(MENT_ERPInstance *erp, int year,
                                 time_t start_date);

/** Close a fiscal period (no more postings allowed) */
int  ment_erp_close_period(MENT_ERPInstance *erp, int period);

/* ================================================================
 *  API: Chart of Accounts (L2: Double-Entry Foundation)
 * ================================================================ */

/** Add an account to the COA */
int  ment_erp_add_account(MENT_ERPInstance *erp, const char *code,
                           const char *name, MENT_AccountType type,
                           int parent_idx);

/** Look up account by code */
int  ment_erp_find_account(const MENT_ERPInstance *erp, const char *code);

/** Get all children of an account (for hierarchical rollup) */
int  ment_erp_account_children(const MENT_ERPInstance *erp, int account_idx,
                                int *children, int max_children);

/** Validate COA: check that total debits = total credits in trial balance */
bool ment_erp_validate_coa(const MENT_ERPInstance *erp);

/* ================================================================
 *  API: Journal Entry & Posting (L4: GAAP/IFRS)
 *
 *  Core accounting equation: Assets = Liabilities + Equity
 *  Extended: Assets + Expenses = Liabilities + Equity + Revenue
 *
 *  Every journal entry must satisfy: Σ debits = Σ credits
 *  (Double-entry bookkeeping principle, Pacioli 1494)
 * ================================================================ */

/** Create a journal entry (voucher) */
MENT_JournalEntry* ment_erp_create_journal(MENT_ERPInstance *erp,
                                             time_t doc_date,
                                             const char *reference);

/** Add a line to a journal entry */
int  ment_erp_journal_add_line(MENT_JournalEntry *journal, int account_idx,
                                float amount, MENT_DebitCredit dc,
                                const char *description);

/**
 * Validate journal entry: ensure debits = credits within tolerance.
 * Also validates that control accounts are posted through subledger.
 */
bool ment_erp_validate_journal(const MENT_ERPInstance *erp,
                                const MENT_JournalEntry *journal);

/**
 * Post journal entry to General Ledger.
 * Updates ledger balances for the current period.
 * After posting, journal becomes immutable (is_posted = true).
 * Returns 0 on success, -1 if validation fails.
 */
int  ment_erp_post_journal(MENT_ERPInstance *erp, MENT_JournalEntry *journal);

/** Reverse a posted journal entry (creates offsetting entry) */
MENT_JournalEntry* ment_erp_reverse_journal(MENT_ERPInstance *erp,
                                              const MENT_JournalEntry *original);

/* ================================================================
 *  API: General Ledger Queries
 * ================================================================ */

/** Get account balance for a period */
float ment_erp_account_balance(const MENT_ERPInstance *erp,
                                int account_idx, int period);

/** Get trial balance (all accounts with balances for a period) */
int  ment_erp_trial_balance(const MENT_ERPInstance *erp, int period,
                             float *debits, float *credits);

/** Generate balance sheet (assets, liabilities, equity) */
int  ment_erp_balance_sheet(const MENT_ERPInstance *erp, int period,
                             float *total_assets, float *total_liabilities,
                             float *total_equity);

/** Generate income statement (revenue - expenses = net income) */
int  ment_erp_income_statement(const MENT_ERPInstance *erp,
                                int period_start, int period_end,
                                float *total_revenue, float *total_expenses,
                                float *net_income);

/** Get account transaction history (all postings to an account) */
int  ment_erp_account_transactions(const MENT_ERPInstance *erp,
                                    int account_idx, int period_start,
                                    int period_end,
                                    int *journal_indices, int max_results);

/* ================================================================
 *  API: Multi-Currency (L8: IFRS IAS 21)
 * ================================================================ */

/** Add a currency with exchange rate */
int  ment_erp_add_currency(MENT_ERPInstance *erp, const char *code,
                            const char *name, float rate, const char *symbol);

/** Convert amount from source currency to base currency */
float ment_erp_convert_currency(const MENT_ERPInstance *erp,
                                 float amount, int src_currency);

/** Revalue foreign currency balances (period-end adjustment) */
int  ment_erp_revalue_currencies(MENT_ERPInstance *erp, int period);

/* ================================================================
 *  API: Period Close & Consolidation (L5)
 * ================================================================ */

/**
 * Execute period-end close:
 * 1. Post recurring entries (depreciation, accruals)
 * 2. Revalue foreign currencies
 * 3. Close revenue/expense to retained earnings
 * 4. Mark period as closed
 */
int  ment_erp_period_close(MENT_ERPInstance *erp, int period);

/** Consolidate multiple ERP instances (multi-entity) */
int  ment_erp_consolidate(MENT_ERPInstance *parent,
                           MENT_ERPInstance **subsidiaries, int num_subs);

/* ================================================================
 *  API: Audit Trail (L4: SOX Compliance)
 * ================================================================ */

typedef struct {
    time_t   timestamp;
    char     user[32];
    char     action[64];
    char     detail[256];
    int      journal_idx;     /* -1 if not journal-related */
    int      account_idx;     /* -1 if not account-related */
} MENT_AuditEntry;

/** Log an audit entry */
void ment_erp_audit_log(MENT_ERPInstance *erp, const char *user,
                          const char *action, const char *detail,
                          int journal_idx, int account_idx);

/** Query audit trail within date range */
int  ment_erp_audit_query(const MENT_ERPInstance *erp,
                           time_t from, time_t to,
                           MENT_AuditEntry *results, int max_results);

/** Verify that no posted journal has been modified (checksum-based) */
bool ment_erp_audit_verify(const MENT_ERPInstance *erp);

/* ---- Utility ---- */

const char* ment_acctype_name(MENT_AccountType type);
MENT_DebitCredit ment_acctype_normal_balance(MENT_AccountType type);
int  ment_erp_get_next_voucher(MENT_ERPInstance *erp, char *buf, int bufsize);

#endif /* MINI_ENT_ERP_CORE_H */
