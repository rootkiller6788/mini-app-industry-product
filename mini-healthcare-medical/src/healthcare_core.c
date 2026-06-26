#include "healthcare_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

PatientRegistry* reg_create(void) {
    PatientRegistry* reg = malloc(sizeof(PatientRegistry));
    if (!reg) return NULL;
    reg->patient_count = 0;
    reg->condition_count = 0;
    reg->vital_count = 0;
    memset(reg->patients, 0, sizeof(reg->patients));
    memset(reg->conditions, 0, sizeof(reg->conditions));
    memset(reg->vitals, 0, sizeof(reg->vitals));
    return reg;
}

Patient* reg_add_patient(PatientRegistry* reg, const char* name, int age, BloodType bt) {
    if (!reg || reg->patient_count >= MED_MAX_PATIENTS) return NULL;
    Patient* p = &reg->patients[reg->patient_count];
    p->id = reg->patient_count + 1;
    snprintf(p->name, MED_MAX_NAME, "%s", name);
    p->age = age;
    p->blood_type = bt;
    p->weight_kg = 0.0;
    p->height_cm = 0.0;
    p->registered_at = (int64_t)time(NULL);
    reg->patient_count++;
    return p;
}

Patient* reg_find_patient(PatientRegistry* reg, int id) {
    if (!reg || id <= 0 || id > reg->patient_count) return NULL;
    return &reg->patients[id - 1];
}

Condition* reg_add_condition(PatientRegistry* reg, int patient_id,
                              const char* icd_code, const char* desc, Severity sev) {
    if (!reg || reg->condition_count >= MED_MAX_RECORDS) return NULL;
    if (patient_id <= 0 || patient_id > reg->patient_count) return NULL;
    Condition* c = &reg->conditions[reg->condition_count];
    c->id = reg->condition_count + 1;
    c->patient_id = patient_id;
    snprintf(c->icd_code, MED_MAX_CODE_LEN, "%s", icd_code);
    snprintf(c->description, MED_MAX_NAME, "%s", desc);
    c->severity = sev;
    c->diagnosed_at = (int64_t)time(NULL);
    c->active = true;
    reg->condition_count++;
    return c;
}

VitalSigns* reg_record_vitals(PatientRegistry* reg, int patient_id,
                               double temp, int hr, int sbp, int dbp, int spo2) {
    if (!reg || reg->vital_count >= MED_MAX_RECORDS) return NULL;
    VitalSigns* vs = &reg->vitals[reg->vital_count];
    vs->id = reg->vital_count + 1;
    vs->patient_id = patient_id;
    vs->temperature_c = temp;
    vs->heart_rate_bpm = hr;
    vs->systolic_bp = sbp;
    vs->diastolic_bp = dbp;
    vs->spo2_pct = spo2;
    vs->measured_at = (int64_t)time(NULL);
    reg->vital_count++;
    return vs;
}

double reg_bmi(const Patient* p) {
    if (!p || p->height_cm <= 0.0) return 0.0;
    double height_m = p->height_cm / 100.0;
    return p->weight_kg / (height_m * height_m);
}

static int score_resp(int rr) {
    if (rr <= 8) return 3;
    if (rr <= 11) return 1;
    if (rr <= 20) return 0;
    if (rr <= 24) return 2;
    return 3;
}
static int score_spo2(int s) {
    if (s <= 91) return 3;
    if (s <= 93) return 2;
    if (s <= 95) return 1;
    return 0;
}
static int score_temp(double t) {
    if (t <= 35.0) return 3;
    if (t <= 36.0) return 1;
    if (t <= 38.0) return 0;
    if (t <= 39.0) return 1;
    return 2;
}
static int score_sbp(int s) {
    if (s <= 90) return 3;
    if (s <= 100) return 2;
    if (s <= 110) return 1;
    if (s <= 219) return 0;
    return 3;
}
static int score_hr(int h) {
    if (h <= 40) return 3;
    if (h <= 50) return 1;
    if (h <= 90) return 0;
    if (h <= 110) return 1;
    if (h <= 130) return 2;
    return 3;
}

int news2_score(const VitalSigns* vs) {
    if (!vs) return 0;
    return score_resp(15) + score_spo2(vs->spo2_pct) +
           score_temp(vs->temperature_c) + score_sbp(vs->systolic_bp) +
           score_hr(vs->heart_rate_bpm);
}

bool news2_is_urgent(const VitalSigns* vs) {
    return news2_score(vs) >= 5;
}

const char* news2_risk_label(int score) {
    if (score >= 7) return "HIGH (urgent review)";
    if (score >= 5) return "MEDIUM (urgent within 1hr)";
    if (score >= 3) return "LOW-MEDIUM (escalate)";
    return "LOW (routine)";
}

/*
 * ===================================================================
 * L5: Glasgow Coma Scale (GCS) — neurological assessment
 *
 * Eye: 1-4, Verbal: 1-5, Motor: 1-6. Total 3-15.
 * Severe: GCS <= 8 (intubate), Moderate: 9-12, Mild: 13-15.
 *
 * Reference: Teasdale G, Jennett B (1974), The Lancet
 * ===================================================================
 */
int gcs_score(int eye_opening, int verbal_response, int motor_response) {
    if (eye_opening < 1 || eye_opening > 4) return 0;
    if (verbal_response < 1 || verbal_response > 5) return 0;
    if (motor_response < 1 || motor_response > 6) return 0;
    return eye_opening + verbal_response + motor_response;
}

const char* gcs_severity(int score) {
    if (score <= 8) return "SEVERE — Consider intubation, ICU admission";
    if (score <= 12) return "MODERATE — Close observation, frequent reassessment";
    return "MILD — Routine neurological checks";
}

bool gcs_needs_intubation(int score) { return score <= 8; }

/*
 * ===================================================================
 * L5: PERC Rule — Pulmonary Embolism Rule-out Criteria
 *
 * If all 8 criteria are met AND pre-test probability <15%,
 * PE can be ruled out without D-dimer or imaging.
 *
 * Reference: Kline JA et al. (2004), J Thromb Haemost
 * ===================================================================
 */
bool perc_rule_negative(const PERC_Input *p) {
    if (!p) return false;
    if (p->age_years >= 50) return false;
    if (p->heart_rate_ge_100) return false;
    if (p->hypoxia_spo2_lt_95) return false;
    if (p->prior_dvt_pe) return false;
    if (p->surgery_recent) return false;
    if (p->hemoptysis) return false;
    if (p->estrogen_use) return false;
    if (p->unilateral_leg_swelling) return false;
    return true;
}

const char* perc_recommendation(const PERC_Input *p) {
    if (perc_rule_negative(p))
        return "PERC NEGATIVE: PE ruled out. No further testing needed.";
    return "PERC POSITIVE: Cannot rule out PE. Consider D-dimer or CTPA.";
}

/*
 * ===================================================================
 * L5: Wells Criteria — DVT probability
 *
 * Score >= 2: DVT likely (needs ultrasound)
 * Score < 2: DVT unlikely (check D-dimer)
 *
 * Reference: Wells PS et al. (2003), NEJM
 * ===================================================================
 */
double wells_dvt_score(const WellsDVT_Input *w) {
    if (!w) return 0.0;
    double score = 0.0;
    if (w->active_cancer) score += 1.0;
    if (w->paralysis_or_cast) score += 1.0;
    if (w->bedridden_gt_3days) score += 1.0;
    if (w->localized_tenderness) score += 1.0;
    if (w->entire_leg_swollen) score += 1.0;
    if (w->calf_swelling_gt_3cm) score += 1.0;
    if (w->pitting_edema) score += 1.0;
    if (w->collateral_veins) score += 1.0;
    if (w->alternative_diagnosis_more_likely) score -= 2.0;
    return score;
}

const char* wells_dvt_recommendation(double score) {
    if (score >= 2.0)
        return "DVT LIKELY: Proceed to compression ultrasound.";
    if (score >= 1.0)
        return "DVT UNCERTAIN: Check D-dimer. If elevated, ultrasound.";
    return "DVT UNLIKELY: Consider alternative diagnosis.";
}

/*
 * ===================================================================
 * L5: CURB-65 — Pneumonia severity score
 *
 * Confusion, Urea>7mmol/L, RR>=30, BP<90/60, Age>=65
 * Score 0-1: home treatment; 2: hospital; >=3: severe/ICU
 *
 * Reference: Lim WS et al. (2003), Thorax
 * ===================================================================
 */
int curb65_score(const CURB65_Input *c) {
    if (!c) return 0;
    int score = 0;
    if (c->confusion) score++;
    if (c->urea_mmol_l > 7.0) score++;
    if (c->resp_rate >= 30) score++;
    if (c->systolic_bp < 90 || c->diastolic_bp <= 60) score++;
    if (c->age >= 65) score++;
    return score;
}

const char* curb65_management(int score) {
    if (score >= 4) return "CURB-65 SEVERE (mortality ~27%): ICU admission, IV antibiotics";
    if (score == 3) return "CURB-65 SEVERE (mortality ~17%): Hospital admission, consider ICU";
    if (score == 2) return "CURB-65 MODERATE (mortality ~7%): Hospital admission";
    if (score == 1) return "CURB-65 MILD (mortality ~3%): Consider outpatient with follow-up";
    return "CURB-65 LOW (mortality <1%): Outpatient management appropriate";
}

double curb65_mortality_pct(int score) {
    const double mortality[] = {0.7, 3.2, 7.0, 17.0, 27.0};
    if (score < 0) return 0.0;
    if (score > 4) return 27.0;
    return mortality[score];
}

/*
 * ===================================================================
 * L5: QTc interval correction (Bazett's formula)
 *
 * QTc = QT / sqrt(RR) where RR = 60/HR
 * Normal: <440ms (male), <460ms (female)
 * Prolonged: >500ms = high risk of Torsades de Pointes
 *
 * Reference: Bazett HC (1920), Heart
 * ===================================================================
 */
double qtc_bazett(double qt_ms, double heart_rate_bpm) {
    if (heart_rate_bpm <= 0) return 0.0;
    double rr_sec = 60.0 / heart_rate_bpm;
    return qt_ms / sqrt(rr_sec);
}

double qtc_fridericia(double qt_ms, double heart_rate_bpm) {
    if (heart_rate_bpm <= 0) return 0.0;
    double rr_sec = 60.0 / heart_rate_bpm;
    return qt_ms / cbrt(rr_sec);
}

const char* qtc_risk_assessment(double qtc_ms, bool female) {
    double threshold = female ? 460.0 : 440.0;
    if (qtc_ms > 500.0)
        return "QTc >500ms: HIGH RISK — Immediate cardiology consult, review medications";
    if (qtc_ms > threshold)
        return "QTc PROLONGED — Review QT-prolonging drugs, check electrolytes";
    return "QTc NORMAL — No specific action needed";
}

/*
 * ===================================================================
 * L6: BMI classification (WHO standard)
 * ===================================================================
 */
const char* bmi_classification(double bmi) {
    if (bmi < 16.0) return "Severe thinness";
    if (bmi < 17.0) return "Moderate thinness";
    if (bmi < 18.5) return "Mild thinness";
    if (bmi < 25.0) return "Normal weight";
    if (bmi < 30.0) return "Overweight (Pre-obese)";
    if (bmi < 35.0) return "Obese Class I";
    if (bmi < 40.0) return "Obese Class II";
    return "Obese Class III (Severe)";
}

double bmi_calculate(double weight_kg, double height_cm) {
    if (height_cm <= 0.0) return 0.0;
    double height_m = height_cm / 100.0;
    return weight_kg / (height_m * height_m);
}

/*
 * ===================================================================
 * L7: Pediatric early warning score (PEWS)
 *
 * Behavior, Cardiovascular, Respiratory — each 0-3, total 0-9
 * >=4: Urgent pediatric review
 *
 * Reference: Monaghan A (2005), Paediatric Nursing
 * ===================================================================
 */
int pews_score(const PEWS_Input *p) {
    if (!p) return 0;
    return p->behavior_score + p->cardiovascular_score + p->respiratory_score;
}

const char* pews_action(int score) {
    if (score >= 8) return "PEWS CRITICAL: Immediate PICU/rapid response team";
    if (score >= 5) return "PEWS HIGH: Urgent pediatric review within 30 minutes";
    if (score >= 4) return "PEWS ELEVATED: Senior nurse review, increase monitoring frequency";
    if (score >= 2) return "PEWS BORDERLINE: Document, continue routine monitoring";
    return "PEWS NORMAL: Routine care";
}

/*
 * ===================================================================
 * L8: Anion gap — metabolic acidosis differential
 *
 * AG = Na - (Cl + HCO3)
 * Normal: 8-12 mEq/L (without K), 12-16 mEq/L (with K)
 * High AG: MUDPILES (Methanol, Uremia, DKA, etc.)
 * Normal AG: HARDASS (Hyperalimentation, Acetazolamide, etc.)
 * ===================================================================
 */
double anion_gap(double sodium, double chloride, double bicarbonate) {
    return sodium - (chloride + bicarbonate);
}

const char* anion_gap_interpretation(double ag) {
    if (ag > 16.0) return "HIGH ANION GAP METABOLIC ACIDOSIS — Consider MUDPILES: Methanol, Uremia, DKA, Paraldehyde, Iron/INH, Lactic acidosis, Ethylene glycol, Salicylates";
    if (ag > 12.0) return "BORDERLINE HIGH — Recheck, consider early MUDPILES workup";
    if (ag < 8.0) return "LOW ANION GAP — Consider hypoalbuminemia, lithium, bromide, paraproteinemia";
    return "NORMAL ANION GAP — If acidosis present, consider HARDASS causes (GI/HCO3 loss)";
}

double anion_gap_with_k(double sodium, double potassium, double chloride, double bicarbonate) {
    return (sodium + potassium) - (chloride + bicarbonate);
}

/*
 * ===================================================================
 * L8: Wells Score for PE (Pulmonary Embolism)
 *
 * Clinical prediction rule: >6 high, 2-6 moderate, <2 low probability
 *
 * Reference: Wells PS et al. (2000), Thromb Haemost
 * ===================================================================
 */
double wells_pe_score(const WellsPE_Input *w) {
    if (!w) return 0.0;
    double score = 0.0;
    if (w->clinical_signs_dvt) score += 3.0;
    if (w->pe_most_likely_diagnosis) score += 3.0;
    if (w->heart_rate > 100) score += 1.5;
    if (w->immobilization_surgery) score += 1.5;
    if (w->prior_dvt_pe) score += 1.5;
    if (w->hemoptysis) score += 1.0;
    if (w->active_cancer) score += 1.0;
    return score;
}

const char* wells_pe_management(double score) {
    if (score > 6.0)
        return "PE LIKELY: Proceed directly to CTPA. Start anticoagulation if delay expected.";
    if (score >= 2.0)
        return "PE MODERATE: Check D-dimer. If positive, CTPA.";
    return "PE UNLIKELY: Consider PERC rule. If PERC positive, check D-dimer.";
}

/*
 * ===================================================================
 * L9: SIRS Criteria — Systemic Inflammatory Response Syndrome
 *
 * >=2 criteria = SIRS (precursor to sepsis screening)
 *
 * Reference: Bone RC et al. (1992), Chest
 * ===================================================================
 */
int sirs_criteria_count(const SIRS_Input *s) {
    if (!s) return 0;
    int count = 0;
    if (s->temperature_c > 38.0 || s->temperature_c < 36.0) count++;
    if (s->heart_rate > 90) count++;
    if (s->resp_rate > 20) count++;
    if (s->wbc_count_x10e9_l > 12.0 || s->wbc_count_x10e9_l < 4.0 || s->bands_gt_10pct) count++;
    return count;
}

const char* sirs_screening_recommendation(const SIRS_Input *s) {
    if (!s) return "Invalid input";
    int criteria = sirs_criteria_count(s);
    if (criteria >= 2)
        return "SIRS POSITIVE: Screen for sepsis (qSOFA, lactate, blood cultures, IV fluids, antibiotics within 1 hour)";
    if (criteria == 1)
        return "SIRS BORDERLINE: Monitor closely, reassess in 2-4 hours";
    return "SIRS NEGATIVE: Routine monitoring";
}

/*
 * ===================================================================
 * L9: qSOFA — Quick Sepsis-Related Organ Failure Assessment
 *
 * RR>=22, SBP<=100, GCS<15. >=2 criteria = high risk.
 *
 * Reference: Seymour CW et al. (2016), JAMA
 * ===================================================================
 */
int qsofa_score(const qSOFA_Input *q) {
    if (!q) return 0;
    int score = 0;
    if (q->resp_rate >= 22) score++;
    if (q->systolic_bp <= 100) score++;
    if (q->gcs < 15) score++;
    return score;
}

const char* qsofa_sepsis_risk(int score) {
    if (score >= 2)
        return "qSOFA POSITIVE: HIGH RISK of poor outcome. Activate sepsis protocol: lactate, blood cultures x2, IV crystalloids 30mL/kg, broad-spectrum antibiotics within 1 hour, measure urine output";
    if (score == 1)
        return "qSOFA BORDERLINE: Monitor for deterioration, reassess frequently";
    return "qSOFA NEGATIVE: Low risk. Continue routine care";
}

/*
 * ===================================================================
 * SOFA Score — Sequential Organ Failure Assessment (simplified)
 *
 * Assesses 6 organ systems: Respiration, Coagulation, Liver,
 * Cardiovascular, CNS, Renal. Each 0-4, total 0-24.
 * Delta SOFA >=2 = sepsis (Sepsis-3 definition).
 *
 * Reference: Vincent JL et al. (1996), ICM
 * ===================================================================
 */
int sofa_respiration_score(double pf_ratio, bool ventilated) {
    if (pf_ratio >= 400 && !ventilated) return 0;
    if (pf_ratio < 400 && !ventilated) return 1;
    if (pf_ratio < 300 && !ventilated) return 2;
    if (pf_ratio < 200 && ventilated) return 3;
    return 4;
}

int sofa_coagulation_score(double platelets) {
    if (platelets >= 150) return 0;
    if (platelets >= 100) return 1;
    if (platelets >= 50) return 2;
    if (platelets >= 20) return 3;
    return 4;
}

int sofa_liver_score(double bilirubin) {
    if (bilirubin < 20) return 0;
    if (bilirubin < 33) return 1;
    if (bilirubin < 102) return 2;
    if (bilirubin < 204) return 3;
    return 4;
}

int sofa_cardiovascular_score(double map, bool vasopressors) {
    if (map >= 70 && !vasopressors) return 0;
    if (map < 70 && !vasopressors) return 1;
    if (vasopressors) {
        /* Simplified: any vasopressor = score 3 */
        return 3;
    }
    return 4;
}

int sofa_cns_score(int gcs) {
    if (gcs >= 15) return 0;
    if (gcs >= 13) return 1;
    if (gcs >= 10) return 2;
    if (gcs >= 6) return 3;
    return 4;
}

int sofa_renal_score(double creatinine, double urine_output) {
    if (creatinine < 110) return 0;
    if (creatinine < 170) return 1;
    if (creatinine < 300) return 2;
    if (creatinine < 440 || urine_output < 500) return 3;
    return 4;
}

int sofa_total_score(const SOFA_Input *s) {
    if (!s) return 0;
    int resp = sofa_respiration_score(s->pao2_fio2_ratio, false);
    int coag = sofa_coagulation_score(s->platelets_x10e9_l);
    int liver = sofa_liver_score(s->bilirubin_umol_l);
    int cv = sofa_cardiovascular_score(s->mean_arterial_pressure, s->on_vasopressors);
    int cns = sofa_cns_score(s->gcs);
    int renal = sofa_renal_score(s->creatinine_umol_l, s->urine_output_ml_day);
    return resp + coag + liver + cv + cns + renal;
}

const char* sofa_mortality_estimate(int score) {
    if (score >= 16) return "SOFA >=16: Mortality ~90%";
    if (score >= 13) return "SOFA 13-15: Mortality ~70-80%";
    if (score >= 10) return "SOFA 10-12: Mortality ~40-50%";
    if (score >= 7)  return "SOFA 7-9: Mortality ~15-20%";
    if (score >= 4)  return "SOFA 4-6: Mortality ~5-10%";
    return "SOFA <4: Mortality <5%";
}

void reg_destroy(PatientRegistry* reg) { free(reg); }
