#include "probabilistic_compute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Probabilistic Computing -- Stochastic & Bayesian Computation
 * L1: StochasticBit, BayesianNetwork, MCMCSampler
 * L2: Stochastic computing, Bayesian inference
 * L4: Bayes theorem, central limit theorem
 * L5: Metropolis-Hastings, Gibbs sampling, belief propagation
 * L6: Bayesian diagnosis
 * References: Bayes (1763), Metropolis et al. (1953), Pearl (1988)
 */

/* ---- Stochastic Bit (L2) ---- */

StochasticBit* prob_bit_create(double probability) {
    StochasticBit *bit = (StochasticBit*)calloc(1, sizeof(StochasticBit));
    if (!bit) return NULL;
    bit->probability = (probability < 0.0) ? 0.0 : ((probability > 1.0) ? 1.0 : probability);
    bit->temperature = 1.0;
    bit->history_cap = 256;
    bit->history = (double*)calloc((size_t)bit->history_cap, sizeof(double));
    return bit;
}

void prob_bit_destroy(StochasticBit *bit) {
    if (bit) { free(bit->history); free(bit); }
}

int prob_bit_sample(StochasticBit *bit) {
    if (!bit) return 0;
    double r = (double)rand() / (double)RAND_MAX;
    double p = 1.0 / (1.0 + exp(-bit->bias / bit->temperature));
    bit->last_output = (r < p) ? 1 : 0;
    if (bit->history_len < bit->history_cap) {
        bit->history[bit->history_len++] = (double)bit->last_output;
    }
    return bit->last_output;
}

int prob_bit_sample_n(StochasticBit *bit, int n, int *results) {
    if (!bit || !results || n < 1) return -1;
    int count = 0;
    for (int i = 0; i < n; i++) {
        results[i] = prob_bit_sample(bit);
        count += results[i];
    }
    return count;
}

double prob_bit_estimate_probability(const StochasticBit *bit) {
    if (!bit || bit->history_len < 1) return bit ? bit->probability : 0.0;
    double sum = 0.0;
    for (int i = 0; i < bit->history_len; i++) sum += bit->history[i];
    return sum / (double)bit->history_len;
}

/* ---- Stochastic Arithmetic (L2) ---- */

double prob_sadd(double pa, double pb) {
    double p = pa + pb;
    return (p > 1.0) ? 1.0 : ((p < 0.0) ? 0.0 : p);
}

double prob_smul(double pa, double pb) {
    return pa * pb;
}

double prob_sdiv(double pa, double pb) {
    if (pb < 1e-10) return 0.0;
    double p = pa / pb;
    return (p > 1.0) ? 1.0 : p;
}

double prob_sneg(double pa) { return 1.0 - pa; }

double prob_binary_to_stochastic(int value, int bit_width) {
    if (bit_width < 1) return 0.0;
    return (double)value / (double)((1 << bit_width) - 1);
}

int prob_stochastic_to_binary(double p, int bit_width) {
    if (bit_width < 1) return 0;
    int max_val = (1 << bit_width) - 1;
    int result = (int)(p * (double)max_val + 0.5);
    return (result > max_val) ? max_val : ((result < 0) ? 0 : result);
}

/* ---- Bayesian Network (L3) ---- */

BayesianNetwork* prob_bn_create(void) {
    return (BayesianNetwork*)calloc(1, sizeof(BayesianNetwork));
}

void prob_bn_destroy(BayesianNetwork *bn) {
    if (!bn) return;
    for (int i = 0; i < bn->num_nodes; i++) {
        free(bn->nodes[i].cpt);
        free(bn->nodes[i].parent_ids);
        free(bn->nodes[i].marginal);
    }
    free(bn);
}

int prob_bn_add_node(BayesianNetwork *bn, const char *name, int num_states) {
    if (!bn || !name || bn->num_nodes >= PROB_MAX_NODES) return -1;
    if (num_states < 2 || num_states > PROB_MAX_STATES) return -1;
    int idx = bn->num_nodes++;
    BayesianNode *node = &bn->nodes[idx];
    memset(node, 0, sizeof(BayesianNode));
    node->id = idx;
    strncpy(node->name, name, PROB_NAME_LEN - 1);
    node->num_states = num_states;
    node->marginal = (double*)calloc((size_t)num_states, sizeof(double));
    return idx;
}

int prob_bn_add_edge(BayesianNetwork *bn, int parent_id, int child_id) {
    if (!bn || parent_id < 0 || parent_id >= bn->num_nodes ||
        child_id < 0 || child_id >= bn->num_nodes) return -1;
    if (bn->num_edges >= PROB_MAX_EDGES) return -1;
    bn->edge_from[bn->num_edges] = parent_id;
    bn->edge_to[bn->num_edges] = child_id;
    bn->num_edges++;

    BayesianNode *child = &bn->nodes[child_id];
    child->parent_ids = (int*)realloc(child->parent_ids,
        (size_t)(child->num_parents + 1) * sizeof(int));
    child->parent_ids[child->num_parents++] = parent_id;
    return 0;
}

int prob_bn_set_cpt(BayesianNetwork *bn, int node_id,
                     const double *cpt, int cpt_len) {
    if (!bn || !cpt || node_id < 0 || node_id >= bn->num_nodes) return -1;
    BayesianNode *node = &bn->nodes[node_id];
    free(node->cpt);
    node->cpt = (double*)malloc((size_t)cpt_len * sizeof(double));
    if (!node->cpt) return -1;
    memcpy(node->cpt, cpt, (size_t)cpt_len * sizeof(double));
    return 0;
}

int prob_bn_set_evidence(BayesianNetwork *bn, int node_id, int state) {
    if (!bn || node_id < 0 || node_id >= bn->num_nodes) return -1;
    if (state < 0 || state >= bn->nodes[node_id].num_states) return -1;
    bn->nodes[node_id].is_evidence = true;
    bn->nodes[node_id].evidence_state = state;
    return 0;
}

int prob_bn_clear_evidence(BayesianNetwork *bn, int node_id) {
    if (!bn || node_id < 0 || node_id >= bn->num_nodes) return -1;
    bn->nodes[node_id].is_evidence = false;
    return 0;
}

/* ---- L5: Belief Propagation ---- */

int prob_bn_belief_propagation(BayesianNetwork *bn, int max_iterations) {
    if (!bn) return -1;
    for (int i = 0; i < bn->num_nodes; i++) {
        BayesianNode *node = &bn->nodes[i];
        for (int s = 0; s < node->num_states; s++) {
            if (node->is_evidence) {
                node->marginal[s] = (s == node->evidence_state) ? 1.0 : 0.0;
            } else if (node->num_parents == 0) {
                node->marginal[s] = 1.0 / (double)node->num_states;
            }
        }
    }
    for (int iter = 0; iter < max_iterations; iter++) {
        double max_change = 0.0;
        for (int i = 0; i < bn->num_nodes; i++) {
            BayesianNode *node = &bn->nodes[i];
            if (node->is_evidence || node->num_parents == 0) continue;
            double old_marginal[PROB_MAX_STATES];
            memcpy(old_marginal, node->marginal,
                   (size_t)node->num_states * sizeof(double));

            for (int s = 0; s < node->num_states; s++) node->marginal[s] = 0.0;

            int parent_configs = 1;
            for (int p = 0; p < node->num_parents; p++)
                parent_configs *= bn->nodes[node->parent_ids[p]].num_states;

            for (int pc = 0; pc < parent_configs; pc++) {
                double prob = 1.0;
                int tmp = pc;
                for (int p = 0; p < node->num_parents; p++) {
                    int pstate = tmp % bn->nodes[node->parent_ids[p]].num_states;
                    prob *= bn->nodes[node->parent_ids[p]].marginal[pstate];
                    tmp /= bn->nodes[node->parent_ids[p]].num_states;
                }
                for (int s = 0; s < node->num_states; s++) {
                    int cpt_idx = pc * node->num_states + s;
                    if (node->cpt) node->marginal[s] += prob * node->cpt[cpt_idx];
                }
            }
            double sum = 0.0;
            for (int s = 0; s < node->num_states; s++) sum += node->marginal[s];
            if (sum > 1e-10) {
                for (int s = 0; s < node->num_states; s++) node->marginal[s] /= sum;
            }
            double change = 0.0;
            for (int s = 0; s < node->num_states; s++)
                change += fabs(node->marginal[s] - old_marginal[s]);
            if (change > max_change) max_change = change;
        }
        if (max_change < 1e-6) break;
    }
    return 0;
}

double prob_bn_posterior(const BayesianNetwork *bn, int node_id, int state) {
    if (!bn || node_id < 0 || node_id >= bn->num_nodes ||
        state < 0 || state >= bn->nodes[node_id].num_states) return -1.0;
    return bn->nodes[node_id].marginal[state];
}

int prob_bn_map_query(BayesianNetwork *bn, int *best_states) {
    if (!bn || !best_states) return -1;
    for (int i = 0; i < bn->num_nodes; i++) {
        BayesianNode *node = &bn->nodes[i];
        int best_s = 0;
        for (int s = 1; s < node->num_states; s++) {
            if (node->marginal[s] > node->marginal[best_s]) best_s = s;
        }
        best_states[i] = best_s;
    }
    return 0;
}

/* ---- MCMC Sampling (L5) ---- */

MCMCSampler* prob_mcmc_create(int dim, MCMCAlgorithm algo, double step_size) {
    MCMCSampler *mcmc = (MCMCSampler*)calloc(1, sizeof(MCMCSampler));
    if (!mcmc) return NULL;
    mcmc->dim = dim;
    mcmc->algorithm = algo;
    mcmc->step_size = step_size;
    mcmc->burn_in = 0.3;
    mcmc->capacity = PROB_MAX_SAMPLES;
    mcmc->samples = (MarkovState*)calloc((size_t)mcmc->capacity, sizeof(MarkovState));
    for (int i = 0; i < mcmc->capacity; i++) {
        mcmc->samples[i].state = (double*)calloc((size_t)dim, sizeof(double));
    }
    return mcmc;
}

void prob_mcmc_destroy(MCMCSampler *mcmc) {
    if (!mcmc) return;
    for (int i = 0; i < mcmc->capacity; i++) free(mcmc->samples[i].state);
    free(mcmc->samples);
    free(mcmc);
}

int prob_mcmc_sample(MCMCSampler *mcmc,
                      double (*log_prob)(const double *x, int dim),
                      const double *initial, int num_samples) {
    if (!mcmc || !log_prob || !initial || num_samples < 1) return -1;
    mcmc->num_samples = 0;
    mcmc->total_proposals = 0;
    mcmc->total_accepts = 0;

    double *current = (double*)malloc((size_t)mcmc->dim * sizeof(double));
    double *proposal = (double*)malloc((size_t)mcmc->dim * sizeof(double));
    if (!current || !proposal) { free(current); free(proposal); return -1; }

    memcpy(current, initial, (size_t)mcmc->dim * sizeof(double));
    double cur_logp = log_prob(current, mcmc->dim);

    int total_iter = (int)((double)num_samples / (1.0 - mcmc->burn_in));
    for (int iter = 0; iter < total_iter && mcmc->num_samples < num_samples; iter++) {
        for (int d = 0; d < mcmc->dim; d++)
            proposal[d] = current[d] + mcmc->step_size * ((double)rand() / RAND_MAX - 0.5) * 2.0;

        double prop_logp = log_prob(proposal, mcmc->dim);
        mcmc->total_proposals++;

        double log_alpha = prop_logp - cur_logp;
        if (log_alpha >= 0.0 || log((double)rand() / RAND_MAX) < log_alpha) {
            memcpy(current, proposal, (size_t)mcmc->dim * sizeof(double));
            cur_logp = prop_logp;
            mcmc->total_accepts++;
        }

        if (iter >= (int)((double)total_iter * mcmc->burn_in)) {
            int si = mcmc->num_samples++;
            memcpy(mcmc->samples[si].state, current,
                   (size_t)mcmc->dim * sizeof(double));
            mcmc->samples[si].log_prob = cur_logp;
            mcmc->samples[si].accepted = true;
        }
    }
    mcmc->acceptance_rate = (double)mcmc->total_accepts / (double)mcmc->total_proposals;
    free(current);
    free(proposal);
    return mcmc->num_samples;
}

double prob_mcmc_ess(const MCMCSampler *mcmc) {
    if (!mcmc || mcmc->num_samples < 2) return 0.0;
    /* Effective sample size: N / (1 + 2*sum(autocorr)) */
    int lag = (mcmc->num_samples > 100) ? 100 : mcmc->num_samples / 2;
    double sum_ac = 0.0;
    for (int k = 1; k < lag; k++) {
        double ac = 0.0;
        for (int i = 0; i < mcmc->num_samples - k; i++) {
            ac += (mcmc->samples[i].state[0] * mcmc->samples[i + k].state[0]);
        }
        ac /= (double)(mcmc->num_samples - k);
        sum_ac += ac;
    }
    return (double)mcmc->num_samples / (1.0 + 2.0 * sum_ac);
}

void prob_mcmc_summary(const MCMCSampler *mcmc, double *mean,
                        double *std, int dim) {
    if (!mcmc || !mean || !std) return;
    int d = (dim < mcmc->dim) ? dim : mcmc->dim;
    for (int j = 0; j < d; j++) {
        double sum = 0.0, sum2 = 0.0;
        for (int i = 0; i < mcmc->num_samples; i++) {
            double v = mcmc->samples[i].state[j];
            sum += v;
            sum2 += v * v;
        }
        mean[j] = sum / (double)mcmc->num_samples;
        double variance = sum2 / (double)mcmc->num_samples - mean[j] * mean[j];
        std[j] = (variance > 0.0) ? sqrt(variance) : 0.0;
    }
}

/* ---- Probability Distributions ---- */

double prob_gaussian_pdf(double x, double mu, double sigma) {
    double z = (x - mu) / sigma;
    return exp(-0.5 * z * z) / (sigma * sqrt(2.0 * M_PI));
}

double prob_gaussian_log_pdf(double x, double mu, double sigma) {
    double z = (x - mu) / sigma;
    return -0.5 * z * z - log(sigma * sqrt(2.0 * M_PI));
}

double prob_beta_pdf(double x, double alpha, double beta) {
    if (x < 0.0 || x > 1.0) return 0.0;
    return pow(x, alpha - 1.0) * pow(1.0 - x, beta - 1.0);
}

double prob_dirichlet_pdf(const double *x, const double *alpha, int k) {
    double sum_alpha = 0.0, log_p = 0.0;
    for (int i = 0; i < k; i++) {
        sum_alpha += alpha[i];
        log_p += (alpha[i] - 1.0) * log(x[i] + 1e-15);
    }
    return exp(log_p);
}

double prob_kl_divergence(const double *p, const double *q, int n) {
    double kl = 0.0;
    for (int i = 0; i < n; i++) {
        if (p[i] > 1e-15 && q[i] > 1e-15) {
            kl += p[i] * log(p[i] / q[i]);
        }
    }
    return kl;
}

/* ---- Utility ---- */

void prob_print_bn(const BayesianNetwork *bn) {
    if (!bn) return;
    printf("Bayesian Network: %d nodes, %d edges\n", bn->num_nodes, bn->num_edges);
    for (int i = 0; i < bn->num_nodes; i++) {
        BayesianNode *n = &bn->nodes[i];
        printf("  Node[%d] '%s': %d states%s\n",
               n->id, n->name, n->num_states,
               n->is_evidence ? " [EVIDENCE]" : "");
        printf("    Marginals: ");
        for (int s = 0; s < n->num_states; s++)
            printf("%.3f ", n->marginal[s]);
        printf("\n");
    }
}

double prob_entropy(const double *distribution, int n) {
    double h = 0.0;
    for (int i = 0; i < n; i++) {
        if (distribution[i] > 1e-15) {
            h -= distribution[i] * log2(distribution[i]);
        }
    }
    return h;
}

double prob_mutual_information(const double *joint, int rows, int cols) {
    double mi = 0.0;
    double *row_sum = (double*)calloc((size_t)rows, sizeof(double));
    double *col_sum = (double*)calloc((size_t)cols, sizeof(double));
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++) {
            row_sum[i] += joint[i * cols + j];
            col_sum[j] += joint[i * cols + j];
        }
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++) {
            if (joint[i * cols + j] > 1e-15) {
                mi += joint[i * cols + j] *
                      log2(joint[i * cols + j] / (row_sum[i] * col_sum[j] + 1e-15));
            }
        }
    free(row_sum);
    free(col_sum);
    return mi;
}
