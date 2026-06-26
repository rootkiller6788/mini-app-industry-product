#include "quantum_emulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    srand((unsigned)time(NULL));
    printf("=== Quantum Circuit Demo ===\n\n");

    /* Demo: Bell state preparation */
    printf("--- Bell State (Entanglement) ---\n");
    QuantumState *bell = quantum_state_create(2);
    quantum_apply_gate(bell, QGATE_H, 0, 0.0);
    quantum_apply_controlled(bell, QGATE_X, 0, 1);
    printf("State after H[0]; CNOT[0,1]:\n");
    quantum_state_print(bell);
    double concurrence = quantum_state_concurrence(bell);
    printf("Concurrence (entanglement measure): %.4f\n\n", concurrence);

    /* Demo: Grover's search */
    printf("--- Grover Search (3 qubits, N=8) ---\n");
    for (int target = 0; target < 8; target++) {
        QuantumMeasurement result;
        int found = quantum_grover_search(3, target, &result);
        printf("  Target |%d>: measured |%d> (prob=%.3f) %s\n",
               target, found, result.probability,
               (found == target) ? "CORRECT" : "WRONG");
    }

    /* Demo: Quantum circuit construction */
    printf("\n--- Quantum Circuit (QFT on 3 qubits) ---\n");
    QuantumCircuit *circ = quantum_circuit_create(3, "QFT-3");
    quantum_circuit_add_gate(circ, QGATE_H, 0, -1, 0.0);
    quantum_circuit_add_gate(circ, QGATE_CNOT, 1, 0, 0.0);
    quantum_circuit_add_gate(circ, QGATE_H, 1, -1, 0.0);
    quantum_circuit_add_gate(circ, QGATE_CNOT, 2, 1, 0.0);
    quantum_circuit_add_gate(circ, QGATE_H, 2, -1, 0.0);
    quantum_circuit_print(circ);

    QuantumState *result = quantum_circuit_execute(circ);
    printf("QFT|000> result:\n");
    quantum_state_print(result);

    quantum_state_destroy(bell);
    quantum_state_destroy(result);
    quantum_circuit_destroy(circ);
    return 0;
}
