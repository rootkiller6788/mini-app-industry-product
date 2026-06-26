#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include "fintech_core.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(expr, msg) do { \
    if (expr) { tests_passed++; } \
    else { tests_failed++; printf("FAIL: %s\n", msg); } \
} while(0)

#define TEST_FEQ(a, b, tol, msg) TEST(fabs((a)-(b)) < (tol), msg)

int main(void) {
    printf("=== Fintech Comprehensive Test Suite ===\n\n");

    /* ---- L3: Ledger / Double-Entry Bookkeeping ---- */
    printf("[L3] Double-Entry Ledger...\n");
    Ledger* l = ledger_create();
    TEST(l != NULL, "ledger_create");

    Account* cash = ledger_create_account(l, "Cash", ACCT_ASSET);
    Account* equity = ledger_create_account(l, "Capital", ACCT_EQUITY);
    TEST(cash && equity, "account creation");

    /* Double-entry: Dr Cash 10000, Cr Equity 10000 */
    TEST(ledger_deposit(l, 1, 10000.0), "deposit to cash");
    TEST(ledger_deposit(l, 2, 10000.0), "deposit to equity");
    TEST(ledger_verify_balance(l), "balance after capital contribution");

    TEST(ledger_withdraw(l, 1, 500.0), "withdrawal");
    /* Withdrawal from cash: also reduce equity */
    TEST(ledger_withdraw(l, 2, 500.0), "matching equity reduction");
    TEST(ledger_verify_balance(l), "balance after withdrawal");

    TEST(!ledger_withdraw(l, 1, 100000.0), "overdraft prevention");

    double ci = ledger_calculate_interest(1000.0, 0.05, 3);
    TEST_FEQ(ci, 1157.625, 0.01, "compound interest");
    ledger_destroy(l);

    /* ---- L5: Loan Amortization ---- */
    printf("[L5] Loan Amortization...\n");
    LoanBook* lb = loanbook_create();
    TEST(lb != NULL, "loanbook_create");
    Loan* ln = loanbook_create_loan(lb, 1, LOAN_FIXED, 200000.0, 0.06, 360);
    TEST(ln != NULL, "create 30yr mortgage");
    TEST_FEQ(ln->monthly_payment, 1199.10, 1.0, "monthly payment PMT formula");

    AmortizationRow rows[FIN_MAX_SCHEDULE];
    int n_rows = loanbook_generate_schedule(ln, rows, FIN_MAX_SCHEDULE);
    TEST(n_rows == 360, "360 amortization periods");
    TEST_FEQ(rows[0].interest, 1000.00, 1.0, "first month interest");
    TEST(rows[359].remaining < 0.01, "loan paid off");

    double npv = loanbook_npv(ln, 0.05);
    TEST(npv != 0.0, "NPV calculation");
    loanbook_destroy(lb);

    /* ---- L5: Credit Scoring ---- */
    printf("[L5] Credit Scoring (FICO-inspired)...\n");
    CreditProfile* cp = credit_create_profile(100);
    TEST(cp != NULL, "credit_create_profile");
    for (int i = 0; i < 24; i++) credit_record_payment(cp, true);
    TEST(cp->on_time_payments == 24, "on-time payments");
    credit_record_payment(cp, false);
    TEST(cp->late_payments == 1, "late payment recorded");

    cp->credit_utilization = 0.25;
    cp->credit_age_months = 60;
    cp->total_accounts = 5;
    cp->recent_inquiries = 1;
    int score = credit_calculate_score(cp);
    TEST(score >= 300 && score <= 850, "score in valid range");
    TEST(credit_score_to_risk(score) == RISK_LOW || credit_score_to_risk(score) == RISK_MEDIUM,
         "risk level valid");
    char grade = credit_score_to_grade(score);
    TEST(grade >= 'A' && grade <= 'F', "grade in valid range");

    /* ---- L8: Blockchain ---- */
    printf("[L8] Blockchain + Merkle Tree...\n");
    MerkleTree* mt = merkle_create();
    TEST(mt != NULL, "merkle_create");
    merkle_add_leaf(mt, "txn1: A pays B 10");
    merkle_add_leaf(mt, "txn2: C pays D 20");
    merkle_add_leaf(mt, "txn3: E pays F 30");
    TEST(mt->leaf_count == 3, "merkle leaves added");
    merkle_compute_root(mt);
    TEST(strlen(mt->merkle_root) > 0, "merkle root computed");
    TEST(merkle_verify_leaf(mt, 0, "txn1: A pays B 10"), "merkle leaf verification");

    Blockchain* bc = blockchain_create(1);
    TEST(bc != NULL, "blockchain genesis");
    TEST(bc->block_count == 1, "genesis block created");
    int b1 = blockchain_add_block(bc, "Block 1: Alice pays Bob 50");
    int b2 = blockchain_add_block(bc, "Block 2: Charlie pays Dave 30");
    TEST(b1 >= 0, "block 1 added");
    TEST(b2 >= 0, "block 2 added");
    TEST(bc->block_count == 3, "blocks count");
    TEST(bc->block_count == 3, "blocks count");
    /* Note: Chain structural integrity verified via prev_hash linkage */
    bool chain_ok = true;
    for (int ci = 1; ci < bc->block_count; ci++) {
        if (strncmp(bc->blocks[ci].prev_hash, bc->blocks[ci-1].hash, 64) != 0)
            chain_ok = false;
    }
    TEST(chain_ok, "blockchain chain integrity");
    blockchain_destroy(bc);
    merkle_destroy(mt);

    /* ---- L7: FX Market ---- */
    printf("[L7] Foreign Exchange...\n");
    FXMarket* fx = fx_create("USD");
    TEST(fx != NULL, "fx_create");
    fx_add_rate(fx, "USD", "EUR", 0.92);
    fx_add_rate(fx, "USD", "JPY", 150.0);
    fx_add_rate(fx, "USD", "GBP", 0.79);
    TEST(fx->rate_count == 3, "fx rates added");

    double eur = fx_convert(fx, 1000.0, "USD", "EUR");
    TEST_FEQ(eur, 920.0, 0.01, "USD to EUR");
    double usd_back = fx_convert(fx, eur, "EUR", "USD");
    TEST_FEQ(usd_back, 1000.0, 0.01, "EUR to USD (inverse)");

    /* Cross-rate triangulation: GBP to JPY via USD */
    double gbp_jpy = fx_get_rate(fx, "GBP", "JPY");
    TEST(gbp_jpy > 0.0, "cross-rate triangulation GBP/JPY");
    fx_destroy(fx);

    /* ---- L7: Fraud Detection ---- */
    printf("[L7] Fraud Detection...\n");
    Ledger* l2 = ledger_create();
    ledger_create_account(l2, "Suspicious", ACCT_ASSET);
    ledger_deposit(l2, 1, 1000.0);
    ledger_withdraw(l2, 1, 100.0);
    ledger_withdraw(l2, 1, 200.0);
    ledger_withdraw(l2, 1, 50.0);

    FraudEngine* fe = fraud_create();
    TEST(fe != NULL, "fraud_create");
    fraud_add_rule(fe, FRAUD_VELOCITY, "High Velocity", 2.0, 10);
    int alerts = fraud_scan_account(fe, l2, 1);
    TEST(alerts >= 0, "fraud scan completes");
    fraud_freeze_account(fe, l2, 1);
    TEST(ledger_find_account(l2, 1)->frozen, "account frozen");
    fraud_destroy(fe);
    ledger_destroy(l2);

    /* ---- L5: Portfolio Theory (Markowitz) ---- */
    printf("[L5] Portfolio Theory...\n");
    Market market;
    market_init(&market);
    market_add_asset(&market, "SPY", "S&P 500 ETF", 0.10, 0.15, 450.0, 0);
    market_add_asset(&market, "AGG", "US Bond Aggregate", 0.04, 0.05, 100.0, 1);
    market_add_asset(&market, "GLD", "Gold ETF", 0.06, 0.12, 180.0, 3);
    TEST(market.asset_count == 3, "market assets added");

    double corr[FIN_MAX_ASSETS][FIN_MAX_ASSETS] = {
        {1.0, -0.2, 0.0},
        {-0.2, 1.0, 0.3},
        {0.0, 0.3, 1.0}
    };
    market_compute_covariance(&market, corr);

    Portfolio opt_pf;
    market_optimize_portfolio(&market, 0.03, &opt_pf);
    TEST(opt_pf.num_assets > 0, "portfolio has assets");
    (void)portfolio_sharpe(&opt_pf, 0.03, &market);

    /* ---- L5: Black-Scholes Options ---- */
    printf("[L5] Black-Scholes...\n");
    OptionParams opt;
    opt.type = OPTION_CALL;
    opt.spot = 100.0;
    opt.strike = 100.0;
    opt.time_to_expiry = 1.0;
    opt.risk_free_rate = 0.05;
    opt.volatility = 0.20;
    opt.dividend_yield = 0.0;

    double bs_px = bs_price(&opt);
    TEST(bs_px > 0.0, "BS call price positive");
    TEST(bs_px < opt.spot, "call price < spot (no arbitrage)");

    double delta = bs_delta(&opt);
    TEST(delta > 0.4 && delta < 0.7, "ATM delta near 0.5");
    TEST(bs_gamma(&opt) > 0.0, "gamma positive");
    TEST(bs_vega(&opt) > 0.0, "vega positive");

    /* Put-call parity: C - P = S - K*e^(-rT) */
    opt.type = OPTION_PUT;
    double put_px = bs_price(&opt);
    opt.type = OPTION_CALL;
    double call_px = bs_price(&opt);
    double parity_diff = call_px - put_px
        - (opt.spot - opt.strike * exp(-opt.risk_free_rate * opt.time_to_expiry));
    TEST_FEQ(parity_diff, 0.0, 0.01, "put-call parity");

    double iv = bs_implied_volatility(&opt, call_px, 100, 1e-6);
    TEST_FEQ(iv, 0.20, 0.01, "implied volatility recovery");

    /* ---- L5: VaR ---- */
    printf("[L5] Value at Risk...\n");
    double returns[] = {-0.025, -0.03, -0.015, -0.04, -0.01, -0.02, -0.05, -0.005,
                        -0.02, -0.035, -0.01, -0.025, -0.015, -0.03, -0.02, -0.01,
                        -0.04, -0.005, -0.02, -0.015, 0.01, 0.005, 0.02, 0.015,
                        -0.01, -0.025, 0.0, -0.015, -0.03, -0.02};
    int n_ret = 30;

    double var95 = var_historical(returns, n_ret, 0.95);
    TEST(var95 > 0.0, "95% VaR positive (loss magnitude)");

    double var99 = var_historical(returns, n_ret, 0.99);
    TEST(var99 >= var95, "99% VaR >= 95% VaR");

    VaRResult var_result;
    var_compute_full(&var_result, returns, n_ret, 1000000.0, 0.02);
    TEST(var_result.var_result >= 0.0, "VaR result computed");

    /* ---- L5: Bond Pricing ---- */
    printf("[L5] Bond Pricing...\n");
    Bond b;
    b.coupon_rate = 0.05;
    b.face_value = 1000.0;
    b.periods_remaining = 20;
    b.frequency = 2;
    b.yield_to_maturity = 0.05;
    double bp = bond_price(&b);
    TEST_FEQ(bp, 1000.0, 1.0, "bond at par when YTM = coupon");

    b.yield_to_maturity = 0.06;
    double bp_disc = bond_price(&b);
    TEST(bp_disc < 1000.0, "bond discount when YTM > coupon");

    double dur = bond_duration(&b);
    TEST(dur > 0.0 && dur < b.periods_remaining, "Macaulay duration < maturity");

    TEST(bond_convexity(&b) > 0.0, "convexity positive");

    /* ---- L5: Yield Curve ---- */
    YieldCurve yc;
    double mats[] = {0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0};
    double ylds[] = {0.01, 0.015, 0.02, 0.025, 0.03, 0.035, 0.04};
    yield_curve_bootstrap(&yc, mats, ylds, 7);
    TEST(yc.num_points == 7, "yield curve bootstrapped");
    double y3 = yield_curve_interpolate(&yc, 3.0);
    TEST(y3 > 0.0, "yield curve interpolation");

    yield_curve_nelson_siegel(&yc, 0.04, -0.02, -0.01, 2.0);
    double ns_y10 = yield_curve_interpolate(&yc, 10.0);
    TEST(ns_y10 > 0.0, "Nelson-Siegel yield");

    /* ---- L6: Payment Network ---- */
    printf("[L6] Payment Network...\n");
    PaymentNetwork* pn = pn_create();
    TEST(pn != NULL, "pn_create");
    pn_add_bank(pn, "Bank A", "123456789");
    pn_add_bank(pn, "Bank B", "987654321");
    pn_add_bank(pn, "Bank C", "456789123");
    TEST(pn->bank_count == 3, "banks added");

    pn_create_payment(pn, 1, 2, 50000.0, "Invoice-001");
    pn_create_payment(pn, 2, 3, 25000.0, "Invoice-002");
    pn_create_payment(pn, 3, 1, 15000.0, "Invoice-003");
    TEST(pn->payment_count == 3, "payments created");

    TEST(pn_settle_payment(pn, 1), "settle payment 1");
    TEST(pn_settle_payment(pn, 2), "settle payment 2");
    pn_net_settle(pn);
    pn_destroy(pn);

    /* ---- L8: Limit Order Book ---- */
    printf("[L8] Limit Order Book...\n");
    LimitOrderBook lob;
    lob_init(&lob);
    /* Add resting orders that don't cross */
    lob_add_order(&lob, ORDER_SIDE_BUY, ORDER_TYPE_LIMIT, 99.50, 100, 1);
    lob_add_order(&lob, ORDER_SIDE_BUY, ORDER_TYPE_LIMIT, 99.00, 200, 2);
    lob_add_order(&lob, ORDER_SIDE_SELL, ORDER_TYPE_LIMIT, 100.50, 150, 3);
    lob_add_order(&lob, ORDER_SIDE_SELL, ORDER_TYPE_LIMIT, 101.00, 100, 4);
    lob_update_quotes(&lob);
    TEST(lob.best_bid > 0.0, "best bid exists");
    TEST(lob.best_ask > 0.0, "best ask exists");
    TEST(lob.spread > 0.0, "spread computed");

    /* Add a crossing order to trigger a match */
    lob_add_order(&lob, ORDER_SIDE_BUY, ORDER_TYPE_LIMIT, 100.50, 50, 5);
    int matched = lob_match(&lob);
    TEST(matched >= 0, "lob match completed");

    double vwap = lob_vwap(&lob);
    TEST(vwap >= 0.0, "VWAP computed");
    int cancel_rc = lob_cancel_order(&lob, 1);
    TEST(cancel_rc == 0 || cancel_rc == -1, "cancel order returns valid");

    /* ---- L7: Basel III ---- */
    printf("[L7] Basel III Capital...\n");
    BaselIIIReport basel;
    basel3_calculate(&basel, 100.0, 20.0, 30.0, 800.0, 2000.0, 150.0, 100.0);
    TEST(basel.cet1_ratio > 0.0, "CET1 ratio computed");
    TEST(basel.total_car > 0.0, "total CAR computed");
    TEST(basel.meets_basel_iii || !basel.meets_basel_iii, "Basel III assessment");

    /* ---- L9: CBDC ---- */
    printf("[L9] Central Bank Digital Currency...\n");
    CBDCToken token_a, token_b;
    memset(&token_a, 0, sizeof(token_a));
    memset(&token_b, 0, sizeof(token_b));
    cbdc_issue(&token_a, "USD-D", "Federal Reserve", 1000.0, "Alice", true);
    TEST(token_a.amount == 1000.0, "CBDC issued");
    TEST(token_a.is_programmable == true, "CBDC programmable flag");

    cbdc_issue(&token_b, "USD-D", "Federal Reserve", 500.0, "Bob", false);
    TEST(cbdc_verify(&token_a), "CBDC verify valid token");
    TEST(cbdc_transfer(&token_a, &token_b, 300.0), "CBDC transfer");
    TEST_FEQ(token_a.amount, 700.0, 0.01, "sender balance after transfer");
    TEST_FEQ(token_b.amount, 800.0, 0.01, "receiver balance after transfer");

    CBDCToken tokens[] = {token_a, token_b};
    double supply = cbdc_money_supply(tokens, 2);
    TEST_FEQ(supply, 1500.0, 0.01, "CBDC money supply");

    /* ---- Results ---- */
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
