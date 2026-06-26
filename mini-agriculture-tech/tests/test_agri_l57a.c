#include "../include/agri_core.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
static int tr=0,tp=0;
#define T(n) do{tr++;printf("  TEST %s ... ",n);}while(0)
#define P() do{tp++;printf("PASS\n");}while(0)
#define F(m) do{printf("FAIL: %s\n",m);return 1;}while(0)
#define C(c,m) if(!(c))F(m)

static int t_irrig(void){
    T("irrigation_scheduling");
    agri_season_context_t ctx; agri_season_config_t cfg;
    memset(&cfg,0,sizeof(cfg)); cfg.crop_type=AGRI_CROP_MAIZE; cfg.planting_doy=91; cfg.irrigation_efficiency=0.75f;
    agri_season_init(&ctx,&cfg);
    agri_soil_init(&ctx.soil,"loam"); agri_soil_add_layer(&ctx.soil,40,20,40,40,2.5f,6.5f);
    ctx.field.vertices[0]=agri_make_gps(39,-98,400);
    agri_weather_init(&ctx.weather);
    for(int d=0;d<100;d++) agri_weather_add_day(&ctx.weather,2025,5,1+d,25,15,(d%5==0)?5:0,22,50,2);
    agri_season_simulate(&ctx);
    agri_irrigation_schedule_t sched;
    agri_irrigation_optimize(&ctx,&sched,0.50f);
    C(sched.num_events>=0,"schedule generated");
    float rel=agri_irrigation_deficit_irrigation(&ctx,20);
    C(rel>=0&&rel<=100,"rel yield 0-100%");
    float wp=agri_irrigation_water_productivity(&ctx);
    C(wp>=0,"water prod nonneg");
    P(); return 0;
}

static int t_pest(void){
    T("pest_risk");
    agri_pest_predictor_t pp; agri_pest_init(&pp,AGRI_CROP_MAIZE);
    agri_pest_add_model(&pp,AGRI_PEST_FUNGAL,"Gray Leaf Spot",22,30,70,10);
    agri_pest_add_model(&pp,AGRI_PEST_INSECT,"Corn Borer",16,30,50,10);
    agri_weather_series_t ws; agri_weather_init(&ws); ws.start_doy=150;
    for(int d=0;d<60;d++) agri_weather_add_day(&ws,2025,6,1+d,26,16,(d%4==0)?8:0,20,70,2);
    agri_pest_risk_event_t risks[32]; int n=0;
    agri_pest_predict(&pp,&ws,risks,&n);
    C(n>0,"should predict risks");
    float dd=agri_pest_degree_day_risk(&pp.models[0],&ws,150,180);
    C(dd>=0&&dd<=1,"DD risk [0,1]");
    P(); return 0;
}

static int t_farm(void){
    T("farm_management");
    agri_farm_t* farm=malloc(sizeof(agri_farm_t));
    C(farm!=NULL,"malloc farm");
    agri_farm_init(farm,"Test Farm");
    agri_field_t field; memset(&field,0,sizeof(field));
    field.vertices[0]=agri_make_gps(40,-100,0); field.vertices[1]=agri_make_gps(40.001,-100,0);
    field.vertices[2]=agri_make_gps(40.001,-99.999,0); field.vertices[3]=agri_make_gps(40,-99.999,0);
    field.num_vertices=4; field.area_hectares=agri_polygon_area_ha(field.vertices,4);
    agri_soil_profile_t soil; agri_soil_init(&soil,"loam"); agri_soil_add_layer(&soil,30,20,40,40,2.5f,6.5f);
    agri_weather_series_t weather; agri_weather_init(&weather); weather.start_doy=91;
    for(int d=0;d<100;d++) agri_weather_add_day(&weather,2025,5,1+d,25,15,2,20,55,2);
    agri_season_config_t cfg; memset(&cfg,0,sizeof(cfg));
    cfg.crop_type=AGRI_CROP_WHEAT; cfg.planting_doy=91; cfg.irrigation_efficiency=0.75f;
    agri_farm_add_field(farm,&field,&cfg,&soil,&weather);
    agri_farm_simulate_all(farm);
    C(farm->total_predicted_yield_ton>0,"farm yield >0");
    C(agri_farm_add_field(NULL,NULL,NULL,NULL,NULL)==-1,"null add returns -1");
    free(farm);
    P(); return 0;
}

static int t_usle(void){
    T("usle_erosion");
    agri_usle_factors_t f={100,0.03f,1.5f,0.2f,0.5f};
    float A=agri_usle_soil_loss_ton_ha_yr(&f);
    C(A>0&&A<100,"erosion valid");
    agri_weather_series_t ws; agri_weather_init(&ws);
    for(int d=0;d<30;d++) agri_weather_add_day(&ws,2025,6,1+d,25,15,(d%3==0)?15:0,20,55,2);
    C(agri_usle_compute_R(&ws)>0,"R positive");
    agri_soil_profile_t soil; agri_soil_init(&soil,"silt_loam"); agri_soil_add_layer(&soil,30,18,55,27,2.5f,6.5f);
    C(agri_usle_compute_K(&soil)>0,"K positive");
    C(agri_usle_compute_LS(100,3)>0,"LS positive");
    C(agri_usle_compute_C(AGRI_CROP_MAIZE,AGRI_STAGE_VEGETATIVE)>0,"C positive");
    C(agri_usle_soil_loss_ton_ha_yr(NULL)==0,"null returns 0");
    P(); return 0;
}

static int t_fert(void){
    T("fertilizer_plan");
    agri_soil_test_t st={45,25,180,10,20,15};
    agri_fertilizer_plan_t plan;
    agri_fertilizer_plan(&st,AGRI_CROP_MAIZE,&plan);
    C(plan.nitrogen_kg_ha+plan.phosphorus_kg_ha+plan.potassium_kg_ha>0,"fert planned");
    float vr=agri_fertilizer_variable_rate(100,10,4);
    float vr2=agri_fertilizer_variable_rate(100,5,1);
    C(vr>vr2,"higher yield=higher VRT");
    C(agri_fertilizer_nitrogen_use_efficiency(NULL,NULL)==0,"null NUE 0");
    P(); return 0;
}

int main(void){
    printf("=== Agri Tests: L5-L7 APIs (part 1) ===\n\n");
    int f=0;
    f+=t_irrig(); f+=t_pest(); f+=t_farm(); f+=t_usle(); f+=t_fert();
    printf("\n=== %d/%d passed ===\n",tp,tr);
    return f>0?1:0;
}
