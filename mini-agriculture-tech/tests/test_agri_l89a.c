#include "../include/agri_core.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
static int tr=0,tp=0;
#define T(n) do{tr++;printf("  TEST %s ... ",n);}while(0)
#define P() do{tp++;printf("PASS\n");}while(0)
#define F(m) do{printf("FAIL: %s\n",m);return 1;}while(0)
#define C(c,m) if(!(c))F(m)

static int t_n(void){
    T("nitrogen_cycle");
    agri_nitrogen_pool_t np; agri_nitrogen_init(&np,5000,30,10);
    C(np.no3_n_kg_ha>0,"NO3 init");
    agri_nitrogen_step(&np,25,0.30f,0.35f,3,2.5f,5);
    C(np.mineralized_kg_ha_day>0,"min at 25C");
    float mr=agri_nitrogen_mineralization(5000,25,65,3);
    C(mr>0,"min rate>0");
    C(agri_nitrogen_denitrification(30,75,25,2)>0,"denitrification");
    C(agri_nitrogen_leaching(30,20,60,0.35f)>0,"leaching");
    agri_nitrogen_pool_t np2; agri_nitrogen_init(&np2,5000,30,10);
    agri_nitrogen_step(&np2,2,0.30f,0.35f,3,2.5f,0);
    C(np2.mineralized_kg_ha_day<np.mineralized_kg_ha_day,"cold<warm");
    P(); return 0;
}

static int t_clim(void){
    T("climate_impact");
    float ce=agri_climate_co2_fertilization_effect(550,AGRI_CROP_WHEAT);
    float ce2=agri_climate_co2_fertilization_effect(550,AGRI_CROP_MAIZE);
    C(ce>ce2,"C3>C4 CO2 response");
    float yr=agri_climate_yield_response(2,-10,1.1f,AGRI_CROP_MAIZE);
    C(yr<1,"warming+drying reduces yield");
    agri_season_context_t bl; agri_season_config_t cfg;
    memset(&cfg,0,sizeof(cfg)); cfg.crop_type=AGRI_CROP_WHEAT;
    agri_season_init(&bl,&cfg); bl.predicted_yield_ton_ha=8;
    agri_climate_scenario_t sc={2,-10,550,2050,20,30,10};
    agri_climate_impact_t imp;
    agri_climate_impact_assess(&bl,&sc,&imp);
    P(); return 0;
}

static int t_ins(void){
    T("insurance");
    agri_season_context_t ctx; agri_season_config_t cfg;
    memset(&cfg,0,sizeof(cfg)); cfg.crop_type=AGRI_CROP_MAIZE;
    agri_season_init(&ctx,&cfg); ctx.predicted_yield_ton_ha=10;
    agri_insurance_policy_t pol;
    agri_insurance_quote(&pol,&ctx,AGRI_INSURANCE_REVENUE_PROTECTION,0.75f,250);
    C(pol.coverage_level==0.75f,"coverage set");
    C(pol.premium_per_ha>0,"premium>0");
    float ind=agri_insurance_calculate_indemnity(&pol,5,220);
    C(ind>0,"indemnity on loss");
    C(agri_insurance_calculate_indemnity(&pol,10,250)==0,"no indemnity full yield");
    C(agri_insurance_actuarial_premium(10,2.5f,0.75f,250)>0,"actuarial premium");
    agri_insurance_policy_t yp;
    agri_insurance_quote(&yp,&ctx,AGRI_INSURANCE_YIELD_PROTECTION,0.75f,250);
    C(agri_insurance_calculate_indemnity(&yp,5,250)>0,"yield prot indemnity");
    P(); return 0;
}

static int t_wq(void){
    T("water_quality");
    agri_season_context_t ctx; agri_season_config_t cfg;
    memset(&cfg,0,sizeof(cfg)); cfg.crop_type=AGRI_CROP_MAIZE;
    agri_season_init(&ctx,&cfg);
    agri_soil_init(&ctx.soil,"loam"); agri_soil_add_layer(&ctx.soil,30,20,40,40,2.5f,6.5f);
    agri_weather_init(&ctx.weather);
    for(int d=0;d<30;d++) agri_weather_add_day(&ctx.weather,2025,6,1+d,25,15,(d%4==0)?12:0,20,55,2);
    agri_fertilizer_plan_t fp={150,40,100,15,2,1};
    agri_water_quality_t wq;
    agri_water_quality_estimate(&ctx,&fp,2,50,&wq);
    C(wq.total_n_loss_kg_ha>=0,"N loss nonneg");
    float cn=agri_runoff_curve_number(AGRI_CROP_WHEAT,&ctx.soil);
    C(cn>0&&cn<100,"CN valid");
    float sdr=agri_sediment_delivery_ratio(10,3);
    C(sdr>0&&sdr<1,"SDR valid");
    P(); return 0;
}

static int t_till(void){
    T("tillage_systems");
    agri_soil_profile_t soil; agri_soil_init(&soil,"loam"); agri_soil_add_layer(&soil,30,20,40,40,2.5f,6.5f);
    agri_tillage_system_t ts;
    agri_tillage_evaluate(AGRI_TILLAGE_NO_TILL,&soil,&ts);
    C(ts.surface_residue_pct>50,"no-till high residue");
    C(ts.erosion_reduction_pct>50,"no-till reduces erosion");
    agri_tillage_evaluate(AGRI_TILLAGE_CONVENTIONAL,&soil,&ts);
    C(ts.soil_disturbance_pct>50,"conventional disturbs");
    float decay=agri_tillage_residue_decay(5000,20,0.25f,30);
    C(decay>0,"residue decays");
    float soc=agri_tillage_carbon_impact(AGRI_TILLAGE_CONVENTIONAL,AGRI_TILLAGE_NO_TILL,50,20);
    C(soc>0,"switch to no-till sequesters C");
    P(); return 0;
}

int main(void){
    printf("=== Agri Tests: L7-L9 APIs (part 3) ===\n\n");
    int f=0;
    f+=t_n(); f+=t_clim(); f+=t_ins(); f+=t_wq(); f+=t_till();
    printf("\n=== %d/%d passed ===\n",tp,tr);
    return f>0?1:0;
}
