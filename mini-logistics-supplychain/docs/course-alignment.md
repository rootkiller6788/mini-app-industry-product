# Course Alignment — mini-logistics-supplychain

## MIT

| Course | Topic | Module Implementation |
|--------|-------|----------------------|
| 15.762 Supply Chain Planning | EOQ, Newsvendor, Bullwhip, MRP | `src/inventory.c`, `src/scm_optimize.c` |
| CTL.SC1x Supply Chain Fundamentals | Inventory basics, transportation | `src/inventory.c`, `src/transportation.c` |
| CTL.SC2x Supply Chain Design | Network design, facility location | `src/scm_optimize.c` |
| CTL.SC4x Supply Chain Dynamics | System dynamics, bullwhip | `src/scm_optimize.c::bullwhip_analyze` |
| 15.053 Optimization Methods | TSP, VRP, facility location | `src/transportation.c`, `src/scm_optimize.c` |

## Stanford

| Course | Topic | Module Implementation |
|--------|-------|----------------------|
| MS&E 246 Supply Chain Management | Inventory policies, forecasting | `src/inventory.c`, `src/demand_forecast.c` |
| MS&E 262 SCM Technology | Green logistics, CO2 tracking | `src/scm_optimize.c::co2_emissions` |

## Georgia Tech

| Course | Topic | Module Implementation |
|--------|-------|----------------------|
| ISYE 6333 Supply Chain Engineering | VRP, network design, optimization | `src/transportation.c`, `src/scm_optimize.c` |
| ISYE 6201 Inventory & Supply Chain | EOQ, safety stock, newsvendor | `src/inventory.c` |
| ISYE 6402 Time Series Analysis | Exponential smoothing, Holt-Winters | `src/demand_forecast.c` |
| ISYE 6203 Warehouse Systems | Slotting, picking, layout | `src/warehouse.c` |

## CMU

| Course | Topic | Module Implementation |
|--------|-------|----------------------|
| 45-870 Supply Chain Management | Core SCM concepts | All modules |
| 45-871 Logistics & Transportation | Fleet, routing, mode selection | `src/transportation.c` |
| 45-875 Supply Chain Optimization | Multi-echelon, MRP | `src/scm_optimize.c` |
| 45-849 Warehousing & Distribution | Warehouse design, picking | `src/warehouse.c` |

## ETH Zurich

| Course | Topic | Module Implementation |
|--------|-------|----------------------|
| 363-0541 Computational Optimization | Dijkstra, TSP, 2-opt | `src/transportation.c` |

## Cambridge

| Course | Topic | Module Implementation |
|--------|-------|----------------------|
| Part II: Operations Management | Inventory models, forecasting | `src/inventory.c`, `src/demand_forecast.c` |

## Tsinghua (清华)

| Course | Topic | Module Implementation |
|--------|-------|----------------------|
| 物流与供应链管理 (Logistics & SCM) | Full supply chain | All modules |
| 运筹学 (Operations Research) | Graph algorithms, optimization | `src/transportation.c` |

## Berkeley

| Course | Topic | Module Implementation |
|--------|-------|----------------------|
| IEOR 151 Service Operations | Little's Law, queueing | `src/warehouse.c` |
| IEOR 153 Logistics Network Design | Facility location | `src/scm_optimize.c` |
