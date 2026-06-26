#ifndef MINI_FUTURE_NEUROMORPHIC_H
#define MINI_FUTURE_NEUROMORPHIC_H

#include <stdbool.h>
#include <stdint.h>

/* Neuromorphic Computing -- Spiking Neural Networks (SNN)
 * L1: SNNeuron, SNNSynapse, SNNLayer structs
 * L2: Leaky Integrate-and-Fire (LIF), Izhikevich, Hodgkin-Huxley models
 * L3: Layer topology, feed-forward SNN, recurrent SNN
 * L4: Hebbian learning (Hebb 1949), STDP (Bi & Poo 1998)
 * L5: STDP learning rule, rate coding, temporal coding
 * L6: Pattern recognition with SNN
 * L7: Edge AI inference, sensor fusion
 * L8: Loihi (Intel), TrueNorth (IBM) architectures
 * L9: Neuromorphic chips roadmap, memristor crossbar integration
 * References:
 * - Hebb, "The Organization of Behavior" (1949)
 * - Hodgkin & Huxley, "A quantitative description of membrane current" (1952)
 * - Izhikevich, "Simple model of spiking neurons" (IEEE TNN 2003)
 * - Mead, "Neuromorphic electronic systems" (Proc. IEEE 1990)
 */

#define NEURO_MAX_NEURONS     1024
#define NEURO_MAX_SYNAPSES    16384
#define NEURO_MAX_LAYERS      32
#define NEURO_MAX_TIMESTEPS   10000
#define NEURO_MAX_INPUTS      256

typedef enum {
    NEURO_MODEL_LIF         = 0,
    NEURO_MODEL_IZHIKEVICH  = 1,
    NEURO_MODEL_THRESHOLD   = 2,
    NEURO_MODEL_RESONATE    = 3
} NeuroNeuronModel;

typedef enum {
    NEURO_SYN_EXCITATORY = 0,
    NEURO_SYN_INHIBITORY = 1,
    NEURO_SYN_MODULATORY = 2
} NeuroSynapseType;

typedef struct {
    int neuron_id;
    int timestep;
    double timestamp_ms;
} NeuroSpike;

/* LIF neuron: tau_m * dV/dt = -(V - V_rest) + R_m * I_syn */
typedef struct {
    int id;
    NeuroNeuronModel model;
    double v_membrane;
    double v_rest;
    double v_threshold;
    double v_reset;
    double tau_m;
    double tau_ref;
    double R_m;
    double I_syn;
    int refrac_remaining;
    int total_spikes;
    double last_spike_time;
    double adaptation;
    double tau_adap;
    double adap_increment;
    double u;
    double a, b, c, d;
} NeuroNeuron;

/* STDP: Delta_w = A+ * exp(-delta_t/tau+) for delta_t > 0 */
typedef struct {
    int id;
    int pre_neuron_id;
    int post_neuron_id;
    double weight;
    double delay_ms;
    NeuroSynapseType type;
    double A_plus;
    double A_minus;
    double tau_plus;
    double tau_minus;
    double w_min;
    double w_max;
    double last_pre_spike_time;
    double last_post_spike_time;
    double eligibility_trace;
} NeuroSynapse;

typedef struct {
    int id;
    char name[32];
    int num_neurons;
    int neuron_start_idx;
    bool is_input;
    bool is_output;
} NeuroLayer;

typedef struct {
    NeuroNeuron neurons[NEURO_MAX_NEURONS];
    int num_neurons;
    NeuroSynapse synapses[NEURO_MAX_SYNAPSES];
    int num_synapses;
    NeuroLayer layers[NEURO_MAX_LAYERS];
    int num_layers;
    NeuroSpike *spike_buffer;
    int spike_count;
    int spike_capacity;
    double time_ms;
    int current_timestep;
    double dt_ms;
    bool has_run;
} NeuroSNN;

NeuroSNN* neuro_snn_create(double dt_ms);
void neuro_snn_destroy(NeuroSNN *snn);
int  neuro_add_neuron(NeuroSNN *snn, NeuroNeuronModel model,
                       double v_threshold, double tau_m);
int  neuro_add_layer(NeuroSNN *snn, const char *name, int num_neurons,
                      bool is_input, bool is_output);
int  neuro_add_synapse(NeuroSNN *snn, int pre_id, int post_id,
                        double weight, double delay_ms,
                        NeuroSynapseType type);
int  neuro_connect_layers(NeuroSNN *snn, int layer_from, int layer_to,
                           double weight_scale, NeuroSynapseType type);
void neuro_step_neuron(NeuroSNN *snn, int neuron_id, double input_current);
void neuro_step_layer(NeuroSNN *snn, int layer_id, const double *inputs);
void neuro_step_network(NeuroSNN *snn, const double *inputs, int input_len);
void neuro_propagate_spikes(NeuroSNN *snn);
int  neuro_get_spikes(NeuroSNN *snn, int *neuron_ids, int max_spikes);
void neuro_stdp_update(NeuroSNN *snn, double learning_rate);
void neuro_stdp_reset_traces(NeuroSNN *snn);
double neuro_stdp_weight_change(double delta_t, double A_plus, double A_minus,
                                 double tau_plus, double tau_minus);
void neuro_rate_encode(const double *analog_values, int num_values,
                        int *spike_train, int max_time, double max_rate);
void neuro_rate_decode(const int *spike_train, int num_neurons, int max_time,
                        double *analog_values);
int  neuro_train_pattern(NeuroSNN *snn, const double *pattern, int pattern_len,
                          int label, int epochs);
int  neuro_classify(NeuroSNN *snn, const double *input, int input_len,
                     double *confidence, int max_classes);
double neuro_compute_accuracy(const NeuroSNN *snn, const double *inputs,
                               const int *labels, int num_samples, int input_len);
void neuro_print_network(const NeuroSNN *snn);
double neuro_total_synapses_weight(const NeuroSNN *snn);
int  neuro_neuron_count_by_model(const NeuroSNN *snn, NeuroNeuronModel model);

#endif
