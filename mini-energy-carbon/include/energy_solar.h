#ifndef ENERGY_SOLAR_H
#define ENERGY_SOLAR_H

#include "energy_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Solar Geometry (L5: Algorithm → NREL SPA) ────────────────────────── */

typedef struct {
    float  latitude_deg;       /* positive = north */
    float  longitude_deg;      /* positive = east */
    float  elevation_m;        /* above sea level */
    float  timezone_offset_h;  /* UTC offset */
} energy_location_t;

typedef struct {
    float  declination_deg;    /* solar declination */
    float  hour_angle_deg;     /* hour angle */
    float  elevation_deg;      /* solar elevation */
    float  azimuth_deg;        /* solar azimuth (0=N, 90=E) */
    float  sunrise_h;          /* sunrise decimal hour */
    float  sunset_h;           /* sunset decimal hour */
    float  day_length_h;       /* daylight duration */
    float  equation_of_time_min;/* EoT in minutes */
    float  extraterrestrial_w_m2;/* top-of-atmosphere irradiance */
} energy_solar_geometry_t;

/* ── Irradiance Models (L5: Algorithm) ────────────────────────────────── */

typedef struct {
    float  ghi_w_m2;           /* Global Horizontal Irradiance */
    float  dni_w_m2;           /* Direct Normal Irradiance */
    float  dhi_w_m2;           /* Diffuse Horizontal Irradiance */
    float  gti_w_m2;           /* Global Tilted Irradiance */
    float  clearness_index;    /* Kt = GHI / ET */
    float  diffuse_fraction;   /* Kd = DHI / GHI */
} energy_irradiance_t;

typedef struct {
    float  tilt_deg;           /* panel tilt from horizontal */
    float  azimuth_deg;        /* panel azimuth (0=N, 90=E) */
    float  albedo;             /* ground reflectance (typical 0.2) */
    bool   is_tracking;        /* single-axis or dual-axis tracking */
    int    tracking_axis;      /* 0=fixed, 1=horizontal, 2=vertical, 3=dual */
} energy_panel_orientation_t;

/* ── PV Module Models (L5: Single-Diode) ──────────────────────────────── */

typedef struct {
    float  i_sc_A;             /* short-circuit current at STC */
    float  v_oc_V;             /* open-circuit voltage at STC */
    float  i_mp_A;             /* current at max power */
    float  v_mp_V;             /* voltage at max power */
    float  p_max_W;            /* max power at STC */
    float  temp_coeff_i_pct_K; /* temperature coefficient of Isc */
    float  temp_coeff_v_pct_K; /* temperature coefficient of Voc */
    float  temp_coeff_p_pct_K; /* temperature coefficient of Pmax */
    float  noct_c;             /* nominal operating cell temperature */
    float  series_resistance_ohm; /* Rs for single-diode model */
    float  shunt_resistance_ohm;  /* Rsh */
    float  ideality_factor;    /* diode ideality n, typically 1.0-1.5 */
    float  modules_in_series;  /* string configuration */
    float  modules_in_parallel;
    float  degradation_first_year_pct;
    float  degradation_annual_pct;
} energy_pv_module_t;

typedef struct {
    float  power_kw;
    float  voltage_v;
    float  current_a;
    float  cell_temp_c;
    float  efficiency_pct;
} energy_pv_output_t;

/* ── Solar Thermal Collector (L7: Applications) ──────────────────────── */

typedef struct {
    float  aperture_area_m2;
    float  f_ta_eta0;          /* optical efficiency (zero-loss) */
    float  f_ta_a1;            /* first-order heat loss coeff W/(m²·K) */
    float  f_ta_a2;            /* second-order heat loss coeff W/(m²·K²) */
    float  incidence_angle_modifier; /* IAM at 50° */
    float  fluid_heat_capacity_kj_K;
} energy_solar_thermal_collector_t;

/* ── API: Solar Geometry (L5: Meeus / Reda-Andreas) ──────────────────── */

/* Compute full solar geometry for given location and UTC time */
energy_solar_geometry_t energy_solar_geometry(const energy_location_t* loc,
                                               int year, int month, int day,
                                               float decimal_hour);

/* Individual components */
float energy_solar_declination(int day_of_year);
float energy_solar_hour_angle(float longitude_deg, float timezone_offset_h,
                               int day_of_year, float decimal_hour);
float energy_solar_elevation(float latitude_deg, float declination_deg,
                              float hour_angle_deg);
float energy_solar_azimuth(float latitude_deg, float declination_deg,
                            float hour_angle_deg, float elevation_deg);
float energy_solar_sunrise_hour(float latitude_deg, float declination_deg);
float energy_day_of_year(int year, int month, int day);

/* Extraterrestrial radiation (L4: Solar constant) */
float energy_extraterrestrial_radiation(int day_of_year);
float energy_air_mass(float elevation_deg, float altitude_m);

/* ── API: Irradiance Decomposition (L5: HDKR Model) ──────────────────── */

/* Decompose GHI into beam + diffuse using Erbs model */
void  energy_irradiance_decompose(energy_irradiance_t* irr, float ghi,
                                   const energy_solar_geometry_t* geo);
/* Compute irradiance on tilted surface using HDKR anisotropic sky model */
float energy_irradiance_tilted(const energy_irradiance_t* irr,
                                const energy_solar_geometry_t* geo,
                                const energy_panel_orientation_t* orient);
/* Clear-sky irradiance (simplified Haurwitz model) */
float energy_clearsky_ghi(float elevation_deg, float altitude_m);

/* ── API: PV Output (L5: Single-Diode Model) ─────────────────────────── */

/* Full PV output computation */
energy_pv_output_t energy_pv_output(const energy_pv_module_t* pv,
                                     float gti_w_m2, float ambient_temp_c,
                                     float wind_speed_m_s);

/* Single-diode model: I-V curve */
float energy_pv_current_A(float voltage_V, const energy_pv_module_t* pv,
                           float irradiance_w_m2, float cell_temp_c);
float energy_pv_max_power_point(const energy_pv_module_t* pv,
                                 float irradiance_w_m2, float cell_temp_c,
                                 float* v_mp, float* i_mp);

/* PV temperature model */
float energy_pv_cell_temperature(float ambient_temp_c, float irradiance_w_m2,
                                  float noct_c, float wind_speed_m_s);

/* PV degradation over time */
float energy_pv_degraded_power(const energy_pv_module_t* pv, int year);

/* ── Advanced Topics (L8): Bifacial, Soiling, Shading ────────────────── */

float energy_pv_bifacial_gain(float gti_front, float albedo, float tilt_deg,
                               float bifacial_factor);
float energy_pv_soiling_loss(float base_power, float soiling_ratio_pct,
                              int days_since_cleaning);
float energy_pv_shading_loss(float unshaded_power, float shaded_fraction,
                              float topology_factor);

/* ── Solar Thermal (L7: Applications) ─────────────────────────────────── */

float energy_solar_thermal_power_kw(const energy_solar_thermal_collector_t* sc,
                                     float gti_w_m2, float ambient_temp_c,
                                     float fluid_inlet_temp_c);
float energy_solar_thermal_efficiency(const energy_solar_thermal_collector_t* sc,
                                       float delta_T, float gti_w_m2);

/* ── Utility ──────────────────────────────────────────────────────────── */
float energy_albedo_lookup(const char* surface_type);
int   energy_is_daylight(const energy_solar_geometry_t* geo);

#ifdef __cplusplus
}
#endif

#endif
