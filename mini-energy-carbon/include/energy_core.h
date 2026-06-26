#ifndef ENERGY_CORE_H
#define ENERGY_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Module Constants (L1: Definitions) ──────────────────────────────── */
#define ENERGY_MAX_DEVICES         1024
#define ENERGY_MAX_TIMESERIES      8760   /* max hours in a year */
#define ENERGY_MAX_NAME             256
#define ENERGY_MAX_BUSES            128
#define ENERGY_MAX_CIRCUITS         256
#define ENERGY_MAX_THERMAL_ZONES     64
#define ENERGY_MAX_EV_CHARGERS       512
#define ENERGY_MAX_SEASONS            12

/* ── Physical Constants (L4: Standards) ──────────────────────────────── */
#define ENERGY_STEFAN_BOLTZMANN    5.670374419e-8f  /* W/(m²·K⁴) */
#define ENERGY_SOLAR_CONSTANT      1361.0f           /* W/m², satellite-measured */
#define ENERGY_CARNOT_T_HOT         573.15f           /* K, typical steam turbine */
#define ENERGY_CARNOT_T_COLD        303.15f           /* K, cooling water */
#define ENERGY_BETZ_LIMIT             0.59259f        /* 16/27, Betz 1919 */
#define ENERGY_SHOCKLEY_QUEISSER     0.337f           /* SQ limit for single-junction Si */
#define ENERGY_AIR_DENSITY_STD       1.225f           /* kg/m³ at 15°C, sea level */
#define ENERGY_GRAVITY               9.80665f         /* m/s² */
#define ENERGY_WATER_DENSITY      1000.0f             /* kg/m³ */
#define ENERGY_SPECIFIC_HEAT_WATER   4.186f           /* kJ/(kg·K) */

/* ── Energy Source Enumeration (L1: Definitions) ──────────────────────── */
typedef enum {
    ENERGY_SOURCE_SOLAR = 0,
    ENERGY_SOURCE_WIND,
    ENERGY_SOURCE_HYDRO,
    ENERGY_SOURCE_BIOMASS,
    ENERGY_SOURCE_GEOTHERMAL,
    ENERGY_SOURCE_NUCLEAR,
    ENERGY_SOURCE_COAL,
    ENERGY_SOURCE_NATURAL_GAS,
    ENERGY_SOURCE_OIL,
    ENERGY_SOURCE_BATTERY,
    ENERGY_SOURCE_HYDROGEN,
    ENERGY_SOURCE_WAVE,
    ENERGY_SOURCE_TIDAL,
    ENERGY_SOURCE_SOLAR_THERMAL,
    ENERGY_SOURCE_PUMPED_HYDRO,
    ENERGY_SOURCE_COMPRESSED_AIR,
    ENERGY_SOURCE_FLYWHEEL,
} energy_source_t;

/* ── Carbon Accounting Scopes (GHG Protocol, L4) ─────────────────────── */
typedef enum {
    ENERGY_CARBON_SCOPE1 = 0,  /* direct emissions from owned sources */
    ENERGY_CARBON_SCOPE2,      /* indirect from purchased electricity */
    ENERGY_CARBON_SCOPE3,      /* all other indirect in value chain */
} energy_carbon_scope_t;

/* ── Microgrid Topology (L3: Engineering Structures) ─────────────────── */
typedef enum {
    ENERGY_TOPOLOGY_RADIAL = 0,
    ENERGY_TOPOLOGY_MESH,
    ENERGY_TOPOLOGY_ISLAND,
    ENERGY_TOPOLOGY_RING,
    ENERGY_TOPOLOGY_HYBRID,
} energy_topology_t;

/* ── Energy Market Types (L7: Applications) ──────────────────────────── */
typedef enum {
    ENERGY_MARKET_DAY_AHEAD = 0,
    ENERGY_MARKET_REAL_TIME,
    ENERGY_MARKET_ANCILLARY_SVC,
    ENERGY_MARKET_CAPACITY,
    ENERGY_MARKET_REC,
    ENERGY_MARKET_CARBON_CREDIT,
    ENERGY_MARKET_PPA,
} energy_market_type_t;

/* ── Battery Chemistry Types (L8: Advanced) ──────────────────────────── */
typedef enum {
    ENERGY_BATTERY_LFP = 0,
    ENERGY_BATTERY_NMC,
    ENERGY_BATTERY_NCA,
    ENERGY_BATTERY_LTO,
    ENERGY_BATTERY_LEAD_ACID,
    ENERGY_BATTERY_VANADIUM_FLOW,
    ENERGY_BATTERY_SODIUM_SULFUR,
    ENERGY_BATTERY_SOLID_STATE,
    ENERGY_BATTERY_LICAP,
} energy_battery_chemistry_t;

/* ── Thermal Energy Storage Types (L7: Applications) ─────────────────── */
typedef enum {
    ENERGY_THERMAL_SENSIBLE = 0,
    ENERGY_THERMAL_LATENT,
    ENERGY_THERMAL_THERMOCHEMICAL,
    ENERGY_THERMAL_ICE,
    ENERGY_THERMAL_HOT_WATER,
    ENERGY_THERMAL_MOLTEN_SALT,
} energy_thermal_type_t;

/* ── Carbon Offset Standard Types (L7: Applications) ─────────────────── */
typedef enum {
    ENERGY_OFFSET_VCS = 0,     /* Verified Carbon Standard */
    ENERGY_OFFSET_GOLD_STD,    /* Gold Standard */
    ENERGY_OFFSET_CDM,         /* Clean Development Mechanism */
    ENERGY_OFFSET_CORSIA,      /* aviation offset */
    ENERGY_OFFSET_ACR,         /* American Carbon Registry */
    ENERGY_OFFSET_CAR,         /* Climate Action Reserve */
} energy_offset_standard_t;

/* ── Timeslot: hourly energy balance record (L1: Definitions) ───────── */
typedef struct {
    int    year, month, day, hour;
    float  load_kw;                    /* gross electric load */
    float  generation_kw;              /* total generation */
    float  solar_kw;                   /* solar PV contribution */
    float  wind_kw;                    /* wind contribution */
    float  import_kw;                  /* grid import */
    float  export_kw;                  /* grid export */
    float  storage_charge_kw;          /* battery charge power */
    float  storage_discharge_kw;       /* battery discharge power */
    float  soc_before_pct;             /* SOC before this slot */
    float  soc_after_pct;              /* SOC after this slot */
    float  carbon_intensity_gco2_kwh;  /* grid carbon intensity */
    float  marginal_emission_gco2_kwh; /* marginal emission factor */
    float  price_per_kwh;              /* wholesale electricity price */
    float  feed_in_tariff;             /* export price per kWh */
    float  temperature_c;              /* ambient dry-bulb temp */
    float  dewpoint_c;                 /* dew point temperature */
    float  cloud_cover_pct;            /* 0-100 opacity */
    float  wind_speed_m_s;             /* at hub height */
    float  ghi_w_m2;                   /* Global Horizontal Irradiance */
    float  dni_w_m2;                   /* Direct Normal Irradiance */
    float  dhi_w_m2;                   /* Diffuse Horizontal Irradiance */
    float  humidity_pct;               /* relative humidity */
    float  pressure_hpa;               /* atmospheric pressure */
} energy_timeslot_t;

/* ── Generator model (L1: Definitions → L4: NREL LCOE) ──────────────── */
typedef struct {
    energy_source_t source_type;
    char   name[ENERGY_MAX_NAME];
    float  capacity_kw;              /* nameplate (AC for solar) */
    float  efficiency;               /* conversion efficiency 0-1 */
    float  lifetime_years;           /* economic life */
    float  capex_per_kw;             /* overnight capital cost */
    float  opex_per_kwh;             /* variable O&M */
    float  fixed_opex_per_kw_yr;     /* fixed O&M annual */
    float  carbon_intensity_gco2_kwh;/* lifecycle gCO₂/kWh */
    float  capacity_factor;          /* annual average */
    float  degradation_rate_per_year;/* annual efficiency loss */
    float  min_stable_load_pct;      /* minimum generation % */
    float  ramp_up_kw_per_min;       /* ramp rate up */
    float  ramp_down_kw_per_min;     /* ramp rate down */
    float  startup_cost;             /* $ per start */
    float  min_uptime_h;             /* minimum run hours */
    float  min_downtime_h;           /* minimum off hours */
    float  heat_rate_btu_kwh;        /* thermal efficiency metric */
    float  fuel_cost_per_mmbtu;      /* fuel price */
    bool   is_dispatchable;
    bool   is_renewable;
    bool   is_must_run;
} energy_generator_t;

/* ── Battery model (L1: Definitions → L5: Degradation) ──────────────── */
typedef struct {
    energy_battery_chemistry_t chemistry;
    float  capacity_kwh;              /* usable (accounting for DoD limits) */
    float  nominal_capacity_kwh;      /* nameplate before degradation */
    float  max_charge_rate_kw;        /* C-rate × capacity */
    float  max_discharge_rate_kw;
    float  roundtrip_efficiency;      /* AC-to-AC */
    float  coulombic_efficiency;      /* charge efficiency only */
    float  soc_pct;                   /* state of charge 0-100 */
    float  min_soc_pct;
    float  max_soc_pct;
    float  cycle_life;                /* cycles at 80% DoD */
    float  cycles_used;               /* equivalent full cycles */
    float  degradation_per_cycle;     /* fractional capacity loss/cycle */
    float  calendar_degradation_pct_yr; /* time-based aging */
    float  soh_pct;                   /* state of health */
    float  internal_resistance_ohm;   /* R_int for I²R losses */
    float  temperature_c;             /* cell temperature */
    float  thermal_capacity_kj_K;     /* heat capacity */
    float  thermal_resistance_K_kW;   /* thermal resistance to ambient */
    float  self_discharge_pct_day;    /* daily self-discharge */
    bool   is_charging;
    int    cycle_count_array[100];    /* rainflow partial cycle bins */
    int    num_cycle_bins;
} energy_battery_t;

/* ── Grid Interconnection Point (L3: Engineering Structures) ─────────── */
typedef struct {
    char   name[ENERGY_MAX_NAME];
    float  voltage_kv;
    float  max_import_kw;
    float  max_export_kw;
    float  interconnection_cost;
    float  transformer_efficiency;
    float  line_resistance_ohm;
    float  line_inductance_mh;
    float  short_circuit_current_ka;
    bool   is_net_metered;
    bool   has_export_limit;
} energy_grid_pcc_t;

/* ── Microgrid / Energy System (L3: Engineering Structures) ──────────── */
typedef struct {
    energy_generator_t  generators[ENERGY_MAX_DEVICES];
    int                 num_generators;
    energy_battery_t    battery;
    energy_grid_pcc_t   grid_pcc;
    energy_topology_t   topology;
    energy_timeslot_t   timeseries[ENERGY_MAX_TIMESERIES];
    int                 num_slots;
    float               total_capacity_kw;
    float               total_renewable_kw;
    float               peak_load_kw;
    float               total_carbon_kg;
    float               renewable_fraction;       /* 0-100 pct */
    float               self_consumption_pct;
    float               self_sufficiency_pct;
    float               lcoe_per_kwh;
    float               annual_energy_kwh;
    float               curtailed_energy_kwh;
    float               loss_of_load_hours;
    float               grid_dependency_pct;
} energy_system_t;

/* ── Thermal Zone / Building Envelope (L7: Applications) ─────────────── */
typedef struct {
    char   name[ENERGY_MAX_NAME];
    float  floor_area_m2;
    float  wall_u_value;            /* W/(m²·K) */
    float  roof_u_value;
    float  window_u_value;
    float  window_to_wall_ratio;    /* 0-1 */
    float  infiltration_ach;        /* air changes per hour */
    float  thermal_mass_kj_K;       /* effective heat capacity */
    float  internal_gains_kw;       /* occupants + equipment */
    float  heating_setpoint_c;
    float  cooling_setpoint_c;
    float  ventilation_rate_m3_s;
    float  heat_recovery_efficiency;
} energy_thermal_zone_t;

/* ── Heat Pump Model (L7: Applications) ──────────────────────────────── */
typedef struct {
    float  rated_capacity_kw;        /* at standard conditions */
    float  rated_cop;                /* coefficient of performance */
    float  min_capacity_pct;         /* turndown ratio */
    float  source_temp_min_c;        /* minimum source temperature */
    float  source_temp_max_c;        /* maximum source temperature */
    float  sink_temp_min_c;
    float  sink_temp_max_c;
    char   refrigerant[32];          /* e.g., R-32, R-290, R-744 */
    float  gwp_of_refrigerant;
    float  charge_kg;
    float  annual_leakage_pct;
} energy_heatpump_t;

/* ── EV Charger Model (L7: Applications) ─────────────────────────────── */
typedef struct {
    float  max_power_kw;             /* L1=1.8, L2=7.2, DCFC=150 */
    float  efficiency;               /* AC-DC conversion */
    float  arrival_soc_pct;          /* typical arrival SOC */
    float  target_soc_pct;           /* desired departure SOC */
    float  battery_capacity_kwh;     /* vehicle battery size */
    float  parking_duration_h;       /* expected plugged-in time */
    bool   is_bidirectional;         /* V2G capable */
    bool   is_smart_charging;
} energy_ev_charger_t;

/* ── Power Purchase Agreement (L7: Applications) ─────────────────────── */
typedef struct {
    char   counterparty[ENERGY_MAX_NAME];
    float  contract_capacity_kw;
    float  fixed_price_per_kwh;
    float  index_price_per_kwh;       /* wholesale index component */
    float  fixed_escalator_pct;       /* annual price escalation */
    float  settlement_period_days;
    float  contract_start_year;
    float  contract_duration_years;
    float  liquidated_damages_per_kwh;/* penalty for non-delivery */
    bool   is_physically_settled;
    bool   is_virtual_ppa;
    float  shape_risk_premium_pct;    /* generation shape mismatch */
} energy_ppa_t;

/* ── Solar PV Power Model (L5: Algorithm, L4: Shockley-Queisser) ─────── */
float energy_solar_power_kw(float irradiance_w_m2, float panel_area_m2,
                             float efficiency, float temperature_c,
                             float temp_coefficient);

/* Wind turbine power (L5: Algorithm, L4: Betz Limit) */
float energy_wind_power_kw(float wind_speed_m_s, float rotor_diameter_m,
                            float efficiency, float air_density_kg_m3);

/* Carnot efficiency limit (L4: Thermodynamics) */
float energy_carnot_efficiency(float T_hot_K, float T_cold_K);

/* Rankine cycle thermal efficiency (L4: Thermodynamics) */
float energy_rankine_efficiency(float T_boiler_K, float T_condenser_K,
                                 float turbine_efficiency,
                                 float pump_efficiency);

/* Betz limit verification (L4: Fluid Dynamics) */
float energy_betz_power_limit(float rotor_diameter_m, float wind_speed_m_s,
                               float air_density_kg_m3);
float energy_betz_cp_ideal(void);

/* Shockley-Queisser limit (L4: Semiconductor Physics) */
float energy_sq_limit_power_per_area(float bandgap_ev);

/* ── Carbon Footprint (L4: GHG Protocol) ──────────────────────────────── */
float energy_carbon_total_kg(const energy_system_t* sys);
float energy_carbon_per_kwh(const energy_system_t* sys);
float energy_carbon_offset_kg(const energy_system_t* sys, float grid_intensity);
float energy_carbon_scope1_kg(const energy_system_t* sys);
float energy_carbon_scope2_kg(const energy_system_t* sys, float grid_emission_factor);
float energy_carbon_scope3_estimate_kg(const energy_system_t* sys);

/* Equivalent CO₂ conversion (L4: IPCC GWP100) */
float energy_co2e_from_ch4_kg(float ch4_kg);
float energy_co2e_from_n2o_kg(float n2o_kg);
float energy_co2e_from_sf6_kg(float sf6_kg);
float energy_ipcc_gwp100(int gas_id);

/* ── Battery Storage (L5: Coulomb counting, Kalman SOC) ──────────────── */
void  energy_battery_init(energy_battery_t* bat, float capacity_kwh,
                           float max_charge_kw, float rte);
void  energy_battery_init_advanced(energy_battery_t* bat,
                                    energy_battery_chemistry_t chem,
                                    float capacity_kwh, float max_c_rate);
float energy_battery_charge(energy_battery_t* bat, float power_kw,
                             float duration_h);
float energy_battery_discharge(energy_battery_t* bat, float power_kw,
                                float duration_h);
float energy_battery_soh(const energy_battery_t* bat);
float energy_battery_soc_coulomb(const energy_battery_t* bat, float current_a,
                                  float duration_s);
float energy_battery_thermal_loss_kw(const energy_battery_t* bat,
                                      float ambient_temp_c);
float energy_battery_degradation_cycle(const energy_battery_t* bat, float dod);
void  energy_battery_rainflow_count(energy_battery_t* bat, float soc_delta);
float energy_battery_kalman_soc(const energy_battery_t* bat, float voltage_v,
                                 float current_a, float dt_s);

/* ── Load Forecasting (L5: Holt-Winters, ARIMA) ──────────────────────── */
void  energy_load_forecast_init(void);
float energy_load_forecast_hour(const energy_timeslot_t* history, int n,
                                 int hour, float alpha, float beta, float gamma);
float energy_holt_winters_additive(const float* series, int n, int period,
                                    int horizon, float alpha, float beta,
                                    float gamma);
float energy_holt_winters_multiplicative(const float* series, int n,
                                          int period, int horizon,
                                          float alpha, float beta, float gamma);
float energy_load_forecast_similar_day(const energy_timeslot_t* history, int n,
                                        int target_dow, int target_hour);
float energy_persistence_forecast(const float* series, int n, int horizon);

/* ── LCOE / LCOS (L4: NREL Methodology) ──────────────────────────────── */
float energy_lcoe(const energy_generator_t* gen, float discount_rate);
float energy_lcoe_with_inflation(const energy_generator_t* gen,
                                  float discount_rate, float inflation_rate);
float energy_lcos(/* Levelized Cost of Storage */
    float capex_per_kwh, float opex_per_kwh_yr, float rte,
    float cycles_per_year, float depth_of_discharge, float lifetime_yrs,
    float discount_rate);
float energy_npv(const float* cashflows, int n, float discount_rate);
float energy_irr(const float* cashflows, int n);
float energy_payback_years(float capex, float annual_revenue,
                            float annual_opex);

/* ── Self-Consumption Optimization (L5: Greedy LP) ───────────────────── */
float energy_self_consumption_optimize(energy_system_t* sys, int horizon_h);
float energy_self_sufficiency(const energy_system_t* sys);
float energy_maximize_self_consumption_lp(energy_system_t* sys, int horizon_h);

/* ── Energy Balance / Power Flow (L3: Engineering) ───────────────────── */
float energy_transmission_loss_kw(float power_kw, float voltage_kv,
                                   float resistance_ohm, float distance_km);
float energy_transformer_loss_kw(float power_kw, float rating_kva,
                                  float no_load_loss_pct, float load_loss_pct);
float energy_line_reactance_ohm(float inductance_mh, float frequency_hz);
float energy_power_factor_correction_kvar(float real_power_kw,
                                           float current_pf, float target_pf);

/* ── Building Energy (L7: Applications) ──────────────────────────────── */
float energy_heating_degree_days(float t_balance_c, const energy_timeslot_t* ts,
                                  int n);
float energy_cooling_degree_days(float t_balance_c, const energy_timeslot_t* ts,
                                  int n);
float energy_building_heat_loss_kw(const energy_thermal_zone_t* zone,
                                    float outdoor_temp_c);
float energy_building_heat_gain_kw(const energy_thermal_zone_t* zone,
                                    float solar_radiation_w_m2,
                                    float outdoor_temp_c);
float energy_hvac_load_kw(const energy_thermal_zone_t* zone,
                           float outdoor_temp_c, float solar_w_m2);

/* ── Heat Pump (L7: Applications) ─────────────────────────────────────── */
float energy_heatpump_cop(const energy_heatpump_t* hp, float source_temp_c,
                           float sink_temp_c);
float energy_heatpump_power_kw(const energy_heatpump_t* hp, float heat_load_kw,
                                float source_temp_c, float sink_temp_c);
float energy_heatpump_carnot_cop(float T_sink_K, float T_source_K);

/* ── Green Hydrogen (L9: Industry Frontiers) ──────────────────────────── */
float energy_electrolyzer_h2_kg(float power_kw, float efficiency,
                                 float duration_h);
float energy_hydrogen_lhv_kwh_per_kg(void);
float energy_fuel_cell_power_kw(float h2_flow_kg_h, float efficiency);

/* ── System Simulation (L6: Canonical Problems) ───────────────────────── */
void energy_system_init(energy_system_t* sys);
void energy_system_add_generator(energy_system_t* sys,
                                  const energy_generator_t* gen);
void energy_system_simulate_slot(energy_system_t* sys, int slot_idx);
void energy_system_run(energy_system_t* sys, int num_slots);
void energy_system_compute_kpis(energy_system_t* sys);
int  energy_system_dispatch_merit_order(energy_system_t* sys, int slot_idx,
                                         float load_kw);
float energy_system_renewable_penetration(const energy_system_t* sys);

/* ── Utility Functions ────────────────────────────────────────────────── */
const char* energy_source_name(energy_source_t src);
const char* energy_battery_chemistry_name(energy_battery_chemistry_t chem);
float energy_kwh_to_mj(float kwh);
float energy_mj_to_kwh(float mj);
float energy_btu_to_kwh(float btu);

#ifdef __cplusplus
}
#endif

#endif
