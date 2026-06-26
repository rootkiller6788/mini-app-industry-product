#ifndef HEALTHCARE_CORE_H
#define HEALTHCARE_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MED_MAX_PATIENTS    2048
#define MED_MAX_RECORDS     8192
#define MED_MAX_NAME        128
#define MED_MAX_CODE_LEN    16

typedef enum { BLOOD_TYPE_A, BLOOD_TYPE_B, BLOOD_TYPE_AB, BLOOD_TYPE_O } BloodType;
typedef enum { SEVERITY_MILD, SEVERITY_MODERATE, SEVERITY_SEVERE, SEVERITY_CRITICAL } Severity;

typedef struct {
    int       id;
    char      name[MED_MAX_NAME];
    int       age;
    BloodType blood_type;
    double    weight_kg;
    double    height_cm;
    int64_t   registered_at;
} Patient;

typedef struct {
    int       id;
    int       patient_id;
    char      icd_code[MED_MAX_CODE_LEN];
    char      description[MED_MAX_NAME];
    Severity  severity;
    int64_t   diagnosed_at;
    bool      active;
} Condition;

typedef struct {
    int       id;
    int       patient_id;
    double    temperature_c;
    int       heart_rate_bpm;
    int       systolic_bp;
    int       diastolic_bp;
    int       spo2_pct;
    int64_t   measured_at;
} VitalSigns;

int news2_score(const VitalSigns* vs);
bool news2_is_urgent(const VitalSigns* vs);
const char* news2_risk_label(int score);

typedef struct {
    Patient    patients[MED_MAX_PATIENTS];
    int        patient_count;
    Condition  conditions[MED_MAX_RECORDS];
    int        condition_count;
    VitalSigns vitals[MED_MAX_RECORDS];
    int        vital_count;
} PatientRegistry;

PatientRegistry* reg_create(void);
Patient*   reg_add_patient(PatientRegistry* reg, const char* name, int age, BloodType bt);
Patient*   reg_find_patient(PatientRegistry* reg, int id);
Condition* reg_add_condition(PatientRegistry* reg, int patient_id,
                             const char* icd_code, const char* desc, Severity sev);
VitalSigns* reg_record_vitals(PatientRegistry* reg, int patient_id,
                              double temp, int hr, int sbp, int dbp, int spo2);
double     reg_bmi(const Patient* p);
void       reg_destroy(PatientRegistry* reg);

/* L5: Glasgow Coma Scale */
int        gcs_score(int eye_opening, int verbal_response, int motor_response);
const char* gcs_severity(int score);
bool       gcs_needs_intubation(int score);

/* L5: PERC Rule — PE Rule-out Criteria */
typedef struct { int age_years; bool prior_dvt_pe; bool surgery_recent; bool hemoptysis; bool estrogen_use; bool hypoxia_spo2_lt_95; bool unilateral_leg_swelling; bool heart_rate_ge_100; } PERC_Input;
bool       perc_rule_negative(const PERC_Input *p);
const char* perc_recommendation(const PERC_Input *p);

/* L5: Wells DVT Criteria */
typedef struct { bool active_cancer; bool paralysis_or_cast; bool bedridden_gt_3days; bool localized_tenderness; bool entire_leg_swollen; bool calf_swelling_gt_3cm; bool pitting_edema; bool collateral_veins; bool alternative_diagnosis_more_likely; } WellsDVT_Input;
double     wells_dvt_score(const WellsDVT_Input *w);
const char* wells_dvt_recommendation(double score);

/* L5: CURB-65 Pneumonia Severity */
typedef struct { bool confusion; double urea_mmol_l; int resp_rate; int systolic_bp; int diastolic_bp; int age; } CURB65_Input;
int        curb65_score(const CURB65_Input *c);
const char* curb65_management(int score);
double     curb65_mortality_pct(int score);

/* L5: QTc interval correction */
double     qtc_bazett(double qt_ms, double heart_rate_bpm);
double     qtc_fridericia(double qt_ms, double heart_rate_bpm);
const char* qtc_risk_assessment(double qtc_ms, bool female);

/* L6: BMI classification */
const char* bmi_classification(double bmi);
double     bmi_calculate(double weight_kg, double height_cm);

/* L7: Pediatric Early Warning Score (PEWS) */
typedef struct { int behavior_score; int cardiovascular_score; int respiratory_score; } PEWS_Input;
int        pews_score(const PEWS_Input *p);
const char* pews_action(int score);

/* L8: Anion Gap */
double     anion_gap(double sodium, double chloride, double bicarbonate);
const char* anion_gap_interpretation(double ag);
double     anion_gap_with_k(double sodium, double potassium, double chloride, double bicarbonate);

/* L8: Wells PE Criteria */
typedef struct { bool clinical_signs_dvt; bool pe_most_likely_diagnosis; int heart_rate; bool immobilization_surgery; bool prior_dvt_pe; bool hemoptysis; bool active_cancer; } WellsPE_Input;
double     wells_pe_score(const WellsPE_Input *w);
const char* wells_pe_management(double score);

/* L9: SIRS Criteria */
typedef struct { double temperature_c; int heart_rate; int resp_rate; double wbc_count_x10e9_l; bool bands_gt_10pct; } SIRS_Input;
int        sirs_criteria_count(const SIRS_Input *s);
const char* sirs_screening_recommendation(const SIRS_Input *s);

/* L9: qSOFA */
typedef struct { int resp_rate; int systolic_bp; int gcs; } qSOFA_Input;
int        qsofa_score(const qSOFA_Input *q);
const char* qsofa_sepsis_risk(int score);

/* L9: SOFA Score */
typedef struct { double pao2_fio2_ratio; double platelets_x10e9_l; double bilirubin_umol_l; double mean_arterial_pressure; bool on_vasopressors; int gcs; double creatinine_umol_l; double urine_output_ml_day; } SOFA_Input;
int        sofa_total_score(const SOFA_Input *s);
const char* sofa_mortality_estimate(int score);

#endif
