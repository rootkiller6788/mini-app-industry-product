#include "../include/agri_core.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
static int tr=0,tp=0;
#define T(n) do{tr++;printf("  TEST %s ... ",n);}while(0)
#define P() do{tp++;printf("PASS\n");}while(0)
#define F(m) do{printf("FAIL: %s\n",m);return 1;}while(0)
#define C(c,m) if(!(c))F(m)

static int t_econ(void){
    T("economic_analysis");
    agri_season_context_t ctx; agri_season_config_t cfg;
    memset(&cfg,0,sizeof(cfg)); cfg.crop_type=AGRI_CROP_MAIZE;
    agri_season_init(&ctx,&cfg); ctx.predicted_yield_ton_ha=10; ctx.total_irrigation_mm=300;
    agri_cost_breakdown_t c={180,350,120,90,200,0.25f,600,75,50};
    agri_economic_analysis_t ea;
    agri_economic_analyze(&ctx,&c,250,&ea);
    C(ea.expected_revenue_per_ha>0,"revenue >0");
    C(ea.break_even_yield_ton_ha>0,"BE yield >0");
    float pr[5]={200,225,250,275,300},yr[5]={6,8,10,12,14},mx[25];
    agri_economic_sensitivity(&ctx,&c,pr,yr,mx);
    C(agri_economic_breakeven_price(&c,10)>0,"BE price >0");
    P(); return 0;
}

static int t_harv(void){
    T("harvest_model");
    agri_season_context_t ctx; agri_season_config_t cfg;
    memset(&cfg,0,sizeof(cfg)); cfg.crop_type=AGRI_CROP_MAIZE;
    agri_season_init(&ctx,&cfg); ctx.predicted_yield_ton_ha=10; ctx.current.stage=AGRI_STAGE_MATURITY;
    agri_harvest_model_t hm;
    agri_harvest_model(&ctx,AGRI_HARVEST_COMBINE,&hm);
    C(hm.total_loss_pct>0,"loss>0");
    C(hm.marketable_yield_ton_ha<10,"marketable<full");
    float m=agri_harvest_dry_down(AGRI_CROP_MAIZE,28,12,15);
    C(m<28&&m>=12,"drydown works");
    float l1=agri_grain_storage_loss(100,16,25,60);
    float l2=agri_grain_storage_loss(100,12,10,60);
    C(l1>l2,"high moisture=more loss");
    P(); return 0;
}

static int t_rot(void){
    T("crop_rotation");
    float nb=agri_rotation_nitrogen_benefit(AGRI_CROP_SOYBEAN,AGRI_CROP_MAIZE);
    C(nb>0,"soybean->maize N");
    C(agri_rotation_nitrogen_benefit(AGRI_CROP_MAIZE,AGRI_CROP_MAIZE)==0,"maize->maize=0");
    C(agri_rotation_break_crop_effect(AGRI_CROP_SOYBEAN)>0,"break crop effect");
    agri_crop_t crops[4]; const agri_crop_t *cp;
    cp=agri_crop_lookup(AGRI_CROP_MAIZE); if(cp)crops[0]=*cp;
    cp=agri_crop_lookup(AGRI_CROP_SOYBEAN); if(cp)crops[1]=*cp;
    cp=agri_crop_lookup(AGRI_CROP_WHEAT); if(cp)crops[2]=*cp;
    cp=agri_crop_lookup(AGRI_CROP_POTATO); if(cp)crops[3]=*cp;
    agri_soil_profile_t soil; agri_soil_init(&soil,"loam"); agri_soil_add_layer(&soil,30,20,40,40,2.5f,6.5f);
    agri_rotation_plan_t plans[10]; int np=0;
    agri_rotation_optimize(crops,4,&soil,plans,&np);
    C(np>0,"rotation plans generated");
    C(plans[0].length>=2,">=2 crops");
    P(); return 0;
}

static int t_carb(void){
    T("carbon_farming");
    agri_soil_profile_t soil; agri_soil_init(&soil,"loam"); agri_soil_add_layer(&soil,30,20,40,40,3,6.5f);
    soil.soil_organic_carbon_ton_ha=45;
    float s1=agri_carbon_sequestration_potential(&soil,AGRI_CROP_MAIZE,true,true);
    float s2=agri_carbon_sequestration_potential(&soil,AGRI_CROP_MAIZE,false,false);
    C(s1>s2,"no-till+cover>conventional");
    C(agri_carbon_methane_from_rice(10,120,0)>0,"rice CH4");
    C(agri_carbon_n2o_from_fertilizer(200,0.01f)>0,"N2O from N");
    C(agri_carbon_sequestration_potential(NULL,AGRI_CROP_MAIZE,false,false)==0,"null soil 0");
    P(); return 0;
}

static int t_wgen(void){
    T("wgen");
    agri_wgen_params_t p; agri_wgen_init(&p);
    C(p.temp_lag_autocorr>0,"autocorr set");
    agri_weather_series_t ws;
    int nd=agri_wgen_generate(&p,2025,365,&ws,12345u);
    C(nd>0,"WGEN generates");
    agri_wgen_params_t e; agri_wgen_estimate_from_weather(&ws,&e);
    C(e.monthly_tmax_mean[6]>0,"July tmax>0");
    P(); return 0;
}

int main(void){
    printf("=== Agri Tests: L7-L8 APIs (part 2) ===\n\n");
    int f=0;
    f+=t_econ(); f+=t_harv(); f+=t_rot(); f+=t_carb(); f+=t_wgen();
    printf("\n=== %d/%d passed ===\n",tp,tr);
    return f>0?1:0;
}
