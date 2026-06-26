#include "../include/agri_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_run = 0, tests_passed = 0;

#define TEST(n) do { tests_run++; printf("  TEST %s ... ", n); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(m) do { printf("FAIL: %s\n", m); return 1; } while(0)
#define CHECK(c,m) if (!(c)) FAIL(m)

static int test_gps_distance(void) {
    TEST("gps_distance");
    agri_gps_t a = agri_make_gps(40.0, -105.0, 1600.0);
    agri_gps_t b = agri_make_gps(40.0, -105.0, 1600.0);
    double d = agri_gps_distance_m(&a, &b);
    CHECK(d < 1.0, "same point should have 0 distance");
    PASS(); return 0;
}

static int test_polygon_area(void) {
    TEST("polygon_area");
    agri_gps_t v[4];
    v[0] = agri_make_gps(40.0, -105.0, 0);
    v[1] = agri_make_gps(40.001, -105.0, 0);
    v[2] = agri_make_gps(40.001, -104.999, 0);
    v[3] = agri_make_gps(40.0, -104.999, 0);
    double area = agri_polygon_area_ha(v, 4);
    CHECK(area > 0.1, "area should be positive");
    CHECK(agri_polygon_area_ha(NULL, 4) == 0.0, "null vertices returns 0");
    CHECK(agri_polygon_area_ha(v, 2) == 0.0, "fewer than 3 returns 0");
    PASS(); return 0;
}

static int test_soil_awc(void) {
    TEST("soil_awc");
    agri_soil_profile_t soil;
    agri_soil_init(&soil, "loam");
    agri_soil_add_layer(&soil, 30.0f, 20.0f, 40.0f, 40.0f, 2.0f, 6.5f);
    agri_soil_add_layer(&soil, 60.0f, 25.0f, 35.0f, 40.0f, 1.5f, 6.8f);
    float awc = agri_soil_awc_mm(&soil, 50.0f);
    CHECK(awc > 0.0f, "AWC should be positive");
    CHECK(agri_soil_awc_mm(NULL, 50.0f) == 0.0f, "null soil returns 0");
    PASS(); return 0;
}

static int test_et0_penman_monteith(void) {
    TEST("et0_penman_monteith");
    agri_weather_day_t w;
    memset(&w, 0, sizeof(w));
    w.day = 180; w.temp_max_c = 30.0f; w.temp_min_c = 18.0f;
    w.temp_avg_c = 24.0f; w.humidity_pct = 60.0f;
    w.solar_radiation_MJ_m2 = 22.0f; w.wind_speed_m_s = 2.0f;
    float et0 = agri_weather_et0_penman_monteith(&w, 100.0f, 35.0f);
    CHECK(et0 > 0.0f && et0 < 20.0f, "ET0 should be in range");
    CHECK(agri_weather_et0_penman_monteith(NULL, 0, 0) == 0.0f, "null returns 0");
    PASS(); return 0;
}

static int test_gdd_accum(void) {
    TEST("gdd_accum");
    agri_weather_series_t ws;
    agri_weather_init(&ws);
    agri_weather_add_day(&ws, 2024, 6, 1, 25.0f, 15.0f, 0.0f, 20.0f, 50.0f, 2.0f);
    agri_weather_add_day(&ws, 2024, 6, 2, 28.0f, 17.0f, 0.0f, 22.0f, 45.0f, 1.5f);
    agri_weather_add_day(&ws, 2024, 6, 3, 30.0f, 19.0f, 5.0f, 18.0f, 60.0f, 3.0f);
    float gdd = agri_weather_gdd_accum(&ws, 152, 153, 10.0f, 30.0f);
    CHECK(gdd > 0.0f, "GDD should be positive");
    CHECK(agri_weather_gdd_accum(NULL, 1, 10, 10.0f, 30.0f) == 0.0f, "null returns 0");
    PASS(); return 0;
}

static int test_doy_conversion(void) {
    TEST("doy_conversion");
    int doy = agri_doy_from_date(2024, 6, 15);
    CHECK(doy == 167, "June 15 2024 should be DOY 167");
    int m, d;
    agri_date_from_doy(2024, 167, &m, &d);
    CHECK(m == 6 && d == 15, "DOY 167 should be June 15");
    int doy_leap = agri_doy_from_date(2024, 2, 29);
    CHECK(doy_leap == 60, "Feb 29 leap year");
    agri_date_from_doy(2024, 100, NULL, NULL);
    PASS(); return 0;
}

static int test_season_simulation(void) {
    TEST("season_simulation");
    agri_season_context_t ctx;
    agri_season_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.crop_type = AGRI_CROP_WHEAT; cfg.planting_doy = 60;
    cfg.irrigation_efficiency = 0.75f;
    agri_season_init(&ctx, &cfg);
    agri_soil_init(&ctx.soil, "silt_loam");
    agri_soil_add_layer(&ctx.soil, 40.0f, 18.0f, 55.0f, 27.0f, 2.5f, 6.8f);
    ctx.field.vertices[0] = agri_make_gps(38.0, -97.0, 350.0);
    agri_weather_init(&ctx.weather);
    for (int d = 0; d < 120; d++) {
        int month = 3 + d/30, day = 1 + d%30;
        agri_weather_add_day(&ctx.weather, 2024, month, day,
            18.0f + 10.0f*sinf((float)d/120.0f*3.14159f),
            8.0f + 5.0f*sinf((float)d/120.0f*3.14159f),
            1.0f + 2.0f*((float)(d%7==0)),
            15.0f + 10.0f*sinf((float)d/120.0f*3.14159f),
            55.0f, 2.5f);
    }
    agri_season_simulate(&ctx);
    float yield = agri_predict_yield(&ctx);
    CHECK(yield > 0.0f, "yield should be positive");
    CHECK(ctx.days_simulated > 0, "should have simulated days");
    CHECK(ctx.complete, "simulation should complete");
    agri_season_free(&ctx);
    CHECK(agri_predict_yield(NULL) == 0.0f, "null predict_yield returns 0");
    PASS(); return 0;
}

int main(void) {
    printf("=== mini-agriculture-tech Unit Tests ===\n\n");
    int failed = 0;
    failed += test_gps_distance();
    failed += test_polygon_area();
    failed += test_soil_awc();
    failed += test_et0_penman_monteith();
    failed += test_gdd_accum();
    failed += test_doy_conversion();
    failed += test_season_simulation();
    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, failed);
    return failed > 0 ? 1 : 0;
}
