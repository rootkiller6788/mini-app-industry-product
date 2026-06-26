#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "ecommerce_core.h"
#include "ecommerce_payment.h"

int main(void) {
    printf("=== Example 1: End-to-End Checkout Flow ===\n\n");

    /* 1. Setup inventory */
    Inventory* inv = inv_create();
    inv_add_product(inv, "Laptop Pro 15", "LP-001", 1299.99, 800.00, 50, 2.0);
    inv_add_product(inv, "Wireless Mouse", "WM-002", 29.99, 12.00, 200, 0.1);
    inv_add_product(inv, "USB-C Dongle", "UC-003", 39.99, 18.00, 100, 0.05);
    inv_print_summary(inv);

    /* 2. Setup order system */
    OrderSystem* os = osys_create(inv);
    Customer* cust = osys_add_customer(os, "Jane Doe", "jane@example.com");
    printf("\nCustomer created: %s (ID: %d)\n", cust->name, cust->id);

    /* 3. Create shopping cart and add items */
    ShoppingCart* cart = cart_create(cust->id);
    cart_add_item(cart, 1, 1);  /* Laptop */
    cart_add_item(cart, 2, 2);  /* 2 Mice */
    cart_add_item(cart, 3, 1);  /* Dongle */
    double subtotal = cart_calculate_subtotal(cart, inv);
    printf("Cart: %d items, Subtotal: $%.2f\n", cart_count_items(cart), subtotal);

    /* 4. Apply coupon */
    CouponEngine* coupons = coupon_create();
    coupon_add(coupons, "WELCOME10", DISCOUNT_PERCENT, 10.0, 100.0, 50);
    double after_coupon = coupon_apply(coupons, "WELCOME10", subtotal);
    printf("After coupon: $%.2f (saved $%.2f)\n", after_coupon, subtotal - after_coupon);

    /* 5. Calculate shipping */
    double weight = 2.0 + 0.2 + 0.05;  /* Laptop + 2 mice + dongle */
    double shipping = shipping_calculate(weight, 500.0, SHIPPING_STANDARD);
    printf("Shipping (Standard): $%.2f\n", shipping);

    /* 6. Create order */
    Order* order = osys_create_order(os, cust->id);
    osys_add_line(order, 1, 1, 1299.99);
    osys_add_line(order, 2, 2, 29.99);
    osys_add_line(order, 3, 1, 39.99);
    osys_confirm_order(os, order->id);
    osys_ship_order(os, order->id);
    osys_deliver_order(os, order->id);
    osys_print_order(order);

    /* 7. Process payment */
    PaymentEngine* pe = payment_engine_create();
    payment_gateway_register(pe, "Stripe", 2.9, 0.30, true, true);
    PaymentTransaction* tx = payment_authorize(pe, order->id, PM_CREDIT_CARD,
                                                after_coupon + shipping, "USD", "4242");
    payment_capture(pe, tx->id);
    payment_settle(pe, tx->id);

    PaymentReport report;
    payment_generate_report(pe, &report);
    payment_print_report(&report);

    /* 8. Cleanup */
    cart_destroy(cart);
    coupon_destroy(coupons);
    payment_engine_destroy(pe);
    osys_destroy(os);
    inv_destroy(inv);

    printf("\n=== Checkout flow completed successfully ===\n");
    return 0;
}