#ifndef MINI_FUTURE_BIO_COMPUTE_H
#define MINI_FUTURE_BIO_COMPUTE_H

#include <stdbool.h>
#include <stdint.h>

/* Biological / DNA Computing
 * L1: DNAStrand, DNALogicGate, DNACircuit, BioProcessor
 * L2: Watson-Crick base pairing (A-T, C-G), strand displacement
 * L3: DNA tile assembly, toehold-mediated strand displacement
 * L4: Adleman Hamiltonian path experiment (Science 1994)
 * L5: DNA logic gates, seesaw circuit
 * L6: DNA-based SAT solver, NP-complete problem mapping
 * L7: DNA data storage, molecular diagnostics
 * L8: DNA strand displacement cascades, enzyme-free amplification
 * L9: Catalog Technologies, CRISPR-based computing
 */

#define BIO_MAX_STRANDS       1024
#define BIO_MAX_GATES         512
#define BIO_MAX_CIRCUITS      64
#define BIO_SEQUENCE_LEN      256
#define BIO_NAME_LEN          64

typedef enum {
    BIO_BASE_A = 0,
    BIO_BASE_T = 1,
    BIO_BASE_C = 2,
    BIO_BASE_G = 3
} BioBase;

typedef struct {
    int id;
    char name[BIO_NAME_LEN];
    BioBase sequence[BIO_SEQUENCE_LEN];
    int length;
    double concentration;
    bool is_output;
    bool is_input;
    double melting_temp;
} DNAStrand;

typedef enum {
    BIO_GATE_AND       = 0,
    BIO_GATE_OR        = 1,
    BIO_GATE_NOT       = 2,
    BIO_GATE_NAND      = 3,
    BIO_GATE_NOR       = 4,
    BIO_GATE_XOR       = 5,
    BIO_GATE_SEESAW    = 6,
    BIO_GATE_THRESHOLD = 7,
    BIO_GATE_CATALYTIC = 8
} BioGateType;

typedef struct {
    int id;
    BioGateType type;
    int input_strand_ids[4];
    int num_inputs;
    int output_strand_id;
    double rate_constant;
    double threshold;
    bool has_fuel_strand;
    int fuel_strand_id;
} DNALogicGate;

typedef struct {
    DNAStrand strands[BIO_MAX_STRANDS];
    int num_strands;
    DNALogicGate gates[BIO_MAX_GATES];
    int num_gates;
    double temperature_celsius;
    double total_volume_uL;
} DNACircuit;

typedef struct {
    DNACircuit circuits[BIO_MAX_CIRCUITS];
    int num_circuits;
    int active_circuit;
} BioProcessor;

DNAStrand bio_strand_create(const char *name, const char *sequence);
void bio_strand_set_sequence(DNAStrand *strand, const char *sequence);
int  bio_strand_to_string(const DNAStrand *strand, char *buf, int bufsize);
DNAStrand bio_complement(const DNAStrand *strand);
double bio_melting_temperature(const DNAStrand *strand);
double bio_gc_content(const DNAStrand *strand);
bool bio_are_complementary(const DNAStrand *a, const DNAStrand *b);

DNACircuit* bio_circuit_create(double temperature_c);
void bio_circuit_destroy(DNACircuit *circuit);
int  bio_circuit_add_strand(DNACircuit *circuit, const char *name,
                             const char *sequence, double concentration);
int  bio_circuit_find_strand(const DNACircuit *circuit, const char *name);

int  bio_circuit_add_gate(DNACircuit *circuit, BioGateType type,
                           const int *input_ids, int num_inputs,
                           int output_id);
int  bio_add_seesaw_gate(DNACircuit *circuit, int input_id, int output_id,
                          double rate);
int  bio_add_threshold_gate(DNACircuit *circuit, int input_id, int output_id,
                             double threshold);

int  bio_circuit_simulate(DNACircuit *circuit, double total_time_s,
                           double dt_s);
int  bio_circuit_read_outputs(const DNACircuit *circuit, double *outputs,
                               int max_outputs);

int  bio_eval_and(DNACircuit *circuit, int in_a_id, int in_b_id,
                   int out_id);
int  bio_eval_or(DNACircuit *circuit, int in_a_id, int in_b_id,
                  int out_id);
int  bio_eval_not(DNACircuit *circuit, int in_id, int out_id);

int  bio_hamiltonian_path_setup(DNACircuit *circuit,
                                 const int *graph, int num_vertices,
                                 int **path_found, int *path_len);
bool bio_verify_hamiltonian_path(const DNAStrand *path_strand,
                                  int vertices, const char **vertex_seqs);

void bio_encode_binary(const uint8_t *data, int data_len,
                        char *dna_sequence, int max_seq_len);
int  bio_decode_binary(const char *dna_sequence,
                        uint8_t *data, int max_data_len);

int  bio_needleman_wunsch(const DNAStrand *a, const DNAStrand *b,
                           int *score, char *alignment_a, char *alignment_b,
                           int max_align_len);

void bio_print_circuit(const DNACircuit *circuit);
double bio_compute_diversity(const DNACircuit *circuit);
BioBase bio_complement_base(BioBase base);
const char* bio_gate_name(BioGateType type);

#endif
