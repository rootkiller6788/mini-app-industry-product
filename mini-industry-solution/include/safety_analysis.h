/**
 * mini-industry-solution: safety_analysis.h
 *
 * Process Safety Analysis - HAZOP, LOPA, FMEA, Fault Tree, Event Tree
 * ===================================================================
 *
 * L1 Definitions: Hazard, RiskAssessment, SafetyBarrier, SILTarget
 * L2 Core Concepts: ALARP principle, Risk matrix, Swiss Cheese Model
 * L3 Engineering: Safety lifecycle (IEC 61511), SIS design workflow
 * L4 Standards: IEC 61508 (Functional Safety), IEC 61511 (Process SIS),
 *               ISO 31000 (Risk Management), ANSI/ISA 84.00.01
 * L5 Algorithms: HAZOP guidewords, Fault Tree probability (MOCUS),
 *                Event Tree outcome, LOPA gap analysis
 * L6 Canonical Problems: Layer of Protection Analysis, SIL verification
 * L7 Applications: SIF design, Safety requirement specification
 * L8 Advanced: STPA/STAMP (Leveson), Formal verification of SIS
 * L9 Industry Frontiers: Autonomous safety, AI in safety systems
 */

#ifndef SAFETY_ANALYSIS_H
#define SAFETY_ANALYSIS_H

#include <stddef.h>
#include <time.h>

/* === L1: Core Definitions === */

typedef enum {
    SEVERITY_NEGLIGIBLE, /* S1: Minor injury, no lost time */
    SEVERITY_MARGINAL,   /* S2: Minor injury, lost time */
    SEVERITY_CRITICAL,   /* S3: Major injury, permanent disability */
    SEVERITY_CATASTROPHIC/* S4: Fatality, multiple fatalities */
} HazardSeverity;

typedef enum {
    LIKELIHOOD_RARE,       /* < 10^-4 per year */
    LIKELIHOOD_UNLIKELY,   /* 10^-4 to 10^-3 */
    LIKELIHOOD_POSSIBLE,   /* 10^-3 to 10^-2 */
    LIKELIHOOD_LIKELY,     /* 10^-2 to 10^-1 */
    LIKELIHOOD_FREQUENT    /* > 10^-1 */
} HazardLikelihood;

typedef enum {
    RISK_LOW,
    RISK_MEDIUM,
    RISK_HIGH,
    RISK_INTOLERABLE
} RiskLevel;

typedef enum {
    BARRIER_PHYSICAL,
    BARRIER_PROCEDURAL,
    BARRIER_INSTRUMENTED,
    BARRIER_INHERENT
} BarrierType;

typedef enum { SSIL_NONE, SSIL_1, SSIL_2, SSIL_3, SSIL_4 } SIL;

typedef struct {
    char name[64];
    char description[256];
    HazardSeverity severity;
    HazardLikelihood likelihood;
    RiskLevel risk;
    char cause[128];
    char consequence[256];
} Hazard;

typedef struct {
    char name[64];
    BarrierType type;
    double pfd;
    char description[128];
    int independent;
    int audited;
} SafetyBarrier;

typedef struct {
    Hazard hazard;
    RiskLevel inherent_risk;
    SafetyBarrier *barriers;
    int n_barriers;
    RiskLevel residual_risk;
    SIL required_sil;
    int alarp_met;
} RiskAssessment;

typedef struct {
    char tag[48];
    char description[256];
    double demand_rate;
    double required_rrf;
    SIL target_sil;
    double achieved_rrf;
    SIL achieved_sil;
    int compliant;
} SafetyInstrumentedFunction;

/* === L5: HAZOP Analysis === */

typedef enum {
    GUIDE_NO, GUIDE_MORE, GUIDE_LESS, GUIDE_AS_WELL_AS,
    GUIDE_PART_OF, GUIDE_REVERSE, GUIDE_OTHER_THAN,
    GUIDE_EARLY, GUIDE_LATE, GUIDE_BEFORE, GUIDE_AFTER
} HAZOPGuideword;

typedef struct {
    char node[48];
    char parameter[32];
    HAZOPGuideword guideword;
    char deviation[128];
    char cause[128];
    char consequence[256];
    char safeguard[128];
    char recommendation[256];
    HazardSeverity severity;
    HazardLikelihood likelihood;
    RiskLevel risk;
} HAZOPRecord;

int  hazop_init_record(HAZOPRecord *rec, const char *node,
                        const char *parameter, HAZOPGuideword gw);
void hazop_set_deviation(HAZOPRecord *rec, const char *deviation,
                          const char *cause, const char *consequence);
void hazop_set_safeguard(HAZOPRecord *rec, const char *safeguard,
                          const char *recommendation);
void hazop_assess_risk(HAZOPRecord *rec, HazardSeverity sev,
                        HazardLikelihood like);

/* === L5: Risk Matrix === */

RiskLevel risk_matrix_evaluate(HazardSeverity severity,
                                HazardLikelihood likelihood);
int       risk_acceptable(RiskLevel level);
const char *risk_level_name(RiskLevel level);

/* === L5: LOPA (Layer of Protection Analysis) === */

double lopa_event_frequency(double initiating_event_freq,
                              const double *ipl_pfds, int n_ipls);
int    lopa_sil_determine(double tolerable_frequency,
                            double mitigated_frequency,
                            SIL *out_sil);
double lopa_required_rrf(double tolerable_freq, double mitigated_freq);

/* === L5: FMEA === */

typedef struct {
    char item[64];
    char function[128];
    char failure_mode[128];
    char failure_effect[256];
    char failure_cause[128];
    int severity_rank;
    int occurrence_rank;
    int detection_rank;
    int rpn;
    char action[256];
} FMEARecord;

void fmea_init_record(FMEARecord *rec, const char *item,
                       const char *function);
int  fmea_calculate_rpn(FMEARecord *rec);
int  fmea_sort_by_rpn(FMEARecord *records, int count);

/* === L5: Fault Tree Analysis === */

typedef enum { FTA_AND, FTA_OR, FTA_BASIC, FTA_UNDEVELOPED } FTAGateType;

typedef struct FTANode {
    char label[64];
    FTAGateType type;
    double probability;
    struct FTANode *children[8];
    int n_children;
} FTANode;

void    fta_init_node(FTANode *node, const char *label, FTAGateType type);
void    fta_add_child(FTANode *parent, FTANode *child);
double  fta_compute_probability(FTANode *root);
void    fta_minimal_cut_sets(FTANode *root, int max_sets);

/* === L5: Event Tree Analysis === */

typedef enum { ETA_SUCCESS, ETA_FAILURE, ETA_PARTIAL } ETABranchType;

typedef struct ETANode {
    char label[64];
    double branch_probability;
    ETABranchType type;
    char consequence[256];
} ETABranch;

typedef struct {
    char initiating_event[64];
    double initiating_freq;
    ETABranch *branches;
    int n_branches;
    int n_levels;
} EventTree;

void   eta_init(EventTree *et, const char *event, double freq,
                  ETABranch *branches, int n_levels, int branches_per_level);
double eta_outcome_probability(const EventTree *et, int *path, int path_len);

/* === L5: ALARP Evaluation === */

int  alarp_evaluate(RiskLevel risk, double cost_benefit_ratio,
                      double disproportion_factor);
double safety_distance_calculate(double release_rate, double concentration,
                                   double toxicity_limit, double wind_speed);

/* === L6: Bow-Tie Analysis === */

typedef struct {
    Hazard *threat;
    SafetyBarrier *preventive_barriers;
    int n_preventive;
    SafetyBarrier *mitigative_barriers;
    int n_mitigative;
    char top_event[128];
    char consequence[256];
} BowTieDiagram;

int bowtie_analyze(const BowTieDiagram *bt, RiskLevel *out_risk);

/* === L6: Safety Lifecycle (IEC 61511) === */

void sil_pfd_single(double pfd_sensor, double pfd_logic,
                      double pfd_actuator, double *out_pfd,
                      SIL *out_sil);

#endif /* SAFETY_ANALYSIS_H */
