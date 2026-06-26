#ifndef ECOMMERCE_PAYMENT_H
#define ECOMMERCE_PAYMENT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "ecommerce_core.h"

#define PAYMENT_MAX_TRANSACTIONS 4096
#define PAYMENT_MAX_CURRENCIES   32
#define PAYMENT_MAX_TAX_RULES    64
#define PAYMENT_MAX_PAYMENT_METHODS 16
#define PAYMENT_CURRENCY_CODE_LEN 4

/* L1: Payment method types — covers all major e-commerce payment channels */
typedef enum {
    PM_CREDIT_CARD,
    PM_DEBIT_CARD,
    PM_PAYPAL,
    PM_BANK_TRANSFER,
    PM_CRYPTO,
    PM_GIFT_CARD,
    PM_CASH_ON_DELIVERY,
    PM_BNPL,
    PM_COUNT
} PaymentMethodType;

/* L1: Transaction state machine — full payment lifecycle */
typedef enum {
    TX_AUTHORIZED,
    TX_CAPTURED,
    TX_SETTLED,
    TX_REFUNDED,
    TX_VOIDED,
    TX_DECLINED,
    TX_ERROR,
    TX_COUNT
} TransactionState;

/* L1: Tax types for multi-jurisdiction support */
typedef enum {
    TAX_VAT,
    TAX_GST,
    TAX_SALES,
    TAX_EXCISE,
    TAX_DIGITAL,
    TAX_COUNT
} TaxType;

/* L1: Currency with exchange rate — basis for multi-currency support */
typedef struct {
    char   code[PAYMENT_CURRENCY_CODE_LEN];
    char   name[32];
    double rate_to_usd;
    int    decimal_places;
    char   symbol[8];
} Currency;

/* L1: Tax rule — jurisdiction-specific taxation */
typedef struct {
    char     region[64];
    TaxType  type;
    double   rate;
    double   min_taxable;
    bool     inclusive;
    char     description[128];
} TaxRule;

/* L1: Payment transaction — immutable record of a payment event */
typedef struct {
    int              id;
    int              order_id;
    PaymentMethodType method;
    TransactionState state;
    double           amount;
    double           tax_amount;
    double           tip_amount;
    double           total_amount;
    char             currency_code[PAYMENT_CURRENCY_CODE_LEN];
    char             gateway_ref[64];
    char             last_four[5];
    int64_t          created_at;
    int64_t          settled_at;
    bool             is_recurring;
} PaymentTransaction;

/* L3: Payment gateway abstraction — adapter pattern for multiple providers */
typedef struct {
    char              name[64];
    PaymentMethodType supported_methods[PAYMENT_MAX_PAYMENT_METHODS];
    int               method_count;
    double            fee_percent;
    double            fee_fixed;
    bool              supports_refund;
    bool              supports_recurring;
    int               success_count;
    int               failure_count;
    double            total_processed;
} PaymentGateway;

/* L3: Payment processing engine — aggregates all payment logic */
typedef struct {
    PaymentTransaction transactions[PAYMENT_MAX_TRANSACTIONS];
    int                transaction_count;
    Currency           currencies[PAYMENT_MAX_CURRENCIES];
    int                currency_count;
    TaxRule            tax_rules[PAYMENT_MAX_TAX_RULES];
    int                tax_rule_count;
    PaymentGateway     gateways[PAYMENT_MAX_PAYMENT_METHODS];
    int                gateway_count;
    double             total_processed_volume;
    double             total_fees_collected;
} PaymentEngine;

/* L1: Split payment support */
typedef struct {
    PaymentMethodType method;
    double            amount;
    char              last_four[5];
} PaymentSplit;

typedef struct {
    int              order_id;
    PaymentSplit     splits[8];
    int              split_count;
    double           total;
} SplitPayment;

typedef struct {
    double   total_volume;
    double   total_fees;
    int      transaction_count;
    int      success_count;
    int      refund_count;
    int      decline_count;
    double   avg_transaction_value;
    double   refund_rate;
    double   decline_rate;
} PaymentReport;

/* --- API Declarations --- */
PaymentEngine* payment_engine_create(void);
int    payment_currency_add(PaymentEngine* pe, const char* code, const char* name,
                            double rate_to_usd, int decimals, const char* symbol);
const Currency* payment_currency_find(const PaymentEngine* pe, const char* code);
double payment_convert_currency(const PaymentEngine* pe, double amount,
                                 const char* from, const char* to);
bool   payment_detect_arbitrage(const PaymentEngine* pe, double* max_profit_pct);

int    payment_tax_add_rule(PaymentEngine* pe, const char* region, TaxType type,
                             double rate, double min_taxable, bool inclusive);
const TaxRule* payment_tax_find_rule(const PaymentEngine* pe, const char* region, TaxType type);
double payment_tax_calculate(const PaymentEngine* pe, const char* region,
                              double subtotal, double shipping);
double payment_composite_tax(const PaymentEngine* pe, const char* region,
                              double subtotal, double shipping, TaxType* applied, int* count);

int     payment_gateway_register(PaymentEngine* pe, const char* name,
                                  double fee_pct, double fee_fixed,
                                  bool refund, bool recurring);
double  payment_calculate_fee(const PaymentGateway* gw, double amount);

PaymentTransaction* payment_authorize(PaymentEngine* pe, int order_id,
                                       PaymentMethodType method, double amount,
                                       const char* currency_code, const char* last_four);
bool  payment_capture(PaymentEngine* pe, int tx_id);
bool  payment_settle(PaymentEngine* pe, int tx_id);
bool  payment_refund(PaymentEngine* pe, int tx_id, double amount);
bool  payment_void(PaymentEngine* pe, int tx_id);
PaymentTransaction* payment_get_transaction(const PaymentEngine* pe, int tx_id);
int   payment_get_transactions_by_order(const PaymentEngine* pe, int order_id,
                                         int* tx_indices, int max_results);
bool  payment_process_split(PaymentEngine* pe, const SplitPayment* sp);

void   payment_generate_report(const PaymentEngine* pe, PaymentReport* report);
void   payment_print_report(const PaymentReport* report);
void   payment_print_summary(const PaymentEngine* pe);
void   payment_engine_destroy(PaymentEngine* pe);

const char* payment_method_name(PaymentMethodType m);
const char* transaction_state_name(TransactionState s);
const char* tax_type_name(TaxType t);

#endif
