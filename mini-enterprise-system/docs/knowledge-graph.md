# mini-enterprise-system — Knowledge Graph (L1-L9)

## L1: Core Definitions (Complete)
| Type | Definition |
|------|-----------|
| MENT_ERPInstance | ERP system instance (company, accounts, journals, ledger) |
| MENT_Account | Chart of Accounts entry (code, name, type, hierarchy) |
| MENT_AccountType | ASSET, LIABILITY, EQUITY, REVENUE, EXPENSE, OTHER_INCOME, OTHER_EXPENSE |
| MENT_JournalEntry | Accounting voucher (voucher_no, lines, posting status) |
| MENT_JournalLine | Single journal entry line (account, amount, debit/credit) |
| MENT_LedgerBalance | Account-period balance (opening, debit, credit, closing) |
| MENT_FiscalPeriod | Accounting period (year, period, dates, closed status) |
| MENT_AuditEntry | SOX audit trail entry (timestamp, user, action, detail) |
| MENT_Item | MRP item master (code, type, lead time, safety stock) |
| MENT_BOMLine | Bill of Materials (parent, component, quantity per) |
| MENT_WorkOrder | Manufacturing work order (status, quantity, dates) |
| MENT_MRPPegging | MRP pegging record (gross req, net req, planned orders) |
| MENT_Employee | HR employee record (name, status, salary, leave) |
| MENT_Payroll | Payroll record (basic, overtime, deductions, net pay) |
| MENT_Lead | CRM sales lead (company, contact, source, score) |
| MENT_Opportunity | Sales opportunity (amount, stage, probability) |
| MENT_Budget | Financial budget (type, year, approval status) |
| MENT_FinancialRatio | Computed financial ratio (name, value, assessment) |
| MENT_KPI | Management KPI (current, target, change %) |

## L2: Core Concepts (Complete)
- Double-entry bookkeeping (Pacioli 1494)
- Chart of Accounts hierarchy
- General Ledger posting cycle
- Fiscal period management
- Dependent vs independent demand (MRP)
- Bill of Materials (Gozinto graph)
- Lot sizing (EOQ, L4L, POQ, PPB)
- Organizational hierarchy (departments, positions)
- Progressive taxation
- Sales funnel stages
- Lead scoring methodology
- RFM (Recency, Frequency, Monetary) analysis
- Budget vs Actual variance
- Financial ratios (liquidity, profitability, solvency, efficiency)

## L3: Engineering Structures (Complete)
- Account tree (hierarchical COA with parent-child relationships)
- B-tree account index
- Audit trail (SOX-compliant append-only log)
- BOM tree (Gozinto graph with LLC traversal)
- Work center routing model
- MRP low-level coding algorithm
- Payroll calculation pipeline
- Org chart (department-level tree)
- Pipeline state machine (7-stage opportunity funnel)
- Budget line aggregation
- Multi-entity consolidation

## L4: Standards/Theorems (Complete)
- Accounting Equation: Assets = Liabilities + Equity
- Double-Entry Principle: Sum Debits = Sum Credits
- GAAP/IFRS accounting principles
- SOX (Sarbanes-Oxley Act 2002) compliance
- APICS body of knowledge (MRP II framework)
- Just-in-Time (Toyota Production System)
- Theory of Constraints (Goldratt 1984)
- FLSA labor standards
- ISO 30414 Human Capital Reporting
- GDPR consent management (EU 2016/679)
- IFRS reporting requirements

## L5: Algorithms/Methods (Complete)
- MRP explosion algorithm (Orlicky 1975): O(I*H*B)
- Lot-sizing: EOQ (Harris 1913), L4L, POQ, PPB
- Capacity Requirements Planning (CRP)
- Low-Level Code assignment (iterative graph algorithm)
- Lead scoring heuristic
- RFM analysis
- NPV: Sum(CashFlow_t / (1+r)^t) - I_0
- IRR: Newton-Raphson root finding
- Depreciation: Straight-Line, Declining Balance, Sum-of-Years
- Progressive tax calculation
- Period close algorithm (revenue/expense → retained earnings)
- Sales forecast (pipeline-weighted)
- Break-even analysis: BEP = FC / (P - VC)

## L6: Canonical Problems (Complete)
- General Ledger posting and Trial Balance
- Balance Sheet and Income Statement generation
- MRP net requirements calculation
- Work order generation from planned orders
- Payroll run (gross → deductions → net)
- Attendance reconciliation
- Sales pipeline management
- Budget variance analysis
- Cash flow statement (indirect method)
- Multi-entity consolidation

## L7: Applications (Complete)
- SME ERP system (full accounting cycle)
- Manufacturing MRP II planning
- Workforce HR management
- B2B CRM pipeline management
- Financial reporting and ratio analysis
- See examples/ and demos/ for end-to-end demos

## L8: Advanced Topics (Partial)
- Real-time consolidation (multi-currency IFRS IAS 21)
- IFRS 16 lease accounting
- DDMRP (Demand-Driven MRP) concepts documented
- People analytics (tenure, turnover, compensation)
- APS (Advanced Planning & Scheduling) theory

## L9: Industry Frontiers (Partial)
- SAP S/4HANA architecture reference
- Oracle NetSuite cloud ERP
- Salesforce CRM platform
- Workday HCM
- Odoo open-source ERP
- Industry 4.0 smart manufacturing
