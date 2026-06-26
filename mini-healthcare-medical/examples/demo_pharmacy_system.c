#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "pharmacy_mgmt.h"

int main(void) {
    printf("=== Pharmacy Management Demo ===\n");
    PharmacySystem *ps = pharm_system_create();
    assert(ps);

    int amox_id = pharm_add_drug(ps, "Amoxicillin", "Amoxil", 500, 1000, 100);
    int warf_id = pharm_add_drug(ps, "Warfarin", "Coumadin", 5, 500, 50);
    int metf_id = pharm_add_drug(ps, "Metformin", "Glucophage", 1000, 2000, 200);
    printf("Drug catalog: %d items\n", ps->drug_count);

    int rx1 = pharm_create_prescription(ps, 1, amox_id, 101, 500,
        PHARM_ROUTE_ORAL, PHARM_SCHEDULE_TID, 7, 0);
    assert(rx1 > 0);
    assert(pharm_verify_prescription(ps, rx1));
    printf("Rx #%d verified\n", rx1);

    assert(pharm_dispense(ps, rx1, 1, 21, "LOT-2026-001",
           time(NULL) + 86400 * 365));
    printf("Dispensed 21 tablets\n");

    int rx2 = pharm_create_prescription(ps, 1, amox_id, 102, 500,
        PHARM_ROUTE_ORAL, PHARM_SCHEDULE_TID, 7, 0);
    printf("Duplicate therapy for pt1+Amox: %s\n",
           pharm_check_duplicate_therapy(ps, 1, amox_id) ? "YES" : "NO");
    (void)rx2;

    ps->drugs[warf_id - 1].stock_quantity = 30;
    printf("Warfarin needs reorder: %s\n",
           pharm_needs_reorder(ps, warf_id) ? "YES" : "NO");
    printf("Reorder quantity: %llu\n",
           (unsigned long long)pharm_reorder_quantity(ps, warf_id));

    MedRecEntry admit_meds[3] = {
        {amox_id, 500, PHARM_ROUTE_ORAL, PHARM_SCHEDULE_TID, false, ""},
        {warf_id, 5, PHARM_ROUTE_ORAL, PHARM_SCHEDULE_QD, false, ""},
        {metf_id, 1000, PHARM_ROUTE_ORAL, PHARM_SCHEDULE_BID, false, ""},
    };
    pharm_create_prescription(ps, 1, warf_id, 101, 5,
        PHARM_ROUTE_ORAL, PHARM_SCHEDULE_QD, 30, 0);
    MedRecEntry reconciled[3];
    int n = pharm_medication_reconciliation(ps, 1, admit_meds, 3, reconciled);
    printf("Med rec: %d reviewed\n", n);
    for (int i = 0; i < n; i++)
        printf("  Drug %d: %s\n", reconciled[i].drug_id,
               reconciled[i].reconciled ? "RECONCILED" : "NOT IN ACTIVE LIST");

    /* Pharmacokinetics */
    double kel = pharm_half_life_to_elimination(4.0);
    printf("PK: kel=%.4f h^-1, steady state=%.1fh\n",
           kel, pharm_steady_state_time(4.0));
    printf("Loading dose (target=10, Vd=30, F=0.8): %.1f mg\n",
           pharm_loading_dose(10.0, 30.0, 0.8));
    printf("Maintenance dose (target=10, Cl=5, int=8h, F=0.8): %.1f mg\n",
           pharm_maintenance_dose(10.0, 5.0, 8.0, 0.8));
    double peak = pharm_peak_concentration(500, 30, 0.8);
    printf("Peak conc: %.2f ug/mL, Trough: %.2f ug/mL\n",
           peak, pharm_trough_concentration(peak, kel, 8.0));

    /* Pharmacogenomics */
    printf("Pharmacogenomics dose adjustment:\n");
    printf("  Poor metabolizer: %.0f%% of standard dose\n",
           pharm_genomic_dose_adjustment(100.0, PHARM_METABOLIZER_POOR));
    printf("  Ultra-rapid: %.0f%% of standard dose\n",
           pharm_genomic_dose_adjustment(100.0, PHARM_METABOLIZER_ULTRA));
    for (int i = 0; i < 4; i++)
        printf("  %s\n", pharm_metabolizer_label((PharmMetabolizer)i));

    /* Inventory report */
    int low_ids[10];
    int low_count = pharm_get_low_stock_drugs(ps, low_ids, 10);
    printf("Low stock drugs: %d\n", low_count);
    printf("Active prescriptions for patient 1: %d\n",
           pharm_get_active_rx_count(ps, 1));

    pharm_system_destroy(ps);
    printf("All pharmacy demos passed.\n");
    return 0;
}
