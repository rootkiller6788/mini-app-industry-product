#ifndef MINI_SCI_RESEARCH_H
#define MINI_SCI_RESEARCH_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

/* ================================================================
 *  sci_research -- Scientific Research Workflow & Data Management
 *
 *  L1: Core types -- experiment, dataset, observation, pipeline stage
 *  L2: Core concepts -- scientific method (Popperian falsifiability),
 *      reproducibility, provenance tracking, peer review, meta-analysis
 *  L3: Engineering structures -- experiment lifecycle state machine,
 *      data provenance DAG, versioned dataset management
 *  L4: Standards/Theorems -- FAIR data principles (Wilkinson 2016),
 *      ARRIVE guidelines, CONSORT statement, GDPR for research data,
 *      CITI Program ethics standards
 *  L5: Algorithms -- experiment randomization (Fisher),
 *      power analysis, effect size computation (Cohen d, eta^2),
 *      reproducibility checks, data lineage tracking
 *  L6: Canonical problems -- Clinical trial design, survey analysis,
 *      reproducibility crisis remediation, meta-analysis
 *  L7: Applications -- Peer-reviewed paper workflow, lab notebook
 *      digitization, research data management platforms
 *  L8: Advanced -- Registered reports, preregistration,
 *      computational reproducibility (Docker/containerization),
 *      open science frameworks
 *  L9: Industry -- OSF (Open Science Framework), Dryad, Figshare,
 *      Zenodo, Jupyter Notebooks, R Markdown
 *
 *  References:
 *  - Popper, "The Logic of Scientific Discovery" (1934)
 *  - Kuhn, "The Structure of Scientific Revolutions" (1962)
 *  - Wilkinson et al., "FAIR Guiding Principles" (2016)
 *  - Ioannidis, "Why Most Published Research Findings Are False" (2005)
 *  - NPOS, "National Plan for Open Science" (multiple countries)
 * ================================================================ */

/* -----------------------------------------------------------------
 * L1: Core Type Definitions
 * ----------------------------------------------------------------- */

typedef enum {
    SCI_EXP_DRAFT       = 0,   /* Experiment being designed */
    SCI_EXP_PREREGISTERED = 1, /* Preregistered (e.g., OSF) */
    SCI_EXP_RUNNING     = 2,   /* Data collection in progress */
    SCI_EXP_COMPLETED   = 3,   /* Data collection complete */
    SCI_EXP_ANALYZING   = 4,   /* Analysis in progress */
    SCI_EXP_WRITTEN     = 5,   /* Manuscript draft complete */
    SCI_EXP_SUBMITTED   = 6,   /* Under peer review */
    SCI_EXP_REVISED     = 7,   /* After revision */
    SCI_EXP_PUBLISHED   = 8,   /* Published / preprinted */
    SCI_EXP_RETRACTED   = 9,   /* Retracted / withdrawn */
} SCI_ExperimentStage;

typedef enum {
    SCI_STUDY_OBSERVATIONAL = 0,
    SCI_STUDY_EXPERIMENTAL  = 1,
    SCI_STUDY_QUASI_EXP     = 2,
    SCI_STUDY_META_ANALYSIS = 3,
    SCI_STUDY_SIMULATION    = 4,
    SCI_STUDY_SURVEY        = 5,
    SCI_STUDY_CASE_CONTROL  = 6,
    SCI_STUDY_COHORT        = 7,
    SCI_STUDY_LONGITUDINAL  = 8,
} SCI_StudyDesign;

typedef enum {
    SCI_METHOD_QUANTITATIVE = 0,
    SCI_METHOD_QUALITATIVE  = 1,
    SCI_METHOD_MIXED        = 2,
    SCI_METHOD_COMPUTATIONAL = 3,
} SCI_ResearchMethod;

typedef enum {
    SCI_RANDOM_SIMPLE       = 0,  /* Simple random sampling */
    SCI_RANDOM_BLOCKED      = 1,  /* Blocked randomization */
    SCI_RANDOM_STRATIFIED   = 2,  /* Stratified randomization */
    SCI_RANDOM_CLUSTER      = 3,  /* Cluster randomization */
    SCI_RANDOM_LATIN_SQUARE = 4,  /* Latin square design */
    SCI_RANDOM_MINIMIZATION = 5,  /* Minimization (biased coin) */
} SCI_RandomMethod;

typedef struct {
    int     id;
    char    title[256];
    char    authors[512];
    char    hypothesis[512];       /* Null and alternative hypotheses */
    char    design_notes[1024];    /* Study design description */
    SCI_StudyDesign    design;
    SCI_ResearchMethod method;
    SCI_RandomMethod   randomization;
    SCI_ExperimentStage stage;
    int     sample_size_planned;
    int     sample_size_actual;
    double  alpha;                 /* Significance level */
    double  target_power;          /* Statistical power target */
    int     num_groups;            /* 1 = single group, 2+ = multiple */
    char    outcome_measures[1024];
    time_t  created_at;
    time_t  updated_at;
    time_t  preregistered_at;
    char    preregistration_doi[128];
    char    data_doi[128];
    char    analysis_script[128];  /* Path/URL to analysis code */
    bool    is_reproducible;
    bool    is_blinded;
    bool    is_preregistered;
} SCI_Experiment;

typedef struct {
    int     version_id;
    int     experiment_id;
    int     version_number;
    time_t  timestamp;
    char    description[256];
    char    checksum_md5[33];      /* Data integrity */
    char    file_path[256];
    int     num_observations;
    int     num_variables;
    char    variable_names[64][64]; /* Up to 64 variables */
} SCI_DatasetVersion;

typedef struct {
    int     id;
    double  value;
    int     group_id;
    int     time_point;
    double  measurement_error;
    bool    is_outlier;            /* Flagged by outlier detection */
    bool    is_missing;            /* TRUE if value is missing/NA */
} SCI_Observation;

typedef struct {
    int     obs_count;
    int     group_count;
    double  effect_size;
    double  power;
    double  minimum_detectable_effect;
} SCI_PowerAnalysis;

typedef struct {
    int      experiment_id;
    time_t   start_time;
    time_t   end_time;
    int      total_observations;
    int      successful_observations;
    int      missing_data;
    int      excluded_observations;
    double   completion_rate;
    char     notes[512];
    bool     data_quality_passed;
} SCI_DataCollectionReport;

/* -----------------------------------------------------------------
 * L2-L3: Experiment Lifecycle
 * ----------------------------------------------------------------- */

/**
 * Create a new experiment record.
 * Initializes with DRAFT stage, current timestamp.
 * Complexity: O(1).
 *
 * @param title       Experiment title
 * @param hypothesis  Research hypothesis
 * @param design      Study design type
 * @return            Experiment ID (>=1), or -1 on error
 */
int sci_experiment_create(const char *title, const char *hypothesis,
                           SCI_StudyDesign design);

/**
 * Transition experiment to next stage in accordance with
 * scientific workflow state machine.
 * Valid transitions: DRAFT->PREREGISTERED->RUNNING->COMPLETED->
 *   ANALYZING->WRITTEN->SUBMITTED->(REVISED)/PUBLISHED
 *
 * @param exp_id       Experiment ID
 * @param new_stage    Target stage
 * @return             true if transition is valid and successful
 */
bool sci_experiment_set_stage(int exp_id, SCI_ExperimentStage new_stage);

/**
 * Retrieve experiment by ID.
 *
 * @param exp_id  Experiment ID
 * @param exp     Output experiment (caller-allocated)
 * @return        true if found
 */
bool sci_experiment_get(int exp_id, SCI_Experiment *exp);

/**
 * Count experiments by stage. Useful for research dashboard.
 *
 * @param stage  Stage to count (-1 for all)
 * @return       Number of matching experiments
 */
int sci_experiment_count(SCI_ExperimentStage stage);

/* -----------------------------------------------------------------
 * L5: Randomization & Experimental Design
 * ----------------------------------------------------------------- */

/**
 * Fisher randomization: assign n subjects to k groups.
 * Uses Fisher-Yates shuffle on assignment indices.
 *
 * Fisher, "The Design of Experiments" (1935):
 * Randomization is the foundation of valid statistical inference.
 *
 * @param n        Number of subjects
 * @param k        Number of groups
 * @param seed     Random seed
 * @param groups   Output: group assignment [0..k-1] for each subject
 */
void sci_randomize_subjects(int n, int k, unsigned int seed, int *groups);

/**
 * Blocked randomization: ensure balanced groups within each block.
 * Each block of size (k * block_multiplier) is randomized independently.
 *
 * @param n                 Number of subjects
 * @param k                 Number of groups
 * @param block_size        Number per group per block
 * @param seed              Random seed
 * @param groups            Output assignments
 */
void sci_randomize_blocked(int n, int k, int block_size,
                            unsigned int seed, int *groups);

/**
 * Stratified randomization: subjects are stratified by covariate,
 * then randomized within each stratum.
 *
 * @param n        Number of subjects
 * @param strata   Stratum for each subject (values 0..S-1)
 * @param S        Number of strata
 * @param k        Number of groups per stratum
 * @param seed     Random seed
 * @param groups   Output assignments [0..k-1] for each subject
 */
void sci_randomize_stratified(int n, const int *strata, int S, int k,
                               unsigned int seed, int *groups);

/* -----------------------------------------------------------------
 * L5: Power Analysis & Sample Size
 * ----------------------------------------------------------------- */

/**
 * Statistical power analysis for two-group comparison (t-test).
 *
 * Power = 1 - beta = P(reject H0 | H1 true)
 *
 * For two-sample t-test (equal sizes per group):
 *   n_per_group = 2 * (z_{1-alpha/2} + z_{1-beta})^2 * sigma^2 / delta^2
 *
 * where delta = mu1 - mu2 is the effect size.
 *
 * Reference: Cohen (1988), "Statistical Power Analysis for the
 * Behavioral Sciences" (2nd ed).
 *
 * @param effect_size  Cohen d = delta / sigma
 * @param alpha        Significance level
 * @param power        Desired power (0.8 typical)
 * @return             Required sample size per group
 */
int sci_power_sample_size_ttest(double effect_size, double alpha,
                                 double power);

/**
 * Compute achieved power given sample size, effect size, alpha.
 *
 * @param n_per_group  Sample size per group
 * @param effect_size  Cohen d
 * @param alpha        Significance level
 * @return             Achieved power in (0, 1)
 */
double sci_power_achieved(int n_per_group, double effect_size, double alpha);

/**
 * Minimum detectable effect size given sample size and desired power.
 *
 * MDE = (z_{1-alpha/2} + z_{1-beta}) * sqrt(2 * sigma^2 / n_per_group)
 *
 * @param n_per_group  Sample size per group
 * @param alpha        Significance level
 * @param power        Desired power
 * @return             Minimum detectable Cohen d
 */
double sci_min_detectable_effect(int n_per_group, double alpha, double power);

/* -----------------------------------------------------------------
 * L5: Effect Size Computation
 * ----------------------------------------------------------------- */

/**
 * Cohen's d for independent groups.
 *
 * d = (mean1 - mean2) / s_pooled
 * s_pooled = sqrt(((n1-1)*var1 + (n2-1)*var2) / (n1+n2-2))
 *
 * Interpretation: small=0.2, medium=0.5, large=0.8 (Cohen 1988).
 *
 * @param mean1, mean2  Group means
 * @param var1, var2    Group variances
 * @param n1, n2        Group sizes
 * @return              Cohen's d
 */
double sci_cohens_d(double mean1, double mean2,
                     double var1, double var2, int n1, int n2);

/**
 * Eta-squared: variance explained in ANOVA.
 * eta^2 = SS_between / SS_total
 *
 * Small=0.01, medium=0.06, large=0.14.
 */
double sci_eta_squared(double ss_between, double ss_total);

/**
 * Odds ratio for 2x2 contingency tables.
 *
 * OR = (a*d) / (b*c) where a=TP, d=TN, b=FP, c=FN
 *
 * Table:   Outcome+  Outcome-
 * Group A:    a         b
 * Group B:    c         d
 *
 * @param a, b, c, d  Cell counts
 * @return            Odds ratio (>=0, inf if b*c=0)
 */
double sci_odds_ratio(int a, int b, int c, int d);

/* -----------------------------------------------------------------
 * L6: Data Quality & Reproducibility
 * ----------------------------------------------------------------- */

/**
 * Outlier detection using modified Z-score (Iglewicz & Hoaglin 1993).
 *
 * M_i = 0.6745 * (x_i - median) / MAD
 * where MAD = median(|x_i - median|) is the Median Absolute Deviation.
 *
 * Recommended threshold: |M_i| > 3.5 (Iglewicz & Hoaglin).
 *
 * @param data      Data array
 * @param n         Number of observations
 * @param threshold Modified Z-score threshold (3.5 typical)
 * @param is_outlier Output: true if outlier for each observation
 */
void sci_outlier_detection(const double *data, int n, double threshold,
                            bool *is_outlier);

/**
 * Compute reproducibility check: verify that analysis yields same
 * results from original dataset and independent reanalysis.
 *
 * Compares two sets of results using equivalence testing (TOST).
 *
 * @param results_a  First analysis results (size n)
 * @param results_b  Second analysis results (size n)
 * @param n          Number of statistics compared
 * @param tol        Equivalence tolerance (relative)
 * @return           true if results are equivalent within tolerance
 */
bool sci_reproducibility_check(const double *results_a,
                                const double *results_b,
                                int n, double tol);

/**
 * Compute data quality metrics.
 *
 * @param data          Data array
 * @param n             Size
 * @param completeness  Output: proportion of non-missing values
 * @param uniqueness    Output: proportion of unique values
 * @param z_score_mean  Output: mean absolute z-score (distribution check)
 */
void sci_data_quality_metrics(const double *data, int n,
                               double *completeness,
                               double *uniqueness,
                               double *z_score_mean);

/**
 * Compute intra-class correlation coefficient (ICC) for reliability.
 *
 * ICC(1,1) = (MS_between - MS_within) / (MS_between + (k-1)*MS_within)
 * where k = number of raters/measurements per subject.
 *
 * Used for inter-rater reliability, test-retest reliability.
 * Shrout & Fleiss (1979): "Intraclass Correlations"
 *
 * @param data  Matrix: rows=subjects, cols=raters (row-major)
 * @param n     Number of subjects
 * @param k     Number of raters
 * @return      ICC coefficient in [0,1]
 */
double sci_icc_reliability(const double *data, int n, int k);

/* -----------------------------------------------------------------
 * L7: Research Data Pipeline
 * ----------------------------------------------------------------- */

/**
 * Create a versioned dataset associated with an experiment.
 *
 * @param experiment_id    Parent experiment
 * @param num_observations Number of observations
 * @param num_variables    Number of variables
 * @param var_names        Variable names array
 * @return                 Version ID
 */
int sci_dataset_version_create(int experiment_id, int num_observations,
                                int num_variables, const char var_names[][64]);

/**
 * Add observation to dataset version.
 *
 * @param version_id  Dataset version ID
 * @param obs         Observation data
 * @return            Observation index within version, or -1 on error
 */
int sci_observation_add(int version_id, const SCI_Observation *obs);

/**
 * Generate data collection report summarizing quality and completeness.
 *
 * @param experiment_id  Experiment to report on
 * @param report         Output report (caller-allocated)
 * @return               true if data was available
 */
bool sci_generate_data_report(int experiment_id,
                               SCI_DataCollectionReport *report);

/**
 * Compute meta-analysis summary using fixed-effects model.
 * Combines effect sizes from multiple studies weighted by inverse variance.
 *
 * theta_hat = sum(w_i * theta_i) / sum(w_i) where w_i = 1 / var(theta_i)
 *
 * @param effects    Effect sizes from k studies
 * @param variances  Variance of each effect estimate
 * @param k          Number of studies
 * @param pooled     Output: pooled effect estimate
 * @param pooled_ci  Output: 95% CI half-width
 */
void sci_meta_analysis_fixed(const double *effects, const double *variances,
                              int k, double *pooled, double *pooled_ci);

/**
 * Initialize the research management system.
 * Must be called before any other functions.
 */
void sci_research_init(void);

/**
 * Shutdown and free all research system resources.
 */
void sci_research_shutdown(void);

#endif /* MINI_SCI_RESEARCH_H */