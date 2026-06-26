#include "ecommerce_payment.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ================================================================
 * L2: Payment Engine — core lifecycle & multi-currency
 * L4: Triangular Arbitrage Detection (Currency arbitrage)
 * L5: Multi-jurisdiction Tax Calculation
 * L7: Complete payment processing & reporting
 * ================================================================ */

const char* payment_method_name(PaymentMethodType m) {
    switch (m) {
        case PM_CREDIT_CARD:      return "Credit Card";
        case PM_DEBIT_CARD:       return "Debit Card";
        case PM_PAYPAL:           return "PayPal";
        case PM_BANK_TRANSFER:    return "Bank Transfer";
        case PM_CRYPTO:           return "Cryptocurrency";
        case PM_GIFT_CARD:        return "Gift Card";
        case PM_CASH_ON_DELIVERY: return "Cash on Delivery";
        case PM_BNPL:             return "Buy Now Pay Later";
        default:                  return "Unknown";
    }
}

const char* transaction_state_name(TransactionState s) {
    switch (s) {
        case TX_AUTHORIZED: return "Authorized";
        case TX_CAPTURED:   return "Captured";
        case TX_SETTLED:    return "Settled";
        case TX_REFUNDED:   return "Refunded";
        case TX_VOIDED:     return "Voided";
        case TX_DECLINED:   return "Declined";
        case TX_ERROR:      return "Error";
        default:            return "Unknown";
    }
}

const char* tax_type_name(TaxType t) {
    switch (t) {
        case TAX_VAT:     return "VAT";
        case TAX_GST:     return "GST";
        case TAX_SALES:   return "Sales Tax";
        case TAX_EXCISE:  return "Excise Duty";
        case TAX_DIGITAL: return "Digital Services Tax";
        default:          return "Unknown";
    }
}

/* --- L2: Engine Lifecycle --- */

PaymentEngine* payment_engine_create(void) {
    PaymentEngine* pe = (PaymentEngine*)calloc(1, sizeof(PaymentEngine));
    if (!pe) return NULL;
    /* Pre-populate with common currencies */
    payment_currency_add(pe, "USD", "US Dollar", 1.0, 2, "$");
    payment_currency_add(pe, "EUR", "Euro", 1.08, 2, "EUR");
    payment_currency_add(pe, "GBP", "British Pound", 1.27, 2, "GBP");
    payment_currency_add(pe, "JPY", "Japanese Yen", 0.0067, 0, "JPY");
    payment_currency_add(pe, "CNY", "Chinese Yuan", 0.14, 2, "CNY");
    payment_currency_add(pe, "INR", "Indian Rupee", 0.012, 2, "INR");
    return pe;
}

/* --- L4: Currency Management ---
 * Exchange rates model: rate_to_usd represents how many USD per 1 unit of currency.
 * convert: amount_in_from * (rate_from_usd / rate_to_usd) = amount_in_to
 * Theorem: Interest Rate Parity — forward/spot rates reflect interest differentials
 */

int payment_currency_add(PaymentEngine* pe, const char* code, const char* name,
                          double rate_to_usd, int decimals, const char* symbol) {
    if (!pe || !code || pe->currency_count >= PAYMENT_MAX_CURRENCIES) return -1;
    Currency* c = &pe->currencies[pe->currency_count];
    snprintf(c->code, PAYMENT_CURRENCY_CODE_LEN, "%s", code);
    snprintf(c->name, 31, "%s", name);
    c->rate_to_usd = rate_to_usd;
    c->decimal_places = decimals;
    snprintf(c->symbol, 7, "%s", symbol);
    pe->currency_count++;
    return pe->currency_count - 1;
}

const Currency* payment_currency_find(const PaymentEngine* pe, const char* code) {
    if (!pe || !code) return NULL;
    for (int i = 0; i < pe->currency_count; i++) {
        if (strcmp(pe->currencies[i].code, code) == 0)
            return &pe->currencies[i];
    }
    return NULL;
}

double payment_convert_currency(const PaymentEngine* pe, double amount,
                                 const char* from_code, const char* to_code) {
    if (!pe || !from_code || !to_code) return amount;
    const Currency* from = payment_currency_find(pe, from_code);
    const Currency* to   = payment_currency_find(pe, to_code);
    if (!from || !to) return amount;
    /* Convert to USD first, then to target */
    double usd = amount * from->rate_to_usd;
    if (to->rate_to_usd <= 0.0) return amount;
    return usd / to->rate_to_usd;
}

/* L4: Triangular Arbitrage Detection
 * Given three currencies A, B, C with exchange rates:
 * If rate(A->B) * rate(B->C) * rate(C->A) > 1, arbitrage profit exists.
 * Our implementation checks all currency triples in the engine.
 * O(n^3) where n = number of currencies.
 */
bool payment_detect_arbitrage(const PaymentEngine* pe, double* max_profit_pct) {
    if (!pe || pe->currency_count < 3) {
        if (max_profit_pct) *max_profit_pct = 0.0;
        return false;
    }
    double best_profit = 0.0;
    int n = pe->currency_count;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            for (int k = 0; k < n; k++) {
                if (k == i || k == j) continue;
                /* A(i) -> B(j) -> C(k) -> A(i) */
                double a_to_b = pe->currencies[j].rate_to_usd /
                                pe->currencies[i].rate_to_usd;
                double b_to_c = pe->currencies[k].rate_to_usd /
                                pe->currencies[j].rate_to_usd;
                double c_to_a = pe->currencies[i].rate_to_usd /
                                pe->currencies[k].rate_to_usd;
                double product = a_to_b * b_to_c * c_to_a;
                if (product > best_profit) best_profit = product;
            }
        }
    }
    if (max_profit_pct) *max_profit_pct = (best_profit - 1.0) * 100.0;
    return best_profit > 1.0001; /* small threshold to avoid rounding noise */
}

/* --- L5: Tax Calculation Engine ---
 * Supports VAT (inclusive pricing common in EU), Sales Tax (US, additive),
 * GST, Excise, and Digital Services Tax.
 * Stacked tax: multiple taxes may apply to the same transaction.
 */

int payment_tax_add_rule(PaymentEngine* pe, const char* region, TaxType type,
                          double rate, double min_taxable, bool inclusive) {
    if (!pe || !region || pe->tax_rule_count >= PAYMENT_MAX_TAX_RULES) return -1;
    TaxRule* r = &pe->tax_rules[pe->tax_rule_count];
    snprintf(r->region, 63, "%s", region);
    r->type = type;
    r->rate = rate;
    r->min_taxable = min_taxable;
    r->inclusive = inclusive;
    snprintf(r->description, 127, "%s %s %.1f%%", region, tax_type_name(type), rate);
    pe->tax_rule_count++;
    return pe->tax_rule_count - 1;
}

const TaxRule* payment_tax_find_rule(const PaymentEngine* pe, const char* region,
                                      TaxType type) {
    if (!pe || !region) return NULL;
    for (int i = 0; i < pe->tax_rule_count; i++) {
        if (strcmp(pe->tax_rules[i].region, region) == 0 &&
            pe->tax_rules[i].type == type)
            return &pe->tax_rules[i];
    }
    return NULL;
}

double payment_tax_calculate(const PaymentEngine* pe, const char* region,
                              double subtotal, double shipping) {
    if (!pe || !region) return 0.0;
    double total_tax = 0.0;
    double taxable = subtotal + shipping;

    for (int i = 0; i < pe->tax_rule_count; i++) {
        const TaxRule* r = &pe->tax_rules[i];
        if (strcmp(r->region, region) != 0) continue;
        if (taxable < r->min_taxable) continue;

        if (r->inclusive) {
            /* VAT-inclusive: tax = total * rate/(100+rate) */
            total_tax += taxable * r->rate / (100.0 + r->rate);
        } else {
            total_tax += taxable * r->rate / 100.0;
        }
    }
    return total_tax;
}

/* L5: Composite tax — returns total tax and lists which types were applied */
double payment_composite_tax(const PaymentEngine* pe, const char* region,
                              double subtotal, double shipping,
                              TaxType* applied, int* count) {
    if (!pe || !region) { if (count) *count = 0; return 0.0; }
    double total = 0.0;
    int cnt = 0;
    double taxable = subtotal + shipping;

    for (int i = 0; i < pe->tax_rule_count && cnt < 8; i++) {
        const TaxRule* r = &pe->tax_rules[i];
        if (strcmp(r->region, region) != 0) continue;
        if (taxable < r->min_taxable) continue;

        double tax = r->inclusive
            ? taxable * r->rate / (100.0 + r->rate)
            : taxable * r->rate / 100.0;
        total += tax;
        if (applied) applied[cnt] = r->type;
        cnt++;
    }
    if (count) *count = cnt;
    return total;
}

/* --- L3: Payment Gateway Abstraction --- */

int payment_gateway_register(PaymentEngine* pe, const char* name,
                              double fee_pct, double fee_fixed,
                              bool refund, bool recurring) {
    if (!pe || !name || pe->gateway_count >= PAYMENT_MAX_PAYMENT_METHODS) return -1;
    PaymentGateway* gw = &pe->gateways[pe->gateway_count];
    snprintf(gw->name, 63, "%s", name);
    gw->fee_percent = fee_pct;
    gw->fee_fixed = fee_fixed;
    gw->supports_refund = refund;
    gw->supports_recurring = recurring;
    gw->success_count = 0;
    gw->failure_count = 0;
    gw->total_processed = 0.0;
    gw->method_count = 0;
    pe->gateway_count++;
    return pe->gateway_count - 1;
}

double payment_calculate_fee(const PaymentGateway* gw, double amount) {
    if (!gw) return 0.0;
    return amount * gw->fee_percent / 100.0 + gw->fee_fixed;
}

/* --- L2: Transaction Lifecycle ---
 * Authorize -> Capture -> Settle -> (optional) Refund/Void
 * Each transition validates the current state before proceeding.
 */

PaymentTransaction* payment_authorize(PaymentEngine* pe, int order_id,
                                       PaymentMethodType method, double amount,
                                       const char* currency_code,
                                       const char* last_four) {
    if (!pe || pe->transaction_count >= PAYMENT_MAX_TRANSACTIONS) return NULL;
    PaymentTransaction* tx = &pe->transactions[pe->transaction_count];
    memset(tx, 0, sizeof(PaymentTransaction));
    tx->id = pe->transaction_count + 1;
    tx->order_id = order_id;
    tx->method = method;
    tx->state = TX_AUTHORIZED;
    tx->amount = amount;
    tx->total_amount = amount;
    if (currency_code) snprintf(tx->currency_code, PAYMENT_CURRENCY_CODE_LEN, "%s", currency_code);
    if (last_four) snprintf(tx->last_four, 4, "%s", last_four);
    tx->created_at = (int64_t)time(NULL);
    pe->transaction_count++;
    return tx;
}

bool payment_capture(PaymentEngine* pe, int tx_id) {
    if (!pe || tx_id <= 0 || tx_id > pe->transaction_count) return false;
    PaymentTransaction* tx = &pe->transactions[tx_id - 1];
    if (tx->state != TX_AUTHORIZED) return false;
    tx->state = TX_CAPTURED;
    pe->total_processed_volume += tx->total_amount;
    return true;
}

bool payment_settle(PaymentEngine* pe, int tx_id) {
    if (!pe || tx_id <= 0 || tx_id > pe->transaction_count) return false;
    PaymentTransaction* tx = &pe->transactions[tx_id - 1];
    if (tx->state != TX_CAPTURED) return false;
    tx->state = TX_SETTLED;
    tx->settled_at = (int64_t)time(NULL);
    return true;
}

bool payment_refund(PaymentEngine* pe, int tx_id, double amount) {
    if (!pe || tx_id <= 0 || tx_id > pe->transaction_count) return false;
    PaymentTransaction* tx = &pe->transactions[tx_id - 1];
    if (tx->state != TX_SETTLED && tx->state != TX_CAPTURED) return false;
    if (amount <= 0.0 || amount > tx->total_amount) return false;
    /* Full refund transitions state, partial keeps settled */
    if (amount >= tx->total_amount * 0.999) {
        tx->state = TX_REFUNDED;
    }
    tx->total_amount -= amount;
    pe->total_processed_volume -= amount;
    return true;
}

bool payment_void(PaymentEngine* pe, int tx_id) {
    if (!pe || tx_id <= 0 || tx_id > pe->transaction_count) return false;
    PaymentTransaction* tx = &pe->transactions[tx_id - 1];
    if (tx->state != TX_AUTHORIZED) return false;
    tx->state = TX_VOIDED;
    return true;
}

PaymentTransaction* payment_get_transaction(const PaymentEngine* pe, int tx_id) {
    if (!pe || tx_id <= 0 || tx_id > pe->transaction_count) return NULL;
    return (PaymentTransaction*)&pe->transactions[tx_id - 1];
}

int payment_get_transactions_by_order(const PaymentEngine* pe, int order_id,
                                       int* tx_indices, int max_results) {
    if (!pe || !tx_indices) return 0;
    int found = 0;
    for (int i = 0; i < pe->transaction_count && found < max_results; i++) {
        if (pe->transactions[i].order_id == order_id)
            tx_indices[found++] = i;
    }
    return found;
}

/* --- L7: Split Payment Processing ---
 * An order may be paid with multiple methods (e.g., gift card + credit card).
 * Validates that split amounts sum to total.
 */
bool payment_process_split(PaymentEngine* pe, const SplitPayment* sp) {
    if (!pe || !sp || sp->split_count <= 0) return false;
    double sum = 0.0;
    for (int i = 0; i < sp->split_count; i++) {
        sum += sp->splits[i].amount;
    }
    /* Allow small rounding tolerance */
    if (fabs(sum - sp->total) > 0.02) return false;

    for (int i = 0; i < sp->split_count; i++) {
        PaymentTransaction* tx = payment_authorize(
            pe, sp->order_id, sp->splits[i].method,
            sp->splits[i].amount, "USD", sp->splits[i].last_four);
        if (!tx) return false;
        payment_capture(pe, tx->id);
        payment_settle(pe, tx->id);
    }
    return true;
}

/* --- L7: Payment Report Generation --- */

void payment_generate_report(const PaymentEngine* pe, PaymentReport* report) {
    if (!pe || !report) return;
    memset(report, 0, sizeof(PaymentReport));

    for (int i = 0; i < pe->transaction_count; i++) {
        const PaymentTransaction* tx = &pe->transactions[i];
        report->transaction_count++;
        switch (tx->state) {
            case TX_SETTLED:
                report->success_count++;
                report->total_volume += tx->total_amount;
                break;
            case TX_REFUNDED:
                report->refund_count++;
                break;
            case TX_DECLINED:
            case TX_ERROR:
                report->decline_count++;
                break;
            default: break;
        }
    }
    if (report->success_count > 0)
        report->avg_transaction_value = report->total_volume / report->success_count;
    if (report->transaction_count > 0) {
        report->refund_rate = (double)report->refund_count / report->transaction_count * 100.0;
        report->decline_rate = (double)report->decline_count / report->transaction_count * 100.0;
    }
    report->total_fees = pe->total_fees_collected;
}

void payment_print_report(const PaymentReport* report) {
    if (!report) return;
    printf("=== Payment Report ===\n");
    printf("  Total Volume:     $%.2f\n", report->total_volume);
    printf("  Total Fees:       $%.2f\n", report->total_fees);
    printf("  Transactions:     %d\n", report->transaction_count);
    printf("  Successful:       %d\n", report->success_count);
    printf("  Refunded:         %d (%.1f%%)\n", report->refund_count, report->refund_rate);
    printf("  Declined:         %d (%.1f%%)\n", report->decline_count, report->decline_rate);
    printf("  Avg TX Value:     $%.2f\n", report->avg_transaction_value);
}

void payment_print_summary(const PaymentEngine* pe) {
    if (!pe) return;
    printf("Payment Engine: %d currencies, %d tax rules, %d gateways\n",
           pe->currency_count, pe->tax_rule_count, pe->gateway_count);
    printf("  %d transactions, $%.2f processed\n",
           pe->transaction_count, pe->total_processed_volume);
}

void payment_engine_destroy(PaymentEngine* pe) { free(pe); }