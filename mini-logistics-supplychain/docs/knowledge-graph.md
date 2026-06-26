# Knowledge Graph — mini-logistics-supplychain

## L1: Core Definitions ✅ COMPLETE

| Entry | Type | Location |
|-------|------|----------|
| `sku_t` | Struct | `include/logistics_core.h` |
| `order_t` | Struct | `include/logistics_core.h` |
| `order_line_t` | Struct | `include/logistics_core.h` |
| `facility_t` | Struct | `include/logistics_core.h` |
| `shipment_t` | Struct | `include/logistics_core.h` |
| `route_t` | Struct | `include/logistics_core.h` |
| `route_stop_t` | Struct | `include/logistics_core.h` |
| `carrier_t` | Struct | `include/logistics_core.h` |
| `geo_location_t` | Struct | `include/logistics_core.h` |
| `address_t` | Struct | `include/logistics_core.h` |
| `sc_link_t` | Struct | `include/logistics_core.h` |
| `sc_kpi_t` | Struct | `include/logistics_core.h` |
| `inventory_record_t` | Struct | `include/logistics_core.h` |
| `order_status_t` | Enum | `include/logistics_core.h` |
| `shipment_mode_t` | Enum | `include/logistics_core.h` |
| `facility_type_t` | Enum | `include/logistics_core.h` |
| `priority_t` | Enum | `include/logistics_core.h` |
| `abc_class_t` | Enum | `include/logistics_core.h` |
| `eoq_params_t` / `eoq_result_t` | Struct | `include/inventory.h` |
| `safety_stock_params_t` | Struct | `include/inventory.h` |
| `cost_layer_t` / `cost_layer_stack_t` | Struct | `include/inventory.h` |
| `abc_item_t` | Struct | `include/inventory.h` |
| `echelon_node_t` | Struct | `include/inventory.h` |
| `vehicle_t` | Struct | `include/transportation.h` |
| `transport_graph_t` | Struct | `include/transportation.h` |
| `vrp_solution_t` / `vrp_route_t` | Struct | `include/transportation.h` |
| `load_plan_t` / `load_item_t` | Struct | `include/transportation.h` |
| `warehouse_layout_t` | Struct | `include/warehouse.h` |
| `storage_location_t` | Struct | `include/warehouse.h` |
| `pick_list_t` / `pick_item_t` | Struct | `include/warehouse.h` |
| `forecast_model_t` | Enum | `include/demand_forecast.h` |
| `sma_model_t` / `wma_model_t` | Struct | `include/demand_forecast.h` |
| `ses_model_t` / `holt_model_t` | Struct | `include/demand_forecast.h` |
| `holt_winters_model_t` | Struct | `include/demand_forecast.h` |
| `bom_t` / `bom_component_t` | Struct | `include/scm_optimize.h` |
| `mrp_plan_t` / `mrp_record_t` | Struct | `include/scm_optimize.h` |
| `network_design_t` | Struct | `include/scm_optimize.h` |
| `bullwhip_echelon_t` | Struct | `include/scm_optimize.h` |

## L2: Core Concepts ✅ COMPLETE

| Concept | Implementation | Location |
|---------|---------------|----------|
| Inventory Management | EOQ, safety stock, ROP | `src/inventory.c` |
| FIFO/LIFO Cost Tracking | Cost layer stack | `src/inventory.c` |
| ABC Classification | Pareto-based classification | `src/inventory.c` |
| Transportation Mode Selection | Decision matrix | `src/transportation.c` |
| Freight Cost Estimation | Mode-specific rate structures | `src/transportation.c` |
| Warehouse Slotting | ABC slotting | `src/warehouse.c` |
| Pick Path Strategies | S-shape, sort-by-location | `src/warehouse.c` |
| Batch Picking | SKU-based batching | `src/warehouse.c` |
| Demand Forecasting | SMA, WMA, SES, Holt, HW | `src/demand_forecast.c` |
| MRP | Material Requirements Planning | `src/scm_optimize.c` |
| CO2 Emissions Tracking | GLEC Framework factors | `src/scm_optimize.c` |

## L3: Engineering Structures ✅ COMPLETE

| Structure | Purpose | Location |
|-----------|---------|----------|
| Binary Min-Heap | Priority queue for Dijkstra | `src/transportation.c` |
| Adjacency List Graph | Transport network | `src/transportation.c` |
| Cost Layer Stack | FIFO/LIFO cost tracking | `src/inventory.c` |
| 2D Warehouse Grid | Storage location layout | `src/warehouse.c` |
| Circular Buffer | SMA sliding window | `src/demand_forecast.c` |

## L4: Standards/Theorems ✅ COMPLETE

| Theorem/Standard | Formula | Verification |
|-----------------|---------|-------------|
| Harris-Wilson EOQ | Q* = √(2DS/H) | `tests/test_all.c::test_eoq` |
| Haversine Formula | d = 2R·atan2(√a, √(1-a)) | `tests/test_all.c::test_haversine` |
| Safety Stock Formula | SS = Z·σ·√LT | `tests/test_all.c::test_safety_stock` |
| Newsvendor Critical Ratio | F(Q*) = cu/(cu+co) | `tests/test_all.c::test_newsvendor` |
| Little's Law | L = λ·W | `tests/test_all.c::test_littles_law` |
| Square Root Law (Risk Pooling) | SS_cent = SS_each·√N | `tests/test_all.c::test_risk_pooling` |
| Bullwhip Effect | Var(Orders)/Var(Demand) | `tests/test_all.c::test_bullwhip` |
| Linear Regression (OLS) | b = Σ(x-x̄)(y-ȳ)/Σ(x-x̄)² | `tests/test_all.c::test_linear_regression` |
| Dijkstra Optimality | Greedy with non-neg weights | `tests/test_all.c::test_dijkstra` |

## L5: Algorithms/Methods ✅ COMPLETE

| Algorithm | Complexity | Location |
|-----------|-----------|----------|
| EOQ Computation | O(1) | `src/inventory.c` |
| ABC Classification Sort | O(n log n) | `src/inventory.c` |
| FIFO Cost Draw | O(k) layers | `src/inventory.c` |
| LIFO Cost Draw | O(k) layers | `src/inventory.c` |
| Newsvendor Optimal Qty | O(1) | `src/inventory.c` |
| Dijkstra Shortest Path | O((V+E) log V) | `src/transportation.c` |
| Floyd-Warshall APSP | O(V³) | `src/transportation.c` |
| TSP Nearest Neighbor | O(n²) | `src/transportation.c` |
| TSP 2-opt Improvement | O(n²) per iter | `src/transportation.c` |
| Clarke-Wright VRP | O(n² log n) | `src/transportation.c` |
| 0/1 Knapsack (2-constraint) | O(n·W) | `src/transportation.c` |
| ABC Slotting | O(n log n) | `src/warehouse.c` |
| S-Shape Pick Path | O(n log n) | `src/warehouse.c` |
| Batch Picking Grouping | O(n log n) | `src/warehouse.c` |
| SMA/WMA | O(1) update | `src/demand_forecast.c` |
| Single Exp Smoothing | O(1) update | `src/demand_forecast.c` |
| Holt Double ES | O(1) update | `src/demand_forecast.c` |
| Holt-Winters Triple ES | O(1) update | `src/demand_forecast.c` |
| MRP Explosion | O(T) periods | `src/scm_optimize.c` |
| Pareto Frontier Filter | O(n²) | `src/scm_optimize.c` |
| Greedy Facility Location | O(F²·D) | `src/scm_optimize.c` |

## L6: Canonical Problems ✅ COMPLETE

| Problem | Example |
|---------|---------|
| Inventory Optimization (EOQ + Safety Stock + ABC) | `examples/example_inventory.c` |
| Transportation/Route Optimization (Dijkstra + VRP) | `examples/example_routing.c` |
| Warehouse Operations (Slotting + Pick Path) | `examples/example_warehouse.c` |
| End-to-End Supply Chain Demo | `demos/supply_chain_demo.c` |

## L7: Applications ✅ COMPLETE (3 applications)

| Application | Location |
|-------------|----------|
| Regional Distribution Network | `demos/supply_chain_demo.c` (Phases 3-4) |
| CO2 Sustainability Tracking | `demos/supply_chain_demo.c` (Phase 5) |
| Supply Chain KPI Dashboard | `demos/supply_chain_demo.c` (Phase 6) |

## L8: Advanced Topics ✅ PARTIAL+ (4 implemented)

| Topic | Implementation |
|-------|---------------|
| Multi-Echelon Inventory (GST) | `src/scm_optimize.c::guaranteed_service_time` |
| Bullwhip Effect Analysis | `src/scm_optimize.c::bullwhip_analyze` |
| Green Logistics Pareto Frontier | `src/scm_optimize.c::green_pareto_frontier` |
| Risk Pooling (Square Root Law) | `src/scm_optimize.c::risk_pooling_savings` |

## L9: Industry Frontiers ✅ PARTIAL (documented)

| Topic | Status |
|-------|--------|
| Autonomous Delivery (mode selection includes DRONE/AUTONOMOUS) | Documented in `include/logistics_core.h` |
| Blockchain Supply Chain Traceability | Documented in gap-report.md |
| AI Demand Sensing (Holt-Winters as baseline) | Implementable extension documented |
| Digital Twin Simulation Framework | Documented in gap-report.md |
