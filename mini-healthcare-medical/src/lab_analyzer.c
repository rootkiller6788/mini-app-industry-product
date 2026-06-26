#include "lab_analyzer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ================================================================
 * lab_analyzer.c -- Clinical Laboratory Test Analyzer
 *
 * L4: CLIA reference ranges for standard lab tests
 * L5: Linear regression trend analysis, delta check algorithm
 * L7: Clinical interpretation for decision support
 * ================================================================ */

/* Standard reference ranges (adult, conventional units)
 * Based on CLIA guidelines and standard clinical laboratory references.
 */

static const LabTest default_tests[] = {
    {LAB_CBC_WBC,     "WBC",     "K/uL",   {4.0,  11.0,  1.5,  30.0,  0.5,  50.0},  6690},
    {LAB_CBC_RBC,     "RBC",     "M/uL",   {4.2,  6.1,   2.5,  8.0,   1.5,  10.0},  789},
    {LAB_CBC_HGB,     "Hemoglobin", "g/dL",{12.0, 17.5,  7.0,  22.0,  5.0,  25.0},  718},
    {LAB_CBC_HCT,     "Hematocrit", "%",  {36.0, 52.0,  20.0, 65.0,  15.0, 70.0},  4544},
    {LAB_CBC_PLT,     "Platelets", "K/uL",{150.0,400.0, 50.0, 1000.0,20.0, 2000.0},777},
    {LAB_CHEM_GLUCOSE,"Glucose",   "mg/dL",{70.0, 110.0, 40.0, 400.0, 20.0, 600.0},2345},
    {LAB_CHEM_BUN,    "BUN",       "mg/dL",{7.0,  22.0,  2.0,  80.0,  1.0,  120.0}, 3094},
    {LAB_CHEM_CREAT,  "Creatinine","mg/dL",{0.6,  1.3,   0.2,  6.0,   0.1,  10.0},  2160},
    {LAB_CHEM_NA,     "Sodium",    "mEq/L",{135.0,148.0, 120.0,160.0, 110.0,170.0}, 2951},
    {LAB_CHEM_K,      "Potassium", "mEq/L",{3.5,  5.2,   2.5,  6.5,   2.0,  7.5},   2823},
    {LAB_CHEM_CL,     "Chloride",  "mEq/L",{98.0, 110.0, 85.0, 120.0, 80.0, 130.0}, 2075},
    {LAB_CHEM_CO2,    "CO2",       "mEq/L",{22.0, 32.0,  12.0, 40.0,  8.0,  50.0},  2028},
    {LAB_CHEM_CA,     "Calcium",   "mg/dL",{8.5,  10.5,  6.0,  14.0,  5.0,  16.0},  17861},
    {LAB_CHEM_ALT,    "ALT",       "U/L",  {7.0,  55.0,  3.0,  500.0, 1.0,  1000.0},1742},
    {LAB_CHEM_AST,    "AST",       "U/L",  {8.0,  48.0,  3.0,  500.0, 1.0,  1000.0},1920},
    {LAB_CHEM_ALB,    "Albumin",   "g/dL", {3.5,  5.5,   2.0,  7.0,   1.5,  8.0},    1751},
    {LAB_CHEM_TP,     "Total Protein","g/dL",{6.0,8.3,   4.0,  10.0,  3.0,  12.0},   2885},
    {LAB_CHEM_BILI,   "Bilirubin", "mg/dL",{0.1,  1.2,   0.05, 5.0,   0.01, 10.0},   1975},
    {LAB_CARDIAC_TNI, "Troponin I","ng/mL",{0.0,  0.04,  0.04, 1.0,   0.04, 5.0},    4276},
    {LAB_CARDIAC_BNP, "BNP",       "pg/mL",{0.0,  100.0, 100.0,500.0, 100.0,1000.0}, 3097},
    {LAB_COAG_PT,     "PT",        "sec",  {9.5,  13.5,  8.0,  20.0,  6.0,  30.0},   5902},
    {LAB_COAG_PTT,    "PTT",       "sec",  {25.0, 38.0,  20.0, 60.0,  15.0, 80.0},    3173},
    {LAB_COAG_INR,    "INR",       "",     {0.9,  1.2,   0.7,  3.0,   0.5,  5.0},     3471},
    {LAB_URINE_PH,    "Urine pH",  "",     {5.0,  8.0,   4.0,  9.0,   3.0,  10.0},    2756},
    {LAB_URINE_SG,    "Urine SG",  "",     {1.005,1.030, 1.001,1.040, 1.000,1.050},   5811},
};

LabTestCatalog* lab_catalog_create(void) {
    LabTestCatalog *cat = (LabTestCatalog*)calloc(1, sizeof(LabTestCatalog));
    return cat;
}

void lab_catalog_destroy(LabTestCatalog *cat) {
    if (cat) free(cat);
}

int lab_catalog_load_defaults(LabTestCatalog *cat) {
    if (!cat) return -1;
    int n = sizeof(default_tests) / sizeof(default_tests[0]);
    for (int i = 0; i < n && cat->test_count < LAB_MAX_TESTS; i++) {
        cat->tests[cat->test_count++] = default_tests[i];
    }
    return cat->test_count;
}

LabTest* lab_catalog_find(LabTestCatalog *cat, LabTestType type) {
    if (!cat) return NULL;
    for (int i = 0; i < cat->test_count; i++)
        if (cat->tests[i].type == type) return &cat->tests[i];
    return NULL;
}

const char* lab_test_name(LabTestType t) {
    for (int i = 0; i < (int)(sizeof(default_tests)/sizeof(default_tests[0])); i++)
        if (default_tests[i].type == t) return default_tests[i].name;
    return "Unknown";
}

const char* lab_flag_name(LabResultFlag f) {
    switch (f) {
        case LAB_NORMAL:        return "Normal";
        case LAB_LOW:           return "Low";
        case LAB_HIGH:          return "High";
        case LAB_CRITICAL_LOW:  return "Critical Low";
        case LAB_CRITICAL_HIGH: return "Critical High";
        case LAB_PANIC_LOW:     return "PANIC Low";
        case LAB_PANIC_HIGH:    return "PANIC High";
        default: return "?";
    }
}

/* ---- L4: Reference Range Evaluation ---- */

LabResultFlag lab_evaluate_result(const LabTest *test, double value) {
    if (!test) return LAB_NORMAL;
    const LabRefRange *r = &test->range;
    if (value <= r->panic_low)  return LAB_PANIC_LOW;
    if (value >= r->panic_high) return LAB_PANIC_HIGH;
    if (value <= r->critical_low)  return LAB_CRITICAL_LOW;
    if (value >= r->critical_high) return LAB_CRITICAL_HIGH;
    if (value < r->low)  return LAB_LOW;
    if (value > r->high) return LAB_HIGH;
    return LAB_NORMAL;
}

bool lab_is_critical(LabResultFlag flag) {
    return flag == LAB_CRITICAL_LOW || flag == LAB_CRITICAL_HIGH ||
           flag == LAB_PANIC_LOW || flag == LAB_PANIC_HIGH;
}

bool lab_is_panic(LabResultFlag flag) {
    return flag == LAB_PANIC_LOW || flag == LAB_PANIC_HIGH;
}

int lab_flag_severity(LabResultFlag flag) {
    switch (flag) {
        case LAB_NORMAL:  return 0;
        case LAB_LOW:     return 1;
        case LAB_HIGH:    return 1;
        case LAB_CRITICAL_LOW:  return 3;
        case LAB_CRITICAL_HIGH: return 3;
        case LAB_PANIC_LOW:     return 5;
        case LAB_PANIC_HIGH:    return 5;
        default: return 0;
    }
}

/* ---- L5: Trend Analysis via Linear Regression ---- */
/* y = mx + b
 * m = (n*Σxy - Σx*Σy) / (n*Σx² - (Σx)²)
 * b = (Σy - m*Σx) / n
 * r² = (n*Σxy - Σx*Σy)² / ((n*Σx² - (Σx)²)(n*Σy² - (Σy)²))
 */

LabTrend lab_compute_trend(const LabHistory *hist) {
    LabTrend trend = {0.0, 0.0, 0.0, 0};
    if (!hist || hist->count < 3) return trend;

    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0, sum_y2 = 0.0;
    int n = hist->count;

    for (int i = 0; i < n; i++) {
        double x = (double)i;
        double y = hist->history[i].value;
        sum_x  += x;
        sum_y  += y;
        sum_xy += x * y;
        sum_x2 += x * x;
        sum_y2 += y * y;
    }

    double denom = n * sum_x2 - sum_x * sum_x;
    if (fabs(denom) < 1e-10) return trend;

    trend.slope     = (n * sum_xy - sum_x * sum_y) / denom;
    trend.intercept = (sum_y - trend.slope * sum_x) / n;
    double r_num    = n * sum_xy - sum_x * sum_y;
    double r_denom  = (n * sum_x2 - sum_x * sum_x) * (n * sum_y2 - sum_y * sum_y);
    trend.r_squared = (fabs(r_denom) > 1e-10) ? (r_num * r_num) / r_denom : 0.0;
    trend.n_points  = n;
    return trend;
}

int lab_predict_next(const LabTrend *trend, double *out) {
    if (!trend || !out) return -1;
    *out = trend->slope * trend->n_points + trend->intercept;
    return 0;
}

/* ---- L5: Delta Check ---- */
/* Delta check flags significant changes between consecutive lab results.
 * delta_percent = |curr - prev| / prev * 100
 * Significant if delta_percent > threshold_pct and delta_value > 0.
 */

DeltaCheck lab_delta_check(const LabResult *prev, const LabResult *curr, double threshold_pct) {
    DeltaCheck dc = {0.0, 0.0, false, false};
    if (!prev || !curr) return dc;
    dc.delta_value = curr->value - prev->value;
    dc.increase = dc.delta_value > 0;
    if (fabs(prev->value) > 1e-10) {
        dc.delta_percent = fabs(dc.delta_value) / fabs(prev->value) * 100.0;
        dc.significant = dc.delta_percent > threshold_pct;
    } else if (fabs(dc.delta_value) > 1e-10) {
        dc.delta_percent = 100.0;
        dc.significant = true;
    }
    return dc;
}

/* ---- L7: Clinical Interpretation ---- */

const char* lab_interpret_result(const LabTest *test, LabResultFlag flag, char *buf, int buf_sz) {
    if (!test || !buf) return NULL;
    const char *msg = "";
    switch (flag) {
        case LAB_NORMAL: msg = "Within normal range."; break;
        case LAB_LOW:    msg = "Below normal range. Consider clinical correlation."; break;
        case LAB_HIGH:   msg = "Above normal range. Consider clinical correlation."; break;
        case LAB_CRITICAL_LOW:
            msg = "CRITICAL LOW. Requires immediate clinical attention.";
            break;
        case LAB_CRITICAL_HIGH:
            msg = "CRITICAL HIGH. Requires immediate clinical attention.";
            break;
        case LAB_PANIC_LOW:
            msg = "PANIC VALUE LOW. Life-threatening. Call provider immediately.";
            break;
        case LAB_PANIC_HIGH:
            msg = "PANIC VALUE HIGH. Life-threatening. Call provider immediately.";
            break;
    }
    snprintf(buf, buf_sz, "%s: %.1f %s [%s] — %s",
             test->name, 0.0, test->unit,
             lab_flag_name(flag), msg);
    return buf;
}

int lab_panel_summary(const LabResult *results, int count, char *buf, int buf_sz) {
    if (!results || !buf) return -1;
    int off = snprintf(buf, buf_sz, "Lab Panel Summary (%d results):\n", count);
    int abnormal = 0, critical = 0;
    for (int i = 0; i < count && off < buf_sz; i++) {
        const char *name = lab_test_name(results[i].test_type);
        const char *flag = lab_flag_name(results[i].flag);
        off += snprintf(buf + off, buf_sz - off, "  %-15s %8.2f  [%s]\n",
                        name, results[i].value, flag);
        if (results[i].flag != LAB_NORMAL) abnormal++;
        if (lab_is_critical(results[i].flag)) critical++;
    }
    off += snprintf(buf + off, buf_sz - off,
                    "Summary: %d normal, %d abnormal, %d critical\n",
                    count - abnormal, abnormal, critical);
    return off;
}

/* ---- L5: Estimated GFR (eGFR) via CKD-EPI 2021 formula ---- */
/* Reference: Inker et al., NEJM 2021. CKD-EPI creatinine equation (2021)
 *   eGFR = 142 * (Scr/k)^a * (0.9938)^age * [1.012 if female]
 *   where k = 0.7 (female) or 0.9 (male), a = -0.241 (female) or -0.302 (male)
 *   Scr in mg/dL
 * EgfrResult struct defined in lab_analyzer.h
 */

EgfrResult lab_calculate_egfr(double creatinine, int age, bool is_female, bool is_black) {
    EgfrResult r = {0.0, 0, "Unknown"};
    if (creatinine <= 0 || age <= 0) return r;

    double k = is_female ? 0.7 : 0.9;
    double a = is_female ? -0.241 : -0.302;
    double race_factor = is_black ? 1.0 : 1.0;  /* 2021 equation removed race */

    double ratio = creatinine / k;
    double ratio_pow = pow(ratio, a);
    double age_factor = pow(0.9938, (double)age);
    double female_factor = is_female ? 1.012 : 1.0;

    r.egfr = 142.0 * ratio_pow * age_factor * female_factor;
    (void)race_factor;

    if (r.egfr >= 90)      { r.stage = 1; r.classification = "Normal"; }
    else if (r.egfr >= 60) { r.stage = 2; r.classification = "Mildly decreased"; }
    else if (r.egfr >= 45) { r.stage = 3; r.classification = "Mild-moderate CKD"; }
    else if (r.egfr >= 30) { r.stage = 3; r.classification = "Moderate-severe CKD"; }
    else if (r.egfr >= 15) { r.stage = 4; r.classification = "Severe CKD"; }
    else                   { r.stage = 5; r.classification = "Kidney failure"; }
    return r;
}

/* Anion Gap: AG = Na - (Cl + HCO3), normal 8-16 mEq/L */
double lab_anion_gap(double sodium, double chloride, double bicarbonate) {
    return sodium - (chloride + bicarbonate);
}

bool lab_is_anion_gap_elevated(double ag) {
    return ag > 16.0;
}

/* Corrected Calcium (for hypoalbuminemia):
 *   Ca_corrected = Ca_measured + 0.8 * (4.0 - Albumin)
 * Reference: Payne et al., BMJ 1973
 */
double lab_corrected_calcium(double ca_measured, double albumin) {
    return ca_measured + 0.8 * (4.0 - albumin);
}

/* LDL Cholesterol via Friedewald equation (valid when TG < 400 mg/dL):
 *   LDL = TC - HDL - TG/5
 * Reference: Friedewald et al., Clinical Chemistry 1972
 */
double lab_ldl_cholesterol(double total_chol, double hdl, double triglycerides) {
    if (triglycerides > 400) return -1;  /* Invalid for TG >= 400 */
    return total_chol - hdl - triglycerides / 5.0;
}

/* Serum Osmolality (calculated):
 *   Osm = 2*Na + Glucose/18 + BUN/2.8
 * Reference: Worthley et al., Archives of Internal Medicine 1987
 */
double lab_serum_osmolality(double sodium, double glucose, double bun) {
    return 2.0 * sodium + glucose / 18.0 + bun / 2.8;
}

/* Osmolar Gap: measured_osm - calculated_osm (>10 suggests toxin) */
double lab_osmolar_gap(double measured_osm, double calculated_osm) {
    return measured_osm - calculated_osm;
}

bool lab_is_osmolar_gap_elevated(double gap) {
    return gap > 10.0;
}

/* Reticulocyte Production Index (RPI): corrects retic count for anemia
 *   RPI = (retic% * HCT / 45) / maturation_factor
 *   maturation_factor: HCT 45=1.0, 35=1.5, 25=2.0, 15=2.5
 */
double lab_rpi(double retic_percent, double hct) {
    double mat_factor = 1.0;
    if (hct <= 15) mat_factor = 2.5;
    else if (hct <= 25) mat_factor = 2.0;
    else if (hct <= 35) mat_factor = 1.5;
    return (retic_percent * hct / 45.0) / mat_factor;
}

/* MELD Score (Model for End-Stage Liver Disease):
 *   MELD = 3.78 * ln(serum bilirubin) + 11.2 * ln(INR) + 9.57 * ln(creatinine) + 6.43
 * Reference: Kamath et al., Hepatology 2001
 */
double lab_meld_score(double bilirubin, double inr, double creatinine) {
    double bili_log = bilirubin > 0 ? log(bilirubin) : 0;
    double inr_log  = inr > 0 ? log(inr) : 0;
    double creat_log = creatinine > 0 ? log(creatinine) : 0;
    double meld = 3.78 * bili_log + 11.2 * inr_log + 9.57 * creat_log + 6.43;
    if (meld < 6) meld = 6;  /* MELD floor */
    return meld;
}
