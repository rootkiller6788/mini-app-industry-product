# Knowledge Graph — E-Commerce Retail

## L1: Core Definitions ✅ Complete

| Entry | Type | Location |
|-------|------|----------|
| OrderStatus enum | enum | ecommerce_core.h:17 |
| PaymentStatus enum | enum | ecommerce_core.h:27 |
| DiscountType enum | enum | ecommerce_core.h:35 |
| ShippingMethod enum | enum | ecommerce_core.h:43 |
| Customer struct | struct | ecommerce_core.h:50 |
| Product struct | struct | ecommerce_core.h:61 |
| OrderLine struct | struct | ecommerce_core.h:75 |
| Order struct | struct | ecommerce_core.h:81 |
| Inventory struct | struct | ecommerce_core.h:95 |
| CartItem struct | struct | ecommerce_core.h:102 |
| ShoppingCart struct | struct | ecommerce_core.h:107 |
| Coupon struct | struct | ecommerce_core.h:114 |
| CouponEngine struct | struct | ecommerce_core.h:125 |
| ShippingCalc struct | struct | ecommerce_core.h:130 |
| ProductRecommendation struct | struct | ecommerce_core.h:139 |
| RecommendationEngine struct | struct | ecommerce_core.h:146 |
| PaymentMethodType enum | enum | ecommerce_payment.h:20 |
| TransactionState enum | enum | ecommerce_payment.h:31 |
| TaxType enum | enum | ecommerce_payment.h:40 |
| Currency struct | struct | ecommerce_payment.h:50 |
| TaxRule struct | struct | ecommerce_payment.h:58 |
| PaymentTransaction struct | struct | ecommerce_payment.h:68 |
| PaymentGateway struct | struct | ecommerce_payment.h:80 |
| PaymentEngine struct | struct | ecommerce_payment.h:91 |
| SearchQuery struct | struct | ecommerce_search.h:53 |
| SearchResult struct | struct | ecommerce_search.h:62 |
| SearchResponse struct | struct | ecommerce_search.h:71 |
| InvertedIndex struct | struct | ecommerce_search.h:38 |
| TermEntry struct | struct | ecommerce_search.h:27 |
| Posting struct | struct | ecommerce_search.h:19 |
| AnalyticsEvent struct | struct | ecommerce_analytics.h:36 |
| RevenueMetrics struct | struct | ecommerce_analytics.h:44 |
| RFMScore struct | struct | ecommerce_analytics.h:64 |
| Cohort struct | struct | ecommerce_analytics.h:73 |
| AnalyticsEngine struct | struct | ecommerce_analytics.h:79 |
| DomainEvent struct | struct | ecommerce_advanced.h:42 |
| EventStore struct | struct | ecommerce_advanced.h:49 |
| FraudRule struct | struct | ecommerce_advanced.h:66 |
| FraudAssessment struct | struct | ecommerce_advanced.h:76 |
| FraudEngine struct | struct | ecommerce_advanced.h:85 |
| ABTest struct | struct | ecommerce_advanced.h:102 |
| ABTestEngine struct | struct | ecommerce_advanced.h:116 |
| RateLimiter struct | struct | ecommerce_advanced.h:128 |
| FlashSaleItem struct | struct | ecommerce_advanced.h:138 |
| FlashSaleEngine struct | struct | ecommerce_advanced.h:146 |

**Total: 46 type definitions across 5 header files**

## L2: Core Concepts ✅ Complete

| Concept | Implementation | File |
|---------|----------------|------|
| Inventory Management | inv_create/inv_reserve_stock/inv_restock | ecommerce_core.c |
| Order Lifecycle | osys_create_order/osys_confirm/osys_ship/osys_deliver | ecommerce_core.c |
| Shopping Cart | cart_create/cart_add_item/cart_remove_item | ecommerce_core.c |
| Payment Processing | payment_authorize/capture/settle/refund | ecommerce_payment.c |
| Tax Calculation | payment_tax_calculate/payment_composite_tax | ecommerce_payment.c |
| Full-text Search | index_add_document/search_execute | ecommerce_search.c |
| RFM Segmentation | analytics_compute_rfm | ecommerce_analytics.c |
| Cohort Analysis | analytics_create_cohort/analytics_cohort_retention | ecommerce_analytics.c |
| Event Sourcing | event_store_append/event_project_product | ecommerce_advanced.c |
| Fraud Detection | fraud_assess_order/fraud_bayesian_probability | ecommerce_advanced.c |
| A/B Testing | ab_test_create_experiment/ab_test_conclude | ecommerce_advanced.c |
| Rate Limiting | rate_limiter_allow | ecommerce_advanced.c |
| Flash Sales | flashsale_reserve | ecommerce_advanced.c |

## L3: Engineering Structures ✅ Complete

| Structure | Data + Operations | File(s) |
|-----------|-------------------|---------|
| Inventory System | Inventory + product CRUD + stock ops | ecommerce_core.h/c |
| Order Management | OrderSystem + lifecycle state machine | ecommerce_core.h/c |
| Payment Gateway | PaymentEngine + gateway abstraction | ecommerce_payment.h/c |
| Inverted Index | InvertedIndex + term→doc mapping | ecommerce_search.h/c |
| Analytics Pipeline | AnalyticsEngine + event→metrics | ecommerce_analytics.h/c |
| Append-only Event Store | EventStore + CQRS projections | ecommerce_advanced.h/c |
| Fraud Detection Engine | FraudEngine + rule evaluation | ecommerce_advanced.h/c |
| A/B Testing Engine | ABTestEngine + statistical tests | ecommerce_advanced.h/c |
| Rate Limiter | Token Bucket + refill logic | ecommerce_advanced.h/c |
| Flash Sale Engine | FlashSaleEngine + atomic reserve | ecommerce_advanced.h/c |

## L4: Standards/Theorems ✅ Complete

| Theorem/Standard | Formula | Implementation |
|------------------|---------|----------------|
| Little's Law | L = λW | inv_reserve_stock() (inventory = rate × holding time) |
| Law of One Price | P₁ = P₂ (no arbitrage) | price_calculate(), payment_detect_arbitrage() |
| TF-IDF | TF(t,d)×IDF(t) = (f/max_f)×log(N/df) | compute_tfidf() |
| BM25 | Σ IDF(qi)×(tf×(k₁+1))/(tf+k₁×(1-b+b×dl/avgdl)) | compute_bm25() |
| Bayes' Theorem | P(F\|E) = P(E\|F)P(F)/P(E) | fraud_bayesian_probability() |
| Pareto Principle | 20% customers → 80% revenue | analytics_rfm_assign_segments() |
| Central Limit Theorem | X̄ ~ N(μ, σ²/n) | ab_test_chi_squared() |
| Amdahl's Law | Speedup ≤ 1/((1-p)+p/n) | flashsale_reserve() (serial stock check) |
| Interest Rate Parity | F/S = (1+i_d)/(1+i_f) | payment_detect_arbitrage() |
| Levenshtein Distance | dp[i][j] = min(dp[i-1][j]+1, dp[i][j-1]+1, dp[i-1][j-1]+c) | levenshtein_distance() |

## L5: Algorithms/Methods ✅ Complete

| Algorithm | Complexity | Function |
|-----------|-----------|----------|
| RFM Quintile Scoring | O(n log n) | analytics_compute_rfm() |
| TF-IDF Ranking | O(\|Q\|·\|D\|) | search_execute() |
| BM25 (Okapi) | O(\|Q\|·\|D\|) | search_bm25_rank() |
| Porter Stemmer | O(n) per word | stem_porter() |
| Levenshtein Edit Distance | O(m·n) | levenshtein_distance() |
| Token Bucket Rate Limiting | O(1) amortized | rate_limiter_allow() |
| Simple Moving Average | O(n) | compute_sma() |
| Exponential Moving Average | O(n) | compute_ema() |
| Linear Trend (Least Squares) | O(n) | compute_trend_slope() |
| Chi-Squared Test | O(1) | ab_test_chi_squared() |
| Naive Bayes Classification | O(k·n) | fraud_bayesian_probability() |
| Z-Score Anomaly Detection | O(n) | fraud_is_anomaly() |
| Cohort Retention Analysis | O(m·o) | analytics_cohort_retention() |
| Inverted Index Build | O(d·t) | index_build_from_inventory() |

## L6: Canonical Problems ✅ Complete

| Problem | Domain | Example Program |
|---------|--------|-----------------|
| Web Store Checkout | End-to-end order flow | examples/example_checkout.c |
| Product Search Engine | Full-text search + ranking | examples/example_search.c |
| Business Intelligence Dashboard | Analytics + reporting | examples/example_analytics.c |
| Fraud Detection Pipeline | Security + event sourcing | examples/example_fraud_flow.c |
| Flash Sale Handling | High-concurrency inventory | flashsale_reserve() |

## L7: Applications ✅ Complete (3+)

| Application | Status | Implementation |
|-------------|--------|----------------|
| Coupon/Discount Engine | Complete | coupon_create/coupon_apply/coupon_is_valid |
| Shipping Calculator | Complete | shipping_calculate/shipping_order_weight |
| Recommendation Engine | Complete | reco_record_purchase/reco_get_also_bought |
| Multi-Currency Payment | Complete | payment_convert_currency/payment_detect_arbitrage |
| Tax Compliance Engine | Complete | payment_tax_calculate/payment_composite_tax |
| Analytics Dashboard | Complete | analytics_generate_dashboard |
| Faceted Product Search | Complete | search_facets_compute |
| Split Payment Processing | Complete | payment_process_split |
| Customer Lifetime Value | Complete | osys_calculate_lifetime_value |

## L8: Advanced Topics ✅ Complete (1+)

| Topic | Status | Implementation |
|-------|--------|----------------|
| Event Sourcing / CQRS | Complete | EventStore + ProductProjection |
| Fraud Detection Engine | Complete | FraudEngine + Bayesian + Rules |
| A/B Testing Framework | Complete | ABTestEngine + Chi-Squared |
| Rate Limiter | Complete | Token Bucket Algorithm |
| Z-Score Anomaly Detection | Complete | fraud_is_anomaly |

## L9: Industry Frontiers ✅ Partial (Documented)

| Topic | Status | Document |
|-------|--------|----------|
| AI-Driven Dynamic Pricing | Documented | docs/industry-frontiers.md |
| Headless Commerce (MACH) | Documented | docs/industry-frontiers.md |
| Blockchain Supply Chain | Documented | docs/industry-frontiers.md |
| Real-Time Personalization | Documented | docs/industry-frontiers.md |