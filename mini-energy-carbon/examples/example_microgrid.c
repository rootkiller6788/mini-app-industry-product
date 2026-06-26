#include "../include/energy_core.h"
#include "../include/energy_solar.h"
#include "../include/energy_wind.h"
#include "../include/energy_market.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== Microgrid Sizing Example ===\n\n");

    /* 1. Define location and solar geometry */
    energy_location_t loc = {.latitude_deg=35.0f,.longitude_deg=139.0f,.elevation_m=50,.timezone_offset_h=9};
    energy_solar_geometry_t geo = energy_solar_geometry(&loc,2024,6,21,12.0f);
    printf("Location: Tokyo (35N, 139E)\n");
    printf("Summer Solstice noon: elevation=%.1f deg, azimuth=%.1f deg\n\n", geo.elevation_deg, geo.azimuth_deg);

    /* 2. Create an energy system with solar + battery */
    energy_system_t sys; energy_system_init(&sys);

    energy_generator_t solar = {
        .source_type=ENERGY_SOURCE_SOLAR,.capacity_kw=500,.efficiency=0.21f,
        .lifetime_years=25,.capex_per_kw=700,.opex_per_kwh=0.005f,
        .fixed_opex_per_kw_yr=8,.carbon_intensity_gco2_kwh=48,
        .capacity_factor=0.18f,.degradation_rate_per_year=0.005f,
        .is_renewable=true,.is_dispatchable=false
    };
    strncpy(solar.name,"Rooftop Solar 500kW",ENERGY_MAX_NAME-1);
    energy_system_add_generator(&sys,&solar);

    /* 3. Add battery */
    energy_battery_init_advanced(&sys.battery,ENERGY_BATTERY_LFP,200,0.5f);

    /* 4. Run a 24-hour simulation */
    sys.num_slots = 24;
    for (int i=0;i<24;i++) {
        sys.timeseries[i].hour=i;
        sys.timeseries[i].temperature_c=20.0f+5.0f*(i-12)*(i-12)/36.0f;
        sys.timeseries[i].ghi_w_m2 = (i>=6&&i<=18)?600.0f*sinf(3.14159f*(i-6)/12.0f):0;
        sys.timeseries[i].load_kw=100+50*((i>=8&&i<=20)?1.0f:0.5f);
        energy_system_simulate_slot(&sys,i);
        printf("Hour %2d: Gen=%.0f kW, Load=%.0f kW, Solar=%.0f kW\n",
               i, sys.timeseries[i].generation_kw,
               sys.timeseries[i].load_kw, sys.timeseries[i].solar_kw);
    }
    energy_system_compute_kpis(&sys);

    /* 5. Compute LCOE */
    float lcoe = energy_lcoe(&solar,0.06f);
    printf("\n=== Results ===\n");
    printf("Total generation: %.0f kWh\n",sys.annual_energy_kwh);
    printf("Renewable fraction: %.1f%%\n",sys.renewable_fraction);
    printf("Solar LCOE: $%.4f/kWh\n",lcoe);
    printf("Self-sufficiency: %.1f%%\n",sys.self_sufficiency_pct);

    /* 6. Carbon offset */
    float offset = energy_carbon_offset_kg(&sys,400.0f);
    printf("Carbon offset: %.1f kg CO2\n",offset);

    return 0;
}
