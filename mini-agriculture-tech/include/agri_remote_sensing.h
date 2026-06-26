#ifndef AGRI_REMOTE_SENSING_H
#define AGRI_REMOTE_SENSING_H

#include <stdbool.h>
#include <stdint.h>

#define RS_MAX_BANDS 16
#define RS_MAX_PIXELS (1024 * 1024)
#define RS_MAX_CLASSES 32

/* L1: Multispectral band definitions */
typedef enum {
    RS_BAND_BLUE,     /* 450-520 nm */
    RS_BAND_GREEN,    /* 520-600 nm */
    RS_BAND_RED,      /* 630-690 nm */
    RS_BAND_REDEDGE,  /* 690-740 nm */
    RS_BAND_NIR,      /* 770-900 nm */
    RS_BAND_SWIR1,    /* 1550-1750 nm */
    RS_BAND_SWIR2,    /* 2080-2350 nm */
    RS_BAND_THERMAL,  /* 10400-12500 nm */
    RS_BAND_COUNT
} RSBand;

/* L4: Radiative Transfer — Beer-Lambert Law for canopy light extinction:
 * I(z) = I0 * exp(-k * LAI(z)), where k = extinction coefficient, LAI = leaf area index */
typedef struct {
    double reflectance[RS_BAND_COUNT];
    double wavelength_nm;
    double bandwidth_nm;
} RSBandData;

typedef struct {
    RSBandData bands[RS_MAX_BANDS];
    int num_bands;
    double pixel_resolution_m;
    uint32_t rows, cols;
    double center_lat, center_lon;
    uint64_t timestamp;
} RSImage;

/* L2: Vegetation Indices — Core remote sensing metrics for precision agriculture */
#define RS_NDVI(nir, red) (((nir) + (red) > 0) ? ((nir) - (red)) / ((nir) + (red)) : 0.0)
#define RS_EVI(nir, red, blue) (((nir) + 6.0*(red) - 7.5*(blue) + 1.0) > 0 ? \
    2.5 * ((nir) - (red)) / ((nir) + 6.0*(red) - 7.5*(blue) + 1.0) : 0.0)
#define RS_SAVI(nir, red, L) (((nir) + (red) + (L)) > 0 ? \
    ((nir) - (red)) * (1.0 + (L)) / ((nir) + (red) + (L)) : 0.0)
#define RS_NDWI(green, nir) (((green) + (nir) > 0) ? ((green) - (nir)) / ((green) + (nir)) : 0.0)
#define RS_GNDVI(nir, green) (((nir) + (green) > 0) ? ((nir) - (green)) / ((nir) + (green)) : 0.0)

/* L4: Leaf Area Index estimation from NDVI (empirical Beer-Lambert inversion) */
double rs_ndvi_to_lai(double ndvi, double k_extinction);
double rs_fapar_from_ndvi(double ndvi);
double rs_canopy_light_interception(double lai, double k);

/* L5: Vegetation index computation for image patches */
double rs_compute_ndvi(const RSImage *img, uint32_t row, uint32_t col);
double rs_compute_evi(const RSImage *img, uint32_t row, uint32_t col);
double rs_compute_savi(const RSImage *img, uint32_t row, uint32_t col, double soil_factor);

/* L5: Zonal statistics for agricultural field zones */
typedef struct {
    double ndvi_mean, ndvi_std, ndvi_min, ndvi_max;
    double evi_mean, evi_std;
    double lai_mean;
    double vegetation_fraction;
    double bare_soil_fraction;
} RSZoneStats;

void rs_zone_stats(const RSImage *img, const uint32_t *mask,
                    uint32_t rows, uint32_t cols, RSZoneStats *stats);

/* L7: Crop health classification from multispectral data */
typedef enum {
    RS_CLASS_HEALTHY,
    RS_CLASS_WATER_STRESS,
    RS_CLASS_NUTRIENT_DEF,
    RS_CLASS_PEST_DAMAGE,
    RS_CLASS_WEED,
    RS_CLASS_BARE_SOIL,
    RS_CLASS_SENESCENT
} RSCropClass;

const char* rs_crop_class_name(RSCropClass c);
RSCropClass rs_classify_pixel(double ndvi, double ndwi, double lai,
                               double canopy_temp_delta);

/* L8: Thermal infrared — crop water stress index (CWSI)
 * CWSI = (Tc - Ta) - (Tc - Ta)_wet / (Tc - Ta)_dry - (Tc - Ta)_wet
 * Jackson et al. (1981), Idso et al. (1981) */
double rs_cwsi(double canopy_temp_c, double air_temp_c,
                double wet_baseline, double dry_baseline);

/* L9: Satellite constellation — Sentinel-2 / Landsat band mapping */
void rs_sentinel2_to_bands(double b2, double b3, double b4,
                            double b5, double b8, double b11,
                            double b12, RSBandData bands[RS_BAND_COUNT]);

/* L7: FAO-56 crop coefficient and evapotranspiration from vegetation indices */
double rs_ndvi_to_kc(double ndvi);
double rs_estimate_etc(double ndvi, double et0_mm_day);

/* L8: Soil moisture proxy via NDVI-Temperature space (triangle method, Carlson 1994) */
double rs_soil_moisture_proxy(double ndvi, double surface_temp_c,
                               double t_min, double t_max);

#endif
