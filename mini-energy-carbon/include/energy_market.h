#ifndef ENERGY_MARKET_H
#define ENERGY_MARKET_H

#include "energy_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Energy Market Structures (L7: Applications) ──────────────────────── */

/* Locational Marginal Pricing node */
typedef struct {
    char   node_name[ENERGY_MAX_NAME];
    int    node_id;
    float  lmp_energy;         /* marginal energy cost $/MWh */
    float  lmp_congestion;     /* congestion component */
    float  lmp_losses;         /* marginal loss component */
    float  voltage_kv;
    bool   is_congested;
} energy_lmp_node_t;

/* Demand Response program types */
typedef enum {
    ENERGY_DR_DIRECT_LOAD = 0,
    ENERGY_DR_INTERRUPTIBLE,
    ENERGY_DR_CAPACITY_BID,
    ENERGY_DR_FREQUENCY_REG,
    ENERGY_DR_PRICE_RESPONSIVE,
} energy_dr_program_t;

typedef struct {
    energy_dr_program_t program_type;
    float  enrolled_capacity_kw;
    float  dispatchable_capacity_kw;
    float  max_reduction_pct;
    float  notification_time_min;
    float  response_time_min;
    float  max_event_duration_h;
    float  max_events_per_year;
    float  incentive_per_kwh;
    float  penalty_per_kwh;
    float  baseline_load_kw;     /* customer baseline load */
} energy_demand_response_t;

/* Virtual Power Plant (VPP) aggregation */
typedef struct {
    char   vpp_name[ENERGY_MAX_NAME];
    float  aggregated_capacity_kw;
    float  aggregated_storage_kwh;
    float  solar_capacity_kw;
    float  wind_capacity_kw;
    float  battery_capacity_kwh;
    float  flexible_load_kw;
    int    num_assets;
    float  forecast_accuracy_pct;
    float  availability_pct;
} energy_vpp_t;

/* Energy Arbitrage (L5: Algorithm) */
typedef struct {
    float  buy_prices[ENERGY_MAX_TIMESERIES];
    float  sell_prices[ENERGY_MAX_TIMESERIES];
    int    num_slots;
    float  storage_capacity_kwh;
    float  charge_efficiency;
    float  discharge_efficiency;
    float  max_charge_kw;
    float  max_discharge_kw;
    float  initial_soc_kwh;
    float  final_soc_kwh;
    int    max_cycles_per_day;
} energy_arbitrage_t;

typedef struct {
    float  total_profit;
    float  charge_schedule[ENERGY_MAX_TIMESERIES];
    float  discharge_schedule[ENERGY_MAX_TIMESERIES];
    float  soc_schedule[ENERGY_MAX_TIMESERIES];
    int    num_slots;
    int    cycles_used;
} energy_arbitrage_result_t;

/* PPA valuation */
typedef struct {
    float  npv_per_kw;
    float  irr_pct;
    float  payback_years;
    float  lcoe;
    float  annual_revenue_per_kw;
    float  debt_service_coverage_ratio;
    float  ppa_price_required;   /* minimum PPA price for target IRR */
} energy_ppa_valuation_t;

/* Energy certificate (REC) tracking */
typedef struct {
    char   certificate_id[64];
    energy_source_t source;
    float  mwh_generated;
    int    generation_year;
    int    generation_month;
    char   facility_name[ENERGY_MAX_NAME];
    char   country[64];
    float  price_per_mwh;
    bool   is_retired;
    bool   is_bundled;           /* bundled with electricity? */
} energy_rec_t;

/* Feed-in tariff scheme */
typedef struct {
    float  tariff_per_kwh;
    float  contract_duration_years;
    float  annual_degression_pct;
    float  capacity_limit_kw;
    bool   is_net_metering;
    bool   is_gross_metering;
} energy_feed_in_tariff_t;

/* ── API: Demand Response (L7: Applications) ──────────────────────────── */

float energy_dr_available_capacity(const energy_demand_response_t* dr);
float energy_dr_event_cost(const energy_demand_response_t* dr,
                            float reduced_kw, float duration_h);
float energy_dr_customer_baseline(const float* load_kw_10day, int n_days,
                                   int event_hour);
float energy_dr_settlement(const energy_demand_response_t* dr,
                            float actual_reduction_kw, float committed_kw);

/* ── API: Energy Arbitrage (L5: DP) ───────────────────────────────────── */

energy_arbitrage_result_t energy_arbitrage_optimize(const energy_arbitrage_t* arb);
float energy_arbitrage_max_charge_slot(const energy_arbitrage_t* arb, int slot);
float energy_arbitrage_spread_per_kwh(const energy_arbitrage_t* arb, int slot);
float energy_arbitrage_optimal_threshold(const float* prices, int n,
                                          float efficiency);

/* ── API: PPA Valuation (L7: Applications) ────────────────────────────── */

energy_ppa_valuation_t energy_ppa_value(const energy_generator_t* gen,
                                         float ppa_price,
                                         float discount_rate,
                                         float tax_rate,
                                         float debt_fraction);
float energy_ppa_shape_risk_cost(const energy_timeslot_t* generation_profile,
                                  const float* price_profile, int n);
float energy_ppa_cannibalization(const energy_timeslot_t* profile, int n);

/* ── API: Virtual Power Plant (L8: Advanced) ─────────────────────────── */

void  energy_vpp_init(energy_vpp_t* vpp, const char* name);
void  energy_vpp_add_solar(energy_vpp_t* vpp, float capacity_kw);
void  energy_vpp_add_wind(energy_vpp_t* vpp, float capacity_kw);
void  energy_vpp_add_battery(energy_vpp_t* vpp, float capacity_kwh);
void  energy_vpp_add_flexible_load(energy_vpp_t* vpp, float capacity_kw);
float energy_vpp_dispatchable_capacity(const energy_vpp_t* vpp);
float energy_vpp_diversity_factor(const energy_vpp_t* vpp);

/* ── API: REC Trading (L7: Applications) ─────────────────────────────── */

void  energy_rec_init(energy_rec_t* rec, energy_source_t src, float mwh);
float energy_rec_value(const energy_rec_t* rec, float market_price_per_mwh);
bool  energy_rec_retire(energy_rec_t* rec);
float energy_rec_bundle_premium(float ppa_price, float rec_price);

/* ── Feed-in Tariff (L7: Applications) ────────────────────────────────── */
float energy_fit_revenue(float generation_kwh, const energy_feed_in_tariff_t* fit,
                          int contract_year);
float energy_fit_effective_rate(const energy_feed_in_tariff_t* fit,
                                 int contract_year);

/* ── Time-of-Use Optimization (L7: Applications) ─────────────────────── */
float energy_tou_optimal_schedule(const float* prices_24h, const float* load_24h,
                                   float battery_capacity, float max_power,
                                   float efficiency, float* schedule_24h);

#ifdef __cplusplus
}
#endif

#endif
