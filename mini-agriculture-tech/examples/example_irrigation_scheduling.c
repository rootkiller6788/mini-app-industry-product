/*
 * example_irrigation_scheduling.c - L5/L7: Irrigation Scheduling & WGEN Demo
 *
 * Demonstrates: MAD-based irrigation scheduling, deficit irrigation strategy,
 * water productivity analysis, and stochastic weather generation (WGEN).
 *
 * Reference: FAO-56 (Allen et al., 1998), Fereres & Soriano (2007),
 *            Richardson & Wright (1984) WGEN model.
 * Compile: gcc -std=c99 -O2 -I../include -o irrigation_demo example_irrigation_scheduling.c -L../build -lagri -lm
 */

#include "../include/agri_core.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

int main(void) {
    agri_init();
    printf("=== Precision Agriculture - Irrigation Scheduling & WGEN Demo ===\n\n");

    /* --- Part A: Build growing season with known weather --- */
    printf("--- Part A: MAD-Based Irrigation Scheduling ---\n");

    agri_soil_profile_t soil;
    agri_soil_init(&soil, "loam");
    agri_soil_add_layer(&soil, 40.0f, 20.0f, 40.0f, 40.0f, 2.5f, 6.5f);
    agri_soil_add_layer(&soil, 80.0f, 25.0f, 35.0f, 40.0f, 1.5f, 6.8f);

    agri_weather_series_t ws;
    agri_weather_init(&ws);
    ws.start_year = 2025; ws.start_doy = 121;

    for (int d = 0; d < 120; d++) {
        int doy = 121 + d, month, day;
        agri_date_from_doy(2025, doy, &month, &day);
        float s = (float)d / 119.0f;
        float bt = 18.0f + 10.0f * sinf(s * 3.14159f);
        float rain = (d % 5 == 0) ? 8.0f * (1.0f + 0.5f * sinf(s * 3.14159f)) : 0.0f;
        float solar = 15.0f + 10.0f * sinf(s * 3.14159f);
        agri_weather_add_day(&ws, 2025, month, day,
                             bt + 6.0f, bt - 4.0f, rain, solar, 55.0f, 2.5f);
    }

    agri_season_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.crop_type = AGRI_CROP_MAIZE;
    cfg.planting_doy = 121; cfg.harvest_doy = 240;
    cfg.irrigation_efficiency = 0.75f;

    agri_season_context_t season;
    agri_season_init(&season, &cfg);
    season.soil = soil; season.weather = ws;

    agri_gps_t v[4] = {
        agri_make_gps(39.0, -98.0, 500.0), agri_make_gps(39.001, -98.0, 500.0),
        agri_make_gps(39.001, -97.999, 501.0), agri_make_gps(39.0, -97.999, 501.0)
    };
    memcpy(season.field.vertices, v, 4 * sizeof(agri_gps_t));
    season.field.num_vertices = 4;

    agri_season_simulate(&season);
    printf("  Full irrigation: %.0f mm, Yield: %.2f t/ha\n",
           season.total_irrigation_mm, season.predicted_yield_ton_ha);
    printf("  Water productivity: %.2f kg/m3\n",
           agri_irrigation_water_productivity(&season));

    /* MAD-based scheduling at different thresholds */
    printf("\n  MAD Irrigation Schedules:\n");
    float mad_levels[] = {0.30f, 0.50f, 0.70f};
    for (int m = 0; m < 3; m++) {
        agri_irrigation_schedule_t sched;
        agri_irrigation_optimize(&season, &sched, mad_levels[m]);
        printf("  MAD=%.0f%%: %d events, total=%.0f mm, saved=%.0f mm vs full\n",
               mad_levels[m]*100.0f, sched.num_events,
               sched.total_water_applied_mm, sched.water_saved_vs_full_mm);
    }

    /* Deficit irrigation impact */
    printf("\n  Deficit Irrigation Yield Impact (Stewart model):\n");
    for (int d = 0; d <= 40; d += 10) {
        float rel_yield = agri_irrigation_deficit_irrigation(&season, (float)d);
        printf("  %d%% water reduction -> %.0f%% relative yield\n", d, rel_yield);
    }

    /* --- Part B: Stochastic Weather Generation (WGEN) --- */
    printf("\n--- Part B: Stochastic Weather Generation (WGEN) ---\n");
    agri_wgen_params_t wgen_params;
    agri_wgen_init(&wgen_params);
    agri_wgen_estimate_from_weather(&ws, &wgen_params);

    /* Generate 3 synthetic years for climate risk analysis */
    printf("  Parameters estimated from observed series\n");
    agri_weather_series_t synth[3];
    float synth_yields[3];
    for (int yr = 0; yr < 3; yr++) {
        agri_wgen_generate(&wgen_params, 2026 + yr, 365, &synth[yr], (uint32_t)(42 + yr * 137));

        agri_season_context_t s_ctx;
        agri_season_init(&s_ctx, &cfg);
        s_ctx.soil = soil; s_ctx.weather = synth[yr];
        memcpy(s_ctx.field.vertices, v, 4 * sizeof(agri_gps_t));
        s_ctx.field.num_vertices = 4;
        agri_season_simulate(&s_ctx);
        synth_yields[yr] = s_ctx.predicted_yield_ton_ha;
        printf("  Synthetic year %d: yield=%.2f t/ha, days=%d\n",
               2026 + yr, synth_yields[yr], synth[yr].num_days);
    }
    float mean_y = (synth_yields[0]+synth_yields[1]+synth_yields[2])/3.0f;
    float var = 0.0f;
    for (int i = 0; i < 3; i++) var += (synth_yields[i]-mean_y)*(synth_yields[i]-mean_y);
    printf("  Mean yield: %.2f t/ha, Std dev: %.2f t/ha\n",
           mean_y, sqrtf(var/2.0f));
    printf("  Climate risk: P(yield < 80%% baseline) estimated via WGEN ensemble\n");

    /* --- Part C: Soil Nitrogen Dynamics --- */
    printf("\n--- Part C: Soil Nitrogen Cycling ---\n");
    agri_nitrogen_pool_t n_pool;
    agri_nitrogen_init(&n_pool, 5000.0f, 30.0f, 10.0f);
    printf("  Initial: NO3=%.1f NH4=%.1f Organic=%.1f kg/ha\n",
           n_pool.no3_n_kg_ha, n_pool.nh4_n_kg_ha, n_pool.organic_n_kg_ha);

    for (int d = 0; d < 30; d++) {
        agri_nitrogen_step(&n_pool, 22.0f + 0.1f*(float)d,
                           0.25f, 0.35f, 3.0f, 2.5f, 5.0f);
    }
    printf("  After 30 days (warm, moist):\n");
    printf("  Mineralized: %.1f, Nitrified: %.1f, Denitrified: %.1f kg/ha/day\n",
           n_pool.mineralized_kg_ha_day, n_pool.nitrified_kg_ha_day,
           n_pool.denitrified_kg_ha_day);
    printf("  Leached: %.1f, Plant uptake: %.1f kg/ha/day\n",
           n_pool.leached_kg_ha_day, n_pool.plant_uptake_kg_ha_day);

    printf("\n=== Irrigation & WGEN Demo Complete ===\n");
    return 0;
}
