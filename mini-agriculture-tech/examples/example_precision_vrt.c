/*
 * example_precision_vrt.c - L8/L9: Precision Agriculture VRT & Carbon Farming
 *
 * Demonstrates: Variable Rate Technology (VRT) with yield mapping,
 * management zone delineation, precision fertilizer prescription,
 * crop rotation optimization, carbon sequestration assessment,
 * climate change impact projection, and intercropping evaluation.
 *
 * Reference: Gebbers & Adamchuk (2010), IPCC AFOLU (2019),
 *            Rosenzweig et al. (2014) PNAS, Willey (1979)
 * Compile: gcc -std=c99 -O2 -I../include -o precision_vrt example_precision_vrt.c -L../build -lagri -lm
 */

#include "../include/agri_core.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

int main(void) {
    agri_init();
    printf("=== Precision Agriculture - VRT, Carbon & Climate Demo ===\n\n");

    /* --- Part A: Yield Mapping & VRT --- */
    printf("--- Part A: Yield Mapping & Variable Rate Technology ---\n");
    agri_yield_map_t ymap;
    agri_yield_map_init(&ymap, 10, 10, 30.0f);

    /* Create spatial yield variability: high center, lower edges */
    for (int r = 0; r < ymap.rows; r++) {
        for (int c = 0; c < ymap.cols; c++) {
            float dist = sqrtf((float)((r-5)*(r-5)+(c-5)*(c-5)));
            float base_yield = 10.0f - dist * 0.6f;
            float noise = sinf((float)(r*c+1)*0.7f) * 1.5f;
            agri_yield_map_set(&ymap, r, c, base_yield + noise);
            /* Vary soil properties spatially */
            ymap.soil_ph_data[r*ymap.cols+c] = 6.8f - (float)r * 0.05f;
            ymap.organic_matter_data[r*ymap.cols+c] = 3.5f - (float)c * 0.2f;
        }
    }

    printf("  Grid: %dx%d, Cell size: %.0f m\n", ymap.rows, ymap.cols, ymap.cell_size_m);
    printf("  Average yield: %.2f t/ha\n", agri_yield_map_average(&ymap));
    printf("  Yield CV: %.1f%%\n", agri_yield_map_cv(&ymap));

    /* Zone delineation */
    int zone_map[100];
    int nz = agri_yield_map_zones(&ymap, 3, zone_map);
    printf("  Management zones: %d\n", nz);

    /* Generate VRT prescription for a high-yield and low-yield zone */
    agri_fertilizer_plan_t base_plan;
    agri_soil_test_t st = {.soil_n_kg_ha=40.0f, .soil_p_kg_ha=20.0f,
        .soil_k_kg_ha=150.0f, .yield_goal_ton_ha=10.0f,
        .previous_crop_n_credit=20.0f, .organic_matter_n_release=15.0f};
    agri_fertilizer_plan(&st, AGRI_CROP_MAIZE, &base_plan);
    printf("  Base N rate: %.1f kg/ha\n", base_plan.nitrogen_kg_ha);

    agri_vrt_prescription_t rx_high, rx_low;
    agri_vrt_generate_prescription(&ymap, &base_plan, &rx_high, 3, 3);
    agri_vrt_generate_prescription(&ymap, &base_plan, &rx_low, 8, 8);
    printf("  VRT high zone (3,3): N=%.1f P=%.1f K=%.1f kg/ha\n",
           rx_high.n_rate_kg_ha, rx_high.p_rate_kg_ha, rx_high.k_rate_kg_ha);
    printf("  VRT low zone  (8,8): N=%.1f P=%.1f K=%.1f kg/ha\n",
           rx_low.n_rate_kg_ha, rx_low.p_rate_kg_ha, rx_low.k_rate_kg_ha);
    agri_yield_map_free(&ymap);

    /* --- Part B: Crop Rotation Optimization --- */
    printf("\n--- Part B: Crop Rotation Optimization ---\n");
    agri_crop_t available_crops[4];
    agri_crop_type_t crop_types[] = {AGRI_CROP_MAIZE, AGRI_CROP_SOYBEAN,
                                      AGRI_CROP_WHEAT, AGRI_CROP_POTATO};
    for (int i = 0; i < 4; i++) {
        const agri_crop_t* cp = agri_crop_lookup(crop_types[i]);
        if (cp) available_crops[i] = *cp;
    }

    agri_rotation_plan_t plans[10];
    int n_plans = 0;
    agri_soil_profile_t rot_soil;
    agri_soil_init(&rot_soil, "loam");
    agri_soil_add_layer(&rot_soil, 30.0f, 20.0f, 40.0f, 40.0f, 2.5f, 6.5f);

    agri_rotation_optimize(available_crops, 4, &rot_soil, plans, &n_plans);
    printf("  Generated %d candidate rotations:\n", n_plans);
    for (int p = 0; p < n_plans && p < 3; p++) {
        printf("  Rotation %d: [", p+1);
        for (int i = 0; i < plans[p].length; i++) {
            const agri_crop_t* cp = agri_crop_lookup(plans[p].sequence[i]);
            printf("%s%s", cp ? cp->name : "?", i < plans[p].length-1 ? ", " : "");
        }
        printf("] N_credit=%.0f Health=%.2f Profit=$%.0f\n",
               plans[p].nitrogen_credit_kg_ha, plans[p].soil_health_score,
               plans[p].profit_rotation_avg);
    }

    /* --- Part C: Carbon Farming Assessment --- */
    printf("\n--- Part C: Carbon Farming & GHG Accounting ---\n");
    printf("  Sequestration potentials:\n");
    printf("  Maize no-till + cover:    %.2f t/ha/yr\n",
           agri_carbon_sequestration_potential(&rot_soil, AGRI_CROP_MAIZE, true, true));
    printf("  Wheat conventional:       %.2f t/ha/yr\n",
           agri_carbon_sequestration_potential(&rot_soil, AGRI_CROP_WHEAT, false, false));
    printf("  Soybean no-till:          %.2f t/ha/yr\n",
           agri_carbon_sequestration_potential(&rot_soil, AGRI_CROP_SOYBEAN, false, true));
    printf("  Rice CH4 (1ha, 120d):     %.0f kg CO2e\n",
           agri_carbon_methane_from_rice(1.0f, 120.0f, 0.0f));
    printf("  N2O from 200kg N/ha:      %.0f kg CO2e\n",
           agri_carbon_n2o_from_fertilizer(200.0f, 0.01f));

    /* --- Part D: Climate Change Impact Projection --- */
    printf("\n--- Part D: Climate Change Impact (IPCC AR6) ---\n");
    agri_climate_scenario_t rcp45 = {
        .temp_increase_c = 1.5f, .precip_change_pct = 5.0f,
        .co2_ppm = 550.0f, .projection_year = 2050,
        .extreme_heat_days_yr = 15.0f, .drought_frequency_pct = 20.0f
    };
    agri_climate_scenario_t rcp85 = {
        .temp_increase_c = 3.5f, .precip_change_pct = -10.0f,
        .co2_ppm = 850.0f, .projection_year = 2080,
        .extreme_heat_days_yr = 45.0f, .drought_frequency_pct = 40.0f
    };

    agri_crop_type_t cc_crops[] = {AGRI_CROP_MAIZE, AGRI_CROP_WHEAT, AGRI_CROP_SOYBEAN};
    const char* cc_names[] = {"Maize", "Wheat", "Soybean"};
    agri_climate_scenario_t* scenarios[] = {&rcp45, &rcp85};
    const char* sc_names[] = {"RCP4.5 (2050)", "RCP8.5 (2080)"};

    for (int s = 0; s < 2; s++) {
        printf("  %s:\n", sc_names[s]);
        for (int c = 0; c < 3; c++) {
            float co2_eff = agri_climate_co2_fertilization_effect(scenarios[s]->co2_ppm, cc_crops[c]);
            float y_resp = agri_climate_yield_response(scenarios[s]->temp_increase_c,
                            scenarios[s]->precip_change_pct, co2_eff, cc_crops[c]);
            printf("    %-7s: CO2 effect=%.3f, Yield change=%+.0f%%\n",
                   cc_names[c], co2_eff, (y_resp-1.0f)*100.0f);
        }
    }

    /* --- Part E: Intercropping Evaluation --- */
    printf("\n--- Part E: Intercropping Systems ---\n");
    agri_crop_type_t inter_pairs[][2] = {
        {AGRI_CROP_MAIZE, AGRI_CROP_SOYBEAN},
        {AGRI_CROP_WHEAT, AGRI_CROP_SOYBEAN},
        {AGRI_CROP_MAIZE, AGRI_CROP_POTATO}
    };
    for (int i = 0; i < 3; i++) {
        agri_intercrop_system_t ics;
        agri_intercrop_evaluate(inter_pairs[i][0], inter_pairs[i][1], &ics);
        const agri_crop_t* p1 = agri_crop_lookup(inter_pairs[i][0]);
        const agri_crop_t* p2 = agri_crop_lookup(inter_pairs[i][1]);
        printf("  %s+%s: LER=%.2f, N_benefit=%.0f, Yield+=%.0f%%\n",
               p1->name, p2->name, ics.land_equivalent_ratio,
               ics.nitrogen_benefit_kg_ha, ics.yield_advantage_pct);
    }

    /* --- Part F: Tillage Comparison --- */
    printf("\n--- Part F: Tillage System Comparison ---\n");
    agri_tillage_type_t tills[] = {AGRI_TILLAGE_CONVENTIONAL, AGRI_TILLAGE_REDUCED,
                                    AGRI_TILLAGE_NO_TILL, AGRI_TILLAGE_STRIP_TILL};
    const char* till_names[] = {"Conventional", "Reduced", "No-Till", "Strip-Till"};
    for (int t = 0; t < 4; t++) {
        agri_tillage_system_t ts;
        agri_tillage_evaluate(tills[t], &rot_soil, &ts);
        printf("  %-14s: residue=%.0f%%, fuel=%.0f L/ha, erosion=-%.0f%%, yield=%+.0f%%\n",
               till_names[t], ts.surface_residue_pct, ts.fuel_use_l_ha,
               ts.erosion_reduction_pct, ts.yield_impact_pct);
    }

    /* --- Part G: Pest Risk Prediction --- */
    printf("\n--- Part G: Pest & Disease Risk Modeling ---\n");
    agri_pest_predictor_t pp;
    agri_pest_init(&pp, AGRI_CROP_MAIZE);
    agri_pest_add_model(&pp, AGRI_PEST_FUNGAL, "Gray Leaf Spot", 22.0f, 30.0f, 70.0f, 10.0f);
    agri_pest_add_model(&pp, AGRI_PEST_INSECT, "European Corn Borer", 16.0f, 30.0f, 50.0f, 10.0f);

    agri_weather_series_t pest_ws;
    agri_weather_init(&pest_ws);
    pest_ws.start_year = 2025; pest_ws.start_doy = 150;
    for (int d = 0; d < 90; d++) {
        int doy = 150 + d, month, day;
        agri_date_from_doy(2025, doy, &month, &day);
        float tavg = 22.0f + 6.0f * sinf((float)d/90.0f * 3.14159f);
        agri_weather_add_day(&pest_ws, 2025, month, day,
            tavg+4.0f, tavg-3.0f, (d%3==0)?10.0f:0.0f, 20.0f, 65.0f+(float)(d%20), 2.0f);
    }
    agri_pest_risk_event_t risks[32];
    int n_risks = 0;
    agri_pest_predict(&pp, &pest_ws, risks, &n_risks);
    printf("  %d pest risk events predicted:\n", n_risks);
    for (int i = 0; i < n_risks && i < 4; i++) {
        printf("  Day %d: risk=%.2f, severity=%.1f, yield_loss=%.1f%% — %s\n",
               risks[i].doy, risks[i].risk_index, risks[i].severity_estimate,
               risks[i].yield_loss_pct, risks[i].recommendation);
    }

    printf("\n=== Precision VRT & Carbon Demo Complete ===\n");
    return 0;
}
