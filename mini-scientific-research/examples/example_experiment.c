#include "sci_research.h"
#include "sci_statistics.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Example: End-to-end research experiment workflow.
 * Design an experiment, collect data, analyze results.
 */

int main(void) {
    printf("=== Example: Scientific Experiment Workflow ===\n\n");

    sci_research_init();

    /* Create experiment */
    int exp_id = sci_experiment_create(
        "Effect of Intervention on Cognitive Performance",
        "H1: Intervention group scores higher than control",
        SCI_STUDY_EXPERIMENTAL);
    printf("Created experiment #%d\n", exp_id);

    /* Set stages */
    sci_experiment_set_stage(exp_id, SCI_EXP_RUNNING);
    printf("Stage: RUNNING\n");

    /* Power analysis */
    int n_per_group = sci_power_sample_size_ttest(0.5, 0.05, 0.80);
    printf("Required sample size per group: %d (d=0.5, alpha=0.05, power=0.80)\n",
           n_per_group);

    /* Simulate data: control group (mean=100, sd=15), intervention (mean=110, sd=15) */
    int n = 30;
    double control[30], intervention[30];
    printf("\nSimulated data:\n");
    printf("Control:     ");
    for (int i = 0; i < n; i++) {
        control[i] = 100.0 + 15.0 * ((double)rand()/RAND_MAX * 2.0 - 1.0);
        printf("%.1f ", control[i]);
    }
    printf("\nIntervention: ");
    for (int i = 0; i < n; i++) {
        intervention[i] = 110.0 + 15.0 * ((double)rand()/RAND_MAX * 2.0 - 1.0);
        printf("%.1f ", intervention[i]);
    }
    printf("\n");

    /* t-test */
    SCI_TestResult test = sci_ttest_2sample(
        control, n, intervention, n, 0.05, SCI_TEST_TWO_TAILED);

    printf("\nIndependent t-test results:\n");
    printf("  t-statistic: %.4f\n", test.statistic);
    printf("  df:          %d (Welch-Satterthwaite)\n", test.df);
    printf("  p-value:     %.6f\n", test.p_value);
    printf("  Effect size: %.3f (Cohen d)\n", test.effect_size);
    printf("  Reject H0:   %s\n", test.reject_h0 ? "YES" : "NO");

    /* Effect size manually */
    SCI_SummaryStats s1 = sci_compute_summary(control, n);
    SCI_SummaryStats s2 = sci_compute_summary(intervention, n);
    double cohens_d = sci_cohens_d(s2.mean, s1.mean, s1.variance,
                                    s2.variance, n, n);
    printf("  Manual d:    %.3f\n", cohens_d);

    /* Finalize experiment */
    sci_experiment_set_stage(exp_id, SCI_EXP_COMPLETED);
    sci_experiment_set_stage(exp_id, SCI_EXP_ANALYZING);
    sci_experiment_set_stage(exp_id, SCI_EXP_WRITTEN);
    printf("\nExperiment stage: WRITTEN\n");

    sci_research_shutdown();
    printf("\n=== Done ===\n");
    return 0;
}
