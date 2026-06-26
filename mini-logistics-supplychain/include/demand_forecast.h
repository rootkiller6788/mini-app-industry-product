/**
 * demand_forecast.h — Demand Forecasting & Time Series
 *
 * L4-L5: Statistical forecasting methods for supply chain demand.
 *
 * Core Concepts (L2):
 *   - Time series decomposition: level, trend, seasonality
 *   - Forecast error metrics: MAD, MSE, MAPE, tracking signal
 *   - Demand sensing and short-term forecasting
 *
 * Algorithms (L5):
 *   - Simple Moving Average (SMA)
 *   - Weighted Moving Average (WMA)
 *   - Single Exponential Smoothing (SES)
 *   - Holt's Double Exponential Smoothing (trend-corrected)
 *   - Holt-Winters Triple Exponential Smoothing (seasonal)
 *
 * Standards (L4):
 *   - Exponential smoothing: Brown (1959), Holt (1957), Winters (1960)
 *   - Forecast accuracy: MAD/MSE/MAPE formulae
 *
 * Course Alignment:
 *   - MIT 15.060 Data, Models, and Decisions
 *   - Georgia Tech ISYE 6402 Time Series Analysis
 *   - CMU 45-880 Demand Forecasting and Planning
 *   - Stanford MS&E 246 Supply Chain Management
 */

#ifndef DEMAND_FORECAST_H
#define DEMAND_FORECAST_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * L1 — Forecast Model Enums & Structures
 * ============================================================ */

/** Forecast model type */
typedef enum {
    FORECAST_SMA              = 0,  /* Simple Moving Average */
    FORECAST_WMA              = 1,  /* Weighted Moving Average */
    FORECAST_SES              = 2,  /* Single Exponential Smoothing */
    FORECAST_HOLT             = 3,  /* Holt's Double ES (level + trend) */
    FORECAST_HOLT_WINTERS_ADD = 4,  /* HW additive seasonality */
    FORECAST_HOLT_WINTERS_MUL = 5   /* HW multiplicative seasonality */
} forecast_model_t;

/** Error metric type */
typedef enum {
    ERROR_MAD   = 0,  /* Mean Absolute Deviation */
    ERROR_MSE   = 1,  /* Mean Squared Error */
    ERROR_RMSE  = 2,  /* Root Mean Squared Error */
    ERROR_MAPE  = 3,  /* Mean Absolute Percentage Error */
    ERROR_MD    = 4   /* Mean Deviation (bias) */
} error_metric_t;

/** A single forecast result */
typedef struct {
    double  forecast_value;   /* Point forecast */
    double  lower_ci;         /* Lower confidence interval (95%) */
    double  upper_ci;         /* Upper confidence interval (95%) */
    double  forecast_error;   /* One-step-ahead error if actual known */
} forecast_point_t;

/** Simple Moving Average model state */
typedef struct {
    double  *window;          /* Circular buffer of recent values */
    int      window_size;
    int      count;           /* Number of values added so far */
    int      index;           /* Current write position */
    double   current_avg;
} sma_model_t;

/** Weighted Moving Average model state */
typedef struct {
    double  *window;
    double  *weights;         /* Precomputed weight array (sum = 1.0) */
    int      window_size;
    int      count;
    int      index;
} wma_model_t;

/** Single Exponential Smoothing state */
typedef struct {
    double  alpha;            /* Smoothing constant (0 < α < 1) */
    double  level;            /* Current level estimate */
    double  last_forecast;
    int     initialized;
} ses_model_t;

/** Holt's Double Exponential Smoothing (level + trend) */
typedef struct {
    double  alpha;            /* Level smoothing constant */
    double  beta;             /* Trend smoothing constant */
    double  level;
    double  trend;
    double  last_forecast;
    int     initialized;
} holt_model_t;

/**
 * Holt-Winters Triple Exponential Smoothing
 * (level + trend + seasonality)
 */
typedef struct {
    double  alpha;            /* Level smoothing constant */
    double  beta;             /* Trend smoothing constant */
    double  gamma;            /* Seasonal smoothing constant */
    int     season_length;    /* Number of periods per season (e.g., 12 for monthly) */
    int     is_multiplicative; /* 0 = additive, 1 = multiplicative */
    double  level;
    double  trend;
    double  *seasonal;        /* Seasonal indices (length = season_length) */
    int     count;            /* Observations processed */
    double  last_forecast;
    int     initialized;
} holt_winters_model_t;

/* ============================================================
 * L1 — Forecast Accuracy
 * ============================================================ */

/**
 * Compute forecast error metrics between actual and forecast arrays.
 *
 *   MAD  = (1/n) Σ|e_t|          where e_t = actual_t - forecast_t
 *   MSE  = (1/n) Σ(e_t)²
 *   RMSE = sqrt(MSE)
 *   MAPE = (100/n) Σ|e_t / actual_t|
 *   MD   = (1/n) Σ(e_t)          (positive = forecast too low)
 *
 * Complexity: O(n)
 *
 * @param actual     Actual observed values
 * @param forecast   Forecasted values
 * @param n          Number of data points
 * @param metric     Which error metric to compute
 * @return Error metric value
 */
double forecast_error(const double *actual, const double *forecast,
                      int n, error_metric_t metric);

/* ============================================================
 * L5 — Moving Average Models
 * ============================================================ */

/**
 * Initialize a Simple Moving Average model.
 * Complexity: O(1) + O(window_size) allocation
 */
int  sma_init(sma_model_t *model, int window_size);

/**
 * Add a new observation and update the moving average.
 * Complexity: O(1)
 */
void sma_update(sma_model_t *model, double value);

/**
 * Get the current moving average forecast.
 * Complexity: O(1)
 */
double sma_forecast(const sma_model_t *model);

/**
 * Free SMA model memory.
 * Complexity: O(1)
 */
void sma_destroy(sma_model_t *model);

/**
 * Initialize a Weighted Moving Average model.
 * Weights are normalized internally (forgot = 1.0, each weight ≥ 0).
 * Weights should be in chronological order (oldest first).
 * Complexity: O(window_size)
 */
int  wma_init(wma_model_t *model, int window_size, const double *weights);

/**
 * Add a new observation and update the weighted moving average.
 * Complexity: O(1)
 */
void wma_update(wma_model_t *model, double value);

/**
 * Get the current weighted moving average forecast.
 * Complexity: O(window_size)
 */
double wma_forecast(const wma_model_t *model);

/**
 * Free WMA model memory.
 * Complexity: O(1)
 */
void wma_destroy(wma_model_t *model);

/* ============================================================
 * L5 — Exponential Smoothing Models
 * ============================================================ */

/**
 * Initialize a Single Exponential Smoothing model.
 *
 * Theorem (Brown, 1959): SES is optimal for a random walk with
 * noise process (ARIMA(0,1,1) = IMA(1,1)):
 *   Y_t = Y_{t-1} + ε_t - θ·ε_{t-1}
 * where α = 1 - θ.
 *
 * Complexity: O(1)
 */
void ses_init(ses_model_t *model, double alpha);

/**
 * Update SES with new observation.
 *
 *   level_t = α·Y_t + (1-α)·level_{t-1}
 *   forecast_{t+1} = level_t
 *
 * Complexity: O(1)
 */
void ses_update(ses_model_t *model, double value);

/**
 * Get the current SES forecast.
 * Complexity: O(1)
 */
double ses_forecast(const ses_model_t *model);

/**
 * Get k-step-ahead forecast from SES.
 *
 *   Y_{t+k|t} = level_t
 *
 * (SES forecasts are flat — no trend)
 * Complexity: O(1)
 */
double ses_forecast_k(const ses_model_t *model, int k);

/* ============================================================
 * L5 — Holt's Double Exponential Smoothing
 * ============================================================ */

/**
 * Initialize Holt's model.
 *
 * Theorem (Holt, 1957): Extends SES by adding a trend component:
 *   level_t   = α·Y_t + (1-α)·(level_{t-1} + trend_{t-1})
 *   trend_t   = β·(level_t - level_{t-1}) + (1-β)·trend_{t-1}
 *   forecast_{t+k} = level_t + k·trend_t
 *
 * This is equivalent to ARIMA(0,2,2).
 *
 * Complexity: O(1)
 */
void holt_init(holt_model_t *model, double alpha, double beta);

/**
 * Update Holt's model with new observation.
 * Complexity: O(1)
 */
void holt_update(holt_model_t *model, double value);

/**
 * Get k-step-ahead forecast from Holt's model.
 * Complexity: O(1)
 */
double holt_forecast_k(const holt_model_t *model, int k);

/**
 * Get the current level and trend components.
 * Complexity: O(1)
 */
void holt_get_components(const holt_model_t *model, double *level, double *trend);

/* ============================================================
 * L5 — Holt-Winters Triple Exponential Smoothing
 * ============================================================ */

/**
 * Initialize Holt-Winters model.
 *
 * Theorem (Winters, 1960): Adds seasonal component to Holt's:
 *
 * Additive:
 *   level_t    = α·(Y_t - S_{t-m}) + (1-α)·(level_{t-1} + trend_{t-1})
 *   trend_t    = β·(level_t - level_{t-1}) + (1-β)·trend_{t-1}
 *   S_t        = γ·(Y_t - level_t) + (1-γ)·S_{t-m}
 *   forecast_{t+k} = level_t + k·trend_t + S_{t-m+k mod m}
 *
 * Multiplicative:
 *   level_t    = α·(Y_t / S_{t-m}) + (1-α)·(level_{t-1} + trend_{t-1})
 *   trend_t    = β·(level_t - level_{t-1}) + (1-β)·trend_{t-1}
 *   S_t        = γ·(Y_t / level_t) + (1-γ)·S_{t-m}
 *   forecast_{t+k} = (level_t + k·trend_t) × S_{t-m+k mod m}
 *
 * Complexity: O(season_length) for initialization
 */
int  holt_winters_init(holt_winters_model_t *model,
                       double alpha, double beta, double gamma,
                       int season_length, int is_multiplicative);

/**
 * Initialize seasonal indices from historical data.
 * Must be called before first update.
 * Complexity: O(K·season_length) where K = number of full seasons
 */
int  holt_winters_fit_seasonal(holt_winters_model_t *model,
                               const double *data, int data_len);

/**
 * Update Holt-Winters model with new observation.
 * Complexity: O(1)
 */
void holt_winters_update(holt_winters_model_t *model, double value);

/**
 * Get k-step-ahead forecast from Holt-Winters model.
 * Complexity: O(1)
 */
double holt_winters_forecast_k(const holt_winters_model_t *model, int k);

/**
 * Free Holt-Winters model memory.
 * Complexity: O(1)
 */
void holt_winters_destroy(holt_winters_model_t *model);

/* ============================================================
 * L2/L4 — Tracking Signal
 * ============================================================ */

/**
 * Compute the tracking signal (TS).
 *
 *   TS = Σe_t / MAD
 *
 * If |TS| > threshold (typically 4 or 5), the forecast is
 * considered biased and needs recalibration.
 *
 * Theorem: Under normally distributed errors, TS follows
 * a standardized normal distribution.
 *
 * Complexity: O(1) if MAD precomputed
 */
double tracking_signal(double cumulative_error, double mad);

/**
 * Simple linear regression for trend estimation.
 *
 *   Y = a + b·X
 *   b = Σ((x_i - x̄)(y_i - ȳ)) / Σ((x_i - x̄)²)
 *   a = ȳ - b·x̄
 *
 * Complexity: O(n)
 *
 * @param x     Independent variable (e.g., time indices)
 * @param y     Dependent variable (demand)
 * @param n     Number of data points
 * @param slope Output: slope b
 * @param intercept Output: intercept a
 */
void linear_regression(const double *x, const double *y, int n,
                       double *slope, double *intercept);

#endif /* DEMAND_FORECAST_H */
