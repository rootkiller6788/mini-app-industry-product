#ifndef INFERENCE_SERVER_H
#define INFERENCE_SERVER_H
#include <stdbool.h>
#include <stdint.h>

#define INF_MAX_MODELS     16
#define INF_MAX_BATCH      64
#define INF_MAX_QUEUE     256
#define INF_QUEUE_DEPTH    64

typedef enum { INF_ONNX, INF_TFLITE, INF_TORCH, INF_CUSTOM } ModelFormat;

typedef struct {
    char name[64]; char path[256];
    ModelFormat format; uint32_t input_dim, output_dim;
    bool loaded; uint64_t total_inferences;
    double avg_latency_ms, p99_latency_ms;
} ModelMetadata;

typedef struct {
    float *data; uint32_t dim;
    uint64_t id; uint64_t arrival_time;
} InferenceRequest;

typedef struct {
    float *data; uint32_t dim;
    uint64_t id; double latency_ms; bool success;
} InferenceResponse;

typedef struct {
    InferenceRequest queue[INF_MAX_QUEUE];
    uint32_t head, tail, count;
    uint64_t total_requests, total_success;
    double throughput_qps;
} RequestQueue;

typedef struct {
    ModelMetadata models[INF_MAX_MODELS];
    uint32_t model_count;
    RequestQueue queue;
    uint32_t batch_size;
    bool batching_enabled;
    double gpu_utilization;
} InferenceServer;

/* L1 API */
void inf_server_init(InferenceServer *srv, uint32_t batch);
void inf_register_model(InferenceServer *srv, const char *name, const char *path, ModelFormat fmt, uint32_t in_dim, uint32_t out_dim);
bool inf_enqueue(InferenceServer *srv, const float *input, uint32_t dim, uint64_t *req_id);
bool inf_dequeue_result(InferenceServer *srv, InferenceResponse *resp);

/* L2: Dynamic batching (Nvidia Triton-style) */
void inf_dynamic_batch(InferenceServer *srv);
uint32_t inf_batch_size_heuristic(const InferenceServer *srv);

/* L5: Load shedding / rate limiting */
bool inf_rate_limit(const InferenceServer *srv, double max_qps);
void inf_load_shed(InferenceServer *srv, double threshold);

/* L7: Model warm-up and cold start */
void inf_warm_up(InferenceServer *srv, uint32_t model_idx, uint32_t iterations);
double inf_cold_start_latency(const InferenceServer *srv);

/* L8: Quantization-aware inference (INT8) */
void inf_quantize_fp32_to_int8(const float *fp32, int8_t *int8, uint32_t dim, float scale);
void inf_dequantize_int8_to_fp32(const int8_t *int8, float *fp32, uint32_t dim, float scale);

/* L7: Multi-model ensemble */
typedef struct { uint32_t *model_ids; uint32_t count; } ModelEnsemble;
void inf_ensemble_predict(InferenceServer *srv, const ModelEnsemble *ensemble, const float *input, float *output, uint32_t dim);

void inf_print_stats(const InferenceServer *srv);

/* L8: KV-Cache */
#define KV_CACHE_MAX_SEQ    2048
#define KV_CACHE_MAX_LAYERS 32
typedef struct { float *keys; float *values; uint32_t seq_len, hidden_dim; bool allocated; } KVCacheLayer;
typedef struct { KVCacheLayer layers[KV_CACHE_MAX_LAYERS]; uint32_t num_layers, max_seq_len; uint64_t total_bytes; } KVCache;
void inf_kv_cache_init(KVCache *cache, uint32_t num_layers, uint32_t max_seq_len, uint32_t hidden_dim);
void inf_kv_cache_append(KVCache *cache, uint32_t layer_idx, const float *new_key, const float *new_val, uint32_t hidden_dim);
void inf_kv_cache_clear(KVCache *cache);

/* L9: Continuous Batching */
typedef struct { uint32_t seq_id, tokens_generated, max_tokens; bool completed; float priority; } GenerationSequence;
typedef struct { GenerationSequence sequences[256]; uint32_t active_count, batch_size, total_tokens_generated; } ContinuousBatcher;
void inf_cb_init(ContinuousBatcher *cb, uint32_t batch_size);
int inf_cb_add_sequence(ContinuousBatcher *cb, uint32_t max_tokens);
void inf_cb_step(ContinuousBatcher *cb);

/* L9: Model Deployment */
typedef enum { DEPLOY_SHADOW, DEPLOY_CANARY, DEPLOY_AB, DEPLOY_BLUEGREEN } DeployStrategy;
typedef struct { uint32_t model_id_a, model_id_b; DeployStrategy strategy; double traffic_split; uint64_t requests_a, requests_b; double latency_a, latency_b; double error_rate_a, error_rate_b; } ModelDeployment;
void inf_deploy_init(ModelDeployment *deploy, uint32_t baseline, uint32_t candidate, DeployStrategy strategy, double split);
uint32_t inf_deploy_select_model(const ModelDeployment *deploy);
void inf_deploy_record(ModelDeployment *deploy, uint32_t model_id, double latency, bool error);
bool inf_deploy_should_rollback(const ModelDeployment *deploy, double latency_threshold, double error_threshold);

/* L9: Model Compression */
typedef struct { float sparsity; double size_mb_before, size_mb_after; double accuracy_retained; } CompressionResult;
CompressionResult inf_simulate_pruning(double sparsity_target);
CompressionResult inf_simulate_quantization(int bits);

/* Statistics */
void inf_update_gpu_utilization(InferenceServer *srv);
#endif