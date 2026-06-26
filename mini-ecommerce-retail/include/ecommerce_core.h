#ifndef ECOMMERCE_CORE_H
#define ECOMMERCE_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ECOM_MAX_ITEMS      1024
#define ECOM_MAX_ORDERS     4096
#define ECOM_MAX_NAME       128
#define ECOM_MAX_CUSTOMERS  1024
#define ECOM_MAX_CART_ITEMS 64
#define ECOM_MAX_COUPONS    128
#define ECOM_MAX_RECOS      64
#define ECOM_MAX_PRODUCTS   256

typedef enum {
    ORDER_PENDING,
    ORDER_CONFIRMED,
    ORDER_SHIPPED,
    ORDER_DELIVERED,
    ORDER_CANCELLED,
    ORDER_RETURNED,
    ORDER_COUNT
} OrderStatus;

typedef enum {
    PAYMENT_PENDING,
    PAYMENT_PAID,
    PAYMENT_FAILED,
    PAYMENT_REFUNDED,
    PAYMENT_COUNT
} PaymentStatus;

typedef enum {
    DISCOUNT_PERCENT,
    DISCOUNT_FIXED,
    DISCOUNT_BOGO,
    DISCOUNT_FREE_SHIPPING,
    DISCOUNT_COUNT
} DiscountType;

typedef enum {
    SHIPPING_STANDARD,
    SHIPPING_EXPRESS,
    SHIPPING_OVERNIGHT,
    SHIPPING_COUNT
} ShippingMethod;

typedef struct {
    char   name[ECOM_MAX_NAME];
    char   email[ECOM_MAX_NAME];
    int    id;
    double credit_limit;
    char   address[ECOM_MAX_NAME];
    char   city[ECOM_MAX_NAME];
    int    loyalty_points;
    int    total_orders;
} Customer;

typedef struct {
    char   name[ECOM_MAX_NAME];
    char   sku[32];
    int    id;
    double price;
    double cost;
    int    stock_qty;
    int    reorder_point;
    char   category[64];
    double weight_kg;
    double rating;
    int    review_count;
} Product;

typedef struct {
    int    product_id;
    int    quantity;
    double unit_price;
} OrderLine;

typedef struct {
    int          id;
    int          customer_id;
    OrderLine    lines[32];
    int          line_count;
    OrderStatus  status;
    PaymentStatus payment;
    double       total_amount;
    double       discount_amount;
    double       shipping_cost;
    int64_t      created_at;
    char         tracking_number[64];
} Order;

typedef struct {
    Product  products[ECOM_MAX_ITEMS];
    int      product_count;
    int      total_units_sold;
    double   total_revenue;
} Inventory;

typedef struct {
    int    product_id;
    int    quantity;
} CartItem;

typedef struct {
    CartItem items[ECOM_MAX_CART_ITEMS];
    int      item_count;
    int      customer_id;
    double   subtotal;
} ShoppingCart;

typedef struct {
    char         code[32];
    DiscountType type;
    double       value;
    double       min_order;
    int          max_uses;
    int          used_count;
    bool         active;
    int64_t      expires_at;
} Coupon;

typedef struct {
    Coupon   coupons[ECOM_MAX_COUPONS];
    int      coupon_count;
} CouponEngine;

typedef struct {
    double weight;
    double distance_km;
    double base_cost;
    double per_kg_rate;
    double insurance;
} ShippingCalc;

/* Product recommendation based on category co-occurrence */
typedef struct {
    int      product_id;
    int      also_bought[ECOM_MAX_RECOS];
    int      also_bought_count;
    double   purchase_count;
} ProductRecommendation;

typedef struct {
    ProductRecommendation recos[ECOM_MAX_PRODUCTS];
    int      reco_count;
    /* Co-occurrence matrix: count of times two products appear together */
    int      cooccur[ECOM_MAX_PRODUCTS][ECOM_MAX_PRODUCTS];
    int      product_map[ECOM_MAX_PRODUCTS];
    int      product_map_count;
} RecommendationEngine;

/* L1-L3: Inventory Management */
Inventory* inv_create(void);
Product*   inv_add_product(Inventory* inv, const char* name, const char* sku,
                           double price, double cost, int qty, double weight);
Product*   inv_find_by_sku(Inventory* inv, const char* sku);
Product*   inv_find_by_id(Inventory* inv, int id);
bool       inv_reserve_stock(Inventory* inv, int product_id, int qty);
void       inv_restock(Inventory* inv, int product_id, int qty);
bool       inv_needs_reorder(const Inventory* inv, int product_id);
Product*   inv_search_category(Inventory* inv, const char* category, int* count);
void       inv_print_summary(const Inventory* inv);
void       inv_destroy(Inventory* inv);

/* L3: Order Management System */
typedef struct {
    Order      orders[ECOM_MAX_ORDERS];
    int        order_count;
    Customer   customers[ECOM_MAX_CUSTOMERS];
    int        customer_count;
    Inventory* inventory;
} OrderSystem;

OrderSystem* osys_create(Inventory* inv);
Customer*    osys_add_customer(OrderSystem* os, const char* name, const char* email);
Customer*    osys_find_customer(OrderSystem* os, int id);
Order*       osys_create_order(OrderSystem* os, int customer_id);
bool         osys_add_line(Order* order, int product_id, int qty, double price);
bool         osys_confirm_order(OrderSystem* os, int order_id);
bool         osys_ship_order(OrderSystem* os, int order_id);
bool         osys_deliver_order(OrderSystem* os, int order_id);
bool         osys_cancel_order(OrderSystem* os, int order_id);
Order*       osys_get_order(OrderSystem* os, int order_id);
int          osys_get_orders_by_customer(const OrderSystem* os, int customer_id,
                                          int* order_indices, int max_results);
double       osys_calculate_lifetime_value(const OrderSystem* os, int customer_id);
void         osys_print_order(const Order* order);
void         osys_print_summary(const OrderSystem* os);
void         osys_destroy(OrderSystem* os);

/* L5: Pricing Engine */
double price_calculate(double base, double discount_pct, int quantity);
double price_bulk_discount(double base, int qty);
double price_with_margin(double cost, double margin_pct);

/* L5: Shopping Cart */
ShoppingCart* cart_create(int customer_id);
bool    cart_add_item(ShoppingCart* cart, int product_id, int qty);
bool    cart_remove_item(ShoppingCart* cart, int product_id);
bool    cart_update_quantity(ShoppingCart* cart, int product_id, int qty);
double  cart_calculate_subtotal(const ShoppingCart* cart, const Inventory* inv);
int     cart_count_items(const ShoppingCart* cart);
void    cart_clear(ShoppingCart* cart);
void    cart_destroy(ShoppingCart* cart);

/* L7: Coupon / Discount Engine */
CouponEngine* coupon_create(void);
int     coupon_add(CouponEngine* ce, const char* code, DiscountType type,
                   double value, double min_order, int max_uses);
bool    coupon_is_valid(const CouponEngine* ce, const char* code);
double  coupon_apply(const CouponEngine* ce, const char* code, double order_total);
void    coupon_print_summary(const CouponEngine* ce);
void    coupon_destroy(CouponEngine* ce);

/* L7: Shipping Calculator */
double  shipping_calculate(double weight_kg, double distance_km, ShippingMethod method);
double  shipping_order_weight(const Order* order, const Inventory* inv);

/* L7: Recommendation Engine */
RecommendationEngine* reco_create(void);
int     reco_record_purchase(RecommendationEngine* re, const int* product_ids, int count);
int     reco_get_also_bought(RecommendationEngine* re, int product_id,
                              int* results, int max_results);
void    reco_destroy(RecommendationEngine* re);

/* ── L5: Product Search Engine (TF-IDF) ── */
#define ECOM_SEARCH_INDEX_SIZE 4096
#define ECOM_SEARCH_TERM_MAX   64

typedef struct {
    char   term[64];
    int    product_ids[ECOM_MAX_PRODUCTS];
    int    doc_count;
    double idf;  /* Inverse document frequency */
} SearchTerm;

typedef struct {
    SearchTerm terms[ECOM_SEARCH_INDEX_SIZE];
    int        term_count;
    int        total_docs;
} SearchIndex;

SearchIndex* search_create(void);
int     search_index_product(SearchIndex* si, int product_id, const char* text);
int     search_query(const SearchIndex* si, const char* query, int* results, int max_results);
int     search_query_ranked(const SearchIndex* si, const char* query,
                             int* results, double* scores, int max_results);
void    search_destroy(SearchIndex* si);

/* ── L7: Customer Review System ── */
typedef struct {
    int    product_id;
    int    customer_id;
    int    rating;       /* 1-5 */
    char   title[ECOM_MAX_NAME];
    char   body[512];
    int64_t created_at;
    int    helpful_votes;
    int    total_votes;
    bool   verified_purchase;
} CustomerReview;

typedef struct {
    CustomerReview reviews[ECOM_MAX_ORDERS];
    int    review_count;
} ReviewSystem;

ReviewSystem* review_create(void);
int   review_add(ReviewSystem* rs, int product_id, int customer_id,
                  int rating, const char* title, const char* body,
                  bool verified);
double review_avg_rating(const ReviewSystem* rs, int product_id);
int    review_count_for_product(const ReviewSystem* rs, int product_id);
void   review_destroy(ReviewSystem* rs);

/* ── L8: Inventory Forecasting ── */
/* Simple exponential smoothing with trend (Holt's method)
 * F_{t+1} = α·Y_t + (1-α)·F_t
 * T_{t+1} = β·(F_{t+1}-F_t) + (1-β)·T_t
 * Forecast = F_{t+m} + m·T_{t+m} */
typedef struct {
    double level;
    double trend;
    double alpha;
    double beta;
} HoltWinters;

void holt_winters_init(HoltWinters* hw, double alpha, double beta);
double holt_winters_update(HoltWinters* hw, double value);
double holt_winters_forecast(const HoltWinters* hw, int steps_ahead);

/* ── L6: Flash Sale / Promotion Engine ── */
typedef struct {
    int    product_id;
    double discount_pct;
    double sale_price;
    int    max_units;
    int    sold_units;
    int64_t start_time;
    int64_t end_time;
    bool   active;
} FlashSale;

typedef struct {
    FlashSale sales[ECOM_MAX_ITEMS];
    int       sale_count;
} PromotionEngine;

PromotionEngine* promo_create(void);
int  promo_create_sale(PromotionEngine* pe, int product_id, double discount_pct,
                        int max_units, int64_t duration_seconds);
bool promo_is_active(const FlashSale* sale);
bool promo_reserve(FlashSale* sale, int qty);
void promo_destroy(PromotionEngine* pe);

/* ── L7: Warehouse / Fulfillment ── */
typedef struct {
    int    warehouse_id;
    char   name[ECOM_MAX_NAME];
    char   location[ECOM_MAX_NAME];
    double capacity_m3;
    double used_m3;
    double lat, lon;
} Warehouse;

typedef struct {
    Warehouse warehouses[32];
    int       warehouse_count;
    int       product_warehouse[ECOM_MAX_PRODUCTS]; /* maps product_id -> warehouse_id */
} FulfillmentNetwork;

FulfillmentNetwork* fulfillment_create(void);
int  fulfillment_add_warehouse(FulfillmentNetwork* fn, const char* name,
                                 const char* location, double capacity);
int  fulfillment_assign_product(FulfillmentNetwork* fn, int product_id, int warehouse_id);
int  fulfillment_nearest_warehouse(const FulfillmentNetwork* fn, double lat, double lon);
double fulfillment_shipping_distance(const FulfillmentNetwork* fn,
                                       int product_id, double dest_lat, double dest_lon);
void fulfillment_destroy(FulfillmentNetwork* fn);

/* ── L8: Dynamic Pricing Engine ── */
/* Adjusts prices based on demand, inventory, and competitor data.
 * Reference: Gallego & van Ryzin (1994) "Optimal dynamic pricing"
 *            Amazon dynamic pricing: ~2.5M price changes/day */
double dynamic_price_elasticity(double base_price, double demand_index,
                                 double inventory_pct, double competitor_price);
double dynamic_price_surge(double base_price, double demand_surge_factor,
                             int stock_remaining, int reorder_point);
double dynamic_price_markdown(double original_price, int days_on_shelf,
                                int shelf_life_days);

/* Utility */
const char* order_status_name(OrderStatus s);
const char* payment_status_name(PaymentStatus s);

#endif
