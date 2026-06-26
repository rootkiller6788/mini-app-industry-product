#include "energy_core.h"
#include "energy_forecast.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* -- Holt-Winters State Management (L5) ----------------------------- */
void energy_holt_winters_init(energy_holt_winters_t* hw, int period, bool mult) {
    if (!hw) return;
    memset(hw, 0, sizeof(energy_holt_winters_t));
    hw->period = period;
    hw->num_seasons = period;
    hw->alpha = 0.3f; hw->beta = 0.1f; hw->gamma = 0.1f;
    hw->is_multiplicative = mult;
    hw->is_initialized = false;
}
void energy_holt_winters_fit(energy_holt_winters_t* hw, const float* series, int n) {
    if (!hw || !series || n < hw->period) return;
    /* Initialize level from first period */
    float level = 0.0f;
    for (int i = 0; i < hw->period; i++) level += series[i];
    level /= hw->period;
    if (level <= 0.0f) level = 1.0f;
    /* Initialize trend */
    float trend = 0.0f;
    for (int i = hw->period; i < 2*hw->period && i < n; i++) trend += series[i]-series[i-hw->period];
    trend /= (float)(hw->period*hw->period);
    /* Initialize seasonal */
    for (int i = 0; i < hw->period && i < n; i++)
        hw->seasonal[i] = hw->is_multiplicative ? series[i]/level : series[i]-level;
    /* Smoothing iterations */
    for (int t = hw->period; t < n; t++) {
        float old_level = level;
        int si = t % hw->period;
        if (hw->is_multiplicative) {
            float s = hw->seasonal[si]; if (s < 0.001f) s = 0.001f;
            level = hw->alpha*(series[t]/s) + (1.0f-hw->alpha)*(level+trend);
            trend = hw->beta*(level-old_level) + (1.0f-hw->beta)*trend;
            if (level > 0.001f) hw->seasonal[si] = hw->gamma*(series[t]/level) + (1.0f-hw->gamma)*hw->seasonal[si];
        } else {
            level = hw->alpha*(series[t]-hw->seasonal[si]) + (1.0f-hw->alpha)*(level+trend);
            trend = hw->beta*(level-old_level) + (1.0f-hw->beta)*trend;
            hw->seasonal[si] = hw->gamma*(series[t]-level) + (1.0f-hw->gamma)*hw->seasonal[si];
        }
    }
    hw->level = level; hw->trend = trend; hw->is_initialized = true;
}
float energy_holt_winters_forecast(const energy_holt_winters_t* hw, int steps) {
    if (!hw || !hw->is_initialized || steps < 1) return 0.0f;
    int si = steps % hw->period;
    if (hw->is_multiplicative)
        return (hw->level + steps*hw->trend) * (hw->seasonal[si] > 0.001f ? hw->seasonal[si] : 1.0f);
    return hw->level + steps*hw->trend + hw->seasonal[si];
}
void energy_holt_winters_update(energy_holt_winters_t* hw, float new_val) {
    if (!hw) return;
    float old_level = hw->level;
    /* Simplified single-step update */
    hw->level = hw->alpha*new_val + (1.0f-hw->alpha)*(hw->level+hw->trend);
    hw->trend = hw->beta*(hw->level-old_level) + (1.0f-hw->beta)*hw->trend;
}
void energy_holt_winters_forecast_n(const energy_holt_winters_t* hw, float* fc, int n) {
    if (!hw || !fc || n <= 0) return;
    for (int i = 0; i < n; i++) fc[i] = energy_holt_winters_forecast(hw, i+1);
}

/* -- ARIMA (L5: Box-Jenkins) ---------------------------------------- */
void energy_arima_init(energy_arima_t* arima, int p, int d, int q) {
    if (!arima) return;
    memset(arima, 0, sizeof(energy_arima_t));
    arima->p = p; arima->d = d; arima->q = q;
}
void energy_arima_seasonal(energy_arima_t* arima, int P, int D, int Q, int s) {
    if (!arima) return;
    arima->seasonal_p = P; arima->seasonal_d = D; arima->seasonal_q = Q;
    arima->seasonal_period = s;
}
/* OLS estimation of AR coefficients using Yule-Walker equations */
void energy_arima_fit_ols(energy_arima_t* arima, const float* series, int n) {
    if (!arima || !series || n <= arima->p) return;
    /* Differencing */
    float* diff = (float*)malloc(n * sizeof(float));
    if (!diff) return;
    memcpy(diff, series, n * sizeof(float));
    for (int d = 0; d < arima->d; d++) {
        for (int i = n-1; i > d; i--) diff[i] = diff[i] - diff[i-1];
    }
    int nd = n - arima->d;
    /* Estimate AR coefficients using covariance method */
    for (int i = 0; i < arima->p && i < 32; i++) {
        float num=0, den=0;
        for (int t = arima->p; t < nd; t++) {
            num += diff[t] * diff[t-i-1];
            den += diff[t-i-1] * diff[t-i-1];
        }
        arima->ar_coeffs[i] = den > 1e-6f ? num/den : 0;
    }
    /* Residual variance */
    float ssr = 0;
    for (int t = arima->p; t < nd; t++) {
        float pred = arima->constant;
        for (int i = 0; i < arima->p; i++) pred += arima->ar_coeffs[i] * diff[t-i-1];
        float e = diff[t] - pred; ssr += e*e;
    }
    arima->sigma2 = (nd - arima->p) > 0 ? ssr/(nd - arima->p) : 1.0f;
    arima->is_fitted = true;
    free(diff);
}
float energy_arima_forecast(const energy_arima_t* arima, const float* series, int n, int steps) {
    if (!arima || !arima->is_fitted || !series || n <= arima->p) return 0.0f;
    float pred = arima->constant;
    for (int i = 0; i < arima->p && i < n; i++) pred += arima->ar_coeffs[i] * series[n-1-i];
    return pred;
}
void energy_arima_forecast_n(const energy_arima_t* arima, const float* series, int n, float* fc, int na) {
    if (!arima || !series || !fc) return;
    for (int i = 0; i < na; i++) fc[i] = energy_arima_forecast(arima, series, n, i+1);
}

/* -- STL Decomposition (L5: Cleveland 1990, simplified) ------------- */
energy_stl_decomp_t energy_stl_decompose(const float* series, int n, int period) {
    energy_stl_decomp_t stl; memset(&stl, 0, sizeof(stl));
    if (!series || n < period*2) return stl;
    stl.n = n; stl.period = period;
    /* Extract trend: moving average of length period */
    int half = period/2;
    for (int i = 0; i < n; i++) {
        float sum = 0; int cnt = 0;
        for (int j = i-half; j <= i+half; j++)
            if (j >= 0 && j < n) { sum += series[j]; cnt++; }
        stl.trend[i] = cnt > 0 ? sum/cnt : series[i];
    }
    /* Detrend */
    float* detrended = (float*)malloc(n * sizeof(float));
    if (!detrended) return stl;
    for (int i = 0; i < n; i++) detrended[i] = series[i] - stl.trend[i];
    /* Seasonal: average by period position */
    for (int p = 0; p < period; p++) {
        float sum = 0; int cnt = 0;
        for (int i = p; i < n; i += period) { sum += detrended[i]; cnt++; }
        float avg = cnt > 0 ? sum/cnt : 0;
        for (int i = p; i < n; i += period) stl.seasonal[i] = avg;
    }
    /* Residual = data - trend - seasonal */
    for (int i = 0; i < n; i++) stl.residual[i] = series[i] - stl.trend[i] - stl.seasonal[i];
    free(detrended);
    return stl;
}
float energy_stl_seasonal_adjust(const energy_stl_decomp_t* stl, int i) {
    if (!stl || i < 0 || i >= stl->n) return 0.0f;
    return stl->trend[i] + stl->residual[i];
}

/* -- Similar Day Forecast ------------------------------------------- */
float energy_similar_day_forecast(const energy_timeslot_t* history, int n, int target_month, int target_day_type, int target_hour) {
    if (!history || n < 720) return 0.0f; /* need at least 30 days */
    float sum = 0; int cnt = 0;
    for (int i = 0; i < n; i++) {
        if (history[i].month != target_month) continue;
        /* Calculate day of week */
        int sd = (history[i].day-1)+(history[i].month-1)*30+(history[i].year-2020)*365;
        int dow = (sd+4)%7;
        int dt = (dow < 5) ? 0 : 1; /* 0=weekday, 1=weekend */
        if (dt != target_day_type || history[i].hour != target_hour) continue;
        sum += history[i].load_kw; cnt++;
    }
    return cnt > 0 ? sum/cnt : 0.0f;
}
/* -- Persistence ---------------------------------------------------- */
float energy_persistence_forecast(const float* series, int n, int steps) {
    if (!series || n < 1) return 0.0f;
    int idx = n - steps; if (idx < 0) idx = 0;
    return series[idx];
}
/* -- Clear Sky Index Forecast --------------------------------------- */
float energy_clear_sky_index_forecast(const float* ghi, const float* clearsky, int n, int steps) {
    if (!ghi || !clearsky || n < 1) return 0.0f;
    /* Persistence of clear sky index */
    float csi_last = clearsky[n-1] > 0 ? ghi[n-1]/clearsky[n-1] : 0;
    if (csi_last > 2.0f) csi_last = 1.0f;
    return csi_last * clearsky[n-1];
    (void)steps;
}

/* -- Forecast Metrics (L5: Model Evaluation) ------------------------ */
energy_forecast_metrics_t energy_forecast_evaluate(const float* actual, const float* predicted, int n) {
    energy_forecast_metrics_t m; memset(&m, 0, sizeof(m));
    if (!actual || !predicted || n <= 0) return m;
    float sum_ae=0, sum_se=0, sum_ape=0, sum_sape=0, sum_e=0, sum_a=0, sum_p=0, sum_ap=0, sum_aa=0, sum_pp=0;
    int valid = 0;
    for (int i=0; i<n; i++) {
        if (actual[i] <= 0.001f) continue;
        float e = actual[i] - predicted[i];
        sum_ae += fabsf(e); sum_se += e*e;
        sum_ape += fabsf(e)/actual[i];
        sum_sape += fabsf(e)/((fabsf(actual[i])+fabsf(predicted[i]))*0.5f);
        sum_e += e; sum_a += actual[i]; sum_p += predicted[i];
        sum_ap += actual[i]*predicted[i]; sum_aa += actual[i]*actual[i]; sum_pp += predicted[i]*predicted[i];
        valid++;
    }
    if (valid == 0) return m;
    m.mae = sum_ae/valid;
    m.rmse = sqrtf(sum_se/valid);
    m.mape = sum_ape/valid * 100.0f;
    m.smape = sum_sape/valid * 100.0f;
    m.bias = sum_e/valid;
    float mean_a = sum_a/valid, mean_p = sum_p/valid;
    float num = 0, den = 0;
    for (int i=0; i<n; i++) { num += (actual[i]-predicted[i])*(actual[i]-predicted[i]); den += (actual[i]-mean_a)*(actual[i]-mean_a); }
    m.r_squared = den > 1e-10f ? 1.0f - num/den : 0;
    /* Nash-Sutcliffe Efficiency */
    float nse_num=0, nse_den=0;
    for (int i=0; i<n; i++) { nse_num += (actual[i]-predicted[i])*(actual[i]-predicted[i]); nse_den += (actual[i]-mean_a)*(actual[i]-mean_a); }
    m.nash_sutcliffe = nse_den > 1e-10f ? 1.0f - nse_num/nse_den : 0;
    return m;
}
float energy_forecast_skill_score(const energy_forecast_metrics_t* model, const energy_forecast_metrics_t* ref) {
    if (!model || !ref || ref->rmse <= 0) return 0;
    return 1.0f - model->rmse/ref->rmse;
}

/* -- Probabilistic Forecasting (L8: Ensemble) ----------------------- */
energy_probabilistic_forecast_t energy_forecast_ensemble(const float* members, int n) {
    energy_probabilistic_forecast_t pf; memset(&pf, 0, sizeof(pf));
    if (!members || n < 2) return pf;
    /* Sort members for quantile extraction */
    float* sorted = (float*)malloc(n * sizeof(float));
    if (!sorted) return pf;
    memcpy(sorted, members, n * sizeof(float));
    for (int i=1; i<n; i++) { float k=sorted[i]; int j=i-1; while(j>=0&&sorted[j]>k){sorted[j+1]=sorted[j];j--;} sorted[j+1]=k; }
    pf.quantile_10 = sorted[n*10/100];
    pf.quantile_25 = sorted[n*25/100];
    pf.quantile_50 = sorted[n/2];
    pf.quantile_75 = sorted[n*75/100];
    pf.quantile_90 = sorted[n*90/100];
    float sum=0, sum2=0;
    for (int i=0; i<n; i++) { sum+=members[i]; sum2+=members[i]*members[i]; }
    pf.mean = sum/n;
    pf.std_dev = sqrtf(sum2/n - pf.mean*pf.mean);
    free(sorted);
    return pf;
}
float energy_forecast_pinball_loss(const float* actual, int n, const float* qf, float q) {
    if (!actual || !qf || n <= 0) return 0;
    float loss = 0;
    for (int i=0; i<n; i++) { float e = actual[i]-qf[i]; loss += e > 0 ? q*e : (q-1)*e; }
    return loss/n;
}
float energy_forecast_crps(const float* ensemble, int n, float obs) {
    if (!ensemble || n < 2) return 0;
    float* sorted = (float*)malloc(n * sizeof(float));
    if (!sorted) return 0;
    memcpy(sorted, ensemble, n*sizeof(float));
    for (int i=1; i<n; i++) { float k=sorted[i]; int j=i-1; while(j>=0&&sorted[j]>k){sorted[j+1]=sorted[j];j--;} sorted[j+1]=k; }
    float crps=0;
    for (int i=0; i<n; i++) {
        float p_i = (i+0.5f)/n;
        float obs_ind = obs <= sorted[i] ? 1.0f : 0.0f;
        crps += (p_i-obs_ind)*(p_i-obs_ind);
    }
    free(sorted);
    return crps/n;
}
/* -- Weather-Normalized Load ----------------------------------------- */
float energy_weather_normalize_load(const energy_timeslot_t* ts, int n, int target_hour) {
    if (!ts || n <= 0) return 0.0f;
    /* Find base temperature (balance point) */
    float t_base = 15.5f;
    float t_balance = energy_heating_cooling_balance_point(ts, n, &t_base);
    /* Compute HDD/CDD and normalize */
    float hdd = energy_heating_degree_days(t_balance, ts, n);
    float cdd = energy_cooling_degree_days(t_balance, ts, n);
    (void)target_hour;
    return hdd + cdd;
}
float energy_heating_cooling_balance_point(const energy_timeslot_t* ts, int n, float* t_balance) {
    if (!ts || n < 24 || !t_balance) { if(t_balance)*t_balance=15.5f; return 15.5f; }
    /* Simple: find temperature where load is minimum (balance point) */
    float min_load = 1e9f; float best_t = 15.5f;
    for (int i=0; i<n; i++) {
        if (ts[i].load_kw < min_load) { min_load = ts[i].load_kw; best_t = ts[i].temperature_c; }
    }
    *t_balance = best_t;
    return best_t;
}
