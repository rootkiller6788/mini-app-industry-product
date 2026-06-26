#ifndef CLINICAL_DSS_H
#define CLINICAL_DSS_H

#include <stdbool.h>
#include <stdint.h>

#define DSS_MAX_DRUGS 256
#define DSS_MAX_INTERACTIONS 1024
#define DSS_MAX_SCORES 64

/* L1: Risk score structures */
typedef enum { DSS_RISK_LOW, DSS_RISK_MODERATE, DSS_RISK_HIGH, DSS_RISK_CRITICAL } DSSRiskLevel;

typedef struct {
    char     name[64];
    double   score;
    DSSRiskLevel risk;
    char     recommendation[256];
} DSSRiskAssessment;

/* L1: Drug interaction types */
typedef enum {
    DSS_INTERACTION_NONE,
    DSS_INTERACTION_MILD,
    DSS_INTERACTION_MODERATE,
    DSS_INTERACTION_SEVERE,
    DSS_INTERACTION_CONTRAINDICATED
} DSSInteractionSeverity;

typedef struct {
    char     drug_a[64];
    char     drug_b[64];
    DSSInteractionSeverity severity;
    char     mechanism[256];
    char     recommendation[256];
} DSSDrugInteraction;

/* L1: Drug database entry */
typedef struct {
    int      id;
    char     generic_name[128];
    char     brand_name[128];
    char     atc_code[16];
    double   half_life_h;
    double   therapeutic_index;
    bool     controlled_substance;
    uint64_t max_daily_dose_mg;
} DSSDrug;

typedef struct {
    DSSDrug  drugs[DSS_MAX_DRUGS];
    int      drug_count;
    DSSDrugInteraction interactions[DSS_MAX_INTERACTIONS];
    int      interaction_count;
} DSSDrugDatabase;

/* L5: Clinical risk scores */
typedef struct {
    int      age;
    bool     female;
    bool     has_chf;
    bool     has_hypertension;
    bool     has_diabetes;
    bool     has_stroke_tia;
    bool     has_vascular_disease;
} CHA2DS2_VASc_Input;

typedef struct {
    int      score;
    double   stroke_risk_pct;
    bool     needs_anticoagulation;
    char     recommendation[256];
} CHA2DS2_VASc_Result;

typedef struct {
    bool     has_hypertension;
    bool     has_renal_disease;
    bool     has_liver_disease;
    bool     has_bleeding_history;
    bool     labile_inr;
    bool     elderly;
    bool     alcohol_use;
    bool     nsaid_use;
} HAS_BLED_Input;

typedef struct {
    int      score;
    double   major_bleeding_risk_pct;
    char     risk_category[32];
    char     recommendation[256];
} HAS_BLED_Result;

/* L5: Dosage calculators */
typedef struct {
    double   weight_kg;
    double   height_cm;
    double   bsa_m2;
    double   creatinine_mg_dl;
    int      age;
    bool     female;
} DosageInput;

typedef struct {
    double   bsa_m2;
    double   crcl_ml_min;
    double   adjusted_dose_mg;
    char     dosing_regimen[256];
} DosageResult;

typedef struct {
    int      age;
    double   creatinine_mg_dl;
    bool     female;
    bool     african_american;
} EGFR_Input;

/* L7: Clinical decision rule engine */
typedef struct {
    char     rule_name[128];
    int      conditions_count;
    bool     (*conditions[8])(const void *ctx);
    char     actions[8][256];
    int      priority;
} DSSRule;

typedef struct {
    DSSRule  rules[DSS_MAX_SCORES];
    int      rule_count;
    int      triggers_fired;
    char     alert_log[4][512];
    int      alert_count;
} DSSRuleEngine;

/* Core API */
void dss_db_init(DSSDrugDatabase *db);
int  dss_db_add_drug(DSSDrugDatabase *db, const char *generic, const char *brand, const char *atc, double half_life, uint64_t max_daily);
DSSDrug* dss_db_find_drug(DSSDrugDatabase *db, const char *generic_name);
void dss_db_add_interaction(DSSDrugDatabase *db, const char *drug_a, const char *drug_b, DSSInteractionSeverity sev, const char *mechanism, const char *rec);
DSSInteractionSeverity dss_check_interaction(const DSSDrugDatabase *db, const char *drug_a, const char *drug_b);
int  dss_find_all_interactions(const DSSDrugDatabase *db, const char *drugs[], int count, DSSDrugInteraction *results, int max_results);
int  dss_filter_interactions_by_threshold(const DSSDrugDatabase *db, DSSInteractionSeverity threshold, DSSDrugInteraction *results, int max_results);

/* L5: CHA2DS2-VASc (stroke risk in AF) */
CHA2DS2_VASc_Result dss_cha2ds2_vasc(const CHA2DS2_VASc_Input *in);
HAS_BLED_Result dss_has_bled(const HAS_BLED_Input *in);

/* L5: Dosage calculation (BSA ? Mosteller formula, CrCl ? Cockcroft-Gault) */
double dss_bsa_mosteller(double weight_kg, double height_cm);
double dss_crcl_cockcroft_gault(const EGFR_Input *in);
double dss_egfr_ckd_epi(const EGFR_Input *in);
DosageResult dss_calculate_dosage(const DosageInput *in, double standard_dose_mg);
double dss_body_surface_area(double weight_kg, double height_cm);

/* L6: Drug dosing adjustments for renal impairment */
double dss_renal_dose_adjustment(double crcl, double normal_dose);
const char* dss_renal_impairment_stage(double egfr);

/* L7: Rule engine */
void dss_rule_engine_init(DSSRuleEngine *engine);
int  dss_rule_add(DSSRuleEngine *engine, const char *name, int priority);
bool dss_rule_add_condition(DSSRuleEngine *engine, int rule_idx, bool (*cond)(const void*), const char *action);
int  dss_rule_engine_evaluate(DSSRuleEngine *engine, const void *context);
const char* dss_rule_engine_get_alert(const DSSRuleEngine *engine, int idx);

/* L8: Bayesian diagnostic probability */
typedef struct {
    double   pretest_probability;
    double   sensitivity;
    double   specificity;
} DSSBayesianInput;

double dss_bayesian_posttest(double pretest, double sensitivity, double specificity);
double dss_likelihood_ratio_positive(double sensitivity, double specificity);
double dss_likelihood_ratio_negative(double sensitivity, double specificity);
double dss_fagans_nomogram(double pretest, double lr);

/* L9: AI-assisted triage (simplified ML classifier ? documented as frontier) */
typedef struct {
    int      resp_rate;
    int      heart_rate;
    int      systolic_bp;
    double   temperature;
    int      spo2;
    int      age;
    int      gcs_score;
} AI_Triage_Input;

typedef enum { AI_TRIAGE_P1, AI_TRIAGE_P2, AI_TRIAGE_P3, AI_TRIAGE_P4 } AI_TriagePriority;
AI_TriagePriority dss_ai_triage_classify(const AI_Triage_Input *in);
const char* dss_clinical_urgency_label(AI_TriagePriority priority);

/* L8: Interaction severity comparison */
DSSInteractionSeverity dss_max_interaction_severity(DSSInteractionSeverity a, DSSInteractionSeverity b);

/* L8: Therapeutic index */
double dss_therapeutic_index_margin(double td50, double ed50);

/* L7: Evidence-Based Medicine: NNT/NNH (Number Needed to Treat/Harm) */
double dss_nnt(double control_event_rate, double experimental_event_rate);
double dss_nnh(double experimental_event_rate, double control_event_rate);

/* L8: Epidemiology: Odds Ratio and Relative Risk */
double dss_odds_ratio(double exposed_cases, double exposed_controls,
                      double unexposed_cases, double unexposed_controls);
double dss_relative_risk(double exposed_cases, double total_exposed,
                         double unexposed_cases, double total_unexposed);

/* L8: Fagan's Nomogram from LR */
double dss_fagans_posttest_from_lr(double pretest_prob, double lr);

#endif
