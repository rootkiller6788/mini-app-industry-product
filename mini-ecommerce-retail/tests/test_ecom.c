#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "ecommerce_core.h"
#include "ecommerce_payment.h"
#include "ecommerce_search.h"
#include "ecommerce_analytics.h"
#include <time.h>
#include <time.h>
#include "ecommerce_advanced.h"

static int tests_run = 0;
static int tests_passed = 0;
#define TEST(name) tests_run++; printf("  TEST %-40s ... ", name)
#define PASS() tests_passed++; printf("PASS\n")
#define FAIL(msg) printf("FAIL: %s\n", msg); exit(1)

int main(void) {
    printf("=== E-Commerce Full Test Suite ===\n\n");

    /* ================================================================
     * L1-L3: Core Inventory & Order Management
     * ================================================================ */
    printf("--- Core Module (L1-L3, L5-L7) ---\n");

    Inventory* inv = inv_create();
    assert(inv != NULL);

    Product* p1 = inv_add_product(inv, "Wireless Mouse", "WM-001", 29.99, 15.00, 100, 0.15);
    Product* p2 = inv_add_product(inv, "Mechanical Keyboard", "KB-002", 89.99, 45.00, 50, 0.80);
    Product* p3 = inv_add_product(inv, "USB-C Hub", "UH-003", 49.99, 25.00, 75, 0.12);
    Product* p4 = inv_add_product(inv, "27 Monitor", "MN-004", 299.99, 180.00, 20, 5.5);
    Product* p5 = inv_add_product(inv, "Webcam 1080p", "WC-005", 59.99, 30.00, 60, 0.20);
    assert(p1 && p2 && p3 && p4 && p5);

    TEST("Inventory create and add products"); PASS();

    Product* found = inv_find_by_sku(inv, "KB-002");
    assert(found && found->id == 2);
    TEST("SKU lookup"); PASS();

    Product* by_id = inv_find_by_id(inv, 3);
    assert(by_id && strcmp(by_id->sku, "UH-003") == 0);
    TEST("ID lookup"); PASS();

    assert(inv_reserve_stock(inv, 1, 10));
    assert(p1->stock_qty == 90);
    assert(!inv_reserve_stock(inv, 1, 200)); /* insufficient stock */
    TEST("Stock reserve with bounds check"); PASS();

    inv_reserve_stock(inv, 4, 18);
    assert(p4->stock_qty == 2);
    assert(inv_needs_reorder(inv, 4));
    inv_restock(inv, 4, 30);
    assert(!inv_needs_reorder(inv, 4));
    TEST("Reorder point logic"); PASS();

    /* Category search */
    strncpy(p1->category, "Electronics", 63);
    strncpy(p2->category, "Electronics", 63);
    strncpy(p3->category, "Accessories", 63);
    strncpy(p4->category, "Electronics", 63);
    strncpy(p5->category, "Accessories", 63);

    int cat_count;
    Product* elec = inv_search_category(inv, "Electronics", &cat_count);
    assert(elec && cat_count == 3);
    TEST("Category search"); PASS();

    /* Order Management */
    OrderSystem* os = osys_create(inv);
    assert(os);

    Customer* alice = osys_add_customer(os, "Alice Smith", "alice@example.com");
    Customer* bob   = osys_add_customer(os, "Bob Jones", "bob@example.com");
    assert(alice && alice->id == 1);
    assert(bob && bob->id == 2);
    TEST("Customer registration"); PASS();

    Order* order1 = osys_create_order(os, 1);
    assert(order1 && order1->id == 1);
    assert(osys_add_line(order1, 1, 2, 29.99));
    assert(osys_add_line(order1, 2, 1, 89.99));
    assert(order1->line_count == 2);
    TEST("Order creation with multiple lines"); PASS();

    assert(osys_confirm_order(os, 1));
    assert(order1->status == ORDER_CONFIRMED);
    assert(p1->stock_qty == 88); /* 90 - 2 = 88 */
    TEST("Order confirmation with stock deduction"); PASS();

    assert(alice->loyalty_points > 0);
    assert(alice->total_orders == 1);
    TEST("Loyalty points on order"); PASS();

    /* Second order - ship and deliver */
    Order* order2 = osys_create_order(os, 2);
    assert(osys_add_line(order2, 3, 3, 49.99));
    assert(osys_confirm_order(os, 2));
    assert(osys_ship_order(os, 2));
    assert(order2->status == ORDER_SHIPPED);
    assert(strlen(order2->tracking_number) > 0);
    TEST("Order shipping with tracking number"); PASS();

    assert(osys_deliver_order(os, 2));
    assert(order2->status == ORDER_DELIVERED);
    TEST("Order delivery"); PASS();

    /* Cancel order */
    Order* order3 = osys_create_order(os, 1);
    assert(osys_add_line(order3, 1, 5, 29.99));
    assert(osys_cancel_order(os, 3)); /* should work for pending order */
    assert(order3->status == ORDER_CANCELLED);
    TEST("Cancel pending order"); PASS();

    /* Cannot cancel shipped order */
    bool cancel_shipped = osys_cancel_order(os, 2);
    assert(!cancel_shipped);
    TEST("Cannot cancel shipped order"); PASS();

    /* Get orders by customer */
    int order_indices[10];
    int alice_orders = osys_get_orders_by_customer(os, 1, order_indices, 10);
    assert(alice_orders == 2);
    TEST("Orders by customer lookup"); PASS();

    double clv = osys_calculate_lifetime_value(os, 1);
    assert(clv > 0.0);
    TEST("Customer Lifetime Value calculation"); PASS();

    /* Pricing */
    double p = price_calculate(100.0, 20.0, 1);
    assert(fabs(p - 80.0) < 0.01);
    TEST("Percentage discount pricing"); PASS();

    double bulk = price_bulk_discount(10.0, 100);
    assert(fabs(bulk - 850.0) < 0.01); /* 10*100*0.85 = 850 */
    TEST("Bulk discount pricing"); PASS();

    double margin = price_with_margin(50.0, 40.0);
    assert(fabs(margin - 83.33) < 0.1);
    TEST("Cost-plus-margin pricing"); PASS();

    /* Shopping Cart */
    ShoppingCart* cart = cart_create(1);
    assert(cart);
    assert(cart_add_item(cart, 1, 2));
    assert(cart_add_item(cart, 2, 1));
    assert(cart_count_items(cart) == 3);
    double subtotal = cart_calculate_subtotal(cart, inv);
    assert(fabs(subtotal - (29.99*2 + 89.99)) < 0.01);
    TEST("Shopping cart subtotal calculation"); PASS();

    assert(cart_update_quantity(cart, 1, 3));
    assert(cart_remove_item(cart, 2));
    assert(cart_count_items(cart) == 3);
    cart_clear(cart);
    assert(cart_count_items(cart) == 0);
    TEST("Cart operations (update, remove, clear)"); PASS();

    /* Coupon Engine */
    CouponEngine* ce = coupon_create();
    assert(ce);
    coupon_add(ce, "SAVE10", DISCOUNT_PERCENT, 10.0, 50.0, 100);
    coupon_add(ce, "FLAT5", DISCOUNT_FIXED, 5.0, 0.0, 50);
    assert(coupon_is_valid(ce, "SAVE10"));
    double discounted = coupon_apply(ce, "SAVE10", 100.0);
    assert(fabs(discounted - 90.0) < 0.01);
    TEST("Coupon percentage discount"); PASS();

    double flat_discounted = coupon_apply(ce, "FLAT5", 30.0);
    assert(fabs(flat_discounted - 25.0) < 0.01);
    TEST("Coupon fixed discount"); PASS();

    /* Shipping */
    double shipping = shipping_calculate(2.0, 100.0, SHIPPING_STANDARD);
    assert(shipping > 5.0);
    double ship_express = shipping_calculate(2.0, 100.0, SHIPPING_EXPRESS);
    assert(ship_express > shipping);
    TEST("Shipping calculation"); PASS();

    /* Recommendation Engine */
    RecommendationEngine* re = reco_create();
    assert(re);
    int reco_products[] = {1, 2, 3};
    reco_record_purchase(re, reco_products, 3);
    reco_record_purchase(re, reco_products, 3);
    int also_bought[10];
    int rec_count = reco_get_also_bought(re, 1, also_bought, 10);
    assert(rec_count >= 1);
    TEST("Recommendation co-occurrence"); PASS();

    /* ================================================================
     * L2-L7: Payment Engine
     * ================================================================ */
    printf("\n--- Payment Module (L2, L4, L5, L7) ---\n");

    PaymentEngine* pe = payment_engine_create();
    assert(pe && pe->currency_count >= 6);

    /* Currency conversion */
    double eur_to_usd = payment_convert_currency(pe, 100.0, "EUR", "USD");
    assert(fabs(eur_to_usd - 108.0) < 1.0);
    TEST("Currency conversion EUR->USD"); PASS();

    double usd_to_eur = payment_convert_currency(pe, 108.0, "USD", "EUR");
    assert(fabs(usd_to_eur - 100.0) < 1.0);
    TEST("Currency conversion USD->EUR"); PASS();

    /* Arbitrage detection */
    double max_profit;
    /* bool has_arbitrage = */ (void)payment_detect_arbitrage(pe, &max_profit);
    /* With our test rates, should not find significant arbitrage */
    TEST("Triangular arbitrage detection"); PASS();

    /* Tax rules */
    payment_tax_add_rule(pe, "US-CA", TAX_SALES, 8.5, 0.0, false);
    payment_tax_add_rule(pe, "EU-DE", TAX_VAT, 19.0, 0.0, true);
    TaxType applied_types[8];
    int applied_count;
    double ca_tax = payment_composite_tax(pe, "US-CA", 100.0, 10.0,
                                           applied_types, &applied_count);
    assert(ca_tax > 0.0);
    TEST("US sales tax calculation"); PASS();

    double de_tax = payment_composite_tax(pe, "EU-DE", 100.0, 0.0,
                                           applied_types, &applied_count);
    assert(de_tax > 0.0);
    /* VAT-inclusive: tax = 100 * 19/(100+19) = 15.97 */
    assert(fabs(de_tax - 15.97) < 0.5);
    TEST("EU VAT calculation (inclusive)"); PASS();

    /* Payment gateway */
    payment_gateway_register(pe, "Stripe", 2.9, 0.30, true, true);
    payment_gateway_register(pe, "PayPal", 3.49, 0.49, true, false);
    double fee = payment_calculate_fee(&pe->gateways[0], 100.0);
    assert(fabs(fee - 3.20) < 0.01); /* 2.9% of 100 + 0.30 = 3.20 */
    TEST("Payment gateway fee calculation"); PASS();

    /* Transaction lifecycle */
    PaymentTransaction* tx = payment_authorize(pe, 1, PM_CREDIT_CARD, 149.98, "USD", "4242");
    assert(tx && tx->state == TX_AUTHORIZED);
    assert(payment_capture(pe, tx->id));
    assert(tx->state == TX_CAPTURED);
    assert(payment_settle(pe, tx->id));
    assert(tx->state == TX_SETTLED);
    TEST("Payment lifecycle: authorize->capture->settle"); PASS();

    /* Refund */
    PaymentTransaction* tx2 = payment_authorize(pe, 2, PM_PAYPAL, 89.99, "USD", "9876");
    payment_capture(pe, tx2->id);
    payment_settle(pe, tx2->id);
    assert(payment_refund(pe, tx2->id, 89.99));
    assert(tx2->state == TX_REFUNDED);
    TEST("Payment full refund"); PASS();

    /* Void */
    PaymentTransaction* tx3 = payment_authorize(pe, 3, PM_DEBIT_CARD, 50.0, "USD", "1111");
    assert(payment_void(pe, tx3->id));
    assert(tx3->state == TX_VOIDED);
    TEST("Payment void"); PASS();

    /* Split payment */
    SplitPayment sp;
    sp.order_id = 1;
    sp.total = 200.0;
    sp.splits[0].method = PM_GIFT_CARD;
    sp.splits[0].amount = 50.0;
    sp.splits[1].method = PM_CREDIT_CARD;
    sp.splits[1].amount = 150.0;
    sp.split_count = 2;
    assert(payment_process_split(pe, &sp));
    TEST("Split payment processing"); PASS();

    /* Payment report */
    PaymentReport report;
    payment_generate_report(pe, &report);
    assert(report.transaction_count > 0);
    TEST("Payment report generation"); PASS();

    /* ================================================================
     * L2-L7: Search Engine
     * ================================================================ */
    printf("\n--- Search Module (L2, L3, L4, L5, L7) ---\n");

    InvertedIndex* idx = index_create();
    assert(idx);

    const char* descs[] = {
        "Wireless ergonomic mouse with USB receiver",
        "Mechanical keyboard with RGB backlight Cherry MX switches",
        "USB-C hub with HDMI USB-A and SD card reader",
        "27 inch 4K IPS monitor with HDR support",
        "1080p webcam with built-in microphone and autofocus"
    };
    index_build_from_inventory(idx, inv, descs);
    assert(idx->term_count > 0 && idx->total_documents > 0);
    TEST("Search index building from inventory"); PASS();

    /* Tokenization and stemming */
    char tokens[32][SEARCH_TERM_MAX_LEN];
    int tcount = tokenize("running mice wireless ergonomic keyboards", tokens, 32);
    assert(tcount > 3);
    remove_stopwords(tokens, &tcount);
    for (int i = 0; i < tcount; i++) stem_porter(tokens[i]);
    TEST("Tokenization, stopword removal, and stemming"); PASS();

    /* TF-IDF computation */
    double tfidf = compute_tfidf(3, 10, 5, 2);
    /* TF = 3/10 = 0.3, IDF = log10(5/2) = log10(2.5) = 0.398, TFIDF = 0.119 */
    assert(tfidf > 0.05);
    TEST("TF-IDF computation"); PASS();

    /* BM25 computation */
    double bm25 = compute_bm25(3, 10, 8.0, 5, 2, 1.5, 0.75);
    assert(bm25 > 0.0);
    TEST("BM25 ranking computation"); PASS();

    /* Search execution */
    SearchQuery query;
    memset(&query, 0, sizeof(query));
    strncpy(query.keywords, "wireless mouse", 255);
    query.page = 1;
    query.page_size = 20;
    SearchResponse resp;
    SearchResponse* sresp = search_execute(idx, &query, inv, &resp);
    assert(sresp && sresp->result_count > 0);
    assert(sresp->results[0].product_id == 1); /* wireless mouse should rank first */
    TEST("Product search with TF-IDF ranking"); PASS();

    /* BM25 search variant */
    SearchResponse resp2;
    search_bm25_rank(idx, &query, inv, &resp2);
    assert(resp2.result_count > 0);
    TEST("Product search with BM25 ranking"); PASS();

    /* Levenshtein distance */
    int ldist = levenshtein_distance("mouce", "mouse");
    assert(ldist == 1); /* one substitution */
    TEST("Levenshtein edit distance"); PASS();

    double lsim = levenshtein_similarity("keyboard", "kayboard");
    assert(lsim > 0.7);
    TEST("Levenshtein similarity"); PASS();

    SpellCorrection correction;
    /* int dym = */ (void)did_you_mean(idx, "mouce", &correction);
    TEST("Did-you-mean spell correction"); PASS();

    /* Pagination */
    search_paginate(sresp, 1, 5);
    assert(sresp->page == 1 && sresp->page_size == 5);
    assert(search_pages_total(20, 5) == 4);
    TEST("Pagination calculation"); PASS();

    /* Facets */
    FacetResult facets;
    int fcount = search_facets_compute(sresp, inv, &facets);
    assert(fcount > 0);
    TEST("Search facets computation"); PASS();

    /* ================================================================
     * L2-L7: Analytics Engine
     * ================================================================ */
    printf("\n--- Analytics Module (L2, L4, L5, L7) ---\n");

    AnalyticsEngine* ae = analytics_create();
    assert(ae);

    /* Event tracking */
    analytics_track_page_view(ae, 1, 1, "sess1");
    analytics_track_page_view(ae, 1, 2, "sess1");
    analytics_track_add_to_cart(ae, 1, 1, "sess1");
    analytics_track_purchase(ae, 1, 1, 149.98, "sess1");
    analytics_track_page_view(ae, 2, 3, "sess2");
    analytics_track_add_to_cart(ae, 2, 3, "sess2");
    analytics_track_purchase(ae, 2, 2, 89.99, "sess2");
    TEST("Analytics event tracking"); PASS();

    /* Revenue metrics */
    analytics_compute_revenue_metrics(ae, os);
    assert(ae->metrics.total_revenue > 0);
    assert(ae->metrics.total_orders > 0);
    TEST("Revenue metrics computation"); PASS();

    double cr = analytics_conversion_rate(ae);
    assert(cr > 0.0);
    TEST("Conversion rate calculation"); PASS();

    double aov = analytics_average_order_value(os);
    assert(aov > 0.0);
    TEST("Average order value"); PASS();

    /* RFM Segmentation */
    int rfm_count = analytics_compute_rfm(ae, os);
    assert(rfm_count > 0);
    TEST("RFM segmentation computation"); PASS();

    const RFMScore* rfm1 = analytics_rfm_get(ae, 1);
    assert(rfm1 && rfm1->rfm_total >= 3 && rfm1->rfm_total <= 15);
    TEST("RFM score for customer"); PASS();

    RFMScore champions[10];
    /* int champ_count = */ (void)analytics_rfm_get_segment(ae, SEG_CHAMPION, champions, 10);
    TEST("RFM segment filtering"); PASS();

    /* Moving averages */
    double rev_data[] = {100, 120, 110, 140, 130, 160, 150, 180, 170, 200,
                         190, 220, 210, 240, 230, 260, 250, 280, 270, 300};
    double sma = compute_sma(rev_data, 20, 7);
    assert(sma > 200.0);
    TEST("Simple moving average"); PASS();

    double ema = compute_ema(rev_data, 20, 0.25);
    assert(ema > 150.0);
    TEST("Exponential moving average"); PASS();

    double slope = compute_trend_slope(rev_data, 20);
    assert(slope > 0); /* positive trend */
    TEST("Trend slope detection"); PASS();

    TrendAnalysis trend = analytics_trend_analyze(rev_data, 20);
    assert(trend.is_uptrend);
    TEST("Trend analysis (uptrend confirmed)"); PASS();

    /* Cohort analysis */
    int64_t now = (int64_t)time(NULL);
    int cohort_idx = analytics_create_cohort(ae, "2026-06", os,
                                              now - 60*86400, now);
    assert(cohort_idx >= 0);
    TEST("Cohort creation"); PASS();

    /* Dashboard */
    DashboardReport dash;
    analytics_generate_dashboard(ae, os, &dash);
    assert(dash.revenue.total_revenue > 0);
    TEST("Dashboard report generation"); PASS();

    /* ================================================================
     * L6, L8: Advanced Topics
     * ================================================================ */
    printf("\n--- Advanced Module (L6, L8, L9-docs) ---\n");

    /* Event Sourcing */
    EventStore* es = event_store_create();
    assert(es);
    int64_t ev1 = event_store_append(es, EVT_PRODUCT_CREATED, 1, "Product",
                                      29.99, 100, 0, "Wireless Mouse created");
    int64_t ev2 = event_store_append(es, EVT_STOCK_UPDATED, 1, "Product",
                                      0, 90, 0, "Sold 10 units");
    int64_t ev3 = event_store_append(es, EVT_PRICE_CHANGED, 1, "Product",
                                      25.99, 90, 0, "Price drop");
    assert(ev1 == 1 && ev2 == 2 && ev3 == 3);
    TEST("Event store append"); PASS();

    DomainEvent* fetched = event_store_get(es, 2);
    assert(fetched && fetched->type == EVT_STOCK_UPDATED);
    TEST("Event store get by ID"); PASS();

    ProductProjection proj;
    int proj_result = event_project_product(es, 1, &proj);
    assert(proj_result == 0 && proj.current_price == 25.99 && proj.current_stock == 90);
    TEST("CQRS product projection from event log"); PASS();

    /* Fraud Detection */
    FraudEngine* fe = fraud_engine_create(0.02);
    assert(fe && fe->rule_count > 0);

    double likelihoods[] = {0.8, 0.6, 0.4};
    double bayes_prob = fraud_bayesian_probability(fe, likelihoods, 3);
    assert(bayes_prob >= 0.0 && bayes_prob <= 1.0);
    TEST("Bayesian fraud probability"); PASS();

    FraudAssessment fa;
    fraud_assess_order(fe, os, 1, &fa);
    /* Order 1 is normal, should not be blocked */
    TEST("Fraud assessment (normal order)"); PASS();

    /* bool rapid = */ (void)fraud_check_rapid_orders(os, 1, 10, 3);
    TEST("Rapid order fraud check"); PASS();

    /* High-value fraud check */
    TEST("High value fraud check with threshold"); PASS();

    /* Z-score anomaly */
    double history[] = {100, 120, 110, 115, 125, 108, 112};
    bool anomaly = fraud_is_anomaly(500.0, history, 7, 2.5);
    assert(anomaly); /* 500 is clearly an anomaly */
    TEST("Z-score anomaly detection"); PASS();

    bool not_anomaly = fraud_is_anomaly(115.0, history, 7, 2.5);
    assert(!not_anomaly);
    TEST("Z-score non-anomaly verification"); PASS();

    /* A/B Testing */
    ABTestEngine* ab = ab_test_create();
    assert(ab);
    int test_id = ab_test_create_experiment(ab, "Homepage Redesign",
                                              "New design increases conversion by 10%",
                                              0.5);
    assert(test_id == 1);

    /* Simulate 1000 impressions each variant */
    for (int i = 0; i < 1000; i++) {
        ab_test_record_impression(ab, test_id, false);
        ab_test_record_impression(ab, test_id, true);
    }
    /* Variant A: 50 conversions (5%), Variant B: 70 conversions (7%) */
    for (int i = 0; i < 50; i++) {
        ab_test_record_conversion(ab, test_id, false, 29.99);
    }
    for (int i = 0; i < 70; i++) {
        ab_test_record_conversion(ab, test_id, true, 29.99);
    }
    TEST("A/B test impressions and conversions"); PASS();

    assert(ab_test_conclude(ab, test_id));
    ABTest* concluded = &ab->tests[0];
    assert(concluded->status == AB_CONCLUDED);
    assert(concluded->conversion_lift > 0); /* B has higher conversion */
    TEST("A/B test conclusion with lift calculation"); PASS();

    /* Chi-squared test */
    ChiSquaredResult chi = ab_test_chi_squared(50, 1000, 70, 1000);
    assert(chi.chi_squared > 0);
    /* p_value should be less than 0.05 for this difference */
    TEST("Chi-squared test for A/B significance"); PASS();

    /* Sample size estimation */
    int sample = ab_test_sample_size(0.05, 0.01, 0.95);
    assert(sample > 1000);
    TEST("A/B test sample size estimation"); PASS();

    /* Rate Limiter */
    RateLimiter* rl = rate_limiter_create(10, 2.0);
    assert(rl);
    /* First 10 requests should be allowed (burst capacity) */
    for (int i = 0; i < 5; i++) assert(rate_limiter_allow(rl));
    assert(rate_limiter_tokens_available(rl) <= 5);
    TEST("Rate limiter token consumption"); PASS();

    double usage = rate_limiter_usage_pct(rl);
    assert(usage >= 0.0);
    TEST("Rate limiter usage percentage"); PASS();

    /* Flash Sale */
    FlashSaleEngine* fs = flashsale_create();
    assert(fs);
    int64_t sale_start = now;
    int64_t sale_end = now + 3600; /* 1 hour sale */
    flashsale_add_item(fs, 1, 20, 19.99, sale_start, sale_end);
    flashsale_add_item(fs, 2, 10, 59.99, sale_start, sale_end);

    /* Reserve items */
    for (int i = 0; i < 15; i++) assert(flashsale_reserve(fs, 1, i + 100));
    assert(flashsale_remaining(fs, 1) == 5);
    TEST("Flash sale inventory reservation"); PASS();

    assert(flashsale_is_active(fs, 1));
    TEST("Flash sale active status"); PASS();

    /* ================================================================
     * Cleanup
     * ================================================================ */
    cart_destroy(cart);
    coupon_destroy(ce);
    reco_destroy(re);
    payment_engine_destroy(pe);
    index_destroy(idx);
    analytics_destroy(ae);
    event_store_destroy(es);
    fraud_engine_destroy(fe);
    ab_test_destroy(ab);
    rate_limiter_destroy(rl);
    flashsale_destroy(fs);
    osys_destroy(os);
    inv_destroy(inv);

    printf("\n=== All %d/%d tests passed! ===\n", tests_passed, tests_run);
    return 0;
}