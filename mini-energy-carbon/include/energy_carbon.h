#ifndef ENERGY_CARBON_H
#define ENERGY_CARBON_H

#include "energy_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── L4: IPCC AR6 Global Warming Potentials (GWP100) ─────────────────── */

/* GHG species identifiers */
typedef enum {
    ENERGY_GHG_CO2 = 0,
    ENERGY_GHG_CH4,        /* methane */
    ENERGY_GHG_N2O,        /* nitrous oxide */
    ENERGY_GHG_SF6,        /* sulfur hexafluoride */
    ENERGY_GHG_NF3,        /* nitrogen trifluoride */
    ENERGY_GHG_PFC_CF4,    /* tetrafluoromethane */
    ENERGY_GHG_PFC_C2F6,   /* hexafluoroethane */
    ENERGY_GHG_HFC_23,     /* trifluoromethane */
    ENERGY_GHG_HFC_32,     /* difluoromethane */
    ENERGY_GHG_HFC_125,    /* pentafluoroethane */
    ENERGY_GHG_HFC_134a,   /* tetrafluoroethane */
    ENERGY_GHG_HFC_143a,   /* trifluoroethane */
    ENERGY_GHG_HFC_152a,   /* difluoroethane */
    ENERGY_GHG_HFC_227ea,
    ENERGY_GHG_HFC_236fa,
} energy_ghg_species_t;

/* Emission factor data for common fuels (L4: IPCC Guidelines) */
typedef struct {
    char   fuel_name[ENERGY_MAX_NAME];
    float  co2_kg_per_kwh;         /* direct combustion CO₂ */
    float  ch4_g_per_kwh;          /* methane slip */
    float  n2o_g_per_kwh;          /* N₂O from combustion */
    float  upstream_co2_kg_per_kwh;/* extraction/transport */
    float  calorific_value_kj_kg;  /* LHV */
    float  carbon_content_pct;     /* mass fraction carbon */
} energy_fuel_factor_t;

/* Carbon credit / offset project (L7: Applications) */
typedef struct {
    energy_offset_standard_t standard;
    char   project_name[ENERGY_MAX_NAME];
    char   project_id[64];
    char   country[64];
    float  tonnes_co2e_per_year;
    float  vintage_year;
    float  permanence_years;       /* expected CO₂ storage duration */
    float  additionality_score;    /* 0-1, was this truly additional? */
    float  leakage_risk_pct;       /* risk of emissions shifting elsewhere */
    float  price_per_tonne;
    float  sdg_co_benefits_count;  /* UN SDG co-benefits */
    bool   is_verified;
    bool   is_retired;
} energy_carbon_offset_t;

/* Life-Cycle Assessment stage (L4: ISO 14040/14044) */
typedef enum {
    ENERGY_LCA_RAW_MATERIAL = 0,
    ENERGY_LCA_MANUFACTURING,
    ENERGY_LCA_TRANSPORT,
    ENERGY_LCA_USE_PHASE,
    ENERGY_LCA_END_OF_LIFE,
    ENERGY_LCA_RECYCLING,
} energy_lca_stage_t;

typedef struct {
    energy_lca_stage_t stage;
    float  co2e_kg;
    float  energy_mj;
    float  water_liters;
    float  land_use_m2;
    char   description[ENERGY_MAX_NAME];
} energy_lca_entry_t;

typedef struct {
    energy_lca_entry_t entries[64];
    int   num_entries;
    float total_co2e_kg;
    float total_energy_mj;
    char  product_name[ENERGY_MAX_NAME];
    char  functional_unit[64];
} energy_lca_t;

/* EU-ETS Allowance model (L7: Applications) */
typedef struct {
    float  current_price_per_tonne;
    float  price_floor;
    float  price_ceiling;
    float  free_allocation_pct;
    float  annual_reduction_factor;  /* linear reduction factor */
    float  market_stability_reserve_pct;
    int    phase_year;               /* ETS phase */
} energy_ets_market_t;

/* Carbon pricing instrument */
typedef enum {
    ENERGY_CARBON_PRICE_ETS = 0,
    ENERGY_CARBON_PRICE_CARBON_TAX,
    ENERGY_CARBON_PRICE_CBAM,        /* Carbon Border Adjustment Mechanism */
    ENERGY_CARBON_PRICE_VOLUNTARY,
} energy_carbon_price_type_t;

typedef struct {
    energy_carbon_price_type_t type;
    float  price_per_tonne_co2e;
    float  coverage_pct;             /* % economy covered */
    int    year;
    char   jurisdiction[128];
} energy_carbon_price_t;

/* ── API: Carbon Accounting (L4: GHG Protocol) ───────────────────────── */

/* Complete Scope 1 calculator: fuel combustion + process + fugitive */
float energy_carbon_scope1_fuel_kg(float fuel_kwh, const energy_fuel_factor_t* ff);
float energy_carbon_scope1_process_kg(float activity_data, float ef);
float energy_carbon_scope1_fugitive_kg(float charge_kg, float leakage_rate,
                                        float gwp);

/* Scope 2: location-based and market-based */
float energy_carbon_scope2_location_kg(float grid_kwh, float grid_ef);
float energy_carbon_scope2_market_kg(float grid_kwh, float residual_mix_ef);

/* Scope 3 categories (simplified) */
float energy_carbon_scope3_cat1_kg(float purchased_tonnes, float ef_per_tonne);
float energy_carbon_scope3_cat3_kg(float fuel_kwh, float ef_per_kwh);
float energy_carbon_scope3_cat6_kg(float travel_km, float ef_per_km);
float energy_carbon_scope3_cat11_kg(float product_kwh, float ef_per_kwh,
                                     float lifetime_years);

/* GWP computation */
float energy_gwp100_co2e(float mass_kg, energy_ghg_species_t species);
float energy_ipcc_gwp100_value(energy_ghg_species_t species);

/* Carbon offset quality assessment (L7) */
float energy_offset_additionality_score(const energy_carbon_offset_t* off);
float energy_offset_permanence_score(const energy_carbon_offset_t* off);
float energy_offset_quality_index(const energy_carbon_offset_t* off);
float energy_offset_tonnes_to_neutralize(float emissions_kg,
                                          const energy_carbon_offset_t* off);

/* Carbon market (L7) */
float energy_ets_compliance_cost_kg(float emissions_tonnes,
                                     const energy_ets_market_t* market);
float energy_carbon_tax_cost(float emissions_tonnes, float tax_per_tonne);
float energy_cbam_cost(float imported_tonnes, float eu_ets_price,
                        float origin_carbon_price);

/* LCA operations */
void  energy_lca_init(energy_lca_t* lca, const char* product, const char* unit);
void  energy_lca_add_entry(energy_lca_t* lca, energy_lca_stage_t stage,
                            float co2e, float energy_mj);
float energy_lca_total_co2e(const energy_lca_t* lca);
float energy_lca_energy_intensity(const energy_lca_t* lca);

/* Emission factor database queries */
const energy_fuel_factor_t* energy_fuel_factor_lookup(const char* fuel_name);
float energy_grid_emission_factor(const char* country_code, int year);
float energy_electricity_co2_per_kwh(energy_source_t source);

/* SBTi alignment check (L9) */
bool  energy_sbti_1_5c_aligned(float emissions_intensity_tco2e_per_m,
                                const char* sector);
float energy_sbti_reduction_required_pct(const char* sector, int target_year);

#ifdef __cplusplus
}
#endif

#endif
