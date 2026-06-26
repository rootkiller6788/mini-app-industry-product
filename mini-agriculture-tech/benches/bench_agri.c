/*
 * bench_agri.c — Performance Benchmarks for Precision Agriculture Core
 *
 * Measures throughput for key computational paths:
 *   1. Season simulation (daily-step throughput)
 *   2. Weather generation (WGEN throughput)
 *   3. Yield map operations (spatial data throughput)
 *   4. Pest prediction (multi-model throughput)
 *
 * Compile: gcc -std=c99 -O2 -I../include -o bench_agri bench_agri.c -L../build -lagri -lm
 */

#include "../include/agri_core.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

static double now_ms(void) {
    return (double)clock() / (double)CLOCKS_PER_SEC * 1000.0;
}

int main(void) {
    agri_init();
    printf("=== Precision Agriculture Benchmarks ===\n\n");

    /* Prepare shared data */
    agri_soil_profile_t soil;
    agri_soil_init(&soil, "loam");
    agri_soil_add_layer(&soil, 40.0f, 20.0f, 40.0f, 40.0f, 2.5f, 6.5f);

    agri_weather_series_t weather;
    agri_weather_init(&weather);
    weather.start_year = 2025; weather.start_doy = 91;
    for (int d = 0; d < 200; d++) {
        int doy = 91 + d, month, day;
        agri_date_from_doy(2025, doy, &month, &day);
        float s = (float)d / 199.0f;
        float bt = 15.0f + 12.0f * sinf(s * 3.14159f);
        agri_weather_add_day(&weather, 2025, month, day,
                             bt+6.0f, bt-4.0f, (d%5==0)?8.0f:0.0f,
                             18.0f, 55.0f, 2.0f);
    }

    /* ── Benchmark 1: Season Simulation ── */
    printf("--- Benchmark 1: Season Simulation ---\n");
    const int SIM_ITER = 1000;
    double start = now_ms();
    float total_yield = 0.0f;
    for (int i = 0; i < SIM_ITER; i++) {
        agri_season_context_t ctx;
        agri_season_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.crop_type = (agri_crop_type_t)(i % 5);
        cfg.planting_doy = 91;
        cfg.irrigation_efficiency = 0.75f;
        agri_season_init(&ctx, &cfg);
        ctx.soil = soil; ctx.weather = weather;
        agri_gps_t v = agri_make_gps(40.0, -100.0, 300.0);
        ctx.field.vertices[0] = v;
        ctx.field.num_vertices = 1;
        agri_season_simulate(&ctx);
        total_yield += ctx.predicted_yield_ton_ha;
    }
    double elapsed = now_ms() - start;
    printf("  %d simulations in %.0f ms (%.2f us/sim)\n",
           SIM_ITER, elapsed, elapsed * 1000.0 / (double)SIM_ITER);
    printf("  Seasons/sec: %.0f\n", SIM_ITER / (elapsed / 1000.0));
    printf("  (avg yield: %.1f t/ha)\n", total_yield / (float)SIM_ITER);

    /* ── Benchmark 2: Weather Generation (WGEN) ── */
    printf("\n--- Benchmark 2: WGEN Weather Generation ---\n");
    agri_wgen_params_t wp;
    agri_wgen_init(&wp);
    const int WGEN_ITER = 100;
    start = now_ms();
    int total_days = 0;
    for (int i = 0; i < WGEN_ITER; i++) {
        agri_weather_series_t ws;
        agri_weather_init(&ws);
        agri_wgen_generate(&wp, 2025, 365, &ws, (uint32_t)(i * 7919 + 1));
        total_days += ws.num_days;
    }
    elapsed = now_ms() - start;
    printf("  %d years (365d) in %.0f ms (%.2f us/day)\n",
           WGEN_ITER, elapsed, elapsed * 1000.0 / (double)total_days);

    /* ── Benchmark 3: Yield Map Operations ── */
    printf("\n--- Benchmark 3: Yield Map Operations ---\n");
    const int MAP_ITER = 500;
    agri_yield_map_t map;
    agri_yield_map_init(&map, 20, 20, 10.0f);
    for (int r = 0; r < map.rows; r++)
        for (int c = 0; c < map.cols; c++)
            agri_yield_map_set(&map, r, c, 5.0f + 3.0f * sinf((float)(r*c)*0.3f));

    start = now_ms();
    float avg = 0.0f;
    for (int i = 0; i < MAP_ITER; i++) {
        avg += agri_yield_map_average(&map);
        avg += agri_yield_map_cv(&map);
        int zones[400];
        agri_yield_map_zones(&map, 3, zones);
    }
    elapsed = now_ms() - start;
    printf("  %d map operations (avg+cv+zones on 20x20) in %.0f ms\n",
           MAP_ITER, elapsed);
    printf("  Avg yield: %.1f t/ha\n", avg / (float)(MAP_ITER * 2));
    agri_yield_map_free(&map);

    /* ── Benchmark 4: Pest Risk Prediction ── */
    printf("\n--- Benchmark 4: Pest Risk Prediction ---\n");
    agri_pest_predictor_t pp;
    agri_pest_init(&pp, AGRI_CROP_MAIZE);
    agri_pest_add_model(&pp, AGRI_PEST_FUNGAL, "TestFungus", 20.0f, 30.0f, 65.0f, 10.0f);
    agri_pest_add_model(&pp, AGRI_PEST_INSECT, "TestInsect", 16.0f, 28.0f, 50.0f, 10.0f);
    agri_pest_add_model(&pp, AGRI_PEST_BACTERIAL, "TestBacteria", 18.0f, 26.0f, 75.0f, 8.0f);

    const int PEST_ITER = 200;
    start = now_ms();
    int total_risks = 0;
    for (int i = 0; i < PEST_ITER; i++) {
        agri_pest_risk_event_t risks[32];
        int n_risks = 0;
        agri_pest_predict(&pp, &weather, risks, &n_risks);
        total_risks += n_risks;
    }
    elapsed = now_ms() - start;
    printf("  %d pest predictions (3 models x 200d) in %.0f ms\n",
           PEST_ITER, elapsed);
    printf("  Avg risks/run: %.1f\n", (float)total_risks / (float)PEST_ITER);

    /* ── Benchmark 5: Nitrogen Cycle ── */
    printf("\n--- Benchmark 5: Nitrogen Cycle Simulation ---\n");
    const int N_ITER = 5000;
    start = now_ms();
    for (int i = 0; i < N_ITER; i++) {
        agri_nitrogen_pool_t np;
        agri_nitrogen_init(&np, 5000.0f, 30.0f, 10.0f);
        agri_nitrogen_step(&np, 20.0f + (float)(i%15), 0.25f, 0.35f, 3.0f, 2.5f, 5.0f);
    }
    elapsed = now_ms() - start;
    printf("  %d N-cycle steps in %.0f ms (%.2f us/step)\n",
           N_ITER, elapsed, elapsed * 1000.0 / (double)N_ITER);

    /* ── Benchmark 6: ET0 Computation ── */
    printf("\n--- Benchmark 6: FAO-56 ET0 Computation ---\n");
    const int ET0_ITER = 10000;
    start = now_ms();
    float total_et0_val = 0.0f;
    for (int i = 0; i < ET0_ITER; i++) {
        agri_weather_day_t wd;
        memset(&wd, 0, sizeof(wd));
        wd.day = 180; wd.temp_max_c = 25.0f + (float)(i%10); wd.temp_min_c = 15.0f;
        wd.temp_avg_c = (wd.temp_max_c + wd.temp_min_c) * 0.5f;
        wd.humidity_pct = 50.0f; wd.solar_radiation_MJ_m2 = 20.0f; wd.wind_speed_m_s = 2.0f;
        total_et0_val += agri_weather_et0_penman_monteith(&wd, 100.0f, 35.0f);
    }
    elapsed = now_ms() - start;
    printf("  %d ET0 computations in %.0f ms (%.2f us/ET0)\n",
           ET0_ITER, elapsed, elapsed * 1000.0 / (double)ET0_ITER);

    printf("\n=== Benchmarks Complete ===\n");
    return 0;
}
