# mini-energy-carbon — Smart Energy & Carbon Management

C99 implementation of renewable energy modeling, carbon footprint
accounting, battery storage optimization, load forecasting, and
energy market simulation.

## Status: COMPLETE ✅

**include/ + src/ = 3569 lines** (target: 3000) ✅

## Knowledge Coverage

| Level | Name | Status | Key Items |
|-------|------|--------|-----------|
| **L1** | Definitions | ✅ Complete | 15+ structs (generator, battery, timeslot, PV module, Weibull, LCA, PPA, VPP, etc.), 10+ enums, 120+ API declarations |
| **L2** | Core Concepts | ✅ Complete | Solar PV model, wind power curve, battery degradation, carbon accounting, energy arbitrage, demand response |
| **L3** | Engineering Structures | ✅ Complete | Merit order dispatch, power flow modeling, microgrid topology, energy balance engine |
| **L4** | Standards/Theorems | ✅ Complete | GHG Protocol (Scope 1/2/3), IPCC AR6 GWP100, Betz Limit (16/27), Shockley-Queisser Limit (33.7%), Carnot efficiency, Rankine cycle, NREL LCOE methodology, Ohm's/Kirchhoff's laws |
| **L5** | Algorithms/Methods | ✅ Complete | Holt-Winters triple exp smoothing, Weibull MLE/MoM, Kalman filter SOC, HDKR irradiance model, single-diode PV model, DP battery dispatch, PSO, Simplex LP, Pareto front, Monte Carlo reliability |
| **L6** | Canonical Problems | ✅ Complete | Microgrid sizing, carbon accounting, renewable integration, energy system hourly simulation |
| **L7** | Applications | ✅ Complete (6+) | Building energy (HDD/CDD), heat pump COP model, PPA valuation, REC trading, EU-ETS/CBAM, feed-in tariff, LCA, demand response, TOU optimization |
| **L8** | Advanced Topics | ✅ Partial+ | Kalman filter SOC, VPP aggregation, probabilistic ensemble forecasting, rainflow cycle counting, Jensen wake model, Gumbel extreme value |
| **L9** | Industry Frontiers | ✅ Partial | Green hydrogen electrolyzer/fuel cell, SBTi 1.5C alignment, LDES evaluation |

## Core Definitions (L1)

- `energy_system_t` — Complete microgrid with generators, battery, grid PCC, timeseries
- `energy_generator_t` — Generator with cost, emissions, ramp rates, must-run status
- `energy_battery_t` — Battery with chemistry-specific degradation, Kalman SOC, thermal model
- `energy_timeslot_t` — Hourly energy balance with solar/wind/import/export/storage
- `energy_solar_geometry_t` — Sun position (declination, elevation, azimuth)
- `energy_weibull_t` — Wind resource statistics (k, c parameters)
- `energy_pv_module_t` — PV module with single-diode model parameters
- `energy_ppa_t` — Power purchase agreement contract model
- `energy_carbon_offset_t` — Carbon credit with quality assessment

## Core Theorems (L4)

- **Betz Limit**: Cp_max = 16/27 ≈ 0.5926 (wind turbine max efficiency)
- **Shockley-Queisser Limit**: η_max ≈ 33.7% for single-junction Si at 1.12eV
- **Carnot Efficiency**: η = 1 − T_cold/T_hot (maximum heat engine efficiency)
- **GHG Protocol**: Scope 1 (direct), Scope 2 (purchased electricity), Scope 3 (value chain)
- **IPCC AR6 GWP100**: CO₂=1, CH₄=28, N₂O=273, SF₆=25,200
- **Ohm's Law**: P_loss = I²R (transmission loss calculation)

## Core Algorithms (L5)

- **Holt-Winters** (additive + multiplicative) — Triple exponential smoothing for load forecasting
- **Weibull MLE** — Maximum likelihood estimation of wind speed distribution
- **Kalman Filter** — SOC estimation with OCV measurement update
- **HDKR Model** — Anisotropic sky model for irradiance on tilted surfaces
- **Single-Diode PV Model** — Newton-Raphson solution of I-V characteristic
- **DP Battery Dispatch** — Bellman-equation optimal charge/discharge scheduling
- **PSO** — Particle Swarm Optimization for microgrid sizing
- **Revised Simplex** — Linear programming for energy dispatch

## Nine-School Course Mapping

| School | Course | Coverage |
|--------|--------|----------|
| **MIT** | 6.004 Computation Structures | Energy balance equations, power flow |
| **Stanford** | CS 229 Machine Learning | Holt-Winters, Kalman filter, PSO |
| **Berkeley** | CS 267 HPC | Monte Carlo reliability simulation |
| **CMU** | 15-418 Parallel | DP parallelization (battery dispatch) |
| **UT Austin** | ECE 382V VLSI | Semiconductor physics (SQ limit) |
| **ETH** | 263-3501 Parallel Programming | PSO, GA for energy optimization |
| **Cambridge** | Part II: Engineering | Thermodynamics (Carnot, Rankine) |
| **清华** | 能源互联网 | Microgrid modeling, carbon accounting |
| **Georgia Tech** | CS 7641 Machine Learning | Time series forecasting, ensemble methods |

## Build & Test

```bash
make          # Build static library libenergy.a
make test     # Run 66-unit test suite (all passing)
```

## File Structure

```
mini-energy-carbon/
├── Makefile              # make test 一键通过
├── README.md             # This file (COMPLETE)
├── include/
│   ├── energy_core.h     # Core types + 120+ API declarations (443 lines)
│   ├── energy_carbon.h   # GHG Protocol, LCA, carbon markets (172 lines)
│   ├── energy_solar.h    # Solar geometry, PV models (170 lines)
│   ├── energy_wind.h     # Weibull, wind farm models (146 lines)
│   ├── energy_market.h   # PPA, VPP, arbitrage, DR (180 lines)
│   ├── energy_optimize.h # LP, DP, PSO, Pareto (196 lines)
│   └── energy_forecast.h # Holt-Winters, ARIMA, STL, metrics (156 lines)
├── src/
│   ├── energy_core.c     # Physics, battery, finance, building, H2 (573 lines)
│   ├── energy_carbon.c   # Carbon accounting, LCA, SBTi (196 lines)
│   ├── energy_solar.c    # Solar geometry, PV, thermal (233 lines)
│   ├── energy_wind.c     # Weibull, wake, AEP (253 lines)
│   ├── energy_market.c   # Arbitrage, PPA, VPP, REC, TOU (241 lines)
│   ├── energy_optimize.c # LP, DP, PSO, Monte Carlo, Pareto (325 lines)
│   └── energy_forecast.c # HW, ARIMA, STL, metrics, ensemble (285 lines)
├── tests/
│   └── test_energy.c     # 66-unit tests (424 lines)
├── examples/
│   └── example_microgrid.c # Microgrid sizing example
└── docs/                 # Documentation directory
```

MIT
