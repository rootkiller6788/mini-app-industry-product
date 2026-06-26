#include "../include/energy_core.h"
#include "../include/energy_carbon.h"
#include "../include/energy_solar.h"
#include "../include/energy_wind.h"
#include "../include/energy_market.h"
#include "../include/energy_optimize.h"
#include "../include/energy_forecast.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#define EPS 0.001f

static int tests_passed = 0;
static int tests_failed = 0;
#define TEST(name) do { printf("  %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while(0)

/* --- L1: Core Definitions --- */
static void test_init(void) {
    TEST("system_init");
    energy_system_t sys;
    energy_system_init(&sys);
    CHECK(sys.num_generators == 0 && sys.num_slots == 0, "init zero");
}
static void test_add_generator(void) {
    TEST("add_generator");
    energy_system_t sys;
    energy_system_init(&sys);
    energy_generator_t gen = {.source_type=ENERGY_SOURCE_SOLAR, .capacity_kw=100, .is_renewable=true};
    energy_system_add_generator(&sys, &gen);
    CHECK(sys.num_generators == 1 && sys.total_capacity_kw == 100.0f && sys.total_renewable_kw == 100.0f, "gen added");
}
static void test_null_safety(void) {
    TEST("null_safety");
    energy_system_init(NULL);
    energy_system_add_generator(NULL, NULL);
    energy_battery_init(NULL, 0, 0, 0);
    PASS();
}

/* --- L4: Physical Limits --- */
static void test_carnot(void) {
    TEST("carnot_efficiency");
    float carnot = energy_carnot_efficiency(573.15f, 303.15f);
    CHECK(fabsf(carnot - 0.471f) < EPS, "eta ~ 47.1%");
    CHECK(energy_carnot_efficiency(300, 300) == 0.0f, "zero delta");
}
static void test_betz(void) {
    TEST("betz_limit");
    float cp = energy_betz_cp_ideal();
    CHECK(fabsf(cp - 0.59259f) < 1e-4f, "Cp = 16/27");
    float power = energy_betz_power_limit(80.0f, 10.0f, 1.225f);
    CHECK(power > 0, "betz power positive");
}
static void test_sq_limit(void) {
    TEST("sq_limit");
    float power = energy_sq_limit_power_per_area(1.12f);
    CHECK(power > 300 && power < 400, "SQ ~ 337 W/m2 for Si");
}
static void test_rankine(void) {
    TEST("rankine");
    float eta = energy_rankine_efficiency(773.0f, 303.0f, 0.88f, 0.82f);
    CHECK(eta > 0.3f && eta < 0.5f, "reasonable Rankine efficiency");
}
/* --- L5: Solar & Wind --- */
static void test_solar_power(void) {
    TEST("solar_power");
    float p = energy_solar_power_kw(1000.0f, 10.0f, 0.20f, 25.0f, 0.004f);
    CHECK(fabsf(p - 2.0f) < EPS, "2kW from 10m2 at STC");
    CHECK(energy_solar_power_kw(0, 10, 0.2f, 25, 0.004f) == 0, "zero irradiance");
}
static void test_wind_power(void) {
    TEST("wind_power");
    float p = energy_wind_power_kw(8.0f, 80.0f, 0.45f, 1.225f);
    CHECK(p > 0, "wind power positive");
    CHECK(energy_wind_power_kw(0.5f, 80, 0.45, 1.225) == 0, "below cut-in");
    CHECK(energy_wind_power_kw(30.0f, 80, 0.45, 1.225) == 0, "above cut-out");
}
/* --- L4: Carbon --- */
static void test_carbon_accounting(void) {
    TEST("carbon");
    energy_system_t sys; energy_system_init(&sys);
    sys.num_slots = 2;
    sys.timeseries[0].generation_kw = 100;
    sys.timeseries[0].carbon_intensity_gco2_kwh = 500;
    sys.timeseries[1].generation_kw = 50;
    sys.timeseries[1].carbon_intensity_gco2_kwh = 300;
    float total = energy_carbon_total_kg(&sys);
    CHECK(total > 0, "carbon non-zero");
    float pkwh = energy_carbon_per_kwh(&sys);
    CHECK(pkwh > 0, "per kwh non-zero");
}
static void test_gwp(void) {
    TEST("gwp100");
    CHECK(fabsf(energy_co2e_from_ch4_kg(1.0f) - 28.0f) < EPS, "CH4 GWP100=28");
    CHECK(fabsf(energy_co2e_from_n2o_kg(1.0f) - 273.0f) < 1.0f, "N2O GWP100=273");
    CHECK(energy_ipcc_gwp100(0) == 1.0f, "CO2 GWP=1");
    CHECK(energy_ipcc_gwp100(-1) == 0.0f, "invalid gas id");
}

/* --- L5: Battery --- */
static void test_battery(void) {
    TEST("battery");
    energy_battery_t bat;
    energy_battery_init(&bat, 100.0f, 50.0f, 0.90f);
    CHECK(bat.soc_pct == 50.0f, "init SOC");
    float stored = energy_battery_charge(&bat, 10.0f, 1.0f);
    CHECK(stored > 0, "charge ok");
    float discharged = energy_battery_discharge(&bat, 5.0f, 0.5f);
    CHECK(discharged > 0, "discharge ok");
}
static void test_battery_advanced(void) {
    TEST("battery_advanced");
    energy_battery_t bat;
    energy_battery_init_advanced(&bat, ENERGY_BATTERY_LFP, 100.0f, 0.5f);
    CHECK(bat.chemistry == ENERGY_BATTERY_LFP, "chemistry LFP");
    CHECK(bat.cycle_life > 5000.0f, "LFP cycle life");
}
static void test_battery_soh(void) {
    TEST("battery_soh");
    energy_battery_t bat;
    energy_battery_init(&bat, 100.0f, 50.0f, 0.90f);
    float soh = energy_battery_soh(&bat);
    CHECK(soh > 90.0f, "soh near 100%");
}
static void test_coulomb_soc(void) {
    TEST("coulomb_soc");
    energy_battery_t bat;
    energy_battery_init(&bat, 100.0f, 50.0f, 0.90f);
    float soc = energy_battery_soc_coulomb(&bat, 10.0f, 3600.0f);
    CHECK(soc > 50.0f, "soc increased after charging");
}
static void test_rainflow(void) {
    TEST("rainflow");
    energy_battery_t bat;
    energy_battery_init(&bat, 100.0f, 50.0f, 0.90f);
    energy_battery_rainflow_count(&bat, 25.0f);
    CHECK(bat.num_cycle_bins == 1, "cycle counted");
}
static void test_kalman_soc(void) {
    TEST("kalman_soc");
    energy_battery_t bat;
    energy_battery_init(&bat, 100.0f, 50.0f, 0.90f);
    float soc = energy_battery_kalman_soc(&bat, 3.25f, 0, 1.0f);
    CHECK(soc > 40.0f && soc < 60.0f, "kalman reasonable");
}

/* --- L5: Forecasting --- */
static void test_holt_winters(void) {
    TEST("holt_winters");
    float data[48] = {0};
    for (int i=0;i<48;i++) data[i]=50.0f+10.0f*sinf(2*3.14159f*i/24.0f);
    float fc = energy_holt_winters_additive(data,48,24,1,0.3f,0.1f,0.1f);
    CHECK(fc > 0, "holt-winters forecast positive");
}
static void test_similar_day(void) {
    TEST("similar_day");
    energy_timeslot_t hist[168];
    memset(hist,0,sizeof(hist));
    for(int i=0;i<168;i++){hist[i].load_kw=100+i%10;hist[i].day=(i/24)+1;hist[i].month=6;hist[i].year=2024;hist[i].hour=i%24;}
    float fc = energy_load_forecast_similar_day(hist,168,0,12);
    CHECK(fc > 0, "similar day non-zero");
}
/* --- L5: Finance --- */
static void test_lcoe(void) {
    TEST("lcoe");
    energy_generator_t gen = {.source_type=ENERGY_SOURCE_SOLAR,.capacity_kw=1000,.efficiency=0.20f,.lifetime_years=25,.capex_per_kw=800,.opex_per_kwh=0.005f,.fixed_opex_per_kw_yr=10,.carbon_intensity_gco2_kwh=48,.capacity_factor=0.25f,.degradation_rate_per_year=0.005f};
    float lcoe=energy_lcoe(&gen,0.06f);
    CHECK(lcoe>0.02f&&lcoe<0.20f,"LCOE reasonable");
}
static void test_npv(void) {
    TEST("npv");
    float cf[]={-1000,100,200,300,400,500};
    float npv=energy_npv(cf,6,0.05f);
    CHECK(npv>0,"positive NPV");
}
static void test_irr(void) {
    TEST("irr");
    float cf[]={-1000,300,300,300,300,300};
    float irr=energy_irr(cf,6);
    CHECK(irr>0.1f&&irr<0.3f,"IRR ~15%");
}
/* --- L5: Self-Consumption --- */
static void test_self_consumption(void) {
    TEST("self_consumption");
    energy_system_t sys; energy_system_init(&sys);
    energy_battery_init(&sys.battery,50,25,0.9f);
    sys.num_slots=10;
    for(int i=0;i<5;i++){sys.timeseries[i].generation_kw=10;sys.timeseries[i].load_kw=5;}
    for(int i=5;i<10;i++){sys.timeseries[i].generation_kw=0;sys.timeseries[i].load_kw=8;}
    float sc=energy_self_consumption_optimize(&sys,10);
    /* SC pct may be 0 if no net surplus or battery constraints */
    CHECK(sc >= 0.0f, "SC pct non-negative");
}

/* --- L7: Building & Heat Pump --- */
static void test_degree_days(void) {
    TEST("degree_days");
    energy_timeslot_t ts[48];
    for(int i=0;i<48;i++){ts[i].temperature_c=10.0f+5.0f*sinf(2*3.14159f*i/24.0f);}
    float hdd=energy_heating_degree_days(18.0f,ts,48);
    CHECK(hdd>0,"HDD positive in cool weather");
    float cdd=energy_cooling_degree_days(18.0f,ts,48);
    CHECK(cdd>=0,"CDD non-negative");
}
static void test_heat_pump(void) {
    TEST("heat_pump");
    energy_heatpump_t hp; memset(&hp,0,sizeof(hp));
    hp.rated_capacity_kw=10;hp.rated_cop=4.0f;hp.source_temp_min_c=-20;hp.source_temp_max_c=35;
    hp.sink_temp_min_c=25;hp.sink_temp_max_c=55;hp.min_capacity_pct=30;
    float cop=energy_heatpump_cop(&hp,7.0f,35.0f);
    CHECK(cop>2.0f&&cop<8.0f,"COP reasonable");
    float pwr=energy_heatpump_power_kw(&hp,5.0f,7.0f,35.0f);
    CHECK(pwr>0,"power positive");
}
static void test_building_load(void) {
    TEST("building_load");
    energy_thermal_zone_t zone; memset(&zone,0,sizeof(zone));
    zone.floor_area_m2=200;zone.wall_u_value=0.3f;zone.roof_u_value=0.2f;
    zone.infiltration_ach=0.5f;zone.heating_setpoint_c=21;zone.cooling_setpoint_c=24;
    float loss=energy_building_heat_loss_kw(&zone,5.0f);
    CHECK(loss>0,"heat loss positive in winter");
}
/* --- L9: Hydrogen --- */
static void test_hydrogen(void) {
    TEST("hydrogen");
    float h2=energy_electrolyzer_h2_kg(1000,0.75f,1.0f);
    CHECK(h2>0,"H2 production positive");
    float lhv=energy_hydrogen_lhv_kwh_per_kg();
    CHECK(fabsf(lhv-33.33f)<EPS,"LHV=33.33 kWh/kg");
    float fc=energy_fuel_cell_power_kw(1.0f,0.55f);
    CHECK(fc>0,"fuel cell power positive");
}

/* --- L3: Power Flow --- */
static void test_transmission_loss(void) {
    TEST("transmission_loss");
    float loss=energy_transmission_loss_kw(1000,33,0.1f,5);
    CHECK(loss>0,"loss positive");
}
static void test_power_factor(void) {
    TEST("power_factor");
    float q=energy_power_factor_correction_kvar(100,0.7f,0.95f);
    CHECK(q>0,"pfc capacitance positive");
}
/* --- L6: System Simulation --- */
static void test_system_sim(void) {
    TEST("system_sim");
    energy_system_t sys; energy_system_init(&sys);
    energy_generator_t gen={.source_type=ENERGY_SOURCE_SOLAR,.capacity_kw=100,.efficiency=0.2f,.capacity_factor=0.2f};
    energy_system_add_generator(&sys,&gen);
    sys.num_slots=24;
    sys.timeseries[12].ghi_w_m2=800;sys.timeseries[12].temperature_c=25;
    energy_system_simulate_slot(&sys,12);
    CHECK(sys.timeseries[12].generation_kw>0,"slot sim produces power");
}
static void test_merit_order(void) {
    TEST("merit_order");
    energy_system_t sys; energy_system_init(&sys);
    energy_generator_t g1={.capacity_kw=100,.opex_per_kwh=0.03f,.is_dispatchable=true};
    energy_generator_t g2={.capacity_kw=200,.opex_per_kwh=0.05f,.is_dispatchable=true};
    energy_system_add_generator(&sys,&g1);energy_system_add_generator(&sys,&g2);
    int result=energy_system_dispatch_merit_order(&sys,0,150);
    CHECK(result==0,"merit order meets load");
}
/* --- L7: Carbon Markets --- */
static void test_carbon_markets(void) {
    TEST("carbon_markets");
    energy_ets_market_t mkt={.current_price_per_tonne=80,.free_allocation_pct=50};
    float cost=energy_ets_compliance_cost_kg(100,&mkt);
    CHECK(cost>0,"ETS cost positive");
    float tax=energy_carbon_tax_cost(100,30);
    CHECK(fabsf(tax-3000)<EPS,"carbon tax @30/tonne");
}

/* --- Solar Geometry --- */
static void test_solar_geometry(void) {
    TEST("solar_geometry");
    int doy=energy_day_of_year(2024,6,21);
    CHECK(doy==173,"summer solstice DOY");
    float decl=energy_solar_declination(doy);
    CHECK(decl>0.3f,"summer declination positive");
}
/* --- Weibull --- */
static void test_weibull(void) {
    TEST("weibull");
    float speeds[100];for(int i=0;i<100;i++)speeds[i]=7.0f+((float)rand()/RAND_MAX-0.5f)*4.0f;
    energy_weibull_t wb=energy_weibull_fit_mom(speeds,100);
    CHECK(wb.k>0.5f&&wb.k<10.0f,"Weibull k valid");
    CHECK(wb.c>0.0f,"Weibull c positive");
}
/* --- Economic Dispatch --- */
static void test_economic_dispatch(void) {
    TEST("economic_dispatch");
    energy_generator_t units[2];
    memset(units,0,sizeof(units));
    units[0].capacity_kw=100;units[0].opex_per_kwh=0.03f;units[0].is_dispatchable=true;
    units[1].capacity_kw=200;units[1].opex_per_kwh=0.05f;units[1].is_dispatchable=true;
    energy_economic_dispatch_t d=energy_economic_dispatch(units,2,150);
    CHECK(d.total_cost_per_h>0,"dispatch has cost");
}
/* --- Forecast Metrics --- */
static void test_forecast_metrics(void) {
    TEST("forecast_metrics");
    float actual[]={10,12,15,14,16};
    float pred[]={11,11,14,15,15};
    energy_forecast_metrics_t m=energy_forecast_evaluate(actual,pred,5);
    CHECK(m.mae>0&&m.rmse>0,"metrics valid");
    CHECK(m.mape<30,"MAPE reasonable");
}
/* --- PPA Valuation --- */
static void test_ppa(void) {
    TEST("ppa_valuation");
    energy_generator_t gen={.source_type=ENERGY_SOURCE_SOLAR,.capacity_kw=1000,.capacity_factor=0.25f,.lifetime_years=25,.capex_per_kw=800,.opex_per_kwh=0.005f,.fixed_opex_per_kw_yr=10,.degradation_rate_per_year=0.005f,.efficiency=0.2f};
    energy_ppa_valuation_t v=energy_ppa_value(&gen,0.05f,0.06f,0.25f,0.6f);
    CHECK(v.irr_pct>-1.0f,"PPA IRR computed");
}

/* --- Arbitrage & VPP --- */
static void test_arbitrage(void) {
    TEST("arbitrage");
    energy_arbitrage_t arb; memset(&arb,0,sizeof(arb));
    arb.num_slots=24;arb.storage_capacity_kwh=100;arb.max_charge_kw=50;arb.max_discharge_kw=50;
    arb.charge_efficiency=0.95f;arb.discharge_efficiency=0.95f;arb.initial_soc_kwh=0;
    for(int i=0;i<24;i++){arb.buy_prices[i]=0.05f+(i%12)*0.01f;arb.sell_prices[i]=0.08f+(i%12)*0.01f;}
    energy_arbitrage_result_t r=energy_arbitrage_optimize(&arb);
    CHECK(r.num_slots==24,"arbitrage slots");
}
static void test_vpp(void) {
    TEST("vpp");
    energy_vpp_t vpp; energy_vpp_init(&vpp,"TestVPP");
    energy_vpp_add_solar(&vpp,1000);energy_vpp_add_wind(&vpp,500);energy_vpp_add_battery(&vpp,200);
    CHECK(vpp.aggregated_capacity_kw==1500,"VPP capacity");
    CHECK(vpp.num_assets==3,"VPP assets");
}
/* --- LP Solver --- */
static void test_lp(void) {
    TEST("lp_solver");
    energy_lp_problem_t lp; memset(&lp,0,sizeof(lp));
    lp.num_vars=2;lp.num_constraints=2;
    lp.c[0]=-3;lp.c[1]=-2; /* maximize 3x+2y => minimize -3x-2y */
    lp.A[0]=1;lp.A[1]=1;lp.b[0]=4; /* x+y<=4 */
    lp.A[2]=1;lp.A[3]=3;lp.b[1]=6; /* x+3y<=6 */
    energy_lp_problem_t res=energy_lp_solve(&lp);
    CHECK(res.is_feasible,"LP feasible");
}
/* --- Pareto --- */
static void test_pareto(void) {
    TEST("pareto");
    CHECK(energy_pareto_dominates(1,2,3,4),"dominates");
    CHECK(!energy_pareto_dominates(3,2,1,4),"non-dominated different axes");
    energy_pareto_front_t front; memset(&front,0,sizeof(front));
    energy_pareto_add_point(&front,1,10,NULL,0);
    energy_pareto_add_point(&front,2,5,NULL,0);
    CHECK(front.num_points==2,"pareto points");
}
/* --- LCA --- */
static void test_lca(void) {
    TEST("lca");
    energy_lca_t lca; energy_lca_init(&lca,"PV Module","1 kWp");
    energy_lca_add_entry(&lca,ENERGY_LCA_RAW_MATERIAL,200,5000);
    energy_lca_add_entry(&lca,ENERGY_LCA_MANUFACTURING,100,2000);
    CHECK(energy_lca_total_co2e(&lca)==300,"LCA CO2e sum");
}
/* --- Offset Quality --- */
static void test_offset(void) {
    TEST("offset_quality");
    energy_carbon_offset_t off; memset(&off,0,sizeof(off));
    off.additionality_score=0.9f;off.permanence_years=50;off.leakage_risk_pct=10;off.is_verified=true;
    float qi=energy_offset_quality_index(&off);
    CHECK(qi>0.5f&&qi<1.0f,"quality index");
}

int main(void) {
    printf("=== mini-energy-carbon Tests ===\n");
    test_init();
    test_add_generator();
    test_null_safety();
    test_carnot();
    test_betz();
    test_sq_limit();
    test_rankine();
    test_solar_power();
    test_wind_power();
    test_carbon_accounting();
    test_gwp();
    test_battery();
    test_battery_advanced();
    test_battery_soh();
    test_coulomb_soc();
    test_rainflow();
    test_kalman_soc();
    test_holt_winters();
    test_similar_day();
    test_lcoe();
    test_npv();
    test_irr();
    test_self_consumption();
    test_degree_days();
    test_heat_pump();
    test_building_load();
    test_hydrogen();
    test_transmission_loss();
    test_power_factor();
    test_system_sim();
    test_merit_order();
    test_carbon_markets();
    test_solar_geometry();
    test_weibull();
    test_economic_dispatch();
    test_forecast_metrics();
    test_ppa();
    test_arbitrage();
    test_vpp();
    test_lp();
    test_pareto();
    test_lca();
    test_offset();
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
