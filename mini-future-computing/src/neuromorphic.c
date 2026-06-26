#include "neuromorphic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Neuromorphic Computing -- Spiking Neural Networks
 * L1: NeuroSNN, NeuroNeuron, NeuroSynapse, NeuroLayer
 * L2: LIF neuron dynamics, Izhikevich model
 * L3: Layer topology, feed-forward SNN
 * L4: Hebbian learning, STDP (Bi and Poo 1998)
 * L5: STDP learning rule, rate coding
 * L6: Pattern recognition with SNN
 * L7: Edge AI inference
 * References: Hebb (1949), Izhikevich (IEEE TNN 2003)
 */

NeuroSNN* neuro_snn_create(double dt_ms) {
    NeuroSNN *snn = (NeuroSNN*)calloc(1, sizeof(NeuroSNN));
    if (!snn) return NULL;
    snn->dt_ms = (dt_ms > 0.0) ? dt_ms : 0.1;
    snn->time_ms = 0.0;
    snn->current_timestep = 0;
    snn->spike_capacity = 4096;
    snn->spike_buffer = (NeuroSpike*)calloc((size_t)snn->spike_capacity,
                                             sizeof(NeuroSpike));
    if (!snn->spike_buffer) { free(snn); return NULL; }
    return snn;
}

void neuro_snn_destroy(NeuroSNN *snn) {
    if (!snn) return;
    free(snn->spike_buffer);
    free(snn);
}

int neuro_add_neuron(NeuroSNN *snn, NeuroNeuronModel model,
                      double v_threshold, double tau_m) {
    if (!snn || snn->num_neurons >= NEURO_MAX_NEURONS) return -1;
    int idx = snn->num_neurons++;
    NeuroNeuron *n = &snn->neurons[idx];
    memset(n, 0, sizeof(NeuroNeuron));
    n->id = idx;
    n->model = model;
    n->v_membrane = -65.0;
    n->v_rest = -65.0;
    n->v_threshold = (v_threshold > 0.0) ? v_threshold : -50.0;
    n->v_reset = -70.0;
    n->tau_m = (tau_m > 0.0) ? tau_m : 10.0;
    n->tau_ref = 2.0;
    n->R_m = 10.0;
    n->tau_adap = 100.0;
    n->adap_increment = 0.5;
    n->a = 0.02; n->b = 0.2; n->c = -65.0; n->d = 8.0;
    n->u = n->b * n->v_membrane;
    return idx;
}

int neuro_add_layer(NeuroSNN *snn, const char *name, int num_neurons,
                     bool is_input, bool is_output) {
    if (!snn || snn->num_layers >= NEURO_MAX_LAYERS) return -1;
    int lidx = snn->num_layers++;
    NeuroLayer *layer = &snn->layers[lidx];
    layer->id = lidx;
    if (name) strncpy(layer->name, name, sizeof(layer->name) - 1);
    layer->num_neurons = num_neurons;
    layer->neuron_start_idx = snn->num_neurons;
    layer->is_input = is_input;
    layer->is_output = is_output;
    return lidx;
}

int neuro_add_synapse(NeuroSNN *snn, int pre_id, int post_id,
                       double weight, double delay_ms,
                       NeuroSynapseType type) {
    if (!snn || snn->num_synapses >= NEURO_MAX_SYNAPSES) return -1;
    if (pre_id < 0 || pre_id >= snn->num_neurons) return -1;
    if (post_id < 0 || post_id >= snn->num_neurons) return -1;
    int idx = snn->num_synapses++;
    NeuroSynapse *syn = &snn->synapses[idx];
    memset(syn, 0, sizeof(NeuroSynapse));
    syn->id = idx;
    syn->pre_neuron_id = pre_id;
    syn->post_neuron_id = post_id;
    syn->weight = weight;
    syn->delay_ms = delay_ms;
    syn->type = type;
    syn->A_plus = 0.01; syn->A_minus = 0.012;
    syn->tau_plus = 20.0; syn->tau_minus = 20.0;
    syn->w_min = 0.0; syn->w_max = 10.0;
    return idx;
}

int neuro_connect_layers(NeuroSNN *snn, int layer_from, int layer_to,
                          double weight_scale, NeuroSynapseType type) {
    if (!snn) return -1;
    if (layer_from < 0 || layer_from >= snn->num_layers) return -1;
    if (layer_to < 0 || layer_to >= snn->num_layers) return -1;
    NeuroLayer *from = &snn->layers[layer_from];
    NeuroLayer *to = &snn->layers[layer_to];
    int count = 0;
    for (int i = 0; i < from->num_neurons; i++) {
        for (int j = 0; j < to->num_neurons; j++) {
            double w = ((double)rand() / (double)RAND_MAX - 0.5) * weight_scale;
            if (neuro_add_synapse(snn, from->neuron_start_idx + i,
                                  to->neuron_start_idx + j, w, 1.0, type) >= 0)
                count++;
        }
    }
    return count;
}

void neuro_step_neuron(NeuroSNN *snn, int neuron_id, double input_current) {
    if (!snn || neuron_id < 0 || neuron_id >= snn->num_neurons) return;
    NeuroNeuron *n = &snn->neurons[neuron_id];
    double dt = snn->dt_ms;

    if (n->refrac_remaining > 0) {
        n->refrac_remaining--;
        n->v_membrane = n->v_reset;
        return;
    }

    switch (n->model) {
        case NEURO_MODEL_LIF: {
            double dv = (-(n->v_membrane - n->v_rest) + n->R_m * (input_current + n->I_syn))
                        * dt / n->tau_m;
            n->v_membrane += dv;
            if (n->v_membrane >= n->v_threshold) {
                n->v_membrane = n->v_reset;
                n->refrac_remaining = (int)(n->tau_ref / dt);
                n->total_spikes++;
                n->last_spike_time = snn->time_ms;
                if (snn->spike_count < snn->spike_capacity) {
                    snn->spike_buffer[snn->spike_count].neuron_id = neuron_id;
                    snn->spike_buffer[snn->spike_count].timestep = snn->current_timestep;
                    snn->spike_buffer[snn->spike_count].timestamp_ms = snn->time_ms;
                    snn->spike_count++;
                }
            }
            break;
        }
        case NEURO_MODEL_IZHIKEVICH: {
            double v = n->v_membrane;
            double u = n->u;
            double I = input_current + n->I_syn;
            n->v_membrane += dt * (0.04 * v * v + 5.0 * v + 140.0 - u + I);
            n->u += dt * (n->a * (n->b * v - u));
            if (n->v_membrane >= 30.0) {
                n->v_membrane = n->c;
                n->u += n->d;
                n->total_spikes++;
                n->last_spike_time = snn->time_ms;
                if (snn->spike_count < snn->spike_capacity) {
                    snn->spike_buffer[snn->spike_count].neuron_id = neuron_id;
                    snn->spike_buffer[snn->spike_count].timestep = snn->current_timestep;
                    snn->spike_buffer[snn->spike_count].timestamp_ms = snn->time_ms;
                    snn->spike_count++;
                }
            }
            break;
        }
        default:
            break;
    }
}

void neuro_propagate_spikes(NeuroSNN *snn) {
    if (!snn) return;
    for (int s = 0; s < snn->num_synapses; s++) {
        NeuroSynapse *syn = &snn->synapses[s];
        NeuroNeuron *pre = &snn->neurons[syn->pre_neuron_id];
        if (pre->last_spike_time == snn->time_ms) {
            syn->last_pre_spike_time = snn->time_ms;
            NeuroNeuron *post = &snn->neurons[syn->post_neuron_id];
            post->I_syn += syn->weight;
            syn->eligibility_trace += 1.0;
        }
    }
}

void neuro_stdp_update(NeuroSNN *snn, double learning_rate) {
    if (!snn) return;
    for (int s = 0; s < snn->num_synapses; s++) {
        NeuroSynapse *syn = &snn->synapses[s];
        NeuroNeuron *pre = &snn->neurons[syn->pre_neuron_id];
        NeuroNeuron *post = &snn->neurons[syn->post_neuron_id];
        if (pre->last_spike_time == snn->time_ms ||
            post->last_spike_time == snn->time_ms) {
            double delta_t = post->last_spike_time - pre->last_spike_time;
            double dw = neuro_stdp_weight_change(delta_t, syn->A_plus,
                                                   syn->A_minus, syn->tau_plus,
                                                   syn->tau_minus);
            syn->weight += learning_rate * dw;
            if (syn->weight < syn->w_min) syn->weight = syn->w_min;
            if (syn->weight > syn->w_max) syn->weight = syn->w_max;
        }
    }
}

double neuro_stdp_weight_change(double delta_t, double A_plus, double A_minus,
                                 double tau_plus, double tau_minus) {
    if (delta_t > 0.0) return A_plus * exp(-delta_t / tau_plus);
    else if (delta_t < 0.0) return -A_minus * exp(delta_t / tau_minus);
    return 0.0;
}

void neuro_stdp_reset_traces(NeuroSNN *snn) {
    if (!snn) return;
    for (int s = 0; s < snn->num_synapses; s++)
        snn->synapses[s].eligibility_trace = 0.0;
}

void neuro_rate_encode(const double *analog_values, int num_values,
                        int *spike_train, int max_time, double max_rate) {
    if (!analog_values || !spike_train) return;
    for (int i = 0; i < num_values; i++) {
        double rate = fabs(analog_values[i]) * max_rate;
        for (int t = 0; t < max_time; t++) {
            double r = (double)rand() / (double)RAND_MAX;
            spike_train[i * max_time + t] = (r < rate / (double)max_time) ? 1 : 0;
        }
    }
}

void neuro_rate_decode(const int *spike_train, int num_neurons, int max_time,
                        double *analog_values) {
    if (!spike_train || !analog_values) return;
    for (int i = 0; i < num_neurons; i++) {
        int count = 0;
        for (int t = 0; t < max_time; t++) count += spike_train[i * max_time + t];
        analog_values[i] = (double)count / (double)max_time;
    }
}

int neuro_classify(NeuroSNN *snn, const double *input, int input_len,
                    double *confidence, int max_classes) {
    if (!snn || !input || !confidence) return -1;
    int out_layer = -1;
    for (int i = 0; i < snn->num_layers; i++)
        if (snn->layers[i].is_output) { out_layer = i; break; }
    if (out_layer < 0) return -1;

    NeuroLayer *layer = &snn->layers[out_layer];
    snn->spike_count = 0;
    for (int t = 0; t < 100; t++) {
        snn->time_ms += snn->dt_ms;
        snn->current_timestep = t;
        NeuroLayer *in_layer = NULL;
        for (int i = 0; i < snn->num_layers; i++) {
            if (snn->layers[i].is_input) { in_layer = &snn->layers[i]; break; }
        }
        if (in_layer) {
            for (int i = 0; i < in_layer->num_neurons && i < input_len; i++)
                neuro_step_neuron(snn, in_layer->neuron_start_idx + i,
                                  input[i] * 10.0);
        }
        neuro_propagate_spikes(snn);
    }

    int best_class = 0;
    int max_spikes = 0;
    int n_out = (layer->num_neurons < max_classes) ? layer->num_neurons : max_classes;
    for (int i = 0; i < n_out; i++) {
        int spikes = snn->neurons[layer->neuron_start_idx + i].total_spikes;
        confidence[i] = (double)spikes / 100.0;
        if (spikes > max_spikes) { max_spikes = spikes; best_class = i; }
    }
    return best_class;
}

int neuro_get_spikes(NeuroSNN *snn, int *neuron_ids, int max_spikes) {
    if (!snn || !neuron_ids) return 0;
    int count = (snn->spike_count < max_spikes) ? snn->spike_count : max_spikes;
    for (int i = 0; i < count; i++)
        neuron_ids[i] = snn->spike_buffer[i].neuron_id;
    return count;
}

void neuro_print_network(const NeuroSNN *snn) {
    if (!snn) return;
    printf("SNN: %d neurons, %d synapses, %d layers, dt=%.2fms\n",
           snn->num_neurons, snn->num_synapses, snn->num_layers, snn->dt_ms);
    for (int i = 0; i < snn->num_layers; i++) {
        NeuroLayer *l = &snn->layers[i];
        printf("  Layer %d '%s': %d neurons %s%s\n",
               l->id, l->name, l->num_neurons,
               l->is_input ? "[INPUT]" : "", l->is_output ? "[OUTPUT]" : "");
    }
}

double neuro_total_synapses_weight(const NeuroSNN *snn) {
    if (!snn) return 0.0;
    double total = 0.0;
    for (int i = 0; i < snn->num_synapses; i++)
        total += snn->synapses[i].weight;
    return total;
}

void neuro_step_layer(NeuroSNN *snn, int layer_id, const double *inputs) {
    if (!snn || !inputs) return;
    if (layer_id < 0 || layer_id >= snn->num_layers) return;
    NeuroLayer *layer = &snn->layers[layer_id];
    for (int i = 0; i < layer->num_neurons; i++) {
        double current = (i < NEURO_MAX_INPUTS) ? inputs[i] : 0.0;
        neuro_step_neuron(snn, layer->neuron_start_idx + i, current);
    }
}

void neuro_step_network(NeuroSNN *snn, const double *inputs, int input_len) {
    if (!snn || !inputs) return;
    for (int l = 0; l < snn->num_layers; l++) {
        if (snn->layers[l].is_input) {
            neuro_step_layer(snn, l, inputs);
        } else {
            for (int i = 0; i < snn->layers[l].num_neurons; i++) {
                neuro_step_neuron(snn, snn->layers[l].neuron_start_idx + i, 0.0);
            }
        }
    }
    neuro_propagate_spikes(snn);
}

int neuro_train_pattern(NeuroSNN *snn, const double *pattern, int pattern_len,
                         int label, int epochs) {
    if (!snn || !pattern || pattern_len < 1 || epochs < 1) return -1;
    for (int e = 0; e < epochs; e++) {
        neuro_step_network(snn, pattern, pattern_len);
        neuro_stdp_update(snn, 0.01);
        snn->time_ms += snn->dt_ms;
        snn->current_timestep++;
    }
    return 0;
}

double neuro_compute_accuracy(const NeuroSNN *snn, const double *inputs,
                               const int *labels, int num_samples, int input_len) {
    if (!snn || !inputs || !labels || num_samples < 1) return 0.0;
    int correct = 0;
    double confidence[16];
    for (int i = 0; i < num_samples; i++) {
        int cls = neuro_classify((NeuroSNN*)snn, &inputs[i * input_len],
                                  input_len, confidence, 16);
        if (cls == labels[i]) correct++;
    }
    return (double)correct / (double)num_samples;
}

int neuro_neuron_count_by_model(const NeuroSNN *snn, NeuroNeuronModel model) {
    if (!snn) return 0;
    int count = 0;
    for (int i = 0; i < snn->num_neurons; i++)
        if (snn->neurons[i].model == model) count++;
    return count;
}
