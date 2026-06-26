#include "future_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define BOLTZMANN_K 1.380649e-23

/* ================================================================
 *  future_core.c -- Emerging Computing Paradigm Framework
 *  L1: FUTURE_ParadigmRegistry, FUTURE_ParadigmDesc
 *  L2: Post-Moore computing, beyond-von-Neumann
 *  L3: Paradigm registry, cross-paradigm comparator
 *  L4: Landauer limit (kT*ln2), Koomey law
 *  L5: Cross-paradigm workload analysis
 *  References: Moore (1965), Landauer (IBM J. 1961), Bennett (IBM J. 1973)
 * ================================================================ */

const char* future_paradigm_name(FUTURE_ParadigmType type) {
    switch (type) {
        case FUTURE_PARA_QUANTUM:          return "Quantum Computing";
        case FUTURE_PARA_QUANTUM_ANNEAL:   return "Quantum Annealing";
        case FUTURE_PARA_NEUROMORPHIC:     return "Neuromorphic Computing";
        case FUTURE_PARA_OPTICAL:          return "Optical Computing";
        case FUTURE_PARA_PROBABILISTIC:    return "Probabilistic Computing";
        case FUTURE_PARA_THERMODYNAMIC:    return "Thermodynamic/Reversible";
        case FUTURE_PARA_BIO_DNA:          return "DNA/Molecular Computing";
        case FUTURE_PARA_MEMRISTOR:        return "Memristor Computing";
        case FUTURE_PARA_CARBON_NANOTUBE:  return "Carbon Nanotube Computing";
        case FUTURE_PARA_QUANTUM_DOT:      return "Quantum Dot Cellular Automata";
        case FUTURE_PARA_SPINTRONIC:       return "Spintronic Computing";
        case FUTURE_PARA_CRYOGENIC:        return "Cryogenic SFQ Computing";
        case FUTURE_PARA_RESERVOIR:        return "Reservoir Computing";
        case FUTURE_PARA_HYPERDIMENSIONAL: return "Hyperdimensional Computing";
        case FUTURE_PARA_ANALOG:           return "Analog Computing";
        default: return "Unknown";
    }
}

const char* future_trl_name(FUTURE_TRLevel level) {
    switch (level) {
        case FUTURE_TRL_CONCEPT:   return "TRL1-BasicPrinciples";
        case FUTURE_TRL_THEORY:    return "TRL2-ConceptFormulated";
        case FUTURE_TRL_PROOF:     return "TRL3-ProofOfConcept";
        case FUTURE_TRL_LAB:       return "TRL4-LabValidated";
        case FUTURE_TRL_SIMULATED: return "TRL5-EnvironmentValidated";
        case FUTURE_TRL_DEMO:      return "TRL6-Demonstrated";
        case FUTURE_TRL_PROTOTYPE: return "TRL7-Prototype";
        case FUTURE_TRL_QUALIFIED: return "TRL8-Qualified";
        case FUTURE_TRL_DEPLOYED:  return "TRL9-Deployed";
        default: return "Unknown";
    }
}

const char* future_workload_name(FUTURE_WorkloadType wl) {
    switch (wl) {
        case FUTURE_WORKLOAD_MATRIX_MUL:    return "Matrix Multiplication";
        case FUTURE_WORKLOAD_SEARCH:        return "Search/DB Lookup";
        case FUTURE_WORKLOAD_OPTIMIZATION:  return "Combinatorial Optimization";
        case FUTURE_WORKLOAD_SAMPLING:      return "Probabilistic Sampling";
        case FUTURE_WORKLOAD_PATTERN_MATCH: return "Pattern Matching";
        case FUTURE_WORKLOAD_SIMULATION:    return "Physics Simulation";
        case FUTURE_WORKLOAD_INFERENCE:     return "ML Inference";
        case FUTURE_WORKLOAD_FACTORING:     return "Integer Factoring";
        default: return "Unknown";
    }
}

const char* future_resource_name(FUTURE_ResourceType res) {
    switch (res) {
        case FUTURE_RES_COMPUTE:   return "Compute (FLOPs)";
        case FUTURE_RES_MEMORY:    return "Memory (bytes)";
        case FUTURE_RES_BANDWIDTH: return "Bandwidth (B/s)";
        case FUTURE_RES_ENERGY:    return "Energy (Joules)";
        case FUTURE_RES_AREA:      return "Area (mm^2)";
        case FUTURE_RES_LATENCY:   return "Latency (seconds)";
        case FUTURE_RES_ACCURACY:  return "Accuracy (0-1)";
        default: return "Unknown";
    }
}

/* ---- L3: Paradigm Registry ---- */

FUTURE_ParadigmRegistry* future_registry_create(void) {
    FUTURE_ParadigmRegistry *reg = (FUTURE_ParadigmRegistry*)calloc(1, sizeof(FUTURE_ParadigmRegistry));
    if (!reg) return NULL;

    future_register_paradigm(reg, FUTURE_PARA_QUANTUM, "IBM Quantum",
        FUTURE_TRL_SIMULATED, 2016, 1e7, 1e12);
    future_register_paradigm(reg, FUTURE_PARA_QUANTUM_ANNEAL, "D-Wave Advantage",
        FUTURE_TRL_DEPLOYED, 2011, 1e8, 5e10);
    future_register_paradigm(reg, FUTURE_PARA_NEUROMORPHIC, "Intel Loihi 2",
        FUTURE_TRL_PROTOTYPE, 2018, 1e11, 1e14);
    future_register_paradigm(reg, FUTURE_PARA_OPTICAL, "Lightmatter Envise",
        FUTURE_TRL_PROTOTYPE, 2020, 1e15, 1e16);
    future_register_paradigm(reg, FUTURE_PARA_PROBABILISTIC, "p-bit Array",
        FUTURE_TRL_LAB, 2019, 1e9, 1e13);
    future_register_paradigm(reg, FUTURE_PARA_THERMODYNAMIC, "Adiabatic CMOS",
        FUTURE_TRL_LAB, 2005, 1e10, 1e17);
    future_register_paradigm(reg, FUTURE_PARA_BIO_DNA, "DNA Strand Logic",
        FUTURE_TRL_LAB, 1994, 1e4, 1e15);
    future_register_paradigm(reg, FUTURE_PARA_MEMRISTOR, "Memristor Crossbar",
        FUTURE_TRL_LAB, 2008, 5e12, 8e14);
    future_register_paradigm(reg, FUTURE_PARA_CRYOGENIC, "SFQ Logic (MIT-LL)",
        FUTURE_TRL_PROOF, 2010, 1e11, 5e14);
    future_register_paradigm(reg, FUTURE_PARA_RESERVOIR, "Photonic Reservoir",
        FUTURE_TRL_LAB, 2018, 5e11, 5e15);

    return reg;
}

void future_registry_destroy(FUTURE_ParadigmRegistry *reg) {
    free(reg);
}

int future_register_paradigm(FUTURE_ParadigmRegistry *reg,
                              FUTURE_ParadigmType type,
                              const char *name, FUTURE_TRLevel trl,
                              int year, double ops_per_sec,
                              double ops_per_joule) {
    if (!reg || !name) return -1;
    if (reg->num_paradigms >= FUTURE_MAX_PARADIGMS) return -1;

    int idx = reg->num_paradigms++;
    FUTURE_ParadigmDesc *p = &reg->paradigms[idx];
    memset(p, 0, sizeof(FUTURE_ParadigmDesc));
    p->id = idx;
    p->type = type;
    strncpy(p->name, name, FUTURE_NAME_LEN - 1);
    p->trl = trl;
    p->year_introduced = year;
    p->ops_per_second = ops_per_sec;
    p->ops_per_joule = ops_per_joule;

    switch (type) {
        case FUTURE_PARA_QUANTUM:
        case FUTURE_PARA_QUANTUM_ANNEAL:
            p->requires_cryogenics = true;
            p->is_probabilistic = true;
            strncpy(p->primary_challenge, "Decoherence and error correction overhead",
                    FUTURE_NAME_LEN - 1);
            strncpy(p->key_advantage, "Exponential speedup for specific problems",
                    FUTURE_NAME_LEN - 1);
            break;
        case FUTURE_PARA_THERMODYNAMIC:
            p->is_reversible = true;
            strncpy(p->primary_challenge, "Clock distribution and overhead",
                    FUTURE_NAME_LEN - 1);
            strncpy(p->key_advantage, "Near-zero energy per operation",
                    FUTURE_NAME_LEN - 1);
            break;
        case FUTURE_PARA_NEUROMORPHIC:
            strncpy(p->primary_challenge, "Training algorithms and precision",
                    FUTURE_NAME_LEN - 1);
            strncpy(p->key_advantage, "Ultra-low power pattern recognition",
                    FUTURE_NAME_LEN - 1);
            break;
        case FUTURE_PARA_OPTICAL:
            strncpy(p->primary_challenge, "Integration density and nonlinearity",
                    FUTURE_NAME_LEN - 1);
            strncpy(p->key_advantage, "Extreme bandwidth and low latency",
                    FUTURE_NAME_LEN - 1);
            break;
        default:
            strncpy(p->primary_challenge, "Technology maturity",
                    FUTURE_NAME_LEN - 1);
            strncpy(p->key_advantage, "Novel computation model",
                    FUTURE_NAME_LEN - 1);
            break;
    }

    p->maturity_score = 0.1 * (double)trl;
    if (year > 2015) p->maturity_score += 0.05;
    if (year > 2020) p->maturity_score += 0.05;
    if (p->maturity_score > 1.0) p->maturity_score = 1.0;

    return idx;
}

int future_find_paradigm(const FUTURE_ParadigmRegistry *reg,
                          FUTURE_ParadigmType type) {
    if (!reg) return -1;
    for (int i = 0; i < reg->num_paradigms; i++) {
        if (reg->paradigms[i].type == type) return i;
    }
    return -1;
}

const FUTURE_ParadigmDesc* future_get_paradigm(
    const FUTURE_ParadigmRegistry *reg, int idx) {
    if (!reg || idx < 0 || idx >= reg->num_paradigms) return NULL;
    return &reg->paradigms[idx];
}

/* ---- L3: Benchmarking ---- */

int future_run_benchmark(FUTURE_ParadigmRegistry *reg,
                          FUTURE_ParadigmType paradigm,
                          const char *name, double score,
                          double energy, double time_sec,
                          double accuracy) {
    if (!reg || !name || reg->num_benchmarks >= FUTURE_MAX_BENCHMARKS)
        return -1;
    int idx = reg->num_benchmarks++;
    FUTURE_BenchmarkResult *bm = &reg->benchmarks[idx];
    strncpy(bm->name, name, FUTURE_NAME_LEN - 1);
    bm->paradigm = paradigm;
    bm->score = score;
    bm->energy_joules = energy;
    bm->time_seconds = time_sec;
    bm->accuracy = accuracy;
    bm->timestamp = time(NULL);
    return idx;
}

int future_rank_paradigms(const FUTURE_ParadigmRegistry *reg,
                           FUTURE_ResourceType metric,
                           int *ranked_indices, int max_results) {
    if (!reg || !ranked_indices) return -1;
    double scores[FUTURE_MAX_PARADIGMS];
    int indices[FUTURE_MAX_PARADIGMS];
    int n = 0;
    for (int i = 0; i < reg->num_paradigms; i++) {
        const FUTURE_ParadigmDesc *p = &reg->paradigms[i];
        switch (metric) {
            case FUTURE_RES_ENERGY:
                scores[n] = p->ops_per_joule; break;
            case FUTURE_RES_COMPUTE:
                scores[n] = p->ops_per_second; break;
            case FUTURE_RES_ACCURACY:
                scores[n] = p->maturity_score; break;
            case FUTURE_RES_LATENCY:
                scores[n] = 1.0 / (1.0 + (1.0 / (p->ops_per_second + 1.0)));
                break;
            default:
                scores[n] = p->maturity_score * p->ops_per_joule; break;
        }
        indices[n] = i;
        n++;
    }
    for (int i = 1; i < n; i++) {
        double key_score = scores[i];
        int key_idx = indices[i];
        int j = i - 1;
        while (j >= 0 && scores[j] < key_score) {
            scores[j + 1] = scores[j];
            indices[j + 1] = indices[j];
            j--;
        }
        scores[j + 1] = key_score;
        indices[j + 1] = key_idx;
    }
    int count = (n < max_results) ? n : max_results;
    for (int i = 0; i < count; i++) ranked_indices[i] = indices[i];
    return count;
}

int future_compare_paradigms(const FUTURE_ParadigmRegistry *reg,
                              int idx_a, int idx_b,
                              double *comparison, int max_metrics) {
    if (!reg || !comparison || idx_a < 0 || idx_a >= reg->num_paradigms ||
        idx_b < 0 || idx_b >= reg->num_paradigms) return -1;
    const FUTURE_ParadigmDesc *a = &reg->paradigms[idx_a];
    const FUTURE_ParadigmDesc *b = &reg->paradigms[idx_b];
    comparison[0] = a->ops_per_second / (b->ops_per_second + 1.0);
    comparison[1] = a->ops_per_joule / (b->ops_per_joule + 1.0);
    comparison[2] = a->maturity_score / (b->maturity_score + 0.001);
    comparison[3] = a->qubit_equivalent / (b->qubit_equivalent + 0.001);
    comparison[4] = a->trl / (double)(b->trl + 1);
    comparison[5] = a->is_reversible ? 10.0 : 0.1;
    return (max_metrics < 6) ? max_metrics : 6;
}

/* ---- L4: Landauer Limit ---- */

double future_landauer_limit(double temperature_kelvin) {
    if (temperature_kelvin <= 0.0) return 0.0;
    return BOLTZMANN_K * temperature_kelvin * log(2.0);
}

double future_min_erase_energy(int num_bits, double temperature_kelvin) {
    if (num_bits < 1) return 0.0;
    return (double)num_bits * future_landauer_limit(temperature_kelvin);
}

double future_max_ops_per_joule_irreversible(double temperature_kelvin) {
    double landauer = future_landauer_limit(temperature_kelvin);
    if (landauer < 1e-30) return 1e30;
    return 1.0 / landauer;
}

double future_reversible_efficiency(double reversibility_factor,
                                     double ops_count, double temp_k) {
    if (reversibility_factor < 0.0) reversibility_factor = 0.0;
    if (reversibility_factor > 1.0) reversibility_factor = 1.0;
    if (ops_count <= 0.0 || temp_k <= 0.0) return 0.0;
    double irreversible_energy = ops_count * future_landauer_limit(temp_k);
    double reversible_energy = irreversible_energy * (1.0 - reversibility_factor);
    return reversible_energy + irreversible_energy * 0.01 * (1.0 - reversibility_factor);
}

/* ---- L2: Moore's Law Projections ---- */

double future_moore_projection(int base_year, long base_count,
                                int target_year) {
    double doubling_years = 2.0;
    double years_passed = (double)(target_year - base_year);
    double doublings = years_passed / doubling_years;
    return (double)base_count * pow(2.0, doublings);
}

int future_moore_crossing_year(int base_year, long base_count,
                                long target_count) {
    if (target_count <= base_count) return base_year;
    double ratio = (double)target_count / (double)base_count;
    double years = 2.0 * log(ratio) / log(2.0);
    return base_year + (int)ceil(years);
}

double future_koomey_projection(int base_year, double base_ops_per_joule,
                                 int target_year) {
    double doubling_years = 1.57;
    double years_passed = (double)(target_year - base_year);
    double doublings = years_passed / doubling_years;
    return base_ops_per_joule * pow(2.0, doublings);
}

/* ---- L5: Cross-Paradigm Workload Analysis ---- */

FUTURE_ParadigmType future_best_paradigm_for_workload(
    FUTURE_WorkloadType workload) {
    switch (workload) {
        case FUTURE_WORKLOAD_MATRIX_MUL:    return FUTURE_PARA_OPTICAL;
        case FUTURE_WORKLOAD_SEARCH:        return FUTURE_PARA_QUANTUM;
        case FUTURE_WORKLOAD_OPTIMIZATION:  return FUTURE_PARA_QUANTUM_ANNEAL;
        case FUTURE_WORKLOAD_SAMPLING:      return FUTURE_PARA_PROBABILISTIC;
        case FUTURE_WORKLOAD_PATTERN_MATCH: return FUTURE_PARA_NEUROMORPHIC;
        case FUTURE_WORKLOAD_SIMULATION:    return FUTURE_PARA_QUANTUM;
        case FUTURE_WORKLOAD_INFERENCE:     return FUTURE_PARA_NEUROMORPHIC;
        case FUTURE_WORKLOAD_FACTORING:     return FUTURE_PARA_QUANTUM;
        default: return FUTURE_PARA_NEUROMORPHIC;
    }
}

double future_paradigm_workload_fit(FUTURE_ParadigmType paradigm,
                                     FUTURE_WorkloadType workload) {
    double fit = 0.15;
    if (paradigm == FUTURE_PARA_QUANTUM) {
        if (workload == FUTURE_WORKLOAD_SEARCH) fit = 0.95;
        else if (workload == FUTURE_WORKLOAD_FACTORING) fit = 0.99;
        else if (workload == FUTURE_WORKLOAD_SIMULATION) fit = 0.90;
        else if (workload == FUTURE_WORKLOAD_OPTIMIZATION) fit = 0.70;
        else fit = 0.30;
    } else if (paradigm == FUTURE_PARA_NEUROMORPHIC) {
        if (workload == FUTURE_WORKLOAD_PATTERN_MATCH) fit = 0.90;
        else if (workload == FUTURE_WORKLOAD_INFERENCE) fit = 0.85;
        else fit = 0.40;
    } else if (paradigm == FUTURE_PARA_OPTICAL) {
        if (workload == FUTURE_WORKLOAD_MATRIX_MUL) fit = 0.95;
        else if (workload == FUTURE_WORKLOAD_INFERENCE) fit = 0.80;
        else fit = 0.35;
    } else if (paradigm == FUTURE_PARA_PROBABILISTIC) {
        if (workload == FUTURE_WORKLOAD_SAMPLING) fit = 0.95;
        else if (workload == FUTURE_WORKLOAD_OPTIMIZATION) fit = 0.70;
        else fit = 0.35;
    } else if (paradigm == FUTURE_PARA_THERMODYNAMIC) {
        fit = 0.50;
    } else if (paradigm == FUTURE_PARA_QUANTUM_ANNEAL) {
        if (workload == FUTURE_WORKLOAD_OPTIMIZATION) fit = 0.92;
        else if (workload == FUTURE_WORKLOAD_SAMPLING) fit = 0.60;
        else fit = 0.25;
    } else if (paradigm == FUTURE_PARA_BIO_DNA) {
        if (workload == FUTURE_WORKLOAD_OPTIMIZATION) fit = 0.55;
        else fit = 0.20;
    } else if (paradigm == FUTURE_PARA_MEMRISTOR) {
        if (workload == FUTURE_WORKLOAD_MATRIX_MUL) fit = 0.85;
        else if (workload == FUTURE_WORKLOAD_INFERENCE) fit = 0.75;
        else fit = 0.35;
    }
    return fit;
}

/* ---- Utility ---- */

void future_print_registry(const FUTURE_ParadigmRegistry *reg) {
    if (!reg) return;
    printf("=== Future Computing Paradigm Registry (%d paradigms) ===\n",
           reg->num_paradigms);
    printf("%-4s %-25s %-6s %-12s %-12s\n",
           "ID", "Name", "TRL", "OPS/sec", "OPS/Joule");
    printf("--------------------------------------------------------------\n");
    for (int i = 0; i < reg->num_paradigms; i++) {
        const FUTURE_ParadigmDesc *p = &reg->paradigms[i];
        printf("%-4d %-25s TRL%-4d %-12.2e %-12.2e\n",
               p->id, p->name, p->trl, p->ops_per_second, p->ops_per_joule);
    }
    printf("---\nLandauer limit at 300K: %.3e J/bit\n",
           future_landauer_limit(300.0));
    printf("Max irreversible ops/Joule at 300K: %.3e\n",
           future_max_ops_per_joule_irreversible(300.0));
}
