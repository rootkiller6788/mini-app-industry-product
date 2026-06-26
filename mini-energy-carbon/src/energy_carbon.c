#include "energy_core.h"
#include "energy_carbon.h"
#include <string.h>
#include <math.h>

/* -- Fuel emission factor database (L4: IPCC Guidelines) ------------ */
static const energy_fuel_factor_t fuel_db[] = {
    {"Natural Gas", 0.202f, 0.003f, 0.0001f, 0.025f, 48000.0f, 75.0f},
    {"Coal (bituminous)", 0.354f, 0.001f, 0.0015f, 0.040f, 24000.0f, 65.0f},
    {"Diesel", 0.267f, 0.002f, 0.001f, 0.050f, 42600.0f, 86.0f},
    {"Gasoline", 0.249f, 0.003f, 0.002f, 0.060f, 44000.0f, 85.0f},
    {"LPG", 0.227f, 0.001f, 0.0005f, 0.030f, 46000.0f, 82.0f},
    {"Biomass (wood)", 0.020f, 0.010f, 0.002f, 0.005f, 15000.0f, 50.0f},
    {"Peat", 0.382f, 0.030f, 0.002f, 0.010f, 10000.0f, 55.0f},
    {"Hydrogen (grey)", 0.000f, 0.000f, 0.000f, 10.0f, 120000.0f, 0.0f},
};
static const int fuel_db_size = sizeof(fuel_db)/sizeof(fuel_db[0]);

const energy_fuel_factor_t* energy_fuel_factor_lookup(const char* fuel_name) {
    if (!fuel_name) return NULL;
    for (int i = 0; i < fuel_db_size; i++)
        if (strstr(fuel_db[i].fuel_name, fuel_name)) return &fuel_db[i];
    return NULL;
}

/* -- Scope 1: Direct emissions -------------------------------------- */
float energy_carbon_scope1_fuel_kg(float fuel_kwh, const energy_fuel_factor_t* ff) {
    if (!ff || fuel_kwh <= 0.0f) return 0.0f;
    return fuel_kwh * ff->co2_kg_per_kwh
         + fuel_kwh * ff->ch4_g_per_kwh * 0.001f * 28.0f
         + fuel_kwh * ff->n2o_g_per_kwh * 0.001f * 273.0f;
}

float energy_carbon_scope1_process_kg(float activity_data, float ef) {
    return activity_data * ef;
}

float energy_carbon_scope1_fugitive_kg(float charge_kg, float leak_rate, float gwp) {
    return charge_kg * leak_rate * gwp;
}

/* -- Scope 2: Purchased electricity --------------------------------- */
float energy_carbon_scope2_location_kg(float grid_kwh, float grid_ef) {
    return grid_kwh * grid_ef;
}

float energy_carbon_scope2_market_kg(float grid_kwh, float residual_mix_ef) {
    return grid_kwh * residual_mix_ef;
}

/* -- Scope 3: Value chain (simplified categories) ------------------- */
float energy_carbon_scope3_cat1_kg(float purchased_tonnes, float ef_per_tonne) {
    return purchased_tonnes * ef_per_tonne;
}
float energy_carbon_scope3_cat3_kg(float fuel_kwh, float ef_per_kwh) {
    return fuel_kwh * ef_per_kwh;
}
float energy_carbon_scope3_cat6_kg(float travel_km, float ef_per_km) {
    return travel_km * ef_per_km;
}
float energy_carbon_scope3_cat11_kg(float product_kwh, float ef_per_kwh, float lifetime_yrs) {
    return product_kwh * ef_per_kwh * lifetime_yrs;
}

/* -- GWP computation (L4: IPCC AR6) --------------------------------- */
float energy_gwp100_co2e(float mass_kg, energy_ghg_species_t species) {
    float gwp = energy_ipcc_gwp100_value(species);
    return mass_kg * gwp;
}

float energy_ipcc_gwp100_value(energy_ghg_species_t species) {
    /* Maps to energy_ipcc_gwp100 gas_id */
    static const int map[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14};
    int n = (int)(sizeof(map)/sizeof(map[0]));
    if (species < 0 || species >= n) return 0.0f;
    return energy_ipcc_gwp100(map[species]);
}

/* -- Carbon offset quality assessment (L7: Applications) ------------ */
float energy_offset_additionality_score(const energy_carbon_offset_t* off) {
    if (!off) return 0.0f;
    float score = off->additionality_score;
    if (!off->is_verified) score *= 0.5f;
    return score;
}
float energy_offset_permanence_score(const energy_carbon_offset_t* off) {
    if (!off) return 0.0f;
    /* 100 years permanence = 1.0, <10 years = 0.1 */
    if (off->permanence_years >= 100.0f) return 1.0f;
    if (off->permanence_years < 10.0f) return 0.1f;
    return off->permanence_years / 100.0f;
}
float energy_offset_quality_index(const energy_carbon_offset_t* off) {
    if (!off) return 0.0f;
    float add = energy_offset_additionality_score(off);
    float perm = energy_offset_permanence_score(off);
    float leak = 1.0f - off->leakage_risk_pct * 0.01f;
    return (add * 0.4f + perm * 0.3f + leak * 0.3f);
}
float energy_offset_tonnes_to_neutralize(float emissions_kg, const energy_carbon_offset_t* off) {
    if (!off || emissions_kg <= 0.0f) return 0.0f;
    float qi = energy_offset_quality_index(off);
    if (qi <= 0.01f) return 1e9f;
    return (emissions_kg * 0.001f) / qi;
}

/* -- Carbon market calculations (L7: EU-ETS, CBAM) ------------------ */
float energy_ets_compliance_cost_kg(float emissions_tonnes, const energy_ets_market_t* market) {
    if (!market) return 0.0f;
    float paid = emissions_tonnes * (1.0f - market->free_allocation_pct * 0.01f);
    return paid * market->current_price_per_tonne;
}
float energy_carbon_tax_cost(float emissions_tonnes, float tax_per_tonne) {
    return emissions_tonnes * tax_per_tonne;
}
float energy_cbam_cost(float imported_tonnes, float eu_ets_price, float origin_carbon_price) {
    float diff = eu_ets_price - origin_carbon_price;
    if (diff < 0.0f) diff = 0.0f;
    return imported_tonnes * diff;
}
/* -- LCA operations ------------------------------------------------- */
void energy_lca_init(energy_lca_t* lca, const char* product, const char* unit) {
    if (!lca) return;
    memset(lca, 0, sizeof(energy_lca_t));
    if (product) { strncpy(lca->product_name, product, ENERGY_MAX_NAME-1); lca->product_name[ENERGY_MAX_NAME-1]='\0'; }
    if (unit) { strncpy(lca->functional_unit, unit, 63); lca->functional_unit[63]='\0'; }
}
void energy_lca_add_entry(energy_lca_t* lca, energy_lca_stage_t stage, float co2e, float energy_mj) {
    if (!lca || lca->num_entries >= 64) return;
    lca->entries[lca->num_entries].stage = stage;
    lca->entries[lca->num_entries].co2e_kg = co2e;
    lca->entries[lca->num_entries].energy_mj = energy_mj;
    lca->total_co2e_kg += co2e;
    lca->total_energy_mj += energy_mj;
    lca->num_entries++;
}
float energy_lca_total_co2e(const energy_lca_t* lca) { return lca ? lca->total_co2e_kg : 0.0f; }
float energy_lca_energy_intensity(const energy_lca_t* lca) {
    if (!lca || lca->total_co2e_kg <= 0.0f) return 0.0f;
    return lca->total_energy_mj / lca->total_co2e_kg;
}

/* -- Grid emission factor by source (L4: IPCC) ---------------------- */
float energy_electricity_co2_per_kwh(energy_source_t source) {
    switch (source) {
        case ENERGY_SOURCE_SOLAR:     return 0.048f;
        case ENERGY_SOURCE_WIND:      return 0.011f;
        case ENERGY_SOURCE_HYDRO:     return 0.024f;
        case ENERGY_SOURCE_NUCLEAR:   return 0.012f;
        case ENERGY_SOURCE_COAL:      return 0.820f;
        case ENERGY_SOURCE_NATURAL_GAS: return 0.490f;
        case ENERGY_SOURCE_OIL:       return 0.650f;
        case ENERGY_SOURCE_BIOMASS:   return 0.230f;
        case ENERGY_SOURCE_GEOTHERMAL: return 0.038f;
        default: return 0.400f;
    }
}
float energy_grid_emission_factor(const char* country_code, int year) {
    if (!country_code) return 0.400f;
    /* Simplified lookup; real data from IEA/EMBER */
    if (strstr(country_code,"CN")) return 0.570f;
    if (strstr(country_code,"US")) return 0.386f;
    if (strstr(country_code,"IN")) return 0.650f;
    if (strstr(country_code,"EU")||strstr(country_code,"DE")) return 0.280f;
    if (strstr(country_code,"FR")) return 0.056f;
    if (strstr(country_code,"GB")||strstr(country_code,"UK")) return 0.210f;
    if (strstr(country_code,"JP")) return 0.450f;
    if (strstr(country_code,"BR")) return 0.120f;
    if (strstr(country_code,"AU")) return 0.500f;
    (void)year;
    return 0.475f; /* global average */
}
/* -- SBTi alignment check (L9: Industry Frontiers) ------------------ */
bool energy_sbti_1_5c_aligned(float ei_tco2e_per_m, const char* sector) {
    if (!sector) return false;
    /* 1.5C pathway requires ~4.2% annual reduction from 2020 baseline */
    float threshold;
    if (strstr(sector,"power")||strstr(sector,"electricity")) threshold = 0.10f;
    else if (strstr(sector,"steel")||strstr(sector,"cement")) threshold = 0.50f;
    else if (strstr(sector,"transport")) threshold = 0.15f;
    else if (strstr(sector,"building")) threshold = 0.08f;
    else threshold = 0.20f;
    return ei_tco2e_per_m <= threshold;
}
float energy_sbti_reduction_required_pct(const char* sector, int target_year) {
    if (!sector) return 42.0f;
    float annual_rate;
    if (strstr(sector,"power")) annual_rate = 8.0f;
    else if (strstr(sector,"steel")) annual_rate = 3.0f;
    else if (strstr(sector,"transport")) annual_rate = 5.0f;
    else annual_rate = 4.2f;
    int years = target_year - 2020;
    if (years < 0) years = 0;
    float reduction = 1.0f - powf(1.0f - annual_rate * 0.01f, (float)years);
    return reduction * 100.0f;
}
