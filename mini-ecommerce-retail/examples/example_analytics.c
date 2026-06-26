#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "ecommerce_core.h"
#include "ecommerce_analytics.h"

int main(void) {
    printf("=== Example 3: Analytics & Business Intelligence ===\n\n");

    /* Setup */
    Inventory* inv = inv_create();
    inv_add_product(inv, "Product A", "PA-001", 49.99, 25.00, 100, 0.5);
    inv_add_product(inv, "Product B", "PB-002", 99.99, 50.00, 50, 1.0);
    inv_add_product(inv, "Product C", "PC-003", 19.99, 8.00, 200, 0.2);

    OrderSystem* os = osys_create(inv);
    AnalyticsEngine* ae = analytics_create();

    /* Simulate customers and orders */
    Customer* c1 = osys_add_customer(os, "VIP Customer", "vip@example.com");
    Customer* c2 = osys_add_customer(os, "Regular Buyer", "reg@example.com");
    Customer* c3 = osys_add_customer(os, "New Shopper", "new@example.com");

    /* Track events */
    for (int s = 0; s < 5; s++) {
        char sid[64];
        snprintf(sid, 63, "session_%d", s);
        analytics_track_page_view(ae, 0, 1, sid);
        analytics_track_page_view(ae, 0, 2, sid);
        analytics_track_add_to_cart(ae, 0, 1, sid);
    }
    analytics_track_purchase(ae, 1, 1, 149.97, "session_vip");

    /* VIP Customer: many orders */
    for (int o = 0; o < 5; o++) {
        Order* order = osys_create_order(os, c1->id);
        osys_add_line(order, 1, 2, 49.99);
        osys_add_line(order, 2, 1, 99.99);
        osys_confirm_order(os, order->id);
        osys_ship_order(os, order->id);
        osys_deliver_order(os, order->id);
    }

    /* Regular Buyer: few orders */
    for (int o = 0; o < 2; o++) {
        Order* order = osys_create_order(os, c2->id);
        osys_add_line(order, 3, 3, 19.99);
        osys_confirm_order(os, order->id);
        osys_ship_order(os, order->id);
        osys_deliver_order(os, order->id);
    }

    /* New Shopper: one order */
    Order* new_order = osys_create_order(os, c3->id);
    osys_add_line(new_order, 1, 1, 49.99);
    osys_confirm_order(os, new_order->id);
    osys_ship_order(os, new_order->id);
    osys_deliver_order(os, new_order->id);

    /* --- Analytics --- */
    printf("--- Revenue Metrics ---\n");
    analytics_compute_revenue_metrics(ae, os);
    printf("Total Revenue: $%.2f\n", ae->metrics.total_revenue);
    printf("Total Orders: %d\n", ae->metrics.total_orders);
    printf("AOV: $%.2f\n", ae->metrics.avg_order_value);
    printf("Conversion Rate: %.2f%%\n", ae->metrics.conversion_rate);
    printf("Gross Margin: %.1f%%\n", ae->metrics.gross_margin);

    /* --- RFM Segmentation --- */
    printf("\n--- RFM Customer Segmentation ---\n");
    analytics_compute_rfm(ae, os);
    for (int i = 0; i < ae->rfm_count; i++) {
        RFMScore* rfm = &ae->rfm_scores[i];
        Customer* c = osys_find_customer(os, rfm->customer_id);
        printf("%s: R=%d F=%d M=%d Total=%d [%s] (orders=%d, spent=$%.2f)\n",
               c ? c->name : "?",
               rfm->r_score, rfm->f_score, rfm->m_score, rfm->rfm_total,
               segment_name(rfm->segment),
               rfm->frequency, rfm->monetary);
    }

    /* --- Pareto Check --- */
    printf("\n--- Pareto Principle (80/20) Check ---\n");
    double total_rev = ae->metrics.total_revenue;
    RFMScore champs[10];
    int nc = analytics_rfm_get_segment(ae, SEG_CHAMPION, champs, 10);
    double champ_rev = 0.0;
    for (int i = 0; i < nc; i++) champ_rev += champs[i].monetary;
    printf("Top segment (Champions) revenue: $%.2f / $%.2f = %.1f%%\n",
           champ_rev, total_rev, total_rev > 0 ? champ_rev/total_rev*100 : 0);

    /* --- Trend Analysis --- */
    printf("\n--- Trend Analysis ---\n");
    double daily_rev[] = {120, 135, 142, 158, 150, 165, 178, 182, 195, 200,
                          210, 225, 218, 240, 255, 248, 260, 275, 290, 280};
    TrendAnalysis trend = analytics_trend_analyze(daily_rev, 20);
    printf("SMA(7): %.1f | EMA(7): %.1f | Slope: %.2f\n",
           trend.sma_7day, trend.ema_7day, trend.trend_slope);
    printf("Volatility: %.1f | Next Pred: %.1f | Uptrend: %s\n",
           trend.volatility, trend.predicted_next,
           trend.is_uptrend ? "YES" : "NO");

    /* --- Dashboard --- */
    printf("\n--- Executive Dashboard ---\n");
    DashboardReport dash;
    analytics_generate_dashboard(ae, os, &dash);
    analytics_print_dashboard(&dash);

    analytics_destroy(ae);
    osys_destroy(os);
    inv_destroy(inv);
    printf("\n=== Analytics example completed ===\n");
    return 0;
}