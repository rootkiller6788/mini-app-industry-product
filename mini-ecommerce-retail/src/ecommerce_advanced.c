#include "ecommerce_advanced.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ================================================================
 * L8: Advanced E-Commerce Topics
 *   - Event Sourcing / CQRS
 *   - Fraud Detection Engine (Bayesian + Rule-based)
 *   - A/B Testing Framework (Chi-squared, Sample Size)
 *   - Rate Limiter (Token Bucket)
 * L6: Flash Sale Engine (canonical high-concurrency problem)
 * L9: Documented in docs/industry-frontiers.md
 *
 * Cross-module integration:
 *   - security(13) audits fraud rules for network(5) + backend(8) entry
 *   - AI(14) consumes analytics & event store for ML pipelines
 * ================================================================ */

const char* domain_event_type_name(DomainEventType t) {
    switch (t) {
        case EVT_PRODUCT_CREATED:    return "ProductCreated";
        case EVT_STOCK_UPDATED:      return "StockUpdated";
        case EVT_PRICE_CHANGED:      return "PriceChanged";
        case EVT_ORDER_PLACED:       return "OrderPlaced";
        case EVT_ORDER_CONFIRMED:    return "OrderConfirmed";
        case EVT_ORDER_SHIPPED:      return "OrderShipped";
        case EVT_ORDER_DELIVERED:    return "OrderDelivered";
        case EVT_ORDER_CANCELLED:    return "OrderCancelled";
        case EVT_PAYMENT_RECEIVED:   return "PaymentReceived";
        case EVT_PAYMENT_REFUNDED:   return "PaymentRefunded";
        case EVT_CUSTOMER_REGISTERED:return "CustomerRegistered";
        case EVT_INVENTORY_ADJUSTED: return "InventoryAdjusted";
        default:                     return "Unknown";
    }
}

const char* fraud_rule_type_name(FraudRuleType t) {
    switch (t) {
        case FR_RAPID_ORDERS:       return "RapidOrders";
        case FR_HIGH_VALUE:         return "HighValue";
        case FR_MISMATCH_ADDRESS:   return "AddressMismatch";
        case FR_NEW_ACCOUNT_SPIKE:  return "NewAccountSpike";
        case FR_GEO_ANOMALY:        return "GeoAnomaly";
        case FR_VELOCITY_CHECK:     return "VelocityCheck";
        case FR_DEVICE_FINGERPRINT: return "DeviceFingerprint";
        case FR_CARD_TESTING:       return "CardTesting";
        default:                    return "Unknown";
    }
}

const char* ab_test_status_name(ABTestStatus s) {
    switch (s) {
        case AB_RUNNING:   return "Running";
        case AB_PAUSED:    return "Paused";
        case AB_CONCLUDED: return "Concluded";
        default:           return "Unknown";
    }
}

/* ================================================================
 * L8: Event Sourcing / CQRS — Append-Only Event Store
 *
 * CQRS (Command Query Responsibility Segregation):
 *   Write Model: Append events to log (immutable).
 *   Read Model: Project events into query-optimized views.
 *
 * Theorem: Eventual Consistency — given the same ordered event log,
 *   all projections converge to the same state (monotonic convergence).
 *   Proof sketch: Each projection is a deterministic fold over events;
 *   if the fold function is associative and events are totally ordered,
 *   all replicas reach identical state.
 * ================================================================ */

EventStore* event_store_create(void) {
    EventStore* es = (EventStore*)calloc(1, sizeof(EventStore));
    if (!es) return NULL;
    es->last_event_id = 0;
    return es;
}

/* L3: Append to event log — O(1) atomic append
 * Return: event_id (monotonically increasing), or -1 on overflow.
 */
int64_t event_store_append(EventStore* es, DomainEventType type,
                            int64_t aggregate_id, const char* aggregate_type,
                            double amount, int quantity, int related_id,
                            const char* metadata) {
    if (!es || es->event_count >= ADV_MAX_EVENTS) return -1;

    DomainEvent* ev = &es->events[es->event_count];
    ev->event_id = ++es->last_event_id;
    ev->type = type;
    ev->timestamp = (int64_t)time(NULL);
    ev->aggregate_id = aggregate_id;
    if (aggregate_type)
        snprintf(ev->aggregate_type, 31, "%s", aggregate_type);
    ev->amount = amount;
    ev->quantity = quantity;
    ev->related_id = related_id;
    if (metadata)
        snprintf(ev->metadata, 255, "%s", metadata);

    if (es->event_count == 0)
        es->oldest_timestamp = ev->timestamp;
    es->newest_timestamp = ev->timestamp;
    es->event_count++;

    return ev->event_id;
}

DomainEvent* event_store_get(const EventStore* es, int64_t event_id) {
    if (!es || event_id <= 0) return NULL;
    for (int64_t i = 0; i < es->event_count; i++) {
        if (es->events[i].event_id == event_id)
            return (DomainEvent*)&es->events[i];
    }
    return NULL;
}

int event_store_get_by_aggregate(const EventStore* es, int64_t aggregate_id,
                                  const char* aggregate_type,
                                  DomainEvent* results, int max_results) {
    if (!es || !results) return 0;
    int found = 0;
    for (int64_t i = 0; i < es->event_count && found < max_results; i++) {
        if (es->events[i].aggregate_id == aggregate_id &&
            (!aggregate_type || strcmp(es->events[i].aggregate_type, aggregate_type) == 0)) {
            results[found++] = es->events[i];
        }
    }
    return found;
}

int64_t event_store_count(const EventStore* es) {
    return es ? es->event_count : 0;
}

/* L8: CQRS Projection — rebuild current product state from event log
 * This demonstrates the "fold over events" pattern central to CQRS.
 */
int event_project_product(const EventStore* es, int product_id,
                           ProductProjection* proj) {
    if (!es || !proj) return -1;
    memset(proj, 0, sizeof(ProductProjection));
    proj->product_id = product_id;

    for (int64_t i = 0; i < es->event_count; i++) {
        const DomainEvent* ev = &es->events[i];
        if (ev->aggregate_id != product_id) continue;
        if (strcmp(ev->aggregate_type, "Product") != 0) continue;

        switch (ev->type) {
            case EVT_PRODUCT_CREATED:
                proj->current_price = ev->amount;
                proj->current_stock = ev->quantity;
                break;
            case EVT_STOCK_UPDATED:
                proj->current_stock = ev->quantity;
                break;
            case EVT_PRICE_CHANGED:
                proj->current_price = ev->amount;
                break;
            case EVT_INVENTORY_ADJUSTED:
                proj->current_stock += ev->quantity;
                if (ev->quantity < 0) {
                    proj->total_sold += (-ev->quantity);
                    proj->total_revenue += (-ev->quantity) * proj->current_price;
                }
                break;
            default: break;
        }
    }
    return 0;
}

int event_project_revenue_by_day(const EventStore* es, int64_t day_start,
                                  int64_t day_end, double* revenue) {
    if (!es || !revenue) return -1;
    *revenue = 0.0;
    for (int64_t i = 0; i < es->event_count; i++) {
        const DomainEvent* ev = &es->events[i];
        if (ev->timestamp >= day_start && ev->timestamp < day_end &&
            (ev->type == EVT_PAYMENT_RECEIVED || ev->type == EVT_ORDER_CONFIRMED)) {
            *revenue += ev->amount;
        }
    }
    return 0;
}

void event_store_print_summary(const EventStore* es) {
    if (!es) return;
    printf("Event Store: %lld events (ID range: 1..%lld)\n",
           (long long)es->event_count, (long long)es->last_event_id);
    printf("  Oldest: %lld, Newest: %lld\n",
           (long long)es->oldest_timestamp, (long long)es->newest_timestamp);
    /* Show event type distribution */
    int counts[EVT_COUNT] = {0};
    for (int64_t i = 0; i < es->event_count; i++)
        counts[es->events[i].type]++;
    for (int t = 0; t < EVT_COUNT; t++) {
        if (counts[t] > 0)
            printf("  %-24s: %d\n", domain_event_type_name((DomainEventType)t), counts[t]);
    }
}

void event_store_destroy(EventStore* es) { free(es); }

/* ================================================================
 * L8: Fraud Detection Engine
 *
 * L4: Bayesian inference for fraud probability
 *   P(Fraud | features) = [P(Fraud) * product(P(feature_i | Fraud))] /
 *                         [P(Fraud) * product(P(feature_i | Fraud)) +
 *                          P(Legit) * product(P(feature_i | Legit))]
 *
 *   log-space computation: log P(Fraud|f) = log P(Fraud) + sum log P(f_i|Fraud) - log Z
 *   where Z = P(Fraud)*prod(P(f_i|Fraud)) + P(Legit)*prod(P(f_i|Legit))
 * ================================================================ */

FraudEngine* fraud_engine_create(double base_fraud_rate) {
    FraudEngine* fe = (FraudEngine*)calloc(1, sizeof(FraudEngine));
    if (!fe) return NULL;
    fe->base_fraud_rate = base_fraud_rate;

    /* Pre-configure standard fraud rules */
    fraud_rule_add(fe, FR_RAPID_ORDERS, "Rapid Successive Orders",
                   3.0, 0.25, ">3 orders in 10 minutes from same customer");
    fraud_rule_add(fe, FR_HIGH_VALUE, "High Value Order",
                   1000.0, 0.20, "Order total exceeds $1000 threshold");
    fraud_rule_add(fe, FR_VELOCITY_CHECK, "Transaction Velocity",
                   5.0, 0.30, ">5 transactions in 60 seconds");
    fraud_rule_add(fe, FR_NEW_ACCOUNT_SPIKE, "New Account Large Purchase",
                   500.0, 0.15, "New account with purchase >$500");
    fraud_rule_add(fe, FR_CARD_TESTING, "Card Testing Pattern",
                   5.0, 0.10, "Multiple small transactions in rapid succession");

    return fe;
}

int fraud_rule_add(FraudEngine* fe, FraudRuleType type, const char* name,
                    double threshold, double weight, const char* description) {
    if (!fe || fe->rule_count >= ADV_MAX_FRAUD_RULES) return -1;
    FraudRule* rule = &fe->rules[fe->rule_count];
    rule->type = type;
    if (name) snprintf(rule->name, 63, "%s", name);
    rule->threshold = threshold;
    rule->weight = weight;
    rule->enabled = true;
    if (description) snprintf(rule->description, 127, "%s", description);
    rule->trigger_count = 0;
    fe->rule_count++;
    return fe->rule_count - 1;
}

int fraud_rule_enable(FraudEngine* fe, FraudRuleType type, bool enabled) {
    if (!fe) return -1;
    for (int i = 0; i < fe->rule_count; i++) {
        if (fe->rules[i].type == type) {
            fe->rules[i].enabled = enabled;
            return 0;
        }
    }
    return -1;
}

/* L4: Naive Bayes fraud probability using log-space to avoid underflow
 * Input: feature_likelihoods[i] = P(feature_i | Fraud) / P(feature_i | Legit)
 *   (likelihood ratio for each feature)
 * Output: P(Fraud | all features) ∈ [0, 1]
 */
double fraud_bayesian_probability(const FraudEngine* fe,
                                   double feature_likelihoods[], int feature_count) {
    if (!fe || !feature_likelihoods || feature_count <= 0)
        return fe ? fe->base_fraud_rate : 0.01;

    double prior_odds = fe->base_fraud_rate / (1.0 - fe->base_fraud_rate);
    double log_odds = log(prior_odds > 0.0 ? prior_odds : 0.01);

    for (int i = 0; i < feature_count; i++) {
        double lr = feature_likelihoods[i];
        if (lr > 0.0 && !isnan(lr) && !isinf(lr))
            log_odds += log(lr);
    }

    double odds = exp(log_odds);
    double prob = odds / (1.0 + odds);

    /* Clamp to valid range */
    if (prob < 0.0) prob = 0.0;
    if (prob > 1.0) prob = 1.0;
    return prob;
}

/* L5: Comprehensive fraud assessment — evaluates all enabled rules */
bool fraud_assess_order(FraudEngine* fe, const OrderSystem* os,
                         int order_id, FraudAssessment* result) {
    if (!fe || !os || !result) return false;
    memset(result, 0, sizeof(FraudAssessment));

    Order* order = osys_get_order((OrderSystem*)os, order_id);
    if (!order) return false;

    result->order_id = order_id;
    result->customer_id = order->customer_id;
    result->evaluated_at = (int64_t)time(NULL);

    double risk = 0.0;
    double weight_sum = 0.0;

    /* Check each enabled rule */
    for (int i = 0; i < fe->rule_count; i++) {
        FraudRule* rule = &fe->rules[i];
        if (!rule->enabled) continue;

        bool triggered = false;
        switch (rule->type) {
            case FR_RAPID_ORDERS:
                triggered = fraud_check_rapid_orders(os, order->customer_id,
                                                     10, (int)rule->threshold);
                break;
            case FR_HIGH_VALUE:
                triggered = fraud_check_high_value(order, rule->threshold);
                break;
            case FR_VELOCITY_CHECK:
                triggered = fraud_check_velocity(os, order->customer_id,
                                                  60, (int)rule->threshold);
                break;
            case FR_NEW_ACCOUNT_SPIKE: {
                Customer* c = osys_find_customer((OrderSystem*)os, order->customer_id);
                triggered = (c && c->total_orders <= 1 &&
                            order->total_amount > rule->threshold);
                break;
            }
            case FR_CARD_TESTING: {
                int small_count = 0;
                for (int j = 0; j < os->order_count; j++) {
                    if (os->orders[j].customer_id == order->customer_id &&
                        os->orders[j].total_amount < rule->threshold)
                        small_count++;
                }
                triggered = (small_count > (int)rule->threshold);
                break;
            }
            default: break;
        }

        if (triggered) {
            rule->trigger_count++;
            risk += rule->weight;
            if (result->triggered_count < 8)
                result->triggered_rules[result->triggered_count++] = rule->type;
        }
        weight_sum += rule->weight;
    }

    if (weight_sum > 0.0)
        result->risk_score = risk / weight_sum;
    else
        result->risk_score = 0.0;

    /* Block if risk exceeds threshold (0.6 = 60% risk) */
    result->blocked = (result->risk_score > 0.6);

    if (result->blocked) {
        snprintf(result->reason, 255,
                 "Blocked: risk score %.2f exceeds threshold 0.60. "
                 "Triggered %d rule(s).",
                 result->risk_score, result->triggered_count);
        fe->total_blocked++;
    }

    fe->assessments[fe->assessment_count % ADV_MAX_EVENTS] = *result;
    fe->assessment_count++;

    return result->blocked;
}

/* L5: Check for rapid successive orders from same customer */
bool fraud_check_rapid_orders(const OrderSystem* os, int customer_id,
                               int window_minutes, int max_orders) {
    if (!os) return false;
    int64_t now = (int64_t)time(NULL);
    int64_t window = (int64_t)window_minutes * 60;
    int count = 0;

    for (int i = 0; i < os->order_count; i++) {
        if (os->orders[i].customer_id == customer_id &&
            (now - os->orders[i].created_at) <= window) {
            count++;
        }
    }
    return count > max_orders;
}

bool fraud_check_high_value(const Order* order, double threshold) {
    return order && order->total_amount > threshold;
}

bool fraud_check_velocity(const OrderSystem* os, int customer_id,
                           int window_seconds, int max_transactions) {
    if (!os) return false;
    int64_t now = (int64_t)time(NULL);
    int count = 0;

    for (int i = 0; i < os->order_count; i++) {
        if (os->orders[i].customer_id == customer_id &&
            (now - os->orders[i].created_at) <= window_seconds) {
            count++;
        }
    }
    return count > max_transactions;
}

/* L8: Z-score anomaly detection
 * z = (x - mu) / sigma
 * |z| > threshold => anomaly (typically threshold = 2.5 or 3.0)
 * Assumes approximately normal distribution (Central Limit Theorem).
 */
double fraud_zscore(double value, double mean, double stddev) {
    if (stddev <= 0.0) return 0.0;
    return (value - mean) / stddev;
}

bool fraud_is_anomaly(double value, const double* history, int history_len,
                       double z_threshold) {
    if (!history || history_len < 2) return false;
    double mean = 0.0, m2 = 0.0;
    for (int i = 0; i < history_len; i++) {
        double delta = history[i] - mean;
        mean += delta / (i + 1);
        m2 += delta * (history[i] - mean);
    }
    double variance = m2 / (history_len - 1);
    double stddev = sqrt(variance);

    double z = fraud_zscore(value, mean, stddev);
    return fabs(z) > z_threshold;
}

void fraud_print_assessment(const FraudAssessment* fa) {
    if (!fa) return;
    printf("Fraud Assessment for Order #%d:\n", fa->order_id);
    printf("  Risk Score: %.4f | Blocked: %s\n",
           fa->risk_score, fa->blocked ? "YES" : "NO");
    printf("  Triggered Rules: %d\n", fa->triggered_count);
    for (int i = 0; i < fa->triggered_count; i++)
        printf("    - %s\n", fraud_rule_type_name(fa->triggered_rules[i]));
    if (fa->blocked) printf("  Reason: %s\n", fa->reason);
}

void fraud_print_summary(const FraudEngine* fe) {
    if (!fe) return;
    printf("Fraud Engine: %d rules, %d assessments, %d blocked\n",
           fe->rule_count, fe->assessment_count, fe->total_blocked);
    printf("  Base fraud rate: %.4f\n", fe->base_fraud_rate);
    for (int i = 0; i < fe->rule_count; i++) {
        const FraudRule* r = &fe->rules[i];
        printf("  [%s] %-30s thresh=%.1f weight=%.2f trig=%d\n",
               r->enabled ? "ON" : "OFF", r->name, r->threshold, r->weight,
               r->trigger_count);
    }
}

void fraud_engine_destroy(FraudEngine* fe) { free(fe); }

/* ================================================================
 * L8: A/B Testing Framework
 *
 * L4: Central Limit Theorem — with sufficient sample size,
 *   conversion rate distribution approximates normal.
 *
 * L5: Chi-squared test for independence
 *   chi^2 = sum((O_i - E_i)^2 / E_i)
 *   For 2x2 contingency table with 1 degree of freedom:
 *   chi^2 = N*(ad-bc)^2 / ((a+b)(c+d)(a+c)(b+d))
 *
 *   p-value computed via regularized incomplete gamma function:
 *   P(chi^2, df) = Q(df/2, chi^2/2) where Q is the regularized gamma.
 *   Approximation for df=1: p = 1 - erf(sqrt(chi^2/2))
 * ================================================================ */

ABTestEngine* ab_test_create(void) {
    return (ABTestEngine*)calloc(1, sizeof(ABTestEngine));
}

int ab_test_create_experiment(ABTestEngine* ab, const char* name,
                               const char* hypothesis, double traffic_split) {
    if (!ab || ab->test_count >= ADV_MAX_AB_TESTS) return -1;
    ABTest* test = &ab->tests[ab->test_count];
    memset(test, 0, sizeof(ABTest));
    test->id = ab->test_count + 1;
    if (name) snprintf(test->name, 127, "%s", name);
    if (hypothesis) snprintf(test->hypothesis, 255, "%s", hypothesis);
    test->traffic_split = traffic_split;
    test->status = AB_RUNNING;
    test->started_at = (int64_t)time(NULL);
    test->confidence_level = 0.95;
    ab->test_count++;
    return test->id;
}

void ab_test_record_impression(ABTestEngine* ab, int test_id, bool is_variant_b) {
    if (!ab || test_id <= 0 || test_id > ab->test_count) return;
    ABTest* test = &ab->tests[test_id - 1];
    if (test->status != AB_RUNNING) return;
    if (is_variant_b) test->impressions_b++;
    else test->impressions_a++;
}

void ab_test_record_conversion(ABTestEngine* ab, int test_id, bool is_variant_b,
                                double revenue) {
    if (!ab || test_id <= 0 || test_id > ab->test_count) return;
    ABTest* test = &ab->tests[test_id - 1];
    if (test->status != AB_RUNNING) return;
    if (is_variant_b) {
        test->conversions_b++;
        test->revenue_b += revenue;
    } else {
        test->conversions_a++;
        test->revenue_a += revenue;
    }
}

bool ab_test_conclude(ABTestEngine* ab, int test_id) {
    if (!ab || test_id <= 0 || test_id > ab->test_count) return false;
    ABTest* test = &ab->tests[test_id - 1];
    if (test->status != AB_RUNNING) return false;
    test->status = AB_CONCLUDED;
    test->ended_at = (int64_t)time(NULL);

    /* Compute conversion lift */
    double cr_a = test->impressions_a > 0
        ? (double)test->conversions_a / test->impressions_a : 0.0;
    double cr_b = test->impressions_b > 0
        ? (double)test->conversions_b / test->impressions_b : 0.0;

    if (cr_a > 0.0)
        test->conversion_lift = (cr_b - cr_a) / cr_a * 100.0;

    /* Chi-squared test */
    ChiSquaredResult chi = ab_test_chi_squared(
        test->conversions_a, test->impressions_a,
        test->conversions_b, test->impressions_b);
    test->p_value = chi.p_value;
    test->is_significant = chi.is_significant;

    return true;
}

/* L5: Chi-squared test implementation */
ChiSquaredResult ab_test_chi_squared(int conv_a, int total_a,
                                      int conv_b, int total_b) {
    ChiSquaredResult result;
    memset(&result, 0, sizeof(result));
    result.degrees_of_freedom = 1.0;

    int n = total_a + total_b;
    if (n == 0) return result;

    int non_conv_a = total_a - conv_a;
    int non_conv_b = total_b - conv_b;

    /* Chi-squared for 2x2 contingency table
     * chi^2 = n * (conv_a*non_conv_b - conv_b*non_conv_a)^2 /
     *         ((conv_a+conv_b)*(non_conv_a+non_conv_b)*total_a*total_b)
     */
    double num = (double)n *
        (double)(conv_a * non_conv_b - conv_b * non_conv_a) *
        (double)(conv_a * non_conv_b - conv_b * non_conv_a);
    double denom = (double)(conv_a + conv_b) *
                   (double)(non_conv_a + non_conv_b) *
                   (double)total_a * (double)total_b;

    if (denom <= 0.0) return result;

    result.chi_squared = num / denom;

    /* p-value approximation using error function for df=1
     * p = 2 * (1 - Phi(sqrt(chi^2))) where Phi is standard normal CDF
     * Using erf: Phi(x) = 0.5 * (1 + erf(x/sqrt(2)))
     * So p = 1 - erf(sqrt(chi^2/2))
     */
    double x = sqrt(result.chi_squared / 2.0);
    result.p_value = 1.0 - erf(x);
    if (result.p_value < 0.0) result.p_value = 0.0;
    if (result.p_value > 1.0) result.p_value = 1.0;
    result.is_significant = (result.p_value < 0.05);

    return result;
}

/* L5: Sample size estimation for A/B testing
 * Using normal approximation:
 * n = (Z_alpha/2^2 * 2 * p * (1-p)) / delta^2
 * where p = baseline conversion rate, delta = min detectable effect,
 * Z_alpha/2 = z-score for desired confidence (1.96 for 95%)
 */
int ab_test_sample_size(double baseline_rate, double min_detectable_effect,
                         double confidence) {
    if (baseline_rate <= 0.0 || baseline_rate >= 1.0) return -1;
    if (min_detectable_effect <= 0.0) return -1;

    /* Z-score for confidence level */
    double z = 1.96; /* 95% default */
    if (confidence >= 0.99) z = 2.576;
    else if (confidence >= 0.98) z = 2.326;
    else if (confidence >= 0.90) z = 1.645;

    double p = baseline_rate;
    double n = z * z * 2.0 * p * (1.0 - p) /
               (min_detectable_effect * min_detectable_effect);
    return (int)ceil(n);
}

void ab_test_print(const ABTest* test) {
    if (!test) return;
    printf("A/B Test #%d: %s [%s]\n", test->id, test->name,
           ab_test_status_name(test->status));
    printf("  Hypothesis: %s\n", test->hypothesis);
    printf("  A: %d impressions, %d conversions (%.2f%%)\n",
           test->impressions_a, test->conversions_a,
           test->impressions_a > 0
               ? 100.0 * test->conversions_a / test->impressions_a : 0.0);
    printf("  B: %d impressions, %d conversions (%.2f%%)\n",
           test->impressions_b, test->conversions_b,
           test->impressions_b > 0
               ? 100.0 * test->conversions_b / test->impressions_b : 0.0);
    if (test->status == AB_CONCLUDED) {
        printf("  Lift: %.1f%%, p=%.4f %s\n",
               test->conversion_lift, test->p_value,
               test->is_significant ? "SIGNIFICANT" : "NOT SIGNIFICANT");
    }
}

void ab_test_print_summary(const ABTestEngine* ab) {
    if (!ab) return;
    printf("A/B Test Engine: %d tests\n", ab->test_count);
    for (int i = 0; i < ab->test_count; i++)
        ab_test_print(&ab->tests[i]);
}

void ab_test_destroy(ABTestEngine* ab) { free(ab); }

/* ================================================================
 * L8: Rate Limiter — Token Bucket Algorithm
 *
 * Tokens are added at a constant refill_rate (tokens/second).
 * Each request consumes one token. Bucket has max_tokens capacity.
 * Burst traffic absorbs up to max_tokens, sustained at refill_rate.
 *
 * Used for: API rate limiting, flash sale traffic shaping,
 *   DDoS mitigation at entry points.
 * ================================================================ */

RateLimiter* rate_limiter_create(int max_tokens, double refill_rate) {
    RateLimiter* rl = (RateLimiter*)calloc(1, sizeof(RateLimiter));
    if (!rl) return NULL;
    rl->max_tokens = max_tokens;
    rl->refill_rate = refill_rate;
    rl->current_tokens = (double)max_tokens;
    rl->last_refill = (int64_t)time(NULL);
    return rl;
}

bool rate_limiter_allow(RateLimiter* rl) {
    if (!rl) return false;

    /* Refill tokens based on elapsed time */
    int64_t now = (int64_t)time(NULL);
    double elapsed = (double)(now - rl->last_refill);
    if (elapsed > 0.0) {
        rl->current_tokens += elapsed * rl->refill_rate;
        if (rl->current_tokens > (double)rl->max_tokens)
            rl->current_tokens = (double)rl->max_tokens;
        rl->last_refill = now;
    }

    if (rl->current_tokens >= 1.0) {
        rl->current_tokens -= 1.0;
        rl->requests_allowed++;
        return true;
    }

    rl->requests_denied++;
    return false;
}

int rate_limiter_tokens_available(const RateLimiter* rl) {
    if (!rl) return 0;
    /* Refill before checking */
    int64_t now = (int64_t)time(NULL);
    double elapsed = (double)(now - rl->last_refill);
    double tokens = rl->current_tokens + elapsed * rl->refill_rate;
    return (int)(tokens > rl->max_tokens ? rl->max_tokens : tokens);
}

double rate_limiter_usage_pct(const RateLimiter* rl) {
    if (!rl) return 0.0;
    int total = rl->requests_allowed + rl->requests_denied;
    if (total == 0) return 0.0;
    return (double)rl->requests_denied / (double)total * 100.0;
}

void rate_limiter_reset(RateLimiter* rl) {
    if (!rl) return;
    rl->current_tokens = (double)rl->max_tokens;
    rl->last_refill = (int64_t)time(NULL);
    rl->requests_allowed = 0;
    rl->requests_denied = 0;
}

void rate_limiter_print(const RateLimiter* rl) {
    if (!rl) return;
    printf("Rate Limiter: bucket=%d, rate=%.1f/s, tokens=%.1f\n",
           rl->max_tokens, rl->refill_rate, rl->current_tokens);
    printf("  Allowed: %d, Denied: %d (%.1f%% throttle)\n",
           rl->requests_allowed, rl->requests_denied,
           rate_limiter_usage_pct(rl));
}

void rate_limiter_destroy(RateLimiter* rl) { free(rl); }

/* ================================================================
 * L6: Flash Sale Engine — Canonical High-Concurrency E-Commerce Problem
 *
 * Flash sales create extreme traffic spikes and inventory contention.
 * Key challenges:
 *   1. Overselling prevention (atomic reserve)
 *   2. Fair queuing under high concurrency
 *   3. Real-time inventory visibility
 *
 * L4: Amdahl's Law applied — the serial inventory reservation step
 *   limits maximum throughput regardless of parallelization.
 *   If 10% of checkout is serial (reserve_stock), max speedup ≤ 10x.
 * ================================================================ */

FlashSaleEngine* flashsale_create(void) {
    FlashSaleEngine* fs = (FlashSaleEngine*)calloc(1, sizeof(FlashSaleEngine));
    return fs;
}

int flashsale_add_item(FlashSaleEngine* fs, int product_id, int total_stock,
                        double sale_price, int64_t starts_at, int64_t ends_at) {
    if (!fs || fs->item_count >= ADV_MAX_FLASH_ITEMS) return -1;
    FlashSaleItem* item = &fs->items[fs->item_count];
    item->product_id = product_id;
    item->total_stock = total_stock;
    item->reserved_stock = 0;
    item->sold_count = 0;
    item->sale_price = sale_price;
    item->starts_at = starts_at;
    item->ends_at = ends_at;
    item->active = false; /* Activated only during the sale window */
    fs->item_count++;
    return fs->item_count - 1;
}

bool flashsale_reserve(FlashSaleEngine* fs, int product_id, int customer_id) {
    if (!fs) return false;
    (void)customer_id;

    for (int i = 0; i < fs->item_count; i++) {
        FlashSaleItem* item = &fs->items[i];
        if (item->product_id != product_id) continue;

        /* Check if sale is active */
        int64_t now = (int64_t)time(NULL);
        if (now < item->starts_at || now > item->ends_at) return false;

        /* Atomic reserve: simple approach for single-threaded env */
        if (item->reserved_stock >= item->total_stock) return false;

        item->reserved_stock++;
        item->sold_count++;
        fs->total_sold++;
        fs->total_revenue += item->sale_price;
        return true;
    }
    return false;
}

bool flashsale_is_active(const FlashSaleEngine* fs, int product_id) {
    if (!fs) return false;
    int64_t now = (int64_t)time(NULL);
    for (int i = 0; i < fs->item_count; i++) {
        if (fs->items[i].product_id == product_id) {
            return (now >= fs->items[i].starts_at &&
                    now <= fs->items[i].ends_at &&
                    fs->items[i].reserved_stock < fs->items[i].total_stock);
        }
    }
    return false;
}

int flashsale_remaining(const FlashSaleEngine* fs, int product_id) {
    if (!fs) return 0;
    for (int i = 0; i < fs->item_count; i++) {
        if (fs->items[i].product_id == product_id)
            return fs->items[i].total_stock - fs->items[i].reserved_stock;
    }
    return 0;
}

void flashsale_print_summary(const FlashSaleEngine* fs) {
    if (!fs) return;
    printf("Flash Sale: %d items, %d sold, $%.2f revenue\n",
           fs->item_count, fs->total_sold, fs->total_revenue);
    for (int i = 0; i < fs->item_count; i++) {
        const FlashSaleItem* item = &fs->items[i];
        int remaining = flashsale_remaining(fs, item->product_id);
        printf("  Product #%d: $%.2f | %d/%d sold | %d remaining | %s\n",
               item->product_id, item->sale_price,
               item->sold_count, item->total_stock, remaining,
               flashsale_is_active(fs, item->product_id) ? "ACTIVE" : "INACTIVE");
    }
}

void flashsale_destroy(FlashSaleEngine* fs) { free(fs); }