#ifndef AGRI_CORE_H
#define AGRI_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Precision Agriculture Core (L1: Definitions) ── */

#define AGRI_MAX_FIELDS     1024
#define AGRI_MAX_CROPS      64
#define AGRI_MAX_SOIL_LAYERS 16
#define AGRI_MAX_NAME       256
#define AGRI_MAX_GPS_POINTS 4096
#define AGRI_MAX_WEATHER_DAYS 365

/* GPS coordinate in decimal degrees */
typedef struct {
    double latitude;
    double longitude;
    double elevation_m;
} agri_gps_t;

/* Field boundary polygon */
typedef struct {
    agri_gps_t vertices[AGRI_MAX_GPS_POINTS];
    int num_vertices;
    double area_hectares;
    char field_id[AGRI_MAX_NAME];
} agri_field_t;

/* Soil layer properties */
typedef struct {
    float depth_cm;
    float clay_pct;
    float silt_pct;
    float sand_pct;
    float organic_matter_pct;
    float ph;
    float cation_exchange_capacity;
    float bulk_density_g_cm3;
    float field_capacity_m3_m3;
    float wilting_point_m3_m3;
    float saturated_hydraulic_conductivity_cm_hr;
} agri_soil_layer_t;

/* Complete soil profile */
typedef struct {
    agri_soil_layer_t layers[AGRI_MAX_SOIL_LAYERS];
    int num_layers;
    float total_depth_cm;
    float available_water_capacity_mm;
    float soil_organic_carbon_ton_ha;
    char soil_type[AGRI_MAX_NAME];
    char usda_texture_class[32];
} agri_soil_profile_t;

/* Crop type enumeration */
typedef enum {
    AGRI_CROP_WHEAT = 0,
    AGRI_CROP_RICE,
    AGRI_CROP_MAIZE,
    AGRI_CROP_SOYBEAN,
    AGRI_CROP_POTATO,
    AGRI_CROP_COTTON,
    AGRI_CROP_SUGARCANE,
    AGRI_CROP_TOMATO,
    AGRI_CROP_COFFEE,
    AGRI_CROP_CUSTOM,
} agri_crop_type_t;

/* Crop growth stage */
typedef enum {
    AGRI_STAGE_DORMANT = 0,
    AGRI_STAGE_GERMINATION,
    AGRI_STAGE_VEGETATIVE,
    AGRI_STAGE_FLOWERING,
    AGRI_STAGE_GRAIN_FILL,
    AGRI_STAGE_MATURITY,
    AGRI_STAGE_HARVEST,
} agri_growth_stage_t;

/* Crop definition */
typedef struct {
    agri_crop_type_t type;
    char name[AGRI_MAX_NAME];
    float base_yield_ton_ha;
    int   days_to_maturity;
    float kc_initial;
    float kc_mid;
    float kc_end;
    float root_depth_max_cm;
    float optimal_temp_min_c;
    float optimal_temp_max_c;
    float n_uptake_kg_ha;
    float p_uptake_kg_ha;
    float k_uptake_kg_ha;
    float harvest_index;
} agri_crop_t;

/* Weather data for one day */
typedef struct {
    int   year, month, day;
    float temp_max_c;
    float temp_min_c;
    float temp_avg_c;
    float humidity_pct;
    float rainfall_mm;
    float solar_radiation_MJ_m2;
    float wind_speed_m_s;
    float et0_mm;
} agri_weather_day_t;

/* Weather time series */
typedef struct {
    agri_weather_day_t days[AGRI_MAX_WEATHER_DAYS];
    int num_days;
    int start_year;
    int start_doy;
} agri_weather_series_t;

/* Growing season configuration */
typedef struct {
    agri_crop_type_t crop_type;
    int planting_doy;
    int harvest_doy;
    int year;
    float planting_density_plants_ha;
    float row_spacing_cm;
    float irrigation_efficiency;
    bool use_fertilizer_optimizer;
    float target_yield_ton_ha;
} agri_season_config_t;

/* Daily simulation state */
typedef struct {
    int    doy;
    agri_growth_stage_t stage;
    float  lai;
    float  root_depth_cm;
    float  biomass_accum_kg_ha;
    float  yield_accum_kg_ha;
    float  soil_water_mm;
    float  water_stress_factor;
    float  nitrogen_stress_factor;
    float  temperature_stress_factor;
    float  etc_actual_mm;
    float  biomass_growth_kg_ha_day;
} agri_daily_state_t;

/* Full season context */
typedef struct {
    agri_season_config_t config;
    agri_soil_profile_t soil;
    agri_weather_series_t weather;
    agri_field_t field;
    agri_daily_state_t current;
    agri_daily_state_t history[AGRI_MAX_WEATHER_DAYS];
    int    days_simulated;
    float  total_irrigation_mm;
    float  total_fertilizer_n_kg_ha;
    float  total_fertilizer_p_kg_ha;
    float  total_fertilizer_k_kg_ha;
    float  predicted_yield_ton_ha;
    float  water_use_efficiency_kg_m3;
    float  nitrogen_use_efficiency;
    bool   complete;
} agri_season_context_t;

/* Core API */
void agri_init(void);
agri_gps_t agri_make_gps(double lat, double lon, double elev);
double agri_gps_distance_m(const agri_gps_t* a, const agri_gps_t* b);
double agri_polygon_area_ha(const agri_gps_t* vertices, int n);

void agri_soil_init(agri_soil_profile_t* soil, const char* texture_class);
void agri_soil_add_layer(agri_soil_profile_t* soil, float depth_cm,
                          float clay_pct, float silt_pct, float sand_pct,
                          float organic_matter_pct, float ph);
float agri_soil_awc_mm(const agri_soil_profile_t* soil, float root_depth_cm);
float agri_soil_water_content_at_fc(const agri_soil_profile_t* soil,
                                     float root_depth_cm);
float agri_soil_water_content_at_wp(const agri_soil_profile_t* soil,
                                     float root_depth_cm);

void agri_weather_init(agri_weather_series_t* ws);
void agri_weather_add_day(agri_weather_series_t* ws, int year, int month,
                           int day, float tmax, float tmin, float rain,
                           float solar, float humidity, float wind);
float agri_weather_et0_penman_monteith(const agri_weather_day_t* w,
                                        float elevation_m, float latitude);
float agri_weather_gdd_accum(const agri_weather_series_t* ws,
                              int start_doy, int end_doy,
                              float tbase, float tupper);
int agri_doy_from_date(int year, int month, int day);
void agri_date_from_doy(int year, int doy, int* month, int* day);

void agri_season_init(agri_season_context_t* ctx,
                       const agri_season_config_t* cfg);
void agri_season_free(agri_season_context_t* ctx);
void agri_season_simulate(agri_season_context_t* ctx);
void agri_season_step(agri_season_context_t* ctx);
float agri_predict_yield(const agri_season_context_t* ctx);

/* ── L5: Irrigation Scheduling Optimizer ── */
#define AGRI_IRR_MAX_SCHEDULE 366

typedef struct {
    int    doy;
    float  soil_moisture_deficit_mm;
    float  recommended_irrigation_mm;
    float  etc_since_last_mm;
    float  rainfall_since_last_mm;
} agri_irrigation_event_t;

typedef struct {
    agri_irrigation_event_t events[AGRI_IRR_MAX_SCHEDULE];
    int    num_events;
    float  total_water_applied_mm;
    float  water_saved_vs_full_mm;
    float  yield_impact_pct;
} agri_irrigation_schedule_t;

void agri_irrigation_optimize(const agri_season_context_t* season,
                               agri_irrigation_schedule_t* schedule,
                               float mad_fraction);
float agri_irrigation_deficit_irrigation(const agri_season_context_t* ctx,
                                          float target_reduction_pct);
float agri_irrigation_water_productivity(const agri_season_context_t* ctx);

/* ── L5: Pest & Disease Risk Modeling ── */
#define AGRI_PEST_MAX_MODELS 32
#define AGRI_PEST_MAX_EVENTS 128

typedef enum {
    AGRI_PEST_INSECT = 0,
    AGRI_PEST_FUNGAL,
    AGRI_PEST_BACTERIAL,
    AGRI_PEST_VIRAL,
    AGRI_PEST_WEED,
} agri_pest_type_t;

typedef struct {
    agri_pest_type_t type;
    char   name[AGRI_MAX_NAME];
    float  temp_optimal_min_c;
    float  temp_optimal_max_c;
    float  humidity_threshold_pct;
    float  leaf_wetness_hours_threshold;
    int    degree_day_accumulation;
    float  base_temp_c;
    float  sporulation_rate;
} agri_pest_model_t;

typedef struct {
    int    doy;
    float  risk_index;        /* 0.0 - 1.0 */
    float  severity_estimate;
    float  yield_loss_pct;
    char   recommendation[AGRI_MAX_NAME];
} agri_pest_risk_event_t;

typedef struct {
    agri_pest_model_t     models[AGRI_PEST_MAX_MODELS];
    int                   num_models;
    agri_pest_risk_event_t risks[AGRI_PEST_MAX_EVENTS];
    int                   num_risks;
    agri_crop_type_t      target_crop;
} agri_pest_predictor_t;

void agri_pest_init(agri_pest_predictor_t* pp, agri_crop_type_t crop);
int  agri_pest_add_model(agri_pest_predictor_t* pp, agri_pest_type_t type,
                          const char* name, float tmin, float tmax,
                          float hum_thresh, float base_temp);
void agri_pest_predict(const agri_pest_predictor_t* pp,
                       const agri_weather_series_t* weather,
                       agri_pest_risk_event_t* risks, int* num_risks);
float agri_pest_degree_day_risk(const agri_pest_model_t* model,
                                 const agri_weather_series_t* weather,
                                 int start_doy, int end_doy);

/* ── L6: Multi-Field Farm Management ── */
#define AGRI_MAX_FIELDS_PER_FARM 64

typedef struct {
    agri_field_t          fields[AGRI_MAX_FIELDS_PER_FARM];
    int                   num_fields;
    agri_season_context_t seasons[AGRI_MAX_FIELDS_PER_FARM];
    double                total_area_ha;
    double                total_predicted_yield_ton;
    double                total_water_use_m3;
    double                total_nitrogen_kg;
    char                  farm_name[AGRI_MAX_NAME];
} agri_farm_t;

void agri_farm_init(agri_farm_t* farm, const char* name);
int  agri_farm_add_field(agri_farm_t* farm, const agri_field_t* field,
                          const agri_season_config_t* config,
                          const agri_soil_profile_t* soil,
                          const agri_weather_series_t* weather);
void agri_farm_simulate_all(agri_farm_t* farm);
void agri_farm_optimize_planting(agri_farm_t* farm,
                                   const agri_crop_t* crops, int num_crops);
double agri_farm_profit_estimate(const agri_farm_t* farm,
                                  double price_per_ton, double cost_per_ha);
void agri_farm_summary(const agri_farm_t* farm);

/* ── L4: Soil Erosion Modeling (USLE) ── */
typedef struct {
    float rainfall_erosivity_R;
    float soil_erodibility_K;
    float slope_length_steepness_LS;
    float cover_management_C;
    float support_practice_P;
} agri_usle_factors_t;

float agri_usle_soil_loss_ton_ha_yr(const agri_usle_factors_t* factors);
float agri_usle_compute_R(const agri_weather_series_t* weather);
float agri_usle_compute_K(const agri_soil_profile_t* soil);
float agri_usle_compute_LS(float slope_length_m, float slope_pct);
float agri_usle_compute_C(agri_crop_type_t crop, agri_growth_stage_t stage);

/* ── L7: Precision Fertilizer Application ── */
typedef struct {
    float nitrogen_kg_ha;
    float phosphorus_kg_ha;
    float potassium_kg_ha;
    float sulfur_kg_ha;
    float zinc_kg_ha;
    float boron_kg_ha;
} agri_fertilizer_plan_t;

typedef struct {
    float soil_n_kg_ha;
    float soil_p_kg_ha;
    float soil_k_kg_ha;
    float yield_goal_ton_ha;
    float previous_crop_n_credit;
    float organic_matter_n_release;
} agri_soil_test_t;

void agri_fertilizer_plan(const agri_soil_test_t* test,
                           agri_crop_type_t crop,
                           agri_fertilizer_plan_t* plan);
float agri_fertilizer_nitrogen_use_efficiency(
    const agri_fertilizer_plan_t* applied,
    const agri_season_context_t* result);
float agri_fertilizer_variable_rate(float base_rate, float yield_potential,
                                     float soil_organic_matter_pct);

/* ── L8: Crop Rotation Optimization ── */
#define AGRI_MAX_ROTATION_LENGTH 12

typedef struct {
    agri_crop_type_t sequence[AGRI_MAX_ROTATION_LENGTH];
    int    length;
    double soil_health_score;
    double pest_pressure_reduction;
    double nitrogen_credit_kg_ha;
    double profit_rotation_avg;
} agri_rotation_plan_t;

void agri_rotation_optimize(const agri_crop_t* available_crops, int num_crops,
                             const agri_soil_profile_t* soil,
                             agri_rotation_plan_t* plans, int* num_plans);
float agri_rotation_nitrogen_benefit(agri_crop_type_t prev, agri_crop_type_t next);
float agri_rotation_break_crop_effect(agri_crop_type_t crop);

/* ── L9: Carbon Farming ── */
typedef struct {
    float soil_organic_carbon_ton_ha;
    float carbon_sequestration_rate_ton_ha_yr;
    float carbon_credit_value_usd;
    float methane_emissions_kg_co2e_ha;
    float nitrous_oxide_emissions_kg_co2e_ha;
    float total_carbon_footprint_kg_co2e;
    int   carbon_credits_earned;
} agri_carbon_farm_t;

void agri_carbon_farm_assess(const agri_farm_t* farm,
                              agri_carbon_farm_t* carbon);
float agri_carbon_sequestration_potential(const agri_soil_profile_t* soil,
                                           agri_crop_type_t crop,
                                           bool cover_crop, bool no_till);
float agri_carbon_methane_from_rice(float area_ha, float flooding_days,
                                      float organic_amendment_ton_ha);
float agri_carbon_n2o_from_fertilizer(float n_applied_kg_ha,
                                        float emission_factor);

/* ── L7: Stochastic Weather Generator (WGEN) ── */
/* Richardson (1981) WGEN for climate risk analysis.
 * Generates synthetic daily weather from monthly statistics.
 * Reference: Richardson & Wright (1984) WGEN model */
typedef struct {
    float monthly_tmax_mean[12];
    float monthly_tmin_mean[12];
    float monthly_rain_mean[12];
    float monthly_rain_std[12];
    float monthly_rain_prob_wet[12];
    float monthly_solar_mean[12];
    float temp_lag_autocorr;
} agri_wgen_params_t;

void agri_wgen_init(agri_wgen_params_t* params);
int  agri_wgen_generate(const agri_wgen_params_t* params,
                          int year, int num_days,
                          agri_weather_series_t* output,
                          uint32_t seed);
void agri_wgen_estimate_from_weather(const agri_weather_series_t* observed,
                                      agri_wgen_params_t* params);

/* ── L7: Nitrogen Cycle Modeling ── */
/* Soil nitrogen dynamics: mineralization, immobilization,
 * nitrification, denitrification, volatilization, leaching.
 * Reference: Stanford & Smith (1972), Godwin & Jones (1991) CERES-N */
typedef struct {
    float no3_n_kg_ha;      /* Soil nitrate-N */
    float nh4_n_kg_ha;      /* Soil ammonium-N */
    float organic_n_kg_ha;  /* Organic N pool */
    float mineralized_kg_ha_day;
    float denitrified_kg_ha_day;
    float leached_kg_ha_day;
    float volatilized_kg_ha_day;
    float nitrified_kg_ha_day;
    float immobilized_kg_ha_day;
    float plant_uptake_kg_ha_day;
} agri_nitrogen_pool_t;

void  agri_nitrogen_init(agri_nitrogen_pool_t* pool,
                          float soil_organic_n, float no3, float nh4);
void  agri_nitrogen_step(agri_nitrogen_pool_t* pool,
                          float soil_temp_c, float soil_water_content,
                          float soil_water_fc, float plant_demand_kg_ha,
                          float organic_matter_pct, float rainfall_mm);
float agri_nitrogen_mineralization(float organic_n_kg_ha, float temp_c,
                                     float water_filled_porosity,
                                     float organic_matter_pct);
float agri_nitrogen_denitrification(float no3_kg_ha, float water_filled_porosity,
                                      float temp_c, float organic_c_pct);
float agri_nitrogen_leaching(float no3_kg_ha, float drainage_mm,
                               float soil_depth_cm, float field_capacity);

/* ── L7: Economic Decision Support ── */
typedef struct {
    float seed_cost_per_ha;
    float fertilizer_cost_per_ha;
    float pesticide_cost_per_ha;
    float fuel_cost_per_ha;
    float labor_cost_per_ha;
    float irrigation_cost_per_mm_ha;
    float land_rent_per_ha;
    float insurance_per_ha;
    float misc_cost_per_ha;
} agri_cost_breakdown_t;

typedef struct {
    float expected_revenue_per_ha;
    float variable_cost_per_ha;
    float fixed_cost_per_ha;
    float gross_margin_per_ha;
    float net_profit_per_ha;
    float break_even_yield_ton_ha;
    float break_even_price_per_ton;
    float return_on_investment_pct;
} agri_economic_analysis_t;

void agri_economic_analyze(const agri_season_context_t* season,
                            const agri_cost_breakdown_t* costs,
                            float expected_price_per_ton,
                            agri_economic_analysis_t* analysis);
void agri_economic_sensitivity(const agri_season_context_t* season,
                                const agri_cost_breakdown_t* costs,
                                float price_range[5], float yield_range[5],
                                float* profit_matrix);
float agri_economic_breakeven_price(const agri_cost_breakdown_t* costs,
                                      float yield_ton_ha);

/* ── L6: Harvest & Post-Harvest Loss Modeling ── */
typedef enum {
    AGRI_HARVEST_MANUAL,
    AGRI_HARVEST_MECHANICAL,
    AGRI_HARVEST_COMBINE,
} agri_harvest_method_t;

typedef struct {
    float field_loss_pct;
    float transport_loss_pct;
    float storage_loss_pct;
    float processing_loss_pct;
    float total_loss_pct;
    float marketable_yield_ton_ha;
    int   days_to_harvest;
    float optimal_harvest_moisture_pct;
} agri_harvest_model_t;

void agri_harvest_model(const agri_season_context_t* season,
                          agri_harvest_method_t method,
                          agri_harvest_model_t* model);
float agri_harvest_dry_down(agri_crop_type_t crop, float initial_moisture,
                              float gdd_per_day, int days);
float agri_grain_storage_loss(float initial_quantity_ton,
                                float moisture_pct, float temp_c,
                                int storage_days);

const agri_crop_t* agri_crop_lookup(agri_crop_type_t type);

/* ── L8: Climate Change Impact Assessment ── */
/* Downscaled climate projections for agriculture adaptation.
 * Reference: IPCC AR6 WG2 Chapter 5 (Food, Fibre, Ecosystem Products) */
typedef struct {
    float temp_increase_c;
    float precip_change_pct;
    float co2_ppm;
    int   projection_year;
    float extreme_heat_days_yr;
    float drought_frequency_pct;
    float flood_risk_pct;
} agri_climate_scenario_t;

typedef struct {
    float baseline_yield_ton_ha;
    float projected_yield_ton_ha;
    float yield_change_pct;
    float water_demand_change_pct;
    float growing_season_shift_days;
    float pest_pressure_change_pct;
    float heat_stress_days_added;
    float adaptation_potential_pct;
} agri_climate_impact_t;

void agri_climate_impact_assess(const agri_season_context_t* baseline,
                                 const agri_climate_scenario_t* scenario,
                                 agri_climate_impact_t* impact);
float agri_climate_co2_fertilization_effect(float co2_ppm, agri_crop_type_t crop);
float agri_climate_yield_response(float temp_change, float precip_change,
                                    float co2_effect, agri_crop_type_t crop);

/* ── L7: Crop Insurance Modeling ── */
/* US RMA (Risk Management Agency) style revenue protection.
 * Reference: Goodwin & Smith (2014) "Agricultural Insurance" */
typedef enum {
    AGRI_INSURANCE_NONE,
    AGRI_INSURANCE_YIELD_PROTECTION,
    AGRI_INSURANCE_REVENUE_PROTECTION,
    AGRI_INSURANCE_AREA_PLAN,
} agri_insurance_type_t;

typedef struct {
    agri_insurance_type_t type;
    float coverage_level;         /* 0.50 - 0.85 */
    float guaranteed_yield_ton_ha;
    float guaranteed_revenue_per_ha;
    float projected_price_per_ton;
    float premium_per_ha;
    float indemnity_payment;
    float loss_ratio;
} agri_insurance_policy_t;

void agri_insurance_quote(agri_insurance_policy_t* policy,
                           const agri_season_context_t* season,
                           agri_insurance_type_t type, float coverage,
                           float projected_price);
float agri_insurance_calculate_indemnity(const agri_insurance_policy_t* policy,
                                          float actual_yield, float harvest_price);
float agri_insurance_actuarial_premium(float expected_yield,
                                         float yield_std,
                                         float coverage_level,
                                         float price_per_ton);

/* ── L4: Water Quality / Nutrient Runoff ── */
/* Edge-of-field nutrient loss estimation.
 * Reference: USDA NRCS Nutrient Tracking Tool (NTT) */
typedef struct {
    float surface_runoff_mm;
    float sediment_loss_ton_ha;
    float particulate_n_loss_kg_ha;
    float particulate_p_loss_kg_ha;
    float dissolved_n_loss_kg_ha;
    float dissolved_p_loss_kg_ha;
    float tile_drainage_mm;
    float total_n_loss_kg_ha;
    float total_p_loss_kg_ha;
} agri_water_quality_t;

void agri_water_quality_estimate(const agri_season_context_t* season,
                                  const agri_fertilizer_plan_t* fertilizer,
                                  float slope_pct, float tile_drainage,
                                  agri_water_quality_t* wq);
float agri_runoff_curve_number(agri_crop_type_t crop, const agri_soil_profile_t* soil);
float agri_sediment_delivery_ratio(float area_ha, float slope_pct);

/* ── L3: Tillage & Soil Management ── */
typedef enum {
    AGRI_TILLAGE_CONVENTIONAL,
    AGRI_TILLAGE_REDUCED,
    AGRI_TILLAGE_NO_TILL,
    AGRI_TILLAGE_STRIP_TILL,
    AGRI_TILLAGE_RIDGE_TILL,
} agri_tillage_type_t;

typedef struct {
    agri_tillage_type_t type;
    float surface_residue_pct;
    float soil_disturbance_pct;
    float fuel_use_l_ha;
    float soil_organic_matter_impact;
    float erosion_reduction_pct;
    float yield_impact_pct;
    int   operations_per_year;
} agri_tillage_system_t;

void agri_tillage_evaluate(agri_tillage_type_t type,
                            const agri_soil_profile_t* soil,
                            agri_tillage_system_t* system);
float agri_tillage_residue_decay(float initial_residue_kg_ha,
                                   float temp_c, float moisture, int days);
float agri_tillage_carbon_impact(agri_tillage_type_t from, agri_tillage_type_t to,
                                   float soil_organic_carbon, int years);

/* ── L9: Precision Agriculture — Variable Rate Technology (VRT) ── */
/* Site-specific crop management using GPS-guided variable rate
 * application of seeds, fertilizer, and pesticides.
 * Reference: Gebbers & Adamchuk (2010) "Precision agriculture and food security" */
typedef struct {
    agri_gps_t   center;
    float        cell_size_m;
    int          rows, cols;
    float*       yield_data;         /* rows*cols float array */
    float*       soil_ph_data;
    float*       organic_matter_data;
    float*       elevation_data;
} agri_yield_map_t;

typedef struct {
    float seed_rate_kg_ha;
    float n_rate_kg_ha;
    float p_rate_kg_ha;
    float k_rate_kg_ha;
    float lime_rate_kg_ha;
} agri_vrt_prescription_t;

void  agri_yield_map_init(agri_yield_map_t* map, int rows, int cols, float cell_size);
void  agri_yield_map_free(agri_yield_map_t* map);
float agri_yield_map_get(const agri_yield_map_t* map, int row, int col);
void  agri_yield_map_set(agri_yield_map_t* map, int row, int col, float value);
float agri_yield_map_average(const agri_yield_map_t* map);
float agri_yield_map_cv(const agri_yield_map_t* map);     /* Coefficient of variation */
int   agri_yield_map_zones(const agri_yield_map_t* map, int num_zones, int* zone_map);
void  agri_vrt_generate_prescription(const agri_yield_map_t* map,
                                      const agri_fertilizer_plan_t* base_plan,
                                      agri_vrt_prescription_t* rx,
                                      int row, int col);

/* ── L8: Food Quality & Mycotoxin Risk ── */
/* Aflatoxin and DON (deoxynivalenol) risk assessment for grain crops.
 * Reference: EFSA (2020) "Risk assessment of aflatoxins in food"
 *            FDA Compliance Program 7307.001 */
typedef struct {
    float aflatoxin_risk_pct;
    float don_risk_pct;
    float fumonisin_risk_pct;
    float grain_protein_pct;
    float test_weight_kg_hl;
    int   falling_number;
    bool  meets_grade1;
    bool  meets_grade2;
    char  quality_grade[32];
} agri_grain_quality_t;

void agri_grain_quality_assess(const agri_season_context_t* season,
                                agri_grain_quality_t* quality);
float agri_mycotoxin_aflatoxin_risk(float temp_c, float humidity_pct,
                                      float kernel_damage_pct, agri_crop_type_t crop);
float agri_mycotoxin_don_risk(float temp_c, float rainfall_mm,
                                agri_crop_type_t crop, float days_post_anthesis);

/* ── L6: Intercropping & Relay Planting ── */
typedef struct {
    agri_crop_type_t primary_crop;
    agri_crop_type_t companion_crop;
    float primary_planting_delay_days;
    float companion_planting_delay_days;
    float land_equivalent_ratio;
    float nitrogen_benefit_kg_ha;
    float pest_suppression_pct;
    float weed_suppression_pct;
    float yield_advantage_pct;
} agri_intercrop_system_t;

void agri_intercrop_evaluate(agri_crop_type_t primary, agri_crop_type_t companion,
                              agri_intercrop_system_t* system);
float agri_land_equivalent_ratio(agri_crop_type_t primary, float primary_yield,
                                   float primary_solo_yield,
                                   agri_crop_type_t companion,
                                   float companion_yield, float companion_solo_yield);

#ifdef __cplusplus
}
#endif

#endif
