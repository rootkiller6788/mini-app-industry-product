# Course Tree — E-Commerce Retail

## Prerequisite Dependency Tree

```
E-Commerce Retail depends on:
├── Data Structures (C structs, arrays, linked concepts)
│   └── Inverted index, state machines, fixed-size containers
├── Algorithms
│   ├── Sorting (qsort for search ranking)
│   ├── Dynamic Programming (Levenshtein distance)
│   ├── String Processing (Porter stemmer, tokenizer)
│   └── Statistical Methods (EMA, SMA, chi-squared, z-score)
├── Systems
│   ├── Concurrency (atomic stock reserve, rate limiting)
│   ├── State Machines (OrderStatus, TransactionState)
│   └── Event Sourcing (append-only log, CQRS projections)
├── Database Concepts
│   ├── Indexing (inverted index, posting lists)
│   ├── Query Processing (search with filters and sorting)
│   └── Ranking (TF-IDF, BM25)
├── Machine Learning Basics
│   ├── Naive Bayes (fraud detection)
│   ├── A/B Testing (controlled experiments)
│   └── Anomaly Detection (z-score)
└── Mathematics
    ├── Linear Algebra (co-occurrence matrix)
    ├── Probability (Bayes theorem, p-values)
    ├── Statistics (Central Limit Theorem, confidence intervals)
    └── Calculus (trend slope via least squares)
```

## Downstream Dependencies (modules that depend on this)

```
E-Commerce Retail feeds into:
├── data-engine(7) → Search index vector store
├── backend(8) → Order API, payment gateway
├── frontend(9) → Analytics dashboard
├── security(13) → Fraud rules audit
└── AI(14) → Recommendation model training data
```