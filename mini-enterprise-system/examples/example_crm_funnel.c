/* example_crm_funnel.c - CRM: Lead-to-Opportunity pipeline, RFM, forecasting */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "crm_pipeline.h"

int main(void) {
  printf("=== CRM Pipeline Demo ===\n\n");
  MENT_CRMInstance *crm = ment_crm_create();

  /* 1. Create accounts */
  int acme  = ment_crm_add_account(crm, "Acme Corp", "Technology");
  int globex= ment_crm_add_account(crm, "Globex Inc", "Manufacturing");
  int inicorp=ment_crm_add_account(crm, "Initech", "Finance");
  printf("[1] Accounts: %d created\n", crm->num_accounts);

  /* 2. Add contacts */
  ment_crm_add_contact(crm, acme, "John", "Doe", "john@acme.com", true);
  ment_crm_add_contact(crm, globex, "Jane", "Smith", "jane@globex.com", true);
  ment_crm_add_contact(crm, inicorp, "Bob", "Wilson", "bob@initech.com", false);
  printf("[2] Contacts: %d added\n", crm->num_contacts);

  /* 3. Leads from various sources */
  int l1 = ment_crm_add_lead(crm, "TechStart", "Founder", "founder@techstart.com", "Referral");
  int l2 = ment_crm_add_lead(crm, "DataFlow", "CTO", "cto@dataflow.com", "Event");
  int l3 = ment_crm_add_lead(crm, "CloudServe", "VP Eng", "vp@cloudserve.com", "Web");
  int l4 = ment_crm_add_lead(crm, "SecureNet", "CEO", "ceo@sec.com", "ColdCall");
  printf("[3] Leads: %d captured\n", crm->num_leads);

  /* 4. Lead scoring */
  printf("\n[4] Lead Scoring:\n");
  printf("    Lead          | Source    | Score\n");
  printf("    --------------+-----------+------\n");
  for (int i = 0; i < crm->num_leads; i++) {
    ment_crm_score_lead(crm, crm->leads[i].id);
    printf("    %-14s | %-9s | %4d\n",
           crm->leads[i].company_name, crm->leads[i].source, crm->leads[i].score);
  }

  /* 5. Convert top leads */
  int opp1 = ment_crm_convert_lead(crm, l1);
  int opp2 = ment_crm_convert_lead(crm, l2);
  printf("\n[5] Converted: %d leads -> opportunities\n", 2);

  /* 6. Opportunity pipeline management */
  int opp3 = ment_crm_add_opportunity(crm, "Enterprise Deal", acme, 250000.0f, time(NULL)+86400*90);
  int opp4 = ment_crm_add_opportunity(crm, "SMB Bundle", inicorp, 45000.0f, time(NULL)+86400*45);
  ment_crm_update_stage(crm, opp1, MENT_OPP_NEGOTIATION);
  ment_crm_update_stage(crm, opp2, MENT_OPP_PROPOSAL);
  ment_crm_update_stage(crm, opp3, MENT_OPP_NEEDS_ANALYSIS);
  ment_crm_update_stage(crm, opp4, MENT_OPP_CLOSED_WON);

  printf("\n[6] Sales Pipeline:\n");
  printf("    Opportunity      | Stage          | Amount   | Prob  | Wtd Value\n");
  printf("    -----------------+----------------+----------+-------+----------\n");
  for (int i = 0; i < crm->num_opportunities; i++) {
    MENT_Opportunity *o = &crm->opportunities[i];
    if (o->is_closed && o->stage != MENT_OPP_CLOSED_WON) continue;
    const char *stages[] = {"Prospecting","Qualification","NeedsAnalysis",
                            "Proposal","Negotiation","ClosedWon","ClosedLost"};
    printf("    %-17s | %-14s | $%7.0f | %.0f%%  | $%7.0f\n",
           o->name, stages[o->stage], o->amount,
           o->probability * 100, o->amount * o->probability);
  }

  /* 7. Pipeline analytics */
  float pv = ment_crm_pipeline_value(crm);
  float wr = ment_crm_win_rate(crm, -1);
  printf("\n[7] Pipeline Analytics:\n");
  printf("    Total Pipeline Value: $%.2f\n", pv);
  printf("    Overall Win Rate: %.0f%%\n", wr * 100);

  /* 8. RFM Analysis */
  float r, f, m;
  ment_crm_rfm_score(crm, acme, &r, &f, &m);
  printf("\n[8] RFM Analysis (Acme Corp):\n");
  printf("    Recency: %.0f days\n", r);
  printf("    Frequency: %.0f deals\n", f);
  printf("    Monetary: $%.2f\n", m);

  /* 9. Forecast */
  time_t q_start = time(NULL);
  time_t q_end = q_start + 86400 * 90;
  float forecast = ment_crm_forecast(crm, q_start, q_end);
  printf("\n[9] Q3 Forecast: $%.2f\n", forecast);

  /* 10. Activities */
  ment_crm_add_activity(crm, 1, opp1, "Send proposal", time(NULL)+86400*2);
  ment_crm_add_activity(crm, 1, opp2, "Follow up call", time(NULL)-86400);
  int overdue_ids[16];
  int n_overdue = ment_crm_overdue_activities(crm, overdue_ids, 16);
  printf("[10] Activities: %d total, %d overdue\n",
         crm->num_activities, n_overdue);

  printf("\n=== CRM Demo Complete ===\n");
  ment_crm_destroy(crm);
  return 0;
}
