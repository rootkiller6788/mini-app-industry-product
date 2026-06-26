#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ecommerce_core.h"
#include "ecommerce_search.h"

int main(void) {
    printf("=== Example 2: Product Search Engine ===\n\n");

    /* Setup inventory with diverse products */
    Inventory* inv = inv_create();
    inv_add_product(inv, "Wireless Bluetooth Mouse", "WBM-001", 29.99, 12.00, 100, 0.12);
    strncpy(inv->products[0].category, "Electronics/Peripherals", 63);
    inv->products[0].rating = 4.5;

    inv_add_product(inv, "Mechanical Gaming Keyboard", "MGK-002", 129.99, 65.00, 50, 0.85);
    strncpy(inv->products[1].category, "Electronics/Peripherals", 63);
    inv->products[1].rating = 4.7;

    inv_add_product(inv, "USB-C Charging Cable 2m", "UCC-003", 12.99, 4.00, 300, 0.03);
    strncpy(inv->products[2].category, "Electronics/Accessories", 63);
    inv->products[2].rating = 4.2;

    inv_add_product(inv, "Wireless Ergonomic Mouse", "WEM-004", 49.99, 22.00, 80, 0.14);
    strncpy(inv->products[3].category, "Electronics/Peripherals", 63);
    inv->products[3].rating = 4.3;

    inv_add_product(inv, "Laptop Cooling Stand", "LCS-005", 34.99, 15.00, 60, 0.50);
    strncpy(inv->products[4].category, "Electronics/Accessories", 63);
    inv->products[4].rating = 4.0;

    /* Build search index */
    const char* descs[] = {
        "Rechargeable wireless Bluetooth mouse with ergonomic design",
        "RGB mechanical gaming keyboard with Cherry MX Blue switches",
        "Fast charging USB-C to USB-C cable 2 meters nylon braided",
        "Wireless ergonomic vertical mouse reduces wrist strain",
        "Adjustable aluminum laptop cooling stand with dual USB fans"
    };
    InvertedIndex* idx = index_create();
    index_build_from_inventory(idx, inv, descs);
    index_print_summary(idx);

    /* Search 1: "wireless mouse" */
    printf("\n--- Search: \"wireless mouse\" ---\n");
    SearchQuery q1;
    memset(&q1, 0, sizeof(q1));
    strncpy(q1.keywords, "wireless mouse", 255);
    q1.page = 1;
    q1.page_size = 10;
    SearchResponse r1;
    search_execute(idx, &q1, inv, &r1);
    search_response_print(&r1);

    /* Search 2: "keyboard gaming" with price filter */
    printf("\n--- Search: \"keyboard gaming\" (under $150) ---\n");
    SearchQuery q2;
    memset(&q2, 0, sizeof(q2));
    strncpy(q2.keywords, "keyboard gaming", 255);
    q2.max_price = 150.0;
    q2.page = 1;
    SearchResponse r2;
    search_execute(idx, &q2, inv, &r2);
    search_response_print(&r2);

    /* Search 3: Fuzzy search for typo */
    printf("\n--- Fuzzy Search: \"mouce\" ---\n");
    SpellCorrection corr;
    if (did_you_mean(idx, "mouce", &corr)) {
        printf("Did you mean: \"%s\"? (similarity: %.2f)\n",
               corr.suggestion, corr.similarity);
    }

    /* Search 4: Faceted search */
    printf("\n--- Faceted Search: all \"Electronics\" ---\n");
    SearchQuery q3;
    memset(&q3, 0, sizeof(q3));
    strncpy(q3.category, "Electronics", 63);
    q3.sort = SORT_PRICE_ASC;
    SearchResponse r3;
    search_execute(idx, &q3, inv, &r3);
    search_response_print(&r3);

    FacetResult facets;
    search_facets_compute(&r3, inv, &facets);
    printf("Facets (%d):\n", facets.facet_count);
    for (int i = 0; i < facets.facet_count; i++) {
        printf("  %s: %d products ($%.2f - $%.2f)\n",
               facets.facets[i].category, facets.facets[i].count,
               facets.facets[i].min_price, facets.facets[i].max_price);
    }

    index_destroy(idx);
    inv_destroy(inv);
    printf("\n=== Search example completed ===\n");
    return 0;
}