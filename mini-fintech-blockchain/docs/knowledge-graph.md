# Knowledge Graph ¡ª Fintech Blockchain Module

## L1: Core Definitions (Complete)
- AccountType enum (Asset, Liability, Equity, Revenue, Expense)
- TransactionType enum (Deposit, Withdrawal, Transfer, Payment, Refund, Fee, Interest)
- LoanType enum (Fixed, Variable, Balloon, InterestOnly)
- RiskLevel enum (Low, Medium, High, Critical)
- FraudRuleType enum (AmountAnomaly, Velocity, Geography, Device, Pattern)
- Account struct (id, name, type, balance, currency, active, frozen)
- Transaction struct (id, type, debit/credit account, amount, timestamp)
- Ledger struct (accounts[], transactions[], account/txn counts)
- Loan struct, AmortizationRow, LoanBook
- CreditProfile, CreditScore
- Block struct (index, timestamp, data, prev_hash, hash, nonce, difficulty)
- MerkleTree, MerkleLeaf, Blockchain
- FXRate, FXMarket
- FraudRule, FraudEngine
- Asset, Portfolio, Market
- OptionParams, OptionType
- VaRResult, VaRMethod
- Bond, YieldCurve
- Bank, Payment, PaymentNetwork
- LOBOrder, LimitOrderBook, OrderSide, OrderTypeLOB
- BaselIIIReport
- CBDCToken

## L2: Core Concepts (Complete)
- Double-entry bookkeeping (Pacioli 1494): Debit = Credit, A = L + E
- Time value of money: PV/FV, compound interest
- Risk-return tradeoff: Higher risk demands higher expected return
- Market efficiency: Prices reflect available information
- No-arbitrage principle: Identical assets must have identical prices
- Decentralized consensus: PoW as Sybil resistance mechanism
- Tokenomics: CBDC supply, programmability, expiry

## L3: Engineering Structures (Complete)
- Ledger System: In-memory account/transaction store with double-entry
- Payment Network: Inter-bank messaging with net settlement
- Limit Order Book: Price-time priority queue with matching engine
- AMM Pool: Constant product invariant with liquidity provisioning
- Market Data: Asset registry with covariance matrix
- Blockchain: Linked list of blocks with Merkle root commitment

## L4: Standards/Theorems (Complete)
1. Accounting Equation (Pacioli 1494): A = L + E
2. Black-Scholes-Merton (1973): Closed-form European option pricing
3. CAPM (Sharpe 1964): E(R_i) = R_f + beta_i * (E(R_m) - R_f)
4. Markowitz MPT (1952): Mean-variance portfolio optimization
5. Basel III (BCBS 2011): Capital adequacy framework
6. Nelson-Siegel (1987): Parsimonious yield curve model
7. Uniswap v2 (Adams et al. 2020): Constant product AMM: x*y=k
8. BIS CBDC Principles (2020): Digital fiat currency design

## L5: Algorithms/Methods (Complete)
1. PMT Formula: Equal installment loan payment
2. FICO Scoring: 5-factor weighted credit model
3. Merkle Tree: Bottom-up binary hash tree construction
4. PoW Mining: Nonce search for target difficulty
5. Black-Scholes Closed Form: European option pricing
6. Binomial Tree (CRR 1979): American option pricing
7. Monte Carlo Simulation: VaR estimation via random sampling
8. Historical VaR: Alpha-quantile of return distribution
9. Parametric VaR: Normal distribution assumption
10. Newton-Raphson: YTM and Implied Volatility root finding
11. Box-Muller Transform: Normal random variate generation
12. QuickSort: In-place O(n log n) sorting
13. VWAP: Volume-weighted average price
14. SMA/R Crossover: Moving average trading signal
15. RSI: Relative strength index oscillator
16. Nelson-Siegel Interpolation: Yield curve fitting

## L6: Canonical Problems (Complete)
1. Payment Clearing: Net settlement between banks
2. Order Book Matching: Price-time priority crossing
3. Portfolio Optimization: Maximum Sharpe ratio allocation
4. Bond Immunization: Duration-matched hedge ratio
5. Blockchain Validation: Chain integrity verification
6. Forex Triangulation: Cross-rate calculation via USD

## L7: Applications (Partial+)
1. Foreign Exchange Market: Multi-currency rates and conversion
2. Fraud Detection: Rule-based transaction monitoring
3. Basel III Capital: Regulatory capital calculation
4. Trading Signals: SMA crossover and RSI indicators
5. Option Strategies: Straddle, Bull Spread payoffs
6. Loan Amortization: Mortgage/loan payment schedules
7. Credit Scoring: Consumer credit assessment

## L8: Advanced Topics (Partial+)
1. Blockchain with PoW: Decentralized consensus mechanism
2. AMM Constant Product: DeFi liquidity pool mathematics
3. Impermanent Loss: LP risk quantification
4. Asian Options: Geometric average closed-form (Kemna & Vorst 1990)
5. Lookback Options: Floating strike (Goldman, Sosin & Gatto 1979)
6. Option Greeks: Delta, Gamma, Theta, Vega, Rho computation
7. Implied Volatility: Newton-Raphson numerical solver

## L9: Industry Frontiers (Partial)
1. CBDC: Central Bank Digital Currency with programmability
2. DeFi AMM: Automated Market Maker liquidity provisioning
3. Basel III: Global regulatory capital framework
