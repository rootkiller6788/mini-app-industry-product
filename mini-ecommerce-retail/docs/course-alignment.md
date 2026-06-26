# Course Alignment — E-Commerce Retail

## MIT (Massachusetts Institute of Technology)

| Course | Topic | Implementation |
|--------|-------|----------------|
| 6.004 Computation Structures | State machines | OrderStatus, TransactionState enums |
| 6.824 Distributed Systems | CAP theorem, consistency | Event store CQRS projections |
| 6.858 Computer Security | Threat detection, rate limiting | FraudEngine, RateLimiter |

## Stanford University

| Course | Topic | Implementation |
|--------|-------|----------------|
| CS 144 Networking | Rate limiting, congestion | Token bucket rate limiter |
| CS 245 Database | Indexing, query processing | Inverted index, TF-IDF/BM25 |
| CS 229 Machine Learning | Naive Bayes, A/B testing | fraud_bayesian_probability(), ab_test_chi_squared() |

## UC Berkeley

| Course | Topic | Implementation |
|--------|-------|----------------|
| CS 162 Operating Systems | Concurrency, atomic ops | flashsale_reserve() atomic stock reserve |
| CS 186 Database Systems | Query optimization, indexing | InvertedIndex, search_execute() |
| CS 267 HPC | Parallel algorithms | Amdahl's Law in flash sale analysis |
| CS 294 AI Systems | ML systems | Naive Bayes fraud detection |

## Carnegie Mellon University

| Course | Topic | Implementation |
|--------|-------|----------------|
| 15-410 Operating Systems | Synchronization | Atomic stock reservation |
| 15-445 Database Systems | TF-IDF, BM25 | compute_tfidf(), compute_bm25() |
| 15-721 Advanced Database | Probabilistic data structures | Inverted index posting lists |
| 15-418 Parallel Computer Architecture | Amdahl's Law | flashsale_reserve() throughput analysis |

## UT Austin

| Course | Topic | Implementation |
|--------|-------|----------------|
| CS 380D Distributed Systems | Event sourcing, CQRS | EventStore, ProductProjection |
| CS 395T Systems for ML | ML infrastructure | Analytics pipeline, fraud detection |
| ECE 382V VLSI | (N/A for e-commerce) | — |

## ETH Zurich

| Course | Topic | Implementation |
|--------|-------|----------------|
| 263-0006 Computer Architecture | Memory efficiency | Fixed-size arrays for cache locality |
| 263-3501 Parallel Programming | Concurrency patterns | Flash sale optimistic locking |

## University of Cambridge

| Course | Topic | Implementation |
|--------|-------|----------------|
| Part II: Operating Systems | Concurrency primitives | Atomic reserve in flashsale |
| Part II: Concurrent Systems | Lock-free patterns | Rate limiter token bucket |
| Part II: Compiler Construction | (N/A for e-commerce) | — |

## Tsinghua University (清华)

| Course | Topic | Implementation |
|--------|-------|----------------|
| 操作系统 | Concurrency control | Stock reservation ordering |
| 编译原理 | (Lexical analysis parallel) | Tokenizer, Porter stemmer |
| 计算机网络 | Rate control | Token bucket rate limiter |
| 计算机体系结构 | Memory hierarchy | Array-based data structures |

## Georgia Tech

| Course | Topic | Implementation |
|--------|-------|----------------|
| CS 6210 Advanced OS | Scheduling, resource mgmt | Rate limiter |
| CS 6290 HPCA | Performance analysis | Amdahl's Law |
| CS 7641 Machine Learning | Naive Bayes, evaluation | Fraud detection, A/B testing |

## Summary

All nine target universities' key CS courses are addressed. The e-commerce module serves as a practical application domain for systems, database, ML, and security concepts taught across these institutions.