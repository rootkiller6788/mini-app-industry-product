#ifndef MINI_FUTURE_THERMODYNAMIC_REV_H
#define MINI_FUTURE_THERMODYNAMIC_REV_H

#include <stdbool.h>
#include <stdint.h>

/* Thermodynamic and Reversible Computing
 * L1: ReversibleGate, AdiabaticStage, ThermoCircuit
 * L2: Landauer principle (kT*ln2), adiabatic switching, reversible logic
 * L3: Adiabatic pipeline, Bennett clocking
 * L4: Thermodynamics of computation (Boltzmann, Gibbs free energy)
 * L5: Adiabatic CMOS, SCRL (split-level charge recovery logic)
 * L6: Reversible adder, reversible multiplier
 * L7: Low-power VLSI, energy recovery circuits
 * L8: Ballistic electron transport, superconducting SFQ
 * L9: Quantum-dot cellular automata, nanomagnetic logic
 */

#define THERMO_MAX_GATES      512
#define THERMO_MAX_SIGNALS    256
#define THERMO_MAX_STAGES     64
#define THERMO_BOLTZMANN      1.380649e-23

typedef enum {
    REV_NOT    = 0,
    REV_CNOT   = 1,
    REV_TOFFOLI = 2,
    REV_FREDKIN = 3,
    REV_SWAP   = 4,
    REV_CUSTOM = 5
} ReversibleGateType;

typedef struct {
    int id;
    bool value;
    double energy_joules;
} ThermoSignal;

typedef struct {
    int id;
    ReversibleGateType type;
    int input_signals[4];
    int output_signals[4];
    int num_signals;
    double switching_energy;
    double delay_ps;
} ReversibleGate;

typedef struct {
    int id;
    ReversibleGate *gates;
    int num_gates;
    int cap_gates;
    double clock_period_ps;
    double energy_per_cycle;
    double ramp_time_ps;
    bool is_driven;
} AdiabaticStage;

typedef struct {
    ThermoSignal signals[THERMO_MAX_SIGNALS];
    int num_signals;
    ReversibleGate gates[THERMO_MAX_GATES];
    int num_gates;
    AdiabaticStage stages[THERMO_MAX_STAGES];
    int num_stages;
    double temperature_k;
    double total_energy_joules;
    double reversible_factor;
} ThermoCircuit;

ThermoCircuit* thermo_circuit_create(double temperature_k);
void thermo_circuit_destroy(ThermoCircuit *tc);
int  thermo_add_signal(ThermoCircuit *tc, bool initial_value);
void thermo_set_signal(ThermoCircuit *tc, int signal_id, bool value);
bool thermo_get_signal(const ThermoCircuit *tc, int signal_id);

int  thermo_add_gate(ThermoCircuit *tc, ReversibleGateType type,
                      int in0, int in1, int in2, int out0, int out1, int out2);
int  thermo_evaluate_gate(ThermoCircuit *tc, int gate_id);
void thermo_evaluate_all(ThermoCircuit *tc);

void thermo_rev_not(const bool *in, bool *out);
void thermo_rev_cnot(const bool *in, bool *out);
void thermo_rev_toffoli(const bool *in, bool *out);
void thermo_rev_fredkin(const bool *in, bool *out);
void thermo_rev_swap(const bool *in, bool *out);

double thermo_landauer_energy(int num_bits_erased, double temperature_k);
double thermo_min_switching_energy(double capacitance_fF,
                                    double voltage_V, bool adiabatic);
double thermo_reversible_energy_saved(const ThermoCircuit *tc);
double thermo_cmos_energy_comparison(int num_gates, double Vdd,
                                      double C_load, double frequency);

int  thermo_add_stage(ThermoCircuit *tc, double clock_period_ps);
int  thermo_stage_add_gate(ThermoCircuit *tc, int stage_id,
                            ReversibleGateType type, int in0, int in1,
                            int out0, int out1);
int  thermo_pipeline_step(ThermoCircuit *tc, const bool *inputs, int num_inputs);
int  thermo_full_pipeline(ThermoCircuit *tc, const bool *inputs, int num_inputs);

void thermo_rev_full_adder(const bool *a, const bool *b, bool cin,
                            bool *sum, bool *cout, bool *garbage);
int  thermo_build_rev_adder(ThermoCircuit *tc, int bit_width,
                             int a_signals[], int b_signals[],
                             int sum_signals[], int cout_signal);
int  thermo_build_rev_multiplier(ThermoCircuit *tc, int bit_width,
                                  int a_signals[], int b_signals[],
                                  int prod_signals[]);

double thermo_gibbs_free_energy(double enthalpy, double entropy,
                                 double temperature);
double thermo_computational_power_dissipation(int num_ops,
                                               double temp_k, double rev_factor);
void thermo_print_energy_report(const ThermoCircuit *tc);

#endif
