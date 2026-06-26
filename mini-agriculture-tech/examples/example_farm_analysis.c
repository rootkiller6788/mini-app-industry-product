/*
 * example_farm_analysis.c - L6/L7: End-to-End Farm Analysis
 *
 * Demonstrates: field geometry, soil characterization, weather time series,
 * crop growth simulation, farm-level optimization, economic analysis,
 * environmental impact (water quality + carbon), crop insurance,
 * harvest loss modeling, and grain quality assessment.
 *
 * Reference: FAO-56, DSSAT, Purdue Farm Management
 * Compile: gcc -std=c99 -O2 -I../include -o farm_analysis example_farm_analysis.c -L../build -lagri -lm
 */

#include "../include/agri_core.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

int main(void) {
    agri_init();
    printf("=== Precision Agriculture - Full Farm Analysis Demo ===\n\n");

    /* Step 1: Field definition */
    printf("--- STEP 1: Field Definition ---\n");
    agri_field_t field;
    memset(&field, 0, sizeof(field));
    snprintf(field.field_id, AGRI_MAX_NAME, "F-IA-042");
    field.vertices[0] = agri_make_gps(41.6000, -93.6000, 280.0);
    field.vertices[1] = agri_make_gps(41.6010, -93.6000, 280.0);
    field.vertices[2] = agri_make_gps(41.6010, -93.5990, 281.0);
    field.vertices[3] = agri_make_gps(41.6000, -93.5990, 281.0);
    field.num_vertices = 4;
    field.area_hectares = agri_polygon_area_ha(field.vertices, 4);
    printf("  Field ID: %s, Area: %.1f ha\n", field.field_id, field.area_hectares);

    /* Step 2: Soil profile */
    printf("\n--- STEP 2: Soil Profile ---\n");
    agri_soil_profile_t soil;
    agri_soil_init(&soil, "silt_loam");
    agri_soil_add_layer(&soil, 30.0f, 18.0f, 55.0f, 27.0f, 3.5f, 6.8f);
    agri_soil_add_layer(&soil, 60.0f, 28.0f, 35.0f, 37.0f, 1.5f, 6.5f);
    agri_soil_add_layer(&soil, 100.0f, 32.0f, 30.0f, 38.0f, 0.5f, 6.2f);
    printf("  Texture: %s, Layers: %d, AWC(100cm): %.0f mm\n",
           soil.usda_texture_class, soil.num_layers,
           agri_soil_awc_mm(&soil, 100.0f));

    /* Step 3: Weather time series */
    printf("\n--- STEP 3: Weather Time Series ---\n");
    agri_weather_series_t weather;
    agri_weather_init(&weather);
    weather.start_year = 2025; weather.start_doy = 91;
    for (int d = 0; d < 150; d++) {
        int doy = 91 + d, month, day;
        agri_date_from_doy(2025, doy, &month, &day);
        float s = (float)d / 149.0f;
        float bt = 12.0f + 15.0f * sinf(s * 3.14159f);
        float dr = 8.0f + 4.0f * sinf(s * 3.14159f);
        float rain = ((d * 7) % 10 == 0) ? 15.0f + 10.0f * ((float)(d % 5)/5.0f) : 0.0f;
        float solar = 10.0f + 16.0f * sinf(s * 3.14159f);
        float hum = 55.0f + 15.0f * sinf(s * 3.14159f);
        agri_weather_add_day(&weather, 2025, month, day,
                             bt+dr*0.6f, bt-dr*0.4f, rain, solar, hum, 2.0f);
    }
    float tot_rain = 0.0f, tot_et0 = 0.0f;
    for (int i = 0; i < weather.num_days; i++) {
        tot_rain += weather.days[i].rainfall_mm;
        tot_et0 += agri_weather_et0_penman_monteith(&weather.days[i], 280.0f, 41.6f);
    }
    printf("  Days: %d, Rain: %.0f mm, ET0: %.0f mm\n",
           weather.num_days, tot_rain, tot_et0);
    printf("  GDD(10-30): %.0f degC-days\n",
           agri_weather_gdd_accum(&weather, 91, 240, 10.0f, 30.0f));

    /* Step 4: Crop simulation - multi-crop comparison */
    printf("\n--- STEP 4: Crop Growth Simulation ---\n");
    agri_crop_type_t crops[] = {AGRI_CROP_MAIZE, AGRI_CROP_WHEAT, AGRI_CROP_SOYBEAN};
    const char* cnames[] = {"Maize", "Wheat", "Soybean"};
    agri_season_context_t sctx[3];
    for (int c = 0; c < 3; c++) {
        agri_season_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.crop_type = crops[c];
        cfg.planting_doy = 91; cfg.harvest_doy = 240;
        cfg.irrigation_efficiency = 0.75f;
        agri_season_init(&sctx[c], &cfg);
        sctx[c].soil = soil; sctx[c].weather = weather; sctx[c].field = field;
        agri_season_simulate(&sctx[c]);
        printf("  %s: yield=%.2f t/ha, irrig=%.0f mm, WUE=%.2f kg/m3\n",
               cnames[c], sctx[c].predicted_yield_ton_ha,
               sctx[c].total_irrigation_mm,
               agri_irrigation_water_productivity(&sctx[c]));
    }

    /* Step 5: Farm management */
    printf("\n--- STEP 5: Farm-Level Management ---\n");
    agri_farm_t farm;
    agri_farm_init(&farm, "Green Valley Farm");
    agri_farm_add_field(&farm, &field, &sctx[0].config, &soil, &weather);
    agri_farm_simulate_all(&farm);
    agri_farm_summary(&farm);

    /* Step 6: Economic analysis */
    printf("\n--- STEP 6: Economic Decision Support ---\n");
    agri_cost_breakdown_t costs = {
        .seed_cost_per_ha = 180.0f, .fertilizer_cost_per_ha = 350.0f,
        .pesticide_cost_per_ha = 120.0f, .fuel_cost_per_ha = 90.0f,
        .labor_cost_per_ha = 200.0f, .irrigation_cost_per_mm_ha = 0.25f,
        .land_rent_per_ha = 600.0f, .insurance_per_ha = 75.0f,
        .misc_cost_per_ha = 50.0f
    };
    agri_economic_analysis_t econ;
    agri_economic_analyze(&sctx[0], &costs, 250.0f, &econ);
    printf("  Revenue/ha: $%.0f, Net profit/ha: $%.0f\n",
           econ.expected_revenue_per_ha, econ.net_profit_per_ha);
    printf("  Break-even yield: %.2f t/ha, ROI: %.1f%%\n",
           econ.break_even_yield_ton_ha, econ.return_on_investment_pct);

    /* Step 7: Environmental impact */
    printf("\n--- STEP 7: Environmental Impact ---\n");
    agri_soil_test_t st = {.soil_n_kg_ha=45.0f, .soil_p_kg_ha=25.0f,
        .soil_k_kg_ha=180.0f, .yield_goal_ton_ha=10.0f,
        .previous_crop_n_credit=20.0f, .organic_matter_n_release=15.0f};
    agri_fertilizer_plan_t fp;
    agri_fertilizer_plan(&st, AGRI_CROP_MAIZE, &fp);
    agri_water_quality_t wq;
    agri_water_quality_estimate(&sctx[0], &fp, 2.0f, 0.0f, &wq);
    printf("  Runoff: %.0f mm, Sediment: %.2f t/ha, N loss: %.2f kg/ha\n",
           wq.surface_runoff_mm, wq.sediment_loss_ton_ha, wq.total_n_loss_kg_ha);

    agri_carbon_farm_t carb;
    agri_carbon_farm_assess(&farm, &carb);
    printf("  SOC: %.1f t/ha, Sequestration: %.2f t/ha/yr, Credits: %d\n",
           carb.soil_organic_carbon_ton_ha,
           carb.carbon_sequestration_rate_ton_ha_yr, carb.carbon_credits_earned);

    /* Step 8: Crop insurance */
    printf("\n--- STEP 8: Crop Insurance ---\n");
    agri_insurance_policy_t pol;
    agri_insurance_quote(&pol, &sctx[0], AGRI_INSURANCE_REVENUE_PROTECTION,
                          0.75f, 250.0f);
    printf("  Coverage: %.0f%%, Premium: $%.2f/ha\n", pol.coverage_level*100.0f, pol.premium_per_ha);
    float ind = agri_insurance_calculate_indemnity(&pol, sctx[0].predicted_yield_ton_ha*0.7f, 220.0f);
    printf("  Indemnity (70%% yield): $%.0f/ha\n", ind);

    /* Step 9: Harvest loss */
    printf("\n--- STEP 9: Post-Harvest Loss ---\n");
    agri_harvest_model_t hm;
    agri_harvest_model(&sctx[0], AGRI_HARVEST_COMBINE, &hm);
    printf("  Total loss: %.1f%%, Marketable: %.2f t/ha\n",
           hm.total_loss_pct, hm.marketable_yield_ton_ha);

    /* Step 10: Grain quality */
    printf("\n--- STEP 10: Grain Quality ---\n");
    agri_grain_quality_t gq;
    agri_grain_quality_assess(&sctx[0], &gq);
    printf("  Protein: %.1f%%, Test weight: %.1f kg/hL, Grade: %s\n",
           gq.grain_protein_pct, gq.test_weight_kg_hl, gq.quality_grade);

    printf("\n=== Farm Analysis Complete ===\n");
    return 0;
}
