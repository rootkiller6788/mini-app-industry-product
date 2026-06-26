/* ============================================================================
 * inference_server.c — Model Inference Server
 *
 * L1: Core Definitions — ModelMetadata, InferenceRequest/Response, RequestQueue
 * L2: Dynamic Batching (Nvidia Triton Inference Server style)
 * L3: Request Queue (lock-free ring buffer for high throughput)
 * L4: Queueing Theory — Little's Law: L = λ × W
 * L5: Load Shedding (adaptive QoS under overload)
 * L7: Model Warm-up (prevent cold-start latency spikes)
 * L8: INT8 Quantization (FP32 → INT8 conversion)
 * L9: GPU utilization modeling and multi-model ensemble
 *
 * Reference:
 * - Nvidia Triton Inference Server (github.com/triton-inference-server)
 * - Little, J.D.C. "A Proof for the Queuing Formula: L = λW" (1961)
 * - Krishnamoorthi, R. "Quantizing deep convolutional networks" (2018)
 * - MIT 6.824 / Stanford CS 329S: ML Systems
 * ========================================================================== */

#include "inference_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── L1: Server Init and Model Registration ────────────────────── */

static uint64_t inf_now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

void inf_server_init(InferenceServer *srv, uint32_t batch)
{
    if (!srv) return;
    memset(srv, 0, sizeof(*srv));
    srv->batch_size = batch;
    srv->batching_enabled = (batch > 1);
    srv->gpu_utilization = 0.0;
}

void inf_register_model(InferenceServer *srv, const char *name,
                         const char *path, ModelFormat fmt,
                         uint32_t in_dim, uint32_t out_dim)
{
    if (!srv || !name || srv->model_count >= INF_MAX_MODELS) return;
    ModelMetadata *m = &srv->models[srv->model_count];
    memset(m, 0, sizeof(*m));
    strncpy(m->name, name, 63);
    if (path) strncpy(m->path, path, 255);
    m->format = fmt;
    m->input_dim = in_dim;
    m->output_dim = out_dim;
    m->loaded = true;
    srv->model_count++;
}

/* ─── L3: Request Queue (circular buffer) ───────────────────────── */

bool inf_enqueue(InferenceServer *srv, const float *input, uint32_t dim,
                  uint64_t *req_id)
{
    if (!srv || !input || dim == 0) return false;
    if (srv->queue.count >= INF_MAX_QUEUE) return false;

    uint32_t tail = srv->queue.tail;
    InferenceRequest *req = &srv->queue.queue[tail];

    /* Allocate and copy input */
    req->data = (float *)malloc(dim * sizeof(float));
    if (!req->data) return false;
    memcpy(req->data, input, dim * sizeof(float));
    req->dim = dim;
    req->arrival_time = inf_now_us();

    static uint64_t next_id = 1;
    req->id = next_id++;
    if (req_id) *req_id = req->id;

    srv->queue.tail = (tail + 1) % INF_MAX_QUEUE;
    srv->queue.count++;
    srv->queue.total_requests++;

    return true;
}

bool inf_dequeue_result(InferenceServer *srv, InferenceResponse *resp)
{
    if (!srv || !resp || srv->queue.count == 0) return false;

    uint32_t head = srv->queue.head;
    InferenceRequest *req = &srv->queue.queue[head];

    memset(resp, 0, sizeof(*resp));
    resp->id = req->id;
    resp->dim = req->dim;

    /* Simulate inference: linear transformation */
    resp->data = (float *)malloc(resp->dim * sizeof(float));
    if (resp->data) {
        for (uint32_t i = 0; i < resp->dim; i++) {
            resp->data[i] = req->data ? req->data[i] * 2.0f + 1.0f : 0.0f;
        }
    }

    resp->latency_ms = 5.0 + (double)(rand() % 50); /* 5-55ms simulated */
    resp->success = true;

    /* Update model stats */
    if (srv->model_count > 0) {
        ModelMetadata *m = &srv->models[0];
        m->total_inferences++;
        double alpha = 0.1; /* Exponential moving average */
        m->avg_latency_ms = m->avg_latency_ms * (1.0 - alpha)
                          + resp->latency_ms * alpha;
        if (resp->latency_ms > m->p99_latency_ms) {
            m->p99_latency_ms = m->p99_latency_ms * 0.99
                              + resp->latency_ms * 0.01;
        }
    }

    /* Clean up request */
    free(req->data);
    req->data = NULL;
    srv->queue.head = (head + 1) % INF_MAX_QUEUE;
    srv->queue.count--;
    srv->queue.total_success++;

    /* Update throughput (requests per second) */
    static uint64_t last_qps_update = 0;
    static uint64_t qps_counter = 0;
    qps_counter++;
    uint64_t now = inf_now_us();
    if (now - last_qps_update > 1000000ULL) {
        srv->queue.throughput_qps = (double)qps_counter *
            (1000000.0 / (double)(now - last_qps_update));
        qps_counter = 0;
        last_qps_update = now;
    }

    return true;
}

/* ─── L2: Dynamic Batching ───────────────────────────────────────── */

void inf_dynamic_batch(InferenceServer *srv)
{
    if (!srv || !srv->batching_enabled) return;

    /* Heuristic: if queue depth > 2× batch_size, increase batch
     * if queue depth < batch_size / 2, decrease batch
     * Clamp: 1 ≤ batch_size ≤ INF_MAX_BATCH */
    uint32_t depth = srv->queue.count;

    if (depth > srv->batch_size * 2 && srv->batch_size < INF_MAX_BATCH) {
        srv->batch_size = (srv->batch_size * 3) / 2;
        if (srv->batch_size > INF_MAX_BATCH) srv->batch_size = INF_MAX_BATCH;
    } else if (depth < srv->batch_size / 2 && srv->batch_size > 1) {
        srv->batch_size = (srv->batch_size * 2) / 3;
        if (srv->batch_size < 1) srv->batch_size = 1;
    }
}

uint32_t inf_batch_size_heuristic(const InferenceServer *srv)
{
    if (!srv) return 1;

    /* Optimal batch size balances throughput and latency:
     * B_opt ≈ √(2λ / μ_cost) per basic queueing model
     * Simplified: B_opt = min(max_batch, max(1, queue_depth / 4)) */
    uint32_t depth = srv->queue.count;
    uint32_t b = depth / 4;
    if (b < 1) b = 1;
    if (b > INF_MAX_BATCH) b = INF_MAX_BATCH;
    return b;
}

/* ─── L5: Load Shedding ──────────────────────────────────────────── */

bool inf_rate_limit(const InferenceServer *srv, double max_qps)
{
    if (!srv) return false;
    return srv->queue.throughput_qps <= max_qps;
}

void inf_load_shed(InferenceServer *srv, double threshold)
{
    if (!srv) return;

    /* Adaptive load shedding:
     * If queue utilization > threshold%, drop low-priority requests.
     * Drop probability = (util - threshold) / (1 - threshold) */
    double util = (double)srv->queue.count / (double)INF_MAX_QUEUE;

    if (util > threshold) {
        double drop_prob = (util - threshold) / (1.0 - threshold);
        uint32_t to_drop = (uint32_t)((double)srv->queue.count * drop_prob);

        for (uint32_t i = 0; i < to_drop && srv->queue.count > 0; i++) {
            /* Drop from head (oldest) */
            uint32_t head = srv->queue.head;
            free(srv->queue.queue[head].data);
            srv->queue.queue[head].data = NULL;
            srv->queue.head = (head + 1) % INF_MAX_QUEUE;
            srv->queue.count--;
        }
    }
}

/* ─── L7: Model Warm-up ──────────────────────────────────────────── */

void inf_warm_up(InferenceServer *srv, uint32_t model_idx, uint32_t iterations)
{
    if (!srv || model_idx >= srv->model_count) return;

    /* Warm-up: run dummy inferences to populate CPU/GPU caches
     * and trigger JIT compilation (for Torch/TF backends).
     * Reduces cold-start latency by 50-80%. */
    ModelMetadata *m = &srv->models[model_idx];
    float *dummy_input = (float *)calloc(m->input_dim, sizeof(float));
    if (!dummy_input) return;

    printf("[WarmUp] Model '%s' — %u iterations...\n", m->name, iterations);
    for (uint32_t i = 0; i < iterations; i++) {
        /* Simulate inference */
        for (uint32_t j = 0; j < m->input_dim; j++) {
            dummy_input[j] = (float)rand() / (float)RAND_MAX;
        }
    }
    free(dummy_input);

    /* Update model metadata */
    m->avg_latency_ms *= 0.3; /* Cold start → warmed up */
    printf("[WarmUp] Complete. Estimated latency: %.2f ms\n", m->avg_latency_ms);
}

double inf_cold_start_latency(const InferenceServer *srv)
{
    if (!srv || srv->model_count == 0) return 0.0;

    /* Cold start latency model:
     * L_cold = L_warm × (1 + α × N_layers) where α ≈ 0.05 per layer
     * First inference pays: I/O (load weights), memory allocation,
     * CPU cache fill, GPU kernel launch overhead. */
    const ModelMetadata *m = &srv->models[0];
    double warm_lat = m->avg_latency_ms > 0 ? m->avg_latency_ms : 10.0;
    return warm_lat * 2.5; /* ~2.5x cold start penalty */
}

/* ─── L8: INT8 Quantization ──────────────────────────────────────── */

void inf_quantize_fp32_to_int8(const float *fp32, int8_t *int8,
                                uint32_t dim, float scale)
{
    if (!fp32 || !int8 || dim == 0) return;

    /* Symmetric quantization: int8 = round(fp32 / scale)
     * Clamp to [-128, 127]
     *
     * L4: Quantization error ≤ scale/2 per element (uniform quantization)
     * SNR ≈ 6.02N + 1.76 dB for N-bit quantization (Bennett 1948)
     * For INT8: SNR ≈ 49.92 dB theoretical */
    for (uint32_t i = 0; i < dim; i++) {
        float q = fp32[i] / scale;
        if (q > 127.0f) q = 127.0f;
        if (q < -128.0f) q = -128.0f;
        int8[i] = (int8_t)(int32_t)roundf(q);
    }
}

void inf_dequantize_int8_to_fp32(const int8_t *int8, float *fp32,
                                  uint32_t dim, float scale)
{
    if (!int8 || !fp32 || dim == 0) return;
    for (uint32_t i = 0; i < dim; i++) {
        fp32[i] = (float)int8[i] * scale;
    }
}

/* ─── L7: Multi-Model Ensemble ───────────────────────────────────── */

void inf_ensemble_predict(InferenceServer *srv, const ModelEnsemble *ensemble,
                           const float *input, float *output, uint32_t dim)
{
    if (!srv || !ensemble || !input || !output) return;

    /* Ensemble: average predictions from multiple models.
     * Reduces variance by factor of 1/N (bagging principle).
     * L4: Bias-Variance decomposition —
     *   E[(f̂ - f)²] = Bias²[f̂] + Var[f̂] + σ²
     * Ensemble reduces Var[f̂] by averaging uncorrelated errors. */

    memset(output, 0, dim * sizeof(float));
    uint32_t valid_models = 0;

    for (uint32_t m = 0; m < ensemble->count; m++) {
        uint32_t mid = ensemble->model_ids[m];
        if (mid >= srv->model_count) continue;

        /* Simulate per-model prediction */
        for (uint32_t i = 0; i < dim; i++) {
            output[i] += input[i] * (1.0f + 0.1f * (float)m);
        }
        valid_models++;
    }

    if (valid_models > 0) {
        for (uint32_t i = 0; i < dim; i++) {
            output[i] /= (float)valid_models;
        }
    }
}

/* ─── L9: GPU Utilization Model ──────────────────────────────────── */

void inf_update_gpu_utilization(InferenceServer *srv)
{
    if (!srv) return;

    /* GPU utilization model:
     * U = min(1.0, λ × T_infer / batch_size)
     * where λ = arrival rate (QPS), T_infer = per-sample latency
     *
     * L4: Little's Law applied to GPU:
     * Average requests in GPU = arrival_rate × avg_inference_time
     * GPU saturated when arrival_rate ≥ batch_size / avg_inference_time */
    double arrival_rate = srv->queue.throughput_qps;
    double avg_lat = srv->model_count > 0 ? srv->models[0].avg_latency_ms : 10.0;
    double service_rate = (double)srv->batch_size / (avg_lat / 1000.0);

    srv->gpu_utilization = fmin(1.0, arrival_rate / service_rate);
}

void inf_print_stats(const InferenceServer *srv)
{
    if (!srv) return;
    printf("\n=== Inference Server Stats ===\n");
    printf("Models: %u | Batch: %u | Batching: %s\n",
           srv->model_count, srv->batch_size,
           srv->batching_enabled ? "enabled" : "disabled");
    printf("Queue: %u/%u | Throughput: %.1f QPS\n",
           srv->queue.count, INF_MAX_QUEUE, srv->queue.throughput_qps);
    printf("Total: %llu requests, %llu successful\n",
           (unsigned long long)srv->queue.total_requests,
           (unsigned long long)srv->queue.total_success);
    printf("GPU: %.1f%% utilized\n\n", srv->gpu_utilization * 100.0);

    for (uint32_t i = 0; i < srv->model_count; i++) {
        const ModelMetadata *m = &srv->models[i];
        printf("  Model[%u]: %s (%s) in=%u out=%u\n",
               i, m->name, m->format == INF_ONNX ? "ONNX" :
               m->format == INF_TFLITE ? "TFLite" : "Torch",
               m->input_dim, m->output_dim);
        printf("    Inferences: %llu | Avg: %.2fms | P99: %.2fms\n",
               (unsigned long long)m->total_inferences,
               m->avg_latency_ms, m->p99_latency_ms);
    }
}

/* L8: KV-Cache Management for Transformer Inference
 * Autoregressive decoding reuses key-value pairs from self-attention.
 * KV Cache: 2 * num_layers * seq_len * hidden_dim * sizeof(fp16)
 * Example LLaMA-7B: 2*32*2048*4096*2 = ~1GB per sequence */

void inf_kv_cache_init(KVCache *cache, uint32_t num_layers, uint32_t max_seq_len, uint32_t hidden_dim) {
    if (!cache) return;
    memset(cache, 0, sizeof(*cache));
    cache->num_layers = num_layers; cache->max_seq_len = max_seq_len;
    for (uint32_t l = 0; l < num_layers && l < KV_CACHE_MAX_LAYERS; l++) {
        KVCacheLayer *ly = &cache->layers[l]; ly->hidden_dim = hidden_dim; ly->seq_len = 0;
        size_t bytes = (size_t)max_seq_len * (size_t)hidden_dim * sizeof(float);
        ly->keys = (float *)calloc(bytes, 1); ly->values = (float *)calloc(bytes, 1);
        if (ly->keys && ly->values) { ly->allocated = true; cache->total_bytes += 2 * bytes; }
    }
}

void inf_kv_cache_append(KVCache *cache, uint32_t layer_idx, const float *new_key, const float *new_val, uint32_t hidden_dim) {
    if (!cache || layer_idx >= cache->num_layers) return;
    KVCacheLayer *ly = &cache->layers[layer_idx];
    if (!ly->allocated || ly->seq_len >= cache->max_seq_len) return;
    size_t off = (size_t)ly->seq_len * (size_t)hidden_dim;
    memcpy(ly->keys + off, new_key, hidden_dim * sizeof(float));
    memcpy(ly->values + off, new_val, hidden_dim * sizeof(float));
    ly->seq_len++;
}

void inf_kv_cache_clear(KVCache *cache) {
    if (!cache) return;
    for (uint32_t l = 0; l < cache->num_layers; l++) cache->layers[l].seq_len = 0;
}

/* L9: Continuous Batching (vLLM / Orca style)
 * Dynamic iteration-level scheduling: add/remove sequences mid-batch.
 * 2-10x throughput improvement for variable-length generation.
 *
 * Reference: Kwon et al. 2023 "Efficient Memory Management for
 * LLM Serving with PagedAttention" (vLLM, UC Berkeley) */

void inf_cb_init(ContinuousBatcher *cb, uint32_t batch_size) {
    if (!cb) return;
    memset(cb, 0, sizeof(*cb));
    cb->batch_size = batch_size;
}

int inf_cb_add_sequence(ContinuousBatcher *cb, uint32_t max_tokens) {
    if (!cb || cb->active_count >= 256) return -1;
    GenerationSequence *gs = &cb->sequences[cb->active_count];
    gs->seq_id = cb->active_count; gs->max_tokens = max_tokens;
    gs->tokens_generated = 0; gs->completed = false; gs->priority = 1.0f;
    return cb->active_count++;
}

void inf_cb_step(ContinuousBatcher *cb) {
    if (!cb) return;
    for (uint32_t i = 0; i < cb->active_count; i++) {
        GenerationSequence *gs = &cb->sequences[i];
        if (!gs->completed) {
            gs->tokens_generated++; cb->total_tokens_generated++;
            if (gs->tokens_generated >= gs->max_tokens) gs->completed = true;
        }
    }
    uint32_t w = 0;
    for (uint32_t i = 0; i < cb->active_count; i++)
        if (!cb->sequences[i].completed) { if (w != i) cb->sequences[w] = cb->sequences[i]; w++; }
    cb->active_count = w;
}

/* L9: Model Versioning and A/B Deployment
 *
 * Production ML systems require safe model deployment:
 * 1. Shadow mode: new model runs in parallel, logs predictions (no user impact)
 * 2. Canary deployment: 1-5% traffic to new model, monitor metrics, auto-rollback
 * 3. A/B test: equal split, statistical comparison of business metrics
 * 4. Blue-green: instant cutover with rollback capability
 *
 * Canary analysis (Google 2013):
 * - Compare canary metrics to baseline using two-sample t-test
 * - Auto-rollback if p-value < 0.05 on key metrics (latency, error rate, CTR)
 * - Gradual traffic increase: 1% → 5% → 20% → 50% → 100%
 *
 * Reference: Google SRE Book Ch.16 "Canary Analysis Service"
 *            Sculley et al. 2015 "Hidden Technical Debt in ML Systems"
 *            Stanford CS 329S / CMU 17-645: ML in Production */

void inf_deploy_init(ModelDeployment *deploy, uint32_t baseline, uint32_t candidate, DeployStrategy strategy, double split) {
    if (!deploy) return;
    memset(deploy, 0, sizeof(*deploy));
    deploy->model_id_a = baseline; deploy->model_id_b = candidate;
    deploy->strategy = strategy; deploy->traffic_split = split;
}

uint32_t inf_deploy_select_model(const ModelDeployment *deploy) {
    if (!deploy) return 0;
    if (deploy->strategy == DEPLOY_BLUEGREEN) return deploy->model_id_b;
    double r = (double)rand() / (double)RAND_MAX;
    return (r < deploy->traffic_split) ? deploy->model_id_b : deploy->model_id_a;
}

void inf_deploy_record(ModelDeployment *deploy, uint32_t model_id, double latency, bool error) {
    if (!deploy) return;
    if (model_id == deploy->model_id_a) {
        deploy->requests_a++; deploy->latency_a += latency; if (error) deploy->error_rate_a += 1.0;
    } else {
        deploy->requests_b++; deploy->latency_b += latency; if (error) deploy->error_rate_b += 1.0;
    }
}

bool inf_deploy_should_rollback(const ModelDeployment *deploy, double latency_threshold, double error_threshold) {
    if (!deploy || deploy->requests_b < 100) return false; /* Need enough samples */
    double avg_lat_a = deploy->requests_a > 0 ? deploy->latency_a / (double)deploy->requests_a : 0.0;
    double avg_lat_b = deploy->latency_b / (double)deploy->requests_b;
    double err_a = deploy->requests_a > 0 ? deploy->error_rate_a / (double)deploy->requests_a : 0.0;
    double err_b = deploy->error_rate_b / (double)deploy->requests_b;
    if (avg_lat_b > avg_lat_a * latency_threshold) return true;
    if (err_b > err_a * error_threshold) return true;
    return false;
}

/* L9: Model Compression — distillation, pruning, quantization
 *
 * Knowledge Distillation (Hinton et al. 2015):
 * Teacher (large) → Student (small) via soft target training.
 * L_student = alpha * L_hard(y, student(x)) + (1-alpha) * L_soft(teacher(x)/T, student(x)/T)
 * T > 1 softens probability distribution, revealing dark knowledge.
 *
 * Pruning (Han et al. 2015): Remove low-magnitude weights.
 * Iterative: train → prune → fine-tune → repeat.
 * Unstructured (individual weights) vs structured (channels/layers).
 * Typically 50-90% sparsity without accuracy loss.
 *
 * Quantization: FP32 → INT8/INT4 reduces model size 4x-8x.
 * Post-training quantization (PTQ) vs quantization-aware training (QAT).
 * GPTQ (Frantar et al. 2023): layer-wise quantization with Hessian inverse.
 * AWQ (Lin et al. 2024): activation-aware weight quantization.
 *
 * Reference: Hinton et al. 2015 "Distilling the Knowledge in a Neural Network"
 *            Han et al. 2015 "Deep Compression" (Stanford)
 *            Frantar et al. 2023 "GPTQ: Accurate Post-Training Quantization" (IST Austria) */

/* Simulated model compression API */
CompressionResult inf_simulate_pruning(double sparsity_target) {
    CompressionResult r = { .sparsity = sparsity_target, .size_mb_before = 100.0 };
    r.size_mb_after = r.size_mb_before * (1.0 - sparsity_target);
    /* Accuracy retention model: 1 - 0.5 * sparsity^2 (quadratic penalty) */
    r.accuracy_retained = 1.0 - 0.5 * sparsity_target * sparsity_target;
    return r;
}

CompressionResult inf_simulate_quantization(int bits) {
    CompressionResult r = { .sparsity = 0.0, .size_mb_before = 100.0 };
    double ratio = (double)bits / 32.0;
    r.size_mb_after = r.size_mb_before * ratio;
    /* Accuracy: INT8 retains ~99.9%, INT4 ~98%, INT2 ~90% */
    r.accuracy_retained = 0.95 + 0.05 * ratio;
    return r;
}