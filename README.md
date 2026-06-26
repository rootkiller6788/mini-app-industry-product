# Mini App Industry Product

A collection of **from-scratch, zero-dependency C implementations** of industry-grade computational kernels spanning finance, healthcare, manufacturing, energy, agriculture, logistics, e-commerce, government, and scientific research. Each sub-module maps to MIT, Stanford, and other top-tier university courses, translating real-world industrial algorithms into runnable C code.

## Sub-Modules

| Sub-Module | Topics | Key Courses |
|------------|--------|-------------|
| [mini-agriculture-tech](mini-agriculture-tech/) | Precision agriculture, GPS field mapping, crop modeling (GDD/NDVI), irrigation scheduling, remote sensing, soil analysis, WGEN weather generation | MIT 6.004, Stanford CS 229, UC Davis, Wageningen |
| [mini-ai-application](mini-ai-application/) | NLP pipeline (BPE, TF-IDF, NER), recommendation systems (collaborative filtering, matrix factorization), agent runtime with safety checking, inference server, AI platform | MIT 6.036, Stanford CS 224N, CS 229, CS 246, CS 329S |
| [mini-dev-tools-platform](mini-dev-tools-platform/) | CI/CD pipeline, CLI parser, code review, config manager (hot-reload), deploy manager (canary), feature flags (A/B rollout), logger, package manager, profiler, test runner | MIT 6.824, Stanford CS 144, CS 245, CMU SEI |
| [mini-ecommerce-retail](mini-ecommerce-retail/) | Inventory/order management, payment engine, full-text search (TF-IDF/BM25), fraud detection (Bayesian), event sourcing/CQRS, A/B testing, flash sale, rate limiting | MIT 6.858, Stanford CS 144, Berkeley CS 186, CMU 15-445 |
| [mini-energy-carbon](mini-energy-carbon/) | Solar PV modeling, wind power (Weibull), battery storage optimization, carbon accounting (GHG Protocol, IPCC AR6), energy markets, load forecasting (Holt-Winters), microgrid sizing | MIT 15.031J, Stanford CS 229, Berkeley CS 267, CMU 15-418 |
| [mini-enterprise-system](mini-enterprise-system/) | ERP core (MRP engine), CRM pipeline, financial reporting, HRM system, enterprise resource planning | MIT 6.004, SAP/Oracle University |
| [mini-fintech-blockchain](mini-fintech-blockchain/) | Double-entry bookkeeping, Black-Scholes/Monte Carlo options pricing, portfolio optimization (Markowitz/CAPM), risk management (VaR), bond pricing (Nelson-Siegel), Merkle tree blockchain, AMM/CBDC | MIT 15.433, MIT 15.450, Stanford MS&E 245G, ETH 363-0537 |
| [mini-future-computing](mini-future-computing/) | Quantum emulation, neuromorphic computing, optical computing, probabilistic (p-bit) computing, thermodynamic reversible computing, bio/DNA computing, post-Moore paradigms | MIT, Caltech, Nature 2016 |
| [mini-gov-smartcity](mini-gov-smartcity/) | Citizen 311 portal, service request routing, emergency management, traffic management, environmental monitoring, infrastructure modeling, ISO 37120/UN SDG 11 | MIT 11.520, NYU CUSP |
| [mini-healthcare-medical](mini-healthcare-medical/) | Clinical decision support (DSS), hospital operations (ADT), lab analyzer, patient monitoring (NEWS2), pharmacy management, drug interaction checking | MIT HST.542J, Stanford MED 277, BIOMEDIN 215, Harvard BMI 703 |
| [mini-industry-solution](mini-industry-solution/) | Industrial process control (PID), PLC protocol (Modbus), predictive maintenance, production scheduling, quality control (SPC), safety analysis (LOPA), ISA-88/ISA-95, SCADA | MIT 2.171, Stanford EE 264, Berkeley EECS 149, IEOR 160 |
| [mini-internet-app](mini-internet-app/) | HTTP/1.1 web server, REST API (JSON), URL router, content cache, session authentication, rate limiter, template engine | MIT 6.033, Stanford CS 142 |
| [mini-logistics-supplychain](mini-logistics-supplychain/) | Vehicle routing (TSP/VRP), demand forecasting, inventory optimization (EOQ/Newsvendor), warehouse management, transportation, supply chain dynamics (Bullwhip), green logistics | MIT 15.762, MIT CTL.SCx, MIT 15.053, Stanford MS&E 246 |
| [mini-scientific-research](mini-scientific-research/) | Experiment lifecycle, numerical methods, optimization, linear algebra, statistics, data provenance, FAIR data principles, reproducibility, meta-analysis | Stanford, Cambridge |
| [mini-smart-manufacturing](mini-smart-manufacturing/) | MES/SCADA core, OEE monitoring, production scheduling (Johnson's rule), quality control (SPC), smart manufacturing, supply chain integration | Georgia Tech ISyE, RWTH Aachen |

## Design Philosophy

- **Zero external dependencies** — pure C (C99/C11), only `libc` and `libm`
- **Self-contained modules** — each directory has its own `Makefile`, `include/`, `src/`, `examples/`, `demos/`, `tests/`, `docs/`
- **Industry algorithm kernels** — every module implements real computational cores (pricing engines, physics models, optimization solvers), not CRUD wrappers
- **Cross-module integration** — fintech references AI predictions, manufacturing integrates IoT sensors, e-commerce connects to security/risk engines

## Building

Each module is standalone. Navigate to a module directory and run:

```bash
cd mini-agriculture-tech
make all    # build everything
make test   # run tests
```

Requires **GCC** and **GNU Make**.

## Project Structure

```
mini-app-industry-product/
├── mini-agriculture-tech/         # Precision agriculture, crop modeling, remote sensing
├── mini-ai-application/           # NLP, recommendation systems, agent runtime, AI platform
├── mini-dev-tools-platform/       # CI/CD, loggers, profilers, config mgmt, feature flags
├── mini-ecommerce-retail/         # Inventory, payments, search, fraud detection, CQRS
├── mini-energy-carbon/            # Solar, wind, battery, carbon accounting, energy markets
├── mini-enterprise-system/        # ERP (MRP, finance), CRM, HRM
├── mini-fintech-blockchain/       # Options pricing, portfolio theory, risk, blockchain, DeFi
├── mini-future-computing/         # Quantum, neuromorphic, optical, thermodynamic computing
├── mini-gov-smartcity/            # Smart city governance, 311 portal, traffic/emergency mgmt
├── mini-healthcare-medical/       # Clinical DSS, hospital ops, lab analysis, patient monitoring
├── mini-industry-solution/        # ISA-95/88, PLC, SCADA, predictive maintenance, SPC
├── mini-internet-app/             # HTTP server, REST API, caching, auth, routing
├── mini-logistics-supplychain/    # VRP, inventory optimization, warehouse, SCM dynamics
├── mini-scientific-research/      # Numerical methods, optimization, FAIR data, reproducibility
└── mini-smart-manufacturing/      # MES, OEE, production scheduling, SPC, smart factory
```

## License

MIT
