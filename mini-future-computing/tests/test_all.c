#include "future_core.h"
#include "quantum_emulator.h"
#include "neuromorphic.h"
#include "optical_compute.h"
#include "probabilistic_compute.h"
#include "thermodynamic_rev.h"
#include "bio_compute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <assert.h>
#include <time.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

static void test_future_registry(void) {
    TEST("Paradigm Registry");
    FUTURE_ParadigmRegistry *reg = future_registry_create();
    CHECK(reg != NULL, "registry null");
    CHECK(reg->num_paradigms >= 8, "too few paradigms");

    TEST("Find Quantum");
    int qi = future_find_paradigm(reg, FUTURE_PARA_QUANTUM);
    CHECK(qi >= 0, "quantum not found");

    TEST("Rank by Energy");
    int ranked[10];
    int n = future_rank_paradigms(reg, FUTURE_RES_ENERGY, ranked, 10);
    CHECK(n > 0, "no results");

    TEST("Compare");
    int qi2 = future_find_paradigm(reg, FUTURE_PARA_OPTICAL);
    double comp[10];
    int cm = future_compare_paradigms(reg, qi, qi2, comp, 10);
    CHECK(cm >= 6, "comparison");

    TEST("Benchmark");
    int bm = future_run_benchmark(reg, FUTURE_PARA_QUANTUM,"Test",0.85,1e-9,0.001,0.99);
    CHECK(bm >= 0, "benchmark");

    future_registry_destroy(reg);
    PASS();
}

static void test_landauer(void) {
    TEST("Landauer 300K");
    double limit = future_landauer_limit(300.0);
    CHECK(limit > 1e-22 && limit < 1e-20, "limit range");
    TEST("Erase Energy");
    double e100 = future_min_erase_energy(100, 300.0);
    CHECK(e100 > limit * 90.0, "erase low");
    TEST("Moore Projection");
    double proj = future_moore_projection(2020, 10000000L, 2030);
    CHECK(proj > 1e7, "Moore low");
    TEST("Koomey Projection");
    double kproj = future_koomey_projection(2020, 1e12, 2030);
    CHECK(kproj > 1e13, "Koomey low");
    TEST("Reversible Efficiency");
    double rev = future_reversible_efficiency(0.9, 1e12, 300.0);
    CHECK(rev > 0.0, "rev zero");
    PASS();
}

static void test_workload(void) {
    TEST("Best Search");
    CHECK(future_best_paradigm_for_workload(FUTURE_WORKLOAD_SEARCH) == FUTURE_PARA_QUANTUM, "search");
    TEST("Best MatMul");
    CHECK(future_best_paradigm_for_workload(FUTURE_WORKLOAD_MATRIX_MUL) == FUTURE_PARA_OPTICAL, "matmul");
    TEST("Workload Fit");
    double fit = future_paradigm_workload_fit(FUTURE_PARA_QUANTUM, FUTURE_WORKLOAD_FACTORING);
    CHECK(fit > 0.9, "fit low");
    PASS();
}

static void test_qstate(void) {
    TEST("State Create");
    QuantumState *st = quantum_state_create(3);
    CHECK(st != NULL, "null");
    double p0 = quantum_state_probability(st, 0);
    CHECK(p0 > 0.99, "prob wrong");
    TEST("Copy");
    QuantumState *cp = quantum_state_copy(st);
    double fid = quantum_state_fidelity(st, cp);
    CHECK(fid > 0.99, "fidelity");
    quantum_state_destroy(cp);
    quantum_state_destroy(st);
    PASS();
}

static void test_qgates(void) {
    TEST("Hadamard");
    QuantumState *st = quantum_state_create(1);
    quantum_apply_gate(st, QGATE_H, 0, 0.0);
    CHECK(fabs(quantum_state_probability(st,0)-0.5)<0.01, "H prob");
    quantum_state_destroy(st);

    TEST("Pauli-X");
    st = quantum_state_create(1);
    quantum_apply_gate(st, QGATE_X, 0, 0.0);
    CHECK(quantum_state_probability(st,1)>0.99, "X|0> not |1>");
    quantum_state_destroy(st);

    TEST("CNOT");
    st = quantum_state_create(2);
    quantum_state_set_basis(st, 1);
    quantum_apply_controlled(st, QGATE_X, 0, 1);
    CHECK(quantum_state_probability(st,3)>0.99, "CNOT wrong");
    quantum_state_destroy(st);

    PASS();
}

static void test_neuro(void) {
    TEST("SNN");
    NeuroSNN *snn = neuro_snn_create(0.1);
    CHECK(snn != NULL, "null");
    neuro_add_neuron(snn, NEURO_MODEL_LIF, -50.0, 10.0);
    neuro_add_neuron(snn, NEURO_MODEL_IZHIKEVICH, -50.0, 10.0);
    neuro_add_synapse(snn, 0, 1, 1.0, 1.0, NEURO_SYN_EXCITATORY);
    double dw = neuro_stdp_weight_change(10.0, 0.01, 0.012, 20.0, 20.0);
    CHECK(dw > 0.0, "STDP potentiation");
    neuro_snn_destroy(snn);
    PASS();
}

static void test_optical(void) {
    TEST("Signal");
    OpticalSignal sig = opt_signal_create(1.0, 0.0, 1550.0);
    CHECK(fabs(opt_signal_intensity(&sig)-1.0)<0.01, "intensity");

    TEST("MZI Transform");
    OpticalMZI mzi = opt_mzi_create(0, 1, M_PI/4.0, 0.0);
    OpticalSignal ia = opt_signal_create(1.0, 0.0, 1550.0);
    OpticalSignal ib = opt_signal_create(0.0, 0.0, 1550.0);
    OpticalSignal oa, ob;
    opt_mzi_transform(&mzi, &ia, &ib, &oa, &ob);
    CHECK(oa.amplitude > 0.0, "MZI output");

    TEST("MatVec");
    double mat[]={1,0,0,1}, x[]={0.6,0.8}, y[2];
    opt_matrix_vector_mul(mat,2,2,x,y);
    CHECK(fabs(y[0]-0.6)<0.01, "matvec");

    TEST("Nonlinear");
    double nl = opt_saturable_absorber(2.0, 1.0);
    CHECK(nl > 0.0 && nl < 2.0, "nonlinear");
    PASS();
}

static void test_prob(void) {
    TEST("Stochastic Bit");
    StochasticBit *bit = prob_bit_create(0.7);
    CHECK(bit != NULL, "null");

    TEST("Bayes Net");
    BayesianNetwork *bn = prob_bn_create();
    prob_bn_add_node(bn, "Rain", 2);
    prob_bn_add_node(bn, "Sprinkler", 2);
    prob_bn_add_edge(bn, 0, 1);
    double cpt[] = {0.2, 0.8};
    prob_bn_set_cpt(bn, 0, cpt, 2);
    double cpt2[] = {0.4, 0.6, 0.01, 0.99};
    prob_bn_set_cpt(bn, 1, cpt2, 4);
    prob_bn_set_evidence(bn, 0, 0);
    prob_bn_belief_propagation(bn, 100);
    double post = prob_bn_posterior(bn, 1, 0);
    CHECK(post > 0.0 && post <= 1.0, "posterior");
    prob_bn_destroy(bn);
    prob_bit_destroy(bit);

    TEST("Gaussian PDF");
    double pdf = prob_gaussian_pdf(0.0, 0.0, 1.0);
    CHECK(pdf > 0.3 && pdf < 0.5, "pdf");

    TEST("Entropy");
    double p[] = {0.5, 0.5};
    double h = prob_entropy(p, 2);
    CHECK(h > 0.9 && h < 1.1, "entropy");
    PASS();
}

static void test_thermo(void) {
    TEST("Circuit");
    ThermoCircuit *tc = thermo_circuit_create(300.0);
    CHECK(tc != NULL, "null");

    TEST("Reversible NOT");
    bool in[]={true}, out[]={false};
    thermo_rev_not(in, out);
    CHECK(out[0] == false, "NOT wrong");

    TEST("Reversible CNOT");
    bool in2[]={true,false}, out2[]={false,false};
    thermo_rev_cnot(in2, out2);
    CHECK(out2[1] == true, "CNOT wrong");

    TEST("Landauer Energy");
    double le = thermo_landauer_energy(1, 300.0);
    CHECK(le > 1e-21 && le < 1e-20, "Landauer");
    thermo_circuit_destroy(tc);
    PASS();
}

static void test_bio(void) {
    TEST("DNA Strand");
    DNAStrand strand = bio_strand_create("Test", "ATCG");
    CHECK(strand.length == 4, "length");

    TEST("Complement");
    DNAStrand comp = bio_complement(&strand);
    char buf[64];
    bio_strand_to_string(&comp, buf, 64);
    CHECK(strcmp(buf, "TAGC") == 0, "complement wrong");

    TEST("GC Content");
    CHECK(fabs(bio_gc_content(&strand)-0.5)<0.01, "GC");

    TEST("Encode Binary");
    uint8_t data[]={0xAB,0xCD};
    char dna[64];
    bio_encode_binary(data,2,dna,64);
    CHECK(strlen(dna)>0, "encode empty");

    TEST("Alignment");
    DNAStrand sa = bio_strand_create("A","AGCT");
    DNAStrand sb = bio_strand_create("B","AGGT");
    int score;
    char aln_a[64], aln_b[64];
    bio_needleman_wunsch(&sa,&sb,&score,aln_a,aln_b,64);
    CHECK(score > 0, "score");
    PASS();
}

int main(void) {
    printf("=== mini-future-computing Test Suite ===\n");
    test_future_registry();
    test_landauer();
    test_workload();
    test_qstate();
    test_qgates();
    test_neuro();
    test_optical();
    test_prob();
    test_thermo();
    test_bio();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
