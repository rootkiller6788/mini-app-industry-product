#ifndef ENERGY_WIND_H
#define ENERGY_WIND_H

#include "energy_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Wind Resource Assessment (L5: Weibull Statistics) ────────────────── */

/* Weibull distribution parameters (L4: Extreme Value Theory) */
typedef struct {
    float  k;                  /* shape parameter (1.5-3.0 typical) */
    float  c;                  /* scale parameter m/s (5-12 typical) */
    float  mean_speed_m_s;     /* arithmetic mean */
    float  mean_power_density_w_m2; /* 0.5 * rho * mean(v³) */
    float  most_probable_m_s;  /* most probable wind speed */
    float  max_energy_m_s;     /* speed delivering max energy */
    int    num_samples;
    float  confidence_95_k;    /* 95% confidence bounds */
    float  confidence_95_c;
} energy_weibull_t;

/* Wind measurement mast data */
typedef struct {
    float  height_m;
    float  speeds_m_s[ENERGY_MAX_TIMESERIES];
    float  directions_deg[ENERGY_MAX_TIMESERIES];
    int    num_measurements;
    float  measurement_interval_min;
} energy_wind_mast_t;

/* Wind shear profile (L4: Atmospheric Boundary Layer) */
typedef struct {
    float  reference_height_m;
    float  reference_speed_m_s;
    float  roughness_length_m;  /* z0: 0.0002(sea) to 2.0(city) */
    float  power_law_exponent;  /* alpha: 0.10-0.40 */
    float  turbine_hub_height_m;
} energy_wind_shear_t;

/* Turbine power curve (L5: Piecewise Model) */
typedef struct {
    char   model_name[ENERGY_MAX_NAME];
    float  rotor_diameter_m;
    float  hub_height_m;
    float  rated_power_kw;
    float  cut_in_speed_m_s;
    float  rated_speed_m_s;
    float  cut_out_speed_m_s;
    float  cp_max;              /* max power coefficient */
    float  thrust_coefficient;   /* CT for wake models */
    /* Piecewise curve points */
    float  speeds_m_s[50];
    float  powers_kw[50];
    int    num_curve_points;
} energy_wind_turbine_t;

/* Wind farm layout (L8: Wake Effects) */
typedef struct {
    float  x_m;                /* easting coordinate */
    float  y_m;                /* northing coordinate */
    int    turbine_count;
    float  turbine_spacing_rotor_d; /* spacing in rotor diameters */
    float  prevailing_wind_dir_deg;
    float  wake_decay_constant; /* Jensen model: 0.075 onshore, 0.04 offshore */
    bool   is_offshore;
} energy_wind_farm_t;

/* ── API: Weibull Distribution (L5: MLE / Method of Moments) ─────────── */

/* Fit 2-parameter Weibull to wind speed data */
energy_weibull_t energy_weibull_fit_mle(const float* speeds, int n);
energy_weibull_t energy_weibull_fit_mom(const float* speeds, int n);
energy_weibull_t energy_weibull_fit_empirical(const float* speeds, int n);

/* Weibull probability functions */
float energy_weibull_pdf(float v, float k, float c);
float energy_weibull_cdf(float v, float k, float c);
float energy_weibull_inverse_cdf(float p, float k, float c);

/* Key Weibull statistics */
float energy_weibull_mean(float k, float c);
float energy_weibull_most_probable(float k, float c);
float energy_weibull_max_energy_speed(float k, float c);
float energy_weibull_capacity_factor(const energy_weibull_t* wb,
                                      const energy_wind_turbine_t* wt);

/* ── API: Wind Shear (L4: Boundary Layer Theory) ─────────────────────── */

/* Power law wind profile: v2 = v1 * (h2/h1)^alpha */
float energy_wind_shear_power_law(float v_ref, float h_ref, float h_target,
                                   float alpha);
/* Log law wind profile: v2 = v1 * ln(h2/z0) / ln(h1/z0) */
float energy_wind_shear_log_law(float v_ref, float h_ref, float h_target,
                                 float z0);
/* Estimate roughness length from terrain type */
float energy_roughness_length(const char* terrain_type);
/* Estimate power law exponent from roughness */
float energy_power_law_exponent(float z0);

/* ── API: Air Density Correction (L4: Ideal Gas Law) ─────────────────── */
float energy_air_density(float temperature_c, float pressure_hpa,
                          float humidity_pct);

/* ── API: Turbine Power (L5: Piecewise Power Curve) ──────────────────── */

/* Power output from piecewise power curve interpolation */
float energy_turbine_power_kw(const energy_wind_turbine_t* wt,
                               float wind_speed_m_s);
/* Ideal Betz-limited power */
float energy_turbine_power_betz(float rotor_diameter_m, float wind_speed_m_s,
                                 float air_density);
/* Annual Energy Production (AEP) with Weibull */
float energy_turbine_aep_kwh(const energy_wind_turbine_t* wt,
                              const energy_weibull_t* wb);
/* Wake-adjusted power for downstream turbine */
float energy_turbine_power_wake(const energy_wind_turbine_t* wt,
                                 float upstream_speed_m_s, float distance_rotor_d,
                                 float thrust_coeff, float wake_decay);

/* ── API: Wind Farm (L8: Wake Modeling – Jensen 1983) ───────────────── */
float energy_wake_deficit(float distance_rotor_d, float ct,
                           float wake_decay_constant);
float energy_wind_farm_aep_gwh(const energy_wind_farm_t* farm,
                                const energy_wind_turbine_t* wt_template,
                                const energy_weibull_t* wb);
float energy_wind_farm_efficiency(const energy_wind_farm_t* farm);

/* ── Extreme Wind Speed (L8: Gumbel Distribution) ────────────────────── */
float energy_gumbel_cdf(float x, float mu, float beta);
float energy_gumbel_return_level(float return_period_years,
                                  const float* annual_max, int n_years);

/* ── Utility ──────────────────────────────────────────────────────────── */
float energy_cut_in_ratio(const energy_wind_turbine_t* wt);
float energy_wind_class_from_speed(float mean_speed_ms_50m);
const char* energy_wind_class_name(float mean_speed_ms_50m);
int   energy_wind_direction_sector(float direction_deg, int num_sectors);

#ifdef __cplusplus
}
#endif

#endif
