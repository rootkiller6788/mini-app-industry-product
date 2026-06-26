#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "ecommerce_core.h"
#include "ecommerce_advanced.h"

int main(void) {
    printf("=== Example 4: Fraud Detection & Event Sourcing ===\n\n");

    /* Setup */
    Inventory* inv = inv_create();
    inv_add_product(inv, "Premium Headphones", "PH-001", 299.99, 150.00, 30, 0.3);
    inv_add_product(inv, "Smart Watch", "SW-002", 199.99, 100.00, 25, 0.15);

    OrderSystem* os = osys_create(inv);
    Customer* legit = osys_add_customer(os, "Legit Buyer", "good@example.com");
    Customer* fraud = osys_add_customer(os, "Suspicious User", "bad@example.com");

    /* --- Event Sourcing --- */
    printf("--- Event Sourcing (CQRS) ---\n");
    EventStore* es = event_store_create();

    /* Record product lifecycle events */
    event_store_append(es, EVT_PRODUCT_CREATED, 1, "Product",
                       299.99, 30, 0, "Headphones added to catalog");
    event_store_append(es, EVT_STOCK_UPDATED, 1, "Product",
                       0, 25, 0, "5 units sold, stock adjusted");

    /* Record order events */
    event_store_append(es, EVT_ORDER_PLACED, 100, "Order",
                       299.99, 1, 1, "Legit order for headphones");
    event_store_append(es, EVT_PAYMENT_RECEIVED, 100, "Order",
                       299.99, 0, 1, "Payment processed");

    event_store_append(es, EVT_ORDER_PLACED, 101, "Order",
                       1999.99, 10, 2, "Suspicious bulk order");
    event_store_append(es, EVT_PAYMENT_RECEIVED, 101, "Order",
                       1999.99, 0, 2, "Payment flagged");

    event_store_print_summary(es);

    /* CQRS projection: rebuild product state from events */
    ProductProjection proj;
    event_project_product(es, 1, &proj);
    printf("\nCQRS Projection for Product #%d:\n", proj.product_id);
    printf("  Current Price: $%.2f\n", proj.current_price);
    printf("  Current Stock: %d\n", proj.current_stock);
    printf("  Total Sold: %d\n", proj.total_sold);
    printf("  Total Revenue: $%.2f\n", proj.total_revenue);

    /* --- Fraud Detection --- */
    printf("\n--- Fraud Detection Engine ---\n");
    FraudEngine* fe = fraud_engine_create(0.02);

    /* Normal order from legit customer */
    Order* good_order = osys_create_order(os, legit->id);
    osys_add_line(good_order, 1, 1, 299.99);
    osys_confirm_order(os, good_order->id);

    FraudAssessment fa1;
    fraud_assess_order(fe, os, good_order->id, &fa1);
    printf("\nLegit Order #%d Assessment:\n", good_order->id);
    fraud_print_assessment(&fa1);

    /* Suspicious: rapid orders from fraud account */
    for (int i = 0; i < 5; i++) {
        Order* sus_order = osys_create_order(os, fraud->id);
        osys_add_line(sus_order, 2, 2, 199.99);
        osys_confirm_order(os, sus_order->id);
    }

    FraudAssessment fa2;
    fraud_assess_order(fe, os, os->order_count, &fa2);
    printf("\nSuspicious Order #%d Assessment:\n", os->order_count);
    fraud_print_assessment(&fa2);

    /* Z-score anomaly detection */
    double history[] = {100, 110, 105, 115, 120, 108, 112, 118, 125, 122};
    printf("\n--- Anomaly Detection ---\n");
    printf("Is 500.0 anomalous? %s\n",
           fraud_is_anomaly(500.0, history, 10, 2.5) ? "YES" : "NO");
    printf("Is 115.0 anomalous? %s\n",
           fraud_is_anomaly(115.0, history, 10, 2.5) ? "YES" : "NO");

    /* Bayesian probability example */
    double likelihoods[] = {0.9, 0.7, 0.5};
    double bayes_prob = fraud_bayesian_probability(fe, likelihoods, 3);
    printf("\nBayesian Fraud Probability (3 features): %.4f (%.1f%%)\n",
           bayes_prob, bayes_prob * 100.0);

    fraud_print_summary(fe);

    /* --- A/B Testing --- */
    printf("\n--- A/B Testing ---\n");
    ABTestEngine* ab = ab_test_create();
    int test_id = ab_test_create_experiment(ab,
        "Checkout Button Color",
        "Green button increases conversion vs Blue",
        0.5);

    for (int i = 0; i < 1000; i++) {
        ab_test_record_impression(ab, test_id, false);
        ab_test_record_impression(ab, test_id, true);
    }
    for (int i = 0; i < 50; i++)
        ab_test_record_conversion(ab, test_id, false, 49.99);
    for (int i = 0; i < 68; i++)
        ab_test_record_conversion(ab, test_id, true, 49.99);

    ab_test_conclude(ab, test_id);
    ab_test_print_summary(ab);

    /* Rate Limiter */
    printf("\n--- Rate Limiter (Token Bucket) ---\n");
    RateLimiter* rl = rate_limiter_create(5, 2.0);
    for (int i = 0; i < 10; i++) {
        bool allowed = rate_limiter_allow(rl);
        printf("Request %d: %s\n", i + 1, allowed ? "ALLOWED" : "DENIED");
    }
    rate_limiter_print(rl);

    /* Flash Sale */
    printf("\n--- Flash Sale Simulation ---\n");
    FlashSaleEngine* fs = flashsale_create();
    int64_t now = (int64_t)time(NULL);
    flashsale_add_item(fs, 1, 10, 149.99, now, now + 3600);

    printf("Flash sale for Product #1: $149.99 (10 units)\n");
    for (int i = 0; i < 11; i++) {
        bool reserved = flashsale_reserve(fs, 1, 100 + i);
        printf("  Reservation %d: %s\n", i + 1, reserved ? "SUCCESS" : "SOLD OUT");
    }
    flashsale_print_summary(fs);

    /* Cleanup */
    event_store_destroy(es);
    fraud_engine_destroy(fe);
    ab_test_destroy(ab);
    rate_limiter_destroy(rl);
    flashsale_destroy(fs);
    osys_destroy(os);
    inv_destroy(inv);

    printf("\n=== Advanced e-commerce example completed ===\n");
    return 0;
}