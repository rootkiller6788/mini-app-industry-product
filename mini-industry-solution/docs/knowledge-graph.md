# Knowledge Graph — mini-industry-solution

## L1: Definitions (Complete ✅)

### Core Data Types
- `SensorReading` — Timestamped sensor measurement with quality/health metadata
- `ProcessVariable` — PID loop controlled parameter per ISA-5.1
- `ActuatorCommand` — Final control element output command
- `AlarmCondition` — Single alarm point per ISA-18.2
- `ControlLoop` — Full PID loop with tuning and anti-windup
- `DataLogger` — Circular buffer data historian
- `DigitalTwin` — Digital representation of physical asset
- `KalmanFilter1D` — 1D Kalman filter state

### PLC/Protocol Types
- `ModbusFrame` — Modbus PDU with RTU/ASCII/TCP framing
- `ModbusSlaveDevice` — Modbus slave device register map
- `ModbusRegisterMap` — Register-to-engineering-unit mapping
- `OPCUANode` — OPC UA data node model

### Predictive Maintenance Types
- `EquipmentProfile` — Physical asset with runtime/health tracking
- `FailureMode` — FMEA failure mode with RPN
- `TrendSeries` — Time-series with statistics and trend detection
- `BearingGeometry` — Bearing parameters for fault frequency calculation

### Quality Control Types
- `XBarRChart` — Shewhart X-bar/R control chart
- `CapabilityIndex` — Cp/Cpk/Pp/Ppk indices
- `QualityCharacteristic` — Part characteristic with tolerances
- `CUSUMChart` — Cumulative sum control chart
- `SupplierScorecard` — Supplier quality performance tracking

### Safety Analysis Types
- `Hazard` — Hazard with severity/likelihood/risk
- `RiskAssessment` — Full risk assessment with barriers
- `SafetyInstrumentedFunction` — SIF with SIL and PFD
- `HAZOPRecord` — HAZOP study record
- `FMEARecord` — FMEA analysis record
- `FTANode` — Fault tree node (AND/OR/basic)
- `BowTieDiagram` — Bow-tie analysis diagram

### Production Scheduling Types
- `Job` — Production job with operations and status
- `Machine` — Work center with status and utilization
- `Schedule` — Complete schedule with makespan/tardiness
- `WorkCenterLoad` — Capacity load analysis

### Enumerations
- `SignalType` (10 signal types), `SensorHealth` (7 states), `PVQuality` (5 levels)
- `AlarmPriority` (6 levels), `LoopMode` (6 modes)
- `ModbusMode` (3 modes), `ModbusFunctionCode` (9 codes), `ModbusExceptionCode` (10 codes)
- `EquipmentType` (10 types), `MaintenanceStrategy` (6 strategies)
- `ChartType` (9 types), `DispatchRule` (9 rules)
- `HAZOPGuideword` (11 guidewords), `BarrierType` (4 types)

## L2: Core Concepts (Complete ✅)

- Process control fundamentals: PID, open/closed loop, cascade, ratio, feedforward
- Sensor validation: range check, NAMUR NE 43 fault detection, deadband hysteresis
- Master-slave protocol: Modbus polling cycle, function codes, exception handling
- Condition-based maintenance (CBM): P-F curve, degradation patterns, health scoring
- Common cause vs. special cause variation: Shewhart control chart philosophy
- ALARP principle: risk reduction to "as low as reasonably practicable"
- Little's Law & Theory of Constraints: WIP-throughput-flow time relationship

## L3: Engineering Structures (Complete ✅)

- PID control loop pipeline: Read → Compute → Output → Track
- Sensor mesh topology: Multi-sensor fusion with quality indicators
- Frame serialization pipeline: Build → CRC → Transmit / Receive → Parse → Validate
- Condition monitoring pipeline: Acquire → Filter → Baseline → Detect → Diagnose
- Control chart data pipeline: Sample → Compute Statistics → Check Rules → Signal
- Safety lifecycle (IEC 61511): Analysis → Realization → Operation
- Job shop scheduling pipeline: Release → Dispatch → Process → Complete

## L4: Standards/Theorems (Complete ✅)

### Process Control & Automation
- ISA-88 Batch Control: process cell, unit, equipment module hierarchy
- ISA-95 Enterprise-Control Integration: Level 0-4 automation pyramid
- IEC 61131-3: PLC programming languages (LD, FBD, ST, IL, SFC)
- NAMUR NE 43: 4-20mA signal fault detection (3.6mA/21mA limits)

### Communication
- Modbus Protocol (Modicon, 1979): RTU/ASCII/TCP framing
- IEC 62541 OPC UA: Information model, address space
- IEC 61508/61511: Functional safety, SIL determination, PFD calculation

### Reliability & Quality
- Weibull Distribution (1951): Scale/shape parameter, reliability function
- ISO 14224 RCM: Reliability-centered maintenance
- ISO 13374 CM&D: Condition monitoring and diagnostics
- Western Electric Rules (1956): 8 rules for SPC out-of-control
- ISO 22514: Process capability indices
- Six Sigma: 3.4 DPMO target, 1.5σ shift
- AIAG SPC Manual: Control chart construction
- AIAG MSA Manual: Gauge R&R analysis

### Safety
- ISO 31000: Risk management framework
- ISA 84.00.01: Safety instrumented systems
- MIL-STD-1629A: FMECA procedure

### Scheduling
- Little's Law: L = λ × W
- Theory of Constraints (Goldratt, 1984): Five focusing steps, DBR

## L5: Algorithms/Methods (Complete ✅)

### Signal Processing
- Kalman filter (1D): Predict-correct cycle with process/measurement noise
- SMA filter: Running sum optimization, O(1) amortized
- WMA filter: Linearly weighted moving average
- EMA filter: 1st-order IIR low-pass, α smoothing factor
- Median filter: Order-statistic noise rejection

### Thermocouple/RTD
- ITS-90 Type K inverse: Newton-Raphson on 10th-order polynomial with exponential correction
- ITS-90 Type J inverse: Newton-Raphson on 5th-order polynomial
- Callendar-Van Dusen RTD: Quadratic (T≥0) / Newton-Raphson quartic (T<0)

### Communications
- CRC-16 Modbus: Bit-wise polynomial division (0xA001 reflected)
- LRC: Two's complement of byte sum
- Modbus slave dispatch: Function code routing with exception handling

### PID Tuning
- Ziegler-Nichols Open-Loop: Process reaction curve, (1/K)×1.2×(T/L)
- Cohen-Coon: Self-regulating processes, larger deadtime
- Lambda (IMC): Robustness-focused tuning

### Reliability & Maintenance
- Weibull parameter estimation: Rank regression on X (RRX), median rank approximation
- Gamma function: Lanczos approximation for MTTF
- Maintenance optimization: Cost rate minimization with Weibull reliability
- Bearing fault frequencies: BPFO/BPFI/BSF/FTF from geometry
- Z-score anomaly detection: Statistical outlier identification
- RUL estimation: Linear, exponential degradation models

### Quality Control
- X-bar/R chart statistics: Grand mean, mean range, A2/D3/D4 constants
- WECO rules: 8-rule pattern detection (trending, runs, stratification, mixture)
- Process capability: Cp = (USL-LSL)/(6σ), Cpk = min(Cpu, Cpl)
- Pareto analysis: Sort-descend, cumulative percentage
- Gauge R&R: ANOVA-based repeatability/reproducibility
- CUSUM: Cumulative sum with decision interval
- Process sigma level: Inverse normal CDF approximation

### Safety Analysis
- HAZOP guidewords: NO, MORE, LESS, AS WELL AS, PART OF, REVERSE, OTHER THAN
- Risk matrix: 5×4 likelihood-severity lookup
- LOPA: IPL PFD multiplication, RRF gap calculation
- FMEA RPN: Severity × Occurrence × Detection
- Fault Tree: AND/OR gate probability propagation
- Event Tree: Path probability multiplication
- Bow-Tie: Preventive + Mitigative barrier assessment

### Production Scheduling
- Johnson's Rule: 2-machine optimal, O(n log n), A-set + B-set partitioning
- NEH heuristic: Constructive insertion heuristic, O(n³m)
- CDS heuristic: m-machine to (m-1) 2-machine reduction
- Priority dispatch: SPT/EDD/CR/SLACK/FIFO/LPT/LIFO/WINQ/COVERT
- Bottleneck identification: Highest utilization resource
- DBR scheduling: Bottleneck-first with rope release

## L6: Canonical Problems (Complete ✅)

1. Industrial I/O Data Pipeline — Sensor → Validate → Filter → Log → Alarm
2. PID Control Loop — Setpoint tracking with anti-windup and derivative filtering
3. Alarm Management — Trip point, deadband, nuisance alarm detection, flood detection
4. Modbus Master/Slave — Frame construction, CRC validation, register read/write
5. Bearing Degradation Prediction — Fault frequency diagnosis from vibration spectrum
6. Rotating Equipment RUL — Linear degradation with Weibull reliability
7. Statistical Process Control — X-bar/R chart with WECO rule violation detection
8. Process Capability Assessment — Cp/Cpk calculation with subgrouped data
9. Layer of Protection Analysis — Initiating event → IPL chain → SIL gap
10. Job Shop Scheduling — Makespan minimization with Johnson/NEH/CDS/dispatch
11. Safety Instrumented Function — PFD calculation, SIL verification
12. Fault Tree Analysis — Probability propagation through AND/OR gates

## L7: Applications (Complete ✅, 5+)

1. **SCADA Data Pipeline** — Full end-to-end: sensor acquisition → filtering → logging → alarming → PID control (demo_scada.c)
2. **Industrial IoT Gateway** — Modbus RTU/TCP frame handling with OPC UA node mapping
3. **Fleet Maintenance System** — Priority sorting, due-date scheduling, overdue filtering
4. **Quality Management Dashboard** — Control charts with WECO violation tracking, capability indices, supplier scorecard
5. **Safety Instrumented System Design** — HAZOP → LOPA → SIL target → SIF PFD verification
6. **Production Scheduler** — Job release, priority dispatch, machine utilization, bottleneck DBR

## L8: Advanced Topics (Partial ✅)

**Implemented:**
1. Digital Twin synchronization (Grieves & Vickers 2017 model)
2. CUSUM/EWMA control charts (Page 1954) for small-shift detection
3. Safety Integrity Level analysis (IEC 61508/61511)
4. Multi-rate Kalman filter sensor fusion

**Documented:**
5. Constraint programming for scheduling optimization
6. Simulated annealing / genetic algorithms for JSSP
7. STPA/STAMP (Leveson, 2011) system-theoretic safety
8. PCA feature extraction for vibration-based diagnostics

## L9: Industry Frontiers (Partial ✅, documented)

1. **Industry 4.0 RAMI 4.0** — Reference Architecture Model Industrie 4.0
2. **OPC UA Pub/Sub** — Publish-subscribe for real-time industrial data
3. **Time-Sensitive Networking (TSN)** — Deterministic Ethernet for industrial control
4. **Federated Learning for Fleet PdM** — Privacy-preserving predictive maintenance
5. **AI Visual Quality Inspection** — Deep learning for defect detection
6. **Autonomous Safety Systems** — AI-driven safety decisions
7. **Advanced Physical Layer (APL)** — Ethernet to field devices
8. **Digital Twin with Real-Time Sync** — Closed-loop digital twin
