#ifndef MINI_FUTURE_PROBABILISTIC_COMPUTE_H
#define MINI_FUTURE_PROBABILISTIC_COMPUTE_H

#include <stdbool.h>
#include <stdint.h>

/* Probabilistic Computing -- Stochastic & Bayesian Computation
 * L1: StochasticBit, BayesianNode, MarkovChain, MCMCSampler
 * L2: Stochastic computing, Bayesian inference, MCMC
 * L3: Bayesian network topology, factor graph, belief propagation
 * L4: Bayes theorem (Bayes 1763), Central Limit Theorem
 * L5: Metropolis-Hastings, Gibbs sampling, belief propagation
 * L6: Bayesian diagnosis, probabilistic classification
 * L7: Probabilistic programming, uncertainty quantification
 * L8: Variational inference, stochastic gradient MCMC
 * L9: Intel probabilistic chips, analog stochastic neurons
 */

#define PROB_MAX_NODES     1024
#define PROB_MAX_EDGES     4096
#define PROB_MAX_SAMPLES   100000
#define PROB_MAX_STATES    32
#define PROB_NAME_LEN      64

typedef struct {
    int id;
    double probability;
    double bias;
    double temperature;
    int last_output;
    double *history;
    int history_len;
    int history_cap;
} StochasticBit;

typedef struct {
    int id;
    char name[PROB_NAME_LEN];
    int num_states;
    char state_names[PROB_MAX_STATES][32];
    double *cpt;
    int *parent_ids;
    int num_parents;
    int parent_state_space;
    bool is_evidence;
    int evidence_state;
    double *marginal;
} BayesianNode;

typedef struct {
    double *state;
    int dim;
    double log_prob;
    bool accepted;
} MarkovState;

typedef enum {
    MCMC_METROPOLIS  = 0,
    MCMC_GIBBS       = 1,
    MCMC_HAMILTONIAN = 2,
    MCMC_SLICE       = 3
} MCMCAlgorithm;

typedef struct {
    BayesianNode nodes[PROB_MAX_NODES];
    int num_nodes;
    int edge_from[PROB_MAX_EDGES];
    int edge_to[PROB_MAX_EDGES];
    int num_edges;
} BayesianNetwork;

typedef struct {
    MarkovState *samples;
    int num_samples;
    int capacity;
    int dim;
    MCMCAlgorithm algorithm;
    double step_size;
    double burn_in;
    double acceptance_rate;
    int total_proposals;
    int total_accepts;
} MCMCSampler;

StochasticBit* prob_bit_create(double probability);
void prob_bit_destroy(StochasticBit *bit);
int  prob_bit_sample(StochasticBit *bit);
int  prob_bit_sample_n(StochasticBit *bit, int n, int *results);
double prob_bit_estimate_probability(const StochasticBit *bit);

double prob_sadd(double pa, double pb);
double prob_smul(double pa, double pb);
double prob_sdiv(double pa, double pb);
double prob_sneg(double pa);
double prob_binary_to_stochastic(int value, int bit_width);
int  prob_stochastic_to_binary(double p, int bit_width);

BayesianNetwork* prob_bn_create(void);
void prob_bn_destroy(BayesianNetwork *bn);
int  prob_bn_add_node(BayesianNetwork *bn, const char *name, int num_states);
int  prob_bn_add_edge(BayesianNetwork *bn, int parent_id, int child_id);
int  prob_bn_set_cpt(BayesianNetwork *bn, int node_id,
                      const double *cpt, int cpt_len);
int  prob_bn_set_evidence(BayesianNetwork *bn, int node_id, int state);
int  prob_bn_clear_evidence(BayesianNetwork *bn, int node_id);

int  prob_bn_belief_propagation(BayesianNetwork *bn, int max_iterations);
double prob_bn_posterior(const BayesianNetwork *bn, int node_id, int state);
int  prob_bn_map_query(BayesianNetwork *bn, int *best_states);
int  prob_bn_sample_posterior(const BayesianNetwork *bn, int num_samples,
                               int **samples);

MCMCSampler* prob_mcmc_create(int dim, MCMCAlgorithm algo, double step_size);
void prob_mcmc_destroy(MCMCSampler *mcmc);
int  prob_mcmc_sample(MCMCSampler *mcmc,
                       double (*log_prob)(const double *x, int dim),
                       const double *initial, int num_samples);
int  prob_mcmc_sample_posterior_bn(MCMCSampler *mcmc,
                                    const BayesianNetwork *bn, int num_samples);
double prob_mcmc_ess(const MCMCSampler *mcmc);
void prob_mcmc_summary(const MCMCSampler *mcmc, double *mean,
                        double *std, int dim);

double prob_gaussian_pdf(double x, double mu, double sigma);
double prob_gaussian_log_pdf(double x, double mu, double sigma);
double prob_beta_pdf(double x, double alpha, double beta);
double prob_dirichlet_pdf(const double *x, const double *alpha, int k);
double prob_kl_divergence(const double *p, const double *q, int n);

void prob_print_bn(const BayesianNetwork *bn);
double prob_entropy(const double *distribution, int n);
double prob_mutual_information(const double *joint, int rows, int cols);

#endif
