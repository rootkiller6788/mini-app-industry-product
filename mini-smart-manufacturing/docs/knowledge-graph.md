# Knowledge Graph — mini-smart-manufacturing

## L1: Definitions ✅ Complete
- SMF_Factory, SMF_Machine, SMF_Tool, SMF_Material, SMF_Product
- SMF_BOMItem, SMF_BillOfMaterials
- SMF_RouteStep, SMF_Routing
- SMF_Workstation, SMF_ProductionLine
- SMF_WorkOrder, SMF_WorkOrderState
- SMF_SchedulingJob, SMF_Schedule, SMF_DispatchRule
- SMF_SPCSample, SMF_SPCSubgroup, SMF_ControlChart, SMF_ChartType
- SMF_InspectionRecord, SMF_ParetoItem, SMF_RootCauseAnalysis
- SMF_OEEMetrics, SMF_MachineEvent, SMF_LossAnalysis
- SMF_MaintenanceJob, SMF_FMEARecord
- SMF_MESTag, SMF_ProductionShift, SMF_ShiftReport
- SMF_Andon, SMF_ProductGenealogy, SMF_PokaYoke
- SMF_MRPItem, SMF_MRPPlan, SMF_PlannedOrder
- SMF_Supplier, SMF_Kanban, SMF_DemandForecast, SMF_ABCItem

## L2: Core Concepts ✅ Complete
- Lean Manufacturing (JIT, TPS, Kaizen, 5S)
- Six Sigma DMAIC methodology
- OEE = Availability × Performance × Quality
- TPM 8 Pillars (Nakajima 1988)
- MRP II / Manufacturing Resource Planning
- Kanban Pull System
- Poka-Yoke (Error Proofing, Shingo)
- 5-Why Root Cause Analysis (Toyota)
- Six Big Losses (Nakajima)
- Andon System (Toyota Production System)
- ISA-95 Automation Pyramid (Levels 0-4)
- MESA-11 MES Functional Model

## L3: Engineering Structures ✅ Complete
- BOM Gozinto Graph (Vazsonyi 1954)
- Routing Chain with precedence constraints
- Factory → Line → Workstation → Machine topology
- Andon escalation matrix (0→3 levels)
- SPC control chart pipeline (data → subgroups → limits → rules)
- MRP time-phased record structure
- Kanban card loop (source → destination)
- Product genealogy / traceability chain
- Shift management / production reporting pipeline

## L4: Standards/Theorems ✅ Complete
- ISA-95 (IEC 62264) — Enterprise-Control System Integration
- ISO 22400 — KPIs for Manufacturing Operations (OEE definition)
- ISO 9001:2015 — Quality Management Systems
- ISO 7870-2 — Control Charts (Shewhart)
- ISO 22514-4 — Process Capability (Cp/Cpk/Pp/Ppk)
- ISO 2859 — AQL Sampling Plans
- Johnson's Theorem (1954) — F2||C_max optimality
- Smith's Rule (1956) — SPT optimal for 1||ΣC_j
- APICS CPIM / SCOR Model
- ANSI/ASQ Z1.4 — Sampling Procedures

## L5: Algorithms/Methods ✅ Complete
- Johnson's Rule: F2||C_max optimal O(n log n)
- Giffler-Thompson: active schedule generation
- Dispatching: FIFO, SPT, LPT, EDD, MS, CR
- BOM Explosion: level-by-level quantity rollup
- EOQ: Q* = √(2DS/H) (Harris 1913)
- Silver-Meal heuristic: dynamic lot sizing
- MRP Regeneration: LLC ordering, net requirements
- X-bar/R Control Limits: Shewhart (1931)
- Western Electric Rules: 8 pattern detectors
- Cp/Cpk/Pp/Ppk computation
- Pareto Analysis: descending sort + cumulative %
- Weibull MLE: Rank Regression (Bernard)
- Exponential Smoothing: SES, Holt, Holt-Winters
- ABC Classification: Pareto 80/20 principle
- Largest Candidate Rule (LCR): line balancing
- Kanban card count: Toyota formula

## L6: Canonical Problems ✅ Complete
- n-job m-machine scheduling (Johnson, Giffler-Thompson)
- X-bar/R SPC chart generation
- OEE monitoring dashboard
- Product genealogy / traceability (forward/backward)
- MRP regeneration (Orlicky 1975)
- Capacity Requirements Planning (CRP)
- Production line balancing (LCR)
- Safety stock optimization
- Demand forecasting evaluation

## L7: Applications ✅ Complete (3 implemented)
- OEE Dashboard Simulation (example_oee.c)
- Production Scheduling Demo (example_scheduling.c)
- SPC & Quality Control Demo (example_spc.c)
- Smart factory digital twin architecture
- MES real-time operator dashboard

## L8: Advanced Topics ✅ Partial (1 implemented)
- Predictive Maintenance: Weibull reliability analysis ✓
- DDMRP: Demand Driven MRP (documented)
- IIoT Edge Computing: MES data tags ✓
- CPS Integration: ISA-95 Level 2 connectivity patterns
- Multivariate SPC: Hotelling T² (documented)

## L9: Industry Frontiers ✅ Partial (documented)
- Siemens MindSphere: IIoT platform architecture
- GE Proficy: MES/SCADA patterns
- Siemens Opcenter: MOM platform
- Industry 4.0 RAMI 4.0: Reference Architecture Model
- OPC-UA Unified Architecture: IEC 62541
