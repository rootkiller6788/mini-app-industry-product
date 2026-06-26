# mini-fintech-blockchain

## Module Status: COMPLETE

**Total Lines (include/ + src/): 4,962**

## Nine-Layer Knowledge Coverage

| Level | Name | Status | Key Implementations |
|-------|------|--------|---------------------|
| L1 | Definitions | Complete | Account, Transaction, Ledger, Loan, CreditProfile, Block, MerkleTree, FXRate, FraudRule, Asset, Portfolio, OptionParams, Bond, YieldCurve, Bank, Payment, LOBOrder, BaselIIIReport, CBDCToken |
| L2 | Core Concepts | Complete | Double-entry bookkeeping (Pacioli 1494), Time value of money, Risk-return tradeoff, No-arbitrage pricing, Decentralized consensus |
| L3 | Engineering Structures | Complete | Ledger system, PaymentNetwork, LimitOrderBook, Market data model, AMM pool |
| L4 | Standards/Theorems | Complete | Accounting equation (A=L+E), Black-Scholes-Merton (1973), CAPM (Sharpe 1964), Markowitz MPT (1952), Basel III (BCBS 2011), Nelson-Siegel yield curve (1987), BIS CBDC principles (2020) |
| L5 | Algorithms/Methods | Complete | Compound interest, Loan amortization (PMT formula), FICO credit scoring, Merkle tree, PoW mining, Black-Scholes closed form, Binomial tree (CRR 1979), Monte Carlo simulation, VaR (Historical/Parametric/Monte Carlo), Newton-Raphson (YTM, IV), VWAP, Box-Muller transform, QuickSort |
| L6 | Canonical Problems | Complete | Payment clearing network (SWIFT/ACH), Limit Order Book matching, Bond portfolio immunization, Portfolio optimization |
| L7 | Applications | Partial+ | FX cross-rate triangulation, Fraud detection engine, Basel III regulatory capital, SMA/RSI trading signals, Option strategies (straddle, bull spread) |
| L8 | Advanced Topics | Partial+ | Blockchain PoW + Merkle tree, AMM constant product (x*y=k), Impermanent loss, Asian/Lookback options, Greeks computation |
| L9 | Industry Frontiers | Partial | CBDC (Central Bank Digital Currency) with programmable money, DeFi liquidity pools |

## Core Definitions (L1)

- **Account Types**: Asset, Liability, Equity, Revenue, Expense
- **Transaction Types**: Deposit, Withdrawal, Transfer, Payment, Refund, Fee, Interest
- **Loan Types**: Fixed, Variable, Balloon, Interest-Only
- **Risk Levels**: Low, Medium, High, Critical
- **Option Types**: Call, Put (European, American, Bermudan)
- **Order Types**: Buy/Sell, Limit/Market/Stop

## Core Theorems (L4)

1. **Accounting Equation**: Assets = Liabilities + Equity
2. **Black-Scholes Formula**: C = S，N(d1) - K，e^(-rT)，N(d2)
3. **CAPM**: E(R_i) = R_f + beta_i ， (E(R_m) - R_f)
4. **Markowitz Portfolio Variance**: sigma_p^2 = sum_i sum_j w_i w_j sigma_ij
5. **Loan PMT Formula**: PMT = P ， r ， (1+r)^n / ((1+r)^n - 1)
6. **AMM Constant Product**: x ， y = k
7. **Impermanent Loss**: IL = 2，sqrt(r)/(1+r) - 1

## Core Algorithms (L5)

1. **Loan Amortization Schedule** ！ Equal installment payment with interest/principal split
2. **FICO Credit Scoring** ！ 5-factor model (payment history, utilization, age, mix, inquiries)
3. **Merkle Tree** ！ Bottom-up hash construction with O(log n) verification
4. **PoW Mining** ！ Nonce search with configurable difficulty
5. **Black-Scholes Pricing** ！ Closed-form European option pricing with dividend yield
6. **Binomial Tree** ！ CRR (1979) backward induction for American options
7. **VaR Computation** ！ Historical, parametric (normal), and Monte Carlo methods
8. **YTM Newton Solver** ！ Yield to maturity via Newton-Raphson iteration
9. **Nelson-Siegel Yield Curve** ！ 4-parameter model for term structure
10. **Order Book Matching** ！ Price-time priority crossing engine
11. **AMM Swap** ！ Constant product market maker with fee

## Classic Problems (L6)

1. **Payment Network** ！ Inter-bank clearing with net settlement
2. **Limit Order Book** ！ Full LOB with bid/ask quotes, matching, VWAP
3. **Portfolio Optimization** ！ Monte Carlo search for maximum Sharpe ratio
4. **Bond Immunization** ！ Duration-matching hedge ratio
5. **Blockchain Validation** ！ Chain integrity via prev_hash linkage

## Nine-School Course Mapping

| School | Course | Module Coverage |
|--------|--------|-----------------|
| MIT | 15.433 Investments | Portfolio Theory, CAPM, Efficient Frontier |
| MIT | 15.450 Analytics of Finance | Black-Scholes, Options Pricing |
| Stanford | MS&E 245G Finance | Fixed Income, Yield Curves |
| Berkeley | UGBA 103 Finance | Credit Scoring, Risk Management |
| CMU | 15-445 Database Systems | Ledger data model |
| ETH | 363-0537 Digital Currency | Blockchain, CBDC |
| Cambridge | Paper 3 Finance | Bond pricing, Duration |
| Georgia Tech | CS 7641 ML | Fraud detection rules |
| Tsinghua | Fintech course | FX, Payment networks |

## Build & Test

mkdir -p bin
gcc -std=c99 -Wall -Wextra -O2 -I include tests/test_fintech.c src/fintech_core.c -o bin/test_fintech -lm
bin/test_fintech
=== Fintech Comprehensive Test Suite ===

[L3] Double-Entry Ledger...
[L5] Loan Amortization...
[L5] Credit Scoring (FICO-inspired)...
[L8] Blockchain + Merkle Tree...
[L7] Foreign Exchange...
[L7] Fraud Detection...
[L5] Portfolio Theory...
[L5] Black-Scholes...
[L5] Value at Risk...
[L5] Bond Pricing...
[L6] Payment Network...
[L8] Limit Order Book...
[L7] Basel III Capital...
[L9] Central Bank Digital Currency...

=== Results: 82 passed, 0 failed ===
rm -rf bin

No compiler warnings. All core APIs tested with assert-based validation.
