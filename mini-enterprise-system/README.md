# mini-enterprise-system — Enterprise Resource Systems (C Implementation)

> **Module Status: COMPLETE ✅**
> include/ + src/ total: 3,524 lines (≥ 3,000 ✓)
> All 57 tests passing · 4 end-to-end examples · 1 full-stack demo

Enterprise systems implementation covering ERP (General Ledger, Double-Entry Bookkeeping), MRP II (Material Requirements Planning), HRM (Human Resource Management), CRM (Customer Relationship Management), and Financial Reporting.

## Modules

| Module | Header | Lines | Source | Lines | Description |
|--------|--------|-------|--------|-------|-------------|
| ERP Core | `erp_core.h` | 323 | `erp_core.c` | 811 | General Ledger, Chart of Accounts, Journal Entry, Double-Entry Posting, Multi-Currency, Period Close, Consolidation, Audit Trail |
| MRP Engine | `mrp_engine.h` | 284 | `mrp_engine.c` | 449 | Item Master, BOM, Low-Level Coding, MRP Explosion, Lot Sizing (EOQ/L4L/POQ/PPB), Capacity Planning, Work Orders |
| HRM System | `hrm_system.h` | 199 | `hrm_system.c` | 251 | Employee/Department/Position Management, Attendance, Leave, Payroll with Progressive Tax, Workforce Analytics |
| CRM Pipeline | `crm_pipeline.h` | 210 | `crm_pipeline.c` | 285 | Lead Management, Opportunity Pipeline, Account/Contact, Lead Scoring, RFM Analysis, Sales Forecasting |
| Finance | `fin_report.h` | 228 | `fin_report.c` | 484 | Budget vs Actual, Financial Ratios (8 types), Cash Flow, KPI Dashboard, NPV/IRR, Depreciation Methods |

## L1-L9 Knowledge Coverage

| Level | Coverage | Key Implementations |
|-------|----------|---------------------|
| **L1** Definitions | ✅ Complete | 20+ struct definitions: MENT_ERPInstance, MENT_Account, MENT_JournalEntry, MENT_Item, MENT_BOMLine, MENT_WorkOrder, MENT_Employee, MENT_Payroll, MENT_Lead, MENT_Opportunity, MENT_Budget, MENT_FinancialRatio, MENT_KPI |
| **L2** Core Concepts | ✅ Complete | Double-entry bookkeeping (Pacioli 1494), COA hierarchy, Fiscal period management, Dependent vs independent demand, BOM structure, Lot sizing, Org hierarchy, Sales funnel stages, Lead scoring, RFM analysis, Budget vs Actual variance, Financial ratios |
| **L3** Engineering Structures | ✅ Complete | Account tree, B-tree index, Audit trail (SOX), BOM tree (Gozinto graph), Low-level coding, Work center routing, Payroll pipeline, Pipeline state machine, KPI dashboard |
| **L4** Standards/Theorems | ✅ Complete | Accounting Equation (A = L + E), Double-Entry Principle (ΣD = ΣC), GAAP/IFRS, SOX 2002, APICS MRP II, JIT/TOC (Goldratt), FLSA, ISO 30414, GDPR (EU 2016/679) |
| **L5** Algorithms/Methods | ✅ Complete | MRP explosion (Orlicky 1975), Lot-sizing (EOQ Harris 1913, L4L, POQ, PPB), CRP, Lead scoring heuristic, RFM analysis, NPV/IRR (Newton-Raphson), Depreciation (SL/DB/SYD), Progressive tax, Period close |
| **L6** Canonical Problems | ✅ Complete | General Ledger posting, Trial Balance, Balance Sheet, Income Statement, Payroll run, Net requirements, Work order generation, Budget variance, Cash flow statement, Multi-entity consolidation |
| **L7** Applications | ✅ Complete | SME ERP system, Manufacturing MRP II, Workforce HRM, B2B CRM pipeline, Financial reporting — 4 examples + 1 full-stack demo |
| **L8** Advanced Topics | ✅ Partial | Real-time consolidation (IFRS IAS 21), IFRS 16 lease accounting, DDMRP concepts, People analytics |
| **L9** Industry Frontiers | ✅ Partial | SAP S/4HANA, Oracle NetSuite, Salesforce, Workday, Odoo — architecture references documented |

## Core Theorems & Formulas

| Theorem | Formula | Verification |
|---------|---------|-------------|
| Accounting Equation | Assets = Liabilities + Equity | `ment_erp_balance_sheet()` |
| Double-Entry | Σ Debits = Σ Credits | `ment_erp_validate_journal()` |
| EOQ | sqrt(2×D×S/H) | `ment_mrp_lot_size(EOQ, ...)` |
| NPV | Σ CF_t / (1+r)^t | `ment_fin_npv()` |
| IRR | NPV(r)=0, Newton-Raphson | `ment_fin_irr()` |
| Break-Even | BEP = FC/(P-VC) | `ment_fin_break_even()` |
| Straight-Line Depr | (Cost-Salvage)/Life | `ment_fin_depreciation_straight_line()` |

## Nine-School Curriculum Mapping

| School | Course | Topics Covered |
|--------|--------|----------------|
| MIT | 15.501 Corporate Financial Accounting | Double-entry, GL, financial statements |
| MIT | 15.762 Supply Chain Planning | MRP explosion, lot sizing |
| Stanford GSB | Financial Accounting | COA, journal entries, ratio analysis |
| Wharton | Corporate Finance | NPV, IRR, depreciation |
| Georgia Tech | ISyE 6203 Mfg Planning | BOM, LLC, CRP |
| CMU | 15-445 Database Systems | Data models for ERP |
| Berkeley | CS 294 Data Systems | ERP architecture |
| ETH | 363-1000 Production & Operations Mgmt | MRP II, lot sizing optimization |
| Cambridge | Part II: Operations Management | Supply chain planning |

## Build & Test

```sh
make           # Build static library (libenterprise.a)
make test      # Build + run 57 assertion-based tests
make examples  # Build + run 4 end-to-end examples
make clean     # Remove build artifacts
```

**Test Summary**: 57/57 passing across 5 modules
- ERP Core: 15 tests (create, fiscal period, COA, journal, double-entry, income statement, currency, reversal, transactions, NULL safety, period close, consolidation, audit)
- MRP Engine: 10 tests (instance, items, BOM, work centers, routing, demand/supply, LLC, explosion, lot sizing, CRP)
- HRM System: 10 tests (instance, departments, employees, org chart, attendance, leave, payroll, pay query, analytics, status)
- CRM Pipeline: 10 tests (instance, accounts, contacts, leads, scoring, conversion, opportunities, pipeline, RFM, forecast)
- Finance: 12 tests (instance, budget, budget lines, approval, variance, ratios, cash flow, KPIs, break-even, NPV, IRR, depreciation)

**Requirements**: GCC/Clang with C99 support. Link with `-lm`.

## Module Status: COMPLETE ✅

- **L1-L6**: Complete (all core definitions, concepts, structures, standards, algorithms, and canonical problems implemented)
- **L7**: Complete (4 examples + 1 full-stack demo covering ERP, MRP, HRM, CRM, Finance)
- **L8**: Partial (IFRS IAS 21, IFRS 16, DDMRP concepts, people analytics documented)
- **L9**: Partial (industry system references documented in docs/knowledge-graph.md)

**Metrics**:
- include/ + src/: 3,524 lines
- tests/: 216 lines (57 test cases)
- examples/: 335 lines (4 end-to-end examples)
- demos/: 143 lines (1 full-stack demo)
- docs/: knowledge-graph, coverage, gap, course alignment
- Total: 4,075+ lines

