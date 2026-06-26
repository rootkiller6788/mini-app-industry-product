#include "ecommerce_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* Utility */
const char* order_status_name(OrderStatus s) {
    switch (s) {
        case ORDER_PENDING:   return "Pending";
        case ORDER_CONFIRMED: return "Confirmed";
        case ORDER_SHIPPED:   return "Shipped";
        case ORDER_DELIVERED: return "Delivered";
        case ORDER_CANCELLED: return "Cancelled";
        case ORDER_RETURNED:  return "Returned";
        default:              return "Unknown";
    }
}

const char* payment_status_name(PaymentStatus s) {
    switch (s) {
        case PAYMENT_PENDING: return "Pending";
        case PAYMENT_PAID:    return "Paid";
        case PAYMENT_FAILED:  return "Failed";
        case PAYMENT_REFUNDED:return "Refunded";
        default:              return "Unknown";
    }
}

/* ================================================================
 * L2: Inventory Management
 * L5: Atomic stock reservation prevents overselling
 * L4: Little's Law: L = λW (avg inventory = arrival_rate × holding_time)
 * ================================================================ */

Inventory* inv_create(void) {
    Inventory* inv = (Inventory*)calloc(1, sizeof(Inventory));
    return inv;
}

Product* inv_add_product(Inventory* inv, const char* name, const char* sku,
                          double price, double cost, int qty, double weight) {
    if (!inv || inv->product_count >= ECOM_MAX_ITEMS) return NULL;
    Product* p = &inv->products[inv->product_count];
    p->id = inv->product_count + 1;
    snprintf(p->name, ECOM_MAX_NAME, "%s", name);
    snprintf(p->sku, 32, "%s", sku);
    p->price = price;
    p->cost = cost;
    p->stock_qty = qty;
    p->reorder_point = qty / 4;
    if (p->reorder_point < 1) p->reorder_point = 1;
    p->weight_kg = weight;
    p->rating = 0.0;
    p->review_count = 0;
    strncpy(p->category, "General", 63);
    inv->product_count++;
    return p;
}

Product* inv_find_by_sku(Inventory* inv, const char* sku) {
    if (!inv || !sku) return NULL;
    for (int i = 0; i < inv->product_count; i++) {
        if (strcmp(inv->products[i].sku, sku) == 0)
            return &inv->products[i];
    }
    return NULL;
}

Product* inv_find_by_id(Inventory* inv, int id) {
    if (!inv || id <= 0 || id > inv->product_count) return NULL;
    return &inv->products[id - 1];
}

bool inv_reserve_stock(Inventory* inv, int product_id, int qty) {
    if (!inv || product_id <= 0 || product_id > inv->product_count || qty <= 0)
        return false;
    Product* p = &inv->products[product_id - 1];
    if (p->stock_qty < qty) return false;
    p->stock_qty -= qty;
    inv->total_units_sold += qty;
    inv->total_revenue += p->price * qty;
    return true;
}

void inv_restock(Inventory* inv, int product_id, int qty) {
    if (!inv || product_id <= 0 || product_id > inv->product_count || qty <= 0) return;
    inv->products[product_id - 1].stock_qty += qty;
}

bool inv_needs_reorder(const Inventory* inv, int product_id) {
    if (!inv || product_id <= 0 || product_id > inv->product_count) return false;
    return inv->products[product_id - 1].stock_qty <=
           inv->products[product_id - 1].reorder_point;
}

Product* inv_search_category(Inventory* inv, const char* category, int* count) {
    if (!inv || !category || !count) return NULL;
    *count = 0;
    static Product results[ECOM_MAX_PRODUCTS];
    for (int i = 0; i < inv->product_count; i++) {
        if (strstr(inv->products[i].category, category)) {
            results[(*count)++] = inv->products[i];
        }
    }
    return *count > 0 ? results : NULL;
}

void inv_print_summary(const Inventory* inv) {
    if (!inv) return;
    printf("Inventory: %d products, %d sold, $%.2f revenue\n",
           inv->product_count, inv->total_units_sold, inv->total_revenue);
    for (int i = 0; i < inv->product_count; i++) {
        const Product* p = &inv->products[i];
        printf("  [%d] %-20s SKU:%-10s $%8.2f %4d in stock %s\n",
               p->id, p->name, p->sku, p->price, p->stock_qty,
               inv_needs_reorder(inv, p->id) ? "[REORDER]" : "");
    }
}

void inv_destroy(Inventory* inv) { free(inv); }

/* ================================================================
 * L3: Order Management System — Full lifecycle
 * L6: Canonical problem — e-commerce order processing
 * ================================================================ */

OrderSystem* osys_create(Inventory* inv) {
    OrderSystem* os = (OrderSystem*)calloc(1, sizeof(OrderSystem));
    if (!os) return NULL;
    os->inventory = inv;
    return os;
}

Customer* osys_add_customer(OrderSystem* os, const char* name, const char* email) {
    if (!os || os->customer_count >= ECOM_MAX_CUSTOMERS) return NULL;
    Customer* c = &os->customers[os->customer_count];
    c->id = os->customer_count + 1;
    snprintf(c->name, ECOM_MAX_NAME, "%s", name);
    snprintf(c->email, ECOM_MAX_NAME, "%s", email);
    c->credit_limit = 10000.0;
    c->loyalty_points = 0;
    c->total_orders = 0;
    os->customer_count++;
    return c;
}

Customer* osys_find_customer(OrderSystem* os, int id) {
    if (!os || id <= 0 || id > os->customer_count) return NULL;
    return &os->customers[id - 1];
}

Order* osys_create_order(OrderSystem* os, int customer_id) {
    if (!os || os->order_count >= ECOM_MAX_ORDERS) return NULL;
    Customer* c = osys_find_customer(os, customer_id);
    if (!c) return NULL;
    Order* o = &os->orders[os->order_count];
    memset(o, 0, sizeof(Order));
    o->id = os->order_count + 1;
    o->customer_id = customer_id;
    o->status = ORDER_PENDING;
    o->payment = PAYMENT_PENDING;
    o->created_at = (int64_t)time(NULL);
    os->order_count++;
    return o;
}

bool osys_add_line(Order* order, int product_id, int qty, double price) {
    if (!order || qty <= 0 || order->line_count >= 32) return false;
    if (order->status != ORDER_PENDING) return false;
    OrderLine* line = &order->lines[order->line_count];
    line->product_id = product_id;
    line->quantity = qty;
    line->unit_price = price;
    order->total_amount += price * qty;
    order->line_count++;
    return true;
}

bool osys_confirm_order(OrderSystem* os, int order_id) {
    if (!os || order_id <= 0 || order_id > os->order_count) return false;
    Order* o = &os->orders[order_id - 1];
    if (o->status != ORDER_PENDING) return false;

    for (int i = 0; i < o->line_count; i++) {
        if (!inv_reserve_stock(os->inventory, o->lines[i].product_id, o->lines[i].quantity)) {
            for (int j = 0; j < i; j++) {
                inv_restock(os->inventory, o->lines[j].product_id, o->lines[j].quantity);
            }
            return false;
        }
    }
    o->status = ORDER_CONFIRMED;
    o->payment = PAYMENT_PAID;
    Customer* c = osys_find_customer(os, o->customer_id);
    if (c) {
        c->total_orders++;
        c->loyalty_points += (int)(o->total_amount / 10.0);
    }
    return true;
}

bool osys_ship_order(OrderSystem* os, int order_id) {
    if (!os || order_id <= 0 || order_id > os->order_count) return false;
    Order* o = &os->orders[order_id - 1];
    if (o->status != ORDER_CONFIRMED) return false;
    o->status = ORDER_SHIPPED;
    snprintf(o->tracking_number, 63, "TRK-%d-%lld", o->id, (long long)time(NULL));
    return true;
}

bool osys_deliver_order(OrderSystem* os, int order_id) {
    if (!os || order_id <= 0 || order_id > os->order_count) return false;
    Order* o = &os->orders[order_id - 1];
    if (o->status != ORDER_SHIPPED) return false;
    o->status = ORDER_DELIVERED;
    return true;
}

bool osys_cancel_order(OrderSystem* os, int order_id) {
    if (!os || order_id <= 0 || order_id > os->order_count) return false;
    Order* o = &os->orders[order_id - 1];
    if (o->status == ORDER_SHIPPED || o->status == ORDER_DELIVERED) return false;
    if (o->status == ORDER_CONFIRMED) {
        for (int i = 0; i < o->line_count; i++) {
            inv_restock(os->inventory, o->lines[i].product_id, o->lines[i].quantity);
        }
    }
    o->status = ORDER_CANCELLED;
    return true;
}

Order* osys_get_order(OrderSystem* os, int order_id) {
    if (!os || order_id <= 0 || order_id > os->order_count) return NULL;
    return &os->orders[order_id - 1];
}

int osys_get_orders_by_customer(const OrderSystem* os, int customer_id,
                                 int* order_indices, int max_results) {
    if (!os || !order_indices) return 0;
    int found = 0;
    for (int i = 0; i < os->order_count && found < max_results; i++) {
        if (os->orders[i].customer_id == customer_id) {
            order_indices[found++] = i;
        }
    }
    return found;
}

/* L7: Customer Lifetime Value (CLV)
 * CLV = avg_order_value × purchase_frequency × customer_lifespan */
double osys_calculate_lifetime_value(const OrderSystem* os, int customer_id) {
    if (!os) return 0.0;
    double total_spent = 0.0;
    int order_count = 0;
    for (int i = 0; i < os->order_count; i++) {
        if (os->orders[i].customer_id == customer_id &&
            os->orders[i].payment == PAYMENT_PAID) {
            total_spent += os->orders[i].total_amount;
            order_count++;
        }
    }
    if (order_count == 0) return 0.0;
    double avg_order = total_spent / order_count;
    return avg_order * 12.0 * 3.0;  /* 12 orders/yr × 3 years */
}

void osys_print_order(const Order* order) {
    if (!order) return;
    printf("Order #%d | Status: %-10s | Payment: %-10s | Total: $%.2f\n",
           order->id, order_status_name(order->status),
           payment_status_name(order->payment), order->total_amount);
    for (int i = 0; i < order->line_count; i++) {
        printf("  %d× product #%d @ $%.2f = $%.2f\n",
               order->lines[i].quantity, order->lines[i].product_id,
               order->lines[i].unit_price,
               order->lines[i].quantity * order->lines[i].unit_price);
    }
}

void osys_print_summary(const OrderSystem* os) {
    if (!os) return;
    printf("Order System: %d customers, %d orders\n",
           os->customer_count, os->order_count);
    int pending = 0, confirmed = 0, shipped = 0, delivered = 0, cancelled = 0;
    double revenue = 0.0;
    for (int i = 0; i < os->order_count; i++) {
        switch (os->orders[i].status) {
            case ORDER_PENDING:   pending++; break;
            case ORDER_CONFIRMED: confirmed++; break;
            case ORDER_SHIPPED:   shipped++; break;
            case ORDER_DELIVERED: delivered++; revenue += os->orders[i].total_amount; break;
            case ORDER_CANCELLED: cancelled++; break;
            default: break;
        }
    }
    printf("  P:%d C:%d S:%d D:%d X:%d | Revenue: $%.2f\n",
           pending, confirmed, shipped, delivered, cancelled, revenue);
}

void osys_destroy(OrderSystem* os) { free(os); }

/* ================================================================
 * L5: Pricing Engine
 * L4: Law of One Price, Price Elasticity of Demand
 * ================================================================ */

double price_calculate(double base, double discount_pct, int quantity) {
    if (discount_pct < 0.0) discount_pct = 0.0;
    if (discount_pct > 100.0) discount_pct = 100.0;
    return base * (1.0 - discount_pct / 100.0) * quantity;
}

double price_bulk_discount(double base, int qty) {
    if (qty >= 100) return price_calculate(base, 15.0, qty);
    if (qty >= 50)  return price_calculate(base, 10.0, qty);
    if (qty >= 10)  return price_calculate(base, 5.0, qty);
    return price_calculate(base, 0.0, qty);
}

double price_with_margin(double cost, double margin_pct) {
    return cost / (1.0 - margin_pct / 100.0);
}

/* ================================================================
 * L5: Shopping Cart
 * ================================================================ */

ShoppingCart* cart_create(int customer_id) {
    ShoppingCart* cart = (ShoppingCart*)calloc(1, sizeof(ShoppingCart));
    if (!cart) return NULL;
    cart->customer_id = customer_id;
    return cart;
}

bool cart_add_item(ShoppingCart* cart, int product_id, int qty) {
    if (!cart || qty <= 0 || cart->item_count >= ECOM_MAX_CART_ITEMS) return false;
    for (int i = 0; i < cart->item_count; i++) {
        if (cart->items[i].product_id == product_id) {
            cart->items[i].quantity += qty;
            return true;
        }
    }
    cart->items[cart->item_count].product_id = product_id;
    cart->items[cart->item_count].quantity = qty;
    cart->item_count++;
    return true;
}

bool cart_remove_item(ShoppingCart* cart, int product_id) {
    if (!cart) return false;
    for (int i = 0; i < cart->item_count; i++) {
        if (cart->items[i].product_id == product_id) {
            for (int j = i; j < cart->item_count - 1; j++)
                cart->items[j] = cart->items[j + 1];
            cart->item_count--;
            return true;
        }
    }
    return false;
}

bool cart_update_quantity(ShoppingCart* cart, int product_id, int qty) {
    if (!cart || qty < 0) return false;
    if (qty == 0) return cart_remove_item(cart, product_id);
    for (int i = 0; i < cart->item_count; i++) {
        if (cart->items[i].product_id == product_id) {
            cart->items[i].quantity = qty;
            return true;
        }
    }
    return false;
}

double cart_calculate_subtotal(const ShoppingCart* cart, const Inventory* inv) {
    if (!cart || !inv) return 0.0;
    double total = 0.0;
    for (int i = 0; i < cart->item_count; i++) {
        Product* p = inv_find_by_id((Inventory*)inv, cart->items[i].product_id);
        if (p) total += p->price * cart->items[i].quantity;
    }
    return total;
}

int cart_count_items(const ShoppingCart* cart) {
    if (!cart) return 0;
    int count = 0;
    for (int i = 0; i < cart->item_count; i++) count += cart->items[i].quantity;
    return count;
}

void cart_clear(ShoppingCart* cart) {
    if (!cart) return;
    cart->item_count = 0;
    cart->subtotal = 0.0;
}

void cart_destroy(ShoppingCart* cart) { free(cart); }

/* ================================================================
 * L7: Coupon / Discount Engine
 * ================================================================ */

CouponEngine* coupon_create(void) {
    return (CouponEngine*)calloc(1, sizeof(CouponEngine));
}

int coupon_add(CouponEngine* ce, const char* code, DiscountType type,
               double value, double min_order, int max_uses) {
    if (!ce || !code || ce->coupon_count >= ECOM_MAX_COUPONS) return -1;
    Coupon* c = &ce->coupons[ce->coupon_count];
    snprintf(c->code, 31, "%s", code);
    c->type = type;
    c->value = value;
    c->min_order = min_order;
    c->max_uses = max_uses;
    c->used_count = 0;
    c->active = true;
    c->expires_at = (int64_t)time(NULL) + 90 * 86400;
    ce->coupon_count++;
    return ce->coupon_count - 1;
}

bool coupon_is_valid(const CouponEngine* ce, const char* code) {
    if (!ce || !code) return false;
    for (int i = 0; i < ce->coupon_count; i++) {
        const Coupon* c = &ce->coupons[i];
        if (strcmp(c->code, code) == 0 && c->active &&
            c->used_count < c->max_uses && (int64_t)time(NULL) < c->expires_at)
            return true;
    }
    return false;
}

double coupon_apply(const CouponEngine* ce, const char* code, double order_total) {
    if (!ce || !code) return order_total;
    for (int i = 0; i < ce->coupon_count; i++) {
        const Coupon* c = &ce->coupons[i];
        if (strcmp(c->code, code) != 0 || !c->active) continue;
        if (order_total < c->min_order) return order_total;
        if (c->used_count >= c->max_uses) return order_total;

        /* Cast to non-const for mutation */
        ((Coupon*)c)->used_count++;
        if (((Coupon*)c)->used_count >= ((Coupon*)c)->max_uses) ((Coupon*)c)->active = false;

        switch (c->type) {
            case DISCOUNT_PERCENT:
                return order_total * (1.0 - c->value / 100.0);
            case DISCOUNT_FIXED:
                return order_total - c->value > 0 ? order_total - c->value : 0.0;
            case DISCOUNT_BOGO:
                return order_total * 0.5;
            case DISCOUNT_FREE_SHIPPING:
                return order_total;
            default: return order_total;
        }
    }
    return order_total;
}

void coupon_print_summary(const CouponEngine* ce) {
    if (!ce) return;
    printf("Coupons: %d active\n", ce->coupon_count);
    for (int i = 0; i < ce->coupon_count; i++) {
        const Coupon* c = &ce->coupons[i];
        printf("  %-16s type=%d value=%.1f used=%d/%d %s\n",
               c->code, c->type, c->value, c->used_count, c->max_uses,
               c->active ? "" : "(inactive)");
    }
}

void coupon_destroy(CouponEngine* ce) { free(ce); }

/* ================================================================
 * L7: Shipping Calculator
 * ================================================================ */

double shipping_calculate(double weight_kg, double distance_km, ShippingMethod method) {
    double base = 5.0 + weight_kg * 0.50 + distance_km * 0.02;
    switch (method) {
        case SHIPPING_STANDARD: return base;
        case SHIPPING_EXPRESS:  return base * 1.8;
        case SHIPPING_OVERNIGHT:return base * 3.5;
        default: return base;
    }
}

double shipping_order_weight(const Order* order, const Inventory* inv) {
    if (!order || !inv) return 0.0;
    double total = 0.0;
    for (int i = 0; i < order->line_count; i++) {
        Product* p = inv_find_by_id((Inventory*)inv, order->lines[i].product_id);
        if (p) total += p->weight_kg * order->lines[i].quantity;
    }
    return total;
}

/* ================================================================
 * L7: Recommendation Engine (Collaborative Filtering — Item-Based)
 * Builds a co-occurrence matrix: "customers who bought X also bought Y"
 * ================================================================ */

RecommendationEngine* reco_create(void) {
    RecommendationEngine* re = (RecommendationEngine*)calloc(1, sizeof(RecommendationEngine));
    return re;
}

static int reco_get_or_add_product_id(RecommendationEngine* re, int product_id) {
    for (int i = 0; i < re->product_map_count; i++) {
        if (re->product_map[product_id] == product_id) return i;
    }
    if (re->product_map_count >= ECOM_MAX_PRODUCTS) return -1;
    int idx = re->product_map_count;
    re->product_map[idx] = product_id;
    re->product_map_count++;
    return idx;
}

int reco_record_purchase(RecommendationEngine* re, const int* product_ids, int count) {
    if (!re || !product_ids || count < 2) return 0;
    for (int i = 0; i < count; i++) {
        int a = reco_get_or_add_product_id(re, product_ids[i]);
        if (a < 0) continue;
        for (int j = 0; j < count; j++) {
            if (i == j) continue;
            int b = reco_get_or_add_product_id(re, product_ids[j]);
            if (b < 0) continue;
            if (a < ECOM_MAX_PRODUCTS && b < ECOM_MAX_PRODUCTS)
                re->cooccur[a][b]++;
        }
    }
    return 0;
}

int reco_get_also_bought(RecommendationEngine* re, int product_id,
                          int* results, int max_results) {
    if (!re || !results) return 0;
    int pidx = -1;
    for (int i = 0; i < re->product_map_count; i++) {
        if (re->product_map[i] == product_id) { pidx = i; break; }
    }
    if (pidx < 0) return 0;

    /* Find top co-occurring products */
    int found = 0;

    for (int j = 0; j < re->product_map_count && found < max_results; j++) {
        if (j == pidx) continue;
        if (re->cooccur[pidx][j] > 0) {
            results[found++] = re->product_map[j];
        }
    }
    return found;
}

void reco_destroy(RecommendationEngine* re) { free(re); }

/* ================================================================
 * L5: Product Search Engine (TF-IDF)
 *
 * TF-IDF = term_frequency × inverse_document_frequency
 * TF(t,d) = count(t in d) / total_terms(d)
 * IDF(t) = log(N / df(t)) where N = total documents, df = docs with term
 *
 * Inverted index: maps terms → list of product IDs for O(1) retrieval.
 *
 * Reference: Salton & Buckley (1988) "Term-weighting approaches
 *            in automatic text retrieval", Information Processing
 *            Google PageRank original paper (Brin & Page 1998)
 * ================================================================ */

SearchIndex* search_create(void) {
    return (SearchIndex*)calloc(1, sizeof(SearchIndex));
}

static void tokenize(const char* text, char terms[][64], int* n) {
    *n = 0;
    if (!text) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", text);
    /* Lowercase */
    for (char* p = buf; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    /* Split by space/comma */
    char* tok = strtok(buf, " ,.-;:\t\n");
    while (tok && *n < ECOM_SEARCH_TERM_MAX) {
        /* Skip short words */
        if (strlen(tok) >= 2) {
            bool dup = false;
            for (int i = 0; i < *n; i++)
                if (strcmp(terms[i], tok) == 0) { dup = true; break; }
            if (!dup) { strncpy(terms[*n], tok, 63); (*n)++; }
        }
        tok = strtok(NULL, " ,.-;:\t\n");
    }
}

int search_index_product(SearchIndex* si, int product_id, const char* text) {
    if (!si || !text) return 0;
    char terms[ECOM_SEARCH_TERM_MAX][64];
    int nterms;
    tokenize(text, terms, &nterms);
    si->total_docs++;

    for (int t = 0; t < nterms; t++) {
        /* Find or create term entry */
        int ti = -1;
        for (int i = 0; i < si->term_count; i++)
            if (strcmp(si->terms[i].term, terms[t]) == 0) { ti = i; break; }
        if (ti < 0) {
            if (si->term_count >= ECOM_SEARCH_INDEX_SIZE) continue;
            ti = si->term_count++;
            strncpy(si->terms[ti].term, terms[t], 63);
            si->terms[ti].doc_count = 0;
        }
        /* Add product to posting list if not already there */
        SearchTerm* st = &si->terms[ti];
        bool found = false;
        for (int p = 0; p < st->doc_count; p++)
            if (st->product_ids[p] == product_id) { found = true; break; }
        if (!found && st->doc_count < ECOM_MAX_PRODUCTS) {
            st->product_ids[st->doc_count++] = product_id;
        }
    }
    /* Recompute IDF for all terms */
    for (int i = 0; i < si->term_count; i++) {
        if (si->terms[i].doc_count > 0)
            si->terms[i].idf = log((double)si->total_docs / (double)si->terms[i].doc_count);
    }
    return nterms;
}

int search_query(const SearchIndex* si, const char* query, int* results, int max_results) {
    return search_query_ranked(si, query, results, NULL, max_results);
}

int search_query_ranked(const SearchIndex* si, const char* query,
                         int* results, double* scores, int max_results) {
    if (!si || !query || !results) return 0;
    char terms[ECOM_SEARCH_TERM_MAX][64];
    int nterms;
    tokenize(query, terms, &nterms);

    /* For each product, compute TF-IDF score */
    int prod_count = 0;
    int prod_ids[ECOM_MAX_PRODUCTS];
    double prod_scores[ECOM_MAX_PRODUCTS];

    for (int i = 0; i < si->total_docs && prod_count < ECOM_MAX_PRODUCTS; i++) {
        int pid = i + 1;
        double score = 0.0;
        int total_terms = 0;

        for (int t = 0; t < nterms; t++) {
            for (int j = 0; j < si->term_count; j++) {
                if (strcmp(si->terms[j].term, terms[t]) == 0) {
                    /* Check if this product is in posting list */
                    bool has_term = false;
                    for (int p = 0; p < si->terms[j].doc_count; p++)
                        if (si->terms[j].product_ids[p] == pid) { has_term = true; break; }
                    if (has_term) {
                        score += si->terms[j].idf;
                    }
                    break;
                }
            }
        }
        if (score > 0.0) {
            prod_ids[prod_count] = pid;
            prod_scores[prod_count] = score;
            prod_count++;
        }
    }

    /* Sort by score descending */
    for (int i = 0; i < prod_count - 1; i++)
        for (int j = i + 1; j < prod_count; j++)
            if (prod_scores[j] > prod_scores[i]) {
                double ts = prod_scores[i];
                prod_scores[i] = prod_scores[j];
                prod_scores[j] = ts;
                int ti = prod_ids[i];
                prod_ids[i] = prod_ids[j];
                prod_ids[j] = ti;
            }

    int n = prod_count < max_results ? prod_count : max_results;
    for (int i = 0; i < n; i++) {
        results[i] = prod_ids[i];
        if (scores) scores[i] = prod_scores[i];
    }
    return n;
}

void search_destroy(SearchIndex* si) { free(si); }

/* ================================================================
 * L7: Customer Review System
 *
 * Aggregates user reviews with verified purchase badges.
 * Average rating: simple arithmetic mean of all ratings.
 * Bayesian average (for small sample correction):
 *   score = (C×m + Σx_i) / (C + n)
 * where C = prior count (e.g., 10), m = prior mean (e.g., 3.0)
 * ================================================================ */

ReviewSystem* review_create(void) {
    return (ReviewSystem*)calloc(1, sizeof(ReviewSystem));
}

int review_add(ReviewSystem* rs, int product_id, int customer_id,
                int rating, const char* title, const char* body,
                bool verified) {
    if (!rs || rs->review_count >= ECOM_MAX_ORDERS) return -1;
    if (rating < 1) rating = 1;
    if (rating > 5) rating = 5;
    CustomerReview* r = &rs->reviews[rs->review_count];
    r->product_id = product_id;
    r->customer_id = customer_id;
    r->rating = rating;
    if (title) strncpy(r->title, title, ECOM_MAX_NAME - 1);
    if (body)  strncpy(r->body, body, 511);
    r->created_at = (int64_t)time(NULL);
    r->verified_purchase = verified;
    rs->review_count++;
    return rs->review_count - 1;
}

double review_avg_rating(const ReviewSystem* rs, int product_id) {
    if (!rs) return 0.0;
    double sum = 0.0;
    int count = 0;
    for (int i = 0; i < rs->review_count; i++) {
        if (rs->reviews[i].product_id == product_id) {
            sum += (double)rs->reviews[i].rating;
            count++;
        }
    }
    /* Bayesian average with prior: 10 reviews at 3.0 */
    double C = 10.0, m = 3.0;
    return (C * m + sum) / (C + (double)count);
}

int review_count_for_product(const ReviewSystem* rs, int product_id) {
    if (!rs) return 0;
    int count = 0;
    for (int i = 0; i < rs->review_count; i++)
        if (rs->reviews[i].product_id == product_id) count++;
    return count;
}

void review_destroy(ReviewSystem* rs) { free(rs); }

/* ================================================================
 * L8: Inventory Forecasting (Holt's Exponential Smoothing)
 *
 * Captures both level and trend in time series data.
 * Optimal for short-term demand forecasting with trend.
 *
 * Level: L_t = α·Y_t + (1-α)(L_{t-1} + T_{t-1})
 * Trend: T_t = β·(L_t - L_{t-1}) + (1-β)·T_{t-1}
 * Forecast: F_{t+m} = L_t + m·T_t
 *
 * Reference: Holt (1957, 2004) "Forecasting seasonals and trends"
 *            Hyndman & Athanasopoulos (2018) "Forecasting: Principles & Practice"
 * ================================================================ */

void holt_winters_init(HoltWinters* hw, double alpha, double beta) {
    if (!hw) return;
    hw->alpha = alpha > 0.0 ? alpha : 0.3;
    hw->beta = beta > 0.0 ? beta : 0.1;
    hw->level = 0.0;
    hw->trend = 0.0;
}

double holt_winters_update(HoltWinters* hw, double value) {
    if (!hw) return 0.0;
    if (hw->level == 0.0) {
        hw->level = value;
        return value;
    }
    double new_level = hw->alpha * value + (1.0 - hw->alpha) * (hw->level + hw->trend);
    double new_trend = hw->beta * (new_level - hw->level) + (1.0 - hw->beta) * hw->trend;
    hw->level = new_level;
    hw->trend = new_trend;
    return hw->level;
}

double holt_winters_forecast(const HoltWinters* hw, int steps_ahead) {
    if (!hw) return 0.0;
    return hw->level + (double)steps_ahead * hw->trend;
}

/* ================================================================
 * L6: Flash Sale / Promotion Engine
 *
 * Time-limited discounts with inventory caps. Flash sales
 * create urgency and drive conversion (FOMO effect).
 * Shopify reports flash sales can boost conversions 3-5x.
 * ================================================================ */

PromotionEngine* promo_create(void) {
    return (PromotionEngine*)calloc(1, sizeof(PromotionEngine));
}

int promo_create_sale(PromotionEngine* pe, int product_id, double discount_pct,
                       int max_units, int64_t duration_seconds) {
    if (!pe || pe->sale_count >= ECOM_MAX_ITEMS) return -1;
    FlashSale* fs = &pe->sales[pe->sale_count];
    fs->product_id = product_id;
    fs->discount_pct = discount_pct;
    fs->max_units = max_units;
    fs->sold_units = 0;
    fs->start_time = (int64_t)time(NULL);
    fs->end_time = fs->start_time + duration_seconds;
    fs->active = true;
    pe->sale_count++;
    return pe->sale_count - 1;
}

bool promo_is_active(const FlashSale* sale) {
    if (!sale || !sale->active) return false;
    int64_t now = (int64_t)time(NULL);
    return (now >= sale->start_time && now < sale->end_time
            && (sale->sold_units < sale->max_units || sale->max_units == 0));
}

bool promo_reserve(FlashSale* sale, int qty) {
    if (!sale || !promo_is_active(sale)) return false;
    if (sale->max_units > 0 && sale->sold_units + qty > sale->max_units) return false;
    sale->sold_units += qty;
    return true;
}

void promo_destroy(PromotionEngine* pe) { free(pe); }

/* ================================================================
 * L7: Warehouse & Fulfillment Network
 *
 * Multi-warehouse inventory allocation with nearest-warehouse
 * routing for shipping cost optimization.
 *
 * Haversine distance for warehouse-to-customer routing.
 * Multi-warehouse fulfillment reduces shipping time and cost
 * (Amazon operates 185+ fulfillment centers globally).
 *
 * Reference: Simchi-Levi, Kaminsky & Simchi-Levi (2008)
 *            "Designing and Managing the Supply Chain" 3rd ed.
 * ================================================================ */

FulfillmentNetwork* fulfillment_create(void) {
    return (FulfillmentNetwork*)calloc(1, sizeof(FulfillmentNetwork));
}

int fulfillment_add_warehouse(FulfillmentNetwork* fn, const char* name,
                                const char* location, double capacity) {
    if (!fn || fn->warehouse_count >= 32) return -1;
    Warehouse* w = &fn->warehouses[fn->warehouse_count];
    w->warehouse_id = fn->warehouse_count + 1;
    if (name) strncpy(w->name, name, ECOM_MAX_NAME - 1);
    if (location) strncpy(w->location, location, ECOM_MAX_NAME - 1);
    w->capacity_m3 = capacity;
    w->used_m3 = 0.0;
    w->lat = 40.0 + (double)(fn->warehouse_count % 10);
    w->lon = -100.0 + (double)(fn->warehouse_count % 20);
    fn->warehouse_count++;
    return w->warehouse_id;
}

int fulfillment_assign_product(FulfillmentNetwork* fn, int product_id, int warehouse_id) {
    if (!fn || product_id <= 0 || product_id > ECOM_MAX_PRODUCTS) return -1;
    if (warehouse_id < 1 || warehouse_id > fn->warehouse_count) return -1;
    fn->product_warehouse[product_id] = warehouse_id;
    return 0;
}

static double ecom_pi(void) { return 3.14159265358979323846; }

static double haversine_km(double lat1, double lon1, double lat2, double lon2) {
    double pi = ecom_pi();
    double dlat = (lat2 - lat1) * pi / 180.0;
    double dlon = (lon2 - lon1) * pi / 180.0;
    double a = sin(dlat * 0.5) * sin(dlat * 0.5)
             + cos(lat1 * pi / 180.0) * cos(lat2 * pi / 180.0)
             * sin(dlon * 0.5) * sin(dlon * 0.5);
    return 6371.0 * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
}

int fulfillment_nearest_warehouse(const FulfillmentNetwork* fn, double lat, double lon) {
    if (!fn || fn->warehouse_count == 0) return -1;
    int best = 0;
    double best_dist = 1e9;
    for (int i = 0; i < fn->warehouse_count; i++) {
        double dist = haversine_km(lat, lon, fn->warehouses[i].lat, fn->warehouses[i].lon);
        if (dist < best_dist) { best_dist = dist; best = i; }
    }
    return best + 1;
}

double fulfillment_shipping_distance(const FulfillmentNetwork* fn,
                                       int product_id, double dest_lat, double dest_lon) {
    if (!fn || product_id <= 0 || product_id > ECOM_MAX_PRODUCTS) return 100.0;
    int wh_id = fn->product_warehouse[product_id];
    if (wh_id < 1 || wh_id > fn->warehouse_count) {
        wh_id = fulfillment_nearest_warehouse(fn, dest_lat, dest_lon);
        if (wh_id < 1) return 100.0;
    }
    Warehouse* w = &fn->warehouses[wh_id - 1];
    return haversine_km(dest_lat, dest_lon, w->lat, w->lon);
}

void fulfillment_destroy(FulfillmentNetwork* fn) { free(fn); }

/* ================================================================
 * L8: Dynamic Pricing Engine
 *
 * Adjusts prices in response to demand, inventory levels,
 * and competitive landscape. Core techniques:
 *
 * 1. Elasticity-based: price × (1 + η × (demand - 1))
 *    where η = price elasticity of demand (negative for normal goods)
 * 2. Surge pricing: multiplicative factor when demand spikes
 *    and inventory is critically low
 * 3. Markdown optimization: clearance pricing as product ages
 *
 * Reference: Gallego & van Ryzin (1994) "Optimal dynamic pricing
 *            of inventories with stochastic demand"
 *            Phillips (2005) "Pricing and Revenue Optimization"
 * ================================================================ */

double dynamic_price_elasticity(double base_price, double demand_index,
                                 double inventory_pct, double competitor_price) {
    double eta = -0.7; /* Typical elasticity for retail goods */
    double demand_factor = 1.0 + eta * (demand_index - 1.0);
    if (demand_factor < 0.6) demand_factor = 0.6;
    if (demand_factor > 1.4) demand_factor = 1.4;

    /* Scarcity multiplier: low inventory → raise price */
    double scarcity = 1.0;
    if (inventory_pct < 0.2) scarcity = 1.0 + (0.2 - inventory_pct) * 1.5;
    if (scarcity > 1.3) scarcity = 1.3;

    /* Competitive pressure: match or beat competitor */
    double comp_factor = 1.0;
    if (competitor_price > 0.0 && competitor_price < base_price) {
        comp_factor = 0.95;
    }
    return base_price * demand_factor * scarcity * comp_factor;
}

double dynamic_price_surge(double base_price, double demand_surge_factor,
                             int stock_remaining, int reorder_point) {
    if (demand_surge_factor <= 1.0) return base_price;
    double surge = 1.0 + (demand_surge_factor - 1.0) * 0.5;
    if (stock_remaining < reorder_point)
        surge += 0.2 * (1.0 - (double)stock_remaining / (double)(reorder_point + 1));
    if (surge > 3.0) surge = 3.0;
    return base_price * surge;
}

double dynamic_price_markdown(double original_price, int days_on_shelf,
                                int shelf_life_days) {
    if (days_on_shelf < 0) days_on_shelf = 0;
    if (shelf_life_days <= 0) shelf_life_days = 90;
    /* Linear markdown schedule: full price at day 0, 50% at shelf life */
    double remaining = 1.0 - (double)days_on_shelf / (double)shelf_life_days;
    if (remaining < 0.2) remaining = 0.2;
    /* Accelerated markdown near end of life */
    if (days_on_shelf > shelf_life_days * 0.7)
        remaining = 0.5 * remaining;
    return original_price * remaining;
}
