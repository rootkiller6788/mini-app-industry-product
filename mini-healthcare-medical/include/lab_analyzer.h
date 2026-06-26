#ifndef LAB_ANALYZER_H
#define LAB_ANALYZER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/**
 * lab_analyzer.h -- Clinical Laboratory Test Analyzer
 *
 * L4 Standards: CLIA (Clinical Laboratory Improvement Amendments) reference ranges
 *               LOINC (Logical Observation Identifiers Names and Codes)
 * L5 Algorithms: Critical value detection, trend analysis (regression), delta check
 * L7 Application: Clinical Decision Support System (CDSS) integration
 *
 * Reference ranges based on standard clinical laboratory guidelines.
 * Trend analysis uses linear regression on historical lab results.
 *
 * Course mapping:
 *   - MIT HST.951 (Medical Decision Support)
 *   - Stanford BIOMEDIN 215 (Clinical Informatics)
 */

#define LAB_MAX_TESTS        128
#define LAB_MAX_HISTORY       64
#define LAB_MAX_NAME_LEN       64
#define LAB_MAX_UNIT_LEN       16
#define LAB_MAX_RESULT_STR    128

typedef enum {
    LAB_CBC_WBC,      /* White Blood Cell count */
    LAB_CBC_RBC,      /* Red Blood Cell count */
    LAB_CBC_HGB,      /* Hemoglobin */
    LAB_CBC_HCT,      /* Hematocrit */
    LAB_CBC_PLT,      /* Platelets */
    LAB_CHEM_GLUCOSE, /* Glucose */
    LAB_CHEM_BUN,     /* Blood Urea Nitrogen */
    LAB_CHEM_CREAT,   /* Creatinine */
    LAB_CHEM_NA,      /* Sodium */
    LAB_CHEM_K,       /* Potassium */
    LAB_CHEM_CL,      /* Chloride */
    LAB_CHEM_CO2,     /* CO2 / Bicarbonate */
    LAB_CHEM_CA,      /* Calcium */
    LAB_CHEM_ALT,     /* ALT (liver enzyme) */
    LAB_CHEM_AST,     /* AST (liver enzyme) */
    LAB_CHEM_ALB,     /* Albumin */
    LAB_CHEM_TP,      /* Total Protein */
    LAB_CHEM_BILI,    /* Bilirubin */
    LAB_CARDIAC_TNI,  /* Troponin I */
    LAB_CARDIAC_BNP,  /* BNP */
    LAB_COAG_PT,      /* Prothrombin Time */
    LAB_COAG_PTT,     /* Partial Thromboplastin Time */
    LAB_COAG_INR,     /* INR */
    LAB_URINE_PH,     /* Urine pH */
    LAB_URINE_SG,     /* Urine Specific Gravity */
    LAB_COUNT
} LabTestType;

typedef enum {
    LAB_NORMAL,
    LAB_LOW,
    LAB_HIGH,
    LAB_CRITICAL_LOW,
    LAB_CRITICAL_HIGH,
    LAB_PANIC_LOW,
    LAB_PANIC_HIGH
} LabResultFlag;

typedef struct {
    double low;
    double high;
    double critical_low;
    double critical_high;
    double panic_low;
    double panic_high;
} LabRefRange;

typedef struct {
    LabTestType type;
    char name[LAB_MAX_NAME_LEN];
    char unit[LAB_MAX_UNIT_LEN];
    LabRefRange range;
    int loinc_code;
} LabTest;

typedef struct {
    LabTestType test_type;
    double value;
    time_t collected_at;
    time_t reported_at;
    char specimen_id[64];
    LabResultFlag flag;
    char comment[LAB_MAX_RESULT_STR];
} LabResult;

typedef struct {
    LabResult history[LAB_MAX_HISTORY];
    int count;
    LabTestType test_type;
} LabHistory;

typedef struct {
    LabTest tests[LAB_MAX_TESTS];
    int test_count;
} LabTestCatalog;

/* L1: Catalog management */
LabTestCatalog* lab_catalog_create(void);
void            lab_catalog_destroy(LabTestCatalog *cat);
int             lab_catalog_load_defaults(LabTestCatalog *cat);
LabTest*        lab_catalog_find(LabTestCatalog *cat, LabTestType type);
const char*     lab_test_name(LabTestType t);
const char*     lab_flag_name(LabResultFlag f);

/* L4: Reference range evaluation */
LabResultFlag lab_evaluate_result(const LabTest *test, double value);
bool          lab_is_critical(LabResultFlag flag);
bool          lab_is_panic(LabResultFlag flag);
int           lab_flag_severity(LabResultFlag flag);

/* L5: Trend analysis via linear regression
 * y = mx + b, where m = slope, b = intercept
 * Pearson correlation r = Σ(x-x̄)(y-ȳ) / sqrt(Σ(x-x̄)² Σ(y-ȳ)²)
 */
typedef struct {
    double slope;
    double intercept;
    double r_squared;
    int n_points;
} LabTrend;

LabTrend lab_compute_trend(const LabHistory *hist);
int      lab_predict_next(const LabTrend *trend, double *out);

/* L5: Delta check — flags significant changes between consecutive results */
typedef struct {
    double delta_value;
    double delta_percent;
    bool significant;
    bool increase;
} DeltaCheck;

DeltaCheck lab_delta_check(const LabResult *prev, const LabResult *curr, double threshold_pct);

/* L7: Clinical interpretation */
const char* lab_interpret_result(const LabTest *test, LabResultFlag flag, char *buf, int buf_sz);
int         lab_panel_summary(const LabResult *results, int count, char *buf, int buf_sz);

/* L8: Special clinical calculations */
typedef struct { double egfr; int stage; const char *classification; } EgfrResult;
EgfrResult lab_calculate_egfr(double creatinine, int age, bool is_female, bool is_black);
double lab_corrected_calcium(double ca_measured, double albumin);
double lab_ldl_cholesterol(double total_chol, double hdl, double triglycerides);
double lab_serum_osmolality(double sodium, double glucose, double bun);
double lab_anion_gap(double sodium, double chloride, double bicarbonate);
bool   lab_is_anion_gap_elevated(double ag);
double lab_meld_score(double bilirubin, double inr, double creatinine);
double lab_rpi(double retic_percent, double hct);
double lab_osmolar_gap(double measured_osm, double calculated_osm);
bool   lab_is_osmolar_gap_elevated(double gap);

#endif
