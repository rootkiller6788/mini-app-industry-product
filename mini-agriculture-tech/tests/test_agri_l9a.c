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

static int t_ymap(void){
    T("yield_map_vrt");
    agri_yield_map_t map;
    agri_yield_map_init(&map,5,5,10);
    C(map.rows==5&&map.cols==5,"dimensions");
    agri_yield_map_set(&map,2,2,6);
    C(agri_yield_map_get(&map,2,2)>0,"yield get");
    C(agri_yield_map_average(&map)>0,"avg>0");
    int zones[25];
    C(agri_yield_map_zones(&map,3,zones)>=2,"multiple zones");
    agri_yield_map_free(&map);
    P(); return 0;
}

static int t_grain(void){
    T("grain_quality");
    agri_season_context_t ctx; agri_season_config_t cfg;
    memset(&cfg,0,sizeof(cfg)); cfg.crop_type=AGRI_CROP_WHEAT;
    agri_season_init(&ctx,&cfg); ctx.predicted_yield_ton_ha=6;
    for(int d=0;d<10;d++){ctx.history[d].stage=AGRI_STAGE_GRAIN_FILL;ctx.history[d].doy=182+d;}
    ctx.days_simulated=10;
    agri_grain_quality_t gq;
    agri_grain_quality_assess(&ctx,&gq);
    C(strlen(gq.quality_grade)>0,"grade assigned");
    P(); return 0;
}

static int t_inter(void){
    T("intercropping");
    agri_intercrop_system_t ics;
    agri_intercrop_evaluate(AGRI_CROP_MAIZE,AGRI_CROP_SOYBEAN,&ics);
    C(ics.land_equivalent_ratio>1,"LER>1");
    agri_intercrop_evaluate(AGRI_CROP_MAIZE,AGRI_CROP_MAIZE,&ics);
    C(ics.land_equivalent_ratio==1,"same crop LER=1");
    P(); return 0;
}

static int t_crop(void){
    T("crop_lookup");
    const agri_crop_t* cp=agri_crop_lookup(AGRI_CROP_WHEAT);
    C(cp!=NULL&&cp->type==AGRI_CROP_WHEAT,"wheat found");
    const agri_crop_t* cp2=agri_crop_lookup(999);
    C(cp2!=NULL&&cp2->type==AGRI_CROP_CUSTOM,"OOB returns custom");
    P(); return 0;
}

int main(void){
    printf("=== Agri Tests: L9 APIs ===\n\n");
    int f=0;
    f+=t_ymap(); f+=t_grain(); f+=t_inter(); f+=t_crop();
    printf("\n=== %d/%d passed ===\n",tp,tr);
    return f>0?1:0;
}
