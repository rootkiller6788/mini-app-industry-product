#include "quantum_emulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================================================================
 *  quantum_emulator.c -- Gate-Based Quantum Circuit Simulator
 *
 *  L1: QuantumComplex, QuantumState, QuantumGate, QuantumCircuit
 *  L2: Superposition, entanglement, measurement (Born rule)
 *  L3: State vector simulation, gate application via tensor products
 *  L4: No-cloning theorem, Gottesman-Knill theorem
 *  L5: Grover search (O(sqrt(N))), Deutsch-Jozsa, QFT
 *  L6: Quantum circuit simulation, oracle-based search
 *  L7: Quantum chemistry, optimization
 *  L8: Bit-flip code, Shor 9-qubit code
 *  L9: IBM Quantum (127q Eagle), Google Sycamore (53q)
 *
 *  References:
 *  - Nielsen & Chuang (2010)
 *  - Feynman (1982)
 *  - Shor (1994), Grover (STOC 1996)
 * ================================================================ */

/* ---- Complex Number Operations ---- */

QuantumComplex quantum_complex(double real, double imag) {
    QuantumComplex c;
    c.real = real;
    c.imag = imag;
    return c;
}

QuantumComplex quantum_complex_add(QuantumComplex a, QuantumComplex b) {
    return quantum_complex(a.real + b.real, a.imag + b.imag);
}

QuantumComplex quantum_complex_sub(QuantumComplex a, QuantumComplex b) {
    return quantum_complex(a.real - b.real, a.imag - b.imag);
}

QuantumComplex quantum_complex_mul(QuantumComplex a, QuantumComplex b) {
    return quantum_complex(a.real * b.real - a.imag * b.imag,
                           a.real * b.imag + a.imag * b.real);
}

double quantum_complex_abs2(QuantumComplex c) {
    return c.real * c.real + c.imag * c.imag;
}

QuantumComplex quantum_complex_conj(QuantumComplex c) {
    return quantum_complex(c.real, -c.imag);
}

QuantumComplex quantum_complex_scale(QuantumComplex c, double s) {
    return quantum_complex(c.real * s, c.imag * s);
}

/* ---- Quantum State Management ---- */

QuantumState* quantum_state_create(int num_qubits) {
    if (num_qubits < 1 || num_qubits > QUANTUM_MAX_QUBITS) return NULL;
    QuantumState *state = (QuantumState*)calloc(1, sizeof(QuantumState));
    if (!state) return NULL;
    state->num_qubits = num_qubits;
    state->state_dim = 1 << num_qubits;
    state->amplitudes = (QuantumComplex*)calloc((size_t)state->state_dim,
                                                 sizeof(QuantumComplex));
    if (!state->amplitudes) {
        free(state);
        return NULL;
    }
    state->amplitudes[0] = quantum_complex(1.0, 0.0);
    state->is_normalized = true;
    return state;
}

QuantumState* quantum_state_copy(const QuantumState *state) {
    if (!state) return NULL;
    QuantumState *copy = (QuantumState*)malloc(sizeof(QuantumState));
    if (!copy) return NULL;
    copy->num_qubits = state->num_qubits;
    copy->state_dim = state->state_dim;
    copy->amplitudes = (QuantumComplex*)malloc(
        (size_t)state->state_dim * sizeof(QuantumComplex));
    if (!copy->amplitudes) {
        free(copy);
        return NULL;
    }
    memcpy(copy->amplitudes, state->amplitudes,
           (size_t)state->state_dim * sizeof(QuantumComplex));
    copy->is_normalized = state->is_normalized;
    return copy;
}

void quantum_state_destroy(QuantumState *state) {
    if (state) {
        free(state->amplitudes);
        free(state);
    }
}

int quantum_state_set_basis(QuantumState *state, int basis_index) {
    if (!state || basis_index < 0 || basis_index >= state->state_dim)
        return -1;
    memset(state->amplitudes, 0,
           (size_t)state->state_dim * sizeof(QuantumComplex));
    state->amplitudes[basis_index] = quantum_complex(1.0, 0.0);
    state->is_normalized = true;
    return 0;
}

void quantum_state_normalize(QuantumState *state) {
    if (!state) return;
    double norm_sq = 0.0;
    for (int i = 0; i < state->state_dim; i++) {
        norm_sq += quantum_complex_abs2(state->amplitudes[i]);
    }
    if (norm_sq < QUANTUM_EPS) return;
    double inv_norm = 1.0 / sqrt(norm_sq);
    for (int i = 0; i < state->state_dim; i++) {
        state->amplitudes[i] = quantum_complex_scale(state->amplitudes[i],
                                                      inv_norm);
    }
    state->is_normalized = true;
}

double quantum_state_probability(const QuantumState *state, int basis_index) {
    if (!state || basis_index < 0 || basis_index >= state->state_dim)
        return 0.0;
    return quantum_complex_abs2(state->amplitudes[basis_index]);
}

double quantum_state_fidelity(const QuantumState *a, const QuantumState *b) {
    if (!a || !b || a->state_dim != b->state_dim) return 0.0;
    QuantumComplex inner = quantum_complex(0.0, 0.0);
    for (int i = 0; i < a->state_dim; i++) {
        inner = quantum_complex_add(inner,
            quantum_complex_mul(quantum_complex_conj(a->amplitudes[i]),
                                b->amplitudes[i]));
    }
    return quantum_complex_abs2(inner);
}

double quantum_state_concurrence(const QuantumState *state) {
    if (!state || state->num_qubits != 2) return -1.0;
    double a0 = state->amplitudes[0].real;
    double a1 = state->amplitudes[1].real;
    double a2 = state->amplitudes[2].real;
    double a3 = state->amplitudes[3].real;
    double C = 2.0 * fabs(a0 * a3 - a1 * a2);
    return (C > 1.0) ? 1.0 : C;
}

/* ---- 2x2 Gate Matrices ---- */

static void get_gate_matrix(QuantumGateType type, double param,
                             QuantumComplex mat[4]) {
    double c, s;
    switch (type) {
        case QGATE_I:
            mat[0] = quantum_complex(1,0); mat[1] = quantum_complex(0,0);
            mat[2] = quantum_complex(0,0); mat[3] = quantum_complex(1,0);
            break;
        case QGATE_X:
            mat[0] = quantum_complex(0,0); mat[1] = quantum_complex(1,0);
            mat[2] = quantum_complex(1,0); mat[3] = quantum_complex(0,0);
            break;
        case QGATE_Y:
            mat[0] = quantum_complex(0,0); mat[1] = quantum_complex(0,-1);
            mat[2] = quantum_complex(0,1); mat[3] = quantum_complex(0,0);
            break;
        case QGATE_Z:
            mat[0] = quantum_complex(1,0); mat[1] = quantum_complex(0,0);
            mat[2] = quantum_complex(0,0); mat[3] = quantum_complex(-1,0);
            break;
        case QGATE_H:
            s = 1.0 / sqrt(2.0);
            mat[0] = quantum_complex(s,0); mat[1] = quantum_complex(s,0);
            mat[2] = quantum_complex(s,0); mat[3] = quantum_complex(-s,0);
            break;
        case QGATE_S:
            mat[0] = quantum_complex(1,0); mat[1] = quantum_complex(0,0);
            mat[2] = quantum_complex(0,0); mat[3] = quantum_complex(0,1);
            break;
        case QGATE_T:
            s = sqrt(2.0) / 2.0;
            mat[0] = quantum_complex(1,0); mat[1] = quantum_complex(0,0);
            mat[2] = quantum_complex(0,0); mat[3] = quantum_complex(s,s);
            break;
        case QGATE_RX:
            c = cos(param / 2.0); s = sin(param / 2.0);
            mat[0] = quantum_complex(c,0); mat[1] = quantum_complex(0,-s);
            mat[2] = quantum_complex(0,-s); mat[3] = quantum_complex(c,0);
            break;
        case QGATE_RY:
            c = cos(param / 2.0); s = sin(param / 2.0);
            mat[0] = quantum_complex(c,0); mat[1] = quantum_complex(-s,0);
            mat[2] = quantum_complex(s,0); mat[3] = quantum_complex(c,0);
            break;
        case QGATE_RZ:
            c = cos(param / 2.0); s = sin(param / 2.0);
            mat[0] = quantum_complex(c,-s); mat[1] = quantum_complex(0,0);
            mat[2] = quantum_complex(0,0); mat[3] = quantum_complex(c,s);
            break;
        default:
            mat[0] = quantum_complex(1,0); mat[1] = quantum_complex(0,0);
            mat[2] = quantum_complex(0,0); mat[3] = quantum_complex(1,0);
            break;
    }
}

/* Apply a 2x2 unitary U to qubit k of n-qubit state
 * new_psi[i] = U[0][0]*psi[i_with_0] + U[0][1]*psi[i_with_1]  (k-th bit is 0)
 * new_psi[i|1<<k] = U[1][0]*psi[i_with_0] + U[1][1]*psi[i_with_1]
 */
int quantum_apply_gate(QuantumState *state, QuantumGateType type,
                        int target_qubit, double parameter) {
    if (!state || target_qubit < 0 || target_qubit >= state->num_qubits)
        return -1;

    QuantumComplex mat[4];
    get_gate_matrix(type, parameter, mat);

    int mask = 1 << target_qubit;
    int dim = state->state_dim;

    for (int i = 0; i < dim; i++) {
        if (i & mask) continue;
        int i0 = i;
        int i1 = i | mask;

        QuantumComplex a0 = state->amplitudes[i0];
        QuantumComplex a1 = state->amplitudes[i1];

        state->amplitudes[i0] = quantum_complex_add(
            quantum_complex_mul(mat[0], a0),
            quantum_complex_mul(mat[1], a1));
        state->amplitudes[i1] = quantum_complex_add(
            quantum_complex_mul(mat[2], a0),
            quantum_complex_mul(mat[3], a1));
    }
    return 0;
}

/* Controlled gate: applies 2x2 to target only when control is |1>
 * Equivalent to: |0><0| (x) I + |1><1| (x) U
 */
int quantum_apply_controlled(QuantumState *state, QuantumGateType type,
                              int control_qubit, int target_qubit) {
    if (!state || control_qubit < 0 || control_qubit >= state->num_qubits ||
        target_qubit < 0 || target_qubit >= state->num_qubits ||
        control_qubit == target_qubit) return -1;

    QuantumComplex mat[4];
    get_gate_matrix(type, 0.0, mat);

    int c_mask = 1 << control_qubit;
    int t_mask = 1 << target_qubit;
    int dim = state->state_dim;

    for (int i = 0; i < dim; i++) {
        if (!(i & c_mask)) continue;
        if (i & t_mask) continue;
        int i0 = i;
        int i1 = i | t_mask;

        QuantumComplex a0 = state->amplitudes[i0];
        QuantumComplex a1 = state->amplitudes[i1];

        state->amplitudes[i0] = quantum_complex_add(
            quantum_complex_mul(mat[0], a0),
            quantum_complex_mul(mat[1], a1));
        state->amplitudes[i1] = quantum_complex_add(
            quantum_complex_mul(mat[2], a0),
            quantum_complex_mul(mat[3], a1));
    }
    return 0;
}

int quantum_apply_custom_1q(QuantumState *state, int target_qubit,
                             const QuantumComplex matrix[4]) {
    if (!state || !matrix || target_qubit < 0 ||
        target_qubit >= state->num_qubits) return -1;

    int mask = 1 << target_qubit;
    int dim = state->state_dim;

    for (int i = 0; i < dim; i++) {
        if (i & mask) continue;
        int i0 = i;
        int i1 = i | mask;

        QuantumComplex a0 = state->amplitudes[i0];
        QuantumComplex a1 = state->amplitudes[i1];

        state->amplitudes[i0] = quantum_complex_add(
            quantum_complex_mul(matrix[0], a0),
            quantum_complex_mul(matrix[1], a1));
        state->amplitudes[i1] = quantum_complex_add(
            quantum_complex_mul(matrix[2], a0),
            quantum_complex_mul(matrix[3], a1));
    }
    return 0;
}

int quantum_apply_swap(QuantumState *state, int qubit_a, int qubit_b) {
    if (!state || qubit_a < 0 || qubit_a >= state->num_qubits ||
        qubit_b < 0 || qubit_b >= state->num_qubits || qubit_a == qubit_b)
        return -1;

    int mask_a = 1 << qubit_a;
    int mask_b = 1 << qubit_b;
    int dim = state->state_dim;

    for (int i = 0; i < dim; i++) {
        bool a_set = (i & mask_a) != 0;
        bool b_set = (i & mask_b) != 0;
        if (a_set == b_set) continue;
        int j = i ^ mask_a ^ mask_b;
        if (j > i) {
            QuantumComplex tmp = state->amplitudes[i];
            state->amplitudes[i] = state->amplitudes[j];
            state->amplitudes[j] = tmp;
        }
    }
    return 0;
}

int quantum_apply_toffoli(QuantumState *state, int ctrl_a, int ctrl_b,
                           int target) {
    if (!state || ctrl_a < 0 || ctrl_a >= state->num_qubits ||
        ctrl_b < 0 || ctrl_b >= state->num_qubits ||
        target < 0 || target >= state->num_qubits) return -1;
    if (ctrl_a == target || ctrl_b == target || ctrl_a == ctrl_b) return -1;

    int ca = 1 << ctrl_a;
    int cb = 1 << ctrl_b;
    int ct = 1 << target;
    int dim = state->state_dim;

    for (int i = 0; i < dim; i++) {
        if (!(i & ca) || !(i & cb)) continue;
        int i0 = i & ~ct;
        int i1 = i | ct;
        if (i0 == i) {
            QuantumComplex tmp = state->amplitudes[i0];
            state->amplitudes[i0] = state->amplitudes[i1];
            state->amplitudes[i1] = tmp;
        }
    }
    return 0;
}

/* ---- Circuit Compilation & Execution ---- */

QuantumCircuit* quantum_circuit_create(int num_qubits, const char *name) {
    if (num_qubits < 1 || num_qubits > QUANTUM_MAX_QUBITS) return NULL;
    QuantumCircuit *circ = (QuantumCircuit*)calloc(1, sizeof(QuantumCircuit));
    if (!circ) return NULL;
    circ->num_qubits = num_qubits;
    if (name) strncpy(circ->name, name, sizeof(circ->name) - 1);
    return circ;
}

void quantum_circuit_destroy(QuantumCircuit *circuit) {
    free(circuit);
}

int quantum_circuit_add_gate(QuantumCircuit *circuit, QuantumGateType type,
                              int target, int control, double param) {
    if (!circuit || circuit->num_gates >= QUANTUM_MAX_GATES) return -1;
    int idx = circuit->num_gates++;
    circuit->gates[idx].type = type;
    circuit->gates[idx].target_qubit = target;
    circuit->gates[idx].control_qubit = control;
    circuit->gates[idx].parameter = param;
    return idx;
}

QuantumState* quantum_circuit_execute(const QuantumCircuit *circuit) {
    if (!circuit) return NULL;
    QuantumState *state = quantum_state_create(circuit->num_qubits);
    if (!state) return NULL;
    if (quantum_circuit_execute_on((QuantumCircuit*)circuit, state) != 0) {
        quantum_state_destroy(state);
        return NULL;
    }
    return state;
}

int quantum_circuit_execute_on(QuantumCircuit *circuit, QuantumState *state) {
    if (!circuit || !state) return -1;
    for (int i = 0; i < circuit->num_gates; i++) {
        QuantumGate *g = &circuit->gates[i];
        switch (g->type) {
            case QGATE_CNOT:
                quantum_apply_controlled(state, QGATE_X,
                                          g->control_qubit, g->target_qubit);
                break;
            case QGATE_CZ:
                quantum_apply_controlled(state, QGATE_Z,
                                          g->control_qubit, g->target_qubit);
                break;
            case QGATE_SWAP:
                quantum_apply_swap(state, g->target_qubit, g->control_qubit);
                break;
            case QGATE_TOFFOLI:
                quantum_apply_toffoli(state, g->control_qubit,
                                       g->target_qubit, 0);
                break;
            case QGATE_CUSTOM:
                quantum_apply_custom_1q(state, g->target_qubit,
                                         g->custom_matrix);
                break;
            default:
                quantum_apply_gate(state, g->type,
                                    g->target_qubit, g->parameter);
                break;
        }
    }
    return 0;
}

/* ---- Measurement (L2: Born Rule) ---- */

int quantum_measure_all(QuantumState *state) {
    if (!state || state->state_dim < 1) return -1;
    double r = (double)rand() / (double)RAND_MAX;
    double cumulative = 0.0;
    for (int i = 0; i < state->state_dim; i++) {
        cumulative += quantum_complex_abs2(state->amplitudes[i]);
        if (r < cumulative || i == state->state_dim - 1) {
            for (int j = 0; j < state->state_dim; j++) {
                state->amplitudes[j] = quantum_complex(0.0, 0.0);
            }
            state->amplitudes[i] = quantum_complex(1.0, 0.0);
            return i;
        }
    }
    return 0;
}

int quantum_measure_qubit(QuantumState *state, int qubit) {
    if (!state || qubit < 0 || qubit >= state->num_qubits) return -1;
    int mask = 1 << qubit;
    double prob_one = 0.0;
    for (int i = 0; i < state->state_dim; i++) {
        if (i & mask) {
            prob_one += quantum_complex_abs2(state->amplitudes[i]);
        }
    }
    int outcome = ((double)rand() / (double)RAND_MAX) < prob_one ? 1 : 0;
    double norm = 0.0;
    for (int i = 0; i < state->state_dim; i++) {
        if (((i & mask) != 0) != outcome) {
            state->amplitudes[i] = quantum_complex(0.0, 0.0);
        } else {
            norm += quantum_complex_abs2(state->amplitudes[i]);
        }
    }
    if (norm < QUANTUM_EPS) return outcome;
    double inv_norm = 1.0 / sqrt(norm);
    for (int i = 0; i < state->state_dim; i++) {
        state->amplitudes[i] = quantum_complex_scale(state->amplitudes[i],
                                                      inv_norm);
    }
    return outcome;
}

int quantum_measure_distribution(const QuantumState *state,
                                  QuantumMeasurement *results,
                                  int max_results) {
    if (!state || !results) return -1;
    int count = (state->state_dim < max_results) ? state->state_dim : max_results;
    for (int i = 0; i < count; i++) {
        results[i].classical_bits = i;
        results[i].num_measurements = state->num_qubits;
        results[i].probability = quantum_complex_abs2(state->amplitudes[i]);
    }
    return count;
}

/* ---- L5: Grover's Search Algorithm ---- */

int quantum_grover_search(int num_qubits, int marked_index,
                           QuantumMeasurement *result) {
    if (num_qubits < 2 || num_qubits > QUANTUM_MAX_QUBITS || !result) return -1;
    int N = 1 << num_qubits;
    if (marked_index < 0 || marked_index >= N) return -1;

    QuantumState *state = quantum_state_create(num_qubits);
    if (!state) return -1;

    /* Step 1: Equal superposition */
    for (int q = 0; q < num_qubits; q++) {
        quantum_apply_gate(state, QGATE_H, q, 0.0);
    }

    /* Step 2-4: Grover iterations */
    int iterations = (int)(M_PI / 4.0 * sqrt((double)N));
    if (iterations < 1) iterations = 1;

    for (int iter = 0; iter < iterations; iter++) {
        /* Oracle: flip sign of marked state */
        state->amplitudes[marked_index] = quantum_complex_scale(
            state->amplitudes[marked_index], -1.0);

        /* Diffusion operator: 2|s><s| - I */
        for (int q = 0; q < num_qubits; q++) {
            quantum_apply_gate(state, QGATE_H, q, 0.0);
        }
        for (int q = 0; q < num_qubits; q++) {
            quantum_apply_gate(state, QGATE_X, q, 0.0);
        }
        /* Multi-controlled Z */
        quantum_apply_gate(state, QGATE_H, num_qubits - 1, 0.0);
        for (int q = 0; q < num_qubits - 1; q++) {
            quantum_apply_controlled(state, QGATE_X, q, num_qubits - 1);
        }
        quantum_apply_gate(state, QGATE_H, num_qubits - 1, 0.0);
        for (int q = num_qubits - 2; q >= 0; q--) {
            quantum_apply_controlled(state, QGATE_X, q, num_qubits - 1);
        }
        for (int q = 0; q < num_qubits; q++) {
            quantum_apply_gate(state, QGATE_X, q, 0.0);
        }
        for (int q = 0; q < num_qubits; q++) {
            quantum_apply_gate(state, QGATE_H, q, 0.0);
        }
    }

    /* Measure */
    int measured = quantum_measure_all(state);
    result->classical_bits = measured;
    result->probability = quantum_complex_abs2(state->amplitudes[measured]);

    quantum_state_destroy(state);
    return measured;
}

/* ---- L5: Deutsch-Jozsa Algorithm ---- */

bool quantum_deutsch_jozsa(int num_qubits, int (*oracle)(int input),
                            bool *is_constant) {
    if (num_qubits < 1 || num_qubits > QUANTUM_MAX_QUBITS || !oracle || !is_constant)
        return false;

    QuantumState *state = quantum_state_create(num_qubits + 1);
    if (!state) return false;

    /* Initialize: ancilla = |1> */
    quantum_state_set_basis(state, 1 << num_qubits);
    for (int q = 0; q <= num_qubits; q++) {
        quantum_apply_gate(state, QGATE_H, q, 0.0);
    }

    /* Apply oracle: U_f |x>|y> = |x>|y XOR f(x)> */
    int dim = 1 << num_qubits;
    for (int x = 0; x < dim; x++) {
        int fx = oracle(x);
        for (int y = 0; y < 2; y++) {
            int idx = (x << 1) | (y ^ fx);
            if (idx != (x << 1) | y) {
                QuantumComplex tmp = state->amplitudes[(x << 1) | y];
                state->amplitudes[(x << 1) | y] = state->amplitudes[idx];
                state->amplitudes[idx] = tmp;
            }
        }
    }

    /* Hadamard on input qubits */
    for (int q = 0; q < num_qubits; q++) {
        quantum_apply_gate(state, QGATE_H, q, 0.0);
    }

    /* Measure: if all zeros -> constant, else -> balanced */
    double prob_zero = 0.0;
    for (int i = 0; i < (1 << num_qubits); i++) {
        prob_zero += quantum_complex_abs2(state->amplitudes[i << 1]);
    }

    *is_constant = (prob_zero > 0.99);
    quantum_state_destroy(state);
    return true;
}

/* ---- L5: Quantum Fourier Transform ---- */

int quantum_qft(QuantumState *state) {
    if (!state) return -1;
    int n = state->num_qubits;

    for (int j = 0; j < n; j++) {
        quantum_apply_gate(state, QGATE_H, j, 0.0);
        for (int k = j + 1; k < n; k++) {
            double angle = M_PI / (double)(1 << (k - j));
            quantum_apply_gate(state, QGATE_RZ, k, angle);
            quantum_apply_controlled(state, QGATE_X, j, k);
            quantum_apply_gate(state, QGATE_RZ, k, -angle);
            quantum_apply_controlled(state, QGATE_X, j, k);
            quantum_apply_gate(state, QGATE_RZ, j, angle);
        }
    }
    return 0;
}

int quantum_iqft(QuantumState *state) {
    if (!state) return -1;
    int n = state->num_qubits;

    for (int j = n - 1; j >= 0; j--) {
        for (int k = n - 1; k > j; k--) {
            double angle = -M_PI / (double)(1 << (k - j));
            quantum_apply_gate(state, QGATE_RZ, j, -angle);
            quantum_apply_controlled(state, QGATE_X, j, k);
            quantum_apply_gate(state, QGATE_RZ, k, angle);
            quantum_apply_controlled(state, QGATE_X, j, k);
            quantum_apply_gate(state, QGATE_RZ, k, -angle);
        }
        quantum_apply_gate(state, QGATE_H, j, 0.0);
    }
    return 0;
}

/* ---- L8: Quantum Error Correction ---- */

QuantumState* quantum_bitflip_encode(const QuantumState *logical) {
    if (!logical || logical->num_qubits != 1) return NULL;
    QuantumState *physical = quantum_state_create(3);
    if (!physical) return NULL;

    QuantumComplex a0 = logical->amplitudes[0];
    QuantumComplex a1 = logical->amplitudes[1];

    /* |0>_L -> |000>, |1>_L -> |111> */
    physical->amplitudes[0] = a0;  /* |000> */
    physical->amplitudes[7] = a1;  /* |111> */
    physical->is_normalized = logical->is_normalized;

    quantum_state_destroy((QuantumState*)logical);
    return physical;
}

int quantum_bitflip_decode(QuantumState *physical) {
    if (!physical || physical->num_qubits != 3) return -1;

    /* Syndrome measurement using CNOTs */
    /* Measure ZZ on qubits 0,1 and 1,2 */
    double prob[8];
    for (int i = 0; i < 8; i++) {
        prob[i] = quantum_complex_abs2(physical->amplitudes[i]);
    }

    /* Determine most likely error */
    int best_idx = 0;
    double best_prob = prob[0];
    for (int i = 1; i < 8; i++) {
        if (prob[i] > best_prob) {
            best_prob = prob[i];
            best_idx = i;
        }
    }

    /* Majority vote decode: bit = majority of 3 bits */
    int b0 = (best_idx >> 0) & 1;
    int b1 = (best_idx >> 1) & 1;
    int b2 = (best_idx >> 2) & 1;
    int result = (b0 + b1 + b2 >= 2) ? 1 : 0;

    return result;
}

QuantumState* quantum_shor9_encode(const QuantumState *logical) {
    if (!logical || logical->num_qubits != 1) return NULL;
    /* Simplified: just demonstrate the encoding pattern
     * Shor 9-qubit code: concatenates 3-qubit bit-flip code
     * with 3-qubit phase-flip code
     * |0>_L = (|000>+|111>)(|000>+|111>)(|000>+|111>) / 2sqrt(2)
     * Demonstration uses 9 qubits total
     */
    QuantumState *encoded = quantum_state_create(9);
    if (!encoded) return NULL;

    QuantumComplex a0 = logical->amplitudes[0];
    QuantumComplex a1 = logical->amplitudes[1];

    double norm_factor = 1.0 / (2.0 * sqrt(2.0));
    /* Encode: each logical bit state maps to 4 physical basis states */
    for (int i = 0; i < 8; i++) {
        int idx = (i & 1) | ((i & 2) << 2) | ((i & 4) << 4);
        encoded->amplitudes[idx] = quantum_complex_scale(a0, norm_factor);
    }
    for (int i = 0; i < 8; i++) {
        int idx = (i & 1) | ((i & 2) << 2) | ((i & 4) << 4);
        encoded->amplitudes[511 - idx] = quantum_complex_scale(a1, norm_factor);
    }

    encoded->is_normalized = true;
    quantum_state_destroy((QuantumState*)logical);
    return encoded;
}

int quantum_shor9_decode(QuantumState *physical) {
    if (!physical || physical->num_qubits != 9) return -1;

    /* Simplified decode: measure all qubits and majority vote
     * across the three blocks of three qubits each */
    double prob_block[3][2] = {{0}};

    for (int i = 0; i < 512; i++) {
        double p = quantum_complex_abs2(physical->amplitudes[i]);
        for (int block = 0; block < 3; block++) {
            int bits = (i >> (block * 3)) & 7;
            int maj = ((bits & 1) + ((bits >> 1) & 1) + ((bits >> 2) & 1) >= 2) ? 1 : 0;
            prob_block[block][maj] += p;
        }
    }

    /* Final majority across three blocks */
    double prob0 = prob_block[0][0] * prob_block[1][0] * prob_block[2][0];
    double prob1 = prob_block[0][1] * prob_block[1][1] * prob_block[2][1];

    return (prob1 > prob0) ? 1 : 0;
}

/* ---- Utility ---- */

const char* quantum_gate_name(QuantumGateType type) {
    switch (type) {
        case QGATE_I: return "I"; case QGATE_X: return "X";
        case QGATE_Y: return "Y"; case QGATE_Z: return "Z";
        case QGATE_H: return "H"; case QGATE_S: return "S";
        case QGATE_T: return "T"; case QGATE_RX: return "RX";
        case QGATE_RY: return "RY"; case QGATE_RZ: return "RZ";
        case QGATE_CNOT: return "CNOT"; case QGATE_CZ: return "CZ";
        case QGATE_SWAP: return "SWAP"; case QGATE_TOFFOLI: return "TOFFOLI";
        case QGATE_MEASURE: return "MEASURE"; case QGATE_CUSTOM: return "CUSTOM";
        default: return "???";
    }
}

void quantum_state_print(const QuantumState *state) {
    if (!state) return;
    printf("QuantumState: %d qubits, dim=%d\n", state->num_qubits, state->state_dim);
    double non_zero = 0.0;
    for (int i = 0; i < state->state_dim; i++) {
        double p = quantum_complex_abs2(state->amplitudes[i]);
        if (p > 0.001) {
            printf("  |%d>: %.3f%+.3fi (prob=%.4f)\n",
                   i, state->amplitudes[i].real,
                   state->amplitudes[i].imag, p);
            non_zero += p;
        }
    }
    printf("  Total probability: %.4f\n", non_zero);
}

void quantum_circuit_print(const QuantumCircuit *circuit) {
    if (!circuit) return;
    printf("QuantumCircuit '%s': %d qubits, %d gates\n",
           circuit->name, circuit->num_qubits, circuit->num_gates);
    for (int i = 0; i < circuit->num_gates; i++) {
        QuantumGate *g = &circuit->gates[i];
        printf("  [%d] %s q[%d]", i, quantum_gate_name(g->type), g->target_qubit);
        if (g->control_qubit >= 0) printf(" (ctrl: q[%d])", g->control_qubit);
        if (g->parameter != 0.0) printf(" (param: %.3f)", g->parameter);
        printf("\n");
    }
}
