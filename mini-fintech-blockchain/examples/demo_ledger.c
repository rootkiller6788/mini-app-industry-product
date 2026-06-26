#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "fintech_core.h"

/* Demo: Double-entry bookkeeping system
 * Showcases: account creation, deposits, transfers, interest, balance verification */

int main(void) {
    printf("=== Ledger Demo: Double-Entry Bookkeeping ===

");

    Ledger* l = ledger_create();

    /* Create accounts */
    Account* cash = ledger_create_account(l, "Business Cash", ACCT_ASSET);
    Account* equipment = ledger_create_account(l, "Equipment", ACCT_ASSET);
    Account* loan = ledger_create_account(l, "Bank Loan", ACCT_LIABILITY);
    Account* capital = ledger_create_account(l, "Owner Capital", ACCT_EQUITY);
    Account* revenue = ledger_create_account(l, "Sales Revenue", ACCT_REVENUE);
    Account* expense = ledger_create_account(l, "Rent Expense", ACCT_EXPENSE);

    printf("Created 6 accounts:
");
    printf("  Assets:      Cash, Equipment
");
    printf("  Liabilities: Bank Loan
");
    printf("  Equity:      Owner Capital
");
    printf("  Revenue:     Sales
");
    printf("  Expense:     Rent

");

    /* Initial capital contribution: Dr Cash 50000, Cr Equity 50000 */
    ledger_deposit(l, 1, 50000.0);
    ledger_deposit(l, 4, 50000.0);
    printf("Owner contributes 0,000 -> Cash +50k, Equity +50k
");

    /* Take a bank loan: Dr Cash 20000, Cr Loan 20000 */
    ledger_deposit(l, 1, 20000.0);
    ledger_deposit(l, 3, 20000.0);
    printf("Bank loan of 0,000 -> Cash +20k, Loan +20k
");

    /* Buy equipment: Dr Equipment 15000, Cr Cash 15000 */
    ledger_transfer(l, 1, 2, 15000.0);
    printf("Buy equipment 5,000 -> Cash -15k, Equipment +15k
");

    /* Earn revenue: Dr Cash 8000, Cr Revenue 8000 */
    ledger_deposit(l, 1, 8000.0);
    ledger_deposit(l, 5, 8000.0);
    printf("Sales revenue ,000 -> Cash +8k, Revenue +8k
");

    /* Pay rent: Dr Expense 3000, Cr Cash 3000 */
    ledger_withdraw(l, 1, 3000.0);
    ledger_withdraw(l, 6, 3000.0);
    printf("Pay rent ,000 -> Cash -3k, Expense +3k
");

    /* Apply interest to savings */
    ledger_apply_interest(l, 1, 0.02);
    printf("Interest earned at 2%% on cash balance

");

    ledger_print_summary(l);

    double future = ledger_calculate_interest(10000.0, 0.05, 10);
    printf("
Compound interest: 0,000 at 5%% for 10 years = $%.2f
", future);

    ledger_destroy(l);
    printf("
Demo complete.
");
    return 0;
}
