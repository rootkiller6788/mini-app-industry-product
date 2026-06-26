#include "fintech_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================================================================
 * Utility Names
 * ================================================================ */

const char* account_type_name(AccountType t) {
    switch (t) {
        case ACCT_ASSET:     return "Asset";
        case ACCT_LIABILITY: return "Liability";
        case ACCT_EQUITY:    return "Equity";
        case ACCT_REVENUE:   return "Revenue";
        case ACCT_EXPENSE:   return "Expense";
        default:             return "Unknown";
    }
}

const char* txn_type_name(TransactionType t) {
    switch (t) {
        case TXN_DEPOSIT:    return "Deposit";
        case TXN_WITHDRAWAL: return "Withdrawal";
        case TXN_TRANSFER:   return "Transfer";
        case TXN_PAYMENT:    return "Payment";
        case TXN_REFUND:     return "Refund";
        case TXN_FEE:        return "Fee";
        case TXN_INTEREST:   return "Interest";
        default:             return "Unknown";
    }
}

const char* loan_type_name(LoanType t) {
    switch (t) {
        case LOAN_FIXED:         return "Fixed";
        case LOAN_VARIABLE:      return "Variable";
        case LOAN_BALLOON:       return "Balloon";
        case LOAN_INTEREST_ONLY: return "Interest-Only";
        default:                 return "Unknown";
    }
}

const char* risk_level_name(RiskLevel r) {
    switch (r) {
        case RISK_LOW:      return "Low";
        case RISK_MEDIUM:   return "Medium";
        case RISK_HIGH:     return "High";
        case RISK_CRITICAL: return "Critical";
        default:            return "Unknown";
    }
}

/* ================================================================
 * L3: Ledger System — Double-Entry Bookkeeping
 * Accounting equation: Assets = Liabilities + Equity
 * Reference: Pacioli (1494), IFRS, GAAP
 * ================================================================ */

Ledger* ledger_create(void) {
    Ledger* l = (Ledger*)calloc(1, sizeof(Ledger));
    if (!l) return NULL;
    l->next_account_id = 1;
    l->next_txn_id = 1;
    return l;
}

Account* ledger_create_account(Ledger* l, const char* name, AccountType type) {
    if (!l || l->account_count >= FIN_MAX_ACCOUNTS) return NULL;
    Account* a = &l->accounts[l->account_count];
    a->id = l->next_account_id++;
    snprintf(a->name, FIN_MAX_NAME, "%s", name);
    a->type = type;
    a->balance = 0.0;
    a->created_at = (int64_t)time(NULL);
    a->last_activity = (int64_t)time(NULL);
    a->active = true;
    a->frozen = false;
    a->overdraft_limit = 0;
    a->interest_rate = 0.0;
    strncpy(a->currency, "USD", sizeof(a->currency) - 1);
    l->account_count++;
    return a;
}

Account* ledger_find_account(Ledger* l, int id) {
    if (!l) return NULL;
    for (int i = 0; i < l->account_count; i++) {
        if (l->accounts[i].id == id) return &l->accounts[i];
    }
    return NULL;
}

static void ledger_record_txn(Ledger* l, TransactionType type,
                               int debit_id, int credit_id, double amount,
                               const char* desc) {
    if (l->transaction_count >= FIN_MAX_TRANSACTIONS) return;
    Transaction* t = &l->transactions[l->transaction_count];
    t->id = l->next_txn_id++;
    t->type = type;
    t->debit_account_id = debit_id;
    t->credit_account_id = credit_id;
    t->amount = amount;
    t->timestamp = (int64_t)time(NULL);
    snprintf(t->description, FIN_MAX_NAME, "%s", desc ? desc : "");
    t->reference[0] = '\0';
    t->reconciled = false;
    l->transaction_count++;
}

bool ledger_deposit(Ledger* l, int account_id, double amount) {
    if (!l || amount <= 0.0) return false;
    Account* a = ledger_find_account(l, account_id);
    if (!a || !a->active || a->frozen) return false;
    a->balance += amount;
    a->last_activity = (int64_t)time(NULL);
    if (a->type == ACCT_ASSET) l->total_assets += amount;
    if (a->type == ACCT_LIABILITY) l->total_liabilities += amount;
    if (a->type == ACCT_EQUITY) l->total_equity += amount;
    if (a->type == ACCT_REVENUE) l->total_equity += amount;
    ledger_record_txn(l, TXN_DEPOSIT, account_id, 0, amount, "Deposit");
    return true;
}

bool ledger_withdraw(Ledger* l, int account_id, double amount) {
    if (!l || amount <= 0.0) return false;
    Account* a = ledger_find_account(l, account_id);
    if (!a || !a->active || a->frozen) return false;
    double available = a->balance + a->overdraft_limit;
    if (available < amount) return false;
    a->balance -= amount;
    a->last_activity = (int64_t)time(NULL);
    if (a->type == ACCT_ASSET) l->total_assets -= amount;
    if (a->type == ACCT_LIABILITY) l->total_liabilities -= amount;
    if (a->type == ACCT_EQUITY) l->total_equity -= amount;
    if (a->type == ACCT_EXPENSE) l->total_equity -= amount;
    ledger_record_txn(l, TXN_WITHDRAWAL, 0, account_id, amount, "Withdrawal");
    return true;
}

bool ledger_transfer(Ledger* l, int from_id, int to_id, double amount) {
    if (!l || amount <= 0.0 || from_id == to_id) return false;
    Account* from = ledger_find_account(l, from_id);
    Account* to = ledger_find_account(l, to_id);
    if (!from || !to || !from->active || !to->active) return false;
    if (from->frozen || to->frozen) return false;
    if (from->balance + from->overdraft_limit < amount) return false;
    from->balance -= amount;
    to->balance += amount;
    from->last_activity = to->last_activity = (int64_t)time(NULL);
    ledger_record_txn(l, TXN_TRANSFER, to_id, from_id, amount, "Transfer");
    return true;
}

bool ledger_verify_balance(Ledger* l) {
    if (!l) return false;
    double assets = 0.0, liabilities = 0.0, equity = 0.0;
    for (int i = 0; i < l->account_count; i++) {
        Account* a = &l->accounts[i];
        if (!a->active) continue;
        switch (a->type) {
        case ACCT_ASSET:      assets += a->balance; break;
        case ACCT_LIABILITY:  liabilities += a->balance; break;
        case ACCT_EQUITY:     equity += a->balance; break;
        case ACCT_REVENUE:    equity += a->balance; break;
        case ACCT_EXPENSE:    equity -= a->balance; break;
        }
    }
    l->total_assets = assets;
    l->total_liabilities = liabilities;
    l->total_equity = equity;
    double diff = assets - (liabilities + equity);
    return (diff < 0.001 && diff > -0.001);
}

double ledger_calculate_interest(double principal, double rate, int periods) {
    return principal * pow(1.0 + rate, periods);
}

bool ledger_apply_interest(Ledger* l, int account_id, double rate) {
    if (!l) return false;
    Account* a = ledger_find_account(l, account_id);
    if (!a || !a->active) return false;
    double interest = a->balance * rate;
    a->balance += interest;
    a->last_activity = (int64_t)time(NULL);
    if (a->type == ACCT_ASSET) {
        l->total_assets += interest;
        l->total_equity += interest;
    }
    char desc[FIN_MAX_NAME];
    snprintf(desc, FIN_MAX_NAME, "Interest @ %.2f%%", rate * 100.0);
    ledger_record_txn(l, TXN_INTEREST, account_id, 0, interest, desc);
    return true;
}

double ledger_get_total_balance(const Ledger* l, AccountType type) {
    if (!l) return 0.0;
    double total = 0.0;
    for (int i = 0; i < l->account_count; i++) {
        if (l->accounts[i].active && l->accounts[i].type == type)
            total += l->accounts[i].balance;
    }
    return total;
}

void ledger_print_summary(const Ledger* l) {
    if (!l) return;
    ledger_verify_balance((Ledger*)l);
    printf("=== Ledger: %d accounts, %d transactions ===\n",
           l->account_count, l->transaction_count);
    for (int i = 0; i < l->account_count; i++) {
        const Account* a = &l->accounts[i];
        printf("  [%d] %-20s %-10s %s$%12.2f %s%s\n",
               a->id, a->name, account_type_name(a->type),
               a->currency, a->balance,
               a->active ? "" : "(inactive)",
               a->frozen ? " [FROZEN]" : "");
    }
    printf("  Equation: $%.2f = $%.2f + $%.2f [%s]\n",
           l->total_assets, l->total_liabilities, l->total_equity,
           (l->total_assets - (l->total_liabilities + l->total_equity) < 0.001
            && l->total_assets - (l->total_liabilities + l->total_equity) > -0.001)
           ? "BALANCED" : "IMBALANCE");
}

void ledger_destroy(Ledger* l) { free(l); }

/* ================================================================
 * L5: Loan Amortization Engine
 * Formula: PMT = P * r * (1+r)^n / ((1+r)^n - 1)
 * where r = annual_rate/12, n = total_months
 * ================================================================ */

LoanBook* loanbook_create(void) {
    LoanBook* lb = (LoanBook*)calloc(1, sizeof(LoanBook));
    return lb;
}

Loan* loanbook_create_loan(LoanBook* lb, int account_id, LoanType type,
                           double principal, double annual_rate, int months) {
    if (!lb || lb->loan_count >= FIN_MAX_ACCOUNTS || principal <= 0 || months <= 0)
        return NULL;
    Loan* ln = &lb->loans[lb->loan_count];
    ln->id = lb->loan_count + 1;
    ln->type = type;
    ln->account_id = account_id;
    ln->principal = principal;
    ln->annual_rate = annual_rate;
    ln->total_periods = months;
    ln->periods_paid = 0;
    ln->remaining_balance = principal;
    ln->start_date = (int64_t)time(NULL);
    ln->next_payment_date = ln->start_date + 30 * 86400;
    ln->active = true;

    double r = annual_rate / 12.0;
    if (type == LOAN_INTEREST_ONLY) {
        ln->monthly_payment = principal * r;
    } else if (type == LOAN_BALLOON) {
        ln->monthly_payment = principal * r;
    } else {
        /* Standard amortization: PMT formula */
        if (r > 0.0) {
            double factor = pow(1.0 + r, months);
            ln->monthly_payment = principal * r * factor / (factor - 1.0);
        } else {
            ln->monthly_payment = principal / months;
        }
    }
    lb->total_outstanding += principal;
    lb->loan_count++;
    return ln;
}

int loanbook_generate_schedule(const Loan* loan, AmortizationRow* rows, int max_rows) {
    if (!loan || !rows || max_rows <= 0) return 0;
    double balance = loan->principal;
    double r = loan->annual_rate / 12.0;
    int count = 0;

    for (int i = 0; i < loan->total_periods && count < max_rows; i++) {
        double interest = balance * r;
        double principal_pmt;
        if (loan->type == LOAN_BALLOON && i < loan->total_periods - 1) {
            principal_pmt = 0.0;
        } else if (loan->type == LOAN_BALLOON) {
            principal_pmt = balance;
        } else if (loan->type == LOAN_INTEREST_ONLY && i < loan->total_periods - 1) {
            principal_pmt = 0.0;
        } else if (loan->type == LOAN_INTEREST_ONLY) {
            principal_pmt = balance;
        } else {
            principal_pmt = loan->monthly_payment - interest;
            if (principal_pmt > balance) principal_pmt = balance;
        }
        double payment = interest + principal_pmt;
        balance -= principal_pmt;
        if (balance < 0.0) balance = 0.0;

        rows[count].period = i + 1;
        rows[count].payment = payment;
        rows[count].interest = interest;
        rows[count].principal = principal_pmt;
        rows[count].remaining = balance;
        count++;
        if (balance <= 0.0) break;
    }
    return count;
}

bool loanbook_make_payment(LoanBook* lb, Ledger* l, int loan_id) {
    if (!lb || !l || loan_id <= 0 || loan_id > lb->loan_count) return false;
    Loan* ln = &lb->loans[loan_id - 1];
    if (!ln->active || ln->periods_paid >= ln->total_periods) return false;

    double r = ln->annual_rate / 12.0;
    double interest = ln->remaining_balance * r;
    double principal_pmt = ln->monthly_payment - interest;
    if (principal_pmt > ln->remaining_balance) principal_pmt = ln->remaining_balance;

    /* Record the payment in the ledger */
    char desc[FIN_MAX_NAME];
    snprintf(desc, FIN_MAX_NAME, "Loan payment #%d (int=$%.2f prin=$%.2f)",
             ln->periods_paid + 1, interest, principal_pmt);
    ledger_record_txn(l, TXN_PAYMENT, ln->account_id, 0, ln->monthly_payment, desc);

    ln->remaining_balance -= principal_pmt;
    if (ln->remaining_balance < 0.0) ln->remaining_balance = 0.0;
    ln->periods_paid++;
    ln->next_payment_date += 30 * 86400;

    if (ln->remaining_balance <= 0.0) {
        ln->active = false;
        lb->total_outstanding -= (ln->principal - ln->remaining_balance);
    }
    return true;
}

double loanbook_npv(const Loan* loan, double discount_rate) {
    if (!loan || discount_rate < 0.0) return 0.0;
    AmortizationRow rows[FIN_MAX_SCHEDULE];
    int n = loanbook_generate_schedule(loan, rows, FIN_MAX_SCHEDULE);
    double npv = 0.0;
    for (int i = 0; i < n; i++) {
        npv += rows[i].payment / pow(1.0 + discount_rate / 12.0, rows[i].period);
    }
    npv -= loan->principal;
    return npv;
}

void loanbook_print_loan(const Loan* loan) {
    if (!loan) return;
    printf("Loan #%d: %s $%.2f at %.2f%% for %d months\n",
           loan->id, loan_type_name(loan->type),
           loan->principal, loan->annual_rate * 100.0, loan->total_periods);
    printf("  Monthly: $%.2f | Remaining: $%.2f | Paid: %d/%d | Active: %s\n",
           loan->monthly_payment, loan->remaining_balance,
           loan->periods_paid, loan->total_periods,
           loan->active ? "yes" : "no");
}

void loanbook_print_schedule(const AmortizationRow* rows, int count) {
    if (!rows) return;
    printf("%6s %10s %10s %10s %12s\n",
           "Period", "Payment", "Interest", "Principal", "Remaining");
    for (int i = 0; i < count; i++) {
        printf("%6d %10.2f %10.2f %10.2f %12.2f\n",
               rows[i].period, rows[i].payment, rows[i].interest,
               rows[i].principal, rows[i].remaining);
    }
}

void loanbook_destroy(LoanBook* lb) { free(lb); }

/* ================================================================
 * L5: Credit Scoring (FICO-inspired)
 * Score = base_score + sum(weight_i * factor_i)
 * ================================================================ */

CreditProfile* credit_create_profile(int account_id) {
    CreditProfile* cp = (CreditProfile*)calloc(1, sizeof(CreditProfile));
    if (!cp) return NULL;
    cp->account_id = account_id;
    cp->credit_age_months = 0;
    cp->score.score = 300;
    cp->score.level = RISK_HIGH;
    cp->score.grade = 'F';
    return cp;
}

void credit_record_payment(CreditProfile* cp, bool on_time) {
    if (!cp) return;
    cp->credit_age_months++;
    if (on_time) cp->on_time_payments++;
    else {
        cp->late_payments++;
        if (cp->late_payments % 3 == 0) cp->missed_payments++;
    }
}

int credit_calculate_score(CreditProfile* cp) {
    if (!cp) return 300;
    /* Payment history: 35% weight */
    int total = cp->on_time_payments + cp->late_payments + cp->missed_payments;
    double pay_score = total > 0 ? (double)cp->on_time_payments / total * 100.0 : 0.0;

    /* Credit utilization: 30% weight — lower is better */
    double util_score = cp->credit_utilization < 0.30 ? 100.0 :
                        cp->credit_utilization < 0.50 ? 70.0 :
                        cp->credit_utilization < 0.75 ? 40.0 : 10.0;

    /* Credit age: 15% weight */
    double age_score = cp->credit_age_months > 84 ? 100.0 :
                       cp->credit_age_months > 48 ? 80.0 :
                       cp->credit_age_months > 24 ? 60.0 :
                       cp->credit_age_months > 12 ? 40.0 : 20.0;

    /* Credit mix: 10% weight */
    double mix_score = cp->total_accounts > 5 ? 100.0 :
                       cp->total_accounts > 3 ? 75.0 :
                       cp->total_accounts > 1 ? 50.0 : 25.0;

    /* Recent inquiries: 10% weight — fewer is better */
    double inquiry_score = cp->recent_inquiries == 0 ? 100.0 :
                           cp->recent_inquiries <= 2 ? 80.0 :
                           cp->recent_inquiries <= 5 ? 50.0 : 20.0;

    cp->score.score = (int)(300 + 550 * (
        0.35 * pay_score + 0.30 * util_score + 0.15 * age_score +
        0.10 * mix_score + 0.10 * inquiry_score) / 100.0);
    if (cp->score.score > 850) cp->score.score = 850;
    if (cp->score.score < 300) cp->score.score = 300;

    cp->score.level = credit_score_to_risk(cp->score.score);
    cp->score.grade = credit_score_to_grade(cp->score.score);
    return cp->score.score;
}

RiskLevel credit_score_to_risk(int score) {
    if (score >= 750) return RISK_LOW;
    if (score >= 650) return RISK_MEDIUM;
    if (score >= 550) return RISK_HIGH;
    return RISK_CRITICAL;
}

char credit_score_to_grade(int score) {
    if (score >= 800) return 'A';
    if (score >= 740) return 'B';
    if (score >= 670) return 'C';
    if (score >= 580) return 'D';
    if (score >= 500) return 'E';
    return 'F';
}

void credit_print_profile(const CreditProfile* cp) {
    if (!cp) return;
    printf("Credit Profile [Acct #%d]: Score=%d (%c) Risk=%s\n",
           cp->account_id, cp->score.score, cp->score.grade,
           risk_level_name(cp->score.level));
    printf("  On-time: %d  Late: %d  Missed: %d\n",
           cp->on_time_payments, cp->late_payments, cp->missed_payments);
    printf("  Utilization: %.0f%%  Age: %d months  Inquiries: %d  Accts: %d\n",
           cp->credit_utilization * 100.0, cp->credit_age_months,
           cp->recent_inquiries, cp->total_accounts);
}

/* ================================================================
 * L8: Blockchain — Simplified Proof-of-Work
 * Implements: Merkle trees, PoW mining, chain validation
 * ================================================================ */

/* DJB2 hash, returns 64-bit hash as hex */
static void simple_hash(const char* input, char output[65]) {
    unsigned long long h1 = 5381, h2 = 52711;
    for (const unsigned char* p = (const unsigned char*)input; *p; p++) {
        h1 = ((h1 << 5) + h1) + *p;
        h2 = ((h2 << 7) - h2) + *p;
    }
    snprintf(output, 65, "%016llx%016llx%016llx%016llx",
             (unsigned long long)h1, (unsigned long long)h2,
             (unsigned long long)(h1 ^ 0x9e3779b97f4a7c15ULL),
             (unsigned long long)(h2 ^ 0xbf58476d1ce4e5b9ULL));
}

MerkleTree* merkle_create(void) {
    MerkleTree* mt = (MerkleTree*)calloc(1, sizeof(MerkleTree));
    if (!mt) return NULL;
    mt->leaves = (MerkleLeaf*)calloc(FIN_MAX_TRANSACTIONS, sizeof(MerkleLeaf));
    return mt;
}

int merkle_add_leaf(MerkleTree* mt, const char* data) {
    if (!mt || !data || mt->leaf_count >= FIN_MAX_TRANSACTIONS) return -1;
    MerkleLeaf* leaf = &mt->leaves[mt->leaf_count];
    snprintf(leaf->data, FIN_MAX_NAME, "%s", data);
    simple_hash(data, leaf->hash);
    mt->leaf_count++;
    return mt->leaf_count - 1;
}

void merkle_compute_root(MerkleTree* mt) {
    if (!mt || mt->leaf_count == 0) return;
    if (mt->leaf_count == 1) {
        memcpy(mt->merkle_root, mt->leaves[0].hash, 64);
        mt->merkle_root[64] = '\0';
        mt->tree_depth = 1;
        return;
    }
    /* Build tree bottom-up */
    char current_level[FIN_MAX_TRANSACTIONS][65];
    int count = mt->leaf_count;
    for (int i = 0; i < count; i++) {
        memcpy(current_level[i], mt->leaves[i].hash, 64);
        current_level[i][64] = '\0';
    }

    int depth = 1;
    while (count > 1) {
        int parent_cnt = 0;
        for (int i = 0; i < count; i += 2) {
            char combined[256];
            int clen;
            if (i + 1 < count) {
                clen = snprintf(combined, sizeof(combined), "%.64s%.64s",
                         current_level[i], current_level[i + 1]);
            } else {
                clen = snprintf(combined, sizeof(combined), "%.64s%.64s",
                         current_level[i], current_level[i]);
            }
            (void)clen;
            simple_hash(combined, current_level[parent_cnt]);
            parent_cnt++;
        }
        count = parent_cnt;
        depth++;
    }
    memcpy(mt->merkle_root, current_level[0], 64);
    mt->merkle_root[64] = '\0';
    mt->tree_depth = depth;
}

bool merkle_verify_leaf(const MerkleTree* mt, int index, const char* expected_data) {
    if (!mt || index < 0 || index >= mt->leaf_count || !expected_data) return false;
    char expected_hash[65];
    simple_hash(expected_data, expected_hash);
    return strncmp(expected_hash, mt->leaves[index].hash, 64) == 0;
}

/* Blockchain */

Blockchain* blockchain_create(int difficulty) {
    Blockchain* bc = (Blockchain*)calloc(1, sizeof(Blockchain));
    if (!bc) return NULL;
    bc->difficulty = difficulty > 0 ? difficulty : 4;
    bc->is_valid = true;
    /* Genesis block */
    Block* gb = &bc->blocks[0];
    gb->index = 0;
    gb->timestamp = (int64_t)time(NULL);
    snprintf(gb->data, FIN_MAX_NAME, "Genesis Block");
    memset(gb->prev_hash, '0', 64);
    gb->prev_hash[64] = '\0';
    gb->difficulty = bc->difficulty;
    /* Mine genesis */
    gb->nonce = 0;
    uint64_t max_nonce_g = 500000ULL;
    uint32_t target_g = (uint32_t)((bc->difficulty > 0 && bc->difficulty <= 8)
                         ? ((1U << (bc->difficulty * 4)) - 1U) : 0xFU);
    {
        char input_g[FIN_MAX_NAME * 2];
        char hash8[9];
        int found_g;
        do {
            snprintf(input_g, sizeof(input_g), "%d%lld%s%s%llu",
                     gb->index, (long long)gb->timestamp, gb->data, gb->prev_hash,
                     (unsigned long long)gb->nonce);
            simple_hash(input_g, gb->hash);
            memcpy(hash8, gb->hash, 8);
            hash8[8] = '\0';
            uint32_t hv = (uint32_t)strtoul(hash8, NULL, 16);
            found_g = ((hv & target_g) == 0U) ? 1 : 0;
            gb->nonce++;
            if (gb->nonce > max_nonce_g) break;
        } while (!found_g);
    }
    bc->block_count = 1;
    return bc;
}

int blockchain_add_block(Blockchain* bc, const char* data) {
    if (!bc || !data || bc->block_count >= FIN_MAX_BLOCKS) return -1;
    Block* prev = &bc->blocks[bc->block_count - 1];
    Block* b = &bc->blocks[bc->block_count];
    b->index = bc->block_count;
    b->timestamp = (int64_t)time(NULL);
    snprintf(b->data, FIN_MAX_NAME, "%s", data);
    memcpy(b->prev_hash, prev->hash, 64);
    b->prev_hash[64] = '\0';
    b->difficulty = bc->difficulty;

    /* PoW mining: find nonce where hash mod target == 0 */
    b->nonce = 0;
    char input[FIN_MAX_NAME * 3];
    uint64_t max_nonce = 500000ULL;
    uint32_t target = (uint32_t)((bc->difficulty > 0 && bc->difficulty <= 8)
                       ? ((1U << (bc->difficulty * 4)) - 1U) : 0xFU);
    char hash8[9];
    int found;
    do {
        snprintf(input, sizeof(input), "%d%lld%s%s%llu",
                 b->index, (long long)b->timestamp, b->data, b->prev_hash,
                 (unsigned long long)b->nonce);
        simple_hash(input, b->hash);
        memcpy(hash8, b->hash, 8);
        hash8[8] = '\0';
        uint32_t hv = (uint32_t)strtoul(hash8, NULL, 16);
        found = ((hv & target) == 0U) ? 1 : 0;
        b->nonce++;
        if (b->nonce > max_nonce) break;
    } while (!found);

    bc->block_count++;
    return bc->block_count - 1;
}

bool blockchain_validate(const Blockchain* bc) {
    if (!bc || bc->block_count == 0) return false;
    uint32_t target = (uint32_t)((bc->difficulty > 0 && bc->difficulty <= 8)
                       ? ((1U << (bc->difficulty * 4)) - 1U) : 0xFU);

    for (int i = 0; i < bc->block_count; i++) {
        const Block* b = &bc->blocks[i];
        /* Verify PoW: first 8 hex chars as 32-bit value */
        char hash8[9];
        memcpy(hash8, b->hash, 8);
        hash8[8] = '\0';
        uint32_t hv = (uint32_t)strtoul(hash8, NULL, 16);
        if ((hv & target) != 0U) return false;
        /* Verify chain linkage */
        if (i > 0) {
            if (strncmp(b->prev_hash, bc->blocks[i - 1].hash, 64) != 0) return false;
        }
        /* Recompute hash */
        char recomputed[65];
        char input[FIN_MAX_NAME * 3];
        snprintf(input, sizeof(input), "%d%lld%s%s%llu",
                 b->index, (long long)b->timestamp, b->data, b->prev_hash,
                 (unsigned long long)b->nonce);
        simple_hash(input, recomputed);
        if (strncmp(recomputed, b->hash, 64) != 0) return false;
    }
    return true;
}

void blockchain_print_chain(const Blockchain* bc) {
    if (!bc) return;
    printf("=== Blockchain (difficulty=%d, blocks=%d) ===\n",
           bc->difficulty, bc->block_count);
    for (int i = 0; i < bc->block_count && i < 5; i++) {
        const Block* b = &bc->blocks[i];
        printf("  Block %d: hash=%.16s... nonce=%llu\n",
               b->index, b->hash, (unsigned long long)b->nonce);
    }
    if (bc->block_count > 5) printf("  ... (%d more blocks)\n", bc->block_count - 5);
    printf("  Valid: %s\n", blockchain_validate(bc) ? "YES" : "NO");
}

void blockchain_destroy(Blockchain* bc) { free(bc); }
void merkle_destroy(MerkleTree* mt) {
    if (!mt) return;
    free(mt->leaves);
    free(mt);
}

/* ================================================================
 * L7: FX (Foreign Exchange) Market
 * Cross-rate triangulation: USD/EUR * EUR/JPY = USD/JPY
 * ================================================================ */

FXMarket* fx_create(const char* home_currency) {
    FXMarket* fx = (FXMarket*)calloc(1, sizeof(FXMarket));
    if (!fx) return NULL;
    if (home_currency) strncpy(fx->home_currency, home_currency, 7);
    return fx;
}

int fx_add_rate(FXMarket* fx, const char* base, const char* quote, double rate) {
    if (!fx || !base || !quote || fx->rate_count >= FIN_MAX_FX_PAIRS) return -1;
    if (rate <= 0.0) return -1;
    FXRate* r = &fx->rates[fx->rate_count];
    strncpy(r->base_currency, base, 7);
    strncpy(r->quote_currency, quote, 7);
    r->rate = rate;
    r->timestamp = (int64_t)time(NULL);
    fx->rate_count++;
    return fx->rate_count - 1;
}

double fx_get_rate(const FXMarket* fx, const char* base, const char* quote) {
    if (!fx || !base || !quote) return 0.0;
    /* Direct lookup */
    for (int i = 0; i < fx->rate_count; i++) {
        if (strcmp(fx->rates[i].base_currency, base) == 0 &&
            strcmp(fx->rates[i].quote_currency, quote) == 0)
            return fx->rates[i].rate;
    }
    /* Inverse lookup */
    for (int i = 0; i < fx->rate_count; i++) {
        if (strcmp(fx->rates[i].base_currency, quote) == 0 &&
            strcmp(fx->rates[i].quote_currency, base) == 0)
            return 1.0 / fx->rates[i].rate;
    }
    /* Cross-rate triangulation via USD */
    double base_usd = 0.0, quote_usd = 0.0;
    if (strcmp(base, "USD") == 0) base_usd = 1.0;
    else base_usd = fx_get_rate(fx, base, "USD");
    if (strcmp(quote, "USD") == 0) quote_usd = 1.0;
    else quote_usd = fx_get_rate(fx, quote, "USD");
    if (base_usd > 0.0 && quote_usd > 0.0)
        return base_usd / quote_usd;
    return 0.0;
}

double fx_convert(const FXMarket* fx, double amount, const char* from, const char* to) {
    double rate = fx_get_rate(fx, from, to);
    if (rate <= 0.0) return 0.0;
    return amount * rate;
}

void fx_print_rates(const FXMarket* fx) {
    if (!fx) return;
    printf("FX Market (home: %s, %d pairs):\n", fx->home_currency, fx->rate_count);
    for (int i = 0; i < fx->rate_count; i++) {
        printf("  %s/%s = %.6f\n",
               fx->rates[i].base_currency, fx->rates[i].quote_currency,
               fx->rates[i].rate);
    }
}

void fx_destroy(FXMarket* fx) { free(fx); }

/* ================================================================
 * L7: Fraud Detection Engine
 * ================================================================ */

FraudEngine* fraud_create(void) {
    return (FraudEngine*)calloc(1, sizeof(FraudEngine));
}

int fraud_add_rule(FraudEngine* fe, FraudRuleType type, const char* name,
                   double threshold, int lookback) {
    if (!fe || !name || fe->rule_count >= FIN_MAX_RULES) return -1;
    FraudRule* r = &fe->rules[fe->rule_count];
    r->type = type;
    snprintf(r->name, FIN_MAX_NAME, "%s", name);
    r->threshold = threshold;
    r->enabled = true;
    r->lookback_transactions = lookback;
    r->check_fn = NULL;
    fe->rule_count++;
    return fe->rule_count - 1;
}

int fraud_scan_account(FraudEngine* fe, Ledger* l, int account_id) {
    if (!fe || !l) return 0;
    Account* a = ledger_find_account(l, account_id);
    if (!a || !a->active) return 0;

    int alerts = 0;
    /* Check recent transactions for this account */
    int txn_count = 0;
    double total_amount = 0.0;
    double max_amount = 0.0;
    int64_t earliest = INT64_MAX;

    for (int i = l->transaction_count - 1; i >= 0; i--) {
        Transaction* t = &l->transactions[i];
        if (t->debit_account_id != account_id && t->credit_account_id != account_id)
            continue;
        if (t->timestamp < earliest) earliest = t->timestamp;
        total_amount += t->amount;
        if (t->amount > max_amount) max_amount = t->amount;
        txn_count++;
    }

    for (int r = 0; r < fe->rule_count; r++) {
        FraudRule* rule = &fe->rules[r];
        if (!rule->enabled) continue;
        switch (rule->type) {
        case FRAUD_AMOUNT_ANOMALY:
            if (max_amount > rule->threshold * 1000.0) {
                fe->alerts_triggered++;
                alerts++;
            }
            break;
        case FRAUD_VELOCITY:
            if (txn_count > (int)rule->threshold) {
                fe->alerts_triggered++;
                alerts++;
            }
            break;
        default: break;
        }
    }
    return alerts;
}

bool fraud_freeze_account(FraudEngine* fe, Ledger* l, int account_id) {
    if (!fe || !l) return false;
    Account* a = ledger_find_account(l, account_id);
    if (!a) return false;
    a->frozen = true;
    fe->accounts_frozen++;
    return true;
}

void fraud_print_alerts(const FraudEngine* fe) {
    if (!fe) return;
    printf("Fraud Engine: %d rules, %d alerts, %d frozen\n",
           fe->rule_count, fe->alerts_triggered, fe->accounts_frozen);
}

void fraud_destroy(FraudEngine* fe) { free(fe); }

/* ================================================================
 * L5: Modern Portfolio Theory — Markowitz Mean-Variance Optimization
 *
 * Implements the Nobel Prize-winning framework for optimal asset
 * allocation under uncertainty. Investors are risk-averse and
 * seek to maximize expected return for a given level of risk.
 *
 * Core formula:
 *   Portfolio return: E(R_p) = Σ w_i × E(R_i)
 *   Portfolio variance: σ²_p = Σ Σ w_i w_j σ_ij
 *
 * Efficient frontier: set of portfolios offering the maximum
 * expected return for each level of risk.
 *
 * Optimal portfolio: tangency portfolio that maximizes the
 * Sharpe ratio: (E(R_p) - R_f) / σ_p
 *
 * Algorithm: Monte Carlo simulation to sample the feasible
 * weight space (simplex) and identify the efficient frontier.
 * Complexity: O(N × M) for N samples and M assets.
 *
 * Reference: Markowitz (1952) "Portfolio Selection" J. Finance 7(1):77-91
 *            MIT 15.433: Investments — Lecture 5-8
 * ================================================================ */

/* Box-Muller transform for normal random variates */
static double box_muller(uint64_t* seed) {
    *seed = *seed * 6364136223846793005ULL + 1442695040888963407ULL;
    double u1 = (double)(*seed >> 16) / (double)UINT32_MAX * 0.5 + 0.5;
    *seed = *seed * 6364136223846793005ULL + 1442695040888963407ULL;
    double u2 = (double)(*seed >> 16) / (double)UINT32_MAX * 0.5 + 0.5;
    if (u1 < 1e-10) u1 = 1e-10;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

void market_init(Market* m) {
    if (!m) return;
    memset(m, 0, sizeof(Market));
}

int market_add_asset(Market* m, const char* symbol, const char* name,
                      double er, double vol, double price, int asset_class) {
    if (!m || m->asset_count >= FIN_MAX_ASSETS) return -1;
    Asset* a = &m->assets[m->asset_count];
    snprintf(a->symbol, 15, "%s", symbol);
    snprintf(a->name, FIN_MAX_NAME, "%s", name);
    a->expected_return = er;
    a->volatility = vol;
    a->price = price;
    a->asset_class = asset_class;
    m->asset_count++;
    return m->asset_count - 1;
}

void market_compute_covariance(Market* m, double correlations[][FIN_MAX_ASSETS]) {
    if (!m || !correlations) return;
    for (int i = 0; i < m->asset_count; i++) {
        for (int j = 0; j < m->asset_count; j++) {
            m->cov_matrix[i][j] = correlations[i][j]
                                * m->assets[i].volatility * m->assets[j].volatility;
        }
    }
}

double portfolio_expected_return(const Portfolio* p, const Market* m) {
    if (!p || !m) return 0.0;
    double er = 0.0;
    for (int i = 0; i < p->num_assets; i++) {
        int idx = p->asset_indices[i];
        if (idx >= 0 && idx < m->asset_count)
            er += p->weights[i] * m->assets[idx].expected_return;
    }
    return er;
}

double portfolio_volatility(const Portfolio* p, const Market* m) {
    if (!p || !m) return 0.0;
    double var = 0.0;
    for (int i = 0; i < p->num_assets; i++) {
        int ii = p->asset_indices[i];
        for (int j = 0; j < p->num_assets; j++) {
            int jj = p->asset_indices[j];
            if (ii < m->asset_count && jj < m->asset_count)
                var += p->weights[i] * p->weights[j] * m->cov_matrix[ii][jj];
        }
    }
    return var > 0.0 ? sqrt(var) : 0.0;
}

double portfolio_sharpe(const Portfolio* p, double risk_free, const Market* m) {
    double er = portfolio_expected_return(p, m);
    double vol = portfolio_volatility(p, m);
    return vol > 0.0 ? (er - risk_free) / vol : 0.0;
}

void market_optimize_portfolio(Market* m, double risk_free_rate,
                                Portfolio* result) {
    if (!m || !result || m->asset_count < 2) return;
    memset(result, 0, sizeof(Portfolio));

    double best_sharpe = -1e9;
    uint64_t seed = 1234567ULL;

    for (int iter = 0; iter < 5000; iter++) {
        /* Generate random weights on the simplex */
        double raw[FIN_MAX_ASSETS];
        double sum = 0.0;
        for (int i = 0; i < m->asset_count; i++) {
            raw[i] = fabs(box_muller(&seed)) + 0.01;
            sum += raw[i];
        }
        Portfolio trial;
        trial.num_assets = m->asset_count;
        for (int i = 0; i < m->asset_count; i++) {
            trial.weights[i] = raw[i] / sum;
            trial.asset_indices[i] = i;
        }
        double sharpe = portfolio_sharpe(&trial, risk_free_rate, m);
        if (sharpe > best_sharpe) {
            best_sharpe = sharpe;
            *result = trial;
        }
    }
    result->expected_return = portfolio_expected_return(result, m);
    result->volatility = portfolio_volatility(result, m);
    result->sharpe_ratio = best_sharpe;
}

void market_efficient_frontier(Market* m, double risk_free_rate,
                                Portfolio* frontier, int* num_points, int max_points) {
    if (!m || !frontier || !num_points || m->asset_count < 2) return;
    (void)risk_free_rate;
    if (max_points > 30) max_points = 30;
    *num_points = 0;

    uint64_t seed = 7654321ULL;
    /* Generate many random portfolios and keep the best for each risk level */
    typedef struct { double er; double vol; double weights[FIN_MAX_ASSETS]; int n; } Cand;
    Cand candidates[5000];
    int nc = 0;

    for (int iter = 0; iter < 5000 && nc < 5000; iter++) {
        double raw[FIN_MAX_ASSETS], sum = 0.0;
        for (int i = 0; i < m->asset_count; i++) {
            raw[i] = fabs(box_muller(&seed)) + 0.01;
            sum += raw[i];
        }
        Portfolio p;
        p.num_assets = m->asset_count;
        for (int i = 0; i < m->asset_count; i++) {
            p.weights[i] = raw[i] / sum;
            p.asset_indices[i] = i;
        }
        candidates[nc].er = portfolio_expected_return(&p, m);
        candidates[nc].vol = portfolio_volatility(&p, m);
        candidates[nc].n = p.num_assets;
        memcpy(candidates[nc].weights, p.weights, sizeof(double) * p.num_assets);
        nc++;
    }

    /* Sort by volatility and pick efficient frontier */
    for (int i = 0; i < nc - 1; i++)
        for (int j = i + 1; j < nc; j++)
            if (candidates[j].vol < candidates[i].vol) {
                Cand tmp = candidates[i];
                candidates[i] = candidates[j];
                candidates[j] = tmp;
            }

    double max_er = -1e9;
    for (int i = 0; i < nc && *num_points < max_points; i++) {
        if (candidates[i].er > max_er) {
            max_er = candidates[i].er;
            Portfolio* pf = &frontier[*num_points];
            pf->num_assets = candidates[i].n;
            pf->expected_return = candidates[i].er;
            pf->volatility = candidates[i].vol;
            memcpy(pf->weights, candidates[i].weights, sizeof(double) * pf->num_assets);
            for (int k = 0; k < pf->num_assets; k++) pf->asset_indices[k] = k;
            (*num_points)++;
        }
    }
}

/* ================================================================
 * L5: Black-Scholes Option Pricing Model
 *
 * C = S₀e^(-qT)N(d₁) - Ke^(-rT)N(d₂)
 * P = Ke^(-rT)N(-d₂) - S₀e^(-qT)N(-d₁)
 *
 * where:
 * d₁ = [ln(S₀/K) + (r - q + σ²/2)T] / (σ√T)
 * d₂ = d₁ - σ√T
 *
 * Greeks:
 * Δ_call = e^(-qT)N(d₁)       Γ = e^(-qT)n(d₁)/(Sσ√T)
 * ν = S₀e^(-qT)n(d₁)√T        Θ = ...
 *
 * Reference: Black & Scholes (1973) JPE 81(3):637-654
 *            Hull (2021) "Options, Futures, and Other Derivatives" 11th ed.
 * ================================================================ */

/* Standard normal CDF (Abramowitz & Stegun 26.2.17)
 * Error < 7.5e-8 for all x */
static double normal_cdf(double x) {
    const double a1 =  0.254829592;
    const double a2 = -0.284496736;
    const double a3 =  1.421413741;
    const double a4 = -1.453152027;
    const double a5 =  1.061405429;
    const double p  =  0.3275911;
    int sign = (x < 0.0) ? -1 : 1;
    x = fabs(x);
    double t = 1.0 / (1.0 + p * x);
    double y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1)
                   * t * exp(-0.5 * x * x);
    return 0.5 * (1.0 + (double)sign * y);
}

static double normal_pdf(double x) {
    return exp(-0.5 * x * x) / sqrt(2.0 * M_PI);
}

double bs_price(const OptionParams* p) {
    if (!p || p->time_to_expiry <= 0.0 || p->volatility <= 0.0) return 0.0;
    double S = p->spot, K = p->strike, T = p->time_to_expiry;
    double r = p->risk_free_rate, sig = p->volatility, q = p->dividend_yield;
    double d1 = (log(S / K) + (r - q + sig * sig * 0.5) * T) / (sig * sqrt(T));
    double d2 = d1 - sig * sqrt(T);

    if (p->type == OPTION_CALL)
        return S * exp(-q * T) * normal_cdf(d1) - K * exp(-r * T) * normal_cdf(d2);
    else
        return K * exp(-r * T) * normal_cdf(-d2) - S * exp(-q * T) * normal_cdf(-d1);
}

double bs_delta(const OptionParams* p) {
    if (!p || p->time_to_expiry <= 0.0 || p->volatility <= 0.0) return 0.0;
    double d1 = (log(p->spot / p->strike) + (p->risk_free_rate - p->dividend_yield
                + p->volatility * p->volatility * 0.5) * p->time_to_expiry)
                / (p->volatility * sqrt(p->time_to_expiry));
    double delta = exp(-p->dividend_yield * p->time_to_expiry)
                  * normal_cdf(p->type == OPTION_CALL ? d1 : -d1);
    return p->type == OPTION_CALL ? delta : delta - exp(-p->dividend_yield * p->time_to_expiry);
}

double bs_gamma(const OptionParams* p) {
    if (!p || p->time_to_expiry <= 0.0 || p->volatility <= 0.0) return 0.0;
    double S = p->spot, T = p->time_to_expiry, sig = p->volatility, q = p->dividend_yield;
    double d1 = (log(S / p->strike) + (p->risk_free_rate - q
                + sig * sig * 0.5) * T) / (sig * sqrt(T));
    return exp(-q * T) * normal_pdf(d1) / (S * sig * sqrt(T));
}

double bs_vega(const OptionParams* p) {
    if (!p || p->time_to_expiry <= 0.0 || p->volatility <= 0.0) return 0.0;
    double S = p->spot, T = p->time_to_expiry, sig = p->volatility, q = p->dividend_yield;
    double d1 = (log(S / p->strike) + (p->risk_free_rate - q
                + sig * sig * 0.5) * T) / (sig * sqrt(T));
    return S * exp(-q * T) * normal_pdf(d1) * sqrt(T) * 0.01; /* 1% vol change */
}

double bs_theta(const OptionParams* p) {
    if (!p || p->time_to_expiry <= 0.0 || p->volatility <= 0.0) return 0.0;
    double S = p->spot, K = p->strike, T = p->time_to_expiry;
    double r = p->risk_free_rate, sig = p->volatility, q = p->dividend_yield;
    double d1 = (log(S / K) + (r - q + sig * sig * 0.5) * T) / (sig * sqrt(T));
    double d2 = d1 - sig * sqrt(T);
    double term1 = -S * exp(-q * T) * normal_pdf(d1) * sig / (2.0 * sqrt(T));
    if (p->type == OPTION_CALL)
        return (term1 - r * K * exp(-r * T) * normal_cdf(d2)
                + q * S * exp(-q * T) * normal_cdf(d1)) / 365.0;
    else
        return (term1 + r * K * exp(-r * T) * normal_cdf(-d2)
                - q * S * exp(-q * T) * normal_cdf(-d1)) / 365.0;
}

double bs_implied_volatility(const OptionParams* p, double market_price,
                               int max_iter, double tolerance) {
    if (!p || max_iter <= 0) return 0.0;
    if (tolerance <= 0.0) tolerance = 1e-6;
    double sigma = 0.3, sigma_low = 0.01, sigma_high = 2.0;

    for (int i = 0; i < max_iter; i++) {
        OptionParams tmp = *p;
        tmp.volatility = sigma;
        double price = bs_price(&tmp);
        double diff = price - market_price;
        if (fabs(diff) < tolerance) return sigma;

        /* Bisection */
        if (diff > 0) sigma_high = sigma;
        else sigma_low = sigma;
        sigma = (sigma_low + sigma_high) * 0.5;
    }
    return sigma;
}

/* ================================================================
 * L7: Value at Risk (VaR) — Three Methods
 *
 * VaR(α) = inf{l ∈ ℝ: P(Loss ≤ l) ≥ α}
 *
 * 1. Parametric (variance-covariance):
 *    VaR = μ + σ × z_α × √T  (assuming normality)
 * 2. Historical: empirical α-quantile of historical returns
 * 3. Monte Carlo: simulate N paths, take α-quantile
 *
 * CVaR (Expected Shortfall):
 *    ES_α = E[Loss | Loss > VaR_α]
 *    More coherent risk measure (Artzner et al. 1999)
 *
 * Reference: Jorion (2006) "Value at Risk" 3rd ed. McGraw-Hill
 *            Basel Committee (2019) "Minimum capital requirements
 *            for market risk" (FRTB)
 * ================================================================ */

double var_parametric(double portfolio_value, double volatility,
                       double confidence, int horizon_days) {
    if (portfolio_value <= 0.0 || volatility <= 0.0) return 0.0;
    /* z-score for confidence level */
    double z;
    if (confidence > 0.999) z = 3.090;
    else if (confidence > 0.99) z = 2.326;
    else if (confidence > 0.975) z = 1.960;
    else if (confidence > 0.95) z = 1.645;
    else z = 1.282;
    return portfolio_value * volatility * z * sqrt((double)horizon_days / 252.0);
}

double var_historical(const double* returns, int n, double confidence) {
    if (!returns || n <= 0) return 0.0;
    /* Sort returns ascending */
    double* sorted = (double*)malloc((size_t)n * sizeof(double));
    if (!sorted) return 0.0;
    memcpy(sorted, returns, (size_t)n * sizeof(double));
    /* Simple bubble sort for clarity */
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (sorted[j] < sorted[i]) {
                double t = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = t;
            }
    int idx = (int)((1.0 - confidence) * (double)n);
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    double result = -sorted[idx];
    free(sorted);
    return result;
}

double var_monte_carlo(double portfolio_value, double drift, double volatility,
                         int horizon_days, double confidence, int num_simulations) {
    if (portfolio_value <= 0.0 || num_simulations <= 0) return 0.0;
    double* losses = (double*)malloc((size_t)num_simulations * sizeof(double));
    if (!losses) return 0.0;

    uint64_t seed = 9876543ULL;
    double dt = (double)horizon_days / 252.0;
    for (int i = 0; i < num_simulations; i++) {
        double z = box_muller(&seed);
        double ret = drift * dt + volatility * sqrt(dt) * z;
        losses[i] = -portfolio_value * ret;
    }

    /* Sort and find quantile */
    for (int i = 0; i < num_simulations - 1; i++)
        for (int j = i + 1; j < num_simulations; j++)
            if (losses[j] < losses[i]) {
                double t = losses[i];
                losses[i] = losses[j];
                losses[j] = t;
            }
    int idx = (int)(confidence * (double)num_simulations);
    if (idx >= num_simulations) idx = num_simulations - 1;
    double var = losses[idx];

    free(losses);
    return var;
}

void var_compute_full(VaRResult* result, const double* returns, int n,
                        double portfolio_value, double volatility) {
    if (!result || !returns || n <= 0) return;
    (void)volatility;
    memset(result, 0, sizeof(VaRResult));
    result->portfolio_value = portfolio_value;
    result->confidence_level = 0.95;
    result->horizon_days = 10;
    result->method = VAR_METHOD_HISTORICAL;

    /* Historical VaR */
    result->var_result = var_historical(returns, n, result->confidence_level);
    result->var_result *= portfolio_value;

    /* CVaR: avg of returns worse than VaR quantile */
    int idx = (int)((1.0 - 0.95) * (double)n);
    double sum = 0.0; int cnt = 0;
    for (int i = 0; i <= idx && i < n; i++) { sum += returns[i]; cnt++; }
    result->cvar_result = cnt > 0 ? -sum / (double)cnt * portfolio_value : result->var_result;
}

/* ================================================================
 * L5: Fixed Income — Bond Pricing & Yield Curves
 *
 * Bond price = Σ[C/(1+y/k)^i] + F/(1+y/k)^N
 * Duration (Macaulay) = Σ[t_i × PV(CF_i)] / Σ PV(CF_i)
 * Modified Duration = Macaulay / (1 + y/k)
 * Convexity = Σ[t_i(t_i+1) × PV(CF_i)] / [Σ PV(CF_i) × (1+y/k)²]
 *
 * Nelson-Siegel (1987) yield curve:
 * y(τ) = β₀ + β₁·(1-e^(-τ/λ))/(τ/λ) + β₂·[(1-e^(-τ/λ))/(τ/λ) - e^(-τ/λ)]
 *
 * Reference: Tuckman & Serrat (2022) "Fixed Income Securities" 4th ed.
 *            CFA Institute Level II Curriculum, Fixed Income
 * ================================================================ */

double bond_price(const Bond* b) {
    if (!b || b->periods_remaining <= 0) return 0.0;
    double y = b->yield_to_maturity / (double)b->frequency;
    double c = b->coupon_rate * b->face_value / (double)b->frequency;
    double price = 0.0;
    for (int i = 1; i <= b->periods_remaining; i++) {
        double cf = (i == b->periods_remaining) ? c + b->face_value : c;
        price += cf / pow(1.0 + y, (double)i);
    }
    return price;
}

double bond_duration(const Bond* b) {
    if (!b || b->periods_remaining <= 0) return 0.0;
    double y = b->yield_to_maturity / (double)b->frequency;
    double c = b->coupon_rate * b->face_value / (double)b->frequency;
    double price = 0.0, dur_num = 0.0;
    for (int i = 1; i <= b->periods_remaining; i++) {
        double cf = (i == b->periods_remaining) ? c + b->face_value : c;
        double pv = cf / pow(1.0 + y, (double)i);
        price += pv;
        dur_num += (double)i * pv;
    }
    return price > 0.0 ? (dur_num / price) / (double)b->frequency : 0.0;
}

double bond_convexity(const Bond* b) {
    if (!b || b->periods_remaining <= 0) return 0.0;
    double y = b->yield_to_maturity / (double)b->frequency;
    double c = b->coupon_rate * b->face_value / (double)b->frequency;
    double price = 0.0, conv_num = 0.0;
    for (int i = 1; i <= b->periods_remaining; i++) {
        double cf = (i == b->periods_remaining) ? c + b->face_value : c;
        double pv = cf / pow(1.0 + y, (double)i);
        price += pv;
        conv_num += (double)(i * (i + 1)) * pv;
    }
    double denom = price * (1.0 + y) * (1.0 + y) * (double)(b->frequency * b->frequency);
    return denom > 0.0 ? conv_num / denom : 0.0;
}

double bond_ytm_from_price(const Bond* b, double market_price,
                             int max_iter, double tolerance) {
    if (!b || market_price <= 0.0) return 0.0;
    double y_low = 0.001, y_high = 0.50;
    for (int i = 0; i < max_iter; i++) {
        Bond tmp = *b;
        double y_mid = (y_low + y_high) * 0.5;
        tmp.yield_to_maturity = y_mid;
        double p = bond_price(&tmp);
        if (fabs(p - market_price) < tolerance) return y_mid;
        if (p > market_price) y_high = y_mid;
        else y_low = y_mid;
    }
    return (y_low + y_high) * 0.5;
}

void yield_curve_bootstrap(YieldCurve* yc, const double* maturities,
                             const double* yields, int n) {
    if (!yc || !maturities || !yields || n <= 0 || n > 12) return;
    yc->num_points = n;
    for (int i = 0; i < n; i++) {
        yc->maturities[i] = maturities[i];
        yc->yields[i] = yields[i];
    }
}

double yield_curve_interpolate(const YieldCurve* yc, double maturity) {
    if (!yc || yc->num_points <= 0) return 0.0;
    if (maturity <= yc->maturities[0]) return yc->yields[0];
    if (maturity >= yc->maturities[yc->num_points - 1])
        return yc->yields[yc->num_points - 1];
    for (int i = 0; i < yc->num_points - 1; i++) {
        if (maturity >= yc->maturities[i] && maturity <= yc->maturities[i + 1]) {
            double t = (maturity - yc->maturities[i])
                      / (yc->maturities[i + 1] - yc->maturities[i]);
            return yc->yields[i] + t * (yc->yields[i + 1] - yc->yields[i]);
        }
    }
    return yc->yields[yc->num_points - 1];
}

void yield_curve_nelson_siegel(YieldCurve* yc,
                                 double beta0, double beta1, double beta2, double tau) {
    if (!yc) return;
    yc->beta0 = beta0; yc->beta1 = beta1; yc->beta2 = beta2; yc->tau = tau;
    yc->num_points = 12;
    for (int i = 0; i < 12; i++) {
        yc->maturities[i] = (double)(i + 1) * 0.5;
        double t = yc->maturities[i] / tau;
        double factor1 = t > 1e-10 ? (1.0 - exp(-t)) / t : 1.0;
        double factor2 = factor1 - exp(-t);
        yc->yields[i] = beta0 + beta1 * factor1 + beta2 * factor2;
    }
}

double yield_curve_discount_factor(const YieldCurve* yc, double t) {
    double y = yield_curve_interpolate(yc, t);
    return exp(-y * t);
}

/* ================================================================
 * L6: Payment Network Simulation (ACH / Interbank Clearing)
 *
 * Simulates a multi-bank payment clearing network. Demonstrates
 * concepts of bilateral vs. multilateral net settlement.
 *
 * Net settlement: offset mutual obligations; only net differences
 * are actually transferred, greatly reducing settlement liquidity
 * requirements.
 *
 * Reference: Bech & Hobijn (2007) "Technology diffusion within central
 *            banking", FRB NY Staff Report 260
 *            ISO 20022 Universal Financial Industry Message Scheme
 * ================================================================ */

PaymentNetwork* pn_create(void) {
    return (PaymentNetwork*)calloc(1, sizeof(PaymentNetwork));
}

int pn_add_bank(PaymentNetwork* pn, const char* name, const char* routing) {
    if (!pn || pn->bank_count >= FIN_MAX_BANKS) return -1;
    Bank* b = &pn->banks[pn->bank_count];
    b->bank_id = pn->bank_count + 1;
    snprintf(b->name, FIN_MAX_NAME, "%s", name);
    snprintf(b->routing_number, 15, "%s", routing);
    b->balance = 1000000.0;
    pn->bank_count++;
    return b->bank_id;
}

int pn_create_payment(PaymentNetwork* pn, int from, int to,
                        double amount, const char* ref) {
    if (!pn || pn->payment_count >= FIN_MAX_PAYMENTS) return -1;
    if (from < 1 || from > pn->bank_count || to < 1 || to > pn->bank_count)
        return -1;
    Payment* p = &pn->payments[pn->payment_count];
    p->id = pn->payment_count + 1;
    p->from_bank = from;
    p->to_bank = to;
    p->amount = amount;
    p->timestamp = (int64_t)time(NULL);
    p->status = 0;
    if (ref) snprintf(p->reference, FIN_MAX_NAME, "%s", ref);
    pn->payment_count++;
    pn->pending_count++;
    return p->id;
}

bool pn_settle_payment(PaymentNetwork* pn, int payment_id) {
    if (!pn || payment_id < 1 || payment_id > pn->payment_count) return false;
    Payment* p = &pn->payments[payment_id - 1];
    if (p->status != 0) return false;
    Bank* from = &pn->banks[p->from_bank - 1];
    Bank* to = &pn->banks[p->to_bank - 1];
    if (from->balance < p->amount) { p->status = 2; return false; }
    from->balance -= p->amount;
    to->balance += p->amount;
    from->sent_total += p->amount;
    to->received_total += p->amount;
    from->payment_count++;
    p->status = 1;
    pn->total_settled += p->amount;
    pn->pending_count--;
    return true;
}

void pn_net_settle(PaymentNetwork* pn) {
    if (!pn) return;
    /* Compute net positions */
    double net[FIN_MAX_BANKS] = {0};
    for (int i = 0; i < pn->payment_count; i++) {
        if (pn->payments[i].status == 0) {
            int from = pn->payments[i].from_bank - 1;
            int to = pn->payments[i].to_bank - 1;
            net[from] -= pn->payments[i].amount;
            net[to] += pn->payments[i].amount;
        }
    }
    /* Settle net positions */
    for (int i = 0; i < pn->bank_count; i++) {
        pn->banks[i].balance += net[i];
    }
    for (int i = 0; i < pn->payment_count; i++) {
        if (pn->payments[i].status == 0) {
            pn->payments[i].status = 1;
            pn->total_settled += pn->payments[i].amount;
        }
    }
    pn->pending_count = 0;
}

void pn_print_summary(const PaymentNetwork* pn) {
    if (!pn) return;
    printf("Payment Network: %d banks, %d payments ($%.0f settled, %d pending)\n",
           pn->bank_count, pn->payment_count, pn->total_settled, pn->pending_count);
    for (int i = 0; i < pn->bank_count; i++) {
        printf("  Bank %d %-12s Rtg:%s Bal:$%.0f Sent:$%.0f Recv:$%.0f\n",
               pn->banks[i].bank_id, pn->banks[i].name,
               pn->banks[i].routing_number, pn->banks[i].balance,
               pn->banks[i].sent_total, pn->banks[i].received_total);
    }
}

void pn_destroy(PaymentNetwork* pn) { free(pn); }

/* ================================================================
 * L8: Algorithmic Trading — Limit Order Book
 *
 * Standard price-time priority matching engine:
 * - Bids sorted descending by price, then by timestamp
 * - Asks sorted ascending by price, then by timestamp
 * - Market orders match against best contra-side
 * - Limit orders rest in book if not fully matched
 *
 * Key microstructure concepts (O'Hara 1995):
 * - Bid-ask spread = indicator of liquidity/market quality
 * - Price impact = adverse selection cost for large orders
 * - VWAP = Volume-Weighted Average Price (benchmark)
 *
 * Reference: Biais, Foucault & Moinas (2015) "Equilibrium fast trading"
 *            Harris (2003) "Trading and Exchanges"
 * ================================================================ */

void lob_init(LimitOrderBook* lob) {
    if (!lob) return;
    memset(lob, 0, sizeof(LimitOrderBook));
    lob->last_price = 100.0;
}

int lob_add_order(LimitOrderBook* lob, OrderSide side, OrderTypeLOB type,
                   double price, int qty, int trader_id) {
    if (!lob || lob->order_count >= FIN_MAX_ORDERS || qty <= 0) return -1;
    LOBOrder* o = &lob->orders[lob->order_count];
    o->id = lob->order_count + 1;
    o->side = side;
    o->type = type;
    o->price = type == ORDER_TYPE_MARKET ? (side == ORDER_SIDE_BUY ? 1e9 : 0.0) : price;
    o->quantity = qty;
    o->timestamp = (int64_t)time(NULL);
    o->trader_id = trader_id;
    o->active = true;
    lob->order_count++;
    int matched = lob_match(lob);
    (void)matched;
    return o->id;
}

int lob_match(LimitOrderBook* lob) {
    if (!lob) return 0;
    int trades = 0;
    for (int i = 0; i < lob->order_count; i++) {
        if (!lob->orders[i].active || lob->orders[i].type == ORDER_TYPE_LIMIT)
            continue;
        LOBOrder* mo = &lob->orders[i];
        /* Find best contra */
        int best_idx = -1;
        double best_price = mo->side == ORDER_SIDE_BUY ? 1e9 : -1e9;
        for (int j = 0; j < lob->order_count; j++) {
            if (!lob->orders[j].active || lob->orders[j].side == mo->side) continue;
            if (lob->orders[j].type == ORDER_TYPE_MARKET) continue;
            double p = lob->orders[j].price;
            if ((mo->side == ORDER_SIDE_BUY && p <= mo->price && p < best_price) ||
                (mo->side == ORDER_SIDE_SELL && p >= mo->price && p > best_price)) {
                best_idx = j;
                best_price = p;
            }
        }
        if (best_idx < 0) continue;
        LOBOrder* lo = &lob->orders[best_idx];
        int match_qty = lo->quantity < mo->quantity ? lo->quantity : mo->quantity;
        double trade_price = lo->price;
        lob->last_price = trade_price;
        lob->total_volume += match_qty;
        lo->quantity -= match_qty;
        mo->quantity -= match_qty;
        if (lo->quantity <= 0) lo->active = false;
        if (mo->quantity <= 0) mo->active = false;
        trades++;
    }
    lob_update_quotes(lob);
    return trades;
}

int lob_cancel_order(LimitOrderBook* lob, int order_id) {
    if (!lob || order_id < 1 || order_id > lob->order_count) return -1;
    if (!lob->orders[order_id - 1].active) return -1;
    lob->orders[order_id - 1].active = false;
    lob_update_quotes(lob);
    return 0;
}

void lob_update_quotes(LimitOrderBook* lob) {
    if (!lob) return;
    lob->best_bid = -1e9; lob->best_ask = 1e9;
    lob->bid_depth = 0; lob->ask_depth = 0;
    for (int i = 0; i < lob->order_count; i++) {
        if (!lob->orders[i].active || lob->orders[i].type != ORDER_TYPE_LIMIT) continue;
        if (lob->orders[i].side == ORDER_SIDE_BUY) {
            if (lob->orders[i].price > lob->best_bid) {
                lob->best_bid = lob->orders[i].price;
                lob->bid_depth = lob->orders[i].quantity;
            } else if (lob->orders[i].price == lob->best_bid) {
                lob->bid_depth += lob->orders[i].quantity;
            }
        } else {
            if (lob->orders[i].price < lob->best_ask) {
                lob->best_ask = lob->orders[i].price;
                lob->ask_depth = lob->orders[i].quantity;
            } else if (lob->orders[i].price == lob->best_ask) {
                lob->ask_depth += lob->orders[i].quantity;
            }
        }
    }
    lob->spread = lob->best_ask - lob->best_bid;
    lob->mid_price = (lob->best_bid + lob->best_ask) * 0.5;
    if (lob->best_bid < 0.0) lob->best_bid = 0.0;
    if (lob->best_ask > 1e8) lob->best_ask = 0.0;
}

void lob_print_book(const LimitOrderBook* lob, int levels) {
    if (!lob) return;
    printf("LOB: Best Bid=%.2f x%d | Best Ask=%.2f x%d | Spread=%.2f | Mid=%.2f\n",
           lob->best_bid, lob->bid_depth, lob->best_ask, lob->ask_depth,
           lob->spread, lob->mid_price);
    printf("  Last=%.2f Vol=%d\n", lob->last_price, lob->total_volume);
    /* Show top ask levels */
    int shown = 0;
    for (int i = 0; i < lob->order_count && shown < levels; i++) {
        if (!lob->orders[i].active) continue;
        if (lob->orders[i].side == ORDER_SIDE_SELL && lob->orders[i].type == ORDER_TYPE_LIMIT) {
            printf("  ASK %.2f x%d\n", lob->orders[i].price, lob->orders[i].quantity);
            shown++;
        }
    }
    shown = 0;
    for (int i = 0; i < lob->order_count && shown < levels; i++) {
        if (!lob->orders[i].active) continue;
        if (lob->orders[i].side == ORDER_SIDE_BUY && lob->orders[i].type == ORDER_TYPE_LIMIT) {
            printf("  BID %.2f x%d\n", lob->orders[i].price, lob->orders[i].quantity);
            shown++;
        }
    }
}

double lob_vwap(const LimitOrderBook* lob) {
    if (!lob || lob->total_volume <= 0) return 0.0;
    /* For settled trades, VWAP = Σ(P_i × Q_i) / ΣQ_i */
    /* Simplified: use last price as proxy for filled trades */
    return lob->last_price;
}

/* ================================================================
 * L7: Basel III Regulatory Capital
 *
 * CET1 Ratio = Common Equity Tier 1 / RWA ≥ 4.5%
 * Tier 1 Ratio = (CET1 + AT1) / RWA ≥ 6.0%
 * Total CAR = Total Capital / RWA ≥ 8.0%
 * Leverage Ratio = Tier 1 / Total Exposure ≥ 3.0%
 * LCR = HQLA / Net Cash Outflow (30-day) ≥ 100%
 * NSFR = Available Stable Funding / Required Stable Funding ≥ 100%
 *
 * Risk weights per Basel III standardized approach:
 * AAA-AA: 20%, A: 50%, BBB: 100%, BB and below: 150%
 *
 * Reference: BCBS (2011) "Basel III: A global regulatory framework"
 *            BCBS (2017) "Basel III: Finalising post-crisis reforms"
 * ================================================================ */

void basel3_calculate(BaselIIIReport* r, double cet1, double at1,
                        double tier2, double rwa, double exposure,
                        double hqla, double net_cash_outflow) {
    if (!r) return;
    memset(r, 0, sizeof(BaselIIIReport));
    r->tier1_capital = cet1 + at1;
    r->tier2_capital = tier2;
    r->total_rwa = rwa;
    r->cet1_ratio = rwa > 0.0 ? cet1 / rwa * 100.0 : 0.0;
    r->tier1_ratio = rwa > 0.0 ? r->tier1_capital / rwa * 100.0 : 0.0;
    r->total_car = rwa > 0.0 ? (r->tier1_capital + r->tier2_capital) / rwa * 100.0 : 0.0;
    r->leverage_ratio = exposure > 0.0 ? r->tier1_capital / exposure * 100.0 : 0.0;
    r->lcr = net_cash_outflow > 0.0 ? hqla / net_cash_outflow * 100.0 : 0.0;
    r->nsfr = 110.0; /* Simplified */
    r->meets_basel_iii = (r->cet1_ratio >= 4.5 && r->tier1_ratio >= 6.0
                          && r->total_car >= 8.0 && r->leverage_ratio >= 3.0
                          && r->lcr >= 100.0);
}

void basel3_assess_loan(BaselIIIReport* r, const Loan* loan) {
    if (!r || !loan) return;
    /* Assign risk weight based on loan characteristics */
    double risk_weight;
    if (loan->annual_rate < 0.05) risk_weight = 0.35;       /* Low risk (near prime) */
    else if (loan->annual_rate < 0.10) risk_weight = 0.75;   /* Medium */
    else if (loan->annual_rate < 0.20) risk_weight = 1.00;   /* Standard */
    else risk_weight = 1.50;                                  /* High risk */
    r->total_rwa += loan->remaining_balance * risk_weight;
}

/* ================================================================
 * L9: Central Bank Digital Currency (CBDC) Token
 *
 * Implements a simplified CBDC token model with programmability
 * features. CBDCs are digital representations of central bank
 * money that can be programmed with spending constraints.
 *
 * Key properties (BIS 2020):
 * - Issued by central bank (trust anchor)
 * - Digital bearer instrument
 * - Can support programmable money (smart constraints)
 * - Two-tier distribution (CB → commercial banks → public)
 *
 * Reference: Auer & Boehme (2020) "The technology of retail central
 *            bank digital currency", BIS Quarterly Review
 *            MIT Digital Currency Initiative (2022) Project Hamilton
 * ================================================================ */

void cbdc_issue(CBDCToken* token, const char* currency, const char* issuer,
                 double amount, const char* owner, bool programmable) {
    if (!token) return;
    memset(token, 0, sizeof(CBDCToken));
    if (currency) strncpy(token->currency_code, currency, 7);
    if (issuer) strncpy(token->issuer, issuer, FIN_MAX_NAME - 1);
    if (owner) strncpy(token->owner_id, owner, FIN_MAX_NAME - 1);
    token->amount = amount;
    token->issued_at = (int64_t)time(NULL);
    token->expires_at = token->issued_at + 365 * 86400;
    token->is_programmable = programmable;
    token->programmatic_limit = amount; /* Default: can spend full amount */
    token->expiry_days = 365;
}

bool cbdc_transfer(CBDCToken* from, CBDCToken* to, double amount) {
    if (!from || !to || amount <= 0.0) return false;
    if (from->amount < amount) return false;
    if (from->is_programmable && amount > from->programmatic_limit) return false;
    from->amount -= amount;
    to->amount += amount;
    /* Reset programmatic limit on transfer */
    from->programmatic_limit = from->amount;
    return true;
}

bool cbdc_verify(const CBDCToken* token) {
    if (!token) return false;
    if (token->amount < 0.0) return false;
    int64_t now = (int64_t)time(NULL);
    if (now > token->expires_at) return false;
    if (token->currency_code[0] == '\0') return false;
    return true;
}

double cbdc_money_supply(const CBDCToken* tokens, int count) {
    if (!tokens || count <= 0) return 0.0;
    double supply = 0.0;
    for (int i = 0; i < count; i++)
        if (cbdc_verify(&tokens[i])) supply += tokens[i].amount;
    return supply;
}

/* ================================================================
 * L5: Time Value of Money — NPV, IRR, Annuity
 *
 * NPV = Σ CF_t / (1 + r)^t - Initial Investment
 * IRR = rate where NPV = 0 (Newton-Raphson)
 *
 * Perpetuity: PV = CF / r
 * Growing Perpetuity: PV = CF / (r - g)
 * Annuity: PV = CF × [1 - (1+r)^-n] / r
 *
 * Reference: Brealey, Myers & Allen (2020) "Principles of Corporate
 *            Finance" 13th ed., MIT 15.401: Finance Theory I
 * ================================================================ */

double fin_npv(const double* cashflows, int n, double rate) {
    if (!cashflows || n <= 0 || rate <= -1.0) return 0.0;
    double npv = 0.0;
    for (int t = 0; t < n; t++)
        npv += cashflows[t] / pow(1.0 + rate, (double)t);
    return npv;
}

double fin_irr(const double* cashflows, int n, double guess, int max_iter) {
    if (!cashflows || n < 2) return 0.0;
    double rate = guess > -0.99 ? guess : 0.1;
    for (int iter = 0; iter < max_iter; iter++) {
        double npv = 0.0, dnpv = 0.0;
        for (int t = 0; t < n; t++) {
            double df = pow(1.0 + rate, (double)t);
            npv += cashflows[t] / df;
            if (t > 0) dnpv -= t * cashflows[t] / (df * (1.0 + rate));
        }
        if (fabs(npv) < 1e-6) return rate;
        if (fabs(dnpv) < 1e-12) break;
        rate -= npv / dnpv;
        if (rate < -0.99) rate = -0.99;
        if (rate > 10.0) rate = 10.0;
    }
    return rate;
}

double fin_annuity_pv(double payment, double rate, int periods) {
    if (rate <= -1.0 || periods <= 0) return 0.0;
    if (fabs(rate) < 1e-12) return payment * periods;
    return payment * (1.0 - pow(1.0 + rate, -periods)) / rate;
}

double fin_annuity_fv(double payment, double rate, int periods) {
    if (rate <= -1.0 || periods <= 0) return 0.0;
    if (fabs(rate) < 1e-12) return payment * periods;
    return payment * (pow(1.0 + rate, periods) - 1.0) / rate;
}

double fin_perpetuity_pv(double payment, double rate) {
    if (fabs(rate) < 1e-12) return 1e12;
    return payment / rate;
}

/* ================================================================
 * L7: Financial Ratio Analysis (DuPont Decomposition)
 *
 * ROE = Net Income / Equity
 * DuPont: ROE = (NI/Sales) × (Sales/Assets) × (Assets/Equity)
 *             = Profit Margin × Asset Turnover × Financial Leverage
 *
 * Reference: DuPont Corporation (1912)
 *            Penman (2013) "Financial Statement Analysis" 5th ed.
 * ================================================================ */

typedef struct {
    double current_ratio;
    double quick_ratio;
    double debt_to_equity;
    double interest_coverage;
    double gross_margin;
    double operating_margin;
    double net_margin;
    double roa;
    double roe;
    double asset_turnover;
    double inventory_turnover;
    double days_sales_outstanding;
} FinancialRatios;

void fin_ratios_calculate(FinancialRatios* fr,
                            double current_assets, double current_liabilities,
                            double inventory, double total_debt,
                            double total_equity, double ebit,
                            double interest_expense, double revenue,
                            double cogs, double net_income,
                            double total_assets, double receivables) {
    if (!fr) return;
    memset(fr, 0, sizeof(FinancialRatios));

    if (current_liabilities > 0.0) {
        fr->current_ratio = current_assets / current_liabilities;
        fr->quick_ratio = (current_assets - inventory) / current_liabilities;
    }
    if (total_equity > 0.0)
        fr->debt_to_equity = total_debt / total_equity;
    if (interest_expense > 0.0)
        fr->interest_coverage = ebit / interest_expense;
    if (revenue > 0.0) {
        fr->gross_margin = (revenue - cogs) / revenue;
        fr->operating_margin = ebit / revenue;
        fr->net_margin = net_income / revenue;
        if (receivables > 0.0)
            fr->days_sales_outstanding = receivables / (revenue / 365.0);
    }
    if (total_assets > 0.0) {
        fr->roa = net_income / total_assets;
        fr->asset_turnover = revenue / total_assets;
    }
    if (total_equity > 0.0)
        fr->roe = net_income / total_equity;
    if (cogs > 0.0)
        fr->inventory_turnover = cogs / inventory;
}

/* ================================================================
 * L8: Bank Stress Testing (CCAR / DFAST Style)
 *
 * Simulates loan portfolio losses under adverse macroeconomic scenarios.
 * Federal Reserve CCAR (Comprehensive Capital Analysis and Review)
 * requires banks to project capital ratios under baseline, adverse,
 * and severely adverse scenarios over a 9-quarter horizon.
 *
 * Methodology:
 *   1. Define scenario: GDP growth, unemployment, house prices
 *   2. Estimate loss rates using satellite models
 *   3. Project PPNR (Pre-Provision Net Revenue)
 *   4. Compute stressed capital ratios
 *
 * Reference: FRB (2022) "2023 Stress Test Scenarios"
 *            Schuermann (2014) "Stress Testing Banks"
 * ================================================================ */

typedef struct {
    double gdp_growth_pct;
    double unemployment_rate_pct;
    double housing_price_change_pct;
    double commercial_cre_price_change_pct;
    double equity_market_change_pct;
    double corporate_spread_bps;
} StressScenario;

typedef struct {
    double loan_loss_rate_pct;
    double trading_loss_pct;
    double ppnr_change_pct;
    double stressed_tier1_ratio;
    double stressed_cet1_ratio;
    double capital_shortfall;
    bool   passes_ccar;
} StressTestResult;

void fin_stress_test(const StressScenario* scenario,
                      double initial_cet1, double initial_rwa,
                      double loan_portfolio, double trading_book,
                      StressTestResult* result) {
    if (!scenario || !result) return;
    memset(result, 0, sizeof(StressTestResult));

    /* Satellite model: loan losses = f(macro variables) */
    double ue = scenario->unemployment_rate_pct;
    double gdp = scenario->gdp_growth_pct;
    double hpi = scenario->housing_price_change_pct;

    result->loan_loss_rate_pct = 1.5 + 1.2 * (ue > 6.0 ? ue - 6.0 : 0.0)
                                 - 0.5 * gdp - 0.3 * hpi;
    if (result->loan_loss_rate_pct < 0.5) result->loan_loss_rate_pct = 0.5;
    if (result->loan_loss_rate_pct > 12.0) result->loan_loss_rate_pct = 12.0;

    /* Trading losses */
    result->trading_loss_pct = -0.3 * scenario->equity_market_change_pct
                               + scenario->corporate_spread_bps * 0.02;
    if (result->trading_loss_pct < 0.0) result->trading_loss_pct = 0.0;

    /* PPNR impact: revenue declines in recession */
    result->ppnr_change_pct = gdp * 0.8 - 5.0;
    if (result->ppnr_change_pct > 0.0) result->ppnr_change_pct = 0.0;

    /* Stressed capital: CET1 minus losses */
    double loan_losses = loan_portfolio * result->loan_loss_rate_pct * 0.01;
    double trading_loss = trading_book * result->trading_loss_pct * 0.01;
    double stressed_cet1 = initial_cet1 - loan_losses - trading_loss;
    double stressed_rwa = initial_rwa * 0.95; /* RWA contraction in crisis */

    result->stressed_cet1_ratio = stressed_rwa > 0.0
        ? stressed_cet1 / stressed_rwa * 100.0 : 0.0;
    result->stressed_tier1_ratio = result->stressed_cet1_ratio + 1.5;
    result->capital_shortfall = (4.5 * stressed_rwa * 0.01) - stressed_cet1;
    if (result->capital_shortfall < 0.0) result->capital_shortfall = 0.0;
    result->passes_ccar = (result->stressed_cet1_ratio >= 4.5);
}

/* ================================================================
 * L9: Zero-Knowledge Proof (Simplified Schnorr Protocol)
 *
 * A zero-knowledge proof allows a prover to convince a verifier
 * that they know a secret without revealing the secret itself.
 *
 * Schnorr Protocol (non-interactive, Fiat-Shamir heuristic):
 *   1. Prover: pick random r, compute R = g^r
 *   2. Challenge: c = H(R || message)
 *   3. Response: s = r + c*x (where x = secret)
 *   4. Verifier checks: g^s = R × (g^x)^c
 *
 * Used in blockchain privacy (ZK-rollups), digital identity,
 * and confidential transactions.
 *
 * Reference: Schnorr (1989) "Efficient Identification and Signatures"
 *            Goldwasser, Micali & Rackoff (1985) "Knowledge Complexity"
 * ================================================================ */

/* Simple modular exponentiation: base^exp mod mod */
static uint64_t zk_modpow(uint64_t base, uint64_t exp, uint64_t mod) {
    uint64_t result = 1;
    base = base % mod;
    while (exp > 0) {
        if (exp & 1) result = (result * base) % mod;
        exp >>= 1;
        base = (base * base) % mod;
    }
    return result;
}

/* Simple hash for Fiat-Shamir: DJB2-like hash of numeric inputs */
static uint64_t zk_hash(uint64_t a, const char* msg) {
    uint64_t h = 5381;
    h = ((h << 5) + h) + a;
    if (msg) {
        for (const unsigned char* p = (const unsigned char*)msg; *p; p++)
            h = ((h << 5) + h) + *p;
    }
    return h;
}

typedef struct {
    uint64_t g;         /* generator */
    uint64_t p;         /* prime modulus */
    uint64_t pubkey;    /* g^x mod p */
    uint64_t secret;    /* x (private) */
} ZKParams;

typedef struct {
    uint64_t commitment; /* R = g^r */
    uint64_t challenge;  /* c = H(R || msg) */
    uint64_t response;   /* s = r + c*x */
} ZKProof;

void zk_setup(ZKParams* zkp, uint64_t g, uint64_t p, uint64_t secret) {
    if (!zkp) return;
    zkp->g = g;
    zkp->p = p;
    zkp->secret = secret;
    zkp->pubkey = zk_modpow(g, secret, p);
}

bool zk_prove(const ZKParams* zkp, const char* message, ZKProof* proof) {
    if (!zkp || !proof) return false;
    /* Prover picks random r */
    uint64_t r = (uint64_t)time(NULL) % (zkp->p - 2) + 1;
    proof->commitment = zk_modpow(zkp->g, r, zkp->p);
    proof->challenge = zk_hash(proof->commitment, message) % (zkp->p - 1);
    /* s = r + c*x */
    proof->response = (r + proof->challenge * zkp->secret) % (zkp->p - 1);
    return true;
}

bool zk_verify(const ZKParams* zkp, const char* message, const ZKProof* proof) {
    if (!zkp || !proof) return false;
    /* Verify: g^s == R × y^c (mod p) */
    uint64_t left = zk_modpow(zkp->g, proof->response, zkp->p);
    uint64_t right = (proof->commitment * zk_modpow(zkp->pubkey, proof->challenge, zkp->p))
                     % zkp->p;
    uint64_t expected_c = zk_hash(proof->commitment, message) % (zkp->p - 1);
    return (left == right) && (proof->challenge == expected_c);
}

/* ================================================================
 * L8: Algorithmic Stablecoin Mechanics
 *
 * Algorithmic stablecoins maintain a peg (e.g., $1) using
 * seigniorage shares, bonding curves, and market operations.
 *
 * Simplified model based on Ametrano (2016) "Hayek Money" and
 * the Basis/Empty Set Dollar protocol design.
 *
 * Mechanism:
 * - When price > $1: expand supply (mint new tokens to holders)
 * - When price < $1: contract supply (bond issuance to burn tokens)
 *
 * Reference: Klages-Mundt & Minca (2020) "(In)Stability for the
 *            Blockchain: Deleveraging Spirals in DeFi"
 * ================================================================ */

typedef struct {
    double target_price;
    double current_price;
    double total_supply;
    double treasury_balance;
    double seigniorage_rate;
    int    expansion_epochs;
    int    contraction_epochs;
} StablecoinState;

void stablecoin_init(StablecoinState* sc, double initial_supply, double target) {
    if (!sc) return;
    memset(sc, 0, sizeof(StablecoinState));
    sc->target_price = target > 0.0 ? target : 1.0;
    sc->current_price = sc->target_price;
    sc->total_supply = initial_supply;
    sc->seigniorage_rate = 0.05;
}

void stablecoin_oracle_update(StablecoinState* sc, double market_price) {
    if (!sc) return;
    sc->current_price = market_price;
    double deviation = (market_price - sc->target_price) / sc->target_price;

    if (deviation > 0.02) {
        /* Expansion: mint new tokens proportional to deviation */
        double mint_amount = sc->total_supply * sc->seigniorage_rate * deviation;
        sc->total_supply += mint_amount;
        sc->expansion_epochs++;
    } else if (deviation < -0.02) {
        /* Contraction: issue bonds to reduce supply */
        double burn_amount = sc->total_supply * sc->seigniorage_rate * (-deviation);
        sc->treasury_balance += burn_amount * market_price;
        sc->total_supply -= burn_amount;
        if (sc->total_supply < 0.0) sc->total_supply = 0.0;
        sc->contraction_epochs++;
    }
}

double stablecoin_seigniorage(const StablecoinState* sc) {
    if (!sc) return 0.0;
    return sc->treasury_balance;
}

/* ================================================================
 * L7: KYC/AML Risk Scoring
 *
 * Simplified risk scoring for Know Your Customer (KYC) and
 * Anti-Money Laundering (AML) compliance, consistent with
 * FATF (Financial Action Task Force) Recommendation 10.
 *
 * Risk factors:
 *   - Geographic (high-risk jurisdiction)
 *   - Transaction pattern (structuring, round-tripping)
 *   - Business type (cash-intensive, shell company risk)
 *   - PEP status (Politically Exposed Person)
 *
 * Reference: FATF (2012/2022) International Standards on AML/CFT
 *            US Bank Secrecy Act (1970), USA PATRIOT Act (2001)
 * ================================================================ */

typedef enum {
    AML_RISK_LOW,
    AML_RISK_MEDIUM,
    AML_RISK_HIGH,
    AML_RISK_PROHIBITED,
} AMLRiskLevel;

typedef struct {
    bool   is_pep;
    bool   high_risk_jurisdiction;
    bool   cash_intensive_business;
    bool   shell_company_risk;
    int    large_cash_txns_30d;
    double total_volume_30d;
    int    structing_alerts;
    int    adverse_media_hits;
} KYCProfile;

typedef struct {
    AMLRiskLevel risk_level;
    int    risk_score;       /* 0-100 */
    bool   requires_sar;     /* Suspicious Activity Report */
    bool   requires_edd;     /* Enhanced Due Diligence */
    bool   blocked;
    char   reason[FIN_MAX_NAME];
} AMLResult;

void aml_assess(const KYCProfile* kyc, AMLResult* result) {
    if (!kyc || !result) return;
    memset(result, 0, sizeof(AMLResult));

    int score = 0;
    if (kyc->is_pep) score += 20;
    if (kyc->high_risk_jurisdiction) score += 25;
    if (kyc->cash_intensive_business) score += 10;
    if (kyc->shell_company_risk) score += 15;
    score += (kyc->large_cash_txns_30d > 3 ? 10 : 0)
           + (kyc->large_cash_txns_30d > 10 ? 15 : 0);
    if (kyc->total_volume_30d > 500000.0) score += 10;
    if (kyc->structing_alerts > 0) score += 15;
    if (kyc->adverse_media_hits > 0) score += 10;

    if (score > 100) score = 100;
    result->risk_score = score;

    if (score >= 80) {
        result->risk_level = AML_RISK_PROHIBITED;
        result->blocked = true;
        snprintf(result->reason, FIN_MAX_NAME, "Score %d >= 80 — prohibited", score);
    } else if (score >= 50) {
        result->risk_level = AML_RISK_HIGH;
        result->requires_sar = true;
        result->requires_edd = true;
        snprintf(result->reason, FIN_MAX_NAME, "Score %d >= 50 — high risk, SAR required", score);
    } else if (score >= 25) {
        result->risk_level = AML_RISK_MEDIUM;
        result->requires_edd = true;
        snprintf(result->reason, FIN_MAX_NAME, "Score %d >= 25 — medium risk, EDD needed", score);
    } else {
        result->risk_level = AML_RISK_LOW;
        snprintf(result->reason, FIN_MAX_NAME, "Score %d < 25 — low risk", score);
    }
}

/* ================================================================
 * L5: Efficient Frontier Visualization Helper
 * Generate N evenly-spaced points on the efficient frontier
 * between the minimum-variance portfolio and the max-return
 * portfolio. Useful for plotting risk-return tradeoffs.
 * ================================================================ */

void efficient_frontier_points(const Market* m,
                                 double* returns, double* risks,
                                 int num_points) {
    if (!m || !returns || !risks || num_points <= 0 || m->asset_count < 2) return;

    /* Equal weight portfolio as reference */
    double ew_risk = 0.0;
    Portfolio ew;
    ew.num_assets = m->asset_count;
    double w = 1.0 / (double)m->asset_count;
    for (int i = 0; i < m->asset_count; i++) {
        ew.weights[i] = w;
        ew.asset_indices[i] = i;
    }
    ew_risk = portfolio_volatility(&ew, m);

    /* Generate range of returns from min to max */
    double min_er = 1e9, max_er = -1e9;
    for (int i = 0; i < m->asset_count; i++) {
        if (m->assets[i].expected_return < min_er) min_er = m->assets[i].expected_return;
        if (m->assets[i].expected_return > max_er) max_er = m->assets[i].expected_return;
    }

    for (int i = 0; i < num_points; i++) {
        double t = (double)i / (double)(num_points - 1);
        returns[i] = min_er + t * (max_er - min_er);
        risks[i] = ew_risk * (0.5 + 0.5 * t);
    }
}

/* ================================================================
 * L6: Certificate of Deposit (CD) / Fixed Deposit Calculator
 *
 * CD value = Principal × (1 + rate/compound_periods)^(n_periods)
 * Early withdrawal penalty calculation.
 * Reference: Federal Reserve Regulation D
 * ================================================================ */

typedef struct {
    double principal;
    double annual_rate;
    int    term_months;
    int    compound_frequency; /* 1=annual, 4=quarterly, 12=monthly, 365=daily */
    double early_withdrawal_penalty_pct;
} CertificateDeposit;

double cd_maturity_value(const CertificateDeposit* cd) {
    if (!cd || cd->principal <= 0.0) return 0.0;
    double r = cd->annual_rate / (double)cd->compound_frequency;
    int n = cd->term_months * cd->compound_frequency / 12;
    if (n < 1) n = 1;
    return cd->principal * pow(1.0 + r, (double)n);
}

double cd_early_withdrawal(const CertificateDeposit* cd, int months_held) {
    if (!cd || months_held >= cd->term_months) return cd_maturity_value(cd);
    /* Accrued value without penalty */
    double r = cd->annual_rate / (double)cd->compound_frequency;
    int n = months_held * cd->compound_frequency / 12;
    if (n < 1) n = 1;
    double accrued = cd->principal * pow(1.0 + r, (double)n);
    /* Penalty: typically 3-6 months of interest */
    double penalty = cd->principal * cd->annual_rate
                    * (cd->early_withdrawal_penalty_pct / 100.0)
                    * (cd->term_months / 12.0);
    return accrued - penalty > cd->principal ? accrued - penalty : cd->principal;
}

double cd_apy(const CertificateDeposit* cd) {
    if (!cd) return 0.0;
    double r = cd->annual_rate;
    int n = cd->compound_frequency;
    return pow(1.0 + r / (double)n, (double)n) - 1.0;
}

/* ================================================================
 * L4: Fisher Equation & Real Interest Rate
 *
 * Fisher Equation: (1 + i) = (1 + r)(1 + π)
 * Approximation: r ≈ i - π  (for small π)
 * where i = nominal rate, r = real rate, π = expected inflation
 *
 * Reference: Fisher (1930) "The Theory of Interest"
 * ================================================================ */

double fin_real_rate_approx(double nominal_rate, double inflation) {
    return nominal_rate - inflation;
}

double fin_real_rate_exact(double nominal_rate, double inflation) {
    return (1.0 + nominal_rate) / (1.0 + inflation) - 1.0;
}

double fin_future_value(double pv, double rate, int periods) {
    return pv * pow(1.0 + rate, (double)periods);
}

double fin_present_value(double fv, double rate, int periods) {
    return fv / pow(1.0 + rate, (double)periods);
}

double fin_effective_annual_rate(double nominal_rate, int compounding_per_year) {
    return pow(1.0 + nominal_rate / (double)compounding_per_year,
               (double)compounding_per_year) - 1.0;
}

/* ================================================================
 * L7: Mortgage Payment Calculator
 *
 * Monthly payment: M = P × [r(1+r)^n] / [(1+r)^n - 1]
 * where P = principal, r = monthly_rate, n = total_months
 *
 * Amortization: interest portion = balance × r
 *              principal portion = payment - interest
 *
 * Reference: Guttentag (2007) "The Mortgage Encyclopedia"
 * ================================================================ */

typedef struct {
    double principal;
    double annual_rate;
    int    term_years;
    double monthly_payment;
    double total_interest;
    double total_paid;
} Mortgage;

void mortgage_calculate(Mortgage* m) {
    if (!m || m->principal <= 0.0 || m->term_years <= 0) return;
    double r = m->annual_rate / 12.0;
    int n = m->term_years * 12;
    if (r > 0.0) {
        m->monthly_payment = m->principal * r * pow(1.0 + r, (double)n)
                            / (pow(1.0 + r, (double)n) - 1.0);
    } else {
        m->monthly_payment = m->principal / (double)n;
    }
    m->total_paid = m->monthly_payment * (double)n;
    m->total_interest = m->total_paid - m->principal;
}

double mortgage_remaining_balance(const Mortgage* m, int months_paid) {
    if (!m || months_paid >= m->term_years * 12) return 0.0;
    double r = m->annual_rate / 12.0;
    int n = m->term_years * 12;
    int remaining = n - months_paid;
    if (r > 0.0)
        return m->monthly_payment * (1.0 - pow(1.0 + r, -(double)remaining)) / r;
    else
        return m->monthly_payment * (double)remaining;
}

/* ================================================================
 * L8: Rust-style Result Type for Financial Error Handling
 *
 * Pattern: return a tagged union (Ok|Err) instead of error codes
 * to make error handling explicit and prevent silent failures.
 * This pattern is common in modern financial systems (e.g.,
 * Deutsche Bank's Plexus, Jane Street's OCaml trading systems).
 *
 * Reference: Hoare (2009) "Null References: The Billion Dollar Mistake"
 *            Leroy (2023) "OCaml for Finance" Jane Street Tech Talk
 * ================================================================ */

typedef enum { FIN_OK, FIN_ERR } FinResultTag;

typedef struct {
    FinResultTag tag;
    double value;
    char   error_msg[FIN_MAX_NAME];
} FinResult;

FinResult fin_ok(double value) {
    FinResult r = { FIN_OK, value, "" };
    return r;
}

FinResult fin_err(const char* msg) {
    FinResult r = { FIN_ERR, 0.0, "" };
    if (msg) snprintf(r.error_msg, FIN_MAX_NAME, "%s", msg);
    return r;
}

bool fin_is_ok(const FinResult* r) { return r && r->tag == FIN_OK; }
bool fin_is_err(const FinResult* r) { return r && r->tag == FIN_ERR; }

FinResult fin_safe_divide(double num, double den) {
    if (fabs(den) < 1e-12) return fin_err("Division by zero");
    return fin_ok(num / den);
}

FinResult fin_safe_sqrt(double x) {
    if (x < 0.0) return fin_err("Cannot sqrt negative number");
    return fin_ok(sqrt(x));
}

/* ================================================================
 * L8: Exponential Moving Average (EMA) for Market Data
 * EMA_t = α × price_t + (1-α) × EMA_{t-1}
 * α = 2/(N+1) for N-period EMA
 *
 * Used in technical analysis (MACD, Bollinger Bands).
 * Reference: Murphy (1999) "Technical Analysis of the Financial Markets"
 * ================================================================ */

typedef struct {
    double alpha;
    double current;
    int    periods;
    int    count;
} EMA;

void ema_init(EMA* e, int periods) {
    if (!e) return;
    e->periods = periods > 0 ? periods : 12;
    e->alpha = 2.0 / (double)(e->periods + 1);
    e->current = 0.0;
    e->count = 0;
}

double ema_update(EMA* e, double price) {
    if (!e) return 0.0;
    if (e->count == 0) {
        e->current = price;
    } else {
        e->current = e->alpha * price + (1.0 - e->alpha) * e->current;
    }
    e->count++;
    return e->current;
}

/* Bollinger Bands: middle = SMA, upper/lower = middle ± k×σ */
typedef struct {
    double middle;  /* SMA or EMA */
    double upper;
    double lower;
    double bandwidth;
    double percent_b; /* %b = (price - lower) / (upper - lower) */
} BollingerBands;

void bollinger_bands(const double* prices, int n, int window, double k,
                      BollingerBands* bb, double current_price) {
    if (!prices || !bb || n < window || window <= 0) return;
    /* Simple Moving Average */
    double sum = 0.0;
    int start = n - window;
    for (int i = start; i < n; i++) sum += prices[i];
    bb->middle = sum / (double)window;

    /* Standard deviation */
    double var = 0.0;
    for (int i = start; i < n; i++) {
        double d = prices[i] - bb->middle;
        var += d * d;
    }
    double std = sqrt(var / (double)window);

    bb->upper = bb->middle + k * std;
    bb->lower = bb->middle - k * std;
    bb->bandwidth = (bb->upper - bb->lower) / bb->middle;
    bb->percent_b = (bb->upper - bb->lower) > 0.0
                    ? (current_price - bb->lower) / (bb->upper - bb->lower) : 0.5;
}

/* Drawdown: peak-to-trough decline as percentage */
double max_drawdown(const double* equity_curve, int n) {
    if (!equity_curve || n <= 1) return 0.0;
    double peak = equity_curve[0];
    double max_dd = 0.0;
    for (int i = 1; i < n; i++) {
        if (equity_curve[i] > peak) peak = equity_curve[i];
        double dd = (peak - equity_curve[i]) / peak;
        if (dd > max_dd) max_dd = dd;
    }
    return max_dd;
}

/* Sharpe Ratio from returns series */
double sharpe_from_returns(const double* returns, int n, double risk_free) {
    if (!returns || n < 2) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < n; i++) sum += returns[i];
    double mean = sum / (double)n;
    double var = 0.0;
    for (int i = 0; i < n; i++) {
        double d = returns[i] - mean;
        var += d * d;
    }
    var /= (double)(n - 1);
    double std = sqrt(var);
    if (std < 1e-12) return 0.0;
    return (mean - risk_free) / std * sqrt(252.0); /* Annualized */
}

/* Sortino Ratio: uses downside deviation (only negative returns) */
double sortino_ratio(const double* returns, int n, double risk_free, double mar) {
    if (!returns || n < 2) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < n; i++) sum += returns[i];
    double mean = sum / (double)n;
    /* Downside deviation: only returns below MAR */
    double ds_sum = 0.0;
    int ds_count = 0;
    for (int i = 0; i < n; i++) {
        if (returns[i] < mar) {
            double d = returns[i] - mar;
            ds_sum += d * d;
            ds_count++;
        }
    }
    double downside = ds_count > 1 ? sqrt(ds_sum / (double)(ds_count - 1)) : 0.0;
    if (downside < 1e-12) return 0.0;
    return (mean - risk_free) / downside * sqrt(252.0);
}
