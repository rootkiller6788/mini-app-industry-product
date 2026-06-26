#ifndef FINTECH_CORE_H
#define FINTECH_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FIN_MAX_NAME            128
#define FIN_MAX_ACCOUNTS        4096
#define FIN_MAX_TRANSACTIONS    16384
#define FIN_MAX_SCHEDULE        360
#define FIN_MAX_BLOCKS          1024
#define FIN_MERKLE_DEPTH        16
#define FIN_MAX_FX_PAIRS        128
#define FIN_MAX_RULES           64

/* ================================================================
 * L1: Core Data Models — Double-Entry Bookkeeping
 * Reference: Pacioli (1494) "Summa de arithmetica", ISO 20022
 * ================================================================ */

typedef enum {
    ACCT_ASSET,
    ACCT_LIABILITY,
    ACCT_EQUITY,
    ACCT_REVENUE,
    ACCT_EXPENSE
} AccountType;

typedef enum {
    TXN_DEPOSIT,
    TXN_WITHDRAWAL,
    TXN_TRANSFER,
    TXN_PAYMENT,
    TXN_REFUND,
    TXN_FEE,
    TXN_INTEREST,
    TXN_COUNT
} TransactionType;

typedef enum {
    LOAN_FIXED,
    LOAN_VARIABLE,
    LOAN_BALLOON,
    LOAN_INTEREST_ONLY,
    LOAN_COUNT
} LoanType;

typedef enum {
    RISK_LOW,
    RISK_MEDIUM,
    RISK_HIGH,
    RISK_CRITICAL,
    RISK_COUNT
} RiskLevel;

typedef enum {
    FRAUD_AMOUNT_ANOMALY,
    FRAUD_VELOCITY,
    FRAUD_GEOGRAPHY,
    FRAUD_DEVICE,
    FRAUD_PATTERN,
    FRAUD_COUNT
} FraudRuleType;

typedef struct {
    int         id;
    char        name[FIN_MAX_NAME];
    AccountType type;
    double      balance;
    int64_t     created_at;
    int64_t     last_activity;
    bool        active;
    bool        frozen;
    int         overdraft_limit;
    double      interest_rate;
    char        currency[8];
} Account;

typedef struct {
    int             id;
    TransactionType type;
    int             debit_account_id;
    int             credit_account_id;
    double          amount;
    int64_t         timestamp;
    char            description[FIN_MAX_NAME];
    char            reference[FIN_MAX_NAME];
    bool            reconciled;
} Transaction;

/* L3: Ledger System */
typedef struct {
    Account     accounts[FIN_MAX_ACCOUNTS];
    int         account_count;
    Transaction transactions[FIN_MAX_TRANSACTIONS];
    int         transaction_count;
    double      total_assets;
    double      total_liabilities;
    double      total_equity;
    int         next_account_id;
    int         next_txn_id;
} Ledger;

/* L5: Loan Amortization — Equal installment payment formula:
 * PMT = P * r * (1+r)^n / ((1+r)^n - 1) where P=principal, r=rate/period, n=periods */

typedef struct {
    int         id;
    LoanType    type;
    int         account_id;
    double      principal;
    double      annual_rate;
    int         total_periods;
    int         periods_paid;
    double      monthly_payment;
    double      remaining_balance;
    int64_t     start_date;
    int64_t     next_payment_date;
    bool        active;
} Loan;

typedef struct {
    int         period;
    double      payment;
    double      interest;
    double      principal;
    double      remaining;
} AmortizationRow;

typedef struct {
    Loan        loans[FIN_MAX_ACCOUNTS];
    int         loan_count;
    double      total_outstanding;
} LoanBook;

/* L5: Risk Scoring — FICO-inspired credit scoring model */
typedef struct {
    int         score;          /* 300-850 */
    RiskLevel   level;
    double      payment_history_weight;
    double      debt_burden_weight;
    double      credit_age_weight;
    double      credit_mix_weight;
    double      recent_inquiry_weight;
    char        grade;          /* A-F */
} CreditScore;

typedef struct {
    int         account_id;
    int         on_time_payments;
    int         late_payments;
    int         missed_payments;
    double      credit_utilization;
    int         credit_age_months;
    int         recent_inquiries;
    int         total_accounts;
    CreditScore score;
} CreditProfile;

/* L8: Blockchain Core — Simplified PoW chain with Merkle trees */
typedef struct {
    char        data[FIN_MAX_NAME];
    char        hash[65];       /* SHA-256 hex */
} MerkleLeaf;

typedef struct {
    char        hash[65];
    MerkleLeaf* leaves;
    int         leaf_count;
    int         tree_depth;
    char        merkle_root[65];
} MerkleTree;

typedef struct {
    int         index;
    int64_t     timestamp;
    char        data[FIN_MAX_NAME];
    char        prev_hash[65];
    char        hash[65];
    uint64_t    nonce;
    int         difficulty;
    char        merkle_root[65];
} Block;

typedef struct {
    Block       blocks[FIN_MAX_BLOCKS];
    int         block_count;
    int         difficulty;
    bool        is_valid;
} Blockchain;

/* L7: FX (Foreign Exchange) & Currency */
typedef struct {
    char        base_currency[8];
    char        quote_currency[8];
    double      rate;
    int64_t     timestamp;
    char        source[FIN_MAX_NAME];
} FXRate;

typedef struct {
    FXRate      rates[FIN_MAX_FX_PAIRS];
    int         rate_count;
    char        home_currency[8];
} FXMarket;

/* L7: Fraud Detection Engine */
typedef struct {
    FraudRuleType type;
    char        name[FIN_MAX_NAME];
    double      threshold;
    bool        enabled;
    int         lookback_transactions;
    double      (*check_fn)(Ledger* l, int account_id, void* ctx);
} FraudRule;

typedef struct {
    FraudRule   rules[FIN_MAX_RULES];
    int         rule_count;
    int         alerts_triggered;
    int         accounts_frozen;
} FraudEngine;

/* --- Ledger API --- */
Ledger*   ledger_create(void);
Account*  ledger_create_account(Ledger* l, const char* name, AccountType type);
Account*  ledger_find_account(Ledger* l, int id);
bool      ledger_deposit(Ledger* l, int account_id, double amount);
bool      ledger_withdraw(Ledger* l, int account_id, double amount);
bool      ledger_transfer(Ledger* l, int from_id, int to_id, double amount);
bool      ledger_verify_balance(Ledger* l);
double    ledger_calculate_interest(double principal, double rate, int periods);
bool      ledger_apply_interest(Ledger* l, int account_id, double rate);
double    ledger_get_total_balance(const Ledger* l, AccountType type);
void      ledger_print_summary(const Ledger* l);
void      ledger_destroy(Ledger* l);

/* --- Loan API --- */
LoanBook* loanbook_create(void);
Loan*     loanbook_create_loan(LoanBook* lb, int account_id, LoanType type,
                               double principal, double annual_rate, int months);
int       loanbook_generate_schedule(const Loan* loan, AmortizationRow* rows, int max_rows);
bool      loanbook_make_payment(LoanBook* lb, Ledger* l, int loan_id);
double    loanbook_npv(const Loan* loan, double discount_rate);
void      loanbook_print_loan(const Loan* loan);
void      loanbook_print_schedule(const AmortizationRow* rows, int count);
void      loanbook_destroy(LoanBook* lb);

/* --- Credit Scoring API --- */
CreditProfile* credit_create_profile(int account_id);
void     credit_record_payment(CreditProfile* cp, bool on_time);
int      credit_calculate_score(CreditProfile* cp);
RiskLevel credit_score_to_risk(int score);
char     credit_score_to_grade(int score);
void     credit_print_profile(const CreditProfile* cp);

/* --- Blockchain API --- */
MerkleTree* merkle_create(void);
int      merkle_add_leaf(MerkleTree* mt, const char* data);
void     merkle_compute_root(MerkleTree* mt);
bool     merkle_verify_leaf(const MerkleTree* mt, int index, const char* expected_data);

Blockchain* blockchain_create(int difficulty);
int      blockchain_add_block(Blockchain* bc, const char* data);
bool     blockchain_validate(const Blockchain* bc);
void     blockchain_print_chain(const Blockchain* bc);
void     blockchain_destroy(Blockchain* bc);
void     merkle_destroy(MerkleTree* mt);

/* --- FX API --- */
FXMarket* fx_create(const char* home_currency);
int      fx_add_rate(FXMarket* fx, const char* base, const char* quote, double rate);
double   fx_convert(const FXMarket* fx, double amount, const char* from, const char* to);
double   fx_get_rate(const FXMarket* fx, const char* base, const char* quote);
void     fx_print_rates(const FXMarket* fx);
void     fx_destroy(FXMarket* fx);

/* --- Fraud Detection API --- */
FraudEngine* fraud_create(void);
int      fraud_add_rule(FraudEngine* fe, FraudRuleType type, const char* name,
                        double threshold, int lookback);
int      fraud_scan_account(FraudEngine* fe, Ledger* l, int account_id);
bool     fraud_freeze_account(FraudEngine* fe, Ledger* l, int account_id);
void     fraud_print_alerts(const FraudEngine* fe);
void     fraud_destroy(FraudEngine* fe);

/* --- Utility --- */
const char* account_type_name(AccountType t);
const char* txn_type_name(TransactionType t);
const char* loan_type_name(LoanType t);
const char* risk_level_name(RiskLevel r);

/* ================================================================
 * L5: Modern Portfolio Theory — Markowitz Mean-Variance Optimization
 * Reference: Markowitz (1952) "Portfolio Selection", Journal of Finance
 *            MIT 15.433: Investments
 * ================================================================ */

#define FIN_MAX_ASSETS      64
#define FIN_MAX_PORTFOLIOS  128
#define FIN_MONTE_CARLO_ITERS 10000

typedef struct {
    char   symbol[16];
    char   name[FIN_MAX_NAME];
    double expected_return;      /* annualized */
    double volatility;           /* annualized std dev */
    double price;
    int    asset_class;          /* 0=equity, 1=bond, 2=cash, 3=commodity */
} Asset;

typedef struct {
    double weights[FIN_MAX_ASSETS];
    int    asset_indices[FIN_MAX_ASSETS];
    int    num_assets;
    double expected_return;
    double volatility;
    double sharpe_ratio;
} Portfolio;

typedef struct {
    Asset   assets[FIN_MAX_ASSETS];
    int     asset_count;
    double  cov_matrix[FIN_MAX_ASSETS][FIN_MAX_ASSETS];
} Market;

void  market_init(Market* m);
int   market_add_asset(Market* m, const char* symbol, const char* name,
                        double er, double vol, double price, int asset_class);
void  market_compute_covariance(Market* m, double correlations[][FIN_MAX_ASSETS]);
void  market_optimize_portfolio(Market* m, double risk_free_rate,
                                 Portfolio* result);
void  market_efficient_frontier(Market* m, double risk_free_rate,
                                 Portfolio* frontier, int* num_points, int max_points);
double portfolio_expected_return(const Portfolio* p, const Market* m);
double portfolio_volatility(const Portfolio* p, const Market* m);
double portfolio_sharpe(const Portfolio* p, double risk_free, const Market* m);

/* ================================================================
 * L5: Black-Scholes Option Pricing
 * C = S·N(d1) - K·e^(-rT)·N(d2)
 * P = K·e^(-rT)·N(-d2) - S·N(-d1)
 * Reference: Black & Scholes (1973), Merton (1973)
 * ================================================================ */

typedef enum {
    OPTION_CALL,
    OPTION_PUT,
} OptionType;

typedef struct {
    OptionType type;
    double     spot;
    double     strike;
    double     time_to_expiry;  /* years */
    double     risk_free_rate;
    double     volatility;
    double     dividend_yield;
} OptionParams;

double bs_price(const OptionParams* params);
double bs_delta(const OptionParams* params);
double bs_gamma(const OptionParams* params);
double bs_vega(const OptionParams* params);
double bs_theta(const OptionParams* params);
double bs_implied_volatility(const OptionParams* params, double market_price,
                               int max_iter, double tolerance);

/* ================================================================
 * L7: Value at Risk (VaR) — Monte Carlo, Historical, Parametric
 * Reference: Jorion (2006) "Value at Risk" 3rd ed.
 *            Basel Committee on Banking Supervision
 * ================================================================ */

typedef enum {
    VAR_METHOD_PARAMETRIC,
    VAR_METHOD_HISTORICAL,
    VAR_METHOD_MONTE_CARLO,
} VaRMethod;

typedef struct {
    VaRMethod method;
    double    confidence_level;  /* 0.95, 0.99 */
    int       horizon_days;
    double    portfolio_value;
    double    var_result;
    double    cvar_result;       /* Conditional VaR / Expected Shortfall */
    double    max_loss;
    double    min_loss;
} VaRResult;

double var_parametric(double portfolio_value, double volatility,
                       double confidence, int horizon_days);
double var_historical(const double* returns, int n, double confidence);
double var_monte_carlo(double portfolio_value, double drift, double volatility,
                         int horizon_days, double confidence, int num_simulations);
void   var_compute_full(VaRResult* result, const double* returns, int n,
                          double portfolio_value, double volatility);

/* ================================================================
 * L5: Fixed Income — Bond Pricing & Yield Curves
 * Reference: Fabozzi (2007) "Fixed Income Analysis" CFA Institute
 *            Nelson & Siegel (1987) yield curve model
 * ================================================================ */

#define FIN_MAX_CASHFLOWS    120

typedef struct {
    double coupon_rate;
    double face_value;
    int    periods_remaining;
    int    frequency;           /* payments per year */
    double yield_to_maturity;
    double price;
    double duration;           /* Macaulay duration */
    double modified_duration;
    double convexity;
} Bond;

typedef struct {
    double maturities[12];     /* years */
    double yields[12];
    int    num_points;
    double beta0, beta1, beta2, tau;  /* Nelson-Siegel parameters */
} YieldCurve;

double bond_price(const Bond* b);
double bond_duration(const Bond* b);
double bond_convexity(const Bond* b);
double bond_ytm_from_price(const Bond* b, double market_price,
                             int max_iter, double tolerance);
void yield_curve_bootstrap(YieldCurve* yc, const double* maturities,
                             const double* yields, int n);
double yield_curve_interpolate(const YieldCurve* yc, double maturity);
void yield_curve_nelson_siegel(YieldCurve* yc,
                                 double beta0, double beta1, double beta2, double tau);
double yield_curve_discount_factor(const YieldCurve* yc, double t);

/* ================================================================
 * L6: Payment Network Simulation
 * Simulates inter-bank payment clearing (SWIFT/ACH-style)
 * Reference: ISO 20022, Fedwire Funds Service
 * ================================================================ */

#define FIN_MAX_BANKS        32
#define FIN_MAX_PAYMENTS      4096

typedef struct {
    int    bank_id;
    char   name[FIN_MAX_NAME];
    char   routing_number[16];
    double balance;
    double sent_total;
    double received_total;
    int    payment_count;
} Bank;

typedef struct {
    int    id;
    int    from_bank;
    int    to_bank;
    double amount;
    int64_t timestamp;
    int    status;          /* 0=pending, 1=cleared, 2=failed */
    char   reference[FIN_MAX_NAME];
} Payment;

typedef struct {
    Bank     banks[FIN_MAX_BANKS];
    int      bank_count;
    Payment  payments[FIN_MAX_PAYMENTS];
    int      payment_count;
    double   total_settled;
    int      pending_count;
} PaymentNetwork;

PaymentNetwork* pn_create(void);
int    pn_add_bank(PaymentNetwork* pn, const char* name, const char* routing);
int    pn_create_payment(PaymentNetwork* pn, int from, int to,
                           double amount, const char* ref);
bool   pn_settle_payment(PaymentNetwork* pn, int payment_id);
void   pn_net_settle(PaymentNetwork* pn);
void   pn_print_summary(const PaymentNetwork* pn);
void   pn_destroy(PaymentNetwork* pn);

/* ================================================================
 * L8: Algorithmic Trading — Limit Order Book
 * Reference: O'Hara (1995) "Market Microstructure Theory"
 *            Biais, Foucault & Moinas (2015) equilibrium LOB
 * ================================================================ */

#define FIN_MAX_ORDERS       8192

typedef enum {
    ORDER_SIDE_BUY,
    ORDER_SIDE_SELL,
} OrderSide;

typedef enum {
    ORDER_TYPE_LIMIT,
    ORDER_TYPE_MARKET,
} OrderTypeLOB;

typedef struct {
    int       id;
    OrderSide side;
    OrderTypeLOB type;
    double    price;
    int       quantity;
    int64_t   timestamp;
    int       trader_id;
    bool      active;
} LOBOrder;

typedef struct {
    LOBOrder orders[FIN_MAX_ORDERS];
    int      order_count;
    double   best_bid;
    double   best_ask;
    double   spread;
    double   mid_price;
    int      bid_depth;   /* total shares at best bid */
    int      ask_depth;   /* total shares at best ask */
    double   last_price;
    int      total_volume;
} LimitOrderBook;

void   lob_init(LimitOrderBook* lob);
int    lob_add_order(LimitOrderBook* lob, OrderSide side, OrderTypeLOB type,
                      double price, int qty, int trader_id);
int    lob_match(LimitOrderBook* lob);
int    lob_cancel_order(LimitOrderBook* lob, int order_id);
void   lob_update_quotes(LimitOrderBook* lob);
void   lob_print_book(const LimitOrderBook* lob, int levels);
double lob_vwap(const LimitOrderBook* lob);

/* ================================================================
 * L7: Regulatory Reporting — Basel III Capital Ratios
 * Reference: Basel III: A global regulatory framework (BCBS 2011)
 * ================================================================ */

typedef struct {
    double tier1_capital;
    double tier2_capital;
    double total_rwa;        /* Risk-Weighted Assets */
    double cet1_ratio;       /* Common Equity Tier 1 / RWA */
    double tier1_ratio;      /* Tier 1 Capital / RWA */
    double total_car;        /* Total Capital Adequacy Ratio */
    double leverage_ratio;
    double lcr;              /* Liquidity Coverage Ratio */
    double nsfr;             /* Net Stable Funding Ratio */
    bool   meets_basel_iii;
} BaselIIIReport;

void   basel3_calculate(BaselIIIReport* r, double cet1, double at1,
                          double tier2, double rwa, double exposure,
                          double hqla, double net_cash_outflow);
void   basel3_assess_loan(BaselIIIReport* r, const Loan* loan);

/* ================================================================
 * L9: Digital Currency / CBDC
 * Reference: BIS (2020) "Central bank digital currencies: foundational principles"
 *            MIT Digital Currency Initiative
 * ================================================================ */

typedef struct {
    char      currency_code[8];
    char      issuer[FIN_MAX_NAME];
    double    total_supply;
    double    amount;
    int64_t   issued_at;
    int64_t   expires_at;
    char      owner_id[FIN_MAX_NAME];
    char      tx_hash[65];
    bool      is_programmable;  /* Smart contract enabled */
    double    programmatic_limit;
    int       expiry_days;
} CBDCToken;

void cbdc_issue(CBDCToken* token, const char* currency, const char* issuer,
                 double amount, const char* owner, bool programmable);
bool cbdc_transfer(CBDCToken* from, CBDCToken* to, double amount);
bool cbdc_verify(const CBDCToken* token);
double cbdc_money_supply(const CBDCToken* tokens, int count);

#endif
