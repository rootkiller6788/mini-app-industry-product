#include "bio_compute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Biological / DNA Computing
 * L1: DNAStrand, DNALogicGate, DNACircuit
 * L2: Watson-Crick base pairing, strand displacement
 * L3: DNA tile assembly, toehold-mediated strand displacement
 * L4: Adleman Hamiltonian path (Science 1994)
 * L5: DNA logic gates, seesaw circuit
 * L6: NP-complete problem mapping
 * L7: DNA data storage
 * L8: Needleman-Wunsch alignment
 * References: Adleman (1994), Seelig et al. (2006), Qian & Winfree (2011)
 */

/* ---- Base Operations ---- */

BioBase bio_complement_base(BioBase base) {
    switch (base) {
        case BIO_BASE_A: return BIO_BASE_T;
        case BIO_BASE_T: return BIO_BASE_A;
        case BIO_BASE_C: return BIO_BASE_G;
        case BIO_BASE_G: return BIO_BASE_C;
        default: return BIO_BASE_A;
    }
}

static char bio_base_to_char(BioBase base) {
    switch (base) {
        case BIO_BASE_A: return 'A';
        case BIO_BASE_T: return 'T';
        case BIO_BASE_C: return 'C';
        case BIO_BASE_G: return 'G';
        default: return 'N';
    }
}

static BioBase bio_char_to_base(char c) {
    switch (c) {
        case 'A': case 'a': return BIO_BASE_A;
        case 'T': case 't': return BIO_BASE_T;
        case 'C': case 'c': return BIO_BASE_C;
        case 'G': case 'g': return BIO_BASE_G;
        default: return BIO_BASE_A;
    }
}

DNAStrand bio_strand_create(const char *name, const char *sequence) {
    DNAStrand strand;
    memset(&strand, 0, sizeof(DNAStrand));
    if (name) strncpy(strand.name, name, BIO_NAME_LEN - 1);
    if (sequence) bio_strand_set_sequence(&strand, sequence);
    strand.concentration = 1.0;
    return strand;
}

void bio_strand_set_sequence(DNAStrand *strand, const char *sequence) {
    if (!strand || !sequence) return;
    int len = (int)strlen(sequence);
    if (len > BIO_SEQUENCE_LEN) len = BIO_SEQUENCE_LEN;
    strand->length = len;
    for (int i = 0; i < len; i++) {
        strand->sequence[i] = bio_char_to_base(sequence[i]);
    }
    strand->melting_temp = bio_melting_temperature(strand);
}

int bio_strand_to_string(const DNAStrand *strand, char *buf, int bufsize) {
    if (!strand || !buf || bufsize < 2) return -1;
    int len = (strand->length < bufsize - 1) ? strand->length : bufsize - 1;
    for (int i = 0; i < len; i++) buf[i] = bio_base_to_char(strand->sequence[i]);
    buf[len] = '\0';
    return len;
}

DNAStrand bio_complement(const DNAStrand *strand) {
    DNAStrand comp;
    memset(&comp, 0, sizeof(DNAStrand));
    if (!strand) return comp;
    comp.length = strand->length;
    for (int i = 0; i < strand->length; i++) {
        comp.sequence[i] = bio_complement_base(strand->sequence[i]);
    }
    snprintf(comp.name, BIO_NAME_LEN, "%s_comp", strand->name);
    comp.concentration = strand->concentration;
    comp.melting_temp = bio_melting_temperature(&comp);
    return comp;
}

double bio_melting_temperature(const DNAStrand *strand) {
    if (!strand || strand->length < 1) return 0.0;
    int gc = 0, at = 0;
    for (int i = 0; i < strand->length; i++) {
        if (strand->sequence[i] == BIO_BASE_G || strand->sequence[i] == BIO_BASE_C)
            gc++;
        else
            at++;
    }
    if (strand->length < 14) return 4.0 * (double)gc + 2.0 * (double)at;
    return 64.9 + 41.0 * ((double)gc - 16.4) / (double)strand->length;
}

double bio_gc_content(const DNAStrand *strand) {
    if (!strand || strand->length < 1) return 0.0;
    int gc = 0;
    for (int i = 0; i < strand->length; i++)
        if (strand->sequence[i] == BIO_BASE_G || strand->sequence[i] == BIO_BASE_C)
            gc++;
    return (double)gc / (double)strand->length;
}

bool bio_are_complementary(const DNAStrand *a, const DNAStrand *b) {
    if (!a || !b || a->length != b->length) return false;
    for (int i = 0; i < a->length; i++)
        if (bio_complement_base(a->sequence[i]) != b->sequence[i]) return false;
    return true;
}

/* ---- DNA Circuit Management ---- */

DNACircuit* bio_circuit_create(double temperature_c) {
    DNACircuit *circuit = (DNACircuit*)calloc(1, sizeof(DNACircuit));
    if (!circuit) return NULL;
    circuit->temperature_celsius = temperature_c;
    circuit->total_volume_uL = 100.0;
    return circuit;
}

void bio_circuit_destroy(DNACircuit *circuit) { free(circuit); }

int bio_circuit_add_strand(DNACircuit *circuit, const char *name,
                            const char *sequence, double concentration) {
    if (!circuit || !name || !sequence) return -1;
    if (circuit->num_strands >= BIO_MAX_STRANDS) return -1;
    int idx = circuit->num_strands++;
    DNAStrand *s = &circuit->strands[idx];
    s->id = idx;
    strncpy(s->name, name, BIO_NAME_LEN - 1);
    bio_strand_set_sequence(s, sequence);
    s->concentration = concentration;
    return idx;
}

int bio_circuit_find_strand(const DNACircuit *circuit, const char *name) {
    if (!circuit || !name) return -1;
    for (int i = 0; i < circuit->num_strands; i++)
        if (strcmp(circuit->strands[i].name, name) == 0) return i;
    return -1;
}

/* ---- DNA Logic Gates (L5) ---- */

int bio_circuit_add_gate(DNACircuit *circuit, BioGateType type,
                          const int *input_ids, int num_inputs,
                          int output_id) {
    if (!circuit || !input_ids || num_inputs < 1) return -1;
    if (circuit->num_gates >= BIO_MAX_GATES) return -1;
    int idx = circuit->num_gates++;
    DNALogicGate *g = &circuit->gates[idx];
    memset(g, 0, sizeof(DNALogicGate));
    g->id = idx;
    g->type = type;
    g->num_inputs = (num_inputs < 4) ? num_inputs : 4;
    for (int i = 0; i < g->num_inputs; i++) g->input_strand_ids[i] = input_ids[i];
    g->output_strand_id = output_id;
    g->rate_constant = 0.01;
    g->threshold = 0.5;
    return idx;
}

int bio_add_seesaw_gate(DNACircuit *circuit, int input_id, int output_id,
                         double rate) {
    if (!circuit) return -1;
    int input_ids[1] = {input_id};
    int gid = bio_circuit_add_gate(circuit, BIO_GATE_SEESAW, input_ids, 1, output_id);
    if (gid >= 0) circuit->gates[gid].rate_constant = rate;
    return gid;
}

int bio_add_threshold_gate(DNACircuit *circuit, int input_id, int output_id,
                            double threshold) {
    if (!circuit) return -1;
    int input_ids[1] = {input_id};
    int gid = bio_circuit_add_gate(circuit, BIO_GATE_THRESHOLD, input_ids, 1, output_id);
    if (gid >= 0) circuit->gates[gid].threshold = threshold;
    return gid;
}

/* ---- L5: Circuit Simulation ---- */

int bio_circuit_simulate(DNACircuit *circuit, double total_time_s,
                          double dt_s) {
    if (!circuit || total_time_s <= 0.0 || dt_s <= 0.0) return -1;
    if (circuit->num_gates < 1) return 0;

    int steps = (int)(total_time_s / dt_s);
    for (int step = 0; step < steps; step++) {
        for (int g = 0; g < circuit->num_gates; g++) {
            DNALogicGate *gate = &circuit->gates[g];
            int out_id = gate->output_strand_id;
            if (out_id < 0 || out_id >= circuit->num_strands) continue;

            double input_conc = 0.0;
            for (int i = 0; i < gate->num_inputs; i++) {
                int in_id = gate->input_strand_ids[i];
                if (in_id >= 0 && in_id < circuit->num_strands)
                    input_conc += circuit->strands[in_id].concentration;
            }

            switch (gate->type) {
                case BIO_GATE_AND:
                    circuit->strands[out_id].concentration +=
                        dt_s * gate->rate_constant * input_conc * input_conc;
                    break;
                case BIO_GATE_OR:
                    circuit->strands[out_id].concentration +=
                        dt_s * gate->rate_constant * input_conc;
                    break;
                case BIO_GATE_NOT:
                    circuit->strands[out_id].concentration +=
                        dt_s * gate->rate_constant * (1.0 - input_conc);
                    break;
                case BIO_GATE_THRESHOLD:
                    if (input_conc > gate->threshold)
                        circuit->strands[out_id].concentration +=
                            dt_s * gate->rate_constant * input_conc;
                    break;
                case BIO_GATE_SEESAW:
                    circuit->strands[out_id].concentration +=
                        dt_s * gate->rate_constant * input_conc;
                    circuit->strands[gate->input_strand_ids[0]].concentration -=
                        dt_s * gate->rate_constant * input_conc;
                    break;
                default:
                    circuit->strands[out_id].concentration +=
                        dt_s * gate->rate_constant * input_conc;
                    break;
            }
            if (circuit->strands[out_id].concentration < 0.0)
                circuit->strands[out_id].concentration = 0.0;
            if (circuit->strands[out_id].concentration > 1.0)
                circuit->strands[out_id].concentration = 1.0;
        }
    }
    return 0;
}

int bio_circuit_read_outputs(const DNACircuit *circuit, double *outputs,
                              int max_outputs) {
    if (!circuit || !outputs) return -1;
    int count = 0;
    for (int i = 0; i < circuit->num_strands && count < max_outputs; i++) {
        if (circuit->strands[i].is_output) {
            outputs[count++] = circuit->strands[i].concentration;
        }
    }
    return count;
}

int bio_eval_and(DNACircuit *circuit, int in_a_id, int in_b_id, int out_id) {
    if (!circuit) return -1;
    double a = circuit->strands[in_a_id].concentration;
    double b = circuit->strands[in_b_id].concentration;
    circuit->strands[out_id].concentration = (a > 0.5 && b > 0.5) ? 1.0 : 0.0;
    return 0;
}

int bio_eval_or(DNACircuit *circuit, int in_a_id, int in_b_id, int out_id) {
    if (!circuit) return -1;
    double a = circuit->strands[in_a_id].concentration;
    double b = circuit->strands[in_b_id].concentration;
    circuit->strands[out_id].concentration = (a > 0.5 || b > 0.5) ? 1.0 : 0.0;
    return 0;
}

int bio_eval_not(DNACircuit *circuit, int in_id, int out_id) {
    if (!circuit) return -1;
    double a = circuit->strands[in_id].concentration;
    circuit->strands[out_id].concentration = (a < 0.5) ? 1.0 : 0.0;
    return 0;
}

/* ---- DNA Data Storage (L7) ---- */

void bio_encode_binary(const uint8_t *data, int data_len,
                        char *dna_sequence, int max_seq_len) {
    if (!data || !dna_sequence || max_seq_len < 1) return;
    int seq_pos = 0;
    for (int i = 0; i < data_len && seq_pos + 4 < max_seq_len; i++) {
        for (int shift = 6; shift >= 0; shift -= 2) {
            int bits = (data[i] >> shift) & 0x03;
            switch (bits) {
                case 0: dna_sequence[seq_pos++] = 'A'; break;
                case 1: dna_sequence[seq_pos++] = 'C'; break;
                case 2: dna_sequence[seq_pos++] = 'G'; break;
                case 3: dna_sequence[seq_pos++] = 'T'; break;
            }
        }
    }
    dna_sequence[seq_pos] = '\0';
}

int bio_decode_binary(const char *dna_sequence,
                       uint8_t *data, int max_data_len) {
    if (!dna_sequence || !data || max_data_len < 1) return -1;
    int seq_len = (int)strlen(dna_sequence);
    int data_pos = 0;
    for (int i = 0; i + 3 < seq_len && data_pos < max_data_len; i += 4) {
        uint8_t byte = 0;
        for (int j = 0; j < 4; j++) {
            int bits = 0;
            switch (dna_sequence[i + j]) {
                case 'A': case 'a': bits = 0; break;
                case 'C': case 'c': bits = 1; break;
                case 'G': case 'g': bits = 2; break;
                case 'T': case 't': bits = 3; break;
                default: bits = 0; break;
            }
            byte = (uint8_t)((byte << 2) | bits);
        }
        data[data_pos++] = byte;
    }
    return data_pos;
}

/* ---- L8: Needleman-Wunsch Global Alignment ---- */

int bio_needleman_wunsch(const DNAStrand *a, const DNAStrand *b,
                          int *score, char *alignment_a, char *alignment_b,
                          int max_align_len) {
    if (!a || !b || !score) return -1;
    int m = a->length;
    int n = b->length;
    if (m < 1 || n < 1) return -1;

    int match = 1, mismatch = -1, gap = -2;

    int *dp = (int*)calloc((size_t)((m + 1) * (n + 1)), sizeof(int));
    if (!dp) return -1;

    for (int i = 0; i <= m; i++) dp[i * (n + 1) + 0] = i * gap;
    for (int j = 0; j <= n; j++) dp[0 * (n + 1) + j] = j * gap;

    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            int diag = dp[(i - 1) * (n + 1) + (j - 1)] +
                       ((a->sequence[i - 1] == b->sequence[j - 1]) ? match : mismatch);
            int up = dp[(i - 1) * (n + 1) + j] + gap;
            int left = dp[i * (n + 1) + (j - 1)] + gap;
            int best = diag;
            if (up > best) best = up;
            if (left > best) best = left;
            dp[i * (n + 1) + j] = best;
        }
    }
    *score = dp[m * (n + 1) + n];

    /* Traceback */
    if (alignment_a && alignment_b && max_align_len > m + n) {
        int i = m, j = n, pos = 0;
        char *rev_a = (char*)malloc((size_t)(m + n + 1));
        char *rev_b = (char*)malloc((size_t)(m + n + 1));
        while (i > 0 || j > 0) {
            if (i > 0 && j > 0 &&
                dp[i * (n + 1) + j] == dp[(i - 1) * (n + 1) + (j - 1)] +
                ((a->sequence[i - 1] == b->sequence[j - 1]) ? match : mismatch)) {
                rev_a[pos] = bio_base_to_char(a->sequence[i - 1]);
                rev_b[pos] = bio_base_to_char(b->sequence[j - 1]);
                i--; j--;
            } else if (i > 0 && dp[i * (n + 1) + j] == dp[(i - 1) * (n + 1) + j] + gap) {
                rev_a[pos] = bio_base_to_char(a->sequence[i - 1]);
                rev_b[pos] = '-';
                i--;
            } else {
                rev_a[pos] = '-';
                rev_b[pos] = bio_base_to_char(b->sequence[j - 1]);
                j--;
            }
            pos++;
        }
        rev_a[pos] = '\0'; rev_b[pos] = '\0';
        for (int k = 0; k < pos; k++) {
            alignment_a[k] = rev_a[pos - 1 - k];
            alignment_b[k] = rev_b[pos - 1 - k];
        }
        alignment_a[pos] = '\0'; alignment_b[pos] = '\0';
        free(rev_a); free(rev_b);
    }
    free(dp);
    return 0;
}

/* ---- Utility ---- */

void bio_print_circuit(const DNACircuit *circuit) {
    if (!circuit) return;
    printf("DNA Circuit: %d strands, %d gates, %.1f C, %.1f uL\n",
           circuit->num_strands, circuit->num_gates,
           circuit->temperature_celsius, circuit->total_volume_uL);
    for (int i = 0; i < circuit->num_strands; i++) {
        char buf[64];
        bio_strand_to_string(&circuit->strands[i], buf, 32);
        printf("  Strand[%d] '%s': %.4f nM [%.32s...]\n",
               i, circuit->strands[i].name,
               circuit->strands[i].concentration, buf);
    }
}

double bio_compute_diversity(const DNACircuit *circuit) {
    if (!circuit || circuit->num_strands < 1) return 0.0;
    double total_gc = 0.0;
    for (int i = 0; i < circuit->num_strands; i++)
        total_gc += bio_gc_content(&circuit->strands[i]);
    return total_gc / (double)circuit->num_strands;
}

const char* bio_gate_name(BioGateType type) {
    switch (type) {
        case BIO_GATE_AND: return "AND";
        case BIO_GATE_OR: return "OR";
        case BIO_GATE_NOT: return "NOT";
        case BIO_GATE_NAND: return "NAND";
        case BIO_GATE_NOR: return "NOR";
        case BIO_GATE_XOR: return "XOR";
        case BIO_GATE_SEESAW: return "Seesaw";
        case BIO_GATE_THRESHOLD: return "Threshold";
        case BIO_GATE_CATALYTIC: return "Catalytic";
        default: return "Unknown";
    }
}
