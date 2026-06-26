/* test_runner.c ? Master test runner for mini-enterprise-system */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* Forward declarations from individual test suites */
int test_erp_suite(void);
int test_mrp_suite(void);
int test_hrm_suite(void);
int test_crm_suite(void);
int test_fin_suite(void);

int main(void) {
    int failures = 0;
    printf("\n");
    printf("========================================\n");
    printf("  mini-enterprise-system Test Runner\n");
    printf("========================================\n");
    
    printf("\n[1/5] ERP Core Tests\n");
    if (test_erp_suite() != 0) failures++;
    
    printf("\n[2/5] MRP Engine Tests\n");
    if (test_mrp_suite() != 0) failures++;
    
    printf("\n[3/5] HRM System Tests\n");
    if (test_hrm_suite() != 0) failures++;
    
    printf("\n[4/5] CRM Pipeline Tests\n");
    if (test_crm_suite() != 0) failures++;
    
    printf("\n[5/5] Financial Reporting Tests\n");
    if (test_fin_suite() != 0) failures++;
    
    printf("\n========================================\n");
    if (failures == 0) {
        printf("  ALL TEST SUITES PASSED\n");
    } else {
        printf("  %d SUITE(S) FAILED\n", failures);
    }
    printf("========================================\n\n");
    
    return failures;
}
