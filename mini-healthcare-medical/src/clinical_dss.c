#include "clinical_dss.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Utility helpers — dss_min_i used for finding worst severity, dss_max_i for comparison */
static int dss_min_i(int a, int b) { return a < b ? a : b; }
static int dss_max_i(int a, int b) { return a > b ? a : b; }

/* dss_max_int — used internally to get the more severe of two interaction severities */
static int dss_compare_severity_int(DSSInteractionSeverity a, DSSInteractionSeverity b) {
    return dss_max_i((int)a, (int)b);
}

/* dss_severity_meets_threshold — used by interaction filtering:
 * returns true if actual severity is at or above the threshold for clinical action */
static bool dss_severity_meets_threshold(DSSInteractionSeverity actual, DSSInteractionSeverity threshold) {
    return dss_min_i((int)actual, (int)threshold) == (int)threshold;
}

/* dss_filter_interactions_by_threshold — filter interaction list to those meeting severity criteria */
int dss_filter_interactions_by_threshold(const DSSDrugDatabase *db, DSSInteractionSeverity threshold,
                                          DSSDrugInteraction *results, int max_results) {
    if (!db || !results) return 0;
    int count = 0;
    for (int i = 0; i < db->interaction_count && count < max_results; i++) {
        if (dss_severity_meets_threshold(db->interactions[i].severity, threshold)) {
            results[count++] = db->interactions[i];
        }
    }
    return count;
}

void dss_db_init(DSSDrugDatabase *db) {
    memset(db, 0, sizeof(DSSDrugDatabase));
}

int dss_db_add_drug(DSSDrugDatabase *db, const char *generic, const char *brand,
                     const char *atc, double half_life, uint64_t max_daily) {
    if (!db || !generic || db->drug_count >= DSS_MAX_DRUGS) return -1;
    DSSDrug *d = &db->drugs[db->drug_count];
    d->id = db->drug_count + 1;
    snprintf(d->generic_name, sizeof(d->generic_name), "%s", generic);
    snprintf(d->brand_name, sizeof(d->brand_name), "%s", brand ? brand : "");
    snprintf(d->atc_code, sizeof(d->atc_code), "%s", atc ? atc : "");
    d->half_life_h = half_life;
    d->max_daily_dose_mg = max_daily;
    d->controlled_substance = false;
    d->therapeutic_index = 1.0;
    db->drug_count++;
    return d->id;
}

DSSDrug* dss_db_find_drug(DSSDrugDatabase *db, const char *generic_name) {
    if (!db || !generic_name) return NULL;
    for (int i = 0; i < db->drug_count; i++) {
        if (strstr(db->drugs[i].generic_name, generic_name))
            return &db->drugs[i];
    }
    return NULL;
}

void dss_db_add_interaction(DSSDrugDatabase *db, const char *drug_a,
                             const char *drug_b, DSSInteractionSeverity sev,
                             const char *mechanism, const char *rec) {
    if (!db || !drug_a || !drug_b || db->interaction_count >= DSS_MAX_INTERACTIONS) return;
    DSSDrugInteraction *ix = &db->interactions[db->interaction_count];
    snprintf(ix->drug_a, sizeof(ix->drug_a), "%s", drug_a);
    snprintf(ix->drug_b, sizeof(ix->drug_b), "%s", drug_b);
    ix->severity = sev;
    snprintf(ix->mechanism, sizeof(ix->mechanism), "%s", mechanism ? mechanism : "");
    snprintf(ix->recommendation, sizeof(ix->recommendation), "%s", rec ? rec : "");
    db->interaction_count++;
}

DSSInteractionSeverity dss_check_interaction(const DSSDrugDatabase *db,
                                              const char *drug_a, const char *drug_b) {
    if (!db || !drug_a || !drug_b) return DSS_INTERACTION_NONE;
    for (int i = 0; i < db->interaction_count; i++) {
        if ((strstr(db->interactions[i].drug_a, drug_a) &&
             strstr(db->interactions[i].drug_b, drug_b)) ||
            (strstr(db->interactions[i].drug_a, drug_b) &&
             strstr(db->interactions[i].drug_b, drug_a))) {
            return db->interactions[i].severity;
        }
    }
    return DSS_INTERACTION_NONE;
}

int dss_find_all_interactions(const DSSDrugDatabase *db, const char *drugs[],
                               int count, DSSDrugInteraction *results, int max_results) {
    if (!db || !drugs || count < 2 || !results) return 0;
    int found = 0;
    for (int i = 0; i < count && found < max_results; i++) {
        for (int j = i + 1; j < count && found < max_results; j++) {
            DSSInteractionSeverity sev = dss_check_interaction(db, drugs[i], drugs[j]);
            if (sev != DSS_INTERACTION_NONE) {
                results[found].severity = sev;
                snprintf(results[found].drug_a, 64, "%s", drugs[i]);
                snprintf(results[found].drug_b, 64, "%s", drugs[j]);
                found++;
            }
        }
    }
    return found;
}

CHA2DS2_VASc_Result dss_cha2ds2_vasc(const CHA2DS2_VASc_Input *in) {
    CHA2DS2_VASc_Result result;
    memset(&result, 0, sizeof(result));
    if (!in) return result;
    int score = 0;
    if (in->has_chf) score += 1;
    if (in->has_hypertension) score += 1;
    if (in->age >= 75) score += 2;
    else if (in->age >= 65) score += 1;
    if (in->has_diabetes) score += 1;
    if (in->has_stroke_tia) score += 2;
    if (in->has_vascular_disease) score += 1;
    if (in->female) score += 1;
    result.score = score;
    const double risk_table[10] = {0.0, 1.3, 2.2, 3.2, 4.0, 6.7, 9.8, 9.6, 6.7, 15.2};
    result.stroke_risk_pct = (score <= 9) ? risk_table[score] : 15.2;
    if (score >= 2) {
        result.needs_anticoagulation = true;
        snprintf(result.recommendation, sizeof(result.recommendation),
                 "CHA2DS2-VASc=%d: Oral anticoagulation recommended. Annual stroke risk %.1f%%.",
                 score, result.stroke_risk_pct);
    } else if (score == 1) {
        result.needs_anticoagulation = true;
        snprintf(result.recommendation, sizeof(result.recommendation),
                 "CHA2DS2-VASc=%d: Consider anticoagulation against bleeding risk.", score);
    } else {
        result.needs_anticoagulation = false;
        snprintf(result.recommendation, sizeof(result.recommendation),
                 "CHA2DS2-VASc=%d: Anticoagulation not indicated (low risk).", score);
    }
    return result;
}

HAS_BLED_Result dss_has_bled(const HAS_BLED_Input *in) {
    HAS_BLED_Result result;
    memset(&result, 0, sizeof(result));
    if (!in) return result;
    int score = 0;
    if (in->has_hypertension) score += 1;
    if (in->has_renal_disease) score += 1;
    if (in->has_liver_disease) score += 1;
    if (in->has_bleeding_history) score += 1;
    if (in->labile_inr) score += 1;
    if (in->elderly) score += 1;
    if (in->alcohol_use) score += 1;
    if (in->nsaid_use) score += 1;
    result.score = score;
    const double bleed_table[6] = {1.13, 1.02, 1.88, 3.74, 8.70, 12.50};
    result.major_bleeding_risk_pct = (score <= 5) ? bleed_table[score] : 12.5;
    if (score >= 3) {
        snprintf(result.risk_category, sizeof(result.risk_category), "HIGH");
        snprintf(result.recommendation, sizeof(result.recommendation),
                 "HAS-BLED=%d: HIGH bleeding risk (%.1f%%/yr).", score, result.major_bleeding_risk_pct);
    } else if (score >= 1) {
        snprintf(result.risk_category, sizeof(result.risk_category), "MODERATE");
        snprintf(result.recommendation, sizeof(result.recommendation),
                 "HAS-BLED=%d: MODERATE risk. Modify reversible factors.", score);
    } else {
        snprintf(result.risk_category, sizeof(result.risk_category), "LOW");
        snprintf(result.recommendation, sizeof(result.recommendation),
                 "HAS-BLED=%d: LOW bleeding risk.", score);
    }
    return result;
}

double dss_bsa_mosteller(double weight_kg, double height_cm) {
    if (weight_kg <= 0 || height_cm <= 0) return 0.0;
    return sqrt((height_cm * weight_kg) / 3600.0);
}

double dss_body_surface_area(double weight_kg, double height_cm) {
    return dss_bsa_mosteller(weight_kg, height_cm);
}

double dss_crcl_cockcroft_gault(const EGFR_Input *in) {
    if (!in || in->creatinine_mg_dl <= 0) return 0.0;
    double crcl = ((140.0 - in->age) * 70.0) / (72.0 * in->creatinine_mg_dl);
    if (in->female) crcl *= 0.85;
    return crcl;
}

double dss_egfr_ckd_epi(const EGFR_Input *in) {
    if (!in || in->creatinine_mg_dl <= 0) return 0.0;
    double scr = in->creatinine_mg_dl;
    double kappa = in->female ? 0.7 : 0.9;
    double alpha = in->female ? -0.241 : -0.302;
    double gender_factor = in->female ? 1.012 : 1.0;
    double min_ratio = (scr / kappa) < 1.0 ? (scr / kappa) : 1.0;
    double max_ratio = (scr / kappa) > 1.0 ? (scr / kappa) : 1.0;
    double egfr = 142.0 * pow(min_ratio, alpha) * pow(max_ratio, -1.200)
                 * pow(0.9938, in->age) * gender_factor;
    return egfr;
}

double dss_renal_dose_adjustment(double crcl, double normal_dose) {
    if (crcl <= 0) return normal_dose;
    if (crcl >= 90) return normal_dose;
    if (crcl >= 60) return normal_dose * 0.85;
    if (crcl >= 30) return normal_dose * 0.65;
    if (crcl >= 15) return normal_dose * 0.50;
    return normal_dose * 0.25;
}

const char* dss_renal_impairment_stage(double egfr) {
    if (egfr >= 90) return "G1: Normal or high";
    if (egfr >= 60) return "G2: Mildly decreased";
    if (egfr >= 45) return "G3a: Mildly to moderately decreased";
    if (egfr >= 30) return "G3b: Moderately to severely decreased";
    if (egfr >= 15) return "G4: Severely decreased";
    return "G5: Kidney failure";
}

DosageResult dss_calculate_dosage(const DosageInput *in, double standard_dose_mg) {
    DosageResult result;
    memset(&result, 0, sizeof(result));
    if (!in) return result;
    result.bsa_m2 = dss_bsa_mosteller(in->weight_kg, in->height_cm);
    EGFR_Input egi = {in->age, in->creatinine_mg_dl, in->female, false};
    result.crcl_ml_min = dss_crcl_cockcroft_gault(&egi);
    double bsa_adjusted = standard_dose_mg * result.bsa_m2 / 1.73;
    result.adjusted_dose_mg = dss_renal_dose_adjustment(result.crcl_ml_min, bsa_adjusted);
    snprintf(result.dosing_regimen, sizeof(result.dosing_regimen),
             "BSA dose: %.0f mg (BSA=%.2f m2, CrCl=%.0f mL/min)",
             result.adjusted_dose_mg, result.bsa_m2, result.crcl_ml_min);
    return result;
}

void dss_rule_engine_init(DSSRuleEngine *engine) {
    memset(engine, 0, sizeof(DSSRuleEngine));
}

int dss_rule_add(DSSRuleEngine *engine, const char *name, int priority) {
    if (!engine || engine->rule_count >= DSS_MAX_SCORES) return -1;
    int idx = engine->rule_count;
    snprintf(engine->rules[idx].rule_name, sizeof(engine->rules[idx].rule_name), "%s", name);
    engine->rules[idx].priority = priority;
    engine->rules[idx].conditions_count = 0;
    engine->rule_count++;
    return idx;
}

bool dss_rule_add_condition(DSSRuleEngine *engine, int rule_idx,
                             bool (*cond)(const void*), const char *action) {
    if (!engine || rule_idx < 0 || rule_idx >= engine->rule_count) return false;
    DSSRule *rule = &engine->rules[rule_idx];
    if (rule->conditions_count >= 8) return false;
    rule->conditions[rule->conditions_count] = cond;
    snprintf(rule->actions[rule->conditions_count], 256, "%s", action ? action : "");
    rule->conditions_count++;
    return true;
}

int dss_rule_engine_evaluate(DSSRuleEngine *engine, const void *context) {
    if (!engine || !context) return 0;
    engine->triggers_fired = 0;
    engine->alert_count = 0;
    for (int i = 0; i < engine->rule_count; i++) {
        DSSRule *rule = &engine->rules[i];
        bool all_true = true;
        for (int j = 0; j < rule->conditions_count; j++) {
            if (rule->conditions[j] && !rule->conditions[j](context)) {
                all_true = false;
                break;
            }
        }
        if (all_true && rule->conditions_count > 0) {
            engine->triggers_fired++;
            if (engine->alert_count < 4) {
                snprintf(engine->alert_log[engine->alert_count], 512,
                         "Rule triggered: %s", rule->rule_name);
                engine->alert_count++;
            }
        }
    }
    return engine->triggers_fired;
}

const char* dss_rule_engine_get_alert(const DSSRuleEngine *engine, int idx) {
    if (!engine || idx < 0 || idx >= engine->alert_count) return NULL;
    return engine->alert_log[idx];
}

double dss_bayesian_posttest(double pretest, double sensitivity, double specificity) {
    if (sensitivity < 0 || sensitivity > 1 || specificity < 0 || specificity > 1) return pretest;
    double numerator = sensitivity * pretest;
    double denominator = sensitivity * pretest + (1.0 - specificity) * (1.0 - pretest);
    if (denominator <= 0) return pretest;
    return numerator / denominator;
}

double dss_likelihood_ratio_positive(double sensitivity, double specificity) {
    if (specificity >= 1.0) return 999.0;
    return sensitivity / (1.0 - specificity);
}

double dss_likelihood_ratio_negative(double sensitivity, double specificity) {
    if (specificity <= 0.0) return 999.0;
    return (1.0 - sensitivity) / specificity;
}

double dss_fagans_nomogram(double pretest, double lr) {
    if (pretest <= 0 || pretest >= 1.0) return pretest;
    if (lr <= 0) return pretest;
    double pre_odds = pretest / (1.0 - pretest);
    double post_odds = pre_odds * lr;
    return post_odds / (1.0 + post_odds);
}

AI_TriagePriority dss_ai_triage_classify(const AI_Triage_Input *in) {
    if (!in) return AI_TRIAGE_P4;
    int danger_flags = 0;
    if (in->resp_rate <= 8 || in->resp_rate >= 30) danger_flags++;
    if (in->heart_rate <= 40 || in->heart_rate >= 140) danger_flags++;
    if (in->systolic_bp <= 80) danger_flags++;
    if (in->spo2 <= 88) danger_flags++;
    if (in->temperature >= 41.0 || in->temperature <= 33.0) danger_flags++;
    if (in->gcs_score <= 8) danger_flags += 2;
    if (danger_flags >= 2 || in->gcs_score <= 8) return AI_TRIAGE_P1;
    if (danger_flags >= 1) return AI_TRIAGE_P2;
    if (in->age >= 75 || in->spo2 <= 92) return AI_TRIAGE_P2;
    if (in->heart_rate >= 110 || in->systolic_bp >= 180) return AI_TRIAGE_P3;
    return AI_TRIAGE_P4;
}

/* L7: Drug safety index — Computes the minimum therapeutic margin between two drugs.
 * Therapeutic Index = TD50/ED50. Narrow TI drugs require careful monitoring.
 * Reference: FDA Guidance on Narrow Therapeutic Index Drugs.
 */

DSSInteractionSeverity dss_max_interaction_severity(DSSInteractionSeverity a, DSSInteractionSeverity b) {
    return (DSSInteractionSeverity)dss_compare_severity_int(a, b);
}

double dss_therapeutic_index_margin(double td50, double ed50) {
    if (ed50 <= 0.0) return 0.0;
    return td50 / ed50;
}

const char* dss_clinical_urgency_label(AI_TriagePriority priority) {
    switch (priority) {
        case AI_TRIAGE_P1: return "P1: Immediate (within minutes) — Life-threatening";
        case AI_TRIAGE_P2: return "P2: Very Urgent (within 10 min) — Potential life threat";
        case AI_TRIAGE_P3: return "P3: Urgent (within 60 min) — Stable but needs care";
        case AI_TRIAGE_P4: return "P4: Standard (within 120 min) — Non-urgent";
        default: return "Unknown priority";
    }
}

/* L8: Number Needed to Treat (NNT) and Number Needed to Harm (NNH)
 * NNT = 1 / ARR (Absolute Risk Reduction)
 * NNH = 1 / ARI (Absolute Risk Increase / Attributable Risk)
 * Reference: Laupacis et al., NEJM 1988
 */
double dss_nnt(double control_event_rate, double experimental_event_rate) {
    double arr = control_event_rate - experimental_event_rate;
    if (arr <= 0.0) return 9999.0;  /* No benefit */
    return 1.0 / arr;
}

double dss_nnh(double experimental_event_rate, double control_event_rate) {
    double ari = experimental_event_rate - control_event_rate;
    if (ari <= 0.0) return 9999.0;  /* No harm */
    return 1.0 / ari;
}

/* L8: Odds Ratio and Relative Risk
 * OR = (a*d) / (b*c) for 2x2 contingency table
 * RR = (a/(a+b)) / (c/(c+d))
 */
double dss_odds_ratio(double exposed_cases, double exposed_controls,
                      double unexposed_cases, double unexposed_controls) {
    double num = exposed_cases * unexposed_controls;
    double den = exposed_controls * unexposed_cases;
    if (den <= 0.0) return 0.0;
    return num / den;
}

double dss_relative_risk(double exposed_cases, double total_exposed,
                         double unexposed_cases, double total_unexposed) {
    double rate_exposed = (total_exposed > 0) ? exposed_cases / total_exposed : 0.0;
    double rate_unexposed = (total_unexposed > 0) ? unexposed_cases / total_unexposed : 0.0;
    if (rate_unexposed <= 0.0) return 9999.0;
    return rate_exposed / rate_unexposed;
}

/* L8: Fagan's Nomogram — converts pretest probability with LR to posttest
 * Uses odds transformation: post_odds = pre_odds * LR
 * probability = odds / (1 + odds)
 */
double dss_fagans_posttest_from_lr(double pretest_prob, double lr) {
    if (pretest_prob <= 0.0 || pretest_prob >= 1.0) return pretest_prob;
    double pre_odds = pretest_prob / (1.0 - pretest_prob);
    double post_odds = pre_odds * lr;
    return post_odds / (1.0 + post_odds);
}
