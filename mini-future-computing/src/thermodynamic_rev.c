#include "thermodynamic_rev.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Thermodynamic & Reversible Computing
 * L1: ThermoCircuit, ReversibleGate, AdiabaticStage
 * L2: Landauer principle (kT*ln2), adiabatic switching
 * L3: Adiabatic pipeline, Bennett clocking
 * L4: Thermodynamics of computation
 * L5: Adiabatic CMOS, energy recovery
 * L6: Reversible adder
 * References: Landauer (1961), Bennett (1973), Fredkin & Toffoli (1982)
 */

/* ---- Signal & Circuit Management ---- */

ThermoCircuit* thermo_circuit_create(double temperature_k) {
    ThermoCircuit *tc = (ThermoCircuit*)calloc(1, sizeof(ThermoCircuit));
    if (!tc) return NULL;
    tc->temperature_k = (temperature_k > 0.0) ? temperature_k : 300.0;
    tc->reversible_factor = 0.0;
    return tc;
}

void thermo_circuit_destroy(ThermoCircuit *tc) {
    if (!tc) return;
    for (int i = 0; i < tc->num_stages; i++)
        free(tc->stages[i].gates);
    free(tc);
}

int thermo_add_signal(ThermoCircuit *tc, bool initial_value) {
    if (!tc || tc->num_signals >= THERMO_MAX_SIGNALS) return -1;
    int idx = tc->num_signals++;
    tc->signals[idx].id = idx;
    tc->signals[idx].value = initial_value;
    tc->signals[idx].energy_joules = 0.0;
    return idx;
}

void thermo_set_signal(ThermoCircuit *tc, int signal_id, bool value) {
    if (!tc || signal_id < 0 || signal_id >= tc->num_signals) return;
    tc->signals[signal_id].value = value;
}

bool thermo_get_signal(const ThermoCircuit *tc, int signal_id) {
    if (!tc || signal_id < 0 || signal_id >= tc->num_signals) return false;
    return tc->signals[signal_id].value;
}

/* ---- Reversible Gates (L2) ---- */

void thermo_rev_not(const bool *in, bool *out) {
    out[0] = !in[0];
}

void thermo_rev_cnot(const bool *in, bool *out) {
    out[0] = in[0];
    out[1] = in[0] ^ in[1];
}

void thermo_rev_toffoli(const bool *in, bool *out) {
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2] ^ (in[0] & in[1]);
}

void thermo_rev_fredkin(const bool *in, bool *out) {
    out[0] = in[0];
    if (in[0]) {
        out[1] = in[2];
        out[2] = in[1];
    } else {
        out[1] = in[1];
        out[2] = in[2];
    }
}

void thermo_rev_swap(const bool *in, bool *out) {
    out[0] = in[1];
    out[1] = in[0];
}

int thermo_add_gate(ThermoCircuit *tc, ReversibleGateType type,
                     int in0, int in1, int in2, int out0, int out1, int out2) {
    if (!tc || tc->num_gates >= THERMO_MAX_GATES) return -1;
    int idx = tc->num_gates++;
    ReversibleGate *g = &tc->gates[idx];
    memset(g, 0, sizeof(ReversibleGate));
    g->id = idx;
    g->type = type;
    g->input_signals[0] = in0;
    g->input_signals[1] = in1;
    g->input_signals[2] = in2;
    g->output_signals[0] = out0;
    g->output_signals[1] = out1;
    g->output_signals[2] = out2;
    g->num_signals = (type == REV_TOFFOLI || type == REV_FREDKIN) ? 3 :
                     (type == REV_CNOT || type == REV_SWAP) ? 2 : 1;
    g->delay_ps = 10.0;
    return idx;
}

int thermo_evaluate_gate(ThermoCircuit *tc, int gate_id) {
    if (!tc || gate_id < 0 || gate_id >= tc->num_gates) return -1;
    ReversibleGate *g = &tc->gates[gate_id];
    bool in[3] = {false, false, false};
    bool out[3] = {false, false, false};

    for (int i = 0; i < g->num_signals; i++)
        if (g->input_signals[i] >= 0 && g->input_signals[i] < tc->num_signals)
            in[i] = tc->signals[g->input_signals[i]].value;

    switch (g->type) {
        case REV_NOT:     thermo_rev_not(in, out); break;
        case REV_CNOT:    thermo_rev_cnot(in, out); break;
        case REV_TOFFOLI: thermo_rev_toffoli(in, out); break;
        case REV_FREDKIN: thermo_rev_fredkin(in, out); break;
        case REV_SWAP:    thermo_rev_swap(in, out); break;
        default: return -1;
    }

    for (int i = 0; i < g->num_signals; i++)
        if (g->output_signals[i] >= 0 && g->output_signals[i] < tc->num_signals)
            tc->signals[g->output_signals[i]].value = out[i];

    g->switching_energy = thermo_landauer_energy(g->num_signals, tc->temperature_k);
    tc->total_energy_joules += g->switching_energy;
    return 0;
}

void thermo_evaluate_all(ThermoCircuit *tc) {
    if (!tc) return;
    for (int i = 0; i < tc->num_gates; i++)
        thermo_evaluate_gate(tc, i);
}

/* ---- L4: Energy Analysis ---- */

double thermo_landauer_energy(int num_bits_erased, double temperature_k) {
    if (num_bits_erased < 1 || temperature_k <= 0.0) return 0.0;
    return (double)num_bits_erased * THERMO_BOLTZMANN * temperature_k * log(2.0);
}

double thermo_min_switching_energy(double capacitance_fF, double voltage_V,
                                    bool adiabatic) {
    double C = capacitance_fF * 1e-15;
    double E_cmos = 0.5 * C * voltage_V * voltage_V;
    if (adiabatic) {
        double T = 1e-9;
        double R = 1e4;
        return (R * C / T) * C * voltage_V * voltage_V;
    }
    return E_cmos;
}

double thermo_reversible_energy_saved(const ThermoCircuit *tc) {
    if (!tc) return 0.0;
    double irreversible = (double)tc->num_gates *
        thermo_landauer_energy(1, tc->temperature_k);
    double reversible = irreversible * tc->reversible_factor;
    return irreversible - reversible;
}

double thermo_cmos_energy_comparison(int num_gates, double Vdd,
                                      double C_load, double frequency) {
    double E_per_gate = C_load * Vdd * Vdd * 0.5;
    return E_per_gate * (double)num_gates * frequency;
}

/* ---- L5: Adiabatic Pipeline ---- */

int thermo_add_stage(ThermoCircuit *tc, double clock_period_ps) {
    if (!tc || tc->num_stages >= THERMO_MAX_STAGES) return -1;
    int idx = tc->num_stages++;
    AdiabaticStage *st = &tc->stages[idx];
    st->id = idx;
    st->clock_period_ps = clock_period_ps;
    st->cap_gates = 16;
    st->gates = (ReversibleGate*)calloc((size_t)st->cap_gates, sizeof(ReversibleGate));
    return idx;
}

int thermo_pipeline_step(ThermoCircuit *tc, const bool *inputs, int num_inputs) {
    if (!tc) return -1;
    for (int i = 0; i < num_inputs && i < tc->num_signals; i++)
        tc->signals[i].value = inputs[i];

    for (int s = 0; s < tc->num_stages; s++) {
        AdiabaticStage *st = &tc->stages[s];
        for (int g = 0; g < st->num_gates; g++)
            thermo_evaluate_gate(tc, st->gates[g].id);
        st->energy_per_cycle = tc->total_energy_joules;
    }
    return 0;
}

int thermo_full_pipeline(ThermoCircuit *tc, const bool *inputs, int num_inputs) {
    return thermo_pipeline_step(tc, inputs, num_inputs);
}

/* ---- L6: Reversible Adder ---- */

void thermo_rev_full_adder(const bool *a, const bool *b, bool cin,
                            bool *sum, bool *cout, bool *garbage) {
    *sum = a[0] ^ b[0] ^ cin;
    *cout = (a[0] & b[0]) | (a[0] & cin) | (b[0] & cin);
    *garbage = a[0] ^ b[0];
}

int thermo_build_rev_adder(ThermoCircuit *tc, int bit_width,
                            int a_signals[], int b_signals[],
                            int sum_signals[], int cout_signal) {
    if (!tc || bit_width < 1) return -1;
    int carry_id = thermo_add_signal(tc, false);

    for (int i = 0; i < bit_width; i++) {
        if (a_signals[i] < 0 || b_signals[i] < 0) return -1;
        int s_id = (sum_signals[i] >= 0) ? sum_signals[i] : thermo_add_signal(tc, false);
        int tmp1 = thermo_add_signal(tc, false);
        int tmp2 = thermo_add_signal(tc, false);

        thermo_add_gate(tc, REV_CNOT, a_signals[i], b_signals[i], -1, a_signals[i], tmp1, -1);
        thermo_add_gate(tc, REV_CNOT, tmp1, carry_id, -1, tmp1, tmp2, -1);
        thermo_add_gate(tc, REV_CNOT, tmp2, s_id, -1, tmp2, s_id, -1);
        thermo_add_gate(tc, REV_TOFFOLI, a_signals[i], b_signals[i], carry_id,
                        a_signals[i], b_signals[i], carry_id);
        if (sum_signals[i] < 0) sum_signals[i] = s_id;
    }
    tc->signals[cout_signal].value = tc->signals[carry_id].value;
    return 0;
}

/* ---- Utility ---- */

double thermo_gibbs_free_energy(double enthalpy, double entropy,
                                 double temperature) {
    return enthalpy - temperature * entropy;
}

double thermo_computational_power_dissipation(int num_ops,
                                               double temp_k, double rev_factor) {
    double irreversible = thermo_landauer_energy(num_ops, temp_k);
    return irreversible * (1.0 - rev_factor) + irreversible * 0.01 * rev_factor;
}

void thermo_print_energy_report(const ThermoCircuit *tc) {
    if (!tc) return;
    printf("=== Thermodynamic Energy Report ===\n");
    printf("Temperature: %.1f K\n", tc->temperature_k);
    printf("Signals: %d, Gates: %d, Stages: %d\n",
           tc->num_signals, tc->num_gates, tc->num_stages);
    printf("Landauer limit: %.3e J/bit\n",
           thermo_landauer_energy(1, tc->temperature_k));
    printf("Total energy: %.3e J\n", tc->total_energy_joules);
    printf("Reversible factor: %.2f\n", tc->reversible_factor);
    double cmos_equiv = thermo_cmos_energy_comparison(tc->num_gates, 1.0, 1e-12, 1e9);
    printf("Equivalent CMOS energy: %.3e J\n", cmos_equiv);
    printf("Energy savings: %.1fx\n",
           cmos_equiv / (tc->total_energy_joules + 1e-30));
}
