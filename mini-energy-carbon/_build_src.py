import os

def write_core_c():
    lines = []
    lines.append('''#include "energy_core.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

void energy_system_init(energy_system_t* sys) {
    if (!sys) return;
    memset(sys, 0, sizeof(energy_system_t));
}

void energy_system_add_generator(energy_system_t* sys,
                                  const energy_generator_t* gen) {
    if (!sys || !gen || sys->num_generators >= ENERGY_MAX_DEVICES) return;
    sys->generators[sys->num_generators] = *gen;
    sys->total_capacity_kw += gen->capacity_kw;
    if (gen->is_renewable) sys->total_renewable_kw += gen->capacity_kw;
    sys->num_generators++;
}

/*
 * P = G * A * eta * [1 - beta*(Tc - 25)]
 * Reference: Duffie & Beckman (2013), Ch.23
 */
float energy_solar_power_kw(float irradiance_w_m2, float panel_area_m2,
                             float efficiency, float temperature_c,
                             float temp_coefficient) {
    if (irradiance_w_m2 <= 0.0f || panel_area_m2 <= 0.0f) return 0.0f;
    if (efficiency > ENERGY_SHOCKLEY_QUEISSER) efficiency = ENERGY_SHOCKLEY_QUEISSER;
    if (efficiency < 0.0f) efficiency = 0.0f;
    float temp_loss = 1.0f - temp_coefficient * (temperature_c - 25.0f);
    if (temp_loss < 0.0f) temp_loss = 0.0f;
    if (temp_loss > 1.3f) temp_loss = 1.3f;
    return irradiance_w_m2 * panel_area_m2 * efficiency * temp_loss * 0.001f;
}

/*
 * P = 0.5 * rho * A * Cp * v^3, Cp <= 16/27 (Betz limit)
 * Reference: Manwell, McGowan, Rogers (2009), Ch.3
 */
float energy_wind_power_kw(float wind_speed_m_s, float rotor_diameter_m,
                            float efficiency, float air_density_kg_m3) {
    if (wind_speed_m_s < 3.0f || wind_speed_m_s > 25.0f) return 0.0f;
    if (rotor_diameter_m <= 0.0f) return 0.0f;
    float cp = efficiency < ENERGY_BETZ_LIMIT ? efficiency : ENERGY_BETZ_LIMIT;
    if (cp < 0.0f) cp = 0.0f;
    float area = M_PI * rotor_diameter_m * rotor_diameter_m * 0.25f;
    float power_w = 0.5f * air_density_kg_m3 * area * cp
                    * wind_speed_m_s * wind_speed_m_s * wind_speed_m_s;
    if (wind_speed_m_s > 12.0f) {
        float rated = 0.5f * air_density_kg_m3 * area * cp * 1728.0f;
        if (power_w > rated) power_w = rated;
    }
    return power_w * 0.001f;
}

/*
 * eta_carnot = 1 - T_cold/T_hot
 * Reference: Carnot (1824), 2nd Law of Thermodynamics
 */
float energy_carnot_efficiency(float T_hot_K, float T_cold_K) {
    if (T_hot_K <= 0.0f || T_cold_K <= 0.0f || T_cold_K >= T_hot_K) return 0.0f;
    return 1.0f - T_cold_K / T_hot_K;
}

float energy_rankine_efficiency(float T_boiler_K, float T_condenser_K,
                                 float turbine_efficiency, float pump_efficiency) {
    if (T_boiler_K <= T_condenser_K) return 0.0f;
    float carnot = energy_carnot_efficiency(T_boiler_K, T_condenser_K);
    if (turbine_efficiency <= 0.0f) turbine_efficiency = 0.85f;
    if (pump_efficiency <= 0.0f) pump_efficiency = 0.80f;
    float factor = turbine_efficiency * pump_efficiency
                 / (turbine_efficiency + pump_efficiency * (1.0f - carnot));
    if (factor > 1.0f) factor = 1.0f;
    return carnot * factor;
}

float energy_betz_power_limit(float rotor_diameter_m, float wind_speed_m_s,
                               float air_density_kg_m3) {
    if (rotor_diameter_m <= 0.0f || wind_speed_m_s <= 0.0f) return 0.0f;
    float area = M_PI * rotor_diameter_m * rotor_diameter_m * 0.25f;
    return ENERGY_BETZ_LIMIT * 0.5f * air_density_kg_m3 * area
           * wind_speed_m_s * wind_speed_m_s * wind_speed_m_s * 0.001f;
}

float energy_betz_cp_ideal(void) { return ENERGY_BETZ_LIMIT; }

float energy_sq_limit_power_per_area(float bandgap_ev) {
    if (bandgap_ev <= 0.0f) return 0.0f;
    float eta;
    if (bandgap_ev < 0.8f) eta = 0.20f + 0.30f * (bandgap_ev - 0.5f) / 0.3f;
    else if (bandgap_ev > 1.6f) eta = 0.337f - 0.15f * (bandgap_ev - 1.6f) / 1.0f;
    else eta = 0.28f + 0.057f * (bandgap_ev - 0.8f) / 0.8f;
    if (eta < 0.0f) eta = 0.0f;
    return eta * 1000.0f;
}
''')
    with open('src/energy_core.c', 'w') as f:
        f.write('\n'.join(lines))
    print(f'Wrote part 1: {len(lines)} lines')

if __name__ == '__main__':
    write_core_c()
