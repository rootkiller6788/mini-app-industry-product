#ifndef MINI_FUTURE_QUANTUM_EMULATOR_H
#define MINI_FUTURE_QUANTUM_EMULATOR_H

#include <stdbool.h>
#include <stdint.h>

/* Quantum Emulator -- Gate-Based Quantum Circuit Simulator
 * L1: QuantumComplex, QuantumState, QuantumGate, QuantumCircuit
 * L2: Superposition, entanglement, measurement collapse
 * L3: State vector simulation (2^n complex amplitudes)
 * L4: No-cloning theorem, quantum threshold theorem
 * L5: Grover search (O(sqrt(N))), Deutsch-Jozsa, QFT
 * L6: Quantum circuit simulation, oracle-based search
 * L7: Quantum chemistry simulation, optimization
 * L8: Surface code error correction
 * L9: IBM Quantum (127q Eagle, 1121q Condor), Google Sycamore
 * References:
 * - Nielsen & Chuang, "Quantum Computation and Quantum Information" (2010)
 * - Feynman, "Simulating physics with computers" (1982)
 * - Shor, "Polynomial-time algorithms for prime factorization" (1994)
 * - Grover, STOC (1996)
 */

#define QUANTUM_MAX_QUBITS    12
#define QUANTUM_MAX_GATES     256
#define QUANTUM_STATE_SIZE    (1 << QUANTUM_MAX_QUBITS)
#define QUANTUM_EPS           1e-10

typedef struct { double real; double imag; } QuantumComplex;

typedef enum {
    QGATE_I=0, QGATE_X=1, QGATE_Y=2, QGATE_Z=3, QGATE_H=4,
    QGATE_S=5, QGATE_T=6, QGATE_RX=7, QGATE_RY=8, QGATE_RZ=9,
    QGATE_CNOT=10, QGATE_CZ=11, QGATE_SWAP=12, QGATE_TOFFOLI=13,
    QGATE_MEASURE=14, QGATE_CUSTOM=15
} QuantumGateType;

typedef struct {
    int num_qubits;
    int state_dim;
    QuantumComplex *amplitudes;
    bool is_normalized;
} QuantumState;

typedef struct {
    QuantumGateType type;
    int target_qubit;
    int control_qubit;
    double parameter;
    QuantumComplex custom_matrix[4];
} QuantumGate;

typedef struct {
    int num_qubits;
    QuantumGate gates[QUANTUM_MAX_GATES];
    int num_gates;
    char name[64];
} QuantumCircuit;

typedef struct {
    int classical_bits;
    int num_measurements;
    double probability;
} QuantumMeasurement;

/* Complex arithmetic */
QuantumComplex quantum_complex(double real, double imag);
QuantumComplex quantum_complex_add(QuantumComplex a, QuantumComplex b);
QuantumComplex quantum_complex_sub(QuantumComplex a, QuantumComplex b);
QuantumComplex quantum_complex_mul(QuantumComplex a, QuantumComplex b);
double        quantum_complex_abs2(QuantumComplex c);
QuantumComplex quantum_complex_conj(QuantumComplex c);
QuantumComplex quantum_complex_scale(QuantumComplex c, double s);

/* State management */
QuantumState* quantum_state_create(int num_qubits);
QuantumState* quantum_state_copy(const QuantumState *state);
void quantum_state_destroy(QuantumState *state);
int  quantum_state_set_basis(QuantumState *state, int basis_index);
void quantum_state_normalize(QuantumState *state);
double quantum_state_probability(const QuantumState *state, int basis_index);
double quantum_state_fidelity(const QuantumState *a, const QuantumState *b);
double quantum_state_concurrence(const QuantumState *state);

/* Gate application */
int quantum_apply_gate(QuantumState *state, QuantumGateType type,
                        int target_qubit, double parameter);
int quantum_apply_controlled(QuantumState *state, QuantumGateType type,
                              int control_qubit, int target_qubit);
int quantum_apply_custom_1q(QuantumState *state, int target_qubit,
                             const QuantumComplex matrix[4]);
int quantum_apply_swap(QuantumState *state, int qubit_a, int qubit_b);
int quantum_apply_toffoli(QuantumState *state, int ctrl_a, int ctrl_b,
                           int target);

/* Circuit execution */
QuantumCircuit* quantum_circuit_create(int num_qubits, const char *name);
void quantum_circuit_destroy(QuantumCircuit *circuit);
int  quantum_circuit_add_gate(QuantumCircuit *circuit, QuantumGateType type,
                               int target, int control, double param);
QuantumState* quantum_circuit_execute(const QuantumCircuit *circuit);
int  quantum_circuit_execute_on(QuantumCircuit *circuit, QuantumState *state);

/* Measurement */
int quantum_measure_all(QuantumState *state);
int quantum_measure_qubit(QuantumState *state, int qubit);
int quantum_measure_distribution(const QuantumState *state,
                                  QuantumMeasurement *results, int max_results);

/* Algorithms */
int  quantum_grover_search(int num_qubits, int marked_index,
                            QuantumMeasurement *result);
bool quantum_deutsch_jozsa(int num_qubits, int (*oracle)(int input),
                            bool *is_constant);
int  quantum_qft(QuantumState *state);
int  quantum_iqft(QuantumState *state);

/* Error correction */
QuantumState* quantum_bitflip_encode(const QuantumState *logical);
int  quantum_bitflip_decode(QuantumState *physical);
QuantumState* quantum_shor9_encode(const QuantumState *logical);
int  quantum_shor9_decode(QuantumState *physical);

/* Utility */
const char* quantum_gate_name(QuantumGateType type);
void quantum_state_print(const QuantumState *state);
void quantum_circuit_print(const QuantumCircuit *circuit);

#endif
