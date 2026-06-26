# Course Tree — mini-logistics-supplychain

## Prerequisites Dependency Graph

```
mini-math-theory (0)
    └── Linear algebra, calculus, probability
        └── Statistics (for forecasting, safety stock)
            └── mini-data-store-search-vec (6)
                └── Data structures (graphs, heaps, queues)
                    └── mini-network-dist-proto (5)
                        └── Graph algorithms
                            └── mini-logistics-supplychain (19)
                                ├── L1-L2: Core logistics concepts
                                ├── L3: Graph/queue/DP engineering structures
                                ├── L4: Supply chain theorems (EOQ, Little, Bullwhip)
                                ├── L5: Routing, forecasting, optimization algorithms
                                └── L6-L9: Applications & advanced topics
```

## Internal Dependency Order

1. `logistics_core.h` → No dependencies (pure type definitions)
2. `inventory.h` → `logistics_core.h`
3. `transportation.h` → `logistics_core.h`
4. `warehouse.h` → `logistics_core.h`
5. `demand_forecast.h` → No dependencies
6. `scm_optimize.h` → `logistics_core.h`, `inventory.h`

## Cross-Module Integration

- **data-engine (7) → backend (8) → frontend (9)**: Logistics module provides shipment tracking data that flows through backend API to frontend dashboard
- **security (13)**: Supply chain data integrity verification
- **AI (14)**: Demand forecasting serves as baseline for ML-based demand sensing
