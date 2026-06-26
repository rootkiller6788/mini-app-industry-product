# Course Dependency Tree — mini-industry-solution

## Prerequisites (What This Module Depends On)

```
Mathematics (L0)
  ├── Calculus → PID derivatives, Newton-Raphson
  ├── Statistics → SPC, Weibull, process capability, z-score
  ├── Linear Algebra → Kalman filter, linear regression
  └── Probability → Reliability engineering, risk matrices

Computer Science Fundamentals
  ├── C Programming → All implementations
  ├── Data Structures → Circular buffer, linked lists (fault tree)
  ├── Algorithms → Sorting (Johnson, Pareto), scheduling (NEH, CDS)
  └── Computer Networks → Modbus framing, CRC/LRC

Control Theory
  ├── PID Control → Astrom & Hagglund (1995)
  ├── Process Dynamics → Ziegler-Nichols, Cohen-Coon
  └── Digital Control → Sample time, anti-windup

Industrial Engineering
  ├── Quality Control → Shewhart, Deming, Juran
  ├── Reliability → Weibull (1951), RCM
  ├── Safety → IEC 61508, HAZOP (Kletz)
  └── Operations → Goldratt (TOC), Johnson (1954)

Signal Processing
  ├── Digital Filters → SMA, EMA, WMA, Median
  ├── State Estimation → Kalman (1960)
  └── Sensor Physics → Thermocouples (Seebeck), RTD (C-V Dusen)
```

## Dependents (What Depends On This Module)

```
Cross-Module Integration:
  ├── data-engine (7) → Consumes industrial data model, sensor types
  ├── backend (8) → SCADA historian, data logger patterns
  ├── frontend (9) → HMI tag database, control chart visualization
  ├── security (13) → Safety instrumented systems audit
  ├── AI (14) → PdM feature extraction, anomaly detection patterns
  └── IoT (15) → Modbus/OPC UA protocol patterns

Industry Applications:
  ├── mini-agriculture-tech → Environmental monitoring, irrigation control
  ├── mini-energy-carbon → Grid monitoring, emissions tracking
  ├── mini-smart-manufacturing → MES, production scheduling
  ├── mini-healthcare-medical → Medical device monitoring, lab QC
  └── mini-logistics-supplychain → Fleet maintenance, quality tracking
```
