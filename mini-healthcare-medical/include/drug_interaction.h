#ifndef DRUG_INTERACTION_H
#define DRUG_INTERACTION_H

#include <stdbool.h>
#include <stdint.h>

#define DDI_MAX_DRUGS 256
#define DDI_MAX_INTERACTIONS 1024

/* L4: Drug-Drug Interaction severity classification (FDA/WHO standard) */
typedef enum {
    DDI_SEV_CONTRAINDICATED,  /* never co-administer */
    DDI_SEV_MAJOR,            /* life-threatening, avoid combination */
    DDI_SEV_MODERATE,         /* may require dose adjustment / monitoring */
    DDI_SEV_MINOR,            /* clinically insignificant */
    DDI_SEV_UNKNOWN
} DDISeverity;

typedef enum {
    DDI_MECH_CYP450,         /* cytochrome P450 metabolism */
    DDI_MECH_PGP,            /* P-glycoprotein transport */
    DDI_MECH_QT_PROLONG,     /* QT interval prolongation */
    DDI_MECH_SEROTONIN,      /* serotonin syndrome risk */
    DDI_MECH_ADDITIVE_CNS,   /* additive CNS depression */
    DDI_MECH_PH_ABSORPTION,  /* pH-dependent absorption */
    DDI_MECH_RENAL,          /* renal clearance competition */
    DDI_MECH_PLASMA_BINDING  /* protein-binding displacement */
} DDIMechanism;

/* L1: Core definition — drug interaction edge in the DDI graph */
typedef struct {
    int drug_a;              /* source drug ID */
    int drug_b;              /* target drug ID */
    DDISeverity severity;
    DDIMechanism mechanism;
    char description[256];   /* clinical recommendation */
    char evidence_level;     /* A= RCT, B= observational, C= case report, D= theoretical */
} DrugInteraction;

/* L3: Engineering structure — adjacency-list DDI graph for O(1) lookup */
typedef struct {
    DrugInteraction interactions[DDI_MAX_INTERACTIONS];
    int count;
    /* fast lookup: adjacency matrix of severity (0 = no interaction) */
    int severity_matrix[DDI_MAX_DRUGS][DDI_MAX_DRUGS];
    int drug_index[DDI_MAX_DRUGS]; /* drug_id -> matrix index */
    int num_drugs;
} DDIGraph;

/* API */
void ddi_graph_init(DDIGraph *g);
int  ddi_register_drug(DDIGraph *g, int drug_id);
int  ddi_add_interaction(DDIGraph *g, int drug_a, int drug_b,
                          DDISeverity sev, DDIMechanism mech,
                          const char *desc, char evidence);
DDISeverity ddi_check(const DDIGraph *g, int drug_a, int drug_b);
int  ddi_find_all(const DDIGraph *g, int drug_id,
                   DrugInteraction *results, int max_results);
bool ddi_has_contraindication(const DDIGraph *g, int drug_id,
                               const int *co_meds, int num_co_meds);

/* L7: Polypharmacy risk score (≥5 concurrent drugs = polypharmacy risk) */
double ddi_polypharmacy_risk(const DDIGraph *g, const int *drug_ids, int n,
                              const int *patient_allergies, int n_allergies);

/* L4: Interaction severity from WHO-UMC causality categories:
 * Certain, Probable, Possible, Unlikely, Unclassified */
const char* ddi_severity_name(DDISeverity s);
const char* ddi_mechanism_name(DDIMechanism m);

#endif
