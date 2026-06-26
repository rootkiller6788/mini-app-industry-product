#include "energy_core.h"
#include "energy_market.h"
#include <string.h>
#include <math.h>

/* -- Demand Response (L7: Applications) ----------------------------- */
float energy_dr_available_capacity(const energy_demand_response_t* dr) {
    if (!dr) return 0.0f;
    return dr->dispatchable_capacity_kw;
}
float energy_dr_event_cost(const energy_demand_response_t* dr, float reduced_kw, float duration_h) {
    if (!dr) return 0.0f;
    float incentive = reduced_kw * duration_h * dr->incentive_per_kwh;
    if (reduced_kw < 0) { /* under-delivery penalty */
        float shortfall = -reduced_kw;
        incentive -= shortfall * duration_h * dr->penalty_per_kwh;
    }
    return incentive;
}
float energy_dr_customer_baseline(const float* load_kw_10day, int n_days, int event_hour) {
    if (!load_kw_10day || n_days < 3) return 0.0f;
    /* Average of 3 highest of last 5 similar days (ISO standard CBL) */
    float vals[5] = {0}; int cnt = 0;
    for (int d = n_days - 5; d < n_days && d >= 0; d++) {
        vals[cnt++] = load_kw_10day[d*24 + event_hour];
    }
    /* Sort descending by simple insertion */
    for (int i = 1; i < cnt; i++) {
        float k = vals[i]; int j = i-1;
        while (j>=0 && vals[j]<k) { vals[j+1]=vals[j]; j--; }
        vals[j+1]=k;
    }
    int use = cnt >= 3 ? 3 : cnt;
    float sum=0; for (int i=0; i<use; i++) sum+=vals[i];
    return use>0 ? sum/use : 0;
}
float energy_dr_settlement(const energy_demand_response_t* dr, float actual_reduction_kw, float committed_kw) {
    if (!dr) return 0.0f;
    float ratio = committed_kw > 0 ? actual_reduction_kw/committed_kw : 0;
    if (ratio >= 1.0f) return 0; /* full compliance */
    float penalty = (committed_kw - actual_reduction_kw) * dr->penalty_per_kwh;
    return penalty;
}

/* -- Energy Arbitrage (L5: DP/Greedy) ------------------------------- */
energy_arbitrage_result_t energy_arbitrage_optimize(const energy_arbitrage_t* arb) {
    energy_arbitrage_result_t res; memset(&res, 0, sizeof(res));
    if (!arb || arb->num_slots <= 0) return res;
    res.num_slots = arb->num_slots;
    /* Greedy: charge at low prices, discharge at high prices */
    /* First, find threshold spread */
    float avg_buy=0, avg_sell=0;
    for (int i=0; i<arb->num_slots; i++) { avg_buy+=arb->buy_prices[i]; avg_sell+=arb->sell_prices[i]; }
    avg_buy/=arb->num_slots; avg_sell/=arb->num_slots;
    float threshold = (avg_sell - avg_buy) * 0.3f; /* profit threshold */
    if (threshold < 0.01f) threshold = 0.01f;
    float soc = arb->initial_soc_kwh;
    int cycles = 0;
    for (int i=0; i<arb->num_slots && i<ENERGY_MAX_TIMESERIES; i++) {
        float spread = arb->sell_prices[i] - arb->buy_prices[i];
        float charge_kw=0, discharge_kw=0;
        if (spread > threshold && soc > 0) {
            /* Discharge */
            discharge_kw = arb->max_discharge_kw;
            if (discharge_kw > soc) discharge_kw = soc;
            discharge_kw *= arb->discharge_efficiency;
            soc -= discharge_kw;
            if (soc < 0) soc = 0;
        } else if (spread < -0.01f) {
            /* Charge */
            float space = arb->storage_capacity_kwh - soc;
            charge_kw = arb->max_charge_kw;
            if (charge_kw > space) charge_kw = space;
            charge_kw /= (arb->charge_efficiency > 0 ? arb->charge_efficiency : 0.9f);
            soc += charge_kw * arb->charge_efficiency;
            if (soc > arb->storage_capacity_kwh) soc = arb->storage_capacity_kwh;
        }
        res.charge_schedule[i] = charge_kw;
        res.discharge_schedule[i] = discharge_kw;
        res.soc_schedule[i] = soc;
        float profit = discharge_kw*arb->sell_prices[i] - charge_kw*arb->buy_prices[i];
        res.total_profit += profit;
        if (discharge_kw > 0) cycles++;
    }
    res.cycles_used = cycles;
    return res;
}
float energy_arbitrage_max_charge_slot(const energy_arbitrage_t* arb, int slot) {
    if (!arb || slot < 0 || slot >= arb->num_slots) return 0.0f;
    return arb->max_charge_kw;
}
float energy_arbitrage_spread_per_kwh(const energy_arbitrage_t* arb, int slot) {
    if (!arb || slot < 0 || slot >= arb->num_slots) return 0.0f;
    return arb->sell_prices[slot] - arb->buy_prices[slot];
}
float energy_arbitrage_optimal_threshold(const float* prices, int n, float efficiency) {
    if (!prices || n < 2) return 0.0f;
    float mean=0; for(int i=0;i<n;i++) mean+=prices[i]; mean/=n;
    return mean * (1.0f/efficiency - 1.0f);
}

/* -- PPA Valuation (L7: Applications) ------------------------------- */
energy_ppa_valuation_t energy_ppa_value(const energy_generator_t* gen, float ppa_price, float dr, float tax_rate, float debt_frac) {
    energy_ppa_valuation_t val; memset(&val, 0, sizeof(val));
    if (!gen || gen->capacity_kw <= 0) return val;
    float annual_gen = gen->capacity_kw * gen->capacity_factor * 8760.0f;
    float annual_rev = annual_gen * ppa_price;
    float annual_opex = annual_gen * gen->opex_per_kwh + gen->fixed_opex_per_kw_yr * gen->capacity_kw;
    float ebitda = annual_rev - annual_opex;
    /* Simple DCF */
    float cashflows[50]; int life = (int)gen->lifetime_years; if (life > 50) life = 50;
    cashflows[0] = -gen->capex_per_kw * gen->capacity_kw * (1.0f - debt_frac);
    for (int y=1; y<=life; y++) {
        float deg = powf(1.0f-gen->degradation_rate_per_year, (float)(y-1));
        float cf = ebitda * deg * (1.0f - tax_rate);
        cashflows[y] = cf;
    }
    val.npv_per_kw = energy_npv(cashflows, life+1, dr) / gen->capacity_kw;
    val.irr_pct = energy_irr(cashflows, life+1) * 100.0f;
    val.payback_years = energy_payback_years(gen->capex_per_kw*gen->capacity_kw, annual_rev, annual_opex);
    val.lcoe = energy_lcoe(gen, dr);
    val.annual_revenue_per_kw = annual_rev / gen->capacity_kw;
    return val;
}
float energy_ppa_shape_risk_cost(const energy_timeslot_t* gen_profile, const float* price_profile, int n) {
    if (!gen_profile || !price_profile || n <= 0) return 0.0f;
    float weighted_sum=0, total_gen=0;
    for (int i=0; i<n; i++) { weighted_sum+=gen_profile[i].generation_kw*price_profile[i]; total_gen+=gen_profile[i].generation_kw; }
    float capture_price = total_gen>0 ? weighted_sum/total_gen : 0;
    float avg_price=0; for(int i=0;i<n;i++) avg_price+=price_profile[i]; avg_price/=n;
    return avg_price>0 ? (1.0f - capture_price/avg_price) : 0;
}
float energy_ppa_cannibalization(const energy_timeslot_t* profile, int n) {
    /* Solar cannibalization: price depression when solar is abundant */
    if (!profile || n<=0) return 0.0f;
    float solar_hours=0, total_hours=0;
    for (int i=0;i<n;i++) {
        if (profile[i].solar_kw > 0.5f*profile[i].generation_kw) solar_hours++;
        total_hours++;
    }
    return total_hours>0 ? solar_hours/total_hours : 0;
}

/* -- Virtual Power Plant (L8: Advanced) ----------------------------- */
void energy_vpp_init(energy_vpp_t* vpp, const char* name) {
    if (!vpp) return;
    memset(vpp, 0, sizeof(energy_vpp_t));
    if (name) { strncpy(vpp->vpp_name, name, ENERGY_MAX_NAME-1); vpp->vpp_name[ENERGY_MAX_NAME-1]='\0'; }
}
void energy_vpp_add_solar(energy_vpp_t* vpp, float capacity_kw) {
    if (!vpp) return;
    vpp->solar_capacity_kw += capacity_kw;
    vpp->aggregated_capacity_kw += capacity_kw;
    vpp->num_assets++;
}
void energy_vpp_add_wind(energy_vpp_t* vpp, float capacity_kw) {
    if (!vpp) return;
    vpp->wind_capacity_kw += capacity_kw;
    vpp->aggregated_capacity_kw += capacity_kw;
    vpp->num_assets++;
}
void energy_vpp_add_battery(energy_vpp_t* vpp, float capacity_kwh) {
    if (!vpp) return;
    vpp->battery_capacity_kwh += capacity_kwh;
    vpp->aggregated_storage_kwh += capacity_kwh;
    vpp->num_assets++;
}
void energy_vpp_add_flexible_load(energy_vpp_t* vpp, float capacity_kw) {
    if (!vpp) return;
    vpp->flexible_load_kw += capacity_kw;
    vpp->num_assets++;
}
float energy_vpp_dispatchable_capacity(const energy_vpp_t* vpp) {
    if (!vpp) return 0.0f;
    return vpp->battery_capacity_kwh * 0.5f + vpp->flexible_load_kw * 0.3f;
}
float energy_vpp_diversity_factor(const energy_vpp_t* vpp) {
    if (!vpp || vpp->aggregated_capacity_kw <= 0) return 0.0f;
    float diverse = vpp->solar_capacity_kw*0.15f + vpp->wind_capacity_kw*0.35f + vpp->battery_capacity_kwh*0.1f + vpp->flexible_load_kw*0.8f;
    return diverse / vpp->aggregated_capacity_kw;
}
/* -- REC Trading (L7: Applications) --------------------------------- */
void energy_rec_init(energy_rec_t* rec, energy_source_t src, float mwh) {
    if (!rec) return;
    memset(rec, 0, sizeof(energy_rec_t));
    rec->source = src; rec->mwh_generated = mwh;
}
float energy_rec_value(const energy_rec_t* rec, float market_price_per_mwh) {
    if (!rec) return 0.0f;
    return rec->mwh_generated * market_price_per_mwh;
}
bool energy_rec_retire(energy_rec_t* rec) {
    if (!rec || rec->is_retired) return false;
    rec->is_retired = true;
    return true;
}
float energy_rec_bundle_premium(float ppa_price, float rec_price) {
    return ppa_price + rec_price;
}

/* -- Feed-in Tariff ------------------------------------------------- */
float energy_fit_revenue(float gen_kwh, const energy_feed_in_tariff_t* fit, int contract_year) {
    if (!fit || gen_kwh <= 0) return 0.0f;
    float deg = powf(1.0f - fit->annual_degression_pct*0.01f, (float)contract_year);
    return gen_kwh * fit->tariff_per_kwh * deg;
}
float energy_fit_effective_rate(const energy_feed_in_tariff_t* fit, int contract_year) {
    if (!fit) return 0.0f;
    return fit->tariff_per_kwh * powf(1.0f - fit->annual_degression_pct*0.01f, (float)contract_year);
}
/* -- Time-of-Use Optimization --------------------------------------- */
float energy_tou_optimal_schedule(const float* prices_24h, const float* load_24h, float bat_cap, float max_pwr, float eff, float* schedule_24h) {
    if (!prices_24h || !load_24h) return 0.0f;
    /* Simple: charge at 3 cheapest hours, discharge at 3 most expensive */
    float sorted_prices[24]; int sorted_idx[24], sorted_load[24]; int load_idx[24];
    for (int i=0;i<24;i++) { sorted_prices[i]=prices_24h[i]; sorted_idx[i]=i; sorted_load[i]=-load_24h[i]; load_idx[i]=i; }
    /* Sort prices ascending (cheapest first) */
    for (int i=1;i<24;i++){ float k=sorted_prices[i]; int ki=sorted_idx[i]; int j=i-1;
        while(j>=0&&sorted_prices[j]>k){sorted_prices[j+1]=sorted_prices[j]; sorted_idx[j+1]=sorted_idx[j]; j--;}
        sorted_prices[j+1]=k; sorted_idx[j+1]=ki; }
    float soc=0, cost=0;
    if (schedule_24h) for(int i=0;i<24;i++) schedule_24h[i]=0;
    /* Charge at cheapest hours, discharge at expensive hours */
    int charge_count=0;
    for (int i=0; i<3 && i<24; i++) {
        int h = sorted_idx[i];
        float charge = max_pwr; if (soc+charge/eff > bat_cap) charge = (bat_cap-soc)*eff;
        soc += charge/eff; cost += charge*prices_24h[h];
        if (schedule_24h) schedule_24h[h] = -charge;
        charge_count++;
    }
    /* Discharge at expensive hours (end of array) */
    for (int i=23; i>=21 && i>=0; i--) {
        int h = sorted_idx[i];
        if (soc <= 0) break;
        float discharge = max_pwr; if (discharge > soc) discharge = soc;
        soc -= discharge; cost -= discharge*prices_24h[h];
        if (schedule_24h) schedule_24h[h] = discharge;
    }
    return cost;
}
