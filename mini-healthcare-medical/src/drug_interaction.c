#include "drug_interaction.h"
#include <string.h>
#include <stdio.h>

/*
 * L4: Drug-Drug Interaction (DDI) Graph — Pharmacology Standard
 *
 * Theorem (Graph Contraindication): If drug A and drug B share a
 * contraindicated edge in the DDI graph, they MUST NOT be co-prescribed
 * regardless of dose adjustment (absolute contraindication, FDA §201.57).
 *
 * Reference:
 * - FDA Guidance for Industry: Drug Interaction Studies (2020)
 * - WHO-UMC Causality Assessment System
 * - Stanford Medicine: "Drug Interactions: Cytochrome P450 System"
 * - CMU 15-445: Adjacency matrix for O(1) interaction lookup
 */

void ddi_graph_init(DDIGraph *g)
{
    if (!g) return;
    memset(g, 0, sizeof(*g));
    for (int i = 0; i < DDI_MAX_DRUGS; i++)
        for (int j = 0; j < DDI_MAX_DRUGS; j++)
            g->severity_matrix[i][j] = -1; /* -1 = no known interaction */
    g->num_drugs = 0;
}

int ddi_register_drug(DDIGraph *g, int drug_id)
{
    if (!g || g->num_drugs >= DDI_MAX_DRUGS) return -1;
    /* check if already registered */
    for (int i = 0; i < g->num_drugs; i++)
        if (g->drug_index[i] == drug_id) return i;
    int idx = g->num_drugs++;
    g->drug_index[idx] = drug_id;
    return idx;
}

static int find_drug_idx(const DDIGraph *g, int drug_id)
{
    if (!g) return -1;
    for (int i = 0; i < g->num_drugs; i++)
        if (g->drug_index[i] == drug_id) return i;
    return -1;
}

int ddi_add_interaction(DDIGraph *g, int drug_a, int drug_b,
                         DDISeverity sev, DDIMechanism mech,
                         const char *desc, char evidence)
{
    if (!g || g->count >= DDI_MAX_INTERACTIONS) return -1;
    int ia = find_drug_idx(g, drug_a);
    int ib = find_drug_idx(g, drug_b);
    if (ia < 0 || ib < 0) return -1;

    DrugInteraction *di = &g->interactions[g->count];
    di->drug_a = drug_a;
    di->drug_b = drug_b;
    di->severity = sev;
    di->mechanism = mech;
    if (desc) strncpy(di->description, desc, 255);
    di->evidence_level = evidence;

    /* set adjacency matrix (undirected) */
    g->severity_matrix[ia][ib] = (int)sev;
    g->severity_matrix[ib][ia] = (int)sev;
    g->count++;
    return 0;
}

DDISeverity ddi_check(const DDIGraph *g, int drug_a, int drug_b)
{
    if (!g) return DDI_SEV_UNKNOWN;
    int ia = find_drug_idx(g, drug_a);
    int ib = find_drug_idx(g, drug_b);
    if (ia < 0 || ib < 0) return DDI_SEV_UNKNOWN;
    int s = g->severity_matrix[ia][ib];
    return (s >= 0) ? (DDISeverity)s : DDI_SEV_UNKNOWN;
}

int ddi_find_all(const DDIGraph *g, int drug_id,
                  DrugInteraction *results, int max_results)
{
    if (!g || !results) return 0;
    int idx = find_drug_idx(g, drug_id);
    if (idx < 0) return 0;
    int found = 0;
    for (int i = 0; i < g->count && found < max_results; i++) {
        if (g->interactions[i].drug_a == drug_id ||
            g->interactions[i].drug_b == drug_id) {
            results[found++] = g->interactions[i];
        }
    }
    return found;
}

bool ddi_has_contraindication(const DDIGraph *g, int drug_id,
                               const int *co_meds, int num_co_meds)
{
    if (!g || !co_meds) return false;
    for (int i = 0; i < num_co_meds; i++) {
        DDISeverity s = ddi_check(g, drug_id, co_meds[i]);
        if (s == DDI_SEV_CONTRAINDICATED || s == DDI_SEV_MAJOR)
            return true;
    }
    return false;
}

/*
 * L7: Polypharmacy Risk Score
 *
 * Polypharmacy (≥5 concurrent drugs) increases adverse drug event risk
 * exponentially with each additional drug due to multiplicative DDI
 * combinatorics.  The risk score combines:
 *   - Interaction count: # of known DDIs in the set
 *   - Severity weighting: major=3, moderate=2, minor=1
 *   - Allergy penalty: any drug matches patient allergy
 *
 * Reference: Masnoon et al. (2017) "What is Polypharmacy? A Systematic Review"
 *            Guthrie et al. (2015) "The rising tide of polypharmacy"
 */
double ddi_polypharmacy_risk(const DDIGraph *g, const int *drug_ids, int n,
                              const int *patient_allergies, int n_allergies)
{
    if (!g || !drug_ids || n < 1) return 0.0;
    double risk = 0.0;

    /* baseline: polypharmacy threshold effect */
    if (n >= 5) risk += 1.0 + 0.2 * (double)(n - 5);

    /* interaction-based risk */
    int interaction_count = 0;
    double severity_sum = 0.0;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            DDISeverity s = ddi_check(g, drug_ids[i], drug_ids[j]);
            if (s != DDI_SEV_UNKNOWN) {
                interaction_count++;
                switch (s) {
                    case DDI_SEV_CONTRAINDICATED: severity_sum += 5.0; break;
                    case DDI_SEV_MAJOR:           severity_sum += 3.0; break;
                    case DDI_SEV_MODERATE:        severity_sum += 1.5; break;
                    case DDI_SEV_MINOR:           severity_sum += 0.5; break;
                    default: break;
                }
            }
        }
    }
    risk += 0.5 * (double)interaction_count + severity_sum;

    /* allergy cross-reactivity penalty */
    if (patient_allergies && n_allergies > 0) {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n_allergies; j++) {
                if (drug_ids[i] == patient_allergies[j]) {
                    risk += 10.0; /* severe allergy penalty */
                }
            }
        }
    }

    return risk;
}

/* L1: String representations */

const char* ddi_severity_name(DDISeverity s)
{
    switch (s) {
        case DDI_SEV_CONTRAINDICATED: return "Contraindicated";
        case DDI_SEV_MAJOR:           return "Major";
        case DDI_SEV_MODERATE:        return "Moderate";
        case DDI_SEV_MINOR:           return "Minor";
        default:                      return "Unknown";
    }
}

const char* ddi_mechanism_name(DDIMechanism m)
{
    switch (m) {
        case DDI_MECH_CYP450:        return "CYP450 enzyme metabolism";
        case DDI_MECH_PGP:           return "P-glycoprotein transport";
        case DDI_MECH_QT_PROLONG:    return "QT interval prolongation";
        case DDI_MECH_SEROTONIN:     return "Serotonin syndrome";
        case DDI_MECH_ADDITIVE_CNS:  return "Additive CNS depression";
        case DDI_MECH_PH_ABSORPTION: return "pH-dependent absorption";
        case DDI_MECH_RENAL:         return "Renal clearance competition";
        case DDI_MECH_PLASMA_BINDING:return "Plasma protein binding";
        default:                     return "Unknown mechanism";
    }
}
