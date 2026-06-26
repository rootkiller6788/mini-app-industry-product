#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>
#include "smart_manufacturing.h"
#include "production_core.h"
#include "quality_control.h"
#include "oee_monitor.h"
#include "mes_core.h"
#include "supply_chain.h"

#define EPS 0.01f
static int tests_run = 0, tests_passed = 0;
#define TEST(n) do{tests_run++;printf("  TEST %s... ",n);}while(0)
#define PASS() do{tests_passed++;printf("PASSED\n");}while(0)
#define FAIL(m) do{printf("FAILED: %s\n",m);}while(0)
#define CHECK(c,m) do{if(!(c)){FAIL(m);return;}}while(0)
#define CHECKF(a,b,m) do{if(fabsf((a)-(b))>EPS){FAIL(m);printf("    exp %.4f got %.4f\n",(float)(b),(float)(a));return;}}while(0)

static void t0(void){TEST("Factory");SMF_Factory*f=smf_factory_create("T","L");CHECK(f,"create");CHECK(f->num_machines==0,"mach");smf_factory_destroy(f);PASS();}
static void t1(void){TEST("Machine");SMF_Factory*f=smf_factory_create("M","L");smf_add_machine(f,"M01","T",SMF_MACH_CNC);CHECK(smf_machine_set_state(f,0,SMF_MACH_RUNNING)==0,"state");CHECK(smf_machine_record_production(f,0,95,5)==0,"prod");CHECK(f->machines[0].good_parts==95,"good");smf_factory_destroy(f);PASS();}
static void t2(void){TEST("Material");SMF_Factory*f=smf_factory_create("M","T");int m=smf_add_material(f,"S","Steel",SMF_MAT_RAW,"kg",3.5f);smf_material_receive(f,m,500);smf_material_issue(f,m,100);CHECKF(f->materials[m].quantity_on_hand,400,"qty");CHECK(smf_material_issue(f,m,9999)==-2,"insuf");float e=smf_calc_eoq(10000,50,2);CHECKF(e,707.1068f,"EOQ");f->materials[m].reorder_point=500;CHECK(smf_material_needs_reorder(&f->materials[m]),"reorder");smf_factory_destroy(f);PASS();}
static void t3(void){TEST("BOM");SMF_Factory*f=smf_factory_create("B","T");smf_add_material(f,"A","A",SMF_MAT_RAW,"pcs",1);smf_add_material(f,"B","B",SMF_MAT_RAW,"pcs",2);smf_add_material(f,"C","C",SMF_MAT_RAW,"pcs",3);int p=smf_add_product(f,"P1","W","A");int b=smf_add_bom(f,"B1",p);smf_bom_add_item(f,b,0,2,1);smf_bom_add_item(f,b,1,1,1);smf_bom_add_item(f,b,2,3,1);float q[8]={0};CHECK(smf_bom_explode(f,p,10,q,8)==3,"exp");CHECKF(q[0],20,"A");CHECKF(q[1],10,"B");CHECKF(q[2],30,"C");CHECKF(smf_product_standard_cost(f,p),13,"cost");smf_factory_destroy(f);PASS();}
static void t4(void){TEST("SPM SPT");SMF_SchedulingJob j[4];float t[]={5,2,8,3};for(int i=0;i<4;i++){smf_sched_job_init(&j[i],i,"",1);smf_sched_job_set_op(&j[i],0,0,t[i]);}SMF_Schedule s=smf_schedule_single_machine(j,4,SMF_DISPATCH_SPT,0);CHECK(s.num_entries==4,"ent");CHECKF(s.makespan,18,"mk");PASS();}
static void t5(void){TEST("Johnson");SMF_SchedulingJob j[5];float tt[5][2]={{3,6},{7,2},{4,7},{5,3},{7,4}};for(int i=0;i<5;i++){smf_sched_job_init(&j[i],i,"",2);smf_sched_job_set_op(&j[i],0,0,tt[i][0]);smf_sched_job_set_op(&j[i],1,1,tt[i][1]);}SMF_Schedule s=smf_schedule_johnson_2machine(j,5);CHECK(s.num_entries>0,"ent");CHECK(s.makespan>0,"mk");PASS();}
static void t6(void){TEST("WorkOrder");SMF_Factory*f=smf_factory_create("W","T");smf_add_material(f,"R","Raw",SMF_MAT_RAW,"pcs",1);int p=smf_add_product(f,"P","P","A");smf_add_bom(f,"B",p);smf_bom_add_item(f,0,0,1,1);smf_add_workstation(f,"W","W","D");smf_add_routing(f,"R",p);smf_routing_add_step(f,0,SMF_OP_ASSEMBLY,"A",0,0.5f,1,0);time_t n=time(NULL);SMF_WorkOrder*wo=smf_wo_create(f,p,100,n,n+86400,1);CHECK(wo,"cr");CHECK(wo->state==SMF_WO_PLANNED,"pl");CHECK(smf_wo_release(f,0)==0,"rel");CHECK(smf_wo_complete_step(f,0,0,50,2)==0,"st");CHECKF(smf_wo_progress(wo),50,"prg");CHECK(smf_wo_close(f,0)==0,"cl");smf_factory_destroy(f);PASS();}
static void t7(void){TEST("SPC");SMF_ControlChart cc;CHECK(smf_spc_init_xbar_r(&cc,"D","Dia",10.1f,9.9f,10.0f,5)==0,"init");CHECKF(cc.a2,0.577f,"A2");SMF_SPCSubgroup sg[10];float m[10][5]={{10.02,9.98,10.01,9.99,10},{10,10.03,9.97,10.02,9.98},{10.01,10,9.99,10.02,9.98},{10.03,9.99,10,10.01,9.97},{9.98,10.02,10,9.99,10.01},{10,9.99,10.01,9.98,10.02},{10.02,10.01,9.99,10,9.98},{9.99,10,10.02,9.97,10.03},{10.01,9.98,10,10.02,9.99},{10,10.01,9.99,9.98,10.02}};for(int i=0;i<10;i++)smf_spc_add_subgroup(&sg[i],m[i],5);CHECK(smf_spc_compute_limits(&cc,sg,10)==10,"lim");CHECK(cc.is_stable,"stab");CHECK(cc.cp>0,"cp");PASS();}
static void t8(void){TEST("OEE");SMF_OEEMetrics o;CHECK(smf_oee_calculate(&o,8,7,1.0f/60,400,380)==0,"calc");CHECKF(o.availability,0.875f,"A");CHECKF(o.oee,0.875f*(400.0f/420)*0.95f,"OEE");PASS();}
static void t9(void){TEST("FMEA");SMF_FMEARecord r[3]={{0,0,"","","",8,5,3,0,"",false,0},{0,0,"","","",6,8,4,0,"",false,0},{0,0,"","","",3,3,2,0,"",false,0}};smf_fmea_sort_by_rpn(r,3);CHECK(r[0].rpn==192,"r1");CHECK(r[1].rpn==120,"r2");PASS();}
static void ta(void){TEST("EOQ/SS");CHECKF(smf_eoq_compute(12000,80,3),800,"EOQ");CHECKF(smf_safety_stock(1.96f,15,4),58.8f,"SS");CHECKF(smf_reorder_point(50,4,58.8f),258.8f,"ROP");PASS();}
static void tb(void){TEST("Takt");CHECKF(smf_calc_takt_time(480,240),2,"takt");float ts[]={1.5f,1,1.8f,0.8f,1.2f,1.5f};CHECK(smf_calc_min_workstations(ts,6,2)==4,"ws");PASS();}
static void tc(void){TEST("Kanban");CHECK(smf_kanban_calc_cards(50,2,0.2f,10)==12,"cards");PASS();}

int main(void){
printf("mini-smart-manufacturing Test Suite\n========================================\n\n");
t0();t1();t2();t3();t4();t5();t6();t7();t8();t9();ta();tb();tc();
printf("\n========================================\n  %d / %d TESTS PASSED\n========================================\n",tests_passed,tests_run);
return (tests_passed==tests_run)?0:1;
}