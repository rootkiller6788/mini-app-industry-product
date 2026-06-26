/* test_mrp_engine.c - MRP Engine Test Suite (10 tests) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "mrp_engine.h"

int test_mrp_suite(void) {
  printf("  Running MRP tests...\n");
  /*1: Instance creation */
  {MENT_MRPInstance*m=ment_mrp_create(26);assert(m);assert(m->horizon_weeks==26);ment_mrp_destroy(m);ment_mrp_destroy(NULL);}
  /*2: Item master */
  {MENT_MRPInstance*m=ment_mrp_create(52);int fg=ment_mrp_add_item(m,"FG001","Finished Good",MENT_ITEM_FINISHED_GOOD,7,100);int rm=ment_mrp_add_item(m,"RM001","Raw Material",MENT_ITEM_RAW_MATERIAL,3,200);int sa=ment_mrp_add_item(m,"SA001","Subassembly",MENT_ITEM_SUBASSEMBLY,5,50);assert(fg==1);assert(rm==2);assert(sa==3);assert(m->num_items==3);ment_mrp_destroy(m);}
  /*3: BOM structure */
  {MENT_MRPInstance*m=ment_mrp_create(52);ment_mrp_add_item(m,"FG","FG",MENT_ITEM_FINISHED_GOOD,7,0);ment_mrp_add_item(m,"SA","SA",MENT_ITEM_SUBASSEMBLY,5,0);ment_mrp_add_item(m,"RM","RM",MENT_ITEM_RAW_MATERIAL,3,0);int b1=ment_mrp_add_bom(m,1,2,2.0f);int b2=ment_mrp_add_bom(m,2,3,3.0f);assert(b1==1);assert(b2==2);assert(m->num_bom_lines==2);ment_mrp_destroy(m);}
  /*4: Work centers */
  {MENT_MRPInstance*m=ment_mrp_create(52);int wc1=ment_mrp_add_workcenter(m,"WC01",40.0f);int wc2=ment_mrp_add_workcenter(m,"WC02",80.0f);assert(wc1==1);assert(wc2==2);ment_mrp_destroy(m);}
  /*5: Routing */
  {MENT_MRPInstance*m=ment_mrp_create(52);ment_mrp_add_item(m,"FG","FG",MENT_ITEM_FINISHED_GOOD,7,0);ment_mrp_add_workcenter(m,"WC01",40.0f);int r=ment_mrp_add_routing(m,1,1,1,2.5f);assert(r==1);assert(m->num_routings==1);ment_mrp_destroy(m);}
  /*6: Demand and supply */
  {MENT_MRPInstance*m=ment_mrp_create(52);ment_mrp_add_item(m,"FG","FG",MENT_ITEM_FINISHED_GOOD,7,0);assert(ment_mrp_add_demand(m,1,3,1000)==0);assert(ment_mrp_add_supply(m,1,1,200,"OnHand")==0);assert(m->num_demands==1);assert(m->num_supplies==1);ment_mrp_destroy(m);}
  /*7: Low-level code assignment */
  {MENT_MRPInstance*m=ment_mrp_create(52);ment_mrp_add_item(m,"FG","FG",MENT_ITEM_FINISHED_GOOD,7,0);ment_mrp_add_item(m,"SA","SA",MENT_ITEM_SUBASSEMBLY,5,0);ment_mrp_add_item(m,"RM","RM",MENT_ITEM_RAW_MATERIAL,3,0);ment_mrp_add_bom(m,1,2,2.0f);ment_mrp_add_bom(m,2,3,3.0f);ment_mrp_assign_low_level_codes(m);assert(m->items[0].low_level_code==0);assert(m->items[1].low_level_code==1);assert(m->items[2].low_level_code==2);ment_mrp_destroy(m);}
  /*8: MRP explosion */
  {MENT_MRPInstance*m=ment_mrp_create(26);int fg=ment_mrp_add_item(m,"FG","FG",MENT_ITEM_FINISHED_GOOD,2,0);int rm=ment_mrp_add_item(m,"RM","RM",MENT_ITEM_RAW_MATERIAL,1,0);ment_mrp_add_bom(m,fg,rm,1.0f);ment_mrp_add_demand(m,fg,5,100);ment_mrp_add_supply(m,fg,1,50,"OnHand");int r=ment_mrp_explode(m);assert(r==0);assert(m->num_pegging>0);ment_mrp_destroy(m);}
  /*9: Lot sizing */
  {int net_req[10]={100,0,50,0,200,0,0,80,0,0};int planned[10]={0};int r=ment_mrp_lot_size(MENT_LOTSIZE_LOT_FOR_LOT,net_req,10,100.0f,1.0f,planned,0);assert(r==0);assert(planned[0]==100);assert(planned[2]==50);assert(planned[4]==200);memset(planned,0,sizeof(planned));r=ment_mrp_lot_size(MENT_LOTSIZE_EOQ,net_req,10,50.0f,0.5f,planned,0);assert(r==0);}
  /*10: CRP capacity check */
  {MENT_MRPInstance*m=ment_mrp_create(26);ment_mrp_add_item(m,"FG","FG",MENT_ITEM_FINISHED_GOOD,2,0);ment_mrp_add_workcenter(m,"WC01",40.0f);ment_mrp_add_routing(m,1,1,1,1.0f);ment_mrp_add_demand(m,1,3,200);ment_mrp_add_supply(m,1,1,100,"OnHand");ment_mrp_explode(m);float load[26],cap[26];int n=ment_mrp_capacity_check(m,1,load,cap,26);assert(n>0);ment_mrp_destroy(m);}
  printf("  MRP: 10 tests passed\n");
  return 0;
}
