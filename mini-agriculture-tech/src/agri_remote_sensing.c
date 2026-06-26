#include "agri_remote_sensing.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Precision Agriculture Remote Sensing
 *
 * L4: Radiative Transfer in Vegetation Canopies
 *
 * Beer-Lambert Law for homogeneous canopies (Monsi & Saeki, 2005):
 *   I(z) = I0 * exp(-k * LAI * z/H)
 * where:
 *   I0 = incident PAR at canopy top
 *   k  = light extinction coefficient (0.4-0.8 typical)
 *   LAI = cumulative leaf area index from top
 *
 * NDVI normalizes difference vegetation index (Rouse et al., 1974):
 *   NDVI = (NIR - RED) / (NIR + RED)
 *
 * Theorem: NDVI is bounded in [-1, 1]; healthy vegetation > 0.5;
 * water < 0; bare soil approximately 0.1.
 *
 * Reference:
 * - Huete et al. (2002) "Overview of the radiometric and biophysical
 *   performance of the MODIS vegetation indices"
 * - Stanford EE 263: Linear regression for spectral unmixing
 * - CMU 15-869: Physics-based rendering vs empirical vegetation indices
 * - ETH: Crop water stress modeling
 */

double rs_ndvi_to_lai(double ndvi, double k_extinction)
{
    if (k_extinction <= 0.0 || ndvi <= 0.0) return 0.0;
    if (ndvi >= 0.95) ndvi = 0.95; /* saturation clamp */
    double fCover = (ndvi - 0.15) / 0.80;
    if (fCover < 0.0) fCover = 0.0;
    if (fCover > 1.0) fCover = 1.0;
    return -log(1.0 - fCover) / k_extinction;
}

double rs_fapar_from_ndvi(double ndvi)
{
    double fapar = 1.24 * ndvi - 0.168;
    if (fapar < 0.0) fapar = 0.0;
    if (fapar > 1.0) fapar = 1.0;
    return fapar;
}

double rs_canopy_light_interception(double lai, double k)
{
    if (lai < 0.0 || k < 0.0) return 0.0;
    return 1.0 - exp(-k * lai);
}

static double get_band_reflectance(const RSImage *img, RSBand b)
{
    if (!img || (int)b >= img->num_bands) return 0.0;
    for (int i = 0; i < img->num_bands && i < RS_MAX_BANDS; i++) {
        if (i == (int)b) return img->bands[i].reflectance[0];
    }
    return 0.0;
}

double rs_compute_ndvi(const RSImage *img, uint32_t row, uint32_t col)
{
    (void)row; (void)col;
    if (!img) return 0.0;
    double nir = 0.0, red = 0.0;
    for (int i = 0; i < img->num_bands; i++) {
        double r = img->bands[i].reflectance[0];
        if (img->bands[i].wavelength_nm >= 770.0 &&
            img->bands[i].wavelength_nm <= 900.0) nir = r;
        if (img->bands[i].wavelength_nm >= 630.0 &&
            img->bands[i].wavelength_nm <= 690.0) red = r;
    }
    return RS_NDVI(nir, red);
}

double rs_compute_evi(const RSImage *img, uint32_t row, uint32_t col)
{
    (void)row; (void)col;
    if (!img) return 0.0;
    double nir = 0.0, red = 0.0, blue = 0.0;
    for (int i = 0; i < img->num_bands; i++) {
        double r = img->bands[i].reflectance[0];
        double wl = img->bands[i].wavelength_nm;
        if (wl >= 770.0 && wl <= 900.0) nir = r;
        if (wl >= 630.0 && wl <= 690.0) red = r;
        if (wl >= 450.0 && wl <= 520.0) blue = r;
    }
    return RS_EVI(nir, red, blue);
}

double rs_compute_savi(const RSImage *img, uint32_t row, uint32_t col,
                        double soil_factor)
{
    (void)row; (void)col;
    if (!img) return 0.0;
    double nir = 0.0, red = 0.0;
    for (int i = 0; i < img->num_bands; i++) {
        double r = img->bands[i].reflectance[0];
        double wl = img->bands[i].wavelength_nm;
        if (wl >= 770.0 && wl <= 900.0) nir = r;
        if (wl >= 630.0 && wl <= 690.0) red = r;
    }
    return RS_SAVI(nir, red, soil_factor);
}

void rs_zone_stats(const RSImage *img, const uint32_t *mask,
                    uint32_t rows, uint32_t cols, RSZoneStats *stats)
{
    if (!img || !stats) return;
    memset(stats, 0, sizeof(*stats));
    if (rows == 0 || cols == 0) return;

    uint32_t count = 0, bare_count = 0;
    stats->ndvi_min = 1.0; stats->ndvi_max = -1.0;

    for (uint32_t r = 0; r < rows; r++) {
        for (uint32_t c = 0; c < cols; c++) {
            if (mask && !mask[r * cols + c]) continue;
            double ndvi = rs_compute_ndvi(img, r, c);
            double evi  = rs_compute_evi(img, r, c);
            stats->ndvi_mean += ndvi;
            stats->evi_mean  += evi;
            if (ndvi < stats->ndvi_min) stats->ndvi_min = ndvi;
            if (ndvi > stats->ndvi_max) stats->ndvi_max = ndvi;
            if (ndvi < 0.2) bare_count++;
            count++;
        }
    }

    if (count > 0) {
        stats->ndvi_mean /= (double)count;
        stats->evi_mean  /= (double)count;
        stats->vegetation_fraction = (double)(count - bare_count) / (double)count;
        stats->bare_soil_fraction = (double)bare_count / (double)count;

        /* compute std via second pass */
        double var_sum = 0.0;
        for (uint32_t r = 0; r < rows; r++) {
            for (uint32_t c = 0; c < cols; c++) {
                if (mask && !mask[r * cols + c]) continue;
                double ndvi = rs_compute_ndvi(img, r, c);
                double d = ndvi - stats->ndvi_mean;
                var_sum += d * d;
            }
        }
        stats->ndvi_std = sqrt(var_sum / (double)count);
        stats->lai_mean = rs_ndvi_to_lai(stats->ndvi_mean, 0.5);
    }
}

const char* rs_crop_class_name(RSCropClass c)
{
    switch (c) {
        case RS_CLASS_HEALTHY:       return "Healthy";
        case RS_CLASS_WATER_STRESS:  return "Water Stress";
        case RS_CLASS_NUTRIENT_DEF:  return "Nutrient Deficiency";
        case RS_CLASS_PEST_DAMAGE:   return "Pest Damage";
        case RS_CLASS_WEED:          return "Weed";
        case RS_CLASS_BARE_SOIL:     return "Bare Soil";
        case RS_CLASS_SENESCENT:     return "Senescent";
        default:                     return "Unknown";
    }
}

RSCropClass rs_classify_pixel(double ndvi, double ndwi, double lai,
                               double canopy_temp_delta)
{
    if (ndvi < 0.15) return RS_CLASS_BARE_SOIL;
    if (ndvi < 0.35 && canopy_temp_delta > 3.0)
        return RS_CLASS_WATER_STRESS;
    if (ndvi > 0.35 && ndvi < 0.55 && ndwi < -0.2)
        return RS_CLASS_NUTRIENT_DEF;
    if (ndvi > 0.25 && ndvi < 0.6 && lai < 1.5 && canopy_temp_delta > 2.0)
        return RS_CLASS_PEST_DAMAGE;
    if (ndvi > 0.3 && ndvi < 0.6 && lai < 2.0 && ndwi > 0.1)
        return RS_CLASS_WEED;
    if (ndvi > 0.45 && ndvi < 0.65 && lai > 2.0 && canopy_temp_delta < 0.5)
        return RS_CLASS_SENESCENT;
    if (ndvi > 0.6) return RS_CLASS_HEALTHY;
    return RS_CLASS_BARE_SOIL;
}

double rs_cwsi(double canopy_temp_c, double air_temp_c,
                double wet_baseline, double dry_baseline)
{
    double dT = canopy_temp_c - air_temp_c;
    double span = dry_baseline - wet_baseline;
    if (fabs(span) < 1e-6) return 0.0;
    double cwsi = (dT - wet_baseline) / span;
    if (cwsi < 0.0) cwsi = 0.0;
    if (cwsi > 1.0) cwsi = 1.0;
    return cwsi;
}

void rs_sentinel2_to_bands(double b2, double b3, double b4,
                            double b5, double b8, double b11,
                            double b12, RSBandData bands[RS_BAND_COUNT])
{
    if (!bands) return;
    memset(bands, 0, sizeof(RSBandData) * RS_BAND_COUNT);

    bands[RS_BAND_BLUE].reflectance[0]    = b2;
    bands[RS_BAND_BLUE].wavelength_nm     = 490.0;
    bands[RS_BAND_GREEN].reflectance[0]   = b3;
    bands[RS_BAND_GREEN].wavelength_nm    = 560.0;
    bands[RS_BAND_RED].reflectance[0]     = b4;
    bands[RS_BAND_RED].wavelength_nm      = 665.0;
    bands[RS_BAND_REDEDGE].reflectance[0] = b5;
    bands[RS_BAND_REDEDGE].wavelength_nm  = 705.0;
    bands[RS_BAND_NIR].reflectance[0]     = b8;
    bands[RS_BAND_NIR].wavelength_nm      = 842.0;
    bands[RS_BAND_SWIR1].reflectance[0]   = b11;
    bands[RS_BAND_SWIR1].wavelength_nm    = 1610.0;
    bands[RS_BAND_SWIR2].reflectance[0]   = b12;
    bands[RS_BAND_SWIR2].wavelength_nm    = 2190.0;
}

/* L7: Crop coefficient (Kc) estimation from NDVI
 * FAO-56 dual crop coefficient approach (Allen et al., 1998):
 * Kc = Kcb + Ke, where Kcb = basal crop coefficient,
 * Ke = soil evaporation coefficient.
 * Empirical Kcb from NDVI: Kcb ≈ 1.2 * NDVI + 0.1  (Choudhury et al., 1994) */
double rs_ndvi_to_kc(double ndvi)
{
    if (ndvi < 0.0) return 0.15; /* bare soil evaporation */
    double kcb = 1.2 * ndvi + 0.1;
    if (kcb < 0.15) kcb = 0.15;
    if (kcb > 1.25) kcb = 1.25;
    return kcb;
}

/* L7: Actual evapotranspiration using crop coefficient
 * ETc = Kc * ETo  (FAO Irrigation and Drainage Paper 56)
 * where ETo = reference evapotranspiration (mm/day),
 * ETc = crop evapotranspiration under standard conditions */
double rs_estimate_etc(double ndvi, double et0_mm_day)
{
    double kc = rs_ndvi_to_kc(ndvi);
    return kc * et0_mm_day;
}

/* L8: Soil moisture estimation from NDVI and thermal data
 * Simplified triangle method (Carlson et al., 1994):
 * Soil moisture proxy via NDVI/Ts (temperature) space */
double rs_soil_moisture_proxy(double ndvi, double surface_temp_c,
                               double t_min, double t_max)
{
    if (fabs(t_max - t_min) < 1e-6) return 0.5;
    /* Temperature-Vegetation Dryness Index (TVDI) */
    double t_norm = (surface_temp_c - t_min) / (t_max - t_min);
    if (t_norm < 0.0) t_norm = 0.0;
    if (t_norm > 1.0) t_norm = 1.0;
    /* Higher TVDI = drier soil */
    return 1.0 - t_norm * (1.0 - 0.2 * ndvi);
}
