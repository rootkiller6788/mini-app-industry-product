#include "neuromorphic.h"
#include "probabilistic_compute.h"
#include "bio_compute.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    srand((unsigned)time(NULL));
    printf("=== Emerging Computing Demo ===\n\n");

    /* SNN Demo */
    printf("--- Neuromorphic SNN ---\n");
    NeuroSNN *snn = neuro_snn_create(0.1);
    neuro_add_layer(snn, "input", 4, true, false);
    for (int i = 0; i < 4; i++) neuro_add_neuron(snn, NEURO_MODEL_LIF, -50.0, 10.0);
    neuro_add_layer(snn, "hidden", 8, false, false);
    for (int i = 0; i < 8; i++) neuro_add_neuron(snn, NEURO_MODEL_LIF, -50.0, 10.0);
    neuro_add_layer(snn, "output", 3, false, true);
    for (int i = 0; i < 3; i++) neuro_add_neuron(snn, NEURO_MODEL_LIF, -50.0, 10.0);
    neuro_connect_layers(snn, 0, 1, 0.5, NEURO_SYN_EXCITATORY);
    neuro_connect_layers(snn, 1, 2, 0.5, NEURO_SYN_EXCITATORY);
    neuro_print_network(snn);

    double input[] = {0.8, 0.3, 0.6, 0.1};
    double confidence[3];
    int cls = neuro_classify(snn, input, 4, confidence, 3);
    printf("Classification result: class %d\n", cls);
    printf("Confidence: %.3f, %.3f, %.3f\n",
           confidence[0], confidence[1], confidence[2]);
    neuro_snn_destroy(snn);

    /* Probabilistic Demo */
    printf("\n--- Probabilistic Inference ---\n");
    BayesianNetwork *bn = prob_bn_create();
    prob_bn_add_node(bn, "Flu", 2);
    prob_bn_add_node(bn, "Fever", 2);
    prob_bn_add_node(bn, "Cough", 2);
    prob_bn_add_edge(bn, 0, 1);
    prob_bn_add_edge(bn, 0, 2);

    double flu_cpt[] = {0.05, 0.95};
    prob_bn_set_cpt(bn, 0, flu_cpt, 2);
    double fever_cpt[] = {0.9, 0.1, 0.05, 0.95};
    prob_bn_set_cpt(bn, 1, fever_cpt, 4);
    double cough_cpt[] = {0.8, 0.2, 0.1, 0.9};
    prob_bn_set_cpt(bn, 2, cough_cpt, 4);

    prob_bn_set_evidence(bn, 1, 0);
    prob_bn_belief_propagation(bn, 100);
    printf("P(Flu | Fever=Yes) = %.4f\n", prob_bn_posterior(bn, 0, 0));
    printf("P(Cough | Fever=Yes) = %.4f\n", prob_bn_posterior(bn, 2, 0));
    prob_bn_destroy(bn);

    /* DNA Storage Demo */
    printf("\n--- DNA Data Storage ---\n");
    uint8_t msg[] = {'H','e','l','l','o'};
    char dna[256];
    bio_encode_binary(msg, 5, dna, 256);
    printf("Binary data encoded to DNA: %s\n", dna);

    uint8_t decoded[10];
    int bytes = bio_decode_binary(dna, decoded, 10);
    printf("Decoded: ");
    for (int i = 0; i < bytes && i < 5; i++) printf("%c", decoded[i]);
    printf("\nGC diversity: encoding uses 4-base alphabet\n");

    return 0;
}
