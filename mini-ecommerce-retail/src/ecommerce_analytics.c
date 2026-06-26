#include "ecommerce_analytics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ================================================================
 * L2: E-commerce Analytics Engine
 * L4: Pareto Principle (80/20 rule), Central Limit Theorem
 * L5: RFM Segmentation, Moving Averages, Cohort Analysis, Trend Detection
 * L7: Business Intelligence Dashboard & Reporting
 * Cross-module: AI(14) consumes analytics data for ML pipelines
 * ================================================================ */

const char* analytics_event_name(AnalyticsEventType t) {
    switch (t) {
        case EV_PAGE_VIEW:         return "Page View";
        case EV_PRODUCT_VIEW:      return "Product View";
        case EV_ADD_TO_CART:       return "Add to Cart";
        case EV_REMOVE_FROM_CART:  return "Remove from Cart";
        case EV_CHECKOUT_START:    return "Checkout Start";
        case EV_CHECKOUT_COMPLETE: return "Checkout Complete";
        case EV_SEARCH:            return "Search";
        case EV_REVIEW:            return "Review";
        default:                   return "Unknown";
    }
}

const char* segment_name(CustomerSegmentType seg) {
    switch (seg) {
        case SEG_CHAMPION:  return "Champion";
        case SEG_LOYAL:     return "Loyal";
        case SEG_POTENTIAL: return "Potential";
        case SEG_AT_RISK:   return "At Risk";
        case SEG_LOST:      return "Lost";
        case SEG_NEW:       return "New";
        default:            return "Unknown";
    }
}

/* --- L2: Engine Lifecycle --- */

AnalyticsEngine* analytics_create(void) {
    AnalyticsEngine* ae = (AnalyticsEngine*)calloc(1, sizeof(AnalyticsEngine));
    return ae;
}

/* --- L2: Event Tracking --- */

int analytics_track_event(AnalyticsEngine* ae, AnalyticsEventType type,
                           int customer_id, int product_id, int order_id,
                           double value, const char* session_id) {
    if (!ae || ae->event_count >= ANALYTICS_MAX_EVENTS) return -1;
    AnalyticsEvent* ev = &ae->events[ae->event_count];
    ev->type = type;
    ev->customer_id = customer_id;
    ev->product_id = product_id;
    ev->order_id = order_id;
    ev->value = value;
    ev->timestamp = (int64_t)time(NULL);
    if (session_id) snprintf(ev->session_id, 63, "%s", session_id);
    ae->event_count++;
    return ae->event_count - 1;
}

int analytics_track_page_view(AnalyticsEngine* ae, int customer_id,
                               int product_id, const char* session_id) {
    return analytics_track_event(ae, EV_PAGE_VIEW, customer_id, product_id, 0, 0.0, session_id);
}

int analytics_track_add_to_cart(AnalyticsEngine* ae, int customer_id,
                                 int product_id, const char* session_id) {
    return analytics_track_event(ae, EV_ADD_TO_CART, customer_id, product_id, 0, 0.0, session_id);
}

int analytics_track_purchase(AnalyticsEngine* ae, int customer_id,
                              int order_id, double value, const char* session_id) {
    return analytics_track_event(ae, EV_CHECKOUT_COMPLETE, customer_id, 0, order_id, value, session_id);
}

/* --- L2: Revenue & Conversion Metrics --- */

void analytics_compute_revenue_metrics(AnalyticsEngine* ae, const OrderSystem* os) {
    if (!ae || !os) return;
    memset(&ae->metrics, 0, sizeof(RevenueMetrics));

    for (int i = 0; i < os->order_count; i++) {
        const Order* o = &os->orders[i];
        if (o->status == ORDER_DELIVERED && o->payment == PAYMENT_PAID) {
            ae->metrics.total_revenue += o->total_amount;
            ae->metrics.total_orders++;

            /* Calculate cost of goods sold */
            for (int j = 0; j < o->line_count; j++) {
                Product* p = inv_find_by_id(os->inventory, o->lines[j].product_id);
                if (p) ae->metrics.total_cost += p->cost * o->lines[j].quantity;
            }
        }
    }

    if (ae->metrics.total_orders > 0) {
        ae->metrics.avg_order_value = ae->metrics.total_revenue / ae->metrics.total_orders;
    }

    /* Count unique customers with orders */
    bool seen[ECOM_MAX_CUSTOMERS] = {false};
    for (int i = 0; i < os->order_count; i++) {
        int cid = os->orders[i].customer_id;
        if (cid > 0 && cid <= ECOM_MAX_CUSTOMERS && !seen[cid - 1]) {
            seen[cid - 1] = true;
            ae->metrics.total_customers++;
        }
    }

    /* Visitor count from page view events */
    ae->metrics.total_visitors = 0;
    bool sess_seen[256] = {false};
    for (int i = 0; i < ae->event_count; i++) {
        if (ae->events[i].type == EV_PAGE_VIEW) {
            unsigned char c = (unsigned char)ae->events[i].session_id[0];
            if (!sess_seen[c]) {
                sess_seen[c] = true;
                ae->metrics.total_visitors++;
            }
        }
    }
    if (ae->metrics.total_visitors == 0) ae->metrics.total_visitors = 1;

    ae->metrics.conversion_rate =
        (double)ae->metrics.total_orders / ae->metrics.total_visitors * 100.0;
    ae->metrics.revenue_per_visitor =
        ae->metrics.total_revenue / ae->metrics.total_visitors;
    ae->metrics.gross_margin =
        ae->metrics.total_revenue > 0
        ? (ae->metrics.total_revenue - ae->metrics.total_cost) / ae->metrics.total_revenue * 100.0
        : 0.0;
    ae->metrics.net_profit = ae->metrics.total_revenue - ae->metrics.total_cost;
}

double analytics_conversion_rate(const AnalyticsEngine* ae) {
    if (!ae) return 0.0;
    return ae->metrics.conversion_rate;
}

double analytics_average_order_value(const OrderSystem* os) {
    if (!os) return 0.0;
    double total = 0.0;
    int count = 0;
    for (int i = 0; i < os->order_count; i++) {
        if (os->orders[i].payment == PAYMENT_PAID) {
            total += os->orders[i].total_amount;
            count++;
        }
    }
    return count > 0 ? total / count : 0.0;
}

/* ================================================================
 * L5: RFM Segmentation (Recency, Frequency, Monetary)
 *
 * RFM is a marketing analysis method for customer value:
 *   R: How recently did the customer purchase? (lower is better)
 *   F: How often do they purchase? (higher is better)
 *   M: How much do they spend? (higher is better)
 *
 * Quintile assignment: sort all customers on each dimension,
 *   divide into 5 equal groups, assign scores 1 (worst) to 5 (best).
 *   RFM total = R + F + M, range [3, 15].
 *
 * L4: Pareto Principle — typically 20% of customers (Champions)
 *   generate 80% of revenue. RFM identifies these top customers.
 * ================================================================ */

/* Helper: sort indices by a double value array (ascending) */
static void sort_indices_by_double(int* indices, const double* values, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (values[indices[j]] < values[indices[i]]) {
                int tmp = indices[i];
                indices[i] = indices[j];
                indices[j] = tmp;
            }
        }
    }
}

int analytics_compute_rfm(AnalyticsEngine* ae, const OrderSystem* os) {
    if (!ae || !os || os->customer_count == 0) return 0;

    double recency_vals[ECOM_MAX_CUSTOMERS];
    double frequency_vals[ECOM_MAX_CUSTOMERS];
    double monetary_vals[ECOM_MAX_CUSTOMERS];
    int64_t now = (int64_t)time(NULL);
    int n = 0;

    for (int cid = 1; cid <= os->customer_count; cid++) {
        const Customer* cust = osys_find_customer((OrderSystem*)os, cid);
        if (!cust) continue;

        /* Compute recency: days since last order */
        int64_t last_order_time = 0;
        int freq = 0;
        double spent = 0.0;

        for (int i = 0; i < os->order_count; i++) {
            if (os->orders[i].customer_id == cid &&
                os->orders[i].payment == PAYMENT_PAID) {
                freq++;
                spent += os->orders[i].total_amount;
                if (os->orders[i].created_at > last_order_time)
                    last_order_time = os->orders[i].created_at;
            }
        }

        if (freq == 0) continue; /* Skip customers with no purchases */

        double recency = (double)((now - last_order_time) / 86400);
        if (recency < 0.0) recency = 0.0;

        recency_vals[n] = recency;
        frequency_vals[n] = (double)freq;
        monetary_vals[n] = spent;
        ae->rfm_scores[n].customer_id = cid;
        ae->rfm_scores[n].recency_days = (int)recency;
        ae->rfm_scores[n].frequency = freq;
        ae->rfm_scores[n].monetary = spent;
        n++;
    }
    ae->rfm_count = n;
    if (n == 0) return 0;

    /* Sort and assign quintile scores */
    int indices[ECOM_MAX_CUSTOMERS];
    for (int i = 0; i < n; i++) indices[i] = i;

    /* R score: lower recency = higher score (invert sorting) */
    double inv_recency[ECOM_MAX_CUSTOMERS];
    double max_rec = recency_vals[0];
    for (int i = 0; i < n; i++)
        if (recency_vals[i] > max_rec) max_rec = recency_vals[i];
    if (max_rec > 0) {
        for (int i = 0; i < n; i++)
            inv_recency[i] = max_rec - recency_vals[i];
    } else {
        for (int i = 0; i < n; i++)
            inv_recency[i] = 1.0;
    }

    sort_indices_by_double(indices, inv_recency, n);
    for (int i = 0; i < n; i++) {
        ae->rfm_scores[indices[i]].r_score = 5 - i * 5 / n;
        if (ae->rfm_scores[indices[i]].r_score < 1) ae->rfm_scores[indices[i]].r_score = 1;
    }

    /* F score: higher frequency = higher score */
    sort_indices_by_double(indices, frequency_vals, n);
    for (int i = 0; i < n; i++) {
        ae->rfm_scores[indices[i]].f_score = 1 + i * 5 / n;
        if (ae->rfm_scores[indices[i]].f_score > 5) ae->rfm_scores[indices[i]].f_score = 5;
    }

    /* M score: higher spend = higher score */
    sort_indices_by_double(indices, monetary_vals, n);
    for (int i = 0; i < n; i++) {
        ae->rfm_scores[indices[i]].m_score = 1 + i * 5 / n;
        if (ae->rfm_scores[indices[i]].m_score > 5) ae->rfm_scores[indices[i]].m_score = 5;
    }

    /* Compute RFM total */
    for (int i = 0; i < n; i++) {
        ae->rfm_scores[i].rfm_total =
            ae->rfm_scores[i].r_score +
            ae->rfm_scores[i].f_score +
            ae->rfm_scores[i].m_score;
    }

    /* Assign segments */
    analytics_rfm_assign_segments(ae);
    return n;
}

/* L4: Segment assignment based on RFM thresholds
 * Champions: R≥4, F≥4, M≥4
 * Loyal: F≥4, M≥3 (frequent purchasers)
 * Potential: M≥4, F≤2 (high spenders, infrequent)
 * At Risk: F≥3, R≤2 (used to buy frequently, not recently)
 * Lost: R≤2, F≤2, M≤2
 * New: R=5, F≤2 (recent first purchase)
 */
void analytics_rfm_assign_segments(AnalyticsEngine* ae) {
    if (!ae) return;
    for (int i = 0; i < ae->rfm_count; i++) {
        int r = ae->rfm_scores[i].r_score;
        int f = ae->rfm_scores[i].f_score;
        int m = ae->rfm_scores[i].m_score;

        if (r >= 4 && f >= 4 && m >= 4)
            ae->rfm_scores[i].segment = SEG_CHAMPION;
        else if (f >= 4 && m >= 3)
            ae->rfm_scores[i].segment = SEG_LOYAL;
        else if (m >= 4 && f <= 2)
            ae->rfm_scores[i].segment = SEG_POTENTIAL;
        else if (f >= 3 && r <= 2)
            ae->rfm_scores[i].segment = SEG_AT_RISK;
        else if (r <= 2 && f <= 2)
            ae->rfm_scores[i].segment = SEG_LOST;
        else if (r == 5 && f <= 2)
            ae->rfm_scores[i].segment = SEG_NEW;
        else
            ae->rfm_scores[i].segment = SEG_LOYAL;
    }
}

const RFMScore* analytics_rfm_get(const AnalyticsEngine* ae, int customer_id) {
    if (!ae) return NULL;
    for (int i = 0; i < ae->rfm_count; i++) {
        if (ae->rfm_scores[i].customer_id == customer_id)
            return &ae->rfm_scores[i];
    }
    return NULL;
}

int analytics_rfm_get_segment(const AnalyticsEngine* ae, CustomerSegmentType seg,
                               RFMScore* results, int max_results) {
    if (!ae || !results) return 0;
    int found = 0;
    for (int i = 0; i < ae->rfm_count && found < max_results; i++) {
        if (ae->rfm_scores[i].segment == seg)
            results[found++] = ae->rfm_scores[i];
    }
    return found;
}

/* ================================================================
 * L5: Moving Averages & Trend Detection
 * SMA(t) = (1/n) * sum(x[t-n+1 .. t])
 * EMA(t) = alpha * x(t) + (1-alpha) * EMA(t-1)
 * Trend slope via linear regression using least squares.
 * ================================================================ */

double compute_sma(const double* values, int count, int window) {
    if (!values || count <= 0 || window <= 0 || window > count) return 0.0;
    double sum = 0.0;
    for (int i = count - window; i < count; i++)
        sum += values[i];
    return sum / (double)window;
}

double compute_ema(const double* values, int count, double alpha) {
    if (!values || count <= 0 || alpha <= 0.0 || alpha > 1.0) return 0.0;
    double ema = values[0];
    for (int i = 1; i < count; i++)
        ema = alpha * values[i] + (1.0 - alpha) * ema;
    return ema;
}

int compute_ema_series(const double* values, int count, double alpha, double* output) {
    if (!values || !output || count <= 0) return 0;
    output[0] = values[0];
    for (int i = 1; i < count; i++)
        output[i] = alpha * values[i] + (1.0 - alpha) * output[i - 1];
    return count;
}

/* L5: Linear trend slope via least squares
 * slope = (n*sum(x*y) - sum(x)*sum(y)) / (n*sum(x^2) - (sum(x))^2)
 */
double compute_trend_slope(const double* values, int count) {
    if (!values || count < 2) return 0.0;
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
    for (int i = 0; i < count; i++) {
        sum_x += i;
        sum_y += values[i];
        sum_xy += i * values[i];
        sum_x2 += i * i;
    }
    double n = (double)count;
    double denom = n * sum_x2 - sum_x * sum_x;
    if (fabs(denom) < 1e-10) return 0.0;
    return (n * sum_xy - sum_x * sum_y) / denom;
}

TrendAnalysis analytics_trend_analyze(const double* daily_revenue, int days) {
    TrendAnalysis result;
    memset(&result, 0, sizeof(TrendAnalysis));
    if (!daily_revenue || days < 7) return result;

    result.sma_7day = compute_sma(daily_revenue, days, 7);
    if (days >= 30)
        result.sma_30day = compute_sma(daily_revenue, days, 30);

    double alpha = 2.0 / (7.0 + 1.0); /* standard alpha for 7-day EMA */
    result.ema_7day = compute_ema(daily_revenue, days, alpha);
    result.trend_slope = compute_trend_slope(daily_revenue, days);

    /* Trend strength: correlation between day index and revenue */
    double mean_x = (days - 1.0) / 2.0;
    double mean_y = 0.0;
    for (int i = 0; i < days; i++) mean_y += daily_revenue[i];
    mean_y /= days;

    double cov = 0.0, var_x = 0.0, var_y = 0.0;
    for (int i = 0; i < days; i++) {
        double dx = i - mean_x;
        double dy = daily_revenue[i] - mean_y;
        cov += dx * dy;
        var_x += dx * dx;
        var_y += dy * dy;
    }
    if (var_x > 0 && var_y > 0)
        result.trend_strength = cov / sqrt(var_x * var_y);
    result.trend_strength = fmax(-1.0, fmin(1.0, result.trend_strength));

    /* Volatility as standard deviation */
    for (int i = 0; i < days; i++) {
        double diff = daily_revenue[i] - mean_y;
        result.volatility += diff * diff;
    }
    result.volatility = sqrt(result.volatility / days);

    /* Simple prediction: extrapolate trend for next day */
    result.predicted_next = mean_y + result.trend_slope * days;
    result.is_uptrend = result.trend_slope > 0;

    return result;
}

/* ================================================================
 * L5: Cohort Analysis
 * Groups customers by their first purchase month.
 * Tracks retention: % of cohort that purchases in each subsequent month.
 *
 * L4: "Retention curves typically follow a power law" —
 *   R(t) = R0 * t^(-alpha) where alpha is the decay rate.
 * ================================================================ */

int analytics_create_cohort(AnalyticsEngine* ae, const char* name,
                             const OrderSystem* os, int64_t start_time,
                             int64_t end_time) {
    if (!ae || !name || !os || ae->cohort_count >= ANALYTICS_MAX_COHORTS) return -1;

    Cohort* cohort = &ae->cohorts[ae->cohort_count];
    memset(cohort, 0, sizeof(Cohort));
    strncpy(cohort->cohort_name, name, 31);

    /* Find customers who made their first purchase in this window */
    for (int cid = 1; cid <= os->customer_count; cid++) {
        int64_t first_purchase = INT64_MAX;
        for (int i = 0; i < os->order_count; i++) {
            if (os->orders[i].customer_id == cid &&
                os->orders[i].payment == PAYMENT_PAID &&
                os->orders[i].created_at < first_purchase) {
                first_purchase = os->orders[i].created_at;
            }
        }
        if (first_purchase >= start_time && first_purchase <= end_time) {
            cohort->cohort_size++;
        }
    }

    int idx = ae->cohort_count;
    ae->cohort_count++;
    return idx;
}

int analytics_cohort_retention(AnalyticsEngine* ae, int cohort_idx,
                                const OrderSystem* os, int months) {
    if (!ae || !os || cohort_idx < 0 || cohort_idx >= ae->cohort_count) return -1;
    Cohort* c = &ae->cohorts[cohort_idx];
    if (c->cohort_size == 0) return -1;
    if (months > 12) months = 12;

    for (int m = 0; m < months; m++) {
        int64_t month_start = (int64_t)time(NULL) - (int64_t)(months - m) * 30 * 86400;
        int64_t month_end = month_start + 30 * 86400;
        int active = 0;
        double month_rev = 0.0;

        for (int i = 0; i < os->order_count; i++) {
            if (os->orders[i].payment == PAYMENT_PAID &&
                os->orders[i].created_at >= month_start &&
                os->orders[i].created_at < month_end) {
                active++;
                month_rev += os->orders[i].total_amount;
            }
        }

        c->retention[m] = (double)active / (double)c->cohort_size;
        c->active_per_month[m] = active;
        c->revenue_per_month[m] = month_rev;
    }
    return 0;
}

void analytics_cohort_print(const Cohort* c) {
    if (!c) return;
    printf("Cohort: %s (size=%d)\n", c->cohort_name, c->cohort_size);
    printf("  Month  Retention  Active  Revenue\n");
    for (int m = 0; m < 12 && c->active_per_month[m] > 0; m++) {
        printf("  %5d  %8.1f%%  %6d  $%10.2f\n",
               m, c->retention[m] * 100.0,
               c->active_per_month[m], c->revenue_per_month[m]);
    }
}

/* --- L7: Dashboard Report --- */

void analytics_generate_dashboard(const AnalyticsEngine* ae, const OrderSystem* os,
                                   DashboardReport* report) {
    if (!ae || !os || !report) return;
    memset(report, 0, sizeof(DashboardReport));

    analytics_compute_revenue_metrics((AnalyticsEngine*)ae, os);
    report->revenue = ae->metrics;

    /* Count visitor events */
    report->visitor_count = 0.0;
    for (int i = 0; i < ae->event_count; i++) {
        if (ae->events[i].type == EV_PAGE_VIEW) report->visitor_count++;
    }
    if (report->visitor_count < 1.0) report->visitor_count = 1.0;

    /* Add-to-cart rate */
    int cart_adds = 0, product_views = 0;
    for (int i = 0; i < ae->event_count; i++) {
        if (ae->events[i].type == EV_ADD_TO_CART) cart_adds++;
        if (ae->events[i].type == EV_PRODUCT_VIEW) product_views++;
    }
    report->add_to_cart_rate = product_views > 0
        ? (double)cart_adds / (double)product_views * 100.0 : 0.0;

    /* Checkout completion */
    int checkout_starts = 0, checkout_completes = 0;
    for (int i = 0; i < ae->event_count; i++) {
        if (ae->events[i].type == EV_CHECKOUT_START) checkout_starts++;
        if (ae->events[i].type == EV_CHECKOUT_COMPLETE) checkout_completes++;
    }
    report->checkout_completion_rate = checkout_starts > 0
        ? (double)checkout_completes / (double)checkout_starts * 100.0 : 0.0;

    /* Count segments */
    for (int i = 0; i < ae->rfm_count; i++) {
        switch (ae->rfm_scores[i].segment) {
            case SEG_CHAMPION: report->champion_count++; break;
            case SEG_AT_RISK:  report->at_risk_count++; break;
            case SEG_LOST:     report->lost_count++; break;
            default: break;
        }
    }

    /* Monthly growth estimate */
    report->monthly_growth = 0.0;
}

void analytics_print_dashboard(const DashboardReport* report) {
    if (!report) return;
    printf("=== E-Commerce Dashboard ===\n");
    printf("  Revenue:     $%.2f (AOV: $%.2f)\n",
           report->revenue.total_revenue, report->revenue.avg_order_value);
    printf("  Orders:      %d | Customers: %d | Visitors: %.0f\n",
           report->revenue.total_orders, report->revenue.total_customers,
           report->visitor_count);
    printf("  Conversion:  %.2f%% | Add-to-Cart: %.1f%% | Checkout: %.1f%%\n",
           report->revenue.conversion_rate, report->add_to_cart_rate,
           report->checkout_completion_rate);
    printf("  Gross Margin: %.1f%% | Net Profit: $%.2f\n",
           report->revenue.gross_margin, report->revenue.net_profit);
    printf("  RFM: Champions=%d At-Risk=%d Lost=%d\n",
           report->champion_count, report->at_risk_count, report->lost_count);
}

void analytics_print_summary(const AnalyticsEngine* ae) {
    if (!ae) return;
    printf("Analytics Engine: %d events, %d RFM scores, %d cohorts\n",
           ae->event_count, ae->rfm_count, ae->cohort_count);
    printf("  Revenue Metrics: $%.2f rev, %.2f%% CR, AOV $%.2f\n",
           ae->metrics.total_revenue, ae->metrics.conversion_rate,
           ae->metrics.avg_order_value);
}

void analytics_destroy(AnalyticsEngine* ae) { free(ae); }