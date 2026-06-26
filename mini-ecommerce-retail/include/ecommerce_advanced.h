#ifndef ECOMMERCE_ADVANCED_H
#define ECOMMERCE_ADVANCED_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "ecommerce_core.h"

#define ADV_MAX_EVENTS       8192
#define ADV_MAX_FRAUD_RULES  64
#define ADV_MAX_AB_TESTS     16
#define ADV_MAX_RATE_LIMITERS 32
#define ADV_EVENT_TYPE_LEN   64
#define ADV_MAX_FLASH_ITEMS  128

/* L8: Event Sourcing / CQRS */
typedef enum {
    EVT_PRODUCT_CREATED,
    EVT_STOCK_UPDATED,
    EVT_PRICE_CHANGED,
    EVT_ORDER_PLACED,
    EVT_ORDER_CONFIRMED,
    EVT_ORDER_SHIPPED,
    EVT_ORDER_DELIVERED,
    EVT_ORDER_CANCELLED,
    EVT_PAYMENT_RECEIVED,
    EVT_PAYMENT_REFUNDED,
    EVT_CUSTOMER_REGISTERED,
    EVT_INVENTORY_ADJUSTED,
    EVT_COUNT
} DomainEventType;

typedef struct {
    DomainEventType type;
    int64_t         event_id;
    int64_t         timestamp;
    int64_t         aggregate_id;
    char            aggregate_type[32];
    double          amount;
    int             quantity;
    int             related_id;
    char            metadata[256];
} DomainEvent;

typedef struct {
    DomainEvent events[ADV_MAX_EVENTS];
    int64_t     event_count;
    int64_t     last_event_id;
    int64_t     oldest_timestamp;
    int64_t     newest_timestamp;
} EventStore;

EventStore*    event_store_create(void);
int64_t        event_store_append(EventStore* es, DomainEventType type,
                   int64_t aggregate_id, const char* aggregate_type,
                   double amount, int quantity, int related_id,
                   const char* metadata);
DomainEvent*   event_store_get(const EventStore* es, int64_t event_id);
int            event_store_get_by_aggregate(const EventStore* es,
                   int64_t aggregate_id, const char* aggregate_type,
                   DomainEvent* results, int max_results);
int64_t        event_store_count(const EventStore* es);

typedef struct {
    int     product_id;
    double  current_price;
    int     current_stock;
    int     total_sold;
    double  total_revenue;
} ProductProjection;

int  event_project_product(const EventStore* es, int product_id,
                            ProductProjection* proj);
int  event_project_revenue_by_day(const EventStore* es, int64_t day_start,
                                   int64_t day_end, double* revenue);
void event_store_print_summary(const EventStore* es);
void event_store_destroy(EventStore* es);

/* L8: Fraud Detection Engine */
typedef enum {
    FR_RAPID_ORDERS,
    FR_HIGH_VALUE,
    FR_MISMATCH_ADDRESS,
    FR_NEW_ACCOUNT_SPIKE,
    FR_GEO_ANOMALY,
    FR_VELOCITY_CHECK,
    FR_DEVICE_FINGERPRINT,
    FR_CARD_TESTING,
    FR_RULE_COUNT
} FraudRuleType;

typedef struct {
    FraudRuleType type;
    char          name[64];
    double        threshold;
    double        weight;
    bool          enabled;
    int           trigger_count;
    char          description[128];
} FraudRule;

typedef struct {
    int            order_id;
    int            customer_id;
    double         risk_score;
    FraudRuleType  triggered_rules[8];
    int            triggered_count;
    bool           blocked;
    char           reason[256];
    int64_t        evaluated_at;
} FraudAssessment;

typedef struct {
    FraudRule      rules[ADV_MAX_FRAUD_RULES];
    int            rule_count;
    FraudAssessment assessments[ADV_MAX_EVENTS];
    int            assessment_count;
    int            total_blocked;
    double         base_fraud_rate;
} FraudEngine;

FraudEngine*     fraud_engine_create(double base_fraud_rate);
int  fraud_rule_add(FraudEngine* fe, FraudRuleType type, const char* name,
                     double threshold, double weight, const char* description);
int  fraud_rule_enable(FraudEngine* fe, FraudRuleType type, bool enabled);
double fraud_bayesian_probability(const FraudEngine* fe,
                                   double feature_likelihoods[], int feature_count);
bool  fraud_assess_order(FraudEngine* fe, const OrderSystem* os,
                          int order_id, FraudAssessment* result);
bool  fraud_check_rapid_orders(const OrderSystem* os, int customer_id,
                                int window_minutes, int max_orders);
bool  fraud_check_high_value(const Order* order, double threshold);
bool  fraud_check_velocity(const OrderSystem* os, int customer_id,
                            int window_seconds, int max_transactions);
double fraud_zscore(double value, double mean, double stddev);
bool   fraud_is_anomaly(double value, const double* history, int history_len,
                         double z_threshold);
void   fraud_print_assessment(const FraudAssessment* fa);
void   fraud_print_summary(const FraudEngine* fe);
void   fraud_engine_destroy(FraudEngine* fe);

/* L8: A/B Testing Framework */
typedef enum {
    AB_RUNNING,
    AB_PAUSED,
    AB_CONCLUDED,
    AB_COUNT
} ABTestStatus;

typedef struct {
    int         id;
    char        name[128];
    char        hypothesis[256];
    double      traffic_split;
    int         impressions_a;
    int         conversions_a;
    double      revenue_a;
    int         impressions_b;
    int         conversions_b;
    double      revenue_b;
    double      conversion_lift;
    double      p_value;
    double      confidence_level;
    bool        is_significant;
    ABTestStatus status;
    int64_t     started_at;
    int64_t     ended_at;
} ABTest;

typedef struct {
    ABTest  tests[ADV_MAX_AB_TESTS];
    int     test_count;
} ABTestEngine;

ABTestEngine* ab_test_create(void);
int   ab_test_create_experiment(ABTestEngine* ab, const char* name,
                                  const char* hypothesis, double traffic_split);
void  ab_test_record_impression(ABTestEngine* ab, int test_id, bool is_variant_b);
void  ab_test_record_conversion(ABTestEngine* ab, int test_id, bool is_variant_b,
                                  double revenue);
bool  ab_test_conclude(ABTestEngine* ab, int test_id);

typedef struct {
    double   chi_squared;
    double   p_value;
    double   degrees_of_freedom;
    bool     is_significant;
} ChiSquaredResult;
ChiSquaredResult ab_test_chi_squared(int conv_a, int total_a,
                                      int conv_b, int total_b);
int   ab_test_sample_size(double baseline_rate, double min_detectable_effect,
                           double confidence);
void  ab_test_print(const ABTest* test);
void  ab_test_print_summary(const ABTestEngine* ab);
void  ab_test_destroy(ABTestEngine* ab);

/* L8: Rate Limiter - Token Bucket Algorithm */
typedef struct {
    int      max_tokens;
    double   refill_rate;
    double   current_tokens;
    int64_t  last_refill;
    int      requests_allowed;
    int      requests_denied;
} RateLimiter;

RateLimiter* rate_limiter_create(int max_tokens, double refill_rate);
bool   rate_limiter_allow(RateLimiter* rl);
int    rate_limiter_tokens_available(const RateLimiter* rl);
double rate_limiter_usage_pct(const RateLimiter* rl);
void   rate_limiter_reset(RateLimiter* rl);
void   rate_limiter_print(const RateLimiter* rl);
void   rate_limiter_destroy(RateLimiter* rl);

/* L6: Flash Sale Engine */
typedef struct {
    int    product_id;
    int    total_stock;
    int    reserved_stock;
    int    sold_count;
    double sale_price;
    int64_t starts_at;
    int64_t ends_at;
    bool   active;
} FlashSaleItem;

typedef struct {
    FlashSaleItem items[ADV_MAX_FLASH_ITEMS];
    int           item_count;
    int           total_sold;
    double        total_revenue;
} FlashSaleEngine;

FlashSaleEngine* flashsale_create(void);
int  flashsale_add_item(FlashSaleEngine* fs, int product_id, int total_stock,
                         double sale_price, int64_t starts_at, int64_t ends_at);
bool flashsale_reserve(FlashSaleEngine* fs, int product_id, int customer_id);
bool flashsale_is_active(const FlashSaleEngine* fs, int product_id);
int  flashsale_remaining(const FlashSaleEngine* fs, int product_id);
void flashsale_print_summary(const FlashSaleEngine* fs);
void flashsale_destroy(FlashSaleEngine* fs);

/* Utility */
const char* domain_event_type_name(DomainEventType t);
const char* fraud_rule_type_name(FraudRuleType t);
const char* ab_test_status_name(ABTestStatus s);

#endif