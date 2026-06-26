#include "future_core.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Future Computing Paradigm Comparison ===\n\n");

    FUTURE_ParadigmRegistry *reg = future_registry_create();

    printf("Registered %d paradigms:\n\n", reg->num_paradigms);
    future_print_registry(reg);

    printf("\n=== Workload-to-Paradigm Mapping ===\n");
    for (int w = 0; w <= FUTURE_WORKLOAD_FACTORING; w++) {
        FUTURE_ParadigmType best = future_best_paradigm_for_workload((FUTURE_WorkloadType)w);
        double fit = future_paradigm_workload_fit(best, (FUTURE_WorkloadType)w);
        printf("  %-25s -> %-25s (fit: %.2f)\n",
               future_workload_name((FUTURE_WorkloadType)w),
               future_paradigm_name(best), fit);
    }

    printf("\n=== Energy Efficiency Ranking ===\n");
    int ranked[16];
    int n = future_rank_paradigms(reg, FUTURE_RES_ENERGY, ranked, 16);
    for (int i = 0; i < n; i++) {
        const FUTURE_ParadigmDesc *p = future_get_paradigm(reg, ranked[i]);
        printf("  %2d. %-25s %.2e OPS/Joule  (TRL%d)\n",
               i + 1, p->name, p->ops_per_joule, p->trl);
    }

    printf("\n=== Moore's Law Projection ===\n");
    long t2024 = 20000000L;
    printf("  Starting transistor count: %ld million\n", t2024);
    for (int y = 2030; y <= 2050; y += 5) {
        double proj = future_moore_projection(2024, t2024, y);
        printf("  Projected %d: %.1f million\n", y, proj / 1e6);
    }

    printf("\n=== Landauer Limit Analysis ===\n");
    printf("  Room temp (300K): %.3e J/bit\n", future_landauer_limit(300.0));
    printf("  Liquid N2 (77K):  %.3e J/bit\n", future_landauer_limit(77.0));
    printf("  Liquid He (4K):   %.3e J/bit\n", future_landauer_limit(4.0));
    printf("  Max ops/J at 300K (irrev): %.3e\n",
           future_max_ops_per_joule_irreversible(300.0));

    future_registry_destroy(reg);
    return 0;
}
