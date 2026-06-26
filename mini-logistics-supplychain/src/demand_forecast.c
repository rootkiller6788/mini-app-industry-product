/**
 * demand_forecast.c — Demand Forecasting Implementations
 *
 * L5: Simple/Weighted Moving Average, Single/Double/Triple
 *     Exponential Smoothing (SES, Holt, Holt-Winters)
 * L2: Forecast error metrics (MAD, MSE, RMSE, MAPE, MD)
 * L4: Tracking signal, linear regression for trend
 *
 * References:
 *   - Brown, R.G. (1959). Statistical Forecasting for Inventory Control
 *   - Holt, C.C. (1957). Forecasting seasonals and trends...
 *   - Winters, P.R. (1960). Forecasting sales by exponentially
 *     weighted moving averages. Management Science, 6(3), 324-342.
 */

#include "demand_forecast.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================
 * L2 — Forecast Error Metrics
 * ============================================================ */
double forecast_error(const double *actual, const double *forecast,
                      int n, error_metric_t metric) {
    if (!actual || !forecast || n <= 0) return 0.0;

    double sum = 0.0;
    int valid_count = 0;

    switch (metric) {
        case ERROR_MAD: {
            for (int i = 0; i < n; i++) {
                sum += fabs(actual[i] - forecast[i]);
                valid_count++;
            }
            return (valid_count > 0) ? sum / valid_count : 0.0;
        }
        case ERROR_MSE: {
            for (int i = 0; i < n; i++) {
                double e = actual[i] - forecast[i];
                sum += e * e;
                valid_count++;
            }
            return (valid_count > 0) ? sum / valid_count : 0.0;
        }
        case ERROR_RMSE: {
            for (int i = 0; i < n; i++) {
                double e = actual[i] - forecast[i];
                sum += e * e;
                valid_count++;
            }
            return (valid_count > 0) ? sqrt(sum / valid_count) : 0.0;
        }
        case ERROR_MAPE: {
            for (int i = 0; i < n; i++) {
                if (fabs(actual[i]) > 1e-9) {
                    sum += fabs((actual[i] - forecast[i]) / actual[i]);
                    valid_count++;
                }
            }
            return (valid_count > 0) ? (sum / valid_count) * 100.0 : 0.0;
        }
        case ERROR_MD: {
            for (int i = 0; i < n; i++) {
                sum += (actual[i] - forecast[i]);
                valid_count++;
            }
            return (valid_count > 0) ? sum / valid_count : 0.0;
        }
        default:
            return 0.0;
    }
}

/* ============================================================
 * L5 — Simple Moving Average (SMA)
 * ============================================================ */
int sma_init(sma_model_t *model, int window_size) {
    if (!model || window_size <= 0) return -1;
    memset(model, 0, sizeof(sma_model_t));
    model->window = (double *)calloc(window_size, sizeof(double));
    if (!model->window) return -1;
    model->window_size = window_size;
    model->count = 0;
    model->index = 0;
    model->current_avg = 0.0;
    return 0;
}

void sma_update(sma_model_t *model, double value) {
    if (!model || !model->window || model->window_size <= 0) return;

    /* Insert new value into circular buffer */
    model->window[model->index] = value;
    model->index = (model->index + 1) % model->window_size;

    /* Track count of valid entries (caps at window_size) */
    if (model->count < model->window_size) {
        model->count++;
    }

    /* Recompute average from all valid entries */
    double sum = 0.0;
    for (int i = 0; i < model->count; i++) {
        sum += model->window[i];
    }
    model->current_avg = sum / (double)model->count;
}

double sma_forecast(const sma_model_t *model) {
    if (!model || model->count == 0) return 0.0;
    return model->current_avg;
}

void sma_destroy(sma_model_t *model) {
    if (!model) return;
    free(model->window);
    model->window = NULL;
}

/* ============================================================
 * L5 — Weighted Moving Average (WMA)
 * ============================================================ */
int wma_init(wma_model_t *model, int window_size, const double *weights) {
    if (!model || window_size <= 0 || !weights) return -1;
    memset(model, 0, sizeof(wma_model_t));

    model->window = (double *)calloc(window_size, sizeof(double));
    model->weights = (double *)malloc(window_size * sizeof(double));
    if (!model->window || !model->weights) {
        free(model->window);
        free(model->weights);
        return -1;
    }

    /* Normalize weights to sum to 1.0 */
    double sum_w = 0.0;
    for (int i = 0; i < window_size; i++) {
        if (weights[i] < 0.0) { model->weights[i] = 0.0; continue; }
        model->weights[i] = weights[i];
        sum_w += weights[i];
    }
    if (sum_w > 0.0) {
        for (int i = 0; i < window_size; i++) {
            model->weights[i] /= sum_w;
        }
    } else {
        /* Equal weights as fallback */
        for (int i = 0; i < window_size; i++) {
            model->weights[i] = 1.0 / (double)window_size;
        }
    }

    model->window_size = window_size;
    return 0;
}

void wma_update(wma_model_t *model, double value) {
    if (!model || !model->window) return;
    model->window[model->index] = value;
    model->index = (model->index + 1) % model->window_size;
    model->count++;
}

double wma_forecast(const wma_model_t *model) {
    if (!model || !model->window || !model->weights || model->count == 0) {
        return 0.0;
    }

    int n = (model->count < model->window_size) ? model->count : model->window_size;
    double sum_w = 0.0;
    double sum_wx = 0.0;

    /* Walk back from most recent to oldest */
    /* Most recent value is at index-1, second most recent at index-2, etc. */
    for (int i = 0; i < n; i++) {
        int idx = (model->index - 1 - i + model->window_size) % model->window_size;
        /* Assign weight: most recent gets weight[n-1], oldest gets weight[0] */
        double w = model->weights[n - 1 - i];
        sum_wx += w * model->window[idx];
        sum_w += w;
    }

    return (sum_w > 0.0) ? (sum_wx / sum_w) : 0.0;
}

void wma_destroy(wma_model_t *model) {
    if (!model) return;
    free(model->window);
    free(model->weights);
    model->window = NULL;
    model->weights = NULL;
}

/* ============================================================
 * L5 — Single Exponential Smoothing (SES)
 * ============================================================ */
void ses_init(ses_model_t *model, double alpha) {
    if (!model) return;
    memset(model, 0, sizeof(ses_model_t));
    /* Clamp alpha to (0, 1) */
    if (alpha <= 0.0) alpha = 0.1;
    if (alpha >= 1.0) alpha = 0.9;
    model->alpha = alpha;
    model->initialized = 0;
}

void ses_update(ses_model_t *model, double value) {
    if (!model) return;
    if (!model->initialized) {
        /* First observation: set level to value */
        model->level = value;
        model->last_forecast = value;
        model->initialized = 1;
    } else {
        model->level = model->alpha * value +
                       (1.0 - model->alpha) * model->level;
        model->last_forecast = model->level;
    }
}

double ses_forecast(const ses_model_t *model) {
    if (!model || !model->initialized) return 0.0;
    return model->level;
}

double ses_forecast_k(const ses_model_t *model, int k) {
    /* SES forecasts are flat — constant level */
    (void)k;
    return ses_forecast(model);
}

/* ============================================================
 * L5 — Holt's Double Exponential Smoothing
 * ============================================================ */
void holt_init(holt_model_t *model, double alpha, double beta) {
    if (!model) return;
    memset(model, 0, sizeof(holt_model_t));
    if (alpha <= 0.0) alpha = 0.1;
    if (alpha >= 1.0) alpha = 0.9;
    if (beta <= 0.0) beta = 0.05;
    if (beta >= 1.0) beta = 0.9;
    model->alpha = alpha;
    model->beta = beta;
    model->initialized = 0;
}

void holt_update(holt_model_t *model, double value) {
    if (!model) return;
    if (!model->initialized) {
        model->level = value;
        model->trend = 0.0;
        model->last_forecast = value;
        model->initialized = 1;
    } else {
        double prev_level = model->level;
        model->level = model->alpha * value +
                       (1.0 - model->alpha) * (model->level + model->trend);
        model->trend = model->beta * (model->level - prev_level) +
                       (1.0 - model->beta) * model->trend;
        model->last_forecast = model->level + model->trend;
    }
}

double holt_forecast_k(const holt_model_t *model, int k) {
    if (!model || !model->initialized || k < 0) return 0.0;
    return model->level + (double)k * model->trend;
}

void holt_get_components(const holt_model_t *model, double *level, double *trend) {
    if (level) *level = model ? model->level : 0.0;
    if (trend) *trend = model ? model->trend : 0.0;
}

/* ============================================================
 * L5 — Holt-Winters Triple Exponential Smoothing
 * ============================================================ */
int holt_winters_init(holt_winters_model_t *model,
                      double alpha, double beta, double gamma,
                      int season_length, int is_multiplicative) {
    if (!model || season_length < 2) return -1;
    memset(model, 0, sizeof(holt_winters_model_t));

    if (alpha <= 0.0) alpha = 0.1;
    if (alpha >= 1.0) alpha = 0.9;
    if (beta <= 0.0) beta = 0.05;
    if (beta >= 1.0) beta = 0.9;
    if (gamma <= 0.0) gamma = 0.05;
    if (gamma >= 1.0) gamma = 0.9;

    model->alpha = alpha;
    model->beta = beta;
    model->gamma = gamma;
    model->season_length = season_length;
    model->is_multiplicative = is_multiplicative;

    model->seasonal = (double *)malloc(season_length * sizeof(double));
    if (!model->seasonal) return -1;

    for (int i = 0; i < season_length; i++) {
        model->seasonal[i] = is_multiplicative ? 1.0 : 0.0;
    }

    model->initialized = 0;
    return 0;
}

int holt_winters_fit_seasonal(holt_winters_model_t *model,
                              const double *data, int data_len) {
    if (!model || !model->seasonal || !data || data_len < model->season_length) {
        return -1;
    }

    int seasons = data_len / model->season_length;
    if (seasons < 1) return -1;

    /* Compute average of each period position across seasons */
    for (int s = 0; s < model->season_length; s++) {
        double sum = 0.0;
        int count = 0;
        for (int k = 0; k < seasons; k++) {
            int idx = k * model->season_length + s;
            if (idx < data_len) {
                sum += data[idx];
                count++;
            }
        }
        double avg = (count > 0) ? sum / count : 0.0;

        if (model->is_multiplicative) {
            model->seasonal[s] = (avg > 1e-9) ? avg : 1.0;
        } else {
            model->seasonal[s] = avg;
        }
    }

    /* For multiplicative: normalize seasonal indices to have mean = 1.0 */
    if (model->is_multiplicative) {
        double mean_s = 0.0;
        for (int s = 0; s < model->season_length; s++) {
            mean_s += model->seasonal[s];
        }
        mean_s /= model->season_length;
        if (mean_s > 1e-9) {
            for (int s = 0; s < model->season_length; s++) {
                model->seasonal[s] /= mean_s;
            }
        }
    }

    /* Initialize level and trend from first season */
    double sum_level = 0.0;
    for (int t = 0; t < model->season_length && t < data_len; t++) {
        double deseason = model->is_multiplicative ?
            data[t] / model->seasonal[t] : data[t] - model->seasonal[t];
        sum_level += deseason;
    }
    model->level = sum_level / model->season_length;
    model->trend = 0.0;
    model->count = seasons * model->season_length;
    model->initialized = 1;

    return 0;
}

void holt_winters_update(holt_winters_model_t *model, double value) {
    if (!model || !model->seasonal) return;

    if (!model->initialized) {
        model->level = value;
        model->trend = 0.0;
        model->last_forecast = value;
        model->initialized = 1;
        model->count = 1;
        return;
    }

    int s_curr = model->count % model->season_length;
    int s_prev = (s_curr - model->season_length + model->season_length) %
                  model->season_length;

    double prev_level = model->level;

    if (model->is_multiplicative) {
        /* Multiplicative Holt-Winters */
        double deseason_val = value / model->seasonal[s_prev];
        model->level = model->alpha * deseason_val +
                       (1.0 - model->alpha) * (model->level + model->trend);
        model->trend = model->beta * (model->level - prev_level) +
                       (1.0 - model->beta) * model->trend;
        model->seasonal[s_curr] = model->gamma * (value / model->level) +
                                  (1.0 - model->gamma) * model->seasonal[s_prev];
        model->last_forecast = (model->level + model->trend) * model->seasonal[s_curr];
    } else {
        /* Additive Holt-Winters */
        double deseason_val = value - model->seasonal[s_prev];
        model->level = model->alpha * deseason_val +
                       (1.0 - model->alpha) * (model->level + model->trend);
        model->trend = model->beta * (model->level - prev_level) +
                       (1.0 - model->beta) * model->trend;
        model->seasonal[s_curr] = model->gamma * (value - model->level) +
                                  (1.0 - model->gamma) * model->seasonal[s_prev];
        model->last_forecast = model->level + model->trend + model->seasonal[s_curr];
    }

    model->count++;
}

double holt_winters_forecast_k(const holt_winters_model_t *model, int k) {
    if (!model || !model->initialized || k < 0) return 0.0;
    if (!model->seasonal) return model->level + k * model->trend;

    int s = (model->count + k) % model->season_length;

    if (model->is_multiplicative) {
        return (model->level + (double)k * model->trend) * model->seasonal[s];
    } else {
        return model->level + (double)k * model->trend + model->seasonal[s];
    }
}

void holt_winters_destroy(holt_winters_model_t *model) {
    if (!model) return;
    free(model->seasonal);
    model->seasonal = NULL;
}

/* ============================================================
 * L2/L4 — Tracking Signal
 * ============================================================
 *
 * TS = cumulative forecast error / MAD
 *
 * |TS| > 4.0 indicates forecast bias requiring recalibration.
 * |TS| > 5.0 is a critical alert.
 */
double tracking_signal(double cumulative_error, double mad) {
    if (fabs(mad) < 1e-9) {
        return (fabs(cumulative_error) > 1e-9) ? 999.0 : 0.0;
    }
    return cumulative_error / mad;
}

/* ============================================================
 * L4 — Linear Regression
 * ============================================================
 *
 * Simple OLS: Y = a + b·X
 *
 *   b = Σ(xi - x̄)(yi - ȳ) / Σ(xi - x̄)²
 *   a = ȳ - b·x̄
 *
 * Where x̄ = Σxi/n, ȳ = Σyi/n
 */
void linear_regression(const double *x, const double *y, int n,
                       double *slope, double *intercept) {
    if (!x || !y || n < 2) {
        if (slope) *slope = 0.0;
        if (intercept) *intercept = 0.0;
        return;
    }

    double sum_x = 0.0, sum_y = 0.0;
    double sum_xy = 0.0, sum_xx = 0.0;

    for (int i = 0; i < n; i++) {
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i] * y[i];
        sum_xx += x[i] * x[i];
    }

    double mean_x = sum_x / n;
    double mean_y = sum_y / n;

    double numerator = sum_xy - n * mean_x * mean_y;
    double denominator = sum_xx - n * mean_x * mean_x;

    double b = (fabs(denominator) > 1e-12) ? (numerator / denominator) : 0.0;
    double a = mean_y - b * mean_x;

    if (slope) *slope = b;
    if (intercept) *intercept = a;
}
