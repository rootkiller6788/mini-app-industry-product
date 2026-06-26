#include "agri_core.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void agri_init(void) {}

agri_gps_t agri_make_gps(double lat, double lon, double elev) {
    agri_gps_t g = { lat, lon, elev };
    return g;
}

double agri_gps_distance_m(const agri_gps_t* a, const agri_gps_t* b) {
    if (!a || !b) return 0.0;
    double R = 6371000.0;
    double lat1 = a->latitude * M_PI / 180.0;
    double lat2 = b->latitude * M_PI / 180.0;
    double dlat = (b->latitude - a->latitude) * M_PI / 180.0;
    double dlon = (b->longitude - a->longitude) * M_PI / 180.0;
    double sa = sin(dlat * 0.5), sb = sin(dlon * 0.5);
    double h = sa * sa + cos(lat1) * cos(lat2) * sb * sb;
    return 2.0 * R * atan2(sqrt(h), sqrt(1.0 - h));
}

double agri_polygon_area_ha(const agri_gps_t* vertices, int n) {
    if (!vertices || n < 3) return 0.0;
    double area = 0.0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        area += (vertices[i].longitude - vertices[j].longitude)
              * (vertices[i].latitude + vertices[j].latitude);
    }
    area = fabs(area) * 0.5;
    double avg_lat = 0.0;
    for (int i = 0; i < n; i++) avg_lat += vertices[i].latitude;
    avg_lat /= (double)n;
    double mpd = 111320.0;
    return area * mpd * mpd * cos(avg_lat * M_PI / 180.0) / 10000.0;
}

void agri_soil_init(agri_soil_profile_t* soil, const char* texture_class) {
    if (!soil) return;
    memset(soil, 0, sizeof(agri_soil_profile_t));
    if (texture_class) {
        strncpy(soil->usda_texture_class, texture_class, 31);
        soil->usda_texture_class[31] = 0;
    }
}

void agri_soil_add_layer(agri_soil_profile_t* soil, float depth_cm,
                          float clay_pct, float silt_pct, float sand_pct,
                          float organic_matter_pct, float ph) {
    if (!soil || soil->num_layers >= AGRI_MAX_SOIL_LAYERS) return;
    agri_soil_layer_t* l = &soil->layers[soil->num_layers];
    memset(l, 0, sizeof(agri_soil_layer_t));
    l->depth_cm = depth_cm;
    l->clay_pct = clay_pct;
    l->silt_pct = silt_pct;
    l->sand_pct = sand_pct;
    l->organic_matter_pct = organic_matter_pct;
    l->ph = ph;
    /* Rawls et al. (1982) pedotransfer functions for θ_FC and θ_WP.
     * S=sand%, C=clay%, OM=organic matter%.
     * θ_FC = 0.2576 - 0.0020*S + 0.0036*C + 0.0299*OM
     * θ_WP = 0.0260 + 0.0050*C + 0.0158*OM
     * Validated for USDA soil texture classes. */
    float theta_33  = 0.2576f - 0.0020f * sand_pct + 0.0036f * clay_pct
                     + 0.0299f * organic_matter_pct;
    float theta_1500 = 0.0260f + 0.0050f * clay_pct
                      + 0.0158f * organic_matter_pct;
    /* Physical plausibility bounds */
    if (theta_33  < 0.05f) theta_33  = 0.05f;
    if (theta_33  > 0.55f) theta_33  = 0.55f;
    if (theta_1500 < 0.01f) theta_1500 = 0.01f;
    if (theta_1500 > 0.40f) theta_1500 = 0.40f;
    if (theta_1500 >= theta_33) theta_1500 = theta_33 * 0.45f;
    l->field_capacity_m3_m3 = theta_33;
    l->wilting_point_m3_m3 = theta_1500;
    l->bulk_density_g_cm3 = 1.66f - 0.318f * sqrtf(organic_matter_pct);
    if (l->bulk_density_g_cm3 < 0.9f) l->bulk_density_g_cm3 = 0.9f;
    if (l->bulk_density_g_cm3 > 1.8f) l->bulk_density_g_cm3 = 1.8f;
    l->cation_exchange_capacity = 0.5f * clay_pct + 2.0f * organic_matter_pct;
    soil->total_depth_cm = depth_cm;
    soil->num_layers++;
}

float agri_soil_awc_mm(const agri_soil_profile_t* soil, float root_depth_cm) {
    if (!soil || soil->num_layers == 0) return 0.0f;
    float awc = 0.0f;
    for (int i = 0; i < soil->num_layers; i++) {
        float top = (i == 0) ? 0.0f : soil->layers[i-1].depth_cm;
        float bot = soil->layers[i].depth_cm;
        if (top >= root_depth_cm) break;
        if (bot > root_depth_cm) bot = root_depth_cm;
        float fc = soil->layers[i].field_capacity_m3_m3;
        float wp = soil->layers[i].wilting_point_m3_m3;
        awc += (fc - wp) * (bot - top) * 10.0f;
    }
    return awc;
}

float agri_soil_water_content_at_fc(const agri_soil_profile_t* soil, float rd) {
    if (!soil || soil->num_layers == 0) return 0.0f;
    float total = 0.0f, da = 0.0f;
    for (int i = 0; i < soil->num_layers; i++) {
        float lt = (i == 0) ? 0.0f : soil->layers[i-1].depth_cm;
        float lb = soil->layers[i].depth_cm;
        if (lt >= rd) break;
        if (lb > rd) lb = rd;
        total += soil->layers[i].field_capacity_m3_m3 * (lb - lt);
        da = lb;
    }
    return da > 0.0f ? total * 10.0f : 0.0f;
}

float agri_soil_water_content_at_wp(const agri_soil_profile_t* soil, float rd) {
    if (!soil || soil->num_layers == 0) return 0.0f;
    float total = 0.0f, da = 0.0f;
    for (int i = 0; i < soil->num_layers; i++) {
        float lt = (i == 0) ? 0.0f : soil->layers[i-1].depth_cm;
        float lb = soil->layers[i].depth_cm;
        if (lt >= rd) break;
        if (lb > rd) lb = rd;
        total += soil->layers[i].wilting_point_m3_m3 * (lb - lt);
        da = lb;
    }
    return da > 0.0f ? total * 10.0f : 0.0f;
}

void agri_weather_init(agri_weather_series_t* ws) {
    if (!ws) return;
    memset(ws, 0, sizeof(agri_weather_series_t));
}

void agri_weather_add_day(agri_weather_series_t* ws, int year, int month,
                           int day, float tmax, float tmin, float rain,
                           float solar, float humidity, float wind) {
    if (!ws || ws->num_days >= AGRI_MAX_WEATHER_DAYS) return;
    agri_weather_day_t* w = &ws->days[ws->num_days];
    w->year = year; w->month = month; w->day = day;
    w->temp_max_c = tmax; w->temp_min_c = tmin;
    w->temp_avg_c = (tmax + tmin) * 0.5f;
    w->rainfall_mm = rain;
    w->solar_radiation_MJ_m2 = solar;
    w->humidity_pct = humidity;
    w->wind_speed_m_s = wind;
    ws->num_days++;
}

/*
 * FAO-56 Penman-Monteith Reference Evapotranspiration (ET0).
 * Standard method for computing reference crop water requirements.
 * Reference: Allen et al. (1998) FAO Irrigation Paper 56.
 */
float agri_weather_et0_penman_monteith(const agri_weather_day_t* w,
                                        float elevation_m, float latitude) {
    if (!w) return 0.0f;
    float T = w->temp_avg_c, Tmax = w->temp_max_c, Tmin = w->temp_min_c;
    float RH = w->humidity_pct, Rs = w->solar_radiation_MJ_m2;
    float u2 = w->wind_speed_m_s > 0.1f ? w->wind_speed_m_s : 0.5f;

    float P = 101.3f * powf((293.0f - 0.0065f * elevation_m) / 293.0f, 5.26f);
    float gamma = 0.000665f * P;
    float es_max = 0.6108f * expf(17.27f * Tmax / (Tmax + 237.3f));
    float es_min = 0.6108f * expf(17.27f * Tmin / (Tmin + 237.3f));
    float es = (es_max + es_min) * 0.5f;
    float ea = es * RH * 0.01f;
    float delta = 4098.0f * (0.6108f * expf(17.27f * T / (T + 237.3f)))
                / ((T + 237.3f) * (T + 237.3f));

    float lat_rad = latitude * (float)M_PI / 180.0f;
    float dr = 1.0f + 0.033f * cosf(2.0f * (float)M_PI * (float)w->day / 365.0f);
    float decl = 0.409f * sinf(2.0f * (float)M_PI * (float)w->day / 365.0f - 1.39f);
    float om_s = acosf(-tanf(lat_rad) * tanf(decl));
    float Ra = 24.0f * 60.0f / (float)M_PI * 0.082f * dr
             * (om_s * sinf(lat_rad) * sinf(decl)
              + cosf(lat_rad) * cosf(decl) * sinf(om_s));

    float Rns = 0.77f * Rs;
    float sigma = 4.903e-9f;
    float Rnl = sigma * ((powf(Tmax+273.16f,4)+powf(Tmin+273.16f,4))*0.5f)
               * (0.34f - 0.14f*sqrtf(ea))
               * (1.35f*Rs/(Ra*0.75f+1e-6f) - 0.35f);
    if (Rnl < 0.0f) Rnl = 0.0f;
    float Rn = Rns - Rnl;

    float num = 0.408f*delta*(Rn-0.0f)
              + gamma*(900.0f/(T+273.0f))*u2*(es-ea);
    float den = delta + gamma*(1.0f + 0.34f*u2);
    return den > 1e-6f ? num/den : 0.0f;
}

float agri_weather_gdd_accum(const agri_weather_series_t* ws,
                              int start_doy, int end_doy,
                              float tbase, float tupper) {
    if (!ws) return 0.0f;
    float gdd = 0.0f;
    for (int i = 0; i < ws->num_days; i++) {
        int doy = agri_doy_from_date(ws->days[i].year,
                                      ws->days[i].month, ws->days[i].day);
        if (doy < start_doy || doy > end_doy) continue;
        float tx = ws->days[i].temp_max_c, tn = ws->days[i].temp_min_c;
        if (tx > tupper) tx = tupper;
        if (tn < tbase) tn = tbase;
        if (tx < tbase) tx = tbase;
        gdd += (tx + tn) * 0.5f - tbase;
    }
    return gdd > 0.0f ? gdd : 0.0f;
}

int agri_doy_from_date(int year, int month, int day) {
    static const int dim[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    int leap = (year%4==0 && (year%100!=0 || year%400==0));
    int doy = day;
    for (int m = 1; m < month; m++) {
        doy += dim[m];
        if (m == 2 && leap) doy++;
    }
    return doy;
}

void agri_date_from_doy(int year, int doy, int* month, int* day) {
    static const int dim[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    int leap = (year%4==0 && (year%100!=0 || year%400==0));
    int m = 1;
    while (m <= 12) {
        int days = dim[m] + ((m==2 && leap)?1:0);
        if (doy <= days) break;
        doy -= days; m++;
    }
    if (month) *month = m;
    if (day) *day = doy;
}

static const agri_crop_t crop_db[] = {
    { AGRI_CROP_WHEAT, "Wheat", 8.0f,120, 0.30f,1.15f,0.30f, 150.0f,15.0f,25.0f, 200,40,180,0.45f },
    { AGRI_CROP_MAIZE, "Maize",10.0f,140, 0.30f,1.20f,0.35f, 180.0f,18.0f,32.0f, 250,50,200,0.50f },
    { AGRI_CROP_RICE,  "Rice",  6.0f,130, 1.05f,1.20f,0.70f,  60.0f,20.0f,35.0f, 120,25,140,0.50f },
    { AGRI_CROP_SOYBEAN,"Soybean",3.5f,110,0.40f,1.15f,0.50f,120.0f,20.0f,30.0f, 220,40,120,0.40f },
    { AGRI_CROP_POTATO,"Potato",40.0f,100,0.50f,1.15f,0.75f, 60.0f,15.0f,25.0f, 180,60,250,0.75f },
    { AGRI_CROP_CUSTOM,"Custom",5.0f,120,0.50f,1.15f,0.50f, 100.0f,15.0f,30.0f, 150,40,150,0.45f },
};

const agri_crop_t* agri_crop_lookup(agri_crop_type_t type) {
    int n = (int)(sizeof(crop_db)/sizeof(crop_db[0]));
    if (type >= 0 && (int)type < n) return &crop_db[type];
    /* Fallback: return last entry in DB (CUSTOM crop type)
     * Note: AGRI_CROP_CUSTOM enum value (9) != crop_db index (5),
     * so we index by DB position, not enum value, for safety. */
    return &crop_db[n - 1];
}

void agri_season_init(agri_season_context_t* ctx,
                       const agri_season_config_t* cfg) {
    if (!ctx || !cfg) return;
    memset(ctx, 0, sizeof(agri_season_context_t));
    ctx->config = *cfg;
}

void agri_season_free(agri_season_context_t* ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(agri_season_context_t));
}

/* Water stress factor (FAO-56 approach) */
static float water_stress(float depl, float taw, float p) {
    if (taw <= 0.0f) return 1.0f;
    float raw = p * taw;
    if (depl <= raw) return 1.0f;
    float ks = (taw - depl) / (taw - raw);
    if (ks < 0.0f) ks = 0.0f;
    if (ks > 1.0f) ks = 1.0f;
    return ks;
}

/* Temperature stress function */
static float temp_stress(float tavg, float tmin_opt, float tmax_opt) {
    if (tavg >= tmin_opt && tavg <= tmax_opt) return 1.0f;
    if (tavg < 0.0f) return 0.0f;
    if (tavg < tmin_opt) return tavg / tmin_opt;
    float tkill = tmax_opt + 15.0f;
    if (tavg > tkill) return 0.0f;
    return (tkill - tavg) / (tkill - tmax_opt);
}

/*
 * agri_season_step - Single-day crop growth simulation.
 * Uses RUE (Radiation Use Efficiency) model with water and temp stress.
 * Reference: Jones et al. (2003) DSSAT cropping system model.
 */
void agri_season_step(agri_season_context_t* ctx) {
    if (!ctx || ctx->complete) return;
    int idx = ctx->days_simulated;
    if (idx >= ctx->weather.num_days) { ctx->complete = true; return; }

    agri_weather_day_t wd = ctx->weather.days[idx];
    const agri_crop_t* crop = agri_crop_lookup(ctx->config.crop_type);
    agri_daily_state_t* s = &ctx->current;

    int doy = agri_doy_from_date(wd.year, wd.month, wd.day);
    float gdd = agri_weather_gdd_accum(&ctx->weather,
                ctx->config.planting_doy, doy, 0.0f, 35.0f);
    float total_gdd = (float)crop->days_to_maturity * 15.0f;
    float prog = total_gdd > 0.0f ? gdd / total_gdd : 0.0f;

    if (prog < 0.05f) s->stage = AGRI_STAGE_GERMINATION;
    else if (prog < 0.40f) s->stage = AGRI_STAGE_VEGETATIVE;
    else if (prog < 0.65f) s->stage = AGRI_STAGE_FLOWERING;
    else if (prog < 0.90f) s->stage = AGRI_STAGE_GRAIN_FILL;
    else if (prog < 1.0f) s->stage = AGRI_STAGE_MATURITY;
    else s->stage = AGRI_STAGE_HARVEST;

    float rf = prog > 1.0f ? 1.0f : prog;
    s->root_depth_cm = 10.0f + (crop->root_depth_max_cm - 10.0f) * rf * rf;

    float lai_max = 6.0f;
    if (prog < 0.05f) s->lai = 0.1f;
    else if (prog < 0.5f)
        s->lai = 0.1f + (lai_max-0.1f)*((prog-0.05f)/0.45f);
    else
        s->lai = lai_max*(1.0f - 0.3f*((prog-0.5f)/0.5f)*((prog-0.5f)/0.5f));

    float kc;
    if (prog < 0.1f) kc = crop->kc_initial;
    else if (prog < 0.5f)
        kc = crop->kc_initial + (crop->kc_mid - crop->kc_initial)*((prog-0.1f)/0.4f);
    else if (prog < 0.8f) kc = crop->kc_mid;
    else kc = crop->kc_mid + (crop->kc_end - crop->kc_mid)*((prog-0.8f)/0.2f);

    float et0 = agri_weather_et0_penman_monteith(&wd,
        (float)ctx->field.vertices[0].elevation_m,
        (float)ctx->field.vertices[0].latitude);
    s->etc_actual_mm = et0 * kc;

    float taw = agri_soil_awc_mm(&ctx->soil, s->root_depth_cm);
    float depl = taw>0.0f ? taw*(1.0f - s->soil_water_mm/(taw+1e-6f)) : 0.0f;
    s->water_stress_factor = water_stress(depl, taw, 0.55f);

    float irr = 0.0f;
    if (s->water_stress_factor < 0.9f && ctx->config.irrigation_efficiency > 0.0f) {
        float deficit = taw*0.5f - (taw - depl);
        if (deficit > 0.0f) {
            irr = deficit / ctx->config.irrigation_efficiency;
            if (irr > 40.0f) irr = 40.0f;
        }
    }
    s->soil_water_mm += wd.rainfall_mm + irr*ctx->config.irrigation_efficiency
                       - s->etc_actual_mm*s->water_stress_factor;
    if (s->soil_water_mm > taw) s->soil_water_mm = taw;
    if (s->soil_water_mm < 0.0f) s->soil_water_mm = 0.0f;

    s->temperature_stress_factor = temp_stress(wd.temp_avg_c,
        crop->optimal_temp_min_c, crop->optimal_temp_max_c);

    float rue = (crop->type == AGRI_CROP_MAIZE) ? 3.5f : 2.8f;
    float fPAR = 0.95f*(1.0f - expf(-0.65f*s->lai));
    float PAR = wd.solar_radiation_MJ_m2*0.48f;
    float stress = (s->water_stress_factor < s->temperature_stress_factor)
                   ? s->water_stress_factor : s->temperature_stress_factor;
    s->biomass_growth_kg_ha_day = rue * fPAR * PAR * stress;
    s->biomass_accum_kg_ha += s->biomass_growth_kg_ha_day;
    if (s->stage >= AGRI_STAGE_GRAIN_FILL)
        s->yield_accum_kg_ha += s->biomass_growth_kg_ha_day * crop->harvest_index;

    ctx->history[idx] = *s;
    ctx->history[idx].doy = doy;
    ctx->total_irrigation_mm += irr;
    ctx->days_simulated++;
    if (s->stage >= AGRI_STAGE_HARVEST) ctx->complete = true;
}

void agri_season_simulate(agri_season_context_t* ctx) {
    if (!ctx) return;
    while (!ctx->complete) agri_season_step(ctx);
    ctx->predicted_yield_ton_ha = ctx->current.yield_accum_kg_ha * 0.001f;
}

float agri_predict_yield(const agri_season_context_t* ctx) {
    if (!ctx) return 0.0f;
    return ctx->current.yield_accum_kg_ha * 0.001f;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L5: Irrigation Scheduling Optimizer
 *
 * Implements MAD-based (Management Allowed Depletion) irrigation scheduling.
 * Reference: FAO Irrigation Paper 56, Chapter 8 — "Irrigation Scheduling"
 *
 * Algorithm: For each day, compute soil water deficit. If deficit exceeds
 * MAD fraction of TAW (total available water), schedule irrigation to refill
 * to field capacity. Optimizes water use while maintaining yield.
 *
 * Complexity: O(n) for n days in growing season.
 * ═══════════════════════════════════════════════════════════════════════════ */

void agri_irrigation_optimize(const agri_season_context_t* season,
                               agri_irrigation_schedule_t* schedule,
                               float mad_fraction) {
    if (!season || !schedule) return;
    memset(schedule, 0, sizeof(agri_irrigation_schedule_t));
    if (mad_fraction <= 0.0f || mad_fraction > 1.0f) mad_fraction = 0.50f;

    float cum_etc = 0.0f, cum_rain = 0.0f;
    for (int i = 0; i < season->days_simulated; i++) {
        const agri_daily_state_t* ds = &season->history[i];
        cum_etc += ds->etc_actual_mm;
        cum_rain += season->weather.days[i].rainfall_mm;

        float taw = ds->root_depth_cm > 0.0f ?
            agri_soil_awc_mm(&season->soil, ds->root_depth_cm) : 60.0f;
        float deficit = cum_etc - cum_rain - season->total_irrigation_mm;
        if (deficit < 0.0f) deficit = 0.0f;

        float mad_threshold = taw * mad_fraction;
        if (deficit > mad_threshold) {
            int idx = schedule->num_events;
            if (idx >= AGRI_IRR_MAX_SCHEDULE) break;
            schedule->events[idx].doy = ds->doy;
            schedule->events[idx].soil_moisture_deficit_mm = deficit;
            schedule->events[idx].recommended_irrigation_mm = deficit;
            schedule->events[idx].etc_since_last_mm = 0.0f;
            schedule->events[idx].rainfall_since_last_mm = 0.0f;
            schedule->total_water_applied_mm += deficit;
            cum_etc = 0.0f;
            cum_rain = 0.0f;
            schedule->num_events++;
        }
    }
    /* Estimate water saved vs. full irrigation (every 3 days) */
    float full_irr = (float)(season->days_simulated / 3) * 30.0f;
    schedule->water_saved_vs_full_mm = full_irr - schedule->total_water_applied_mm;
}

/*
 * Deficit Irrigation Strategy:
 * Deliberately under-irrigate by target_reduction_pct to maximize
 * water productivity (yield per unit water) at cost of some yield.
 * Key in water-scarce regions.
 * Reference: Fereres & Soriano (2007) "Deficit irrigation for reducing
 * agricultural water use", Journal of Experimental Botany.
 */
float agri_irrigation_deficit_irrigation(const agri_season_context_t* ctx,
                                          float target_reduction_pct) {
    if (!ctx) return 0.0f;
    float total_etc = 0.0f;
    for (int i = 0; i < ctx->days_simulated; i++)
        total_etc += ctx->history[i].etc_actual_mm;
    float target_etc = total_etc * (1.0f - target_reduction_pct * 0.01f);
    /* Stewart's water production function: 1 - Ya/Ym = Ky * (1 - ETa/ETm) */
    float ky = 1.0f; /* Yield response factor, ~1.0 for many crops */
    float rel_yield = 1.0f - ky * (1.0f - target_etc / (total_etc + 1e-6f));
    return rel_yield > 0.0f ? rel_yield * 100.0f : 0.0f;
}

float agri_irrigation_water_productivity(const agri_season_context_t* ctx) {
    if (!ctx || ctx->total_irrigation_mm <= 0.0f) return 0.0f;
    float yield_kg = ctx->predicted_yield_ton_ha * 1000.0f;
    return yield_kg / (ctx->total_irrigation_mm + ctx->current.etc_actual_mm);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L5: Pest & Disease Risk Modeling
 *
 * Uses degree-day accumulation and environmental thresholds to predict
 * pest/disease outbreaks. Implements the "EPIdemic Simulation" model
 * framework from Zadoks & Schein (1979) "Epidemiology and Plant Disease
 * Management".
 *
 * Reference: Magarey et al. (2005) "Plant disease warning systems"
 * ═══════════════════════════════════════════════════════════════════════════ */

void agri_pest_init(agri_pest_predictor_t* pp, agri_crop_type_t crop) {
    if (!pp) return;
    memset(pp, 0, sizeof(agri_pest_predictor_t));
    pp->target_crop = crop;
}

int agri_pest_add_model(agri_pest_predictor_t* pp, agri_pest_type_t type,
                         const char* name, float tmin, float tmax,
                         float hum_thresh, float base_temp) {
    if (!pp || !name || pp->num_models >= AGRI_PEST_MAX_MODELS) return -1;
    agri_pest_model_t* m = &pp->models[pp->num_models];
    m->type = type;
    strncpy(m->name, name, AGRI_MAX_NAME - 1);
    m->temp_optimal_min_c = tmin;
    m->temp_optimal_max_c = tmax;
    m->humidity_threshold_pct = hum_thresh;
    m->base_temp_c = base_temp;
    m->leaf_wetness_hours_threshold = 6.0f;
    m->sporulation_rate = 0.05f;
    pp->num_models++;
    return pp->num_models - 1;
}

/* Compute pest risk based on temperature suitability and humidity */
static float pest_environmental_suitability(const agri_pest_model_t* m,
                                             const agri_weather_day_t* w) {
    float temp_score;
    if (w->temp_avg_c >= m->temp_optimal_min_c && w->temp_avg_c <= m->temp_optimal_max_c)
        temp_score = 1.0f;
    else if (w->temp_avg_c < m->temp_optimal_min_c && w->temp_avg_c > 0.0f)
        temp_score = w->temp_avg_c / m->temp_optimal_min_c;
    else if (w->temp_avg_c > m->temp_optimal_max_c)
        temp_score = 1.0f - (w->temp_avg_c - m->temp_optimal_max_c) / 15.0f;
    else
        temp_score = 0.0f;
    if (temp_score < 0.0f) temp_score = 0.0f;
    if (temp_score > 1.0f) temp_score = 1.0f;

    float hum_score = w->humidity_pct >= m->humidity_threshold_pct ? 1.0f
                     : w->humidity_pct / (m->humidity_threshold_pct + 1e-6f);
    if (hum_score > 1.0f) hum_score = 1.0f;

    return 0.6f * temp_score + 0.4f * hum_score;
}

void agri_pest_predict(const agri_pest_predictor_t* pp,
                       const agri_weather_series_t* weather,
                       agri_pest_risk_event_t* risks, int* num_risks) {
    if (!pp || !weather || !risks || !num_risks) return;
    *num_risks = 0;

    for (int m = 0; m < pp->num_models; m++) {
        const agri_pest_model_t* model = &pp->models[m];
        float cumulative_risk = 0.0f;
        int consecutive_favorable = 0;
        int max_risk_doy = 0;
        float max_risk_val = 0.0f;

        for (int d = 1; d < weather->num_days; d++) {
            float env = pest_environmental_suitability(model, &weather->days[d]);
            if (env > 0.6f) {
                consecutive_favorable++;
                float gdd = agri_weather_gdd_accum(weather, weather->start_doy,
                    weather->start_doy + d, model->base_temp_c, 35.0f);
                float risk = env * (1.0f - expf(-gdd / 100.0f))
                           * (1.0f - expf(-(float)consecutive_favorable * model->sporulation_rate));
                cumulative_risk += risk * 0.1f;
                if (risk > max_risk_val) {
                    max_risk_val = risk;
                    max_risk_doy = weather->start_doy + d;
                }
            } else {
                consecutive_favorable = 0;
            }
        }

        /* Estimate yield loss: 0-30% based on risk */
        float severity = cumulative_risk > 1.0f ? 1.0f : cumulative_risk;
        float yield_loss = severity * 30.0f;

        if (*num_risks < AGRI_PEST_MAX_EVENTS) {
            risks[*num_risks].doy = max_risk_doy;
            risks[*num_risks].risk_index = severity;
            risks[*num_risks].severity_estimate = severity * 10.0f;
            risks[*num_risks].yield_loss_pct = yield_loss;
            snprintf(risks[*num_risks].recommendation, AGRI_MAX_NAME,
                     "%s risk %.0f%% — monitor %s",
                     severity > 0.6f ? "HIGH" : severity > 0.3f ? "MODERATE" : "LOW",
                     severity * 100.0f,
                     severity > 0.5f ? "and apply treatment" : "field conditions");
            (*num_risks)++;
        }
    }
}

float agri_pest_degree_day_risk(const agri_pest_model_t* model,
                                 const agri_weather_series_t* weather,
                                 int start_doy, int end_doy) {
    if (!model || !weather) return 0.0f;
    float gdd = agri_weather_gdd_accum(weather, start_doy, end_doy,
                                        model->base_temp_c, 35.0f);
    float threshold = (float)model->degree_day_accumulation;
    if (threshold <= 0.0f) return 0.0f;
    float ratio = gdd / threshold;
    return ratio > 1.0f ? 1.0f : ratio;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L6: Multi-Field Farm Management System
 *
 * Manages multiple fields on a single farm, each with its own soil,
 * weather, and crop configuration. Implements whole-farm optimization.
 *
 * Core optimization: Allocate crops to fields to maximize profit
 * subject to water budget and rotation constraints.
 *
 * Reference: Robertson et al. (2015) "Whole-farm models: a review"
 *            MIT 15.053: Optimization Methods in Business Analytics
 * ═══════════════════════════════════════════════════════════════════════════ */

void agri_farm_init(agri_farm_t* farm, const char* name) {
    if (!farm) return;
    memset(farm, 0, sizeof(agri_farm_t));
    if (name) strncpy(farm->farm_name, name, AGRI_MAX_NAME - 1);
}

int agri_farm_add_field(agri_farm_t* farm, const agri_field_t* field,
                          const agri_season_config_t* config,
                          const agri_soil_profile_t* soil,
                          const agri_weather_series_t* weather) {
    if (!farm || !field || farm->num_fields >= AGRI_MAX_FIELDS_PER_FARM) return -1;
    int idx = farm->num_fields;
    farm->fields[idx] = *field;
    farm->total_area_ha += field->area_hectares > 0.0 ? field->area_hectares : 1.0;
    if (config) {
        agri_season_init(&farm->seasons[idx], config);
        if (soil) farm->seasons[idx].soil = *soil;
        if (weather) farm->seasons[idx].weather = *weather;
        farm->seasons[idx].field = *field;
    }
    farm->num_fields++;
    return idx;
}

void agri_farm_simulate_all(agri_farm_t* farm) {
    if (!farm) return;
    farm->total_predicted_yield_ton = 0.0;
    farm->total_water_use_m3 = 0.0;
    farm->total_nitrogen_kg = 0.0;
    for (int i = 0; i < farm->num_fields; i++) {
        agri_season_context_t* ctx = &farm->seasons[i];
        if (ctx->weather.num_days == 0) continue;
        agri_season_simulate(ctx);
        farm->total_predicted_yield_ton += ctx->predicted_yield_ton_ha
                                           * farm->fields[i].area_hectares;
        farm->total_water_use_m3 += ctx->total_irrigation_mm
                                     * farm->fields[i].area_hectares * 10.0;
        farm->total_nitrogen_kg += ctx->total_fertilizer_n_kg_ha
                                    * farm->fields[i].area_hectares;
    }
}

/* Greedy crop-to-field allocation optimizing profit */
void agri_farm_optimize_planting(agri_farm_t* farm,
                                   const agri_crop_t* crops, int num_crops) {
    if (!farm || !crops || num_crops <= 0) return;
    for (int f = 0; f < farm->num_fields; f++) {
        int best_crop = 0;
        float best_profit = -1e9f;
        for (int c = 0; c < num_crops && c < 6; c++) {
            float yield = crops[c].base_yield_ton_ha;
            float revenue = yield * 250.0f; /* Assume $250/ton */
            float cost = 500.0f;              /* Assume $500/ha input cost */
            float profit = revenue - cost;
            /* Soil suitability adjustment */
            float clay = 20.0f;
            if (farm->seasons[f].soil.num_layers > 0)
                clay = farm->seasons[f].soil.layers[0].clay_pct;
            if (crops[c].type == AGRI_CROP_RICE && clay < 30.0f) profit *= 0.5f;
            if (crops[c].type == AGRI_CROP_POTATO && clay > 35.0f) profit *= 0.7f;
            if (profit > best_profit) {
                best_profit = profit;
                best_crop = c;
            }
        }
        farm->seasons[f].config.crop_type = crops[best_crop].type;
    }
}

double agri_farm_profit_estimate(const agri_farm_t* farm,
                                  double price_per_ton, double cost_per_ha) {
    if (!farm) return 0.0;
    double revenue = farm->total_predicted_yield_ton * price_per_ton;
    double cost = farm->total_area_ha * cost_per_ha;
    return revenue - cost;
}

void agri_farm_summary(const agri_farm_t* farm) {
    if (!farm) return;
    printf("Farm: %s\n", farm->farm_name);
    printf("  Fields: %d | Area: %.1f ha\n", farm->num_fields, farm->total_area_ha);
    printf("  Predicted Yield: %.1f tons\n", farm->total_predicted_yield_ton);
    printf("  Water Use: %.0f m³\n", farm->total_water_use_m3);
    printf("  Nitrogen: %.0f kg\n", farm->total_nitrogen_kg);
    for (int i = 0; i < farm->num_fields; i++) {
        const agri_season_context_t* s = &farm->seasons[i];
        const agri_crop_t* crop = agri_crop_lookup(s->config.crop_type);
        printf("  Field %d [%.1f ha]: %s yield=%.2f t/ha irrig=%.0f mm\n",
               i, farm->fields[i].area_hectares > 0.0 ? farm->fields[i].area_hectares : 1.0,
               crop ? crop->name : "Unknown",
               s->predicted_yield_ton_ha, s->total_irrigation_mm);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L4: Universal Soil Loss Equation (USLE)
 *
 * A = R × K × LS × C × P
 * A = average annual soil loss (ton/ha/yr)
 *
 * Wischmeier & Smith (1978), Renard et al. (1997) — RUSLE
 * USDA Agricultural Handbook No. 537 / No. 703
 *
 * L4 Theorem: Soil conservation — soil formation rate ≈ 0.1-1 ton/ha/yr.
 * Sustainable agriculture requires A < T (tolerable soil loss),
 * typically T = 2-12 ton/ha/yr for most soils.
 * ═══════════════════════════════════════════════════════════════════════════ */

float agri_usle_soil_loss_ton_ha_yr(const agri_usle_factors_t* factors) {
    if (!factors) return 0.0f;
    return factors->rainfall_erosivity_R *
           factors->soil_erodibility_K *
           factors->slope_length_steepness_LS *
           factors->cover_management_C *
           factors->support_practice_P;
}

/* Compute R factor from daily rainfall data
 * R = sum(EI30), where EI30 = kinetic energy × max 30-min intensity
 * Simplified: R ≈ 0.5 × total_annual_rainfall_mm for temperate regions */
float agri_usle_compute_R(const agri_weather_series_t* weather) {
    if (!weather) return 0.0f;
    float total_rain = 0.0f;
    int storm_count = 0;
    float max_daily = 0.0f;
    for (int i = 0; i < weather->num_days; i++) {
        float rain = weather->days[i].rainfall_mm;
        total_rain += rain;
        if (rain > 12.7f) storm_count++;  /* Storms > 0.5 inch */
        if (rain > max_daily) max_daily = rain;
    }
    /* Wischmeier's R-factor approximation using modified Fournier index */
    float mfi = 0.0f;
    for (int i = 0; i < weather->num_days; i++) {
        float p = weather->days[i].rainfall_mm;
        if (p > 0.0f) mfi += p * p / (total_rain + 1e-6f);
    }
    float R = 0.0483f * total_rain + 1.48f * mfi - 0.72f;
    return R > 0.0f ? R : total_rain * 0.5f;
}

/* K factor based on soil texture and organic matter
 * Triangular approximation of the Wischmeier nomograph */
float agri_usle_compute_K(const agri_soil_profile_t* soil) {
    if (!soil || soil->num_layers == 0) return 0.03f;
    const agri_soil_layer_t* top = &soil->layers[0];
    float M = (100.0f - top->clay_pct) * (top->silt_pct + top->sand_pct * 0.1f) * 0.01f;
    float om = top->organic_matter_pct;
    float K = 2.1e-4f * (12.0f - om) * powf(M, 1.14f) * 1e-4f
            + 3.25f * (2.0f - 0) * 1e-3f
            + 2.5f * (3.0f - 0) * 1e-3f;
    /* Simplified: translate to 0.01-0.07 range */
    float clay = top->clay_pct * 0.01f;
    float silt = top->silt_pct * 0.01f;
    K = 0.027f * clay + 0.033f * silt - 0.003f * om + 0.015f;
    if (K < 0.005f) K = 0.005f;
    if (K > 0.07f) K = 0.07f;
    return K;
}

/* LS factor: slope length and steepness */
float agri_usle_compute_LS(float slope_length_m, float slope_pct) {
    float beta = (sinf(slope_pct * 0.01f) / 0.0896f)
               / (3.0f * powf(sinf(slope_pct * 0.01f), 0.8f) + 0.56f);
    float m = beta / (1.0f + beta);
    if (m > 0.5f) m = 0.5f;
    float L = powf(slope_length_m / 22.13f, m);
    float S;
    if (slope_pct < 9.0f)
        S = 10.8f * sinf(atanf(slope_pct * 0.01f)) + 0.03f;
    else
        S = 16.8f * sinf(atanf(slope_pct * 0.01f)) - 0.5f;
    return L * S;
}

float agri_usle_compute_C(agri_crop_type_t crop, agri_growth_stage_t stage) {
    switch (stage) {
        case AGRI_STAGE_DORMANT:     return 0.45f;
        case AGRI_STAGE_GERMINATION: return 0.60f;
        case AGRI_STAGE_VEGETATIVE:  return 0.25f;
        case AGRI_STAGE_FLOWERING:   return 0.15f;
        case AGRI_STAGE_GRAIN_FILL:  return 0.10f;
        case AGRI_STAGE_MATURITY:    return 0.05f;
        case AGRI_STAGE_HARVEST:     return 0.35f;
        default: break;
    }
    /* Crop-specific adjustment */
    if (crop == AGRI_CROP_MAIZE)    return 0.20f;
    if (crop == AGRI_CROP_WHEAT)    return 0.12f;
    if (crop == AGRI_CROP_SOYBEAN)  return 0.30f;
    return 0.15f;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L7: Precision Fertilizer Application
 *
 * Implements the 4R Nutrient Stewardship framework:
 * Right Source, Right Rate, Right Time, Right Place.
 *
 * Reference: IPNI 4R Plant Nutrition Manual (2012)
 *            Stanford's yield-goal method for N recommendation
 *            N_rec = (yield_goal × N_uptake_per_ton) - soil_N_credit
 * ═══════════════════════════════════════════════════════════════════════════ */

void agri_fertilizer_plan(const agri_soil_test_t* test,
                           agri_crop_type_t crop,
                           agri_fertilizer_plan_t* plan) {
    if (!test || !plan) return;
    memset(plan, 0, sizeof(agri_fertilizer_plan_t));
    const agri_crop_t* cp = agri_crop_lookup(crop);
    if (!cp) return;

    /* Nitrogen: yield-goal method */
    float yield_N_required = test->yield_goal_ton_ha * cp->n_uptake_kg_ha
                             / cp->base_yield_ton_ha;
    float n_available = test->soil_n_kg_ha + test->previous_crop_n_credit
                       + test->organic_matter_n_release;
    plan->nitrogen_kg_ha = yield_N_required - n_available;
    if (plan->nitrogen_kg_ha < 0.0f) plan->nitrogen_kg_ha = 0.0f;
    /* Efficiency adjustment: assume 60% recovery */
    if (plan->nitrogen_kg_ha > 0.0f) plan->nitrogen_kg_ha /= 0.60f;

    /* Phosphorus: build-up and maintenance approach (Bray P1) */
    float p_critical = 15.0f; /* mg/kg */
    if (test->soil_p_kg_ha < p_critical * 2.0f) {
        plan->phosphorus_kg_ha = (p_critical * 2.0f - test->soil_p_kg_ha) * 2.29f
                                 * 0.35f;
    }
    plan->phosphorus_kg_ha += cp->p_uptake_kg_ha;

    /* Potassium: sufficiency approach */
    float k_critical = 120.0f;
    if (test->soil_k_kg_ha < k_critical * 2.0f) {
        plan->potassium_kg_ha = (k_critical * 2.0f - test->soil_k_kg_ha) * 1.2f;
    }
    plan->potassium_kg_ha += cp->k_uptake_kg_ha;

    /* Micronutrients based on soil organic matter */
    plan->sulfur_kg_ha = 15.0f;
    plan->zinc_kg_ha = 2.0f;
    plan->boron_kg_ha = 1.0f;
}

float agri_fertilizer_nitrogen_use_efficiency(
    const agri_fertilizer_plan_t* applied,
    const agri_season_context_t* result) {
    if (!applied || !result || applied->nitrogen_kg_ha <= 0.0f) return 0.0f;
    /* NUE = N in harvested product / N applied */
    const agri_crop_t* cp = agri_crop_lookup(result->config.crop_type);
    if (!cp) return 0.0f;
    float n_in_yield = result->predicted_yield_ton_ha
                      * cp->n_uptake_kg_ha / (cp->base_yield_ton_ha + 1e-6f);
    return n_in_yield / applied->nitrogen_kg_ha;
}

float agri_fertilizer_variable_rate(float base_rate, float yield_potential,
                                     float soil_organic_matter_pct) {
    float sm_factor = 1.0f;
    if (soil_organic_matter_pct > 5.0f) sm_factor = 0.7f;
    else if (soil_organic_matter_pct > 3.0f) sm_factor = 0.85f;
    else if (soil_organic_matter_pct < 1.0f) sm_factor = 1.2f;
    return base_rate * (yield_potential / 8.0f) * sm_factor;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L8: Crop Rotation Optimization
 *
 * Uses a greedy + local search algorithm to optimize crop sequences
 * by maximizing nitrogen benefit, minimizing pest pressure, and
 * maintaining soil health.
 *
 * Reference: Bullock (1992) "Crop Rotation" — Critical Reviews in Plant Sciences
 *            Liebman & Dyck (1993) "Crop Rotation and Intercropping"
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Nitrogen fixation credits for legume crops (kg N/ha) */
static float rotation_n_fixation(agri_crop_type_t crop) {
    switch (crop) {
        case AGRI_CROP_SOYBEAN: return 40.0f;
        default:                return 0.0f;
    }
}

float agri_rotation_nitrogen_benefit(agri_crop_type_t prev, agri_crop_type_t next) {
    float n_credit = rotation_n_fixation(prev);
    /* Wheat after soybean benefits significantly */
    if (prev == AGRI_CROP_SOYBEAN && (next == AGRI_CROP_WHEAT || next == AGRI_CROP_MAIZE))
        n_credit *= 1.5f;
    return n_credit;
}

/* Break crop effect: how much pest pressure is reduced */
float agri_rotation_break_crop_effect(agri_crop_type_t crop) {
    switch (crop) {
        case AGRI_CROP_SOYBEAN:  return 0.60f; /* Good break crop */
        case AGRI_CROP_POTATO:   return 0.45f;
        case AGRI_CROP_WHEAT:    return 0.30f;
        case AGRI_CROP_MAIZE:    return 0.20f; /* Continuous corn = high pressure */
        default:                 return 0.35f;
    }
}

void agri_rotation_optimize(const agri_crop_t* available_crops, int num_crops,
                             const agri_soil_profile_t* soil,
                             agri_rotation_plan_t* plans, int* num_plans) {
    if (!available_crops || !plans || !num_plans) return;
    *num_plans = 0;
    if (num_crops < 2) return;
    (void)soil; /* Soil reference for future soil-health-aware rotation */

    /* Generate candidate rotations using greedy sequencing */
    for (int start = 0; start < num_crops && *num_plans < 8; start++) {
        agri_rotation_plan_t* plan = &plans[*num_plans];
        memset(plan, 0, sizeof(agri_rotation_plan_t));

        plan->sequence[0] = available_crops[start].type;
        plan->length = 1;

        bool used[AGRI_MAX_ROTATION_LENGTH] = {false};
        used[start] = true;

        /* Greedily select next crop maximizing rotation benefit */
        for (int pos = 1; pos < AGRI_MAX_ROTATION_LENGTH && pos < num_crops; pos++) {
            int best_idx = -1;
            float best_score = -1e9f;

            for (int c = 0; c < num_crops; c++) {
                if (used[c]) continue;
                agri_crop_type_t prev = plan->sequence[pos - 1];
                agri_crop_type_t cand = available_crops[c].type;
                float n_benefit = agri_rotation_nitrogen_benefit(prev, cand);
                float break_eff = agri_rotation_break_crop_effect(cand);
                /* Penalize same-type consecutive planting */
                float diversity = (prev != cand) ? 1.0f : -5.0f;
                float score = n_benefit * 0.5f + break_eff * 10.0f + diversity * 5.0f
                            + available_crops[c].base_yield_ton_ha * 0.2f;
                if (score > best_score) {
                    best_score = score;
                    best_idx = c;
                }
            }
            if (best_idx < 0) break;
            used[best_idx] = true;
            plan->sequence[plan->length] = available_crops[best_idx].type;
            plan->length++;
        }

        /* Score the rotation */
        float total_n_credit = 0.0f, pest_reduction = 0.0f;
        for (int i = 0; i < plan->length; i++) {
            total_n_credit += rotation_n_fixation(plan->sequence[i]);
            pest_reduction += agri_rotation_break_crop_effect(plan->sequence[i]);
        }
        plan->nitrogen_credit_kg_ha = total_n_credit;
        plan->pest_pressure_reduction = pest_reduction / (float)(plan->length + 1);
        plan->soil_health_score = 0.5f + 0.3f * (total_n_credit / 80.0f)
                                 + 0.2f * plan->pest_pressure_reduction;
        if (plan->soil_health_score > 1.0f) plan->soil_health_score = 1.0f;

        /* Profit estimate */
        float profit_sum = 0.0f;
        for (int i = 0; i < plan->length; i++) {
            const agri_crop_t* cp = agri_crop_lookup(plan->sequence[i]);
            if (cp) profit_sum += cp->base_yield_ton_ha * 250.0f - 500.0f;
        }
        plan->profit_rotation_avg = profit_sum / (float)(plan->length + 1e-6f);
        (*num_plans)++;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L9: Carbon Farming — Soil Carbon Sequestration & GHG Accounting
 *
 * Computes carbon footprint of agricultural operations and estimates
 * carbon credit earnings from regenerative practices.
 *
 * Reference: IPCC Guidelines for National GHG Inventories (2019 Refinement)
 *            Vol. 4: Agriculture, Forestry and Other Land Use (AFOLU)
 *            FAO Ex-Ante Carbon Balance Tool (EX-ACT)
 * ═══════════════════════════════════════════════════════════════════════════ */

void agri_carbon_farm_assess(const agri_farm_t* farm,
                              agri_carbon_farm_t* carbon) {
    if (!farm || !carbon) return;
    memset(carbon, 0, sizeof(agri_carbon_farm_t));

    for (int i = 0; i < farm->num_fields; i++) {
        const agri_soil_profile_t* soil = &farm->seasons[i].soil;
        const agri_season_context_t* ctx = &farm->seasons[i];
        float area = farm->fields[i].area_hectares > 0.0f
                     ? (float)farm->fields[i].area_hectares : 1.0f;

        /* Soil organic carbon stock (0-30cm) */
        if (soil->num_layers > 0) {
            carbon->soil_organic_carbon_ton_ha += soil->soil_organic_carbon_ton_ha;
            carbon->carbon_sequestration_rate_ton_ha_yr += area * 0.5f; /* no-till */
        }

        /* N2O from fertilizer: IPCC Tier 1, EF1 = 1% */
        carbon->nitrous_oxide_emissions_kg_co2e_ha +=
            ctx->total_fertilizer_n_kg_ha * area * 0.01f * 298.0f;

        /* CH4 from rice: IPCC Tier 1 */
        if (ctx->config.crop_type == AGRI_CROP_RICE) {
            carbon->methane_emissions_kg_co2e_ha +=
                agri_carbon_methane_from_rice(area, 120.0f, 0.0f);
        }
    }

    carbon->total_carbon_footprint_kg_co2e =
        carbon->methane_emissions_kg_co2e_ha + carbon->nitrous_oxide_emissions_kg_co2e_ha;
    carbon->total_carbon_footprint_kg_co2e -=
        carbon->carbon_sequestration_rate_ton_ha_yr * 1000.0f;

    carbon->carbon_credits_earned = (int)(carbon->carbon_sequestration_rate_ton_ha_yr * 1.2f);
    carbon->carbon_credit_value_usd = (float)carbon->carbon_credits_earned * 40.0f;
}

float agri_carbon_sequestration_potential(const agri_soil_profile_t* soil,
                                           agri_crop_type_t crop,
                                           bool cover_crop, bool no_till) {
    if (!soil) return 0.0f;
    float base = 0.2f; /* ton CO2/ha/yr baseline */
    if (no_till) base += 0.5f;
    if (cover_crop) base += 0.3f;
    /* Legumes fix N, increase biomass return to soil */
    if (crop == AGRI_CROP_SOYBEAN) base += 0.1f;
    /* High organic matter soils have lower sequestration potential */
    if (soil->soil_organic_carbon_ton_ha > 50.0f) base *= 0.5f;
    return base;
}

float agri_carbon_methane_from_rice(float area_ha, float flooding_days,
                                      float organic_amendment_ton_ha) {
    /* IPCC Tier 1: EF = 1.30 kg CH4/ha/day for continuously flooded rice */
    float ef = 1.30f;
    if (flooding_days < 90.0f) ef *= flooding_days / 150.0f;
    if (organic_amendment_ton_ha > 0.0f) ef *= 2.0f;
    return area_ha * flooding_days * ef * 28.0f; /* GWP100 of CH4 = 28 */
}

float agri_carbon_n2o_from_fertilizer(float n_applied_kg_ha,
                                        float emission_factor) {
    /* IPCC Tier 1: N2O-N = N_input × EF1
     * Then: N2O = N2O-N × 44/28
     * Then: CO2-eq = N2O × 298 (GWP100) */
    if (emission_factor <= 0.0f) emission_factor = 0.01f;
    float n2o_N = n_applied_kg_ha * emission_factor;
    float n2o = n2o_N * 44.0f / 28.0f;
    return n2o * 298.0f;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L7: Stochastic Weather Generator (WGEN)
 *
 * Richardson (1981) stochastic weather generator for producing synthetic
 * daily weather series from monthly climate statistics.
 * Used for climate risk analysis, crop insurance modeling, and
 * long-term yield forecasting under climate variability.
 *
 * Algorithm:
 *   1. Precipitation occurrence: 1st-order Markov chain with
 *      P(wet|wet) and P(wet|dry) transition probabilities
 *   2. Precipitation amount: two-parameter gamma distribution
 *      (simplified with exponential for computational efficiency)
 *   3. Temperature: weakly stationary generating process:
 *      T_i = μ_i + ρ(T_{i-1} - μ_{i-1}) + σ_i * ε
 *   4. Solar radiation: conditional on wet/dry status
 *
 * Reference: Richardson & Wright (1984) "WGEN: A Model for Generating
 *            Daily Weather Variables", USDA ARS-8
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Simple pseudo-random number generator (L'Ecuyer's LCG) */
static uint32_t wgen_rand(uint32_t* state) {
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static float wgen_randf(uint32_t* state) {
    return (float)wgen_rand(state) / (float)UINT32_MAX;
}

/* Box-Muller transform for normally distributed random numbers */
static float wgen_normal(uint32_t* state, float mean, float std) {
    float u1 = wgen_randf(state);
    float u2 = wgen_randf(state);
    if (u1 < 1e-6f) u1 = 1e-6f;
    float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
    return mean + std * z;
}

/* Exponential distribution for rainfall amount */
static float wgen_exponential(uint32_t* state, float mean) {
    float u = wgen_randf(state);
    if (u < 1e-6f) u = 1e-6f;
    return -mean * logf(u);
}

void agri_wgen_init(agri_wgen_params_t* params) {
    if (!params) return;
    memset(params, 0, sizeof(agri_wgen_params_t));
    /* Default temperate climate parameters */
    float tmax_default[] = {5,7,12,17,22,27,29,28,24,18,11,6};
    float tmin_default[] = {-4,-3,1,5,10,15,17,16,12,6,2,-2};
    float rain_default[] = {50,45,60,70,80,85,75,70,60,55,60,55};
    float solar_default[] = {8,12,16,20,24,26,25,22,18,14,10,7};
    for (int m = 0; m < 12; m++) {
        params->monthly_tmax_mean[m] = tmax_default[m];
        params->monthly_tmin_mean[m] = tmin_default[m];
        params->monthly_rain_mean[m] = rain_default[m];
        params->monthly_rain_std[m] = rain_default[m] * 0.8f;
        params->monthly_rain_prob_wet[m] = 0.3f;
        params->monthly_solar_mean[m] = solar_default[m];
    }
    params->temp_lag_autocorr = 0.7f;
}

void agri_wgen_estimate_from_weather(const agri_weather_series_t* observed,
                                      agri_wgen_params_t* params) {
    if (!observed || !params) return;
    agri_wgen_init(params);
    /* Compute monthly means from observed data */
    float monthly_sum_tmax[12] = {0}, monthly_sum_tmin[12] = {0};
    float monthly_sum_rain[12] = {0}, monthly_sum_solar[12] = {0};
    int monthly_count[12] = {0};
    int monthly_wet[12] = {0};

    for (int i = 0; i < observed->num_days; i++) {
        int m = observed->days[i].month - 1;
        if (m < 0 || m >= 12) continue;
        monthly_sum_tmax[m] += observed->days[i].temp_max_c;
        monthly_sum_tmin[m] += observed->days[i].temp_min_c;
        monthly_sum_rain[m] += observed->days[i].rainfall_mm;
        monthly_sum_solar[m] += observed->days[i].solar_radiation_MJ_m2;
        if (observed->days[i].rainfall_mm > 0.1f) monthly_wet[m]++;
        monthly_count[m]++;
    }
    for (int m = 0; m < 12; m++) {
        if (monthly_count[m] > 0) {
            params->monthly_tmax_mean[m] = monthly_sum_tmax[m] / (float)monthly_count[m];
            params->monthly_tmin_mean[m] = monthly_sum_tmin[m] / (float)monthly_count[m];
            params->monthly_rain_mean[m] = monthly_sum_rain[m] / (float)monthly_count[m];
            params->monthly_solar_mean[m] = monthly_sum_solar[m] / (float)monthly_count[m];
            params->monthly_rain_prob_wet[m] = (float)monthly_wet[m] / (float)monthly_count[m];
            params->monthly_rain_std[m] = params->monthly_rain_mean[m] * 0.8f;
        }
    }
}

int agri_wgen_generate(const agri_wgen_params_t* params,
                          int year, int num_days,
                          agri_weather_series_t* output,
                          uint32_t seed) {
    if (!params || !output || num_days <= 0) return 0;
    agri_weather_init(output);
    output->start_year = year;
    output->start_doy = 1;

    uint32_t state = seed > 0 ? seed : 12345u;
    float prev_tmax = params->monthly_tmax_mean[0];
    float prev_tmin = params->monthly_tmin_mean[0];
    bool prev_wet = false;

    int doy = 1;
    for (int d = 0; d < num_days && d < AGRI_MAX_WEATHER_DAYS; d++) {
        int month, day;
        agri_date_from_doy(year, doy, &month, &day);
        int m = month - 1;
        if (m < 0) m = 0;
        if (m > 11) m = 11;

        /* Precipitation: Markov chain */
        float p_ww = params->monthly_rain_prob_wet[m] + 0.2f;
        float p_dw = params->monthly_rain_prob_wet[m] * 0.5f;
        if (p_ww > 0.8f) p_ww = 0.8f;
        if (p_dw < 0.05f) p_dw = 0.05f;
        float p_wet = prev_wet ? p_ww : p_dw;
        bool wet = wgen_randf(&state) < p_wet;
        float rain = 0.0f;
        if (wet) {
            rain = wgen_exponential(&state, params->monthly_rain_mean[m]);
            if (rain < 0.1f) rain = 0.1f;
            if (rain > 100.0f) rain = 100.0f;
        }

        /* Temperature: autoregressive process */
        float tmax_mean = params->monthly_tmax_mean[m];
        float tmin_mean = params->monthly_tmin_mean[m];
        float tmax_std = (wet ? 4.0f : 6.0f);
        float tmin_std = (wet ? 3.0f : 5.0f);

        float tmax = tmax_mean + params->temp_lag_autocorr * (prev_tmax - tmax_mean)
                    + wgen_normal(&state, 0.0f, tmax_std * sqrtf(1.0f - params->temp_lag_autocorr * params->temp_lag_autocorr));
        float tmin = tmin_mean + params->temp_lag_autocorr * (prev_tmin - tmin_mean)
                    + wgen_normal(&state, 0.0f, tmin_std * 0.7f);

        if (tmin > tmax) { float tmp = tmin; tmin = tmax - 2.0f; tmax = tmp; }

        /* Solar radiation: lower on wet days */
        float solar = params->monthly_solar_mean[m] * (wet ? 0.55f : 1.0f)
                     + wgen_normal(&state, 0.0f, 3.0f);
        if (solar < 1.0f) solar = 1.0f;
        if (solar > 35.0f) solar = 35.0f;

        float humidity = wet ? 75.0f + wgen_normal(&state, 0.0f, 8.0f)
                             : 50.0f + wgen_normal(&state, 0.0f, 10.0f);
        if (humidity < 10.0f) humidity = 10.0f;
        if (humidity > 100.0f) humidity = 100.0f;

        float wind = 2.0f + wgen_exponential(&state, 1.5f);
        if (wind > 30.0f) wind = 30.0f;

        agri_weather_add_day(output, year, month, day,
                             tmax, tmin, rain, solar, humidity, wind);

        prev_tmax = tmax;
        prev_tmin = tmin;
        prev_wet = wet;
        doy++;
        if (doy > 365) { doy = 1; year++; }
    }
    return output->num_days;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L7: Nitrogen Cycle Modeling
 *
 * Implements the classic Stanford & Smith (1972) nitrogen mineralization
 * model with first-order kinetics. Tracks NO3, NH4, organic N pools
 * and their transformations under varying temperature/moisture.
 *
 * Key processes:
 *   Mineralization: Organic-N → NH4 (first-order, temp & moisture dependent)
 *   Nitrification: NH4 → NO3 (Michaelis-Menten kinetics)
 *   Denitrification: NO3 → N2/N2O (anaerobic, water-filled porosity > 60%)
 *   Immobilization: NH4/NO3 → Organic-N (C:N ratio dependent)
 *   Leaching: NO3 loss via drainage below root zone
 *   Volatilization: NH4 → NH3 gas loss (high pH, surface applied)
 *
 * Reference: Godwin & Jones (1991) CERES-N submodel in DSSAT v4.7
 *            Parton et al. (1987) CENTURY Soil Organic Matter Model
 * ═══════════════════════════════════════════════════════════════════════════ */

void agri_nitrogen_init(agri_nitrogen_pool_t* pool,
                         float soil_organic_n, float no3, float nh4) {
    if (!pool) return;
    memset(pool, 0, sizeof(agri_nitrogen_pool_t));
    pool->organic_n_kg_ha = soil_organic_n > 0.0f ? soil_organic_n : 5000.0f;
    pool->no3_n_kg_ha = no3 > 0.0f ? no3 : 30.0f;
    pool->nh4_n_kg_ha = nh4 > 0.0f ? nh4 : 10.0f;
}

void agri_nitrogen_step(agri_nitrogen_pool_t* pool,
                         float soil_temp_c, float soil_water_content,
                         float soil_water_fc, float plant_demand_kg_ha,
                         float organic_matter_pct, float rainfall_mm) {
    if (!pool) return;
    if (organic_matter_pct <= 0.0f) organic_matter_pct = 2.0f;

    /* Water-filled pore space (WFPS) for denitrification threshold */
    float wfps = soil_water_fc > 0.0f
                 ? soil_water_content / soil_water_fc * 100.0f : 50.0f;
    if (wfps > 100.0f) wfps = 100.0f;

    /* 1. Mineralization: first-order, Q10 temperature correction */
    pool->mineralized_kg_ha_day = agri_nitrogen_mineralization(
        pool->organic_n_kg_ha, soil_temp_c, wfps, organic_matter_pct);
    pool->organic_n_kg_ha -= pool->mineralized_kg_ha_day;
    pool->nh4_n_kg_ha += pool->mineralized_kg_ha_day;
    if (pool->organic_n_kg_ha < 0.0f) pool->organic_n_kg_ha = 0.0f;

    /* 2. Nitrification: NH4 → NO3 (aerobic, temp > 5°C) */
    if (soil_temp_c > 5.0f && wfps < 80.0f) {
        float nitr_rate = 0.05f; /* fraction of NH4 pool per day */
        float temp_factor = powf(2.0f, (soil_temp_c - 20.0f) / 10.0f);
        if (temp_factor < 0.1f) temp_factor = 0.1f;
        if (temp_factor > 3.0f) temp_factor = 3.0f;
        pool->nitrified_kg_ha_day = pool->nh4_n_kg_ha * nitr_rate * temp_factor;
        if (pool->nitrified_kg_ha_day > pool->nh4_n_kg_ha)
            pool->nitrified_kg_ha_day = pool->nh4_n_kg_ha;
        pool->nh4_n_kg_ha -= pool->nitrified_kg_ha_day;
        pool->no3_n_kg_ha += pool->nitrified_kg_ha_day;
    }

    /* 3. Denitrification: NO3 → N2/N2O (WFPS > 60%) */
    pool->denitrified_kg_ha_day = agri_nitrogen_denitrification(
        pool->no3_n_kg_ha, wfps, soil_temp_c, organic_matter_pct);
    pool->no3_n_kg_ha -= pool->denitrified_kg_ha_day;
    if (pool->no3_n_kg_ha < 0.0f) pool->no3_n_kg_ha = 0.0f;

    /* 4. Leaching: NO3 is mobile, lost with deep drainage */
    /* Assume drainage = rainfall - ET (simplified) */
    float drainage = rainfall_mm > 10.0f ? (rainfall_mm - 5.0f) * 0.3f : 0.0f;
    pool->leached_kg_ha_day = agri_nitrogen_leaching(
        pool->no3_n_kg_ha, drainage, 60.0f, soil_water_fc);
    pool->no3_n_kg_ha -= pool->leached_kg_ha_day;
    if (pool->no3_n_kg_ha < 0.0f) pool->no3_n_kg_ha = 0.0f;

    /* 5. Volatilization: NH4 → NH3 (high temp, surface) */
    if (soil_temp_c > 20.0f) {
        float vol_rate = 0.01f * (soil_temp_c - 15.0f) / 10.0f;
        pool->volatilized_kg_ha_day = pool->nh4_n_kg_ha * vol_rate;
        if (pool->volatilized_kg_ha_day < 0.0f) pool->volatilized_kg_ha_day = 0.0f;
        pool->nh4_n_kg_ha -= pool->volatilized_kg_ha_day;
        if (pool->nh4_n_kg_ha < 0.0f) pool->nh4_n_kg_ha = 0.0f;
    }

    /* 6. Plant uptake: preferential NO3 uptake */
    float total_available = pool->no3_n_kg_ha + pool->nh4_n_kg_ha;
    if (plant_demand_kg_ha > 0.0f && total_available > 0.0f) {
        pool->plant_uptake_kg_ha_day = plant_demand_kg_ha < total_available
                                        ? plant_demand_kg_ha : total_available;
        float no3_share = pool->no3_n_kg_ha / total_available;
        float no3_uptake = pool->plant_uptake_kg_ha_day * no3_share;
        float nh4_uptake = pool->plant_uptake_kg_ha_day - no3_uptake;
        pool->no3_n_kg_ha -= no3_uptake;
        pool->nh4_n_kg_ha -= nh4_uptake;
        if (pool->no3_n_kg_ha < 0.0f) pool->no3_n_kg_ha = 0.0f;
        if (pool->nh4_n_kg_ha < 0.0f) pool->nh4_n_kg_ha = 0.0f;
    }
}

/* First-order mineralization kinetics: dN/dt = k × f(T) × f(W) × N_org
 * k = potential mineralization rate constant (~0.05 day^-1 at 25°C)
 * f(T) = Q10 temperature function
 * f(W) = optimal at WFPS 60-70%, zero below 20% and above 95% */
float agri_nitrogen_mineralization(float organic_n_kg_ha, float temp_c,
                                     float wfps, float organic_matter_pct) {
    if (organic_n_kg_ha <= 0.0f) return 0.0f;
    float k = organic_matter_pct * 0.0008f; /* rate proportional to OM */
    if (k > 0.01f) k = 0.01f;
    /* Q10 = 2: rate doubles per 10°C */
    float ft = powf(2.0f, (temp_c - 20.0f) / 10.0f);
    if (ft < 0.05f) ft = 0.05f;
    if (ft > 4.0f) ft = 4.0f;
    /* Moisture function: optimal 50-70% WFPS */
    float fw;
    if (wfps < 20.0f) fw = wfps / 20.0f;
    else if (wfps <= 70.0f) fw = 1.0f;
    else fw = 1.0f - (wfps - 70.0f) / 30.0f;
    if (fw < 0.0f) fw = 0.0f;
    return organic_n_kg_ha * k * ft * fw;
}

/* Denitrification rate = k_den × f(NO3) × f(WFPS) × f(T) × f(C)
 * Significant when WFPS > 60% and NO3 available */
float agri_nitrogen_denitrification(float no3_kg_ha, float wfps,
                                      float temp_c, float organic_c_pct) {
    if (no3_kg_ha <= 0.0f || wfps < 50.0f) return 0.0f;
    float k_den = 0.02f;
    float f_wfps = wfps < 60.0f ? 0.0f
                  : wfps < 80.0f ? (wfps - 60.0f) / 20.0f : 1.0f;
    float f_temp = powf(2.0f, (temp_c - 20.0f) / 10.0f);
    if (f_temp < 0.0f) f_temp = 0.0f;
    if (f_temp > 3.0f) f_temp = 3.0f;
    float f_c = organic_c_pct > 0.5f ? 1.0f : organic_c_pct / 0.5f;
    return no3_kg_ha * k_den * f_wfps * f_temp * f_c;
}

/* Leaching = NO3_concentration × drainage_volume
 * Assume NO3 is uniformly distributed in the soil solution */
float agri_nitrogen_leaching(float no3_kg_ha, float drainage_mm,
                               float soil_depth_cm, float field_capacity) {
    if (no3_kg_ha <= 0.0f || drainage_mm <= 0.0f) return 0.0f;
    /* Soil water volume in the root zone (mm) */
    float soil_water_mm = soil_depth_cm * 10.0f * field_capacity;
    /* NO3 concentration in soil solution (kg/ha per mm of water) */
    float conc = soil_water_mm > 0.0f ? no3_kg_ha / soil_water_mm : 0.0f;
    float leached = conc * drainage_mm;
    return leached < no3_kg_ha ? leached : no3_kg_ha;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L7: Economic Decision Support for Farmers
 *
 * Provides break-even analysis, sensitivity testing, and profitability
 * assessment for crop production decisions.
 *
 * Key metrics:
 *   Gross Margin = Revenue - Variable Costs
 *   Net Profit = Gross Margin - Fixed Costs
 *   Break-Even Yield = Total Cost / Price per ton
 *   Break-Even Price = Total Cost per ha / Yield per ha
 *   ROI = (Net Profit / Total Cost) × 100
 *
 * Reference: Kay, Edwards & Duffy (2016) "Farm Management" 8th ed.
 *            Purdue University Ag Econ 310: Farm Business Management
 * ═══════════════════════════════════════════════════════════════════════════ */

void agri_economic_analyze(const agri_season_context_t* season,
                            const agri_cost_breakdown_t* costs,
                            float expected_price_per_ton,
                            agri_economic_analysis_t* analysis) {
    if (!season || !costs || !analysis) return;
    memset(analysis, 0, sizeof(agri_economic_analysis_t));

    float yield = season->predicted_yield_ton_ha > 0.0f
                  ? season->predicted_yield_ton_ha : 5.0f;

    analysis->expected_revenue_per_ha = yield * expected_price_per_ton;

    /* Variable costs */
    analysis->variable_cost_per_ha =
        costs->seed_cost_per_ha +
        costs->fertilizer_cost_per_ha +
        costs->pesticide_cost_per_ha +
        costs->fuel_cost_per_ha +
        costs->irrigation_cost_per_mm_ha * season->total_irrigation_mm;

    /* Fixed costs */
    analysis->fixed_cost_per_ha =
        costs->labor_cost_per_ha +
        costs->land_rent_per_ha +
        costs->insurance_per_ha +
        costs->misc_cost_per_ha;

    float total_cost = analysis->variable_cost_per_ha + analysis->fixed_cost_per_ha;
    analysis->gross_margin_per_ha = analysis->expected_revenue_per_ha
                                   - analysis->variable_cost_per_ha;
    analysis->net_profit_per_ha = analysis->gross_margin_per_ha
                                 - analysis->fixed_cost_per_ha;
    analysis->break_even_yield_ton_ha = expected_price_per_ton > 0.0f
        ? total_cost / expected_price_per_ton : 0.0f;
    analysis->break_even_price_per_ton = yield > 0.0f
        ? total_cost / yield : 0.0f;
    analysis->return_on_investment_pct = total_cost > 0.0f
        ? (analysis->net_profit_per_ha / total_cost) * 100.0f : 0.0f;
}

void agri_economic_sensitivity(const agri_season_context_t* season,
                                const agri_cost_breakdown_t* costs,
                                float price_range[5], float yield_range[5],
                                float* profit_matrix) {
    if (!season || !costs || !profit_matrix) return;
    for (int p = 0; p < 5; p++) {
        for (int y = 0; y < 5; y++) {
            agri_economic_analysis_t analysis;
            agri_season_context_t tmp = *season;
            tmp.predicted_yield_ton_ha = yield_range[y];
            agri_economic_analyze(&tmp, costs, price_range[p], &analysis);
            profit_matrix[p * 5 + y] = analysis.net_profit_per_ha;
        }
    }
}

float agri_economic_breakeven_price(const agri_cost_breakdown_t* costs,
                                      float yield_ton_ha) {
    if (!costs || yield_ton_ha <= 0.0f) return 0.0f;
    float total = costs->seed_cost_per_ha + costs->fertilizer_cost_per_ha
                + costs->pesticide_cost_per_ha + costs->fuel_cost_per_ha
                + costs->labor_cost_per_ha + costs->land_rent_per_ha
                + costs->insurance_per_ha + costs->misc_cost_per_ha;
    return total / yield_ton_ha;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L6: Harvest & Post-Harvest Loss Modeling
 *
 * Quantifies yield losses across the harvest-to-market supply chain.
 * Post-harvest losses in developing countries can reach 20-40% for grains.
 *
 * FAO estimates global food loss at ~14% from post-harvest to retail (2019).
 *
 * Key factors:
 *   Field loss: shattering, lodging, pest damage pre-harvest
 *   Transport loss: spillage, spoilage during transit
 *   Storage loss: insects, fungi, moisture damage
 *   Processing loss: milling/sorting inefficiency
 *
 * Reference: FAO (2019) "The State of Food and Agriculture"
 *            Hodges et al. (2011) "Postharvest losses and waste"
 * ═══════════════════════════════════════════════════════════════════════════ */

void agri_harvest_model(const agri_season_context_t* season,
                          agri_harvest_method_t method,
                          agri_harvest_model_t* model) {
    if (!season || !model) return;
    memset(model, 0, sizeof(agri_harvest_model_t));

    /* Base loss rates by method */
    switch (method) {
        case AGRI_HARVEST_MANUAL:
            model->field_loss_pct = 3.0f;
            break;
        case AGRI_HARVEST_MECHANICAL:
            model->field_loss_pct = 5.0f;
            break;
        case AGRI_HARVEST_COMBINE:
            model->field_loss_pct = 2.5f;
            break;
        default:
            model->field_loss_pct = 4.0f;
    }

    /* Adjust for crop maturity conditions */
    if (season->current.stage >= AGRI_STAGE_MATURITY) {
        model->field_loss_pct *= 1.2f;
    }

    /* Transport loss: ~1-3% depending on distance (simplified) */
    model->transport_loss_pct = 2.0f;

    /* Storage loss: depends on grain moisture and duration */
    model->storage_loss_pct = 3.0f;

    /* Processing/milling loss */
    model->processing_loss_pct = season->config.crop_type == AGRI_CROP_RICE
                                  ? 10.0f : 5.0f;

    /* Chain compounding: each step acts on remaining quantity */
    float remaining = 100.0f;
    remaining *= (1.0f - model->field_loss_pct / 100.0f);
    remaining *= (1.0f - model->transport_loss_pct / 100.0f);
    remaining *= (1.0f - model->storage_loss_pct / 100.0f);
    remaining *= (1.0f - model->processing_loss_pct / 100.0f);
    model->total_loss_pct = 100.0f - remaining;

    model->marketable_yield_ton_ha = season->predicted_yield_ton_ha
                                     * remaining / 100.0f;

    /* Dry-down: estimate harvest date and moisture */
    model->optimal_harvest_moisture_pct = 15.0f;
    const agri_crop_t* cp = agri_crop_lookup(season->config.crop_type);
    model->days_to_harvest = cp ? cp->days_to_maturity - season->days_simulated : 0;
    if (model->days_to_harvest < 0) model->days_to_harvest = 0;
}

/* Grain dry-down curve: moisture loss follows exponential decay
 * M(t) = M_eq + (M_0 - M_eq) * exp(-k * GDD * t)
 * where k is species-specific drying coefficient */
float agri_harvest_dry_down(agri_crop_type_t crop, float initial_moisture,
                              float gdd_per_day, int days) {
    float k;
    switch (crop) {
        case AGRI_CROP_MAIZE:    k = 0.005f; break;
        case AGRI_CROP_WHEAT:    k = 0.008f; break;
        case AGRI_CROP_RICE:     k = 0.004f; break;
        case AGRI_CROP_SOYBEAN:  k = 0.006f; break;
        default:                 k = 0.005f; break;
    }
    float m_eq = 12.0f; /* Equilibrium moisture content */
    float moisture = m_eq + (initial_moisture - m_eq)
                     * expf(-k * gdd_per_day * (float)days);
    return moisture < m_eq ? m_eq : moisture;
}

/* Grain storage loss model: fungi/insect growth rate depends on
 * temperature and moisture. High moisture (>14%) dramatically
 * increases loss rate due to mold. */
float agri_grain_storage_loss(float initial_quantity_ton,
                                float moisture_pct, float temp_c,
                                int storage_days) {
    if (initial_quantity_ton <= 0.0f || storage_days <= 0) return 0.0f;
    /* Safe storage: moisture < 13% and temp < 15°C → ~1% loss/6mo */
    float base_rate = 0.0001f; /* daily loss fraction */
    /* Moisture penalty: exponential above 13% */
    if (moisture_pct > 13.0f)
        base_rate *= powf(2.0f, (moisture_pct - 13.0f) * 0.5f);
    /* Temperature penalty: Q10 = 2 */
    if (temp_c > 15.0f)
        base_rate *= powf(2.0f, (temp_c - 15.0f) / 10.0f);
    float loss = initial_quantity_ton * base_rate * (float)storage_days;
    return loss < initial_quantity_ton ? loss : initial_quantity_ton;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L8: Climate Change Impact Assessment
 *
 * Uses crop-climate response functions derived from the DSSAT and APSIM
 * multi-model ensembles (AgMIP) to project yield changes under climate
 * scenarios. Accounts for CO2 fertilization, temperature stress, and
 * precipitation changes.
 *
 * Key relationships:
 *   C3 crops (wheat, rice, soybean): CO2 fertilization ~+15% at 550ppm
 *   C4 crops (maize): minimal CO2 response, more heat-sensitive
 *   Growing season length shifts earlier (warmer spring)
 *
 * Reference: Rosenzweig et al. (2014) "Assessing agricultural risks of
 *            climate change in the 21st century", PNAS 111(9):3268-3273
 *            IPCC AR6 WG2 (2022) Chapter 5
 * ═══════════════════════════════════════════════════════════════════════════ */

float agri_climate_co2_fertilization_effect(float co2_ppm, agri_crop_type_t crop) {
    float baseline_co2 = 360.0f; /* pre-industrial + modern baseline */
    float ratio = co2_ppm / baseline_co2;
    float beta; /* CO2 response coefficient */
    switch (crop) {
        case AGRI_CROP_WHEAT:   beta = 0.08f; break;
        case AGRI_CROP_RICE:    beta = 0.07f; break;
        case AGRI_CROP_SOYBEAN: beta = 0.10f; break;
        case AGRI_CROP_POTATO:  beta = 0.06f; break;
        case AGRI_CROP_MAIZE:   beta = 0.01f; break; /* C4 minimal response */
        default:                beta = 0.05f; break;
    }
    return 1.0f + beta * (ratio - 1.0f);
}

float agri_climate_yield_response(float temp_change, float precip_change,
                                    float co2_effect, agri_crop_type_t crop) {
    /* Temperature effect: non-linear, penalizes above optimal */
    float temp_factor;
    if (crop == AGRI_CROP_MAIZE) {
        /* Maize: sensitive, ~-7.4% per °C above 30°C */
        temp_factor = 1.0f - 0.074f * temp_change;
    } else if (crop == AGRI_CROP_WHEAT) {
        temp_factor = 1.0f - 0.06f * temp_change;
    } else {
        temp_factor = 1.0f - 0.05f * temp_change;
    }
    if (temp_factor < 0.1f) temp_factor = 0.1f;

    /* Precipitation effect: log-linear */
    float precip_factor = 1.0f + 0.003f * precip_change;
    if (precip_factor < 0.2f) precip_factor = 0.2f;
    if (precip_factor > 2.0f) precip_factor = 2.0f;

    return temp_factor * precip_factor * co2_effect;
}

void agri_climate_impact_assess(const agri_season_context_t* baseline,
                                 const agri_climate_scenario_t* scenario,
                                 agri_climate_impact_t* impact) {
    if (!baseline || !scenario || !impact) return;
    memset(impact, 0, sizeof(agri_climate_impact_t));

    impact->baseline_yield_ton_ha = baseline->predicted_yield_ton_ha > 0.0f
                                     ? baseline->predicted_yield_ton_ha : 5.0f;

    agri_crop_type_t crop = baseline->config.crop_type;
    float co2_effect = agri_climate_co2_fertilization_effect(scenario->co2_ppm, crop);
    float yield_response = agri_climate_yield_response(
        scenario->temp_increase_c, scenario->precip_change_pct, co2_effect, crop);

    impact->projected_yield_ton_ha = impact->baseline_yield_ton_ha * yield_response;
    impact->yield_change_pct = (yield_response - 1.0f) * 100.0f;
    impact->water_demand_change_pct = scenario->temp_increase_c * 5.0f;
    impact->growing_season_shift_days = -scenario->temp_increase_c * 5.0f;
    impact->heat_stress_days_added = scenario->extreme_heat_days_yr * 0.3f;
    impact->pest_pressure_change_pct = scenario->temp_increase_c * 15.0f;
    impact->adaptation_potential_pct = impact->yield_change_pct < 0.0f
        ? -impact->yield_change_pct * 0.5f : 0.0f; /* Max 50% recovery */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L7: Crop Insurance Modeling
 *
 * Implements US RMA-style revenue protection and yield protection
 * insurance calculations with actuarial fair premium estimation.
 *
 * Revenue Protection: indemnity = max(0, guaranteed_revenue - actual_revenue)
 * Yield Protection:  indemnity = max(0, guaranteed_yield - actual_yield)
 *                      × projected_price
 *
 * Actuarial Premium: expected value of indemnity under yield distribution
 * with loading factor for administrative costs (typically 1.1-1.3).
 *
 * Reference: Goodwin & Smith (2014) Chap. 10, US Farm Bill Title XI
 *            Coble et al. (2010) "Agricultural Insurance" ARE Review
 * ═══════════════════════════════════════════════════════════════════════════ */

void agri_insurance_quote(agri_insurance_policy_t* policy,
                           const agri_season_context_t* season,
                           agri_insurance_type_t type, float coverage,
                           float projected_price) {
    if (!policy || !season) return;
    memset(policy, 0, sizeof(agri_insurance_policy_t));

    policy->type = type;
    if (coverage < 0.50f) coverage = 0.50f;
    if (coverage > 0.85f) coverage = 0.85f;
    policy->coverage_level = coverage;

    /* APH (Actual Production History) yield */
    float aph_yield = season->predicted_yield_ton_ha > 0.0f
                      ? season->predicted_yield_ton_ha : 5.0f;
    policy->guaranteed_yield_ton_ha = aph_yield * coverage;
    policy->projected_price_per_ton = projected_price > 0.0f ? projected_price : 250.0f;
    policy->guaranteed_revenue_per_ha = policy->guaranteed_yield_ton_ha
                                        * policy->projected_price_per_ton;

    /* Premium: actuarially fair + 20% load */
    float yield_std = aph_yield * 0.25f;
    policy->premium_per_ha = agri_insurance_actuarial_premium(
        aph_yield, yield_std, coverage, policy->projected_price_per_ton);
    policy->premium_per_ha *= 1.20f;

    policy->loss_ratio = 0.0f;
    policy->indemnity_payment = 0.0f;
}

float agri_insurance_calculate_indemnity(const agri_insurance_policy_t* policy,
                                          float actual_yield, float harvest_price) {
    if (!policy || actual_yield < 0.0f) return 0.0f;
    if (harvest_price <= 0.0f) harvest_price = policy->projected_price_per_ton;

    float indemnity = 0.0f;
    switch (policy->type) {
        case AGRI_INSURANCE_YIELD_PROTECTION: {
            float shortfall = policy->guaranteed_yield_ton_ha - actual_yield;
            if (shortfall > 0.0f) {
                indemnity = shortfall * policy->projected_price_per_ton;
            }
            break;
        }
        case AGRI_INSURANCE_REVENUE_PROTECTION: {
            float actual_revenue = actual_yield * harvest_price;
            float guaranteed = policy->guaranteed_revenue_per_ha;
            if (actual_revenue < guaranteed) {
                indemnity = guaranteed - actual_revenue;
            }
            break;
        }
        case AGRI_INSURANCE_AREA_PLAN: {
            /* Area-based: triggered when county yield falls below guarantee */
            if (actual_yield < policy->guaranteed_yield_ton_ha) {
                indemnity = (policy->guaranteed_yield_ton_ha - actual_yield)
                           * policy->projected_price_per_ton * 0.90f;
            }
            break;
        }
        default: break;
    }
    return indemnity < 0.0f ? 0.0f : indemnity;
}

/* Compute actuarially fair premium as expected value of indemnity
 * under a normal yield distribution (simplified).
 * Premium = ∫_{0}^{Y_guaranteed} (Y_guaranteed - y) × Price × f(y) dy */
float agri_insurance_actuarial_premium(float expected_yield,
                                         float yield_std,
                                         float coverage_level,
                                         float price_per_ton) {
    if (expected_yield <= 0.0f || yield_std <= 0.0f) return 0.0f;
    float guaranteed = expected_yield * coverage_level;

    /* Numerical integration: discretize yield distribution into 20 points */
    float premium = 0.0f;
    float step = guaranteed / 20.0f;
    for (int i = 0; i < 20; i++) {
        float y = ((float)i + 0.5f) * step;
        /* Normal PDF */
        float z = (y - expected_yield) / yield_std;
        float pdf = expf(-0.5f * z * z) / (yield_std * sqrtf(2.0f * (float)M_PI));
        float shortfall = guaranteed - y;
        if (shortfall > 0.0f) {
            premium += shortfall * pdf * step * price_per_ton;
        }
    }
    return premium;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L4: Water Quality — Edge-of-Field Nutrient Loss
 *
 * Estimates nitrogen and phosphorus losses from agricultural fields
 * via surface runoff, sediment transport, and tile drainage.
 *
 * Uses the USDA NRCS curve number method for runoff estimation:
 * Q = (P - 0.2S)² / (P + 0.8S)
 * where S = (1000/CN - 10) × 25.4
 *
 * Sediment-bound nutrients: enrichment ratio ≈ 2.0 (sediment has 2×
 * the nutrient concentration of bulk soil)
 *
 * Reference: Sharpley & Williams (1990) EPIC model Chapter 3
 *            USDA NRCS (2004) National Engineering Handbook Part 630
 * ═══════════════════════════════════════════════════════════════════════════ */

float agri_runoff_curve_number(agri_crop_type_t crop, const agri_soil_profile_t* soil) {
    /* Hydrologic Soil Group based on texture */
    float clay = soil && soil->num_layers > 0 ? soil->layers[0].clay_pct : 20.0f;
    char hsg;
    if (clay < 10.0f)        hsg = 'A'; /* Sand */
    else if (clay < 20.0f)   hsg = 'B'; /* Sandy Loam */
    else if (clay < 35.0f)   hsg = 'C'; /* Loam/Silt Loam */
    else                     hsg = 'D'; /* Clay */

    /* CN for row crops, good condition, by HSG (TR-55) */
    int cn;
    switch (hsg) {
        case 'A': cn = 67; break;
        case 'B': cn = 78; break;
        case 'C': cn = 85; break;
        case 'D': cn = 89; break;
        default:  cn = 78; break;
    }
    /* Adjust for crop residue / cover */
    if (crop == AGRI_CROP_WHEAT) cn -= 2;
    return (float)cn;
}

float agri_sediment_delivery_ratio(float area_ha, float slope_pct) {
    /* SDR = fraction of eroded soil that reaches the field edge.
     * Decreases with larger area (more deposition within field). */
    float sdr = 0.42f * powf(area_ha, -0.125f) * (1.0f + 0.01f * slope_pct);
    if (sdr > 0.8f) sdr = 0.8f;
    if (sdr < 0.05f) sdr = 0.05f;
    return sdr;
}

void agri_water_quality_estimate(const agri_season_context_t* season,
                                  const agri_fertilizer_plan_t* fertilizer,
                                  float slope_pct, float tile_drainage,
                                  agri_water_quality_t* wq) {
    if (!season || !fertilizer || !wq) return;
    memset(wq, 0, sizeof(agri_water_quality_t));

    float cn = agri_runoff_curve_number(season->config.crop_type, &season->soil);
    float S_mm = (1000.0f / cn - 10.0f) * 25.4f;
    float total_rain = 0.0f;
    for (int i = 0; i < season->weather.num_days; i++)
        total_rain += season->weather.days[i].rainfall_mm;
    /* Curve number runoff equation */
    float Q_mm = 0.0f;
    float ia = 0.2f * S_mm;
    if (total_rain > ia) {
        Q_mm = (total_rain - ia) * (total_rain - ia) / (total_rain + 0.8f * S_mm);
    }
    wq->surface_runoff_mm = Q_mm;

    /* Sediment loss via USLE with delivery ratio */
    agri_usle_factors_t usle;
    memset(&usle, 0, sizeof(usle));
    usle.rainfall_erosivity_R = agri_usle_compute_R(&season->weather);
    usle.soil_erodibility_K = agri_usle_compute_K(&season->soil);
    usle.slope_length_steepness_LS = agri_usle_compute_LS(100.0f, slope_pct);
    usle.cover_management_C = agri_usle_compute_C(season->config.crop_type,
                                                   season->current.stage);
    usle.support_practice_P = 0.5f; /* Contour farming */
    float gross_erosion = agri_usle_soil_loss_ton_ha_yr(&usle);
    float sdr = agri_sediment_delivery_ratio(1.0f, slope_pct);
    wq->sediment_loss_ton_ha = gross_erosion * sdr;

    /* Particulate N and P: enrichment ratio ≈ 2.0 */
    float soil_n = fertilizer->nitrogen_kg_ha * 0.1f; /* ~10% in top cm */
    float soil_p = fertilizer->phosphorus_kg_ha * 0.1f;
    wq->particulate_n_loss_kg_ha = wq->sediment_loss_ton_ha * 1000.0f
                                    * (soil_n / 2e6f) * 2.0f; /* 2M kg soil/ha */
    wq->particulate_p_loss_kg_ha = wq->sediment_loss_ton_ha * 1000.0f
                                    * (soil_p / 2e6f) * 2.0f;

    /* Dissolved N and P in runoff water */
    wq->dissolved_n_loss_kg_ha = Q_mm * 0.005f * fertilizer->nitrogen_kg_ha / 100.0f;
    wq->dissolved_p_loss_kg_ha = Q_mm * 0.001f * fertilizer->phosphorus_kg_ha / 100.0f;

    /* Tile drainage: nitrate-rich subsurface flow */
    wq->tile_drainage_mm = tile_drainage;
    if (tile_drainage > 0.0f) {
        wq->dissolved_n_loss_kg_ha += tile_drainage * 0.02f;
    }

    wq->total_n_loss_kg_ha = wq->particulate_n_loss_kg_ha + wq->dissolved_n_loss_kg_ha;
    wq->total_p_loss_kg_ha = wq->particulate_p_loss_kg_ha + wq->dissolved_p_loss_kg_ha;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L3: Tillage & Soil Management Systems
 *
 * Evaluates different tillage systems based on soil disturbance,
 * residue retention, fuel use, erosion control, and yield impact.
 *
 * Conservation tillage (no-till, strip-till) maintains ≥30% residue
 * cover after planting (NRCS Conservation Practice Standard 329).
 *
 * No-till benefits: reduced erosion (60-90%), increased SOM,
 * reduced fuel use (50-80%), but may require more herbicide.
 *
 * Reference: Lal et al. (2007) "Evolution of the plow over 10,000 years"
 *            CTIC (2022) National Crop Residue Management Survey
 * ═══════════════════════════════════════════════════════════════════════════ */

void agri_tillage_evaluate(agri_tillage_type_t type,
                            const agri_soil_profile_t* soil,
                            agri_tillage_system_t* system) {
    if (!system) return;
    memset(system, 0, sizeof(agri_tillage_system_t));
    system->type = type;

    switch (type) {
        case AGRI_TILLAGE_CONVENTIONAL:
            system->surface_residue_pct = 5.0f;
            system->soil_disturbance_pct = 95.0f;
            system->fuel_use_l_ha = 55.0f;
            system->soil_organic_matter_impact = -0.05f;
            system->erosion_reduction_pct = 0.0f;
            system->yield_impact_pct = 0.0f;
            system->operations_per_year = 4;
            break;
        case AGRI_TILLAGE_REDUCED:
            system->surface_residue_pct = 30.0f;
            system->soil_disturbance_pct = 50.0f;
            system->fuel_use_l_ha = 30.0f;
            system->soil_organic_matter_impact = 0.02f;
            system->erosion_reduction_pct = 40.0f;
            system->yield_impact_pct = 2.0f;
            system->operations_per_year = 2;
            break;
        case AGRI_TILLAGE_NO_TILL:
            system->surface_residue_pct = 70.0f;
            system->soil_disturbance_pct = 10.0f;
            system->fuel_use_l_ha = 12.0f;
            system->soil_organic_matter_impact = 0.10f;
            system->erosion_reduction_pct = 75.0f;
            system->yield_impact_pct = 3.0f;
            system->operations_per_year = 1;
            break;
        case AGRI_TILLAGE_STRIP_TILL:
            system->surface_residue_pct = 50.0f;
            system->soil_disturbance_pct = 30.0f;
            system->fuel_use_l_ha = 20.0f;
            system->soil_organic_matter_impact = 0.06f;
            system->erosion_reduction_pct = 60.0f;
            system->yield_impact_pct = 4.0f;
            system->operations_per_year = 1;
            break;
        case AGRI_TILLAGE_RIDGE_TILL:
            system->surface_residue_pct = 40.0f;
            system->soil_disturbance_pct = 25.0f;
            system->fuel_use_l_ha = 25.0f;
            system->soil_organic_matter_impact = 0.04f;
            system->erosion_reduction_pct = 55.0f;
            system->yield_impact_pct = 1.0f;
            system->operations_per_year = 2;
            break;
    }
    (void)soil;
}

/* Residue decay follows first-order kinetics:
 * R(t) = R_0 × exp(-k × t)
 * k = k_ref × f(T) × f(W)
 * where k_ref ≈ 0.01 day-1 for cereal residue */
float agri_tillage_residue_decay(float initial_residue_kg_ha,
                                   float temp_c, float moisture, int days) {
    if (initial_residue_kg_ha <= 0.0f || days <= 0) return 0.0f;
    float k_ref = 0.008f;
    float ft = powf(2.0f, (temp_c - 20.0f) / 10.0f);
    if (ft < 0.1f) ft = 0.1f;
    float fw = moisture > 0.15f ? (moisture - 0.10f) / 0.20f : 0.0f;
    if (fw > 1.0f) fw = 1.0f;
    if (fw < 0.0f) fw = 0.0f;
    float k = k_ref * ft * fw;
    return initial_residue_kg_ha * (1.0f - expf(-k * (float)days));
}

/* Soil organic carbon change from tillage transition.
 * IPCC Tier 2: ΔSOC = (SOC_ref × F_LU × F_MG × F_I - SOC_ref) / T
 * where F_LU = land use factor, F_MG = management factor, F_I = input factor */
float agri_tillage_carbon_impact(agri_tillage_type_t from, agri_tillage_type_t to,
                                   float soil_organic_carbon, int years) {
    float f_mg_from, f_mg_to;
    switch (from) {
        case AGRI_TILLAGE_NO_TILL:    f_mg_from = 1.15f; break;
        case AGRI_TILLAGE_REDUCED:    f_mg_from = 1.08f; break;
        case AGRI_TILLAGE_STRIP_TILL: f_mg_from = 1.10f; break;
        default:                      f_mg_from = 1.00f; break;
    }
    switch (to) {
        case AGRI_TILLAGE_NO_TILL:    f_mg_to = 1.15f; break;
        case AGRI_TILLAGE_REDUCED:    f_mg_to = 1.08f; break;
        case AGRI_TILLAGE_STRIP_TILL: f_mg_to = 1.10f; break;
        default:                      f_mg_to = 1.00f; break;
    }
    if (years <= 0) years = 20;
    return soil_organic_carbon * (f_mg_to - f_mg_from) / (float)years;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L9: Precision Agriculture — Variable Rate Technology (VRT)
 *
 * Implements grid-based yield mapping and variable rate prescription
 * generation. Yield maps from combine harvesters (GPS + mass flow sensor)
 * reveal spatial variability that drives site-specific management.
 *
 * Management zones: cluster-based delineation using Jenks natural breaks
 * optimization on multi-layer data (yield, pH, OM, elevation).
 *
 * VRT Prescription: adjusts input rates per grid cell based on:
 *   - Yield potential (higher potential → higher N rate)
 *   - Soil organic matter (higher OM → lower N rate)
 *   - Soil pH (low pH → lime recommendation)
 *
 * Reference: Gebbers & Adamchuk (2010) Science 327(5967):828-831
 *            Grisso et al. (2011) "Precision Farming Tools: Yield Monitor"
 *            Virginia Cooperative Extension Pub 442-502
 * ═══════════════════════════════════════════════════════════════════════════ */

void agri_yield_map_init(agri_yield_map_t* map, int rows, int cols, float cell_size) {
    if (!map || rows <= 0 || cols <= 0) return;
    memset(map, 0, sizeof(agri_yield_map_t));
    map->rows = rows;
    map->cols = cols;
    map->cell_size_m = cell_size;
    size_t sz = (size_t)(rows * cols) * sizeof(float);
    map->yield_data = (float*)malloc(sz);
    map->soil_ph_data = (float*)malloc(sz);
    map->organic_matter_data = (float*)malloc(sz);
    map->elevation_data = (float*)malloc(sz);
    if (map->yield_data) memset(map->yield_data, 0, sz);
    if (map->soil_ph_data) {
        for (size_t i = 0; i < (size_t)(rows * cols); i++)
            map->soil_ph_data[i] = 6.5f;
    }
    if (map->organic_matter_data) {
        for (size_t i = 0; i < (size_t)(rows * cols); i++)
            map->organic_matter_data[i] = 2.5f;
    }
    if (map->elevation_data) memset(map->elevation_data, 0, sz);
}

void agri_yield_map_free(agri_yield_map_t* map) {
    if (!map) return;
    free(map->yield_data);
    free(map->soil_ph_data);
    free(map->organic_matter_data);
    free(map->elevation_data);
    memset(map, 0, sizeof(agri_yield_map_t));
}

static int map_index(const agri_yield_map_t* map, int row, int col) {
    if (!map || row < 0 || row >= map->rows || col < 0 || col >= map->cols)
        return -1;
    return row * map->cols + col;
}

float agri_yield_map_get(const agri_yield_map_t* map, int row, int col) {
    int idx = map_index(map, row, col);
    if (idx < 0 || !map->yield_data) return 0.0f;
    return map->yield_data[idx];
}

void agri_yield_map_set(agri_yield_map_t* map, int row, int col, float value) {
    int idx = map_index(map, row, col);
    if (idx < 0 || !map->yield_data) return;
    map->yield_data[idx] = value;
}

float agri_yield_map_average(const agri_yield_map_t* map) {
    if (!map || !map->yield_data || map->rows * map->cols <= 0) return 0.0f;
    float sum = 0.0f;
    int n = map->rows * map->cols;
    for (int i = 0; i < n; i++) sum += map->yield_data[i];
    return sum / (float)n;
}

float agri_yield_map_cv(const agri_yield_map_t* map) {
    float avg = agri_yield_map_average(map);
    if (avg <= 0.0f) return 0.0f;
    int n = map->rows * map->cols;
    float var = 0.0f;
    for (int i = 0; i < n; i++) {
        float d = map->yield_data[i] - avg;
        var += d * d;
    }
    var /= (float)(n - 1);
    return sqrtf(var) / avg * 100.0f;
}

/* Simple k-means style zone delineation (L5: unsupervised clustering) */
int agri_yield_map_zones(const agri_yield_map_t* map, int num_zones, int* zone_map) {
    if (!map || !map->yield_data || !zone_map || num_zones < 2) return 0;
    int n = map->rows * map->cols;
    int actual_zones = num_zones;
    if (actual_zones > 5) actual_zones = 5;
    if (actual_zones > n) actual_zones = n;

    /* Find min, max yield */
    float ymin = map->yield_data[0], ymax = map->yield_data[0];
    for (int i = 1; i < n; i++) {
        if (map->yield_data[i] < ymin) ymin = map->yield_data[i];
        if (map->yield_data[i] > ymax) ymax = map->yield_data[i];
    }
    float range = ymax - ymin;
    if (range <= 0.0f) range = 1.0f;

    /* Equal-interval zone assignment */
    for (int i = 0; i < n; i++) {
        float norm = (map->yield_data[i] - ymin) / range;
        int zone = (int)(norm * (float)actual_zones);
        if (zone >= actual_zones) zone = actual_zones - 1;
        if (zone < 0) zone = 0;
        zone_map[i] = zone;
    }
    return actual_zones;
}

void agri_vrt_generate_prescription(const agri_yield_map_t* map,
                                      const agri_fertilizer_plan_t* base_plan,
                                      agri_vrt_prescription_t* rx,
                                      int row, int col) {
    if (!map || !base_plan || !rx) return;
    int idx = map_index(map, row, col);
    float yield_factor = 1.0f;
    float om_factor = 1.0f;
    if (idx >= 0) {
        float cell_yield = map->yield_data ? map->yield_data[idx] : 5.0f;
        float avg_yield = agri_yield_map_average(map);
        yield_factor = avg_yield > 0.0f ? cell_yield / avg_yield : 1.0f;
        if (yield_factor < 0.5f) yield_factor = 0.5f;
        if (yield_factor > 1.5f) yield_factor = 1.5f;
        if (map->organic_matter_data) {
            float om = map->organic_matter_data[idx];
            om_factor = agri_fertilizer_variable_rate(1.0f, 1.0f, om)
                       / agri_fertilizer_variable_rate(1.0f, 1.0f, 2.5f);
        }
    }
    rx->seed_rate_kg_ha = 120.0f * yield_factor;
    rx->n_rate_kg_ha = base_plan->nitrogen_kg_ha * yield_factor * om_factor;
    rx->p_rate_kg_ha = base_plan->phosphorus_kg_ha * yield_factor;
    rx->k_rate_kg_ha = base_plan->potassium_kg_ha * yield_factor;
    rx->lime_rate_kg_ha = (map && map->soil_ph_data && map_index(map, row, col) >= 0
                           && map->soil_ph_data[map_index(map, row, col)] < 5.5f)
                          ? 2000.0f : 0.0f;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L8: Grain Quality & Mycotoxin Risk Assessment
 *
 * Mycotoxins are toxic secondary metabolites produced by fungi that
 * contaminate grain crops. Major types:
 *   Aflatoxin (Aspergillus flavus): maize, peanuts — carcinogenic
 *   DON / Vomitoxin (Fusarium graminearum): wheat, barley
 *   Fumonisin (Fusarium verticillioides): maize
 *
 * Risk models use weather-based indices during critical crop stages.
 * FDA action levels: aflatoxin 20 ppb (food), DON 1 ppm (finished wheat)
 *
 * Reference: EFSA Panel on Contaminants (2020) Scientific Opinion
 *            FDA CPG Sec. 555.400, 683.100
 *            De Wolf & Isard (2007) "Disease cycle approach to plant disease"
 * ═══════════════════════════════════════════════════════════════════════════ */

float agri_mycotoxin_aflatoxin_risk(float temp_c, float humidity_pct,
                                      float kernel_damage_pct, agri_crop_type_t crop) {
    if (crop != AGRI_CROP_MAIZE) return 0.0f; /* Aflatoxin primarily in maize */
    /* Optimal conditions: 28-32°C, high humidity (>85%), kernel damage */
    float temp_score;
    if (temp_c >= 28.0f && temp_c <= 32.0f) temp_score = 1.0f;
    else if (temp_c > 20.0f && temp_c < 28.0f) temp_score = (temp_c - 20.0f) / 8.0f;
    else if (temp_c > 32.0f && temp_c < 38.0f) temp_score = (38.0f - temp_c) / 6.0f;
    else temp_score = 0.0f;
    if (temp_score < 0.0f) temp_score = 0.0f;

    float hum_score = humidity_pct > 85.0f ? 1.0f : humidity_pct / 85.0f;
    float damage_score = kernel_damage_pct / 10.0f; /* 10% damage = full risk */
    if (damage_score > 1.0f) damage_score = 1.0f;

    return temp_score * 0.4f + hum_score * 0.3f + damage_score * 0.3f;
}

float agri_mycotoxin_don_risk(float temp_c, float rainfall_mm,
                                agri_crop_type_t crop, float days_post_anthesis) {
    if (crop != AGRI_CROP_WHEAT && crop != AGRI_CROP_MAIZE) return 0.0f;
    /* DON risk highest during flowering (anthesis) with wet, moderate temps */
    float stage_factor;
    if (days_post_anthesis < 0.0f) stage_factor = 0.1f;
    else if (days_post_anthesis < 14.0f) stage_factor = 1.0f; /* Critical period */
    else if (days_post_anthesis < 28.0f) stage_factor = 0.5f;
    else stage_factor = 0.1f;

    float temp_optimal = (temp_c > 15.0f && temp_c < 30.0f) ? 1.0f
                        : (temp_c > 10.0f) ? (temp_c - 10.0f) / 5.0f : 0.0f;
    float rain_factor = rainfall_mm > 5.0f ? 1.0f : rainfall_mm / 5.0f;

    return stage_factor * temp_optimal * rain_factor;
}

void agri_grain_quality_assess(const agri_season_context_t* season,
                                agri_grain_quality_t* quality) {
    if (!season || !quality) return;
    memset(quality, 0, sizeof(agri_grain_quality_t));

    /* Compute average weather during grain fill */
    float avg_temp = 0.0f, avg_humidity = 0.0f, total_rain = 0.0f;
    int grain_fill_days = 0;
    for (int i = 0; i < season->days_simulated; i++) {
        if (season->history[i].stage >= AGRI_STAGE_GRAIN_FILL) {
            avg_temp += season->weather.days[i].temp_avg_c;
            avg_humidity += season->weather.days[i].humidity_pct;
            total_rain += season->weather.days[i].rainfall_mm;
            grain_fill_days++;
        }
    }
    if (grain_fill_days > 0) {
        avg_temp /= (float)grain_fill_days;
        avg_humidity /= (float)grain_fill_days;
    }

    quality->aflatoxin_risk_pct = agri_mycotoxin_aflatoxin_risk(
        avg_temp, avg_humidity, 2.0f, season->config.crop_type) * 100.0f;
    quality->don_risk_pct = agri_mycotoxin_don_risk(
        avg_temp, total_rain, season->config.crop_type,
        (float)(grain_fill_days > 0 ? 7.0f : 0.0f)) * 100.0f;
    quality->fumonisin_risk_pct = season->config.crop_type == AGRI_CROP_MAIZE
                                   ? quality->don_risk_pct * 0.8f : 0.0f;

    /* Protein estimate: inversely related to yield */
    quality->grain_protein_pct = season->predicted_yield_ton_ha > 0.0f
        ? 14.0f - 0.5f * season->predicted_yield_ton_ha : 12.0f;
    if (quality->grain_protein_pct < 8.0f) quality->grain_protein_pct = 8.0f;
    if (quality->grain_protein_pct > 16.0f) quality->grain_protein_pct = 16.0f;

    /* Test weight (kg/hL): indicator of grain density */
    const agri_crop_t* cp = agri_crop_lookup(season->config.crop_type);
    quality->test_weight_kg_hl = cp && cp->type == AGRI_CROP_WHEAT ? 78.0f
                                : cp && cp->type == AGRI_CROP_MAIZE ? 72.0f : 65.0f;
    /* Heat/drought stress reduces test weight */
    float stress = (season->current.water_stress_factor
                   + season->current.temperature_stress_factor) * 0.5f;
    quality->test_weight_kg_hl *= 0.85f + 0.15f * stress;

    /* Falling number (wheat quality, lower = sprouted) */
    quality->falling_number = 350;
    if (total_rain > 100.0f && avg_temp > 20.0f)
        quality->falling_number -= (int)(total_rain * 0.5f);
    if (quality->falling_number < 62) quality->falling_number = 62;

    /* Grade assignment */
    quality->meets_grade1 = (quality->test_weight_kg_hl > 75.0f
                            && quality->grain_protein_pct > 11.0f
                            && quality->aflatoxin_risk_pct < 5.0f
                            && quality->don_risk_pct < 5.0f);
    quality->meets_grade2 = (quality->test_weight_kg_hl > 68.0f
                            && quality->aflatoxin_risk_pct < 15.0f
                            && quality->don_risk_pct < 15.0f);
    if (quality->meets_grade1) strncpy(quality->quality_grade, "US No. 1", 31);
    else if (quality->meets_grade2) strncpy(quality->quality_grade, "US No. 2", 31);
    else strncpy(quality->quality_grade, "Sample Grade", 31);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L6: Intercropping & Relay Planting Systems
 *
 * Intercropping (growing two+ crops simultaneously on the same field)
 * can improve land productivity through complementarity.
 *
 * Land Equivalent Ratio (LER) = (Y_ab / Y_aa) + (Y_ba / Y_bb)
 * where Y_ab = yield of crop A in intercrop with B,
 *       Y_aa = yield of crop A in sole crop.
 * LER > 1 indicates intercropping advantage.
 *
 * Classic combinations:
 *   Maize + Soybean (widely practiced in Africa)
 *   Wheat + Chickpea (South Asia)
 *   Maize + Common Bean (Latin America: "milpa" system)
 *   "Three Sisters": Maize + Bean + Squash (Indigenous Americas)
 *
 * Reference: Willey (1979) "Intercropping — its importance and research needs"
 *            Li et al. (2020) "Syndromes of production in intercropping"
 * ═══════════════════════════════════════════════════════════════════════════ */

void agri_intercrop_evaluate(agri_crop_type_t primary, agri_crop_type_t companion,
                              agri_intercrop_system_t* system) {
    if (!system) return;
    memset(system, 0, sizeof(agri_intercrop_system_t));
    system->primary_crop = primary;
    system->companion_crop = companion;

    /* Planting timing */
    system->primary_planting_delay_days = 0.0f;
    if (companion == AGRI_CROP_SOYBEAN)
        system->companion_planting_delay_days = 14.0f; /* Soybean after maize */
    else
        system->companion_planting_delay_days = 7.0f;

    /* LER estimation based on known pairings */
    if ((primary == AGRI_CROP_MAIZE && companion == AGRI_CROP_SOYBEAN) ||
        (primary == AGRI_CROP_SOYBEAN && companion == AGRI_CROP_MAIZE)) {
        system->land_equivalent_ratio = 1.35f;
        system->nitrogen_benefit_kg_ha = 30.0f;
        system->pest_suppression_pct = 25.0f;
        system->weed_suppression_pct = 35.0f;
        system->yield_advantage_pct = 20.0f;
    } else if ((primary == AGRI_CROP_WHEAT && companion == AGRI_CROP_SOYBEAN) ||
               (primary == AGRI_CROP_SOYBEAN && companion == AGRI_CROP_WHEAT)) {
        system->land_equivalent_ratio = 1.25f;
        system->nitrogen_benefit_kg_ha = 40.0f;
        system->pest_suppression_pct = 30.0f;
        system->weed_suppression_pct = 20.0f;
        system->yield_advantage_pct = 15.0f;
    } else if (primary == AGRI_CROP_MAIZE && companion == AGRI_CROP_POTATO) {
        system->land_equivalent_ratio = 1.40f;
        system->nitrogen_benefit_kg_ha = 10.0f;
        system->pest_suppression_pct = 15.0f;
        system->weed_suppression_pct = 40.0f;
        system->yield_advantage_pct = 25.0f;
    } else {
        system->land_equivalent_ratio = 1.10f;
        system->nitrogen_benefit_kg_ha = 15.0f;
        system->pest_suppression_pct = 10.0f;
        system->weed_suppression_pct = 15.0f;
        system->yield_advantage_pct = 8.0f;
    }

    /* Self-same crop: no benefit */
    if (primary == companion) {
        system->land_equivalent_ratio = 1.0f;
        system->nitrogen_benefit_kg_ha = 0.0f;
        system->yield_advantage_pct = 0.0f;
    }
}

float agri_land_equivalent_ratio(agri_crop_type_t primary, float primary_yield,
                                   float primary_solo_yield,
                                   agri_crop_type_t companion,
                                   float companion_yield, float companion_solo_yield) {
    (void)primary; (void)companion; /* Crop types retained for API consistency */
    float ler_a = primary_solo_yield > 0.0f
                  ? primary_yield / primary_solo_yield : 0.0f;
    float ler_b = companion_solo_yield > 0.0f
                  ? companion_yield / companion_solo_yield : 0.0f;
    return ler_a + ler_b;
}
