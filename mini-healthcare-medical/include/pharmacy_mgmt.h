#ifndef PHARMACY_MGMT_H
#define PHARMACY_MGMT_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define PHARM_MAX_DRUGS 512
#define PHARM_MAX_PRESCRIPTIONS 2048
#define PHARM_MAX_DISPENSES 4096
#define PHARM_MAX_NAME 128

typedef enum {
    PHARM_ROUTE_ORAL,
    PHARM_ROUTE_IV,
    PHARM_ROUTE_IM,
    PHARM_ROUTE_SC,
    PHARM_ROUTE_TOPICAL,
    PHARM_ROUTE_INHALED,
    PHARM_ROUTE_SUBLINGUAL
} PharmRoute;

typedef enum {
    PHARM_SCHEDULE_ONCE,
    PHARM_SCHEDULE_QD,
    PHARM_SCHEDULE_BID,
    PHARM_SCHEDULE_TID,
    PHARM_SCHEDULE_QID,
    PHARM_SCHEDULE_Q4H,
    PHARM_SCHEDULE_Q6H,
    PHARM_SCHEDULE_Q8H,
    PHARM_SCHEDULE_Q12H,
    PHARM_SCHEDULE_PRN
} PharmSchedule;

typedef enum {
    PHARM_STATUS_ACTIVE,
    PHARM_STATUS_COMPLETED,
    PHARM_STATUS_DISCONTINUED,
    PHARM_STATUS_ON_HOLD,
    PHARM_STATUS_EXPIRED
} PharmStatus;

typedef struct {
    int      id;
    char     generic_name[PHARM_MAX_NAME];
    char     brand_name[PHARM_MAX_NAME];
    char     ndc_code[16];
    uint64_t strength_mg;
    char     unit[16];
    PharmRoute default_route;
    double   half_life_hours;
    bool     requires_refrigeration;
    bool     controlled_substance;
    int      schedule_class;
    uint64_t stock_quantity;
    uint64_t reorder_threshold;
    uint64_t cost_per_unit_cents;
} DrugInventory;

typedef struct {
    int      id;
    int      patient_id;
    int      drug_id;
    int      prescriber_id;
    uint64_t dose_mg;
    PharmRoute route;
    PharmSchedule schedule;
    uint64_t duration_days;
    uint64_t quantity;
    int      refills;
    time_t   prescribed_at;
    time_t   start_date;
    time_t   end_date;
    PharmStatus status;
    char     sig_code[256];
    char     notes[256];
    bool     requires_prior_auth;
} Prescription;

typedef struct {
    int      id;
    int      prescription_id;
    int      drug_id;
    int      patient_id;
    uint64_t quantity_dispensed;
    int      dispensed_by_id;
    time_t   dispensed_at;
    char     lot_number[32];
    time_t   expiration_date;
    uint64_t copay_cents;
    bool     counseled;
} DispenseRecord;

typedef struct {
    char     name[PHARM_MAX_NAME];
    int      id;
    char     license_number[32];
    bool     active;
} Pharmacist;

typedef struct {
    DrugInventory drugs[PHARM_MAX_DRUGS];
    int           drug_count;
    Prescription  prescriptions[PHARM_MAX_PRESCRIPTIONS];
    int           rx_count;
    DispenseRecord dispenses[PHARM_MAX_DISPENSES];
    int           dispense_count;
    Pharmacist    pharmacists[32];
    int           pharmacist_count;
} PharmacySystem;

/* Core API */
PharmacySystem* pharm_system_create(void);
void            pharm_system_destroy(PharmacySystem *sys);
void pharm_system_init(PharmacySystem *sys);
int  pharm_add_drug(PharmacySystem *sys, const char *generic, const char *brand, uint64_t strength_mg, uint64_t stock, uint64_t reorder_point);
DrugInventory* pharm_find_drug(PharmacySystem *sys, int drug_id);
bool pharm_has_stock(const PharmacySystem *sys, int drug_id, uint64_t quantity_needed);
int  pharm_create_prescription(PharmacySystem *sys, int patient_id, int drug_id, int prescriber_id, uint64_t dose_mg, PharmRoute route, PharmSchedule schedule, uint64_t duration_days, int refills);
bool pharm_dispense(PharmacySystem *sys, int rx_id, int pharmacist_id, uint64_t quantity, const char *lot, time_t expiry);
bool pharm_discontinue_rx(PharmacySystem *sys, int rx_id);
bool pharm_verify_prescription(const PharmacySystem *sys, int rx_id);

/* L5: Drug interaction check */
bool pharm_check_duplicate_therapy(const PharmacySystem *sys, int patient_id, int new_drug_id);
int  pharm_get_active_rx_count(const PharmacySystem *sys, int patient_id);
int  pharm_get_patient_prescriptions(const PharmacySystem *sys, int patient_id, int *rx_ids, int max_count);

/* L6: Inventory management */
bool pharm_needs_reorder(const PharmacySystem *sys, int drug_id);
uint64_t pharm_reorder_quantity(const PharmacySystem *sys, int drug_id);
void pharm_process_reorder(PharmacySystem *sys, int drug_id, uint64_t quantity);
int  pharm_get_low_stock_drugs(const PharmacySystem *sys, int *drug_ids, int max_count);

/* L7: Medication reconciliation */
typedef struct {
    int      drug_id;
    uint64_t dose_mg;
    PharmRoute route;
    PharmSchedule schedule;
    bool     reconciled;
    char     discrepancy[256];
} MedRecEntry;

int  pharm_medication_reconciliation(const PharmacySystem *sys, int patient_id, const MedRecEntry *admission_meds, int count, MedRecEntry *reconciled);
bool pharm_check_allergies(const PharmacySystem *sys, int patient_id, int drug_id);

/* L8: Pharmacokinetics (one-compartment model) */
double pharm_half_life_to_elimination(double half_life_h);
double pharm_steady_state_time(double half_life_h);
double pharm_loading_dose(double target_conc, double vd, double bioavailability);
double pharm_maintenance_dose(double target_conc, double clearance, double interval_h, double bioavailability);
double pharm_peak_concentration(double dose, double vd, double bioavailability);
double pharm_trough_concentration(double peak, double kel, double interval_h);

/* L9: Pharmacogenomics (industry frontier) */
typedef enum { PHARM_METABOLIZER_POOR, PHARM_METABOLIZER_INTERMEDIATE, PHARM_METABOLIZER_EXTENSIVE, PHARM_METABOLIZER_ULTRA } PharmMetabolizer;
double pharm_genomic_dose_adjustment(double standard_dose, PharmMetabolizer phenotype);
const char* pharm_metabolizer_label(PharmMetabolizer p);

#endif
