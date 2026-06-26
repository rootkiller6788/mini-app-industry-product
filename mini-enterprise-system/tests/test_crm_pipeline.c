/* test_crm_pipeline.c - CRM Pipeline Test Suite (10 tests) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "crm_pipeline.h"

#define AF(a,b) do{float _a=(a),_b=(b); if(fabsf(_a-_b)>0.01f){fprintf(stderr,"FAIL %.4f!=%.4f\n",_a,_b);assert(0);}}while(0)

int test_crm_suite(void) {
  printf("  Running CRM tests...\n");
  /*1: Instance */
  {MENT_CRMInstance*c=ment_crm_create();assert(c);assert(c->next_id==1);ment_crm_destroy(c);ment_crm_destroy(NULL);}
  /*2: Accounts */
  {MENT_CRMInstance*c=ment_crm_create();int a1=ment_crm_add_account(c,"Acme Corp","Technology");int a2=ment_crm_add_account(c,"Globex","Manufacturing");assert(a1>0);assert(a2>a1);assert(c->num_accounts==2);ment_crm_destroy(c);}
  /*3: Contacts */
  {MENT_CRMInstance*c=ment_crm_create();int a=ment_crm_add_account(c,"Acme","Tech");int ct1=ment_crm_add_contact(c,a,"John","Doe","john@acme.com",true);int ct2=ment_crm_add_contact(c,a,"Jane","Doe","jane@acme.com",false);assert(ct1>0);assert(ct2>ct1);assert(c->num_contacts==2);assert(c->contacts[0].gdpr_consent);assert(!c->contacts[1].gdpr_consent);ment_crm_destroy(c);}
  /*4: Leads */
  {MENT_CRMInstance*c=ment_crm_create();int l1=ment_crm_add_lead(c,"Startup Inc","John","john@startup.com","Web");int l2=ment_crm_add_lead(c,"BigCo","Jane","jane@bigco.com","Referral");assert(l1>0);assert(l2>l1);assert(c->leads[0].status==MENT_LEAD_NEW);ment_crm_destroy(c);}
  /*5: Lead scoring */
  {MENT_CRMInstance*c=ment_crm_create();int l=ment_crm_add_lead(c,"Big Enterprise Ltd","John","john@enterprise.com","Referral");int score=ment_crm_score_lead(c,l);assert(score>0);assert(score<=100);assert(c->leads[0].score==score);ment_crm_destroy(c);}
  /*6: Lead conversion */
  {MENT_CRMInstance*c=ment_crm_create();int l=ment_crm_add_lead(c,"NewCo","John","john@newco.com","Event");ment_crm_score_lead(c,l);c->leads[0].status=MENT_LEAD_QUALIFIED;int opp=ment_crm_convert_lead(c,l);assert(opp>0);assert(c->leads[0].status==MENT_LEAD_CONVERTED);assert(c->num_accounts==1);assert(c->num_contacts==1);assert(c->num_opportunities==1);ment_crm_destroy(c);}
  /*7: Opportunities */
  {MENT_CRMInstance*c=ment_crm_create();int a=ment_crm_add_account(c,"Acme","Tech");int opp=ment_crm_add_opportunity(c,"Big Deal",a,50000.0f,time(0)+86400*60);assert(opp>0);assert(c->opportunities[0].stage==MENT_OPP_PROSPECTING);ment_crm_update_stage(c,opp,MENT_OPP_PROPOSAL);assert(c->opportunities[0].stage==MENT_OPP_PROPOSAL);AF(c->opportunities[0].probability,0.60f);ment_crm_update_stage(c,opp,MENT_OPP_CLOSED_WON);assert(c->opportunities[0].is_closed);AF(c->opportunities[0].probability,1.00f);ment_crm_destroy(c);}
  /*8: Pipeline analysis */
  {MENT_CRMInstance*c=ment_crm_create();int a=ment_crm_add_account(c,"Acme","Tech");int o1=ment_crm_add_opportunity(c,"Deal A",a,100000.0f,time(0)+86400*30);int o2=ment_crm_add_opportunity(c,"Deal B",a,50000.0f,time(0)+86400*60);ment_crm_update_stage(c,o1,MENT_OPP_NEGOTIATION);float pv=ment_crm_pipeline_value(c);assert(pv>0);AF(pv,100000.0f*0.80f+50000.0f*0.10f);int counts[7];MENT_OpportunityStage stages[7];int n=ment_crm_pipeline_stages(c,counts,stages,7);assert(n>0);float wr=ment_crm_win_rate(c,-1);assert(wr>=0);ment_crm_destroy(c);}
  /*9: RFM analysis */
  {MENT_CRMInstance*c=ment_crm_create();int a=ment_crm_add_account(c,"Acme","Tech");int o=ment_crm_add_opportunity(c,"Deal",a,10000.0f,time(0)+86400*30);ment_crm_update_stage(c,o,MENT_OPP_CLOSED_WON);float r,f,m;ment_crm_rfm_score(c,a,&r,&f,&m);assert(f>0);assert(m>0);ment_crm_destroy(c);}
  /*10: Forecast & activities */
  {MENT_CRMInstance*c=ment_crm_create();int a=ment_crm_add_account(c,"Acme","Tech");int o=ment_crm_add_opportunity(c,"Q1 Deal",a,100000.0f,time(0)+86400*45);ment_crm_update_stage(c,o,MENT_OPP_PROPOSAL);float fc=ment_crm_forecast(c,time(0),time(0)+86400*90);assert(fc>0);int act=ment_crm_add_activity(c,1,o,"Follow up",time(0)-86400);assert(act>0);int overdue[16];int n=ment_crm_overdue_activities(c,overdue,16);assert(n>0);ment_crm_destroy(c);}
  printf("  CRM: 10 tests passed\n");
  return 0;
}
