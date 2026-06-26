/* quality_control.c - Statistical Process Control & Quality Management
 * L2: Six Sigma DMAIC, SPC, Taguchi quality loss function
 * L4: ISO 9001:2015, ISO 2859 AQL sampling, ISO 7870 control charts
 * L5: X-bar/R charts, Cp/Cpk/Pp/Ppk, Western Electric rules
 * L6: SPC chart generation, capability study, Pareto analysis
 */
#include "quality_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_EPS
#define M_EPS 0.001f
#endif

/* ---- Utility ---- */

const char* smf_chart_type_name(SMF_ChartType type) {
    switch (type) {
        case SMF_CHART_XBAR_R: return "X-bar / R";
        case SMF_CHART_XBAR_S: return "X-bar / S";
        case SMF_CHART_I_MR:   return "Individuals / MR";
        case SMF_CHART_P:      return "p-chart";
        case SMF_CHART_NP:     return "np-chart";
        case SMF_CHART_C:      return "c-chart";
        case SMF_CHART_U:      return "u-chart";
        case SMF_CHART_EWMA:   return "EWMA";
        case SMF_CHART_CUSUM:  return "CUSUM";
        default:               return "Unknown";
    }
}

const char* smf_inspection_type_name(SMF_InspectionType type) {
    switch (type) {
        case SMF_INSP_VISUAL:         return "Visual";
        case SMF_INSP_DIMENSIONAL:    return "Dimensional";
        case SMF_INSP_FUNCTIONAL:     return "Functional";
        case SMF_INSP_MATERIAL:       return "Material";
        case SMF_INSP_NON_DESTRUCTIVE: return "Non-Destructive";
        case SMF_INSP_CMM:            return "CMM";
        default:                      return "Unknown";
    }
}

const char* smf_defect_severity_name(SMF_DefectSeverity severity) {
    switch (severity) {
        case SMF_DEFECT_CRITICAL: return "Critical";
        case SMF_DEFECT_MAJOR:    return "Major";
        case SMF_DEFECT_MINOR:    return "Minor";
        default:                  return "Unknown";
    }
}

/* ================================================================
 *  SPC Chart Constants (ASTM E2587 / ISO 7870-2)
 *
 *  Control chart constants for subgroup sizes n=2..25:
 *  A2 = 3 / (d2 * sqrt(n))  -- X-bar limits
 *  D3 = max(0, 1 - 3*d3/d2) -- R lower limit
 *  D4 = 1 + 3*d3/d2         -- R upper limit
 *  d2 = E[R/sigma]          -- expected range / sigma
 *  d3 = stddev(R/sigma)     -- standard deviation of R/sigma
 *
 *  For n=5: A2=0.577, D3=0.000, D4=2.114, d2=2.326
 * ================================================================ */

static const float _chart_a2[] = {
    0.000f, 0.000f, 1.880f, 1.023f, 0.729f, 0.577f,
    0.483f, 0.419f, 0.373f, 0.337f, 0.308f
};
static const float _chart_d3[] = {
    0.000f, 0.000f, 0.000f, 0.000f, 0.000f, 0.000f,
    0.000f, 0.076f, 0.136f, 0.184f, 0.223f
};
static const float _chart_d4[] = {
    0.000f, 0.000f, 3.267f, 2.574f, 2.282f, 2.114f,
    2.004f, 1.924f, 1.864f, 1.816f, 1.777f
};
static const float _chart_d2[] = {
    0.000f, 0.000f, 1.128f, 1.693f, 2.059f, 2.326f,
    2.534f, 2.704f, 2.847f, 2.970f, 3.078f
};

int smf_spc_init_xbar_r(SMF_ControlChart *chart, const char *name,
                          const char *characteristic, float usl, float lsl,
                          float target, int subgroup_size) {
    if (!chart || subgroup_size < 2 || subgroup_size > 10) return -1;
    memset(chart, 0, sizeof(SMF_ControlChart));
    chart->type = SMF_CHART_XBAR_R;
    if (name) strncpy(chart->name, name, SMF_NAME_LEN - 1);
    if (characteristic) strncpy(chart->characteristic, characteristic,
                                 SMF_DESC_LEN - 1);
    chart->usl = usl;
    chart->lsl = lsl;
    chart->target = target;
    chart->subgroup_size = subgroup_size;
    chart->a2 = _chart_a2[subgroup_size];
    chart->d3 = _chart_d3[subgroup_size];
    chart->d4 = _chart_d4[subgroup_size];
    chart->d2 = _chart_d2[subgroup_size];
    chart->a3 = 1.0f / (chart->d2 * sqrtf((float)subgroup_size)) * 3.0f;
    return 0;
}

int smf_spc_set_standard(SMF_ControlChart *chart, float mu, float sigma) {
    if (!chart) return -1;
    chart->cl = mu;
    chart->sigma = sigma;
    chart->ucl = mu + chart->a2 * sigma * chart->d2;
    chart->lcl = mu - chart->a2 * sigma * chart->d2;
    chart->ucl_r = chart->d4 * sigma * chart->d2;
    chart->lcl_r = chart->d3 * sigma * chart->d2;
    return 0;
}

int smf_spc_add_subgroup(SMF_SPCSubgroup *subgroup,
                          const float *measurements, int n) {
    if (!subgroup || !measurements || n <= 0) return -1;
    memset(subgroup, 0, sizeof(SMF_SPCSubgroup));
    subgroup->sample_size = n;

    float sum = 0.0f, min_val = measurements[0], max_val = measurements[0];
    for (int i = 0; i < n; i++) {
        sum += measurements[i];
        if (measurements[i] < min_val) min_val = measurements[i];
        if (measurements[i] > max_val) max_val = measurements[i];
    }
    subgroup->x_bar = sum / (float)n;
    subgroup->range = max_val - min_val;

    /* Sample standard deviation */
    float sum_sq = 0.0f;
    for (int i = 0; i < n; i++) {
        float diff = measurements[i] - subgroup->x_bar;
        sum_sq += diff * diff;
    }
    subgroup->std_dev = sqrtf(sum_sq / (float)(n - 1));
    subgroup->timestamp = time(NULL);
    return 0;
}

int smf_spc_compute_limits(SMF_ControlChart *chart,
                            const SMF_SPCSubgroup *subgroups,
                            int num_subgroups) {
    if (!chart || !subgroups || num_subgroups <= 0) return -1;

    /* Compute grand average (X_double_bar) and average range (R_bar) */
    float sum_xbar = 0.0f, sum_range = 0.0f;
    for (int i = 0; i < num_subgroups; i++) {
        sum_xbar += subgroups[i].x_bar;
        sum_range += subgroups[i].range;
    }

    float x_double_bar = sum_xbar / (float)num_subgroups;
    float r_bar = sum_range / (float)num_subgroups;

    chart->cl = x_double_bar;
    chart->sigma = r_bar / chart->d2;  /* Estimated process sigma */

    /* Control limits (Shewhart 1931) */
    chart->ucl = x_double_bar + chart->a2 * r_bar;
    chart->lcl = x_double_bar - chart->a2 * r_bar;
    chart->ucl_r = chart->d4 * r_bar;
    chart->lcl_r = chart->d3 * r_bar;
    chart->num_subgroups_used = num_subgroups;

    /* Check stability */
    chart->is_stable = true;
    for (int i = 0; i < num_subgroups; i++) {
        if (subgroups[i].x_bar > chart->ucl
            || subgroups[i].x_bar < chart->lcl) {
            chart->is_stable = false;
            break;
        }
    }

    /* Compute capability */
    smf_spc_capability(chart);

    return num_subgroups;
}

int smf_spc_capability(SMF_ControlChart *chart) {
    if (!chart) return -1;
    if (chart->usl <= chart->lsl) return -1;

    /* Cp = (USL - LSL) / (6 * sigma) */
    if (chart->sigma > 0.0f) {
        chart->cp = (chart->usl - chart->lsl) / (6.0f * chart->sigma);
        /* Cpk = min[(USL - mu)/(3*sigma), (mu - LSL)/(3*sigma)] */
        float cpk_upper = (chart->usl - chart->cl) / (3.0f * chart->sigma);
        float cpk_lower = (chart->cl - chart->lsl) / (3.0f * chart->sigma);
        chart->cpk = cpk_upper < cpk_lower ? cpk_upper : cpk_lower;
    }

    chart->pp = chart->cp;
    chart->ppk = chart->cpk;
    chart->is_capable = (chart->cpk >= 1.33f);

    /* Estimate percent nonconforming from Cp/Cpk (Normal assumption) */
    if (chart->cpk > 0.0f) {
        /* For normal distribution: ppm = 2 * Phi(-3*Cpk) * 1e6 */
        /* Approximate: */
        float z = 3.0f * chart->cpk;
        float ppm = 2.0f * (1.0f - 0.5f * (1.0f + erff(z / sqrtf(2.0f))));
        chart->percent_nonconforming = ppm * 100.0f;
        if (chart->percent_nonconforming < 0.0f) chart->percent_nonconforming = 0.0f;
    }

    return 0;
}

int smf_spc_check_subgroup(const SMF_ControlChart *chart,
                            const SMF_SPCSubgroup *subgroup,
                            int *violated_rules) {
    if (!chart || !subgroup) return -1;
    if (violated_rules) *violated_rules = 0;

    int rules = 0;

    /* Rule 1: Point beyond 3-sigma limits */
    if (subgroup->x_bar > chart->ucl || subgroup->x_bar < chart->lcl) {
        rules |= (1 << 0);
    }

    /* Rule 7: Range out of control */
    if (subgroup->range > chart->ucl_r && chart->ucl_r > 0.0f) {
        rules |= (1 << 6);
    }

    if (violated_rules) *violated_rules = rules;
    return rules;
}

/* ================================================================
 *  Western Electric Rules (WECO Rules)
 *
 *  Standard pattern-detection rules for SPC charts, detecting
 *  non-random patterns indicating assignable causes of variation.
 * ================================================================ */

const char* smf_weco_rule_name(int rule_bit) {
    switch (rule_bit) {
        case 0: return "Rule 1: Point beyond 3-sigma";
        case 1: return "Rule 2: 2 of 3 beyond 2-sigma (same side)";
        case 2: return "Rule 3: 4 of 5 beyond 1-sigma (same side)";
        case 3: return "Rule 4: 8 consecutive points same side of CL";
        case 4: return "Rule 5: 6 consecutive points trending";
        case 5: return "Rule 6: 14 consecutive points alternating";
        case 6: return "Rule 7: 15 consecutive points within 1-sigma";
        case 7: return "Rule 8: 8 points beyond 1-sigma both sides";
        default: return "Unknown Rule";
    }
}

int smf_weco_check_points(const float *values, const float *ranges,
                           int num_points, float cl, float ucl, float lcl,
                           float sigma) {
    if (!values || num_points < 1) return 0;

    float one_sigma = sigma;
    float two_sigma = 2.0f * sigma;
    int rules = 0;

    /* Rule 1: Check each point beyond 3-sigma */
    for (int i = 0; i < num_points; i++) {
        if (values[i] > ucl || values[i] < lcl) {
            rules |= (1 << 0);
            break;
        }
    }

    /* Rule 2: 2 of 3 beyond 2-sigma (same side) */
    for (int i = 0; i <= num_points - 3; i++) {
        int above = 0, below = 0;
        for (int j = 0; j < 3; j++) {
            if (values[i + j] > cl + two_sigma) above++;
            if (values[i + j] < cl - two_sigma) below++;
        }
        if (above >= 2 || below >= 2) { rules |= (1 << 1); break; }
    }

    /* Rule 3: 4 of 5 beyond 1-sigma (same side) */
    for (int i = 0; i <= num_points - 5; i++) {
        int above = 0, below = 0;
        for (int j = 0; j < 5; j++) {
            if (values[i + j] > cl + one_sigma) above++;
            if (values[i + j] < cl - one_sigma) below++;
        }
        if (above >= 4 || below >= 4) { rules |= (1 << 2); break; }
    }

    /* Rule 4: 8 consecutive points same side of center line */
    for (int i = 0; i <= num_points - 8; i++) {
        bool all_above = true, all_below = true;
        for (int j = 0; j < 8; j++) {
            if (values[i + j] <= cl) all_above = false;
            if (values[i + j] >= cl) all_below = false;
        }
        if (all_above || all_below) { rules |= (1 << 3); break; }
    }

    /* Rule 5: 6 consecutive points trending up or down */
    for (int i = 0; i <= num_points - 6; i++) {
        bool up = true, down = true;
        for (int j = 1; j < 6; j++) {
            if (values[i + j] <= values[i + j - 1]) up = false;
            if (values[i + j] >= values[i + j - 1]) down = false;
        }
        if (up || down) { rules |= (1 << 4); break; }
    }

    /* Rule 6: 14 consecutive points alternating up/down */
    if (num_points >= 14) {
        for (int i = 0; i <= num_points - 14; i++) {
            bool alternating = true;
            for (int j = 1; j < 14; j++) {
                bool up_j = values[i + j] > values[i + j - 1];
                bool up_prev = values[i + j - 1] > values[i + j - 2];
                if (j > 1 && up_j == up_prev) { alternating = false; break; }
            }
            if (alternating) { rules |= (1 << 5); break; }
        }
    }

    /* Rule 7: 15 consecutive points within 1-sigma (stratification) */
    for (int i = 0; i <= num_points - 15; i++) {
        bool within = true;
        for (int j = 0; j < 15; j++) {
            if (values[i + j] > cl + one_sigma
                || values[i + j] < cl - one_sigma) {
                within = false;
                break;
            }
        }
        if (within) { rules |= (1 << 6); break; }
    }

    /* Rule 8: 8 points beyond 1-sigma on both sides */
    for (int i = 0; i <= num_points - 8; i++) {
        bool all_beyond = true;
        for (int j = 0; j < 8; j++) {
            float dev = values[i + j] - cl;
            if (dev > -one_sigma && dev < one_sigma) {
                all_beyond = false;
                break;
            }
        }
        if (all_beyond) { rules |= (1 << 7); break; }
    }

    (void)ranges; /* Range rules not implemented in this simplified version */
    return rules;
}

/* ================================================================
 *  Process Capability Indices (ISO 22514-4)
 *
 *  Cp  = (USL - LSL) / (6 * sigma_within)     -- Potential capability
 *  Cpk = min[(USL - mu)/(3*sigma), (mu - LSL)/(3*sigma)] -- Actual capability
 *  Pp  = (USL - LSL) / (6 * sigma_overall)    -- Process performance
 *  Ppk = min[(USL - mu)/(3*sigma_overall), (mu - LSL)/(3*sigma_overall)]
 *
 *  Interpretation: < 1.00 = not capable, 1.00-1.33 = marginal,
 *                   1.33-1.67 = capable, 1.67-2.00 = good, > 2.00 = excellent
 * ================================================================ */

float smf_calc_cp(float usl, float lsl, float sigma_within) {
    if (sigma_within <= 0.0f || usl <= lsl) return 0.0f;
    return (usl - lsl) / (6.0f * sigma_within);
}

float smf_calc_cpk(float usl, float lsl, float mean, float sigma) {
    if (sigma <= 0.0f || usl <= lsl) return 0.0f;
    float cpu = (usl - mean) / (3.0f * sigma);
    float cpl = (mean - lsl) / (3.0f * sigma);
    return cpu < cpl ? cpu : cpl;
}

float smf_calc_pp(float usl, float lsl, float sigma_overall) {
    if (sigma_overall <= 0.0f || usl <= lsl) return 0.0f;
    return (usl - lsl) / (6.0f * sigma_overall);
}

float smf_calc_ppk(float usl, float lsl, float mean, float sigma_overall) {
    if (sigma_overall <= 0.0f || usl <= lsl) return 0.0f;
    float ppu = (usl - mean) / (3.0f * sigma_overall);
    float ppl = (mean - lsl) / (3.0f * sigma_overall);
    return ppu < ppl ? ppu : ppl;
}

const char* smf_capability_rating(float cpk) {
    if (cpk < 0.0f)  return "Impossible (mean outside specs)";
    if (cpk < 1.0f)  return "Not Capable";
    if (cpk < 1.33f) return "Marginally Capable";
    if (cpk < 1.67f) return "Capable";
    if (cpk < 2.0f)  return "Good";
    return "Excellent (Six Sigma)";
}

/* ================================================================
 *  Inspection Management
 * ================================================================ */

/* Internal storage for inspection records */
static SMF_InspectionRecord _insp_records[4096];
static int _insp_count = 0;

int smf_inspection_create(SMF_Factory *factory, int wo_idx,
                           int operation_idx, SMF_InspectionType type,
                           int lot_size) {
    if (!factory || _insp_count >= 4096) return -1;

    int id = _insp_count++;
    SMF_InspectionRecord *insp = &_insp_records[id];
    memset(insp, 0, sizeof(SMF_InspectionRecord));
    insp->inspection_id = id;
    insp->timestamp = time(NULL);
    insp->work_order_idx = wo_idx;
    insp->operation_idx = operation_idx;
    insp->type = type;
    insp->lot_size = lot_size;
    insp->sample_size = 0;
    insp->defects_found = 0;
    insp->num_defects = 0;
    insp->lot_accepted = true;
    return id;
}

int smf_inspection_record_result(SMF_Factory *factory,
                                  int inspection_id, int defects,
                                  const int *defect_codes,
                                  const SMF_DefectSeverity *severities,
                                  int num_defects) {
    if (!factory || inspection_id < 0 || inspection_id >= _insp_count)
        return -1;

    SMF_InspectionRecord *insp = &_insp_records[inspection_id];
    insp->defects_found = defects;
    insp->num_defects = num_defects < 8 ? num_defects : 8;

    for (int i = 0; i < insp->num_defects; i++) {
        insp->defect_codes[i] = defect_codes ? defect_codes[i] : -1;
        insp->severities[i] = severities ? severities[i] : SMF_DEFECT_MINOR;
    }

    return 0;
}

bool smf_inspection_accept_lot(const SMF_InspectionRecord *inspection,
                                float aql_percent, int sample_size) {
    if (!inspection) return false;

    /* Simplified AQL acceptance: accept if defect rate <= AQL
     * ISO 2859 uses binomial sampling; simplified here as:
     * defect_percent = (defects / sample_size) * 100 */
    if (sample_size <= 0) return true;
    float defect_pct = (float)inspection->defects_found
                       / (float)sample_size * 100.0f;
    return defect_pct <= aql_percent;
}

/* ================================================================
 *  Pareto Analysis (L6: Quality Improvement)
 *
 *  Pareto Principle: ~80% of effects come from ~20% of causes.
 *  Used for prioritizing quality improvement efforts.
 * ================================================================ */

int smf_pareto_analyze(const int *defect_counts, const char **defect_names,
                        int num_categories, SMF_ParetoItem *result) {
    if (!defect_counts || !result || num_categories <= 0) return 0;

    /* Compute total defects */
    int total = 0;
    for (int i = 0; i < num_categories; i++) {
        total += defect_counts[i];
    }

    if (total <= 0) return 0;

    /* Initialize results */
    for (int i = 0; i < num_categories; i++) {
        result[i].defect_code = i;
        if (defect_names && defect_names[i]) {
            strncpy(result[i].defect_name, defect_names[i],
                    SMF_NAME_LEN - 1);
        }
        result[i].frequency = defect_counts[i];
        result[i].percentage = (float)defect_counts[i]
                               / (float)total * 100.0f;
    }

    /* Sort by descending frequency */
    for (int i = 0; i < num_categories - 1; i++) {
        for (int j = i + 1; j < num_categories; j++) {
            if (result[i].frequency < result[j].frequency) {
                SMF_ParetoItem tmp = result[i];
                result[i] = result[j];
                result[j] = tmp;
            }
        }
    }

    /* Compute cumulative percentages */
    float cum = 0.0f;
    for (int i = 0; i < num_categories; i++) {
        cum += result[i].percentage;
        result[i].cumulative_pct = cum;
    }

    return num_categories;
}

/* ================================================================
 *  Root Cause Analysis - 5-Why Method (Toyota Production System)
 *
 *  The 5-Why technique, developed by Sakichi Toyoda, iteratively
 *  asks "why" to drill down from symptom to root cause.
 *  Typically 5 iterations reach the systemic root cause.
 * ================================================================ */

int smf_rca_5why(SMF_RootCauseAnalysis *rca, const char *problem) {
    if (!rca || !problem) return -1;
    memset(rca, 0, sizeof(SMF_RootCauseAnalysis));
    strncpy(rca->problem_statement, problem, SMF_DESC_LEN - 1);
    rca->num_whys = 0;
    rca->is_resolved = false;
    rca->root_cause_category = -1;
    rca->corrective_action_idx = -1;
    return 0;
}

const char* smf_ishikawa_category(int category_id) {
    switch (category_id) {
        case 0: return "Man (People)";
        case 1: return "Machine (Equipment)";
        case 2: return "Method (Process)";
        case 3: return "Material (Raw Material)";
        case 4: return "Measurement (Inspection)";
        case 5: return "Mother Nature (Environment)";
        default: return "Unknown Category";
    }
}