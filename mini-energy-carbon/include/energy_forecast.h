#ifndef ENERGY_FORECAST_H
#define ENERGY_FORECAST_H

#include "energy_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Forecasting Models (L5: Time Series) ─────────────────────────────── */

/* Holt-Winters state (L5: Exponential Smoothing) */
typedef struct {
    float  level;
    float  trend;
    float  seasonal[ENERGY_MAX_SEASONS * 24]; /* hourly seasonal factors */
    int    period;                /* season length (e.g., 24 for hourly) */
    int    num_seasons;
    float  alpha;                 /* level smoothing */
    float  beta;                  /* trend smoothing */
    float  gamma;                 /* seasonal smoothing */
    bool   is_multiplicative;
    bool   is_initialized;
} energy_holt_winters_t;

/* ARIMA model parameters (L5: Box-Jenkins) */
typedef struct {
    int    p;                     /* AR order */
    int    d;                     /* differencing order */
    int    q;                     /* MA order */
    int    seasonal_p;
    int    seasonal_d;
    int    seasonal_q;
    int    seasonal_period;
    float  ar_coeffs[32];         /* phi coefficients */
    float  ma_coeffs[32];         /* theta coefficients */
    float  seasonal_ar[32];
    float  seasonal_ma[32];
    float  constant;              /* drift / intercept */
    float  sigma2;                /* residual variance */
    bool   is_fitted;
} energy_arima_t;

/* Seasonal-Trend decomposition (STL) (L5: Cleveland 1990) */
typedef struct {
    float  trend[ENERGY_MAX_TIMESERIES];
    float  seasonal[ENERGY_MAX_TIMESERIES];
    float  residual[ENERGY_MAX_TIMESERIES];
    int    n;
    int    period;
} energy_stl_decomp_t;

/* Forecast error metrics (L5: Model Evaluation) */
typedef struct {
    float  mae;                   /* mean absolute error */
    float  rmse;                  /* root mean square error */
    float  mape;                  /* mean absolute percentage error */
    float  smape;                 /* symmetric MAPE */
    float  r_squared;             /* coefficient of determination */
    float  bias;                  /* mean error */
    float  nash_sutcliffe;        /* NSE efficiency */
} energy_forecast_metrics_t;

/* Probabilistic forecast ensemble */
typedef struct {
    float  quantile_10;
    float  quantile_25;
    float  quantile_50;           /* median */
    float  quantile_75;
    float  quantile_90;
    float  mean;
    float  std_dev;
} energy_probabilistic_forecast_t;

/* Weather-driven forecast inputs (L8: NWP integration) */
typedef struct {
    float  temperature_c;
    float  cloud_cover_pct;
    float  wind_speed_m_s;
    float  wind_direction_deg;
    float  humidity_pct;
    float  pressure_hpa;
    float  ghi_w_m2;
    float  precipitation_mm;
    int    forecast_hour_ahead;
} energy_weather_forecast_t;

/* ── API: Holt-Winters (L5: Triple Exponential Smoothing) ────────────── */

void  energy_holt_winters_init(energy_holt_winters_t* hw, int period,
                                bool multiplicative);
void  energy_holt_winters_fit(energy_holt_winters_t* hw, const float* series,
                               int n);
float energy_holt_winters_forecast(const energy_holt_winters_t* hw,
                                    int steps_ahead);
void  energy_holt_winters_update(energy_holt_winters_t* hw, float new_value);
void  energy_holt_winters_forecast_n(const energy_holt_winters_t* hw,
                                      float* forecasts, int n_ahead);

/* ── API: ARIMA (L5: Box-Jenkins) ─────────────────────────────────────── */

void  energy_arima_init(energy_arima_t* arima, int p, int d, int q);
void  energy_arima_seasonal(energy_arima_t* arima, int P, int D, int Q, int s);
void  energy_arima_fit_ols(energy_arima_t* arima, const float* series, int n);
float energy_arima_forecast(const energy_arima_t* arima, const float* series,
                             int n, int steps_ahead);
void  energy_arima_forecast_n(const energy_arima_t* arima,
                               const float* series, int n,
                               float* forecasts, int n_ahead);

/* ── API: STL Decomposition (L5: Loess-based) ─────────────────────────── */

energy_stl_decomp_t energy_stl_decompose(const float* series, int n,
                                          int period);
float energy_stl_seasonal_adjust(const energy_stl_decomp_t* stl, int i);

/* ── API: Similar Day / Persistence Forecasts (L5: Baselines) ─────────── */

float energy_similar_day_forecast(const energy_timeslot_t* history, int n,
                                   int target_month, int target_day_type,
                                   int target_hour);
float energy_persistence_forecast(const float* series, int n,
                                   int steps_ahead);
float energy_clear_sky_index_forecast(const float* ghi, const float* clearsky,
                                       int n, int steps_ahead);

/* ── API: Forecast Metrics (L5: Model Evaluation) ────────────────────── */

energy_forecast_metrics_t energy_forecast_evaluate(const float* actual,
                                                    const float* predicted,
                                                    int n);
float energy_forecast_skill_score(const energy_forecast_metrics_t* model,
                                   const energy_forecast_metrics_t* reference);

/* ── API: Probabilistic Forecasting (L8: Quantile Regression) ─────────── */

energy_probabilistic_forecast_t energy_forecast_ensemble(
    const float* ensemble_members, int n_members);
float energy_forecast_pinball_loss(const float* actual, int n_actual,
                                    const float* quantile_forecast,
                                    float quantile);
float energy_forecast_crps(const float* ensemble, int n_members,
                            float observation);

/* ── API: Weather-Normalized Load (L7: Applications) ──────────────────── */

float energy_weather_normalize_load(const energy_timeslot_t* ts, int n,
                                     int target_hour);
float energy_heating_cooling_balance_point(const energy_timeslot_t* ts,
                                            int n, float* t_balance);

#ifdef __cplusplus
}
#endif

#endif
