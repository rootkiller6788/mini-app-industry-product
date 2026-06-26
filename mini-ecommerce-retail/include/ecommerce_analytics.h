#ifndef ECOMMERCE_ANALYTICS_H
#define ECOMMERCE_ANALYTICS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "ecommerce_core.h"

/* ================================================================
 * L2: E-commerce analytics & business intelligence
 * L4: Pareto Principle (80/20 rule), Central Limit Theorem
 * L5: RFM segmentation, moving averages, cohort analysis
 * L7: Business dashboard & reporting
 * ================================================================ */

#define ANALYTICS_MAX_EVENTS   8192
#define ANALYTICS_MAX_SEGMENTS 32
#define ANALYTICS_MAX_COHORTS  16
#define ANALYTICS_WINDOW_DAYS  90

/* L1: Event types for analytics tracking */
typedef enum {
    EV_PAGE_VIEW,
    EV_PRODUCT_VIEW,
    EV_ADD_TO_CART,
    EV_REMOVE_FROM_CART,
    EV_CHECKOUT_START,
    EV_CHECKOUT_COMPLETE,
    EV_SEARCH,
    EV_REVIEW,
    EV_COUNT
} AnalyticsEventType;

/* L1: Analytics event — atomic tracking record */
typedef struct {
    AnalyticsEventType type;
    int    customer_id;
    int    product_id;
    int    order_id;
    double value;
    int64_t timestamp;
    char   session_id[64];
} AnalyticsEvent;

/* L1: Revenue metrics */
typedef struct {
    double   total_revenue;
    double   avg_order_value;      /* AOV */
    double   revenue_per_visitor;  /* RPV */
    int      total_orders;
    int      total_customers;
    int      total_visitors;
    double   conversion_rate;      /* orders / visitors */
    double   gross_margin;
    double   total_cost;
    double   net_profit;
} RevenueMetrics;

/* L1: Customer segment based on RFM */
typedef enum {
    SEG_CHAMPION,       /* High RFM */
    SEG_LOYAL,          /* High F, Medium M */
    SEG_POTENTIAL,      /* High M, Low F */
    SEG_AT_RISK,        /* Low R, Medium F */
    SEG_LOST,           /* Low RFM */
    SEG_NEW,            /* Recent, Low F */
    SEG_COUNT
} CustomerSegmentType;

/* L1: RFM (Recency, Frequency, Monetary) score */
typedef struct {
    int    customer_id;
    int    recency_days;       /* days since last purchase */
    int    frequency;          /* number of purchases */
    double monetary;           /* total spend */
    int    r_score;            /* 1-5, 5=most recent */
    int    f_score;            /* 1-5, 5=most frequent */
    int    m_score;            /* 1-5, 5=highest spend */
    int    rfm_total;          /* sum of R+F+M, max 15 */
    CustomerSegmentType segment;
} RFMScore;

/* L1: Cohort — group of customers acquired in same period */
typedef struct {
    char    cohort_name[32];   /* e.g. "2026-01" */
    int     cohort_size;       /* initial customer count */
    double  retention[12];     /* month 0-11 retention rate */
    double  revenue_per_month[12];
    int     active_per_month[12];
    double  avg_ltv;
} Cohort;

/* L3: Analytics engine — aggregates all tracking and reporting */
typedef struct {
    AnalyticsEvent events[ANALYTICS_MAX_EVENTS];
    int            event_count;
    RFMScore       rfm_scores[ECOM_MAX_CUSTOMERS];
    int            rfm_count;
    Cohort         cohorts[ANALYTICS_MAX_COHORTS];
    int            cohort_count;
    RevenueMetrics metrics;
} AnalyticsEngine;

/* --- Event Tracking (L2) --- */
AnalyticsEngine* analytics_create(void);
int  analytics_track_event(AnalyticsEngine* ae, AnalyticsEventType type,
                            int customer_id, int product_id, int order_id,
                            double value, const char* session_id);
int  analytics_track_page_view(AnalyticsEngine* ae, int customer_id, int product_id,
                                const char* session_id);
int  analytics_track_add_to_cart(AnalyticsEngine* ae, int customer_id, int product_id,
                                  const char* session_id);
int  analytics_track_purchase(AnalyticsEngine* ae, int customer_id, int order_id,
                               double value, const char* session_id);

/* --- Revenue & Conversion (L2, L7) --- */
void analytics_compute_revenue_metrics(AnalyticsEngine* ae, const OrderSystem* os);
double analytics_conversion_rate(const AnalyticsEngine* ae);
double analytics_average_order_value(const OrderSystem* os);

/* --- L5: RFM Segmentation ---
 * RFM is a marketing analysis method:
 *   Recency: How recently did the customer purchase?
 *   Frequency: How often do they purchase?
 *   Monetary: How much do they spend?
 * Quintile scoring: sort customers, divide into 5 equal groups, assign scores 1-5
 */
int  analytics_compute_rfm(AnalyticsEngine* ae, const OrderSystem* os);
void analytics_rfm_assign_segments(AnalyticsEngine* ae);
const RFMScore* analytics_rfm_get(const AnalyticsEngine* ae, int customer_id);
int  analytics_rfm_get_segment(const AnalyticsEngine* ae, CustomerSegmentType seg,
                                RFMScore* results, int max_results);
const char* segment_name(CustomerSegmentType seg);

/* --- L5: Moving Averages & Trend Detection ---
 * Simple Moving Average: SMA(t) = (1/n) * sum(x[t-n+1 .. t])
 * Exponential Moving Average: EMA(t) = α*x(t) + (1-α)*EMA(t-1), α=2/(n+1)
 */
double compute_sma(const double* values, int count, int window);
double compute_ema(const double* values, int count, double alpha);
int    compute_ema_series(const double* values, int count, double alpha, double* output);
double compute_trend_slope(const double* values, int count);
typedef struct {
    double   sma_7day;
    double   sma_30day;
    double   ema_7day;
    double   trend_slope;
    double   trend_strength;    /* -1.0 to 1.0 */
    double   volatility;        /* standard deviation */
    double   predicted_next;
    bool     is_uptrend;
} TrendAnalysis;
TrendAnalysis analytics_trend_analyze(const double* daily_revenue, int days);

/* --- L5: Cohort Analysis ---
 * Groups customers by acquisition month and tracks retention over time.
 * Retention Rate = (customers active in month N) / (original cohort size)
 */
int  analytics_create_cohort(AnalyticsEngine* ae, const char* name,
                              const OrderSystem* os, int64_t start_time, int64_t end_time);
int  analytics_cohort_retention(AnalyticsEngine* ae, int cohort_idx,
                                 const OrderSystem* os, int months);
void analytics_cohort_print(const Cohort* c);

/* --- L7: Dashboard & Reporting --- */
typedef struct {
    RevenueMetrics revenue;
    double         visitor_count;
    double         add_to_cart_rate;
    double         checkout_completion_rate;
    int            champion_count;
    int            at_risk_count;
    int            lost_count;
    double         monthly_growth;
} DashboardReport;

void analytics_generate_dashboard(const AnalyticsEngine* ae, const OrderSystem* os,
                                   DashboardReport* report);
void analytics_print_dashboard(const DashboardReport* report);
void analytics_print_summary(const AnalyticsEngine* ae);
void analytics_destroy(AnalyticsEngine* ae);

const char* analytics_event_name(AnalyticsEventType t);

#endif
