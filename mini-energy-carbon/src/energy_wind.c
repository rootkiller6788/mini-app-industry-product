#include "energy_core.h"
#include "energy_wind.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* -- Weibull PDF: f(v) = (k/c)*(v/c)^(k-1)*exp(-(v/c)^k) ----------- */
float energy_weibull_pdf(float v, float k, float c) {
    if (v <= 0 || c <= 0 || k <= 0) return 0.0f;
    float x = v / c;
    return (k / c) * powf(x, k - 1.0f) * expf(-powf(x, k));
}
float energy_weibull_cdf(float v, float k, float c) {
    if (v <= 0) return 0.0f;
    if (c <= 0 || k <= 0) return 0.0f;
    return 1.0f - expf(-powf(v / c, k));
}
float energy_weibull_inverse_cdf(float p, float k, float c) {
    if (p <= 0 || c <= 0 || k <= 0) return 0.0f;
    if (p >= 1.0f) p = 0.9999f;
    return c * powf(-logf(1.0f - p), 1.0f / k);
}
/* -- Weibull MLE fit (L5: Maximum Likelihood Estimation) ------------ */
energy_weibull_t energy_weibull_fit_mle(const float* speeds, int n) {
    energy_weibull_t wb; memset(&wb, 0, sizeof(wb));
    if (!speeds || n < 3) return wb;
    /* Initial guess using method of moments */
    float sum = 0.0f, sum2 = 0.0f, sumln = 0.0f, sum3 = 0.0f;
    int valid = 0;
    for (int i = 0; i < n; i++) {
        if (speeds[i] <= 0.0f) continue;
        sum += speeds[i]; sum2 += speeds[i]*speeds[i];
        sumln += logf(speeds[i]); sum3 += speeds[i]*speeds[i]*speeds[i];
        valid++;
    }
    if (valid < 3) return wb;
    float mean = sum / valid;
    float var = sum2/valid - mean*mean;
    wb.k = powf(mean / (sqrtf(var > 0 ? var : 1.0f) + 0.001f), 1.086f);
    wb.c = mean / (tgammaf(1.0f + 1.0f/wb.k) + 0.001f);
    /* Newton-Raphson iteration for MLE k */
    float sumvk = 0.0f, sumvkln = 0.0f;
    for (int iter = 0; iter < 20; iter++) {
        sumvk = 0.0f; sumvkln = 0.0f;
        for (int i = 0; i < n; i++) {
            if (speeds[i] <= 0) continue;
            float vk = powf(speeds[i]/wb.c, wb.k);
            sumvk += vk; sumvkln += vk * logf(speeds[i]/wb.c);
        }
        float f = sumvkln/sumvk - 1.0f/wb.k - sumln/valid;
        float df = -sumvkln/(sumvk*sumvk) * sumvk * logf(wb.c) + 1.0f/(wb.k*wb.k);
        if (fabsf(df) < 1e-10f) break;
        float dk = f / df;
        wb.k -= dk;
        if (wb.k < 0.5f) wb.k = 0.5f;
        if (wb.k > 10.0f) wb.k = 10.0f;
        if (fabsf(dk) < 1e-4f) break;
    }
    wb.c = powf(sumvk/valid, 1.0f/wb.k) * wb.c;
    wb.num_samples = valid;
    wb.mean_speed_m_s = mean;
    wb.mean_power_density_w_m2 = 0.5f * 1.225f * sum3 / valid;
    wb.most_probable_m_s = energy_weibull_most_probable(wb.k, wb.c);
    wb.max_energy_m_s = energy_weibull_max_energy_speed(wb.k, wb.c);
    return wb;
}

/* -- Method of Moments Weibull fit ---------------------------------- */
energy_weibull_t energy_weibull_fit_mom(const float* speeds, int n) {
    energy_weibull_t wb; memset(&wb, 0, sizeof(wb));
    if (!speeds || n < 3) return wb;
    float sum = 0.0f, sum2 = 0.0f, sum3 = 0.0f; int vn = 0;
    for (int i = 0; i < n; i++) {
        if (speeds[i] <= 0) continue;
        sum += speeds[i]; sum2 += speeds[i]*speeds[i]; sum3 += speeds[i]*speeds[i]*speeds[i]; vn++;
    }
    if (vn < 3) return wb;
    float mean = sum/vn, var = sum2/vn - mean*mean;
    wb.k = powf(mean/(sqrtf(var>0?var:1.0f)+0.001f), 1.086f);
    wb.c = mean/(tgammaf(1.0f+1.0f/wb.k)+0.001f);
    wb.num_samples = vn; wb.mean_speed_m_s = mean;
    wb.mean_power_density_w_m2 = 0.5f*1.225f*sum3/vn;
    wb.most_probable_m_s = energy_weibull_most_probable(wb.k, wb.c);
    wb.max_energy_m_s = energy_weibull_max_energy_speed(wb.k, wb.c);
    return wb;
}
/* -- Empirical (direct method) -------------------------------------- */
energy_weibull_t energy_weibull_fit_empirical(const float* speeds, int n) {
    return energy_weibull_fit_mle(speeds, n); /* equivalent for simplicity */
}
float energy_weibull_mean(float k, float c) {
    return c * tgammaf(1.0f + 1.0f/k);
}
float energy_weibull_most_probable(float k, float c) {
    if (k <= 1.0f) return 0.0f;
    return c * powf((k - 1.0f)/k, 1.0f/k);
}
float energy_weibull_max_energy_speed(float k, float c) {
    if (k <= 2.0f) return c;
    return c * powf((k + 2.0f)/k, 1.0f/k);
}

/* -- Wind shear: power law and log law ------------------------------ */
float energy_wind_shear_power_law(float v_ref, float h_ref, float h_target, float alpha) {
    if (h_ref <= 0 || h_target <= 0) return v_ref;
    return v_ref * powf(h_target/h_ref, alpha);
}
float energy_wind_shear_log_law(float v_ref, float h_ref, float h_target, float z0) {
    if (h_ref <= z0 || h_target <= z0 || z0 <= 0) return v_ref;
    return v_ref * logf(h_target/z0) / logf(h_ref/z0);
}
float energy_roughness_length(const char* terrain_type) {
    if (!terrain_type) return 0.03f;
    if (strstr(terrain_type,"sea")||strstr(terrain_type,"water")) return 0.0002f;
    if (strstr(terrain_type,"sand")||strstr(terrain_type,"snow")) return 0.0005f;
    if (strstr(terrain_type,"grass")||strstr(terrain_type,"farm")) return 0.03f;
    if (strstr(terrain_type,"suburb")||strstr(terrain_type,"low veg")) return 0.3f;
    if (strstr(terrain_type,"forest")||strstr(terrain_type,"city")) return 1.0f;
    if (strstr(terrain_type,"highrise")||strstr(terrain_type,"dense")) return 2.0f;
    return 0.03f;
}
float energy_power_law_exponent(float z0) {
    return 1.0f / logf(60.0f/(z0 > 0.0001f ? z0 : 0.0001f));
}
/* -- Air density correction (ideal gas law) ------------------------- */
float energy_air_density(float temperature_c, float pressure_hpa, float humidity_pct) {
    float T = temperature_c + 273.15f;
    float p = pressure_hpa * 100.0f; /* to Pa */
    float pv = humidity_pct * 0.01f * 610.78f * expf(17.27f*temperature_c/(temperature_c+237.3f));
    float rho_dry = p/(287.058f*T);
    float rho_wet = pv/(461.495f*T);
    return rho_dry * (1.0f - humidity_pct*0.01f*0.378f * pv/p) + rho_wet;
}

/* -- Turbine power from piecewise curve ----------------------------- */
float energy_turbine_power_kw(const energy_wind_turbine_t* wt, float wind_speed_m_s) {
    if (!wt) return 0.0f;
    if (wind_speed_m_s < wt->cut_in_speed_m_s || wind_speed_m_s > wt->cut_out_speed_m_s) return 0.0f;
    if (wind_speed_m_s >= wt->rated_speed_m_s) return wt->rated_power_kw;
    /* Interpolate piecewise curve */
    if (wt->num_curve_points < 2) {
        /* Cubic approximation: P = P_rated * ((v - v_cutin)/(v_rated - v_cutin))^3 */
        float frac = (wind_speed_m_s - wt->cut_in_speed_m_s)/(wt->rated_speed_m_s - wt->cut_in_speed_m_s);
        if (frac < 0) frac = 0; if (frac > 1) frac = 1;
        return wt->rated_power_kw * frac * frac * frac;
    }
    /* Binary search piecewise */
    int lo = 0, hi = wt->num_curve_points - 1;
    while (lo < hi - 1) {
        int mid = (lo+hi)/2;
        if (wt->speeds_m_s[mid] <= wind_speed_m_s) lo = mid; else hi = mid;
    }
    float v0=wt->speeds_m_s[lo], v1=wt->speeds_m_s[hi], p0=wt->powers_kw[lo], p1=wt->powers_kw[hi];
    if (v1 <= v0) return p0;
    return p0 + (p1-p0)*(wind_speed_m_s-v0)/(v1-v0);
}
/* -- Betz-limited power --------------------------------------------- */
float energy_turbine_power_betz(float rotor_diameter_m, float wind_speed_m_s, float air_density) {
    return energy_betz_power_limit(rotor_diameter_m, wind_speed_m_s, air_density);
}
/* -- AEP from Weibull (L5: Convolution integral) -------------------- */
float energy_turbine_aep_kwh(const energy_wind_turbine_t* wt, const energy_weibull_t* wb) {
    if (!wt || !wb || wb->k <= 0 || wb->c <= 0) return 0.0f;
    float aep = 0.0f;
    int steps = 100;
    float dv = (wt->cut_out_speed_m_s - wt->cut_in_speed_m_s) / steps;
    for (int i = 0; i < steps; i++) {
        float v = wt->cut_in_speed_m_s + (i + 0.5f) * dv;
        float prob = energy_weibull_pdf(v, wb->k, wb->c) * dv;
        float power = energy_turbine_power_kw(wt, v);
        aep += power * prob * 8760.0f;
    }
    return aep;
}
/* -- Capacity factor from Weibull ----------------------------------- */
float energy_weibull_capacity_factor(const energy_weibull_t* wb, const energy_wind_turbine_t* wt) {
    if (!wt || wt->rated_power_kw <= 0) return 0.0f;
    float aep = energy_turbine_aep_kwh(wt, wb);
    return aep/(wt->rated_power_kw*8760.0f);
}

/* -- Wake model (Jensen 1983) --------------------------------------- */
float energy_wake_deficit(float dist_rotor_d, float ct, float wake_decay) {
    if (dist_rotor_d <= 0) return 0.0f;
    float k = wake_decay;
    float deficit = (1.0f - sqrtf(1.0f - ct)) / powf(1.0f + 2.0f*k*dist_rotor_d, 2.0f);
    return deficit > 0 ? deficit : 0;
}
float energy_turbine_power_wake(const energy_wind_turbine_t* wt, float upstream_speed, float dist_rotor_d, float thrust_coeff, float wake_decay) {
    if (!wt) return 0.0f;
    float deficit = energy_wake_deficit(dist_rotor_d, thrust_coeff, wake_decay);
    float downstream_speed = upstream_speed * (1.0f - deficit);
    return energy_turbine_power_kw(wt, downstream_speed);
}
/* -- Wind farm AEP with wake model ---------------------------------- */
float energy_wind_farm_aep_gwh(const energy_wind_farm_t* farm, const energy_wind_turbine_t* wt_template, const energy_weibull_t* wb) {
    if (!farm || !wt_template || !wb) return 0.0f;
    float single_aep = energy_turbine_aep_kwh(wt_template, wb);
    /* Array efficiency: ~90% for 7D spacing offshore, ~85% for 5D onshore */
    float array_eff = farm->is_offshore ? 0.92f : 0.87f;
    if (farm->turbine_spacing_rotor_d < 5.0f) array_eff -= 0.05f;
    if (farm->turbine_spacing_rotor_d > 10.0f) array_eff += 0.03f;
    if (array_eff > 0.98f) array_eff = 0.98f;
    return single_aep * farm->turbine_count * array_eff * 1e-6f; /* GWh */
}
float energy_wind_farm_efficiency(const energy_wind_farm_t* farm) {
    if (!farm) return 0.0f;
    return farm->is_offshore ? 0.92f : 0.87f;
}
/* -- Gumbel distribution for extreme winds -------------------------- */
float energy_gumbel_cdf(float x, float mu, float beta) {
    return expf(-expf(-(x-mu)/beta));
}
float energy_gumbel_return_level(float ret_period, const float* ann_max, int n) {
    if (!ann_max || n < 5) return 0.0f;
    float sum = 0, var = 0;
    for (int i = 0; i < n; i++) sum += ann_max[i];
    float mean = sum/n;
    for (int i = 0; i < n; i++) var += (ann_max[i]-mean)*(ann_max[i]-mean);
    float sigma = sqrtf(var/(n-1 > 0 ? n-1 : 1));
    float beta = sigma * 0.7797f;
    float mu = mean - 0.5772f*beta;
    return mu - beta*logf(-logf(1.0f-1.0f/ret_period));
}
/* -- Utility --------------------------------------------------------- */
float energy_cut_in_ratio(const energy_wind_turbine_t* wt) {
    if (!wt || wt->rated_speed_m_s <= 0) return 0;
    return wt->cut_in_speed_m_s/wt->rated_speed_m_s;
}
float energy_wind_class_from_speed(float mean_50m) {
    if (mean_50m >= 9.4f) return 7;
    if (mean_50m >= 8.8f) return 6;
    if (mean_50m >= 8.0f) return 5;
    if (mean_50m >= 7.5f) return 4;
    if (mean_50m >= 7.0f) return 3;
    if (mean_50m >= 6.4f) return 2;
    if (mean_50m >= 5.6f) return 1;
    return 0;
}
const char* energy_wind_class_name(float mean_50m) {
    int c = (int)energy_wind_class_from_speed(mean_50m);
    static const char* names[] = {"Poor","Marginal","Fair","Good","Excellent","Outstanding","Superb","Exceptional"};
    int ns = (int)(sizeof(names)/sizeof(names[0]));
    return (c>=0&&c<ns)?names[c]:"Unknown";
}
int energy_wind_direction_sector(float dir_deg, int ns) {
    if (ns <= 0) return 0;
    float sector = 360.0f/ns;
    return ((int)((dir_deg + sector/2)/sector))%ns;
}
