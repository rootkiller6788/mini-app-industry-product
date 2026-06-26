# Knowledge Graph — mini-ai-application

## L1: Core Definitions

| Type | Header | Description |
|------|--------|-------------|
| `AgentRuntime` | agent_runtime.h | AI agent with ReAct capabilities |
| `AgentConfig` | agent_runtime.h | Agent configuration (tools, memory, prompts) |
| `AgentTool` | agent_runtime.h | Executable tool registered with agent |
| `AgentMessage` | agent_runtime.h | Conversation history entry |
| `InferenceServer` | inference_server.h | Model inference serving engine |
| `ModelMetadata` | inference_server.h | Registered model descriptor |
| `RequestQueue` | inference_server.h | Circular buffer request queue |
| `NLPPipeline` | nlp_pipeline.h | NLP task pipeline state |
| `Vocabulary` | nlp_pipeline.h | Token vocabulary with frequencies |
| `TokenSequence` | nlp_pipeline.h | Ordered sequence of token IDs |
| `RatingMatrix` | recommender.h | User-item rating sparse matrix |
| `MatrixFactorization` | recommender.h | SVD latent factor model |
| `Recommendation` | recommender.h | Predicted user-item rating |
| `AIExperiment` | ai_platform.h | ML experiment tracker |
| `AIMLPipeline` | ai_platform.h | ML workflow pipeline |

## L2: Core Concepts

| Concept | Implementation | Reference |
|---------|----------------|-----------|
| ReAct Loop | agent_react_loop() | Yao et al. 2023 |
| Dynamic Batching | inf_dynamic_batch() | Nvidia Triton |
| BPE Tokenization | nlp_bpe_tokenize() | Sennrich et al. 2016 |
| Collaborative Filtering | rec_user_knn_predict() | Resnick et al. 1994 |
| Feature Store | ai_fs_get_online() | Feast/Tecton |
| Embedding Similarity | nlp_embedding_similarity() | Mikolov et al. 2013 |

## L3: Engineering Structures

| Structure | File | Purpose |
|-----------|------|---------|
| Request Queue (ring buffer) | inference_server.c | Lock-free inference request queue |
| KV Cache | inference_server.c | Transformer key-value cache |
| TextRank Graph | nlp_pipeline.c | Sentence similarity graph for summarization |
| Beam Search State | nlp_pipeline.c | Beam hypothesis management for decoding |
| Continuous Batcher | inference_server.c | Iteration-level dynamic batching |

## L4: Standards/Theorems

| Theorem | Formula | Location |
|---------|---------|----------|
| Little's Law | L = λW | inference_server.c |
| Pearson Correlation | r_xy ∈ [-1, 1] | recommender.c |
| Condorcet Jury Theorem | P(correct) → 1 as N → ∞ | agent_runtime.c |
| Shapley Axioms | Efficiency, Symmetry, Dummy, Additivity | ai_platform.c |
| PageRank | PR(A) = (1-d) + d·Σ PR(B)/out(B) | nlp_pipeline.c |
| Good-Turing | P(unseen) = n₁/N | nlp_pipeline.c |
| CAP Theorem | Consistency vs Availability vs Partition | ai_platform.c |
| Kneser-Ney Smoothing | Discounted + continuation probability | nlp_pipeline.c |
| Bias-Variance Decomposition | E[(f̂-f)²] = Bias² + Var + σ² | inference_server.c |
| No Free Lunch | No universal optimal tool selection | agent_runtime.c |

## L5: Algorithms/Methods

| Algorithm | Complexity | Location |
|-----------|------------|----------|
| SGD Matrix Factorization | O(ratings·dim·iters) | recommender.c |
| Naive Bayes Classification | O(docs·vocab) | nlp_pipeline.c |
| UCB Tool Selection | O(tools) | agent_runtime.c |
| TextRank Summarization | O(n²·dim + n²·iter) | nlp_pipeline.c |
| Kneser-Ney Smoothing | O(vocab²) | nlp_pipeline.c |
| Beam Search Decoding | O(b·k·vocab) | nlp_pipeline.c |
| ALS Implicit MF | O(users·items·dim) | recommender.c |
| TF-IDF Vectorization | O(docs·vocab) | nlp_pipeline.c |
| PSI Drift Detection | O(bins) | ai_platform.c |
| A/B Test z-test | O(1) | recommender.c |

## L6: Canonical Problems

| Problem | Solution | Example |
|---------|----------|---------|
| Recommendation System | CF + MF + Content-based | demo_recommendation.c |
| Text Classification | Naive Bayes + TF-IDF | demo_nlp_sentiment.c |
| Agent Orchestration | ReAct Loop + Multi-agent | demo_agent_workflow.c |
| Model Serving | Dynamic batching + quantization | demo_inference_serving.c |

## L7: Applications

| Application | Files | Status |
|-------------|-------|--------|
| Multi-Agent Debate | agent_runtime.c | ✅ Complete |
| Named Entity Recognition | nlp_pipeline.c | ✅ Complete |
| Text Summarization | nlp_pipeline.c | ✅ Complete |
| A/B Testing Framework | recommender.c | ✅ Complete |
| Model Warm-up | inference_server.c | ✅ Complete |
| Train/Test Split | recommender.c | ✅ Complete |
| Evaluation Metrics (RMSE, DCG) | recommender.c | ✅ Complete |
| ML Pipeline (Kubeflow-style) | ai_platform.c | ✅ Complete |

## L8: Advanced Topics

| Topic | Implementation | Status |
|-------|----------------|--------|
| INT8 Quantization | inf_quantize_fp32_to_int8() | ✅ Complete |
| KV Cache Management | inf_kv_cache_*() | ✅ Complete |
| Federated Learning | ai_fed_init/aggregate() | ✅ Partial |
| Cold Start Handling | rec_cold_start_popularity() | ✅ Complete |
| Implicit ALS | rec_als_*() | ✅ Complete |
| Tree-of-Thought Search | agent_tot_search() | ✅ Complete |
| Reflexion Pattern | agent_reflect() | ✅ Complete |
| Chain-of-Thought | agent_think() | ✅ Complete |

## L9: Industry Frontiers

| Topic | Implementation | Status |
|-------|----------------|--------|
| Tool-Augmented LMs (TALM) | agent_select_tool_ucb() | ✅ Partial |
| Continuous Batching (vLLM) | inf_cb_*() | ✅ Partial |
| Neural Architecture Search | ai_nas_random_search() | ✅ Partial |
| Data Drift Detection | ai_drift_psi/ks_test() | ✅ Partial |
| Explainable AI (SHAP) | ai_permutation_importance() | ✅ Partial |
| Agent Safety | agent_safety_check() | ✅ Partial |
| Multi-Agent Consensus | agent_debate_*() | ✅ Partial |
| Model Compression | inf_simulate_pruning/quantization() | ✅ Partial |
| Model Deployment Strategies | inf_deploy_*() | ✅ Partial |
