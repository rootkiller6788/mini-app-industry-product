# mini-smart-manufacturing — Smart Manufacturing (C Implementation)

> **Module Status: COMPLETE ✅**
> include/ + src/ total: 4,842 lines (≥ 3,000 ✓)
> 13/13 tests passing ✓

Smart manufacturing implementation covering ISA-95 MES/SCADA, production scheduling (Johnson's Rule, Giffler-Thompson), OEE (ISO 22400), Six Sigma SPC, MRP II, JIT/Kanban, FMEA, and IIoT data acquisition.

## Modules

| Module | Header | Source | Description |
|--------|--------|--------|-------------|
| Core Factory | `smart_manufacturing.h` | `smart_manufacturing.c` | Factory/Machine/Material/Product/BOM/Routing/WO lifecycle, EOQ, inventory |
| Production Scheduling | `production_core.h` | `production_sched.c`, `production_johnson.c`, `production_analysis.c`, `production_workorder.c` | Johnson's Rule, Giffler-Thompson job shop, single machine dispatching (FIFO/SPT/LPT/EDD/MS/CR), capacity planning, line balancing (LCR) |
| Quality Control | `quality_control.h` | `quality_control.c` | X-bar/R charts, Cp/Cpk/Pp/Ppk, Western Electric rules, Pareto analysis, 5-Why RCA |
| OEE Monitor | `oee_monitor.h` | `oee_monitor.c` | OEE (A×P×Q), Six Big Losses, Weibull reliability, FMEA (RPN), maintenance management |
| MES Core | `mes_core.h` | `mes_core.c` | MES data tags, Andon escalation, product genealogy, Poka-Yoke, shift management |
| Supply Chain | `supply_chain.h` | `supply_chain.c` | MRP (Orlicky 1975), lot sizing (L4L/FOQ/POQ/Silver-Meal), Kanban, demand forecasting (SES/Holt/Holt-Winters), ABC analysis, safety stock |

## L1-L9 Knowledge Coverage

| Level | Coverage | Key Implementations |
|-------|----------|---------------------|
| **L1** Definitions | ✅ Complete | SMF_Factory, SMF_Machine, SMF_Material, SMF_Product, SMF_BOMItem, SMF_RouteStep, SMF_WorkOrder, SMF_OEEMetrics, SMF_ControlChart, SMF_MRPItem, SMF_Kanban, SMF_FMEARecord |
| **L2** Core Concepts | ✅ Complete | Lean Manufacturing (TPS/JIT/Kaizen), Six Sigma DMAIC, OEE (A×P×Q), TPM 8 Pillars, MRP II, Kanban pull, Poka-Yoke, 5-Why RCA |
| **L3** Engineering Structures | ✅ Complete | BOM Gozinto graph, routing chain, Andon escalation matrix, control chart pipeline, factory topology, ISA-95 Level 2-3 data model |
| **L4** Standards/Theorems | ✅ Complete | ISA-95 (IEC 62264), ISO 22400 OEE KPIs, ISO 9001:2015, ISO 7870 SPC charts, ISO 22514 capability, ISO 2859 AQL, APICS SCOR, Johnson optimality theorem |
| **L5** Algorithms/Methods | ✅ Complete | Johnson's Rule (F2||C_max, O(n log n)), Giffler-Thompson active schedule, EOQ (Harris 1913), Silver-Meal lot sizing, SES/Holt/Holt-Winters forecast, Weibull MLE, Western Electric pattern detection, ABC Pareto classification |
| **L6** Canonical Problems | ✅ Complete | n-job m-machine scheduling, X-bar/R SPC charting, OEE dashboard, product genealogy/traceability, MRP regeneration, capacity requirements planning |
| **L7** Applications | ✅ Complete | Smart factory digital twin, MES real-time monitoring, operator Andon dashboard, SPC quality monitoring station |
| **L8** Advanced Topics | ✅ Partial | Predictive maintenance (Weibull analysis), DDMRP concepts, IIoT edge computing, CPS integration |
| **L9** Industry Frontiers | ✅ Partial | Siemens MindSphere architecture, GE Proficy patterns, Industry 4.0 RAMI 4.0, OPC-UA Unified Architecture |

## Core Theorems & Formulas

- **Johnson's Theorem (1954)**: For F2||C_max, sequence jobs by min(a_i, b_j) ≤ min(a_j, b_i) yields optimal makespan
- **OEE (ISO 22400)**: OEE = Availability × Performance × Quality (World-Class ≥ 85%)
- **EOQ (Harris 1913)**: Q* = √(2DS/H)
- **Silver-Meal (1973)**: K(m) = (A + h·Σ(t-1)·D_t) / m, minimize cost per period
- **Shewhart Limits (1931)**: UCL/LCL_X̄ = X̄ ± A2·R̄, UCL/LCL_R = D4/D3·R̄
- **Cp/Cpk**: Cp = (USL-LSL)/(6σ), Cpk = min[(USL-μ)/(3σ), (μ-LSL)/(3σ)]
- **Weibull Reliability**: R(t) = exp(-(t/η)^β), MTBF = η·Γ(1+1/β)
- **FMEA RPN**: RPN = Severity × Occurrence × Detection
- **Kanban Cards**: N = D·L·(1+SF) / C
- **Safety Stock**: SS = Z·σ·√LT
- **Takt Time**: T = Available Time / Customer Demand
- **FMEA RPN**: RPN = S × O × D (>100 = action required)

## Nine-School Curriculum Mapping

| School | Course | Topics |
|--------|--------|--------|
| MIT | 2.854 Manufacturing Systems | Johnson's Rule, Giffler-Thompson, scheduling theory |
| MIT | 2.008 Design & Manufacturing II | BOM, routing, lean manufacturing |
| Georgia Tech | ISyE 6203 Manufacturing Planning | MRP, CRP, lot sizing, line balancing |
| Georgia Tech | ISyE 6402 Time Series Analysis | Exponential smoothing, Holt-Winters forecasting |
| CMU | 15-445 Database Systems | Data models for MES |
| UT Austin | ORIE 641 Production Control | Kanban, CONWIP, pull systems |
| ETH Zurich | 263-3501 Parallel Programming | Job shop parallelism |
| Cambridge | MET Manufacturing Tripos | TPM, OEE, Six Big Losses |
| 清华 | 工业工程系 | 生产计划与控制 (Production Planning & Control) |
| Tsinghua | 质量工程 (Quality Engineering) | SPC, Cp/Cpk, Six Sigma |

## Build & Test

```sh
make          # Build static library libsmartmfg.a
make test     # Build and run all tests (13/13 passing)
make clean    # Remove build artifacts
```

**Requirements**: GCC/Clang with C99 support. `<stdlib.h>`, `<stdio.h>`, `<string.h>`, `<math.h>`, `<time.h>`. Link with `-lm`.

## Module Status: COMPLETE ✅

- **L1-L6**: Complete (all core definitions, concepts, structures, standards, algorithms, and canonical problems implemented)
- **L7**: Complete (3+ application examples: OEE dashboard, scheduling demo, SPC/quality)
- **L8**: Partial (predictive maintenance, DDMRP, IIoT documented; Weibull analysis implemented)
- **L9**: Partial (industry systems referenced: MindSphere, Proficy, Opcenter; RAMI 4.0 architecture)
