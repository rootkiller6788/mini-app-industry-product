/**
 * mini-industry-solution: safety_analysis.c
 * HAZOP, LOPA, FMEA, Fault Tree, Event Tree, Bow-Tie, ALARP
 * Ref: IEC 61508/61511, ISO 31000, ANSI/ISA 84.00.01,
 *      Leveson (2011) STPA, IEC 61882 HAZOP
 */
#include "safety_analysis.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* === HAZOP Analysis (L5) === */
/*
 * HAZOP (HAZard and OPerability) is a structured team-based
 * technique for identifying hazards in process plants.
 * Uses guidewords applied to process parameters.
 * Ref: IEC 61882, Kletz (1999) HAZOP and HAZAN
 */

int hazop_init_record(HAZOPRecord *rec, const char *node,
                       const char *parameter, HAZOPGuideword gw) {
    if (!rec) return -1;
    memset(rec, 0, sizeof(*rec));
    if (node) {
        strncpy(rec->node, node, sizeof(rec->node) - 1);
        rec->node[sizeof(rec->node) - 1] = '\0';
    }
    if (parameter) {
        strncpy(rec->parameter, parameter, sizeof(rec->parameter) - 1);
        rec->parameter[sizeof(rec->parameter) - 1] = '\0';
    }
    rec->guideword = gw;
    return 0;
}

void hazop_set_deviation(HAZOPRecord *rec, const char *deviation,
                          const char *cause, const char *consequence) {
    if (!rec) return;
    if (deviation) {
        strncpy(rec->deviation, deviation, sizeof(rec->deviation) - 1);
        rec->deviation[sizeof(rec->deviation) - 1] = '\0';
    }
    if (cause) {
        strncpy(rec->cause, cause, sizeof(rec->cause) - 1);
        rec->cause[sizeof(rec->cause) - 1] = '\0';
    }
    if (consequence) {
        strncpy(rec->consequence, consequence, sizeof(rec->consequence) - 1);
        rec->consequence[sizeof(rec->consequence) - 1] = '\0';
    }
}

void hazop_set_safeguard(HAZOPRecord *rec, const char *safeguard,
                          const char *recommendation) {
    if (!rec) return;
    if (safeguard) {
        strncpy(rec->safeguard, safeguard, sizeof(rec->safeguard) - 1);
        rec->safeguard[sizeof(rec->safeguard) - 1] = '\0';
    }
    if (recommendation) {
        strncpy(rec->recommendation, recommendation,
                sizeof(rec->recommendation) - 1);
        rec->recommendation[sizeof(rec->recommendation) - 1] = '\0';
    }
}

void hazop_assess_risk(HAZOPRecord *rec, HazardSeverity sev,
                        HazardLikelihood like) {
    if (!rec) return;
    rec->severity = sev;
    rec->likelihood = like;
    rec->risk = risk_matrix_evaluate(sev, like);
}

/* === Risk Matrix (L4-L5) === */
/*
 * Standard 4x5 or 5x5 risk matrix per ISO 31000.
 * Risk = Severity x Likelihood (or matrix lookup).
 *
 *   Likelihood \ Severity | Negl | Marg | Crit | Catast
 *   ----------------------+------+------+------+--------
 *   Frequent              | MED  | HIGH | INTO | INTO
 *   Likely                | MED  | MED  | HIGH | INTO
 *   Possible              | LOW  | MED  | HIGH | HIGH
 *   Unlikely              | LOW  | LOW  | MED  | HIGH
 *   Rare                  | LOW  | LOW  | LOW  | MED
 */

RiskLevel risk_matrix_evaluate(HazardSeverity severity,
                                HazardLikelihood likelihood) {
    static const RiskLevel matrix[5][4] = {
        {RISK_LOW,    RISK_LOW,       RISK_MEDIUM,      RISK_HIGH},
        {RISK_LOW,    RISK_LOW,       RISK_MEDIUM,      RISK_HIGH},
        {RISK_LOW,    RISK_MEDIUM,     RISK_HIGH,        RISK_HIGH},
        {RISK_MEDIUM, RISK_MEDIUM,     RISK_HIGH,        RISK_INTOLERABLE},
        {RISK_MEDIUM, RISK_HIGH,       RISK_INTOLERABLE, RISK_INTOLERABLE}
    };
    return matrix[likelihood][severity];
}

int risk_acceptable(RiskLevel level) {
    return (level == RISK_LOW || level == RISK_MEDIUM) ? 1 : 0;
}

const char *risk_level_name(RiskLevel level) {
    switch (level) {
    case RISK_LOW:          return "Low";
    case RISK_MEDIUM:       return "Medium";
    case RISK_HIGH:         return "High";
    case RISK_INTOLERABLE:  return "Intolerable";
    default:                return "Unknown";
    }
}

/* === LOPA (L5) === */
/*
 * Layer of Protection Analysis per IEC 61511 / CCPS.
 *
 * Mitigated event frequency = IE_freq * Product of IPL PFDs
 * If mitigated frequency > tolerable frequency, additional SIL required.
 *
 * RRF_required = mitigated_frequency / tolerable_frequency
 *
 * Each IPL (Independent Protection Layer) has a PFD.
 * Safety Instrumented Functions provide the highest RRF.
 */

double lopa_event_frequency(double ie_freq,
                              const double *ipl_pfds, int n_ipls) {
    double mitigated = ie_freq;
    for (int i = 0; i < n_ipls; i++)
        mitigated *= (ipl_pfds[i] > 0.0) ? ipl_pfds[i] : 1.0;
    return mitigated;
}

int lopa_sil_determine(double tolerable_freq,
                         double mitigated_freq,
                         SIL *out_sil) {
    if (!out_sil || tolerable_freq <= 0.0) return -1;
    double rrf_needed = mitigated_freq / tolerable_freq;

    if (rrf_needed <= 1.0) { *out_sil = SSIL_NONE; return 0; }
    if (rrf_needed <= 10.0) { *out_sil = SSIL_1; return 0; }
    if (rrf_needed <= 100.0) { *out_sil = SSIL_2; return 0; }
    if (rrf_needed <= 1000.0) { *out_sil = SSIL_3; return 0; }
    *out_sil = SSIL_4;
    return 0;
}

double lopa_required_rrf(double tolerable_freq,
                           double mitigated_freq) {
    if (tolerable_freq <= 0.0) return 1e6;
    return mitigated_freq / tolerable_freq;
}

/* === FMEA (L5) === */
/*
 * Failure Mode and Effects Analysis per MIL-STD-1629A / AIAG FMEA.
 * RPN = Severity x Occurrence x Detection (each 1-10 scale)
 * Higher RPN = higher priority for corrective action.
 */

void fmea_init_record(FMEARecord *rec, const char *item,
                       const char *function) {
    if (!rec) return;
    memset(rec, 0, sizeof(*rec));
    if (item) {
        strncpy(rec->item, item, sizeof(rec->item) - 1);
        rec->item[sizeof(rec->item) - 1] = '\0';
    }
    if (function) {
        strncpy(rec->function, function, sizeof(rec->function) - 1);
        rec->function[sizeof(rec->function) - 1] = '\0';
    }
}

int fmea_calculate_rpn(FMEARecord *rec) {
    if (!rec) return -1;
    rec->rpn = rec->severity_rank * rec->occurrence_rank
               * rec->detection_rank;
    return rec->rpn;
}

static int cmp_rpn(const void *a, const void *b) {
    return ((const FMEARecord *)b)->rpn - ((const FMEARecord *)a)->rpn;
}

int fmea_sort_by_rpn(FMEARecord *records, int count) {
    if (!records || count <= 0) return -1;
    qsort(records, (size_t)count, sizeof(FMEARecord), cmp_rpn);
    return 0;
}

/* === Fault Tree Analysis (L5) === */
/*
 * Fault Tree Analysis quantifies the probability of a top event.
 *
 * AND gate: P = product of child probabilities
 * OR gate:  P = 1 - product of (1 - child probability)
 *
 * Uses MOCUS (Method of Obtaining Cut Sets) for minimal cut sets.
 */

void fta_init_node(FTANode *node, const char *label, FTAGateType type) {
    if (!node) return;
    memset(node, 0, sizeof(*node));
    if (label) {
        strncpy(node->label, label, sizeof(node->label) - 1);
        node->label[sizeof(node->label) - 1] = '\0';
    }
    node->type = type;
}

void fta_add_child(FTANode *parent, FTANode *child) {
    if (!parent || !child || parent->n_children >= 8) return;
    parent->children[parent->n_children++] = child;
}

double fta_compute_probability(FTANode *root) {
    if (!root) return 0.0;

    if (root->type == FTA_BASIC || root->type == FTA_UNDEVELOPED)
        return root->probability;

    if (root->n_children == 0) return root->probability;

    if (root->type == FTA_AND) {
        double p = 1.0;
        for (int i = 0; i < root->n_children; i++)
            p *= fta_compute_probability(root->children[i]);
        return p;
    }

    if (root->type == FTA_OR) {
        double p = 1.0;
        for (int i = 0; i < root->n_children; i++)
            p *= (1.0 - fta_compute_probability(root->children[i]));
        return 1.0 - p;
    }

    return root->probability;
}

void fta_minimal_cut_sets(FTANode *root, int max_sets) {
    (void)root;
    (void)max_sets;
    /* Minimal cut set extraction using MOCUS algorithm.
     * Full implementation requires recursive set manipulation
     * and absorption. Documented for L8 reference. */
}

/* === Event Tree Analysis (L5) === */

void eta_init(EventTree *et, const char *event, double freq,
                ETABranch *branches, int n_levels, int bpl) {
    if (!et) return;
    memset(et, 0, sizeof(*et));
    if (event) {
        strncpy(et->initiating_event, event,
                sizeof(et->initiating_event) - 1);
        et->initiating_event[sizeof(et->initiating_event) - 1] = '\0';
    }
    et->initiating_freq = freq;
    et->branches = branches;
    et->n_levels = n_levels;
    et->n_branches = n_levels * bpl;
}

double eta_outcome_probability(const EventTree *et, int *path,
                                int path_len) {
    if (!et || !path || path_len <= 0) return 0.0;
    double prob = et->initiating_freq;
    for (int i = 0; i < path_len && i < et->n_levels; i++) {
        int idx = i * 2 + path[i];
        if (idx < et->n_branches)
            prob *= et->branches[idx].branch_probability;
    }
    return prob;
}

/* === ALARP (L5) === */
/*
 * ALARP (As Low As Reasonably Practicable) per UK HSE.
 * A risk is ALARP if the cost of further reduction is
 * grossly disproportionate to the benefit gained.
 *
 * Disproportion factor typically 1-10 depending on risk level.
 */

int alarp_evaluate(RiskLevel risk, double cbr, double df) {
    if (risk == RISK_LOW) return 1;
    if (risk == RISK_INTOLERABLE) return 0;
    return (cbr < df) ? 1 : 0;
}

/*
 * Simplified safety distance calculation for toxic gas dispersion.
 * Based on Gaussian plume model (Pasquill-Gifford).
 *
 * d = (release_rate / (pi * u * toxicity_limit))^(1/2)
 * where: u = wind speed, toxicity_limit = ERPG/IDLH
 */
double safety_distance_calculate(double release_rate,
                                   double concentration,
                                   double toxicity_limit,
                                   double wind_speed) {
    if (toxicity_limit <= 0.0 || wind_speed <= 0.0) return 0.0;
    double denom = M_PI * wind_speed * toxicity_limit;
    if (denom <= 0.0) return 0.0;
    /* Release rate with concentration factor */
    double effective_rate = release_rate * concentration;
    return sqrt(effective_rate / denom);
}

/* === Bow-Tie Analysis (L6) === */

int bowtie_analyze(const BowTieDiagram *bt, RiskLevel *out_risk) {
    if (!bt || !out_risk) return -1;

    /* Assess preventive barrier effectiveness */
    double event_prob = 1.0;
    for (int i = 0; i < bt->n_preventive; i++) {
        event_prob *= (bt->preventive_barriers[i].independent
                       ? bt->preventive_barriers[i].pfd : 1.0);
    }

    /* Assess mitigative barrier effectiveness */
    double mitig_prob = 1.0;
    for (int i = 0; i < bt->n_mitigative; i++) {
        mitig_prob *= (bt->mitigative_barriers[i].independent
                       ? bt->mitigative_barriers[i].pfd : 1.0);
    }

    double residual = event_prob * mitig_prob;

    if (residual < 1e-4) *out_risk = RISK_LOW;
    else if (residual < 1e-3) *out_risk = RISK_MEDIUM;
    else if (residual < 1e-2) *out_risk = RISK_HIGH;
    else *out_risk = RISK_INTOLERABLE;

    return 0;
}

/* === SIF Design (L6-L7) === */

void sil_pfd_single(double pfd_sensor, double pfd_logic,
                      double pfd_actuator, double *out_pfd,
                      SIL *out_sil) {
    if (!out_pfd || !out_sil) return;
    *out_pfd = pfd_sensor + pfd_logic + pfd_actuator;

    if (*out_pfd < 1e-4) *out_sil = SSIL_4;
    else if (*out_pfd < 1e-3) *out_sil = SSIL_3;
    else if (*out_pfd < 1e-2) *out_sil = SSIL_2;
    else if (*out_pfd < 1e-1) *out_sil = SSIL_1;
    else *out_sil = SSIL_NONE;
}
