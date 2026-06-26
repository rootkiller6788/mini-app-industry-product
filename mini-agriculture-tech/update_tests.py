import re

# Read existing test file
with open('tests/test_agri.c', 'r') as f:
    existing = f.read()

# The new test functions to append
new_tests = """
/* ── L5: Irrigation Scheduling ── */
static int test_irrigation_scheduling(void) {
    TEST("irrigation_scheduling");
    agri_season_context_t ctx;
    agri_season_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.crop_type = AGRI_CROP_MAIZE; cfg.planting_doy = 91;
    cfg.irrigation_efficiency = 0.75f;
    agri_season_init(&ctx, &cfg);
    agri_soil_init(&ctx.soil, "loam");
    agri_soil_add_layer(&ctx.soil, 40.0f, 20.0f, 40.0f, 40.0f, 2.5f, 6.5f);
    ctx.field.vertices[0] = agri_make_gps(39.0, -98.0, 400.0);
    agri_weather_init(&ctx.weather);
    for (int d = 0; d < 100; d++)
        agri_weather_add_day(&ctx.weather, 2025, 5, 1+d, 25.0f, 15.0f, (d%5==0)?5.0f:0.0f, 22.0f, 50.0f, 2.0f);
    agri_season_simulate(&ctx);
    agri_irrigation_schedule_t sched;
    agri_irrigation_optimize(&ctx, &sched, 0.50f);
    CHECK(sched.num_events >= 0, "schedule generated");
    float rel = agri_irrigation_deficit_irrigation(&ctx, 20.0f);
    CHECK(rel >= 0.0f && rel <= 100.0f, "rel yield 0-100%%");
    float wp = agri_irrigation_water_productivity(&ctx);
    CHECK(wp >= 0.0f, "water productivity nonneg");
    agri_irrigation_optimize(NULL, &sched, 0.5f);
    agri_irrigation_optimize(&ctx, NULL, 0.5f);
    PASS(); return 0;
}

static int test_pest_risk(void) {
    TEST("pest_risk");
    agri_pest_predictor_t pp;
    agri_pest_init(&pp, AGRI_CROP_MAIZE);
    agri_pest_add_model(&pp, AGRI_PEST_FUNGAL, "Gray Leaf Spot", 22.0f, 30.0f, 70.0f, 10.0f);
    agri_pest_add_model(&pp, AGRI_PEST_INSECT, "Corn Borer", 16.0f, 30.0f, 50.0f, 10.0f);
    agri_weather_series_t ws;
    agri_weather_init(&ws); ws.start_doy = 150;
    for (int d = 0; d < 60; d++)
        agri_weather_add_day(&ws, 2025, 6, 1+d, 26.0f, 16.0f, (d%4==0)?8.0f:0.0f, 20.0f, 70.0f, 2.0f);
    agri_pest_risk_event_t risks[32]; int n = 0;
    agri_pest_predict(&pp, &ws, risks, &n);
    CHECK(n > 0, "should predict risks");
    float dd = agri_pest_degree_day_risk(&pp.models[0], &ws, 150, 180);
    CHECK(dd >= 0.0f && dd <= 1.0f, "DD risk [0,1]");
    agri_pest_init(NULL, AGRI_CROP_WHEAT);
    agri_pest_predict(NULL, &ws, risks, &n);
    PASS(); return 0;
}

static int test_farm_management(void) {
    TEST("farm_management");
    agri_farm_t farm;
    agri_farm_init(&farm, "Test Farm");
    agri_field_t field; memset(&field, 0, sizeof(field));
    field.vertices[0] = agri_make_gps(40.0, -100.0, 0);
    field.vertices[1] = agri_make_gps(40.001, -100.0, 0);
    field.vertices[2] = agri_make_gps(40.001, -99.999, 0);
    field.vertices[3] = agri_make_gps(40.0, -99.999, 0);
    field.num_vertices = 4;
    field.area_hectares = agri_polygon_area_ha(field.vertices, 4);
    agri_soil_profile_t soil; agri_soil_init(&soil, "loam");
    agri_soil_add_layer(&soil, 30.0f, 20.0f, 40.0f, 40.0f, 2.5f, 6.5f);
    agri_weather_series_t weather; agri_weather_init(&weather); weather.start_doy = 91;
    for (int d = 0; d < 100; d++)
        agri_weather_add_day(&weather, 2025, 5, 1+d, 25.0f, 15.0f, 2.0f, 20.0f, 55.0f, 2.0f);
    agri_season_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.crop_type = AGRI_CROP_WHEAT; cfg.planting_doy = 91; cfg.irrigation_efficiency = 0.75f;
    agri_farm_add_field(&farm, &field, &cfg, &soil, &weather);
    agri_farm_simulate_all(&farm);
    CHECK(farm.total_predicted_yield_ton > 0.0, "farm yield positive");
    agri_crop_t crops[3];
    const agri_crop_t *c1 = agri_crop_lookup(AGRI_CROP_MAIZE);
    const agri_crop_t *c2 = agri_crop_lookup(AGRI_CROP_SOYBEAN);
    const agri_crop_t *c3 = agri_crop_lookup(AGRI_CROP_WHEAT);
    if (c1) crops[0] = *c1;
    if (c2) crops[1] = *c2;
    if (c3) crops[2] = *c3;
    agri_farm_optimize_planting(&farm, crops, 3);
    agri_farm_profit_estimate(&farm, 250.0, 500.0);
    agri_farm_init(NULL, NULL);
    CHECK(agri_farm_add_field(NULL, NULL, NULL, NULL, NULL) == -1, "null add returns -1");
    PASS(); return 0;
}

static int test_usle_erosion(void) {
    TEST("usle_erosion");
    agri_usle_factors_t f = {100.0f, 0.03f, 1.5f, 0.2f, 0.5f};
    float A = agri_usle_soil_loss_ton_ha_yr(&f);
    CHECK(A > 0.0f && A < 100.0f, "erosion valid");
    agri_weather_series_t ws; agri_weather_init(&ws);
    for (int d = 0; d < 30; d++)
        agri_weather_add_day(&ws, 2025, 6, 1+d, 25.0f, 15.0f, (d%3==0)?15.0f:0.0f, 20.0f, 55.0f, 2.0f);
    CHECK(agri_usle_compute_R(&ws) > 0.0f, "R positive");
    agri_soil_profile_t soil; agri_soil_init(&soil, "silt_loam");
    agri_soil_add_layer(&soil, 30.0f, 18.0f, 55.0f, 27.0f, 2.5f, 6.5f);
    float K = agri_usle_compute_K(&soil);
    CHECK(K > 0.0f, "K positive");
    CHECK(agri_usle_compute_LS(100.0f, 3.0f) > 0.0f, "LS positive");
    CHECK(agri_usle_compute_C(AGRI_CROP_MAIZE, AGRI_STAGE_VEGETATIVE) > 0.0f, "C positive");
    CHECK(agri_usle_soil_loss_ton_ha_yr(NULL) == 0.0f, "null factors returns 0");
    PASS(); return 0;
}

static int test_fertilizer_plan(void) {
    TEST("fertilizer_plan");
    agri_soil_test_t st = {45.0f, 25.0f, 180.0f, 10.0f, 20.0f, 15.0f};
    agri_fertilizer_plan_t plan;
    agri_fertilizer_plan(&st, AGRI_CROP_MAIZE, &plan);
    CHECK(plan.nitrogen_kg_ha + plan.phosphorus_kg_ha + plan.potassium_kg_ha > 0.0f,
          "fertilizer planned");
    float vr = agri_fertilizer_variable_rate(100.0f, 10.0f, 4.0f);
    float vr2 = agri_fertilizer_variable_rate(100.0f, 5.0f, 1.0f);
    CHECK(vr > vr2, "higher yield = higher VRT rate");
    CHECK(agri_fertilizer_nitrogen_use_efficiency(NULL, NULL) == 0.0f, "null NUE returns 0");
    PASS(); return 0;
}

static int test_economic_analysis(void) {
    TEST("economic_analysis");
    agri_season_context_t ctx; agri_season_config_t cfg;
    memset(&cfg, 0, sizeof(cfg)); cfg.crop_type = AGRI_CROP_MAIZE;
    agri_season_init(&ctx, &cfg);
    ctx.predicted_yield_ton_ha = 10.0f; ctx.total_irrigation_mm = 300.0f;
    agri_cost_breakdown_t c = {180.0f, 350.0f, 120.0f, 90.0f, 200.0f, 0.25f,
                                600.0f, 75.0f, 50.0f};
    agri_economic_analysis_t ea;
    agri_economic_analyze(&ctx, &c, 250.0f, &ea);
    CHECK(ea.expected_revenue_per_ha > 0.0f, "revenue positive");
    CHECK(ea.break_even_yield_ton_ha > 0.0f, "BE yield positive");
    float pr[5] = {200, 225, 250, 275, 300}, yr[5] = {6, 8, 10, 12, 14}, mx[25];
    agri_economic_sensitivity(&ctx, &c, pr, yr, mx);
    CHECK(agri_economic_breakeven_price(&c, 10.0f) > 0.0f, "BE price positive");
    PASS(); return 0;
}

static int test_harvest_model(void) {
    TEST("harvest_model");
    agri_season_context_t ctx; agri_season_config_t cfg;
    memset(&cfg, 0, sizeof(cfg)); cfg.crop_type = AGRI_CROP_MAIZE;
    agri_season_init(&ctx, &cfg);
    ctx.predicted_yield_ton_ha = 10.0f; ctx.current.stage = AGRI_STAGE_MATURITY;
    agri_harvest_model_t hm;
    agri_harvest_model(&ctx, AGRI_HARVEST_COMBINE, &hm);
    CHECK(hm.total_loss_pct > 0.0f, "harvest loss positive");
    CHECK(hm.marketable_yield_ton_ha < 10.0f, "marketable < predicted");
    float m = agri_harvest_dry_down(AGRI_CROP_MAIZE, 28.0f, 12.0f, 15);
    CHECK(m < 28.0f && m >= 12.0f, "dry down works");
    float l1 = agri_grain_storage_loss(100.0f, 16.0f, 25.0f, 60);
    float l2 = agri_grain_storage_loss(100.0f, 12.0f, 10.0f, 60);
    CHECK(l1 > l2, "high moisture/temp = more loss");
    PASS(); return 0;
}

static int test_rotation(void) {
    TEST("crop_rotation");
    float nb = agri_rotation_nitrogen_benefit(AGRI_CROP_SOYBEAN, AGRI_CROP_MAIZE);
    CHECK(nb > 0.0f, "soybean->maize N benefit");
    CHECK(agri_rotation_nitrogen_benefit(AGRI_CROP_MAIZE, AGRI_CROP_MAIZE) == 0.0f,
          "maize->maize = 0");
    CHECK(agri_rotation_break_crop_effect(AGRI_CROP_SOYBEAN) > 0.0f, "break crop effect");
    agri_crop_t crops[4]; const agri_crop_t *cp;
    cp = agri_crop_lookup(AGRI_CROP_MAIZE); if (cp) crops[0] = *cp;
    cp = agri_crop_lookup(AGRI_CROP_SOYBEAN); if (cp) crops[1] = *cp;
    cp = agri_crop_lookup(AGRI_CROP_WHEAT); if (cp) crops[2] = *cp;
    cp = agri_crop_lookup(AGRI_CROP_POTATO); if (cp) crops[3] = *cp;
    agri_soil_profile_t soil; agri_soil_init(&soil, "loam");
    agri_soil_add_layer(&soil, 30.0f, 20.0f, 40.0f, 40.0f, 2.5f, 6.5f);
    agri_rotation_plan_t plans[10]; int np = 0;
    agri_rotation_optimize(crops, 4, &soil, plans, &np);
    CHECK(np > 0, "rotation plans generated");
    CHECK(plans[0].length >= 2, ">=2 crops");
    PASS(); return 0;
}

static int test_carbon_farming(void) {
    TEST("carbon_farming");
    agri_soil_profile_t soil; agri_soil_init(&soil, "loam");
    agri_soil_add_layer(&soil, 30.0f, 20.0f, 40.0f, 40.0f, 3.0f, 6.5f);
    soil.soil_organic_carbon_ton_ha = 45.0f;
    float s1 = agri_carbon_sequestration_potential(&soil, AGRI_CROP_MAIZE, true, true);
    float s2 = agri_carbon_sequestration_potential(&soil, AGRI_CROP_MAIZE, false, false);
    CHECK(s1 > s2, "no-till+cover > conventional");
    CHECK(agri_carbon_methane_from_rice(10.0f, 120.0f, 0.0f) > 0.0f, "rice CH4");
    CHECK(agri_carbon_n2o_from_fertilizer(200.0f, 0.01f) > 0.0f, "N2O from N");
    CHECK(agri_carbon_sequestration_potential(NULL, AGRI_CROP_MAIZE, false, false) == 0.0f,
          "null soil returns 0");
    PASS(); return 0;
}

static int test_wgen(void) {
    TEST("wgen");
    agri_wgen_params_t p; agri_wgen_init(&p);
    CHECK(p.temp_lag_autocorr > 0.0f, "autocorr set");
    agri_weather_series_t ws;
    int nd = agri_wgen_generate(&p, 2025, 365, &ws, 12345u);
    CHECK(nd > 0, "WGEN generates days");
    agri_wgen_params_t e; agri_wgen_estimate_from_weather(&ws, &e);
    CHECK(e.monthly_tmax_mean[6] > 0.0f, "July tmax positive");
    agri_wgen_generate(NULL, 2025, 0, NULL, 0);
    agri_wgen_estimate_from_weather(NULL, NULL);
    PASS(); return 0;
}

static int test_nitrogen_cycle(void) {
    TEST("nitrogen_cycle");
    agri_nitrogen_pool_t np; agri_nitrogen_init(&np, 5000.0f, 30.0f, 10.0f);
    CHECK(np.no3_n_kg_ha > 0.0f, "NO3 init");
    agri_nitrogen_step(&np, 25.0f, 0.30f, 0.35f, 3.0f, 2.5f, 5.0f);
    CHECK(np.mineralized_kg_ha_day > 0.0f, "mineralization at 25C");
    float mr = agri_nitrogen_mineralization(5000.0f, 25.0f, 65.0f, 3.0f);
    CHECK(mr > 0.0f, "min rate positive");
    CHECK(agri_nitrogen_denitrification(30.0f, 75.0f, 25.0f, 2.0f) > 0.0f, "denitrification");
    CHECK(agri_nitrogen_leaching(30.0f, 20.0f, 60.0f, 0.35f) > 0.0f, "leaching");
    agri_nitrogen_pool_t np2; agri_nitrogen_init(&np2, 5000.0f, 30.0f, 10.0f);
    agri_nitrogen_step(&np2, 2.0f, 0.30f, 0.35f, 3.0f, 2.5f, 0.0f);
    CHECK(np2.mineralized_kg_ha_day < np.mineralized_kg_ha_day, "cold = less min");
    agri_nitrogen_init(NULL, 0, 0, 0);
    agri_nitrogen_step(NULL, 0, 0, 0, 0, 0, 0);
    PASS(); return 0;
}

static int test_climate_impact(void) {
    TEST("climate_impact");
    float ce = agri_climate_co2_fertilization_effect(550.0f, AGRI_CROP_WHEAT);
    float ce2 = agri_climate_co2_fertilization_effect(550.0f, AGRI_CROP_MAIZE);
    CHECK(ce > ce2, "C3 > C4 CO2 response");
    float yr = agri_climate_yield_response(2.0f, -10.0f, 1.1f, AGRI_CROP_MAIZE);
    CHECK(yr < 1.0f, "warming+drying reduces yield");
    agri_season_context_t bl; agri_season_config_t cfg;
    memset(&cfg, 0, sizeof(cfg)); cfg.crop_type = AGRI_CROP_WHEAT;
    agri_season_init(&bl, &cfg); bl.predicted_yield_ton_ha = 8.0f;
    agri_climate_scenario_t sc = {2.0f, -10.0f, 550.0f, 2050, 20.0f, 30.0f, 10.0f};
    agri_climate_impact_t imp;
    agri_climate_impact_assess(&bl, &sc, &imp);
    agri_climate_impact_assess(NULL, &sc, &imp);
    PASS(); return 0;
}

static int test_insurance(void) {
    TEST("insurance");
    agri_season_context_t ctx; agri_season_config_t cfg;
    memset(&cfg, 0, sizeof(cfg)); cfg.crop_type = AGRI_CROP_MAIZE;
    agri_season_init(&ctx, &cfg); ctx.predicted_yield_ton_ha = 10.0f;
    agri_insurance_policy_t pol;
    agri_insurance_quote(&pol, &ctx, AGRI_INSURANCE_REVENUE_PROTECTION, 0.75f, 250.0f);
    CHECK(pol.coverage_level == 0.75f, "coverage set");
    CHECK(pol.premium_per_ha > 0.0f, "premium positive");
    float ind = agri_insurance_calculate_indemnity(&pol, 5.0f, 220.0f);
    CHECK(ind > 0.0f, "indemnity on loss");
    CHECK(agri_insurance_calculate_indemnity(&pol, 10.0f, 250.0f) == 0.0f,
          "no indemnity full yield");
    CHECK(agri_insurance_actuarial_premium(10.0f, 2.5f, 0.75f, 250.0f) > 0.0f,
          "actuarial premium");
    agri_insurance_policy_t yp;
    agri_insurance_quote(&yp, &ctx, AGRI_INSURANCE_YIELD_PROTECTION, 0.75f, 250.0f);
    CHECK(agri_insurance_calculate_indemnity(&yp, 5.0f, 250.0f) > 0.0f,
          "yield protection indemnity");
    PASS(); return 0;
}

static int test_water_quality(void) {
    TEST("water_quality");
    agri_season_context_t ctx; agri_season_config_t cfg;
    memset(&cfg, 0, sizeof(cfg)); cfg.crop_type = AGRI_CROP_MAIZE;
    agri_season_init(&ctx, &cfg);
    agri_soil_init(&ctx.soil, "loam");
    agri_soil_add_layer(&ctx.soil, 30.0f, 20.0f, 40.0f, 40.0f, 2.5f, 6.5f);
    agri_weather_init(&ctx.weather);
    for (int d = 0; d < 30; d++)
        agri_weather_add_day(&ctx.weather, 2025, 6, 1+d, 25.0f, 15.0f,
                             (d%4==0)?12.0f:0.0f, 20.0f, 55.0f, 2.0f);
    agri_fertilizer_plan_t fp = {150.0f, 40.0f, 100.0f, 15.0f, 2.0f, 1.0f};
    agri_water_quality_t wq;
    agri_water_quality_estimate(&ctx, &fp, 2.0f, 50.0f, &wq);
    CHECK(wq.total_n_loss_kg_ha >= 0.0f, "N loss nonneg");
    float cn = agri_runoff_curve_number(AGRI_CROP_WHEAT, &ctx.soil);
    CHECK(cn > 0.0f && cn < 100.0f, "CN valid");
    float sdr = agri_sediment_delivery_ratio(10.0f, 3.0f);
    CHECK(sdr > 0.0f && sdr < 1.0f, "SDR valid");
    PASS(); return 0;
}

static int test_tillage(void) {
    TEST("tillage_systems");
    agri_soil_profile_t soil; agri_soil_init(&soil, "loam");
    agri_soil_add_layer(&soil, 30.0f, 20.0f, 40.0f, 40.0f, 2.5f, 6.5f);
    agri_tillage_system_t ts;
    agri_tillage_evaluate(AGRI_TILLAGE_NO_TILL, &soil, &ts);
    CHECK(ts.surface_residue_pct > 50.0f, "no-till high residue");
    CHECK(ts.erosion_reduction_pct > 50.0f, "no-till reduces erosion");
    agri_tillage_evaluate(AGRI_TILLAGE_CONVENTIONAL, &soil, &ts);
    CHECK(ts.soil_disturbance_pct > 50.0f, "conventional disturbs");
    float decay = agri_tillage_residue_decay(5000.0f, 20.0f, 0.25f, 30);
    CHECK(decay > 0.0f, "residue decays");
    float soc = agri_tillage_carbon_impact(AGRI_TILLAGE_CONVENTIONAL,
                                            AGRI_TILLAGE_NO_TILL, 50.0f, 20);
    CHECK(soc > 0.0f, "switch to no-till sequesters C");
    agri_tillage_evaluate(AGRI_TILLAGE_NO_TILL, NULL, &ts);
    agri_tillage_evaluate(AGRI_TILLAGE_NO_TILL, &soil, NULL);
    PASS(); return 0;
}

static int test_yield_map(void) {
    TEST("yield_map_vrt");
    agri_yield_map_t map;
    agri_yield_map_init(&map, 5, 5, 10.0f);
    CHECK(map.rows == 5 && map.cols == 5, "dimensions");
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 5; c++)
            agri_yield_map_set(&map, r, c, 5.0f + (float)r * 0.5f);
    CHECK(agri_yield_map_get(&map, 2, 2) > 0.0f, "yield get");
    CHECK(agri_yield_map_average(&map) > 0.0f, "avg positive");
    CHECK(agri_yield_map_cv(&map) > 0.0f, "CV positive");
    int zones[25];
    CHECK(agri_yield_map_zones(&map, 3, zones) >= 2, "multiple zones");
    agri_fertilizer_plan_t bp = {150.0f, 40.0f, 100.0f, 15.0f, 2.0f, 1.0f};
    agri_vrt_prescription_t rx;
    agri_vrt_generate_prescription(&map, &bp, &rx, 2, 2);
    CHECK(rx.n_rate_kg_ha > 0.0f, "VRT N rate");
    agri_yield_map_free(&map);
    CHECK(agri_yield_map_get(NULL, 0, 0) == 0.0f, "null map returns 0");
    CHECK(agri_yield_map_average(NULL) == 0.0f, "null avg returns 0");
    agri_yield_map_init(NULL, 0, 0, 0);
    agri_yield_map_free(NULL);
    PASS(); return 0;
}

static int test_grain_quality(void) {
    TEST("grain_quality");
    agri_season_context_t ctx; agri_season_config_t cfg;
    memset(&cfg, 0, sizeof(cfg)); cfg.crop_type = AGRI_CROP_WHEAT;
    agri_season_init(&ctx, &cfg); ctx.predicted_yield_ton_ha = 6.0f;
    agri_weather_init(&ctx.weather);
    for (int d = 0; d < 30; d++)
        agri_weather_add_day(&ctx.weather, 2025, 7, 1+d, 28.0f, 16.0f, 2.0f, 22.0f, 60.0f, 2.0f);
    for (int d = 0; d < 30; d++) {
        ctx.history[d].stage = AGRI_STAGE_GRAIN_FILL;
        ctx.history[d].doy = 182 + d;
    }
    ctx.days_simulated = 30;
    ctx.current.water_stress_factor = 0.9f;
    ctx.current.temperature_stress_factor = 0.85f;
    agri_grain_quality_t gq;
    agri_grain_quality_assess(&ctx, &gq);
    CHECK(strlen(gq.quality_grade) > 0, "grade assigned");
    CHECK(gq.grain_protein_pct > 0.0f, "protein computed");
    CHECK(gq.test_weight_kg_hl > 0.0f, "test weight computed");
    float afh = agri_mycotoxin_aflatoxin_risk(30.0f, 90.0f, 5.0f, AGRI_CROP_MAIZE);
    float afl = agri_mycotoxin_aflatoxin_risk(20.0f, 50.0f, 0.0f, AGRI_CROP_MAIZE);
    CHECK(afh > afl, "high risk > low risk");
    CHECK(agri_mycotoxin_aflatoxin_risk(30.0f, 90.0f, 5.0f, AGRI_CROP_WHEAT) == 0.0f,
          "aflatoxin maize-specific");
    CHECK(agri_mycotoxin_don_risk(22.0f, 15.0f, AGRI_CROP_WHEAT, 5.0f) > 0.0f, "DON risk");
    agri_grain_quality_assess(NULL, &gq);
    PASS(); return 0;
}

static int test_intercropping(void) {
    TEST("intercropping");
    agri_intercrop_system_t ics;
    agri_intercrop_evaluate(AGRI_CROP_MAIZE, AGRI_CROP_SOYBEAN, &ics);
    CHECK(ics.land_equivalent_ratio > 1.0f, "LER>1 for maize+soybean");
    CHECK(ics.nitrogen_benefit_kg_ha > 0.0f, "N benefit");
    agri_intercrop_evaluate(AGRI_CROP_MAIZE, AGRI_CROP_MAIZE, &ics);
    CHECK(ics.land_equivalent_ratio == 1.0f, "same crop LER=1");
    float ler = agri_land_equivalent_ratio(AGRI_CROP_MAIZE, 7.0f, 10.0f,
                                            AGRI_CROP_SOYBEAN, 2.5f, 4.0f);
    CHECK(ler > 1.0f, "LER computed");
    CHECK(fabsf(ler - (0.7f + 0.625f)) < 0.01f, "LER correct");
    agri_intercrop_evaluate(AGRI_CROP_MAIZE, AGRI_CROP_SOYBEAN, NULL);
    PASS(); return 0;
}

static int test_crop_lookup(void) {
    TEST("crop_lookup");
    const agri_crop_t* cp = agri_crop_lookup(AGRI_CROP_WHEAT);
    CHECK(cp != NULL && cp->type == AGRI_CROP_WHEAT, "wheat found");
    CHECK(cp->days_to_maturity > 0, "maturity set");
    CHECK(cp->kc_mid > cp->kc_initial, "Kc mid>initial");
    const agri_crop_t* cp2 = agri_crop_lookup(999);
    CHECK(cp2 != NULL && cp2->type == AGRI_CROP_CUSTOM, "OOB returns custom");
    const agri_crop_t* cp3 = agri_crop_lookup(-1);
    CHECK(cp3 != NULL && cp3->type == AGRI_CROP_CUSTOM, "negative returns custom");
    PASS(); return 0;
}
"""

# Find insertion point (before main)
insert_pos = existing.find('\nint main(void)')
if insert_pos < 0:
    insert_pos = existing.find('\nint main(')
    
# Insert new tests before main
updated = existing[:insert_pos] + new_tests + '\n' + existing[insert_pos:]

# Update main to call all new test functions
old_call = 'failed += test_season_simulation();'
new_calls = '''failed += test_season_simulation();
    failed += test_irrigation_scheduling();
    failed += test_pest_risk();
    failed += test_farm_management();
    failed += test_usle_erosion();
    failed += test_fertilizer_plan();
    failed += test_economic_analysis();
    failed += test_harvest_model();
    failed += test_rotation();
    failed += test_carbon_farming();
    failed += test_wgen();
    failed += test_nitrogen_cycle();
    failed += test_climate_impact();
    failed += test_insurance();
    failed += test_water_quality();
    failed += test_tillage();
    failed += test_yield_map();
    failed += test_grain_quality();
    failed += test_intercropping();
    failed += test_crop_lookup();'''

updated = updated.replace(old_call, new_calls)

with open('tests/test_agri.c', 'w') as f:
    f.write(updated)

print("Test file updated successfully")
