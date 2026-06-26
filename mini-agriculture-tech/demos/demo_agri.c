/*
 * demo_agri.c — Interactive Demo: Precision Agriculture Dashboard
 *
 * A simple text-based demo showing the full system in action.
 * Simulates a farmer's dashboard for decision support.
 *
 * Compile: gcc -std=c99 -O2 -I../include -o demo_agri demo_agri.c -L../build -lagri -lm
 * Run: ./demo_agri
 */

#include "../include/agri_core.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Print a horizontal bar chart */
static void bar_chart(const char* label, float value, float max_val, int width) {
    printf("  %-18s |", label);
    int bar_len = (int)(value / (max_val + 1e-6f) * (float)width);
    if (bar_len > width) bar_len = width;
    for (int i = 0; i < bar_len; i++) printf("#");
    for (int i = bar_len; i < width; i++) printf(" ");
    printf("| %.0f\n", value);
}

int main(void) {
    agri_init();
    printf("============================================================\n");
    printf("    GREEN VALLEY FARM — Precision Agriculture Dashboard\n");
    printf("============================================================\n\n");

    /* Setup farm */
    agri_farm_t farm;
    agri_farm_init(&farm, "Green Valley Farm");

    /* Define 4 fields */
    agri_field_t fields[4];
    for (int f = 0; f < 4; f++) {
        memset(&fields[f], 0, sizeof(agri_field_t));
        snprintf(fields[f].field_id, AGRI_MAX_NAME, "GV-%02d", f+1);
        double base_lat = 41.6 + f * 0.002;
        fields[f].vertices[0] = agri_make_gps(base_lat, -93.60, 280.0);
        fields[f].vertices[1] = agri_make_gps(base_lat+0.001, -93.60, 280.0);
        fields[f].vertices[2] = agri_make_gps(base_lat+0.001, -93.599, 281.0);
        fields[f].vertices[3] = agri_make_gps(base_lat, -93.599, 281.0);
        fields[f].num_vertices = 4;
        fields[f].area_hectares = agri_polygon_area_ha(fields[f].vertices, 4);
    }

    /* Soil profiles */
    agri_soil_profile_t soils[4];
    const char* soil_types[] = {"loam", "silt_loam", "sandy_loam", "clay_loam"};
    for (int f = 0; f < 4; f++) {
        agri_soil_init(&soils[f], soil_types[f]);
        float clay_base = 15.0f + (float)f * 7.0f;
        agri_soil_add_layer(&soils[f], 30.0f, clay_base, 40.0f, 60.0f-clay_base, 3.0f-0.5f*(float)f, 6.5f);
        agri_soil_add_layer(&soils[f], 60.0f, clay_base+8.0f, 35.0f, 57.0f-clay_base, 1.5f, 6.8f);
    }

    /* Weather data */
    agri_weather_series_t weather;
    agri_weather_init(&weather);
    weather.start_year = 2025; weather.start_doy = 91;
    for (int d = 0; d < 150; d++) {
        int doy = 91 + d, month, day;
        agri_date_from_doy(2025, doy, &month, &day);
        float s = (float)d / 149.0f;
        float bt = 12.0f + 15.0f * sinf(s * 3.14159f);
        float dr = 8.0f + 4.0f * sinf(s * 3.14159f);
        float rain = ((d*7)%10 == 0) ? 12.0f * (1.0f + 0.5f*sinf(s*3.14159f)) : 0.0f;
        float sol = 10.0f + 16.0f * sinf(s * 3.14159f);
        agri_weather_add_day(&weather, 2025, month, day,
                             bt+dr*0.6f, bt-dr*0.4f, rain, sol, 55.0f, 2.0f);
    }

    /* Crops for each field */
    agri_crop_type_t field_crops[] = {AGRI_CROP_MAIZE, AGRI_CROP_WHEAT,
                                       AGRI_CROP_SOYBEAN, AGRI_CROP_POTATO};

    agri_season_config_t configs[4];
    for (int f = 0; f < 4; f++) {
        memset(&configs[f], 0, sizeof(agri_season_config_t));
        configs[f].crop_type = field_crops[f];
        configs[f].planting_doy = 91;
        configs[f].irrigation_efficiency = 0.75f;
    }

    /* Add fields to farm */
    for (int f = 0; f < 4; f++) {
        agri_farm_add_field(&farm, &fields[f], &configs[f], &soils[f], &weather);
    }

    /* Simulate */
    agri_farm_simulate_all(&farm);

    /* ── Dashboard Output ── */
    printf("═══ FIELD SUMMARY ═══\n");
    for (int f = 0; f < farm.num_fields; f++) {
        const agri_crop_t* cp = agri_crop_lookup(farm.seasons[f].config.crop_type);
        printf("  Field %s [%.1f ha]: %s\n", fields[f].field_id,
               fields[f].area_hectares, cp ? cp->name : "?");
        printf("    Yield: %.2f t/ha, Irrig: %.0f mm, Stage: %d\n",
               farm.seasons[f].predicted_yield_ton_ha,
               farm.seasons[f].total_irrigation_mm,
               farm.seasons[f].current.stage);
    }

    printf("\n═══ YIELD COMPARISON ═══\n");
    float max_yield = 0.0f;
    for (int f = 0; f < farm.num_fields; f++)
        if (farm.seasons[f].predicted_yield_ton_ha > max_yield)
            max_yield = farm.seasons[f].predicted_yield_ton_ha;
    for (int f = 0; f < farm.num_fields; f++) {
        const agri_crop_t* cp = agri_crop_lookup(farm.seasons[f].config.crop_type);
        char label[64];
        snprintf(label, sizeof(label), "%s (%.1f ha)", cp->name, fields[f].area_hectares);
        bar_chart(label, farm.seasons[f].predicted_yield_ton_ha, max_yield * 1.2f, 30);
    }

    printf("\n═══ WATER USE ═══\n");
    float max_water = 0.0f;
    for (int f = 0; f < farm.num_fields; f++)
        if (farm.seasons[f].total_irrigation_mm > max_water)
            max_water = farm.seasons[f].total_irrigation_mm;
    for (int f = 0; f < farm.num_fields; f++) {
        const agri_crop_t* cp = agri_crop_lookup(farm.seasons[f].config.crop_type);
        char label[64];
        snprintf(label, sizeof(label), "%s", cp->name);
        bar_chart(label, farm.seasons[f].total_irrigation_mm, max_water * 1.2f, 30);
    }

    printf("\n═══ FERTILIZER PLAN (4R) ═══\n");
    for (int f = 0; f < farm.num_fields; f++) {
        agri_soil_test_t st = {.soil_n_kg_ha=35.0f, .soil_p_kg_ha=20.0f,
            .soil_k_kg_ha=160.0f, .yield_goal_ton_ha=10.0f,
            .previous_crop_n_credit=15.0f, .organic_matter_n_release=12.0f};
        agri_fertilizer_plan_t plan;
        agri_fertilizer_plan(&st, field_crops[f], &plan);
        const agri_crop_t* cp = agri_crop_lookup(field_crops[f]);
        printf("  %s: N=%.0f P=%.0f K=%.0f kg/ha\n",
               cp->name, plan.nitrogen_kg_ha, plan.phosphorus_kg_ha, plan.potassium_kg_ha);
    }

    printf("\n═══ ECONOMIC OUTLOOK ═══\n");
    agri_cost_breakdown_t costs = {
        .seed_cost_per_ha=180.0f, .fertilizer_cost_per_ha=350.0f,
        .pesticide_cost_per_ha=120.0f, .fuel_cost_per_ha=90.0f,
        .labor_cost_per_ha=200.0f, .irrigation_cost_per_mm_ha=0.25f,
        .land_rent_per_ha=600.0f, .insurance_per_ha=75.0f,
        .misc_cost_per_ha=50.0f
    };
    float total_profit = 0.0f;
    for (int f = 0; f < farm.num_fields; f++) {
        agri_economic_analysis_t ea;
        agri_economic_analyze(&farm.seasons[f], &costs, 250.0f, &ea);
        total_profit += ea.net_profit_per_ha * fields[f].area_hectares;
        const agri_crop_t* cp = agri_crop_lookup(field_crops[f]);
        printf("  %s: Net=$%.0f/ha, B/E Price=$%.0f/t\n",
               cp->name, ea.net_profit_per_ha, ea.break_even_price_per_ton);
    }
    printf("  Total farm profit: $%.0f\n", total_profit);

    printf("\n═══ ENVIRONMENTAL SCORECARD ═══\n");
    agri_carbon_farm_t carbon;
    agri_carbon_farm_assess(&farm, &carbon);
    bar_chart("SOC (t/ha)", carbon.soil_organic_carbon_ton_ha, 100.0f, 30);
    bar_chart("Seq (t/ha/yr)", carbon.carbon_sequestration_rate_ton_ha_yr * 10.0f, 20.0f, 30);
    bar_chart("N2O (kgCO2e/ha)", carbon.nitrous_oxide_emissions_kg_co2e_ha / 10.0f, 100.0f, 30);
    printf("  Carbon credits: %d ($%.0f)\n", carbon.carbon_credits_earned, carbon.carbon_credit_value_usd);

    printf("\n════════════════════════════════════════════════════════\n");
    printf("  Farm Summary: %.1f ha, %.1f t, %.0f m3 water, %.0f kg N\n",
           farm.total_area_ha, farm.total_predicted_yield_ton,
           farm.total_water_use_m3, farm.total_nitrogen_kg);
    printf("============================================================\n");

    return 0;
}
