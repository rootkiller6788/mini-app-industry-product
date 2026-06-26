#include "pharmacy_mgmt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

PharmacySystem* pharm_system_create(void) {
    PharmacySystem *sys = malloc(sizeof(PharmacySystem));
    if (sys) memset(sys, 0, sizeof(PharmacySystem));
    return sys;
}

void pharm_system_destroy(PharmacySystem *sys) { free(sys); }

void pharm_system_init(PharmacySystem *sys) {
    if (sys) memset(sys, 0, sizeof(PharmacySystem));
}

int pharm_add_drug(PharmacySystem *sys, const char *generic, const char *brand,
                    uint64_t strength_mg, uint64_t stock, uint64_t reorder_point) {
    if (!sys || !generic || sys->drug_count >= PHARM_MAX_DRUGS) return -1;
    DrugInventory *d = &sys->drugs[sys->drug_count];
    d->id = sys->drug_count + 1;
    snprintf(d->generic_name, sizeof(d->generic_name), "%s", generic);
    snprintf(d->brand_name, sizeof(d->brand_name), "%s", brand ? brand : "");
    d->strength_mg = strength_mg;
    d->stock_quantity = stock;
    d->reorder_threshold = reorder_point;
    d->requires_refrigeration = false;
    d->controlled_substance = false;
    d->default_route = PHARM_ROUTE_ORAL;
    d->half_life_hours = 4.0;
    sys->drug_count++;
    return d->id;
}

DrugInventory* pharm_find_drug(PharmacySystem *sys, int drug_id) {
    if (!sys || drug_id <= 0 || drug_id > sys->drug_count) return NULL;
    return &sys->drugs[drug_id - 1];
}

bool pharm_has_stock(const PharmacySystem *sys, int drug_id, uint64_t quantity_needed) {
    DrugInventory *d = (DrugInventory*)pharm_find_drug((PharmacySystem*)sys, drug_id);
    if (!d) return false;
    return d->stock_quantity >= quantity_needed;
}

int pharm_create_prescription(PharmacySystem *sys, int patient_id, int drug_id,
                               int prescriber_id, uint64_t dose_mg, PharmRoute route,
                               PharmSchedule schedule, uint64_t duration_days, int refills) {
    if (!sys || sys->rx_count >= PHARM_MAX_PRESCRIPTIONS) return -1;
    DrugInventory *drug = pharm_find_drug(sys, drug_id);
    if (!drug) return -1;
    Prescription *rx = &sys->prescriptions[sys->rx_count];
    rx->id = sys->rx_count + 1;
    rx->patient_id = patient_id;
    rx->drug_id = drug_id;
    rx->prescriber_id = prescriber_id;
    rx->dose_mg = dose_mg;
    rx->route = route;
    rx->schedule = schedule;
    rx->duration_days = duration_days;
    rx->quantity = dose_mg * duration_days / drug->strength_mg;
    if (rx->quantity < 1) rx->quantity = 1;
    rx->refills = refills;
    rx->prescribed_at = time(NULL);
    rx->start_date = rx->prescribed_at;
    rx->end_date = rx->prescribed_at + duration_days * 86400;
    rx->status = PHARM_STATUS_ACTIVE;
    rx->requires_prior_auth = false;
    sys->rx_count++;
    return rx->id;
}

bool pharm_dispense(PharmacySystem *sys, int rx_id, int pharmacist_id,
                     uint64_t quantity, const char *lot, time_t expiry) {
    if (!sys || rx_id <= 0 || rx_id > sys->rx_count) return false;
    if (sys->dispense_count >= PHARM_MAX_DISPENSES) return false;
    Prescription *rx = &sys->prescriptions[rx_id - 1];
    if (rx->status != PHARM_STATUS_ACTIVE) return false;
    DrugInventory *drug = pharm_find_drug(sys, rx->drug_id);
    if (!drug || drug->stock_quantity < quantity) return false;
    drug->stock_quantity -= quantity;
    DispenseRecord *dr = &sys->dispenses[sys->dispense_count];
    dr->id = sys->dispense_count + 1;
    dr->prescription_id = rx_id;
    dr->drug_id = rx->drug_id;
    dr->patient_id = rx->patient_id;
    dr->quantity_dispensed = quantity;
    dr->dispensed_by_id = pharmacist_id;
    dr->dispensed_at = time(NULL);
    if (lot) snprintf(dr->lot_number, sizeof(dr->lot_number), "%s", lot);
    dr->expiration_date = expiry;
    dr->counseled = true;
    sys->dispense_count++;
    return true;
}

bool pharm_discontinue_rx(PharmacySystem *sys, int rx_id) {
    if (!sys || rx_id <= 0 || rx_id > sys->rx_count) return false;
    sys->prescriptions[rx_id - 1].status = PHARM_STATUS_DISCONTINUED;
    return true;
}

bool pharm_verify_prescription(const PharmacySystem *sys, int rx_id) {
    if (!sys || rx_id <= 0 || rx_id > sys->rx_count) return false;
    const Prescription *rx = &sys->prescriptions[rx_id - 1];
    return rx->status == PHARM_STATUS_ACTIVE && rx->prescriber_id > 0;
}

bool pharm_check_duplicate_therapy(const PharmacySystem *sys, int patient_id,
                                    int new_drug_id) {
    if (!sys) return false;
    for (int i = 0; i < sys->rx_count; i++) {
        if (sys->prescriptions[i].patient_id == patient_id &&
            sys->prescriptions[i].drug_id == new_drug_id &&
            sys->prescriptions[i].status == PHARM_STATUS_ACTIVE) {
            return true;
        }
    }
    return false;
}

int pharm_get_active_rx_count(const PharmacySystem *sys, int patient_id) {
    if (!sys) return 0;
    int count = 0;
    for (int i = 0; i < sys->rx_count; i++) {
        if (sys->prescriptions[i].patient_id == patient_id &&
            sys->prescriptions[i].status == PHARM_STATUS_ACTIVE) count++;
    }
    return count;
}

int pharm_get_patient_prescriptions(const PharmacySystem *sys, int patient_id,
                                     int *rx_ids, int max_count) {
    if (!sys || !rx_ids) return 0;
    int count = 0;
    for (int i = 0; i < sys->rx_count && count < max_count; i++) {
        if (sys->prescriptions[i].patient_id == patient_id) {
            rx_ids[count++] = sys->prescriptions[i].id;
        }
    }
    return count;
}

bool pharm_needs_reorder(const PharmacySystem *sys, int drug_id) {
    if (!sys) return false;
    DrugInventory *d = (DrugInventory*)pharm_find_drug((PharmacySystem*)sys, drug_id);
    return d && d->stock_quantity <= d->reorder_threshold;
}

uint64_t pharm_reorder_quantity(const PharmacySystem *sys, int drug_id) {
    DrugInventory *d = (DrugInventory*)pharm_find_drug((PharmacySystem*)sys, drug_id);
    if (!d) return 0;
    return d->reorder_threshold * 3;
}

void pharm_process_reorder(PharmacySystem *sys, int drug_id, uint64_t quantity) {
    DrugInventory *d = pharm_find_drug(sys, drug_id);
    if (d) d->stock_quantity += quantity;
}

int pharm_get_low_stock_drugs(const PharmacySystem *sys, int *drug_ids, int max_count) {
    if (!sys || !drug_ids) return 0;
    int count = 0;
    for (int i = 0; i < sys->drug_count && count < max_count; i++) {
        if (sys->drugs[i].stock_quantity <= sys->drugs[i].reorder_threshold) {
            drug_ids[count++] = sys->drugs[i].id;
        }
    }
    return count;
}

int pharm_medication_reconciliation(const PharmacySystem *sys, int patient_id,
                                     const MedRecEntry *admission_meds, int count,
                                     MedRecEntry *reconciled) {
    if (!sys || !admission_meds || !reconciled) return 0;
    int rec_count = 0;
    for (int i = 0; i < count; i++) {
        reconciled[i] = admission_meds[i];
        bool found = false;
        for (int j = 0; j < sys->rx_count; j++) {
            if (sys->prescriptions[j].patient_id == patient_id &&
                sys->prescriptions[j].drug_id == admission_meds[i].drug_id &&
                sys->prescriptions[j].status == PHARM_STATUS_ACTIVE) {
                found = true;
                break;
            }
        }
        reconciled[i].reconciled = found;
        if (!found) {
            snprintf(reconciled[i].discrepancy, sizeof(reconciled[i].discrepancy),
                     "Not in active medication list — needs verification");
        }
        rec_count++;
    }
    return rec_count;
}

bool pharm_check_allergies(const PharmacySystem *sys, int patient_id, int drug_id) {
    (void)sys; (void)patient_id; (void)drug_id;
    return false;
}

double pharm_half_life_to_elimination(double half_life_h) {
    return half_life_h * log(100.0 / 1.0) / log(2.0);
}

double pharm_steady_state_time(double half_life_h) {
    return 4.0 * half_life_h;
}

double pharm_loading_dose(double target_conc, double vd, double bioavailability) {
    if (bioavailability <= 0) return 0.0;
    return (target_conc * vd) / bioavailability;
}

double pharm_maintenance_dose(double target_conc, double clearance,
                               double interval_h, double bioavailability) {
    if (bioavailability <= 0) return 0.0;
    return (target_conc * clearance * interval_h) / bioavailability;
}

double pharm_peak_concentration(double dose, double vd, double bioavailability) {
    if (vd <= 0) return 0.0;
    return (dose * bioavailability) / vd;
}

double pharm_trough_concentration(double peak, double kel, double interval_h) {
    return peak * exp(-kel * interval_h);
}

double pharm_genomic_dose_adjustment(double standard_dose, PharmMetabolizer phenotype) {
    switch (phenotype) {
        case PHARM_METABOLIZER_POOR:    return standard_dose * 0.25;
        case PHARM_METABOLIZER_INTERMEDIATE: return standard_dose * 0.60;
        case PHARM_METABOLIZER_EXTENSIVE:    return standard_dose;
        case PHARM_METABOLIZER_ULTRA:   return standard_dose * 1.50;
        default: return standard_dose;
    }
}

const char* pharm_metabolizer_label(PharmMetabolizer p) {
    switch (p) {
        case PHARM_METABOLIZER_POOR:    return "Poor Metabolizer — Reduce dose, monitor for toxicity";
        case PHARM_METABOLIZER_INTERMEDIATE: return "Intermediate Metabolizer — Consider 50-75% dose reduction";
        case PHARM_METABOLIZER_EXTENSIVE:    return "Extensive Metabolizer — Standard dosing";
        case PHARM_METABOLIZER_ULTRA:   return "Ultra-Rapid Metabolizer — May need higher dose or alternative drug";
        default: return "Unknown phenotype";
    }
}
