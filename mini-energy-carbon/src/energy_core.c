#include "energy_core.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* --- System Init --------------------------------------------------- */
void energy_system_init(energy_system_t* sys) {
    if (!sys) return;
    memset(sys, 0, sizeof(energy_system_t));
}
void energy_system_add_generator(energy_system_t* sys, const energy_generator_t* gen) {
    if (!sys || !gen || sys->num_generators >= ENERGY_MAX_DEVICES) return;
    sys->generators[sys->num_generators] = *gen;
    sys->total_capacity_kw += gen->capacity_kw;
    if (gen->is_renewable) sys->total_renewable_kw += gen->capacity_kw;
    sys->num_generators++;
}

/* --- Solar PV Power (L5, L4: Shockley-Queisser) -------------------- */
float energy_solar_power_kw(float irradiance_w_m2, float panel_area_m2,
                             float efficiency, float temperature_c,
                             float temp_coefficient) {
    if (irradiance_w_m2 <= 0.0f || panel_area_m2 <= 0.0f) return 0.0f;
    if (efficiency > ENERGY_SHOCKLEY_QUEISSER) efficiency = ENERGY_SHOCKLEY_QUEISSER;
    if (efficiency < 0.0f) efficiency = 0.0f;
    float temp_loss = 1.0f - temp_coefficient * (temperature_c - 25.0f);
    if (temp_loss < 0.0f) temp_loss = 0.0f;
    if (temp_loss > 1.3f) temp_loss = 1.3f;
    return irradiance_w_m2 * panel_area_m2 * efficiency * temp_loss * 0.001f;
}

/* --- Wind Turbine Power (L5, L4: Betz Limit) ----------------------- */
float energy_wind_power_kw(float wind_speed_m_s, float rotor_diameter_m,
                            float efficiency, float air_density_kg_m3) {
    if (wind_speed_m_s < 3.0f || wind_speed_m_s > 25.0f) return 0.0f;
    if (rotor_diameter_m <= 0.0f) return 0.0f;
    float cp = efficiency < ENERGY_BETZ_LIMIT ? efficiency : ENERGY_BETZ_LIMIT;
    if (cp < 0.0f) cp = 0.0f;
    float area = M_PI * rotor_diameter_m * rotor_diameter_m * 0.25f;
    float power_w = 0.5f * air_density_kg_m3 * area * cp
                    * wind_speed_m_s * wind_speed_m_s * wind_speed_m_s;
    if (wind_speed_m_s > 12.0f) {
        float rated = 0.5f * air_density_kg_m3 * area * cp * 1728.0f;
        if (power_w > rated) power_w = rated;
    }
    return power_w * 0.001f;
}

/* --- Carnot Efficiency (L4: 2nd Law of Thermodynamics) ------------- */
float energy_carnot_efficiency(float T_hot_K, float T_cold_K) {
    if (T_hot_K <= 0.0f || T_cold_K <= 0.0f || T_cold_K >= T_hot_K) return 0.0f;
    return 1.0f - T_cold_K / T_hot_K;
}
float energy_rankine_efficiency(float T_boiler_K, float T_condenser_K,
                                 float turbine_efficiency, float pump_efficiency) {
    if (T_boiler_K <= T_condenser_K) return 0.0f;
    float carnot = energy_carnot_efficiency(T_boiler_K, T_condenser_K);
    if (turbine_efficiency <= 0.0f) turbine_efficiency = 0.85f;
    if (pump_efficiency <= 0.0f) pump_efficiency = 0.80f;
    float factor = turbine_efficiency * pump_efficiency
                 / (turbine_efficiency + pump_efficiency * (1.0f - carnot));
    if (factor > 1.0f) factor = 1.0f;
    return carnot * factor;
}

/* --- Betz Limit (L4: Fluid Dynamics) ------------------------------- */
float energy_betz_power_limit(float rotor_diameter_m, float wind_speed_m_s, float air_density_kg_m3) {
    if (rotor_diameter_m <= 0.0f || wind_speed_m_s <= 0.0f) return 0.0f;
    float area = M_PI * rotor_diameter_m * rotor_diameter_m * 0.25f;
    return ENERGY_BETZ_LIMIT * 0.5f * air_density_kg_m3 * area
           * wind_speed_m_s * wind_speed_m_s * wind_speed_m_s * 0.001f;
}
float energy_betz_cp_ideal(void) { return ENERGY_BETZ_LIMIT; }

/* --- Shockley-Queisser Limit (L4: Semiconductor Physics) ----------- */
float energy_sq_limit_power_per_area(float bandgap_ev) {
    if (bandgap_ev <= 0.0f) return 0.0f;
    float eta;
    if (bandgap_ev < 0.8f) eta = 0.20f + 0.30f * (bandgap_ev - 0.5f) / 0.3f;
    else if (bandgap_ev > 1.6f) eta = 0.337f - 0.15f * (bandgap_ev - 1.6f) / 1.0f;
    else eta = 0.28f + 0.057f * (bandgap_ev - 0.8f) / 0.8f;
    if (eta < 0.0f) eta = 0.0f;
    return eta * 1000.0f;
}

/* --- Carbon Footprint (L4: GHG Protocol) --------------------------- */
float energy_carbon_total_kg(const energy_system_t* sys) {
    if (!sys || sys->num_slots == 0) return 0.0f;
    float total = 0.0f;
    for (int i = 0; i < sys->num_slots; i++)
        total += sys->timeseries[i].generation_kw
               * sys->timeseries[i].carbon_intensity_gco2_kwh * 0.001f;
    return total;
}
float energy_carbon_per_kwh(const energy_system_t* sys) {
    if (!sys || sys->num_slots == 0) return 0.0f;
    float te = 0.0f, tc = 0.0f;
    for (int i = 0; i < sys->num_slots; i++) {
        te += sys->timeseries[i].generation_kw;
        tc += sys->timeseries[i].generation_kw
            * sys->timeseries[i].carbon_intensity_gco2_kwh * 0.001f;
    }
    return te > 0.0f ? tc / te : 0.0f;
}
float energy_carbon_offset_kg(const energy_system_t* sys, float grid_intensity) {
    if (!sys) return 0.0f;
    float tr = 0.0f;
    for (int i = 0; i < sys->num_slots; i++)
        if (sys->timeseries[i].solar_kw > 0.0f || sys->timeseries[i].wind_kw > 0.0f)
            tr += sys->timeseries[i].generation_kw;
    return tr * grid_intensity * 0.001f;
}
float energy_carbon_scope1_kg(const energy_system_t* sys) {
    if (!sys) return 0.0f;
    float s1 = 0.0f;
    for (int g = 0; g < sys->num_generators; g++)
        if (!sys->generators[g].is_renewable && sys->generators[g].capacity_factor > 0.0f)
            s1 += sys->generators[g].capacity_kw * sys->generators[g].capacity_factor
                * sys->generators[g].carbon_intensity_gco2_kwh * 0.001f;
    return s1;
}
float energy_carbon_scope2_kg(const energy_system_t* sys, float grid_ef) {
    if (!sys) return 0.0f;
    float imp = 0.0f;
    for (int i = 0; i < sys->num_slots; i++) imp += sys->timeseries[i].import_kw;
    return imp * grid_ef * 0.001f;
}
float energy_carbon_scope3_estimate_kg(const energy_system_t* sys) {
    if (!sys) return 0.0f;
    float s1 = energy_carbon_scope1_kg(sys);
    float s2 = energy_carbon_scope2_kg(sys, 400.0f);
    return (s1 + s2) * 3.0f;
}

/* --- IPCC GWP100 (L4: IPCC AR6) ------------------------------------ */
float energy_co2e_from_ch4_kg(float ch4_kg) { return ch4_kg * 28.0f; }
float energy_co2e_from_n2o_kg(float n2o_kg) { return n2o_kg * 273.0f; }
float energy_co2e_from_sf6_kg(float sf6_kg) { return sf6_kg * 25200.0f; }
float energy_ipcc_gwp100(int gas_id) {
    static const float gwp[] = { 1.0f, 28.0f, 273.0f, 25200.0f, 17400.0f,
        7380.0f, 12400.0f, 14600.0f, 771.0f, 3740.0f, 1530.0f, 5810.0f };
    int n = (int)(sizeof(gwp)/sizeof(gwp[0]));
    return (gas_id >= 0 && gas_id < n) ? gwp[gas_id] : 0.0f;
}

/* --- Battery Storage (L5: Coulomb Counting, Kalman Filter) --------- */
void energy_battery_init(energy_battery_t* bat, float capacity_kwh, float max_charge_kw, float rte) {
    if (!bat) return;
    memset(bat, 0, sizeof(energy_battery_t));
    bat->chemistry = ENERGY_BATTERY_LFP;
    bat->capacity_kwh = capacity_kwh;
    bat->nominal_capacity_kwh = capacity_kwh;
    bat->max_charge_rate_kw = max_charge_kw;
    bat->max_discharge_rate_kw = max_charge_kw;
    bat->roundtrip_efficiency = rte > 0.0f ? rte : 0.90f;
    bat->coulombic_efficiency = sqrtf(bat->roundtrip_efficiency);
    bat->soc_pct = 50.0f; bat->min_soc_pct = 10.0f; bat->max_soc_pct = 90.0f;
    bat->cycle_life = 5000.0f; bat->degradation_per_cycle = 0.00002f;
    bat->calendar_degradation_pct_yr = 1.5f; bat->soh_pct = 100.0f;
    bat->internal_resistance_ohm = 0.01f; bat->self_discharge_pct_day = 0.1f;
    bat->thermal_capacity_kj_K = 50.0f; bat->thermal_resistance_K_kW = 2.0f;
}
void energy_battery_init_advanced(energy_battery_t* bat, energy_battery_chemistry_t chem, float capacity_kwh, float max_c_rate) {
    if (!bat) return;
    energy_battery_init(bat, capacity_kwh, max_c_rate * capacity_kwh, 0.9f);
    bat->chemistry = chem;
    switch (chem) {
        case ENERGY_BATTERY_LFP: bat->cycle_life=6000.0f; bat->degradation_per_cycle=0.000015f; bat->roundtrip_efficiency=0.93f; break;
        case ENERGY_BATTERY_NMC: bat->cycle_life=3000.0f; bat->degradation_per_cycle=0.00003f; bat->roundtrip_efficiency=0.92f; break;
        case ENERGY_BATTERY_NCA: bat->cycle_life=2000.0f; bat->degradation_per_cycle=0.00005f; bat->roundtrip_efficiency=0.90f; break;
        case ENERGY_BATTERY_LTO: bat->cycle_life=15000.0f; bat->degradation_per_cycle=0.000007f; bat->roundtrip_efficiency=0.87f; break;
        case ENERGY_BATTERY_LEAD_ACID: bat->cycle_life=800.0f; bat->degradation_per_cycle=0.00012f; bat->roundtrip_efficiency=0.80f; break;
        case ENERGY_BATTERY_VANADIUM_FLOW: bat->cycle_life=20000.0f; bat->degradation_per_cycle=0.000001f; bat->roundtrip_efficiency=0.75f; break;
        case ENERGY_BATTERY_SODIUM_SULFUR: bat->cycle_life=4500.0f; bat->degradation_per_cycle=0.00002f; bat->roundtrip_efficiency=0.85f; bat->temperature_c=300.0f; break;
        default: break;
    }
    bat->coulombic_efficiency = sqrtf(bat->roundtrip_efficiency);
}
float energy_battery_charge(energy_battery_t* bat, float power_kw, float duration_h) {
    if (!bat || power_kw <= 0.0f || duration_h <= 0.0f) return 0.0f;
    if (power_kw > bat->max_charge_rate_kw) power_kw = bat->max_charge_rate_kw;
    float etc = power_kw * duration_h;
    float avail = (bat->max_soc_pct - bat->soc_pct) * 0.01f * bat->capacity_kwh;
    if (etc > avail) etc = avail;
    float stored = etc * bat->coulombic_efficiency;
    bat->soc_pct += etc / bat->capacity_kwh * 100.0f;
    if (bat->soc_pct > bat->max_soc_pct) bat->soc_pct = bat->max_soc_pct;
    float cf = etc / bat->capacity_kwh;
    bat->cycles_used += cf;
    bat->capacity_kwh *= (1.0f - bat->degradation_per_cycle * cf);
    bat->temperature_c += power_kw * (1.0f - bat->coulombic_efficiency) * duration_h * 3600.0f / bat->thermal_capacity_kj_K;
    bat->is_charging = true;
    return stored;
}
float energy_battery_discharge(energy_battery_t* bat, float power_kw, float duration_h) {
    if (!bat || power_kw <= 0.0f || duration_h <= 0.0f) return 0.0f;
    if (power_kw > bat->max_discharge_rate_kw) power_kw = bat->max_discharge_rate_kw;
    float ereq = power_kw * duration_h;
    float avail = (bat->soc_pct - bat->min_soc_pct) * 0.01f * bat->capacity_kwh;
    if (ereq > avail) ereq = avail;
    bat->soc_pct -= ereq / bat->capacity_kwh * 100.0f;
    if (bat->soc_pct < bat->min_soc_pct) bat->soc_pct = bat->min_soc_pct;
    bat->soc_pct -= bat->self_discharge_pct_day * duration_h / 24.0f;
    if (bat->soc_pct < 0.0f) bat->soc_pct = 0.0f;
    bat->is_charging = false;
    return ereq * bat->coulombic_efficiency;
}
float energy_battery_soh(const energy_battery_t* bat) {
    if (!bat || bat->nominal_capacity_kwh <= 0.0f) return 0.0f;
    float soh = 100.0f * bat->capacity_kwh / bat->nominal_capacity_kwh;
    return soh > 0.0f ? soh : 0.0f;
}

float energy_battery_soc_coulomb(const energy_battery_t* bat, float current_a, float duration_s) {
    if (!bat || bat->nominal_capacity_kwh <= 0.0f) return bat ? bat->soc_pct : 0.0f;
    float ah_change = current_a * duration_s / 3600.0f;
    float nominal_ah = bat->nominal_capacity_kwh / 0.4f;
    float soc_change = ah_change / nominal_ah * 100.0f;
    float ns = bat->soc_pct + soc_change;
    if (ns > 100.0f) ns = 100.0f; if (ns < 0.0f) ns = 0.0f;
    return ns;
}
float energy_battery_thermal_loss_kw(const energy_battery_t* bat, float ambient_temp_c) {
    if (!bat) return 0.0f;
    float dT = bat->temperature_c - ambient_temp_c;
    float cond = dT / bat->thermal_resistance_K_kW;
    float pwr = bat->is_charging ? bat->max_charge_rate_kw * 0.5f : bat->max_discharge_rate_kw * 0.5f;
    float i2r = bat->internal_resistance_ohm * pwr * pwr * 0.001f;
    return i2r + cond;
}
float energy_battery_degradation_cycle(const energy_battery_t* bat, float dod) {
    if (!bat || dod <= 0.0f || dod > 1.0f) return 0.0f;
    float beta = 0.8f;
    float a = bat->cycle_life * powf(0.8f, beta);
    float cycles = a / powf(dod, beta);
    return 1.0f / cycles;
}
void energy_battery_rainflow_count(energy_battery_t* bat, float soc_delta) {
    if (!bat || bat->num_cycle_bins >= 100) return;
    float ad = soc_delta > 0.0f ? soc_delta : -soc_delta;
    int bin = (int)(ad / 10.0f);
    if (bin >= 100) bin = 99; if (bin < 0) bin = 0;
    bat->cycle_count_array[bin]++;
    bat->num_cycle_bins++;
}
float energy_battery_kalman_soc(const energy_battery_t* bat, float voltage_v, float current_a, float dt_s) {
    if (!bat) return 0.0f;
    float sp = energy_battery_soc_coulomb(bat, current_a, dt_s);
    float sf = sp * 0.01f;
    float ocv;
    if (sf < 0.1f) ocv = 3.0f + 0.2f * sf / 0.1f;
    else if (sf < 0.9f) ocv = 3.2f + 0.15f * (sf - 0.1f) / 0.8f;
    else ocv = 3.35f + 0.15f * (sf - 0.9f) / 0.1f;
    float residual = voltage_v - (ocv - current_a * bat->internal_resistance_ohm);
    float K = 0.01f;
    float sc = sp + K * residual * 10.0f;
    if (sc > 100.0f) sc = 100.0f; if (sc < 0.0f) sc = 0.0f;
    return sc;
}

/* --- Load Forecasting (L5: Holt-Winters) --------------------------- */
void energy_load_forecast_init(void) {}
float energy_load_forecast_hour(const energy_timeslot_t* history, int n, int hour, float alpha, float beta, float gamma) {
    if (!history || n < 24) return 0.0f;
    float series[168]; int m = n < 168 ? n : 168;
    for (int i = 0; i < m; i++) series[i] = history[i].load_kw;
    return energy_holt_winters_additive(series, m, 24, 1, alpha, beta, gamma);
}
float energy_holt_winters_additive(const float* series, int n, int period, int horizon, float alpha, float beta, float gamma) {
    if (!series || n < period || period < 2) return 0.0f;
    float level = 0.0f; for (int i = 0; i < period; i++) level += series[i]; level /= (float)period;
    float trend = 0.0f; for (int i = period; i < 2*period && i < n; i++) trend += series[i]-series[i-period]; trend /= (float)(period*period);
    float* seasonal = (float*)calloc(period, sizeof(float));
    if (!seasonal) return level;
    for (int i = 0; i < period && i < n; i++) seasonal[i] = series[i] - level;
    for (int t = period; t < n; t++) {
        float ol = level;
        level = alpha*(series[t]-seasonal[t%period]) + (1.0f-alpha)*(level+trend);
        trend = beta*(level-ol) + (1.0f-beta)*trend;
        seasonal[t%period] = gamma*(series[t]-level) + (1.0f-gamma)*seasonal[t%period];
        if (seasonal[t%period] > level*0.5f) seasonal[t%period] = level*0.5f;
        if (seasonal[t%period] < -level*0.5f) seasonal[t%period] = -level*0.5f;
    }
    float fc = level + (float)horizon*trend + seasonal[(n-1+horizon)%period];
    free(seasonal);
    return fc > 0.0f ? fc : 0.0f;
}
float energy_holt_winters_multiplicative(const float* series, int n, int period, int horizon, float alpha, float beta, float gamma) {
    if (!series || n < period || period < 2) return 0.0f;
    float level = 0.0f; for (int i = 0; i < period; i++) level += series[i]; level /= (float)period; if (level <= 0.0f) level = 1.0f;
    float trend = 0.0f; for (int i = period; i < 2*period && i < n; i++) trend += series[i]-series[i-period]; trend /= (float)(period*period);
    float* seasonal = (float*)calloc(period, sizeof(float)); if (!seasonal) return level;
    for (int i = 0; i < period && i < n; i++) seasonal[i] = series[i]/level;
    for (int t = period; t < n; t++) {
        float ol = level; float s = seasonal[t%period]; if (s < 0.001f) s = 0.001f;
        level = alpha*(series[t]/s) + (1.0f-alpha)*(level+trend);
        trend = beta*(level-ol) + (1.0f-beta)*trend;
        if (level > 0.001f) seasonal[t%period] = gamma*(series[t]/level) + (1.0f-gamma)*seasonal[t%period];
    }
    int si = (n-1+horizon)%period; float sv = seasonal[si] > 0.001f ? seasonal[si] : 1.0f;
    float fc = (level+(float)horizon*trend)*sv; free(seasonal);
    return fc > 0.0f ? fc : 0.0f;
}
float energy_load_forecast_similar_day(const energy_timeslot_t* history, int n, int target_dow, int target_hour) {
    if (!history || n < 168) return 0.0f;
    float sum = 0.0f; int count = 0;
    for (int i = 0; i < n; i++) {
        int sd = (history[i].day-1)+(history[i].month-1)*30+(history[i].year-2020)*365;
        int dow = (sd+4)%7;
        if (dow==target_dow && history[i].hour==target_hour) { sum += history[i].load_kw; count++; }
    }
    return count > 0 ? sum/(float)count : 0.0f;
}
/* energy_persistence_forecast: defined in energy_forecast.c */

/* --- LCOE / LCOS / Financial (L4: NREL Methodology) ---------------- */
float energy_lcoe(const energy_generator_t* gen, float discount_rate) {
    if (!gen || gen->lifetime_years <= 0.0f) return 0.0f;
    if (discount_rate <= 0.0f) discount_rate = 0.05f;
    float tc = gen->capex_per_kw * gen->capacity_kw, te = 0.0f;
    int life = (int)gen->lifetime_years; if (life > 50) life = 50;
    for (int y = 0; y < life; y++) {
        float df = 1.0f/powf(1.0f+discount_rate,(float)(y+1));
        float ann = gen->capacity_kw*gen->capacity_factor*8760.0f;
        ann *= powf(1.0f-gen->degradation_rate_per_year,(float)y);
        tc += (gen->opex_per_kwh*ann + gen->fixed_opex_per_kw_yr*gen->capacity_kw)*df;
        te += ann*df;
    }
    return te > 0.0f ? tc/te : 0.0f;
}
float energy_lcoe_with_inflation(const energy_generator_t* gen, float dr, float inf) {
    float rr = (1.0f+dr)/(1.0f+inf)-1.0f;
    return energy_lcoe(gen, rr > 0.0f ? rr : 0.01f);
}
float energy_lcos(float cpk, float opk, float rte, float cpy, float dod, float lf, float dr) {
    if (lf <= 0.0f || cpy <= 0.0f) return 0.0f;
    float tc = cpk, te = 0.0f;
    int life = (int)lf; if (life > 30) life = 30;
    for (int y = 0; y < life; y++) {
        float df = 1.0f/powf(1.0f+dr,(float)(y+1));
        tc += opk*df; te += cpy*dod*rte*df;
    }
    return te > 0.0f ? tc/te : 0.0f;
}
float energy_npv(const float* cf, int n, float dr) {
    if (!cf || n <= 0) return 0.0f;
    float npv = 0.0f;
    for (int t = 0; t < n; t++) npv += cf[t]/powf(1.0f+dr,(float)t);
    return npv;
}
float energy_irr(const float* cf, int n) {
    if (!cf || n <= 1) return 0.0f;
    float r = 0.1f;
    for (int iter = 0; iter < 100; iter++) {
        float npv=0.0f, dnpv=0.0f;
        for (int t=0; t<n; t++) {
            float df=1.0f/powf(1.0f+r,(float)t);
            npv+=cf[t]*df; dnpv-=(float)t*cf[t]*df/(1.0f+r);
        }
        if (fabsf(dnpv) < 1e-10f) break;
        float rn = r - npv/dnpv;
        if (fabsf(rn-r) < 1e-6f) { r=rn; break; }
        r=rn; if (r<-0.99f) r=-0.99f; if (r>10.0f) r=10.0f;
    }
    return r;
}
float energy_payback_years(float capex, float ar, float ao) {
    float net = ar - ao; return net <= 0.0f ? 1e9f : capex/net;
}

/* --- Self-Consumption Optimization (L5: Greedy + LP) --------------- */
float energy_self_consumption_optimize(energy_system_t* sys, int horizon_h) {
    if (!sys || horizon_h <= 0 || horizon_h > sys->num_slots) return 0.0f;
    float tg=0.0f, sc=0.0f;
    for (int i=0; i<horizon_h; i++) {
        float gen=sys->timeseries[i].generation_kw, load=sys->timeseries[i].load_kw;
        tg+=gen; float net=gen-load;
        if (net>0.0f) { energy_battery_charge(&sys->battery,net,1.0f); sc+=load; }
        else { float dis=energy_battery_discharge(&sys->battery,-net,1.0f); sc+=dis>0.0f?load:gen; }
    }
    sys->self_consumption_pct = tg>0.0f?(sc/tg)*100.0f:0.0f;
    return sys->self_consumption_pct;
}
float energy_self_sufficiency(const energy_system_t* sys) {
    if (!sys) return 0.0f;
    float tl=0.0f, ss=0.0f;
    for (int i=0;i<sys->num_slots;i++) {
        tl+=sys->timeseries[i].load_kw;
        ss+=sys->timeseries[i].generation_kw-sys->timeseries[i].export_kw+sys->timeseries[i].storage_discharge_kw;
    }
    return tl>0.0f?(ss/tl)*100.0f:0.0f;
}
float energy_maximize_self_consumption_lp(energy_system_t* sys, int horizon_h) {
    if (!sys||horizon_h<=0) return 0.0f;
    energy_self_consumption_optimize(sys,horizon_h);
    for (int i=0;i<horizon_h;i++)
        if (sys->timeseries[i].price_per_kwh>0.30f && sys->timeseries[i].generation_kw<sys->timeseries[i].load_kw)
            energy_battery_discharge(&sys->battery,(sys->timeseries[i].load_kw-sys->timeseries[i].generation_kw)*0.5f,1.0f);
    return sys->self_consumption_pct;
}

/* --- Power Flow / Transmission (L3: Engineering, L4: Ohm's Law) ---- */
float energy_transmission_loss_kw(float power_kw, float voltage_kv, float resistance_ohm, float distance_km) {
    if (voltage_kv <= 0.0f || power_kw <= 0.0f) return 0.0f;
    float ika = power_kw/voltage_kv;
    return ika*ika*resistance_ohm*distance_km;
}
float energy_transformer_loss_kw(float power_kw, float rating_kva, float nll_pct, float ll_pct) {
    if (rating_kva <= 0.0f) return 0.0f;
    float load = power_kw/rating_kva;
    return nll_pct*0.01f*rating_kva + ll_pct*0.01f*rating_kva*load*load;
}
float energy_line_reactance_ohm(float inductance_mh, float frequency_hz) {
    return 2.0f*M_PI*frequency_hz*inductance_mh*0.001f;
}
float energy_power_factor_correction_kvar(float real_power_kw, float current_pf, float target_pf) {
    if (current_pf<=0.0f||current_pf>1.0f||target_pf<=0.0f||target_pf>1.0f) return 0.0f;
    float a1=acosf(current_pf), a2=acosf(target_pf);
    return real_power_kw*(tanf(a1)-tanf(a2));
}

/* --- Building Energy (L7: ASHRAE/ISO 13790) ------------------------ */
float energy_heating_degree_days(float t_bal, const energy_timeslot_t* ts, int n) {
    if (!ts||n<=0) return 0.0f;
    float hdd=0.0f; for (int i=0;i<n;i++) { float d=t_bal-ts[i].temperature_c; if(d>0)hdd+=d; }
    return hdd/24.0f;
}
float energy_cooling_degree_days(float t_bal, const energy_timeslot_t* ts, int n) {
    if (!ts||n<=0) return 0.0f;
    float cdd=0.0f; for (int i=0;i<n;i++) { float d=ts[i].temperature_c-t_bal; if(d>0)cdd+=d; }
    return cdd/24.0f;
}
float energy_building_heat_loss_kw(const energy_thermal_zone_t* zone, float t_out) {
    if (!zone) return 0.0f;
    float dT=zone->heating_setpoint_c-t_out; if(dT<=0)return 0.0f;
    float wa=zone->floor_area_m2*2.5f, wl=zone->wall_u_value*wa*dT;
    float rl=zone->roof_u_value*zone->floor_area_m2*dT;
    float vol=zone->floor_area_m2*2.5f, inf=0.33f*zone->infiltration_ach*vol*dT;
    return (wl+rl+inf)*0.001f;
}
float energy_building_heat_gain_kw(const energy_thermal_zone_t* zone, float sol, float t_out) {
    if (!zone) return 0.0f;
    float wa=zone->floor_area_m2*zone->window_to_wall_ratio*0.5f, sg=sol*wa*0.5f;
    float tr=0.0f;
    if (t_out>zone->cooling_setpoint_c) { float wa2=zone->floor_area_m2*2.5f; tr=zone->wall_u_value*wa2*(t_out-zone->cooling_setpoint_c); }
    return (sg+tr+zone->internal_gains_kw*1000.0f)*0.001f;
}
float energy_hvac_load_kw(const energy_thermal_zone_t* zone, float t_out, float sol) {
    if (!zone) return 0.0f;
    float hl=energy_building_heat_loss_kw(zone,t_out), hg=energy_building_heat_gain_kw(zone,sol,t_out);
    return fabsf(hl-hg);
}

/* --- Heat Pump (L7: Applications, Carnot COP) ---------------------- */
float energy_heatpump_carnot_cop(float T_sink_K, float T_source_K) {
    float dt=T_sink_K-T_source_K; if(dt<=0||T_sink_K<=0)return 1.0f;
    return T_sink_K/dt;
}
float energy_heatpump_cop(const energy_heatpump_t* hp, float Ts, float Tk) {
    if (!hp) return 1.0f;
    float TsK=Ts+273.15f, TkK=Tk+273.15f, cc=energy_heatpump_carnot_cop(TkK,TsK);
    float corr=0.5f;
    if(Ts<hp->source_temp_min_c)corr=0.2f; else if(Ts>hp->source_temp_max_c)corr=0.8f;
    float cop=corr*cc; if(cop>hp->rated_cop*1.5f)cop=hp->rated_cop*1.5f; if(cop<1.0f)cop=1.0f;
    return cop;
}
float energy_heatpump_power_kw(const energy_heatpump_t* hp, float hl, float Ts, float Tk) {
    if (!hp||hl<=0)return 0.0f;
    float cop=energy_heatpump_cop(hp,Ts,Tk), pwr=hl/cop;
    if(pwr>hp->rated_capacity_kw)pwr=hp->rated_capacity_kw;
    if(pwr<hp->rated_capacity_kw*hp->min_capacity_pct*0.01f)pwr=hp->rated_capacity_kw*hp->min_capacity_pct*0.01f;
    return pwr;
}

/* --- Green Hydrogen (L9: Industry Frontiers, IEA 2023) ------------- */
float energy_electrolyzer_h2_kg(float power_kw, float efficiency, float duration_h) {
    if (power_kw<=0||duration_h<=0) return 0.0f;
    return power_kw*duration_h*efficiency/55.0f;
}
float energy_hydrogen_lhv_kwh_per_kg(void) { return 33.33f; }
float energy_fuel_cell_power_kw(float h2_flow_kg_h, float efficiency) {
    if (h2_flow_kg_h<=0) return 0.0f;
    return h2_flow_kg_h*energy_hydrogen_lhv_kwh_per_kg()*efficiency;
}

/* --- System Simulation (L6: Canonical Problem) --------------------- */
void energy_system_simulate_slot(energy_system_t* sys, int slot_idx) {
    if (!sys||slot_idx<0||slot_idx>=sys->num_slots) return;
    energy_timeslot_t* s=&sys->timeseries[slot_idx];
    float tg=0.0f, sg=0.0f, wg=0.0f;
    for (int g=0;g<sys->num_generators;g++) {
        energy_generator_t* gen=&sys->generators[g]; float gp=0.0f;
        switch (gen->source_type) {
            case ENERGY_SOURCE_SOLAR:
                gp=energy_solar_power_kw(s->ghi_w_m2>0?s->ghi_w_m2:800.0f,1000.0f,gen->efficiency,s->temperature_c,0.004f);
                sg+=gp; break;
            case ENERGY_SOURCE_WIND:
                gp=energy_wind_power_kw(s->wind_speed_m_s,80.0f,gen->efficiency,1.225f); wg+=gp; break;
            case ENERGY_SOURCE_HYDRO: case ENERGY_SOURCE_BIOMASS: case ENERGY_SOURCE_GEOTHERMAL:
                gp=gen->capacity_kw*gen->capacity_factor; break;
            default: gp=gen->capacity_kw*gen->capacity_factor; break;
        }
        tg+=gp;
    }
    s->generation_kw=tg; s->solar_kw=sg; s->wind_kw=wg;
}
void energy_system_run(energy_system_t* sys, int num_slots) {
    if (!sys) return;
    if (num_slots>ENERGY_MAX_TIMESERIES) num_slots=ENERGY_MAX_TIMESERIES;
    sys->num_slots=num_slots;
    for (int i=0;i<num_slots;i++) {
        sys->timeseries[i].hour=i%24; sys->timeseries[i].day=i/24+1;
        sys->timeseries[i].month=((sys->timeseries[i].day-1)/30)+1;
        if(sys->timeseries[i].month>12)sys->timeseries[i].month=12;
        energy_system_simulate_slot(sys,i);
    }
    energy_self_consumption_optimize(sys,num_slots);
    energy_system_compute_kpis(sys);
}
void energy_system_compute_kpis(energy_system_t* sys) {
    if (!sys||sys->num_slots==0) return;
    float tgen=0,tload=0,trenew=0,timp=0,texp=0,peak=0,lol=0;
    for (int i=0;i<sys->num_slots;i++) {
        energy_timeslot_t* s=&sys->timeseries[i];
        tgen+=s->generation_kw; tload+=s->load_kw;
        trenew+=s->solar_kw+s->wind_kw; timp+=s->import_kw; texp+=s->export_kw;
        if(s->load_kw>peak)peak=s->load_kw;
        float served=s->generation_kw+s->import_kw+s->storage_discharge_kw;
        if(served<s->load_kw)lol+=s->load_kw-served;
    }
    sys->annual_energy_kwh=tgen; sys->peak_load_kw=peak;
    sys->renewable_fraction=tgen>0?(trenew/tgen)*100.0f:0;
    sys->grid_dependency_pct=tload>0?(timp/tload)*100.0f:0;
    sys->loss_of_load_hours=lol;
    sys->self_sufficiency_pct=tload>0?((tgen-texp+timp)/tload)*100.0f:100.0f;
    if(sys->self_sufficiency_pct>100.0f)sys->self_sufficiency_pct=100.0f;
}
int energy_system_dispatch_merit_order(energy_system_t* sys, int slot_idx, float load_kw) {
    if (!sys||slot_idx<0) return -1;
    int idx[ENERGY_MAX_DEVICES], n=sys->num_generators;
    for(int i=0;i<n;i++)idx[i]=i;
    for(int i=1;i<n;i++){int k=idx[i];float kc=sys->generators[k].opex_per_kwh;int j=i-1;
        while(j>=0&&sys->generators[idx[j]].opex_per_kwh>kc){idx[j+1]=idx[j];j--;}idx[j+1]=k;}
    float rem=load_kw;
    for(int i=0;i<n&&rem>0;i++){
        energy_generator_t* g=&sys->generators[idx[i]];
        float avail=g->capacity_kw, disp=rem<avail?rem:avail;
        if(g->min_stable_load_pct>0&&disp>0){float ml=g->capacity_kw*g->min_stable_load_pct*0.01f;if(disp<ml)disp=ml;}
        rem-=disp;
    }
    return rem<=0?0:1;
}
float energy_system_renewable_penetration(const energy_system_t* sys) {
    if (!sys||sys->total_capacity_kw<=0) return 0;
    return (sys->total_renewable_kw/sys->total_capacity_kw)*100.0f;
}

/* --- Utility Functions ---------------------------------------------- */
const char* energy_source_name(energy_source_t src) {
    static const char* n[]={"Solar PV","Wind","Hydro","Biomass","Geothermal","Nuclear","Coal","Natural Gas","Oil","Battery","Hydrogen","Wave","Tidal","Solar Thermal","Pumped Hydro","Compressed Air","Flywheel"};
    int ns=(int)(sizeof(n)/sizeof(n[0])); return (src>=0&&src<ns)?n[src]:"Unknown";
}
const char* energy_battery_chemistry_name(energy_battery_chemistry_t chem) {
    static const char* n[]={"LFP","NMC","NCA","LTO","Lead-Acid","Vanadium Flow","Sodium-Sulfur","Solid-State","Li-Capacitor"};
    int ns=(int)(sizeof(n)/sizeof(n[0])); return (chem>=0&&chem<ns)?n[chem]:"Unknown";
}
float energy_kwh_to_mj(float kwh) { return kwh*3.6f; }
float energy_mj_to_kwh(float mj) { return mj/3.6f; }
