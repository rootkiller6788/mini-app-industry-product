# Dependency Tree - mini-healthcare-medical

## Prerequisites

1. Data Structures (from data-engine module 7)
2. Mathematics (statistics, algebra)
3. C Standard Library (time.h, math.h)

## Dependents

1. AI module (14) - Clinical scores as feature vectors
2. Frontend module (9) - Dashboard visualization
3. Backend module (8) - API endpoints for patient data
4. Security module (13) - Audit trail for clinical data access

## Cross-Module Data Flow

healthcare-medical (19) -> clinical scores -> AI (14)
healthcare-medical (19) -> patient data -> backend (8)
healthcare-medical (19) -> monitoring data -> frontend (9)
healthcare-medical (19) -> audit logs -> security (13)
healthcare-medical (19) -> lab results -> data-engine (7)
