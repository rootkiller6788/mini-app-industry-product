#include "inference_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

static int run = 0, pass = 0;
#define TEST(nm) do { run++; printf("  TEST: %s ... ", nm); } while(0)
#define PASS do { puts("PASS"); pass++; } while(0)

static void t_init(void) {
    TEST("inf_server_init");
    InferenceServer s; inf_server_init(&s, 4);
    assert(s.batch_size == 4);
    assert(s.batching_enabled);
    PASS;
}

static void t_reg_model(void) {
    TEST("inf_register_model");
    InferenceServer s; inf_server_init(&s, 1);
    inf_register_model(&s, "bert", "/models/bert.onnx", INF_ONNX, 768, 10);
    assert(s.model_count == 1);
    assert(s.models[0].loaded);
    PASS;
}

static void t_enqueue_dequeue(void) {
    TEST("enqueue/dequeue");
    InferenceServer s; inf_server_init(&s, 1);
    float in[4]={1,2,3,4}; uint64_t rid;
    assert(inf_enqueue(&s, in, 4, &rid));
    assert(s.queue.count == 1);
    InferenceResponse resp;
    assert(inf_dequeue_result(&s, &resp));
    assert(resp.success);
    free(resp.data);
    PASS;
}

static void t_batching(void) {
    TEST("dynamic_batch");
    InferenceServer s; inf_server_init(&s, 4);
    s.batching_enabled = true;
    float in[2]={1,2};
    for (int i=0;i<100;i++) inf_enqueue(&s,in,2,NULL);
    inf_dynamic_batch(&s);
    assert(s.batch_size >= 1);
    PASS;
}

static void t_batch_heuristic(void) {
    TEST("batch_heuristic");
    InferenceServer s; inf_server_init(&s, 4);
    s.queue.count = 8;
    assert(inf_batch_size_heuristic(&s) >= 1);
    PASS;
}

static void t_rate_limit(void) {
    TEST("rate_limit");
    InferenceServer s; inf_server_init(&s, 1);
    assert(inf_rate_limit(&s, 100.0));
    PASS;
}

static void t_load_shed(void) {
    TEST("load_shed");
    InferenceServer s; inf_server_init(&s, 1);
    float in[2]={0,0};
    for (int i=0;i<100;i++) inf_enqueue(&s,in,2,NULL);
    uint32_t before=s.queue.count;
    inf_load_shed(&s,0.5);
    assert(s.queue.count <= before);
    PASS;
}

static void t_warm_up(void) {
    TEST("warm_up");
    InferenceServer s; inf_server_init(&s, 1);
    inf_register_model(&s,"m","/m",INF_ONNX,64,10);
    inf_warm_up(&s,0,10);
    PASS;
}

static void t_cold_start(void) {
    TEST("cold_start");
    InferenceServer s; inf_server_init(&s, 1);
    inf_register_model(&s,"m","/m",INF_ONNX,64,10);
    s.models[0].avg_latency_ms = 10.0;
    assert(inf_cold_start_latency(&s) > 0.0);
    PASS;
}

static void t_quantize(void) {
    TEST("quantize");
    float fp[4]={0.5,-0.3,1.2,-0.8}; int8_t i8[4]; float fp2[4];
    inf_quantize_fp32_to_int8(fp,i8,4,0.01f);
    inf_dequantize_int8_to_fp32(i8,fp2,4,0.01f);
    for (int i=0;i<4;i++) assert((fp[i]>=0)==(fp2[i]>=0));
    PASS;
}

static void t_ensemble(void) {
    TEST("ensemble");
    InferenceServer s; inf_server_init(&s,1);
    inf_register_model(&s,"m1","/m1",INF_ONNX,4,4);
    inf_register_model(&s,"m2","/m2",INF_ONNX,4,4);
    uint32_t ids[2]={0,1}; ModelEnsemble ens={.model_ids=ids,.count=2};
    float in[4]={1,2,3,4},out[4];
    inf_ensemble_predict(&s,&ens,in,out,4);
    assert(out[0]!=0.0f);
    PASS;
}

static void t_gpu_util(void) {
    TEST("gpu_util");
    InferenceServer s; inf_server_init(&s,1);
    inf_register_model(&s,"m","/m",INF_ONNX,4,4);
    inf_update_gpu_utilization(&s);
    assert(s.gpu_utilization>=0.0 && s.gpu_utilization<=1.0);
    PASS;
}

static void t_kv_cache(void) {
    TEST("kv_cache");
    KVCache kvc; inf_kv_cache_init(&kvc,2,64,16);
    assert(kvc.num_layers==2);
    assert(kvc.layers[0].allocated);
    float k[16],v[16];
    for(int i=0;i<16;i++){k[i]=(float)i;v[i]=(float)(-i);}
    inf_kv_cache_append(&kvc,0,k,v,16);
    assert(kvc.layers[0].seq_len==1);
    inf_kv_cache_clear(&kvc);
    assert(kvc.layers[0].seq_len==0);
    PASS;
}

static void t_continuous_batch(void) {
    TEST("continuous_batch");
    ContinuousBatcher cb; inf_cb_init(&cb,8);
    assert(cb.batch_size==8);
    int id=inf_cb_add_sequence(&cb,10);
    assert(id>=0);
    assert(cb.active_count==1);
    for(int i=0;i<15;i++) inf_cb_step(&cb);
    assert(cb.total_tokens_generated>0);
    PASS;
}

static void t_deploy(void) {
    TEST("deploy AB");
    ModelDeployment dep; inf_deploy_init(&dep,0,1,DEPLOY_AB,0.5);
    assert(dep.strategy==DEPLOY_AB);
    inf_deploy_record(&dep,0,10.0,false);
    inf_deploy_record(&dep,1,15.0,false);
    assert(!inf_deploy_should_rollback(&dep,2.0,2.0));
    PASS;
}

static void t_compression(void) {
    TEST("compression");
    CompressionResult pr=inf_simulate_pruning(0.5);
    assert(pr.size_mb_after<pr.size_mb_before);
    CompressionResult qr=inf_simulate_quantization(8);
    assert(qr.size_mb_after<qr.size_mb_before);
    PASS;
}

static void t_null(void) {
    TEST("null safety");
    inf_server_init(NULL, 1); inf_register_model(NULL,NULL,NULL,0,0,0);
    inf_enqueue(NULL,NULL,0,NULL); inf_dynamic_batch(NULL);
    inf_load_shed(NULL,0); inf_warm_up(NULL,0,0);
    inf_quantize_fp32_to_int8(NULL,NULL,0,0);
    inf_ensemble_predict(NULL,NULL,NULL,NULL,0);
    inf_kv_cache_init(NULL,0,0,0); inf_kv_cache_clear(NULL);
    inf_cb_init(NULL,0); inf_deploy_init(NULL,0,0,0,0);
    PASS;
}

int main(void) {
    puts("\n=== Inference Server Tests ===");
    puts("");
    t_init(); t_reg_model(); t_enqueue_dequeue(); t_batching();
    t_batch_heuristic(); t_rate_limit(); t_load_shed();
    t_warm_up(); t_cold_start(); t_quantize(); t_ensemble();
    t_gpu_util(); t_kv_cache(); t_continuous_batch();
    t_deploy(); t_compression(); t_null();
    printf("\n=== Results: %d/%d tests passed ===\n", pass, run);
    return (pass == run) ? 0 : 1;
}
