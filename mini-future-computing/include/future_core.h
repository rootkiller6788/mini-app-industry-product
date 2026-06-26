#ifndef MINI_FUTURE_CORE_H
#define MINI_FUTURE_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* Future Core -- Emerging Computing Paradigm Framework
 * L1: FUTURE_ParadigmType, FUTURE_ParadigmDesc, FUTURE_ParadigmRegistry
 * L2: Post-Moore computing, beyond-von-Neumann, energy-aware computation
 * L3: Paradigm registry, cross-paradigm comparator
 * L4: Landauer limit (kT*ln2 = 3e-21 J/bit at 300K), Koomey law
 * L5: Cross-paradigm workload analysis
 * L6: Paradigm fitness scoring, energy efficiency benchmarking
 * L7: Heterogeneous computing roadmap, future data center
 * L8: Reversible computing, adiabatic CMOS
 * L9: IBM roadmap, Intel CMOS extension, TSMC 3D IC
 * References:
 * - Moore, "Cramming more components onto integrated circuits" (1965)
 * - Landauer, "Irreversibility and heat generation" (IBM J. 1961)
 * - Bennett, "Logical reversibility of computation" (IBM J. 1973)
 * - Waldrop, "The chips are down for Moore's law" (Nature 2016)
 */

#define FUTURE_MAX_PARADIGMS      32
#define FUTURE_MAX_BENCHMARKS     64
#define FUTURE_MAX_RESOURCES      16
#define FUTURE_NAME_LEN           64

typedef enum {
    FUTURE_PARA_QUANTUM           = 0,
    FUTURE_PARA_QUANTUM_ANNEAL    = 1,
    FUTURE_PARA_NEUROMORPHIC      = 2,
    FUTURE_PARA_OPTICAL           = 3,
    FUTURE_PARA_PROBABILISTIC     = 4,
    FUTURE_PARA_THERMODYNAMIC     = 5,
    FUTURE_PARA_BIO_DNA           = 6,
    FUTURE_PARA_MEMRISTOR         = 7,
    FUTURE_PARA_CARBON_NANOTUBE   = 8,
    FUTURE_PARA_QUANTUM_DOT       = 9,
    FUTURE_PARA_SPINTRONIC        = 10,
    FUTURE_PARA_CRYOGENIC         = 11,
    FUTURE_PARA_RESERVOIR         = 12,
    FUTURE_PARA_HYPERDIMENSIONAL  = 13,
    FUTURE_PARA_ANALOG            = 14,
    FUTURE_PARA_COUNT             = 15
} FUTURE_ParadigmType;

typedef enum {
    FUTURE_TRL_CONCEPT        = 1,
    FUTURE_TRL_THEORY         = 2,
    FUTURE_TRL_PROOF          = 3,
    FUTURE_TRL_LAB            = 4,
    FUTURE_TRL_SIMULATED      = 5,
    FUTURE_TRL_DEMO           = 6,
    FUTURE_TRL_PROTOTYPE      = 7,
    FUTURE_TRL_QUALIFIED      = 8,
    FUTURE_TRL_DEPLOYED       = 9
} FUTURE_TRLevel;

typedef enum {
    FUTURE_RES_COMPUTE   = 0,
    FUTURE_RES_MEMORY    = 1,
    FUTURE_RES_BANDWIDTH = 2,
    FUTURE_RES_ENERGY    = 3,
    FUTURE_RES_AREA      = 4,
    FUTURE_RES_LATENCY   = 5,
    FUTURE_RES_ACCURACY  = 6
} FUTURE_ResourceType;

typedef enum {
    FUTURE_WORKLOAD_MATRIX_MUL    = 0,
    FUTURE_WORKLOAD_SEARCH        = 1,
    FUTURE_WORKLOAD_OPTIMIZATION  = 2,
    FUTURE_WORKLOAD_SAMPLING      = 3,
    FUTURE_WORKLOAD_PATTERN_MATCH = 4,
    FUTURE_WORKLOAD_SIMULATION    = 5,
    FUTURE_WORKLOAD_INFERENCE     = 6,
    FUTURE_WORKLOAD_FACTORING     = 7
} FUTURE_WorkloadType;

typedef struct {
    int                 id;
    FUTURE_ParadigmType type;
    char                name[FUTURE_NAME_LEN];
    FUTURE_TRLevel      trl;
    int                 year_introduced;
    bool                is_reversible;
    bool                requires_cryogenics;
    bool                is_probabilistic;
    double              ops_per_second;
    double              ops_per_joule;
    double              qubit_equivalent;
    double              maturity_score;
    char                primary_challenge[FUTURE_NAME_LEN];
    char                key_advantage[FUTURE_NAME_LEN];
} FUTURE_ParadigmDesc;

typedef struct {
    char                name[FUTURE_NAME_LEN];
    FUTURE_ParadigmType paradigm;
    double              score;
    double              energy_joules;
    double              time_seconds;
    double              accuracy;
    int                 num_qubits_or_neurons;
    time_t              timestamp;
} FUTURE_BenchmarkResult;

typedef struct {
    FUTURE_ParadigmDesc paradigms[FUTURE_MAX_PARADIGMS];
    int                 num_paradigms;
    FUTURE_BenchmarkResult benchmarks[FUTURE_MAX_BENCHMARKS];
    int                 num_benchmarks;
} FUTURE_ParadigmRegistry;

typedef struct {
    double resources[16];
    char   units[16][16];
} FUTURE_ResourceBudget;

/* API */
FUTURE_ParadigmRegistry* future_registry_create(void);
void future_registry_destroy(FUTURE_ParadigmRegistry *reg);

int  future_register_paradigm(FUTURE_ParadigmRegistry *reg,
                               FUTURE_ParadigmType type,
                               const char *name, FUTURE_TRLevel trl,
                               int year, double ops_per_sec,
                               double ops_per_joule);

int  future_find_paradigm(const FUTURE_ParadigmRegistry *reg,
                           FUTURE_ParadigmType type);

const FUTURE_ParadigmDesc* future_get_paradigm(
    const FUTURE_ParadigmRegistry *reg, int idx);

int  future_run_benchmark(FUTURE_ParadigmRegistry *reg,
                           FUTURE_ParadigmType paradigm,
                           const char *name, double score,
                           double energy, double time_sec,
                           double accuracy);

int  future_rank_paradigms(const FUTURE_ParadigmRegistry *reg,
                            FUTURE_ResourceType metric,
                            int *ranked_indices, int max_results);

int  future_compare_paradigms(const FUTURE_ParadigmRegistry *reg,
                               int idx_a, int idx_b,
                               double *comparison, int max_metrics);

/* Landauer limit: E_min = kT * ln(2) */
double future_landauer_limit(double temperature_kelvin);
double future_min_erase_energy(int num_bits, double temperature_kelvin);
double future_max_ops_per_joule_irreversible(double temperature_kelvin);
double future_reversible_efficiency(double reversibility_factor,
                                     double ops_count, double temp_k);

/* Moore's Law: N(t) = N0 * 2^((t - t0) / 2) */
double future_moore_projection(int base_year, long base_count,
                                int target_year);
int  future_moore_crossing_year(int base_year, long base_count,
                                 long target_count);

/* Koomey's Law: computations per joule doubles every ~1.57 years */
double future_koomey_projection(int base_year, double base_ops_per_joule,
                                 int target_year);

/* Cross-paradigm workload analysis */
FUTURE_ParadigmType future_best_paradigm_for_workload(
    FUTURE_WorkloadType workload);
double future_paradigm_workload_fit(FUTURE_ParadigmType paradigm,
                                     FUTURE_WorkloadType workload);

const char* future_paradigm_name(FUTURE_ParadigmType type);
const char* future_trl_name(FUTURE_TRLevel level);
const char* future_workload_name(FUTURE_WorkloadType wl);
const char* future_resource_name(FUTURE_ResourceType res);
void future_print_registry(const FUTURE_ParadigmRegistry *reg);

#endif
