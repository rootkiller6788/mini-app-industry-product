# Gap Report — mini-logistics-supplychain

## L8: Advanced Topics (Missing)

| Topic | Priority | Rationale |
|-------|----------|-----------|
| Stochastic Dynamic Programming for Inventory | Low | Large implementation effort; newsvendor model covers single-period case |
| Simulation-based supply chain optimization | Medium | Useful for teaching but requires significant infrastructure |

## L9: Industry Frontiers (Not Implemented — Documented Only)

| Topic | Status | Notes |
|-------|--------|-------|
| Blockchain for Supply Chain Traceability | Documented | Smart contract verification of provenance; would require external crypto dependency |
| AI/ML Demand Sensing | Documented | Holt-Winters serves as baseline; deep learning extension possible |
| Autonomous Delivery Drones/Robots | Documented | Mode enum includes DRONE/AUTONOMOUS; routing algorithm extensible |
| Digital Twin for SCM | Documented | Real-time simulation mirror; requires continuous data feed infrastructure |
| IoT-Enabled Real-Time Tracking | Documented | Sensor data pipeline integration; architectural concern beyond module scope |

## Known Limitations

1. **VRP solver**: Clarke-Wright is a heuristic; optimal MILP formulation not implemented due to solver dependency requirements.
2. **Multi-echelon optimization**: Guaranteed Service Time model implemented; full stochastic multi-echelon (Graves-Willems) requires integer programming.
3. **Cold chain**: Mode and facility types support cold chain conceptually; temperature monitoring not implemented in code.
4. **Cross-docking**: Facility type enum includes cross-dock; detailed scheduling algorithm not implemented.
