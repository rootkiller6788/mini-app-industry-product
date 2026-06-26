#include "crm_pipeline.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ================================================================
 * CRM Pipeline Implementation
 *
 * L2: Sales funnel, pipeline stages, conversion rates
 * L3: Pipeline state machine, territory hierarchy, activity log
 * L5: Lead scoring, opportunity forecasting, RFM analysis
 * L6: Sales pipeline management
 * ================================================================ */

MENT_CRMInstance* ment_crm_create(void) {
    MENT_CRMInstance *crm = (MENT_CRMInstance*)malloc(sizeof(MENT_CRMInstance));
    if (!crm) return NULL;
    memset(crm, 0, sizeof(MENT_CRMInstance));
    crm->next_id = 1;
    crm->activities = (MENT_Activity*)malloc(
        MENT_CRM_MAX_ACTIVITIES * sizeof(MENT_Activity));
    if (!crm->activities) { free(crm); return NULL; }
    memset(crm->activities, 0, MENT_CRM_MAX_ACTIVITIES * sizeof(MENT_Activity));
    return crm;
}

void ment_crm_destroy(MENT_CRMInstance *crm) {
    if (!crm) return;
    free(crm->activities);
    free(crm);
}

int ment_crm_add_account(MENT_CRMInstance *crm, const char *name,
                          const char *industry) {
    if (crm->num_accounts >= MENT_CRM_MAX_ACCOUNTS) return -1;
    int id = crm->next_id++;
    MENT_CRMAccount *acc = &crm->accounts[crm->num_accounts++];
    memset(acc, 0, sizeof(MENT_CRMAccount));
    acc->id = id;
    acc->is_active = true;
    acc->created_at = time(NULL);
    snprintf(acc->company_name, sizeof(acc->company_name), "%s", name);
    snprintf(acc->industry, sizeof(acc->industry), "%s", industry);
    return id;
}

int ment_crm_add_contact(MENT_CRMInstance *crm, int account_id,
                          const char *first, const char *last,
                          const char *email, bool gdpr) {
    if (crm->num_contacts >= MENT_CRM_MAX_CONTACTS) return -1;
    int id = crm->next_id++;
    MENT_Contact *con = &crm->contacts[crm->num_contacts++];
    memset(con, 0, sizeof(MENT_Contact));
    con->id = id;
    con->account_id = account_id;
    con->gdpr_consent = gdpr;
    if (gdpr) con->consent_date = time(NULL);
    snprintf(con->first_name, sizeof(con->first_name), "%s", first);
    snprintf(con->last_name, sizeof(con->last_name), "%s", last);
    snprintf(con->email, sizeof(con->email), "%s", email);
    return id;
}

int ment_crm_add_lead(MENT_CRMInstance *crm, const char *company,
                       const char *contact, const char *email,
                       const char *source) {
    if (crm->num_leads >= MENT_CRM_MAX_LEADS) return -1;
    int id = crm->next_id++;
    MENT_Lead *lead = &crm->leads[crm->num_leads++];
    memset(lead, 0, sizeof(MENT_Lead));
    lead->id = id;
    lead->status = MENT_LEAD_NEW;
    lead->score = 0;
    lead->converted_opp_id = -1;
    lead->created_at = time(NULL);
    snprintf(lead->company_name, sizeof(lead->company_name), "%s", company);
    snprintf(lead->contact_name, sizeof(lead->contact_name), "%s", contact);
    snprintf(lead->email, sizeof(lead->email), "%s", email);
    snprintf(lead->source, sizeof(lead->source), "%s", source);
    return id;
}

/* Lead Scoring (L5):
 * Score = company_quality + source_weight + engagement_heuristic */
int ment_crm_score_lead(MENT_CRMInstance *crm, int lead_id) {
    int idx = -1;
    for (int i = 0; i < crm->num_leads; i++)
        if (crm->leads[i].id == lead_id) { idx = i; break; }
    if (idx < 0) return -1;

    MENT_Lead *lead = &crm->leads[idx];
    int score = 30; /* base */

    /* Company quality: heuristic based on name length and domain */
    int name_len = (int)strlen(lead->company_name);
    if (name_len > 20) score += 15;

    /* Source weight */
    if (strstr(lead->source, "Referral")) score += 30;
    else if (strstr(lead->source, "Event")) score += 20;
    else if (strstr(lead->source, "Web")) score += 10;
    else score += 5;

    /* Email engagement heuristic */
    if (strstr(lead->email, "gmail") || strstr(lead->email, "yahoo"))
        score -= 5;
    else score += 5;

    if (lead->last_contacted > 0) {
        time_t now = time(NULL);
        double days = difftime(now, lead->last_contacted) / 86400.0;
        if (days < 7) score += 20;  /* recent contact */
        else if (days < 30) score += 10;
        else score -= 10;  /* stale */
    }

    if (score > 100) score = 100;
    if (score < 0) score = 0;
    lead->score = score;
    return score;
}

/* Convert lead: create account, contact, and opportunity */
int ment_crm_convert_lead(MENT_CRMInstance *crm, int lead_id) {
    int idx = -1;
    for (int i = 0; i < crm->num_leads; i++)
        if (crm->leads[i].id == lead_id) { idx = i; break; }
    if (idx < 0 || crm->leads[idx].status == MENT_LEAD_CONVERTED) return -1;

    MENT_Lead *lead = &crm->leads[idx];

    /* Create account */
    int acc_id = ment_crm_add_account(crm, lead->company_name, "Unknown");
    /* Create contact */
    ment_crm_add_contact(crm, acc_id, lead->contact_name, "",
                         lead->email, true);
    /* Create opportunity */
    int opp_id = ment_crm_add_opportunity(crm, "New Opportunity",
                                           acc_id, 10000.0, time(NULL) + 86400*90);

    lead->status = MENT_LEAD_CONVERTED;
    lead->converted_opp_id = opp_id;
    lead->last_contacted = time(NULL);
    return opp_id;
}

int ment_crm_add_opportunity(MENT_CRMInstance *crm, const char *name,
                              int account_id, float amount,
                              time_t close_date) {
    if (crm->num_opportunities >= MENT_CRM_MAX_OPPORTUNITIES) return -1;
    int id = crm->next_id++;
    MENT_Opportunity *opp = &crm->opportunities[crm->num_opportunities++];
    memset(opp, 0, sizeof(MENT_Opportunity));
    opp->id = id;
    opp->account_id = account_id;
    opp->amount = amount;
    opp->stage = MENT_OPP_PROSPECTING;
    opp->probability = 0.10f;
    opp->expected_close_date = close_date;
    opp->is_closed = false;
    snprintf(opp->name, sizeof(opp->name), "%s", name);
    return id;
}

int ment_crm_update_stage(MENT_CRMInstance *crm, int opp_id,
                           MENT_OpportunityStage stage) {
    int idx = -1;
    for (int i = 0; i < crm->num_opportunities; i++)
        if (crm->opportunities[i].id == opp_id) { idx = i; break; }
    if (idx < 0) return -1;

    MENT_Opportunity *opp = &crm->opportunities[idx];
    opp->stage = stage;
    switch (stage) {
        case MENT_OPP_PROSPECTING:    opp->probability = 0.10f; break;
        case MENT_OPP_QUALIFICATION:  opp->probability = 0.25f; break;
        case MENT_OPP_NEEDS_ANALYSIS: opp->probability = 0.40f; break;
        case MENT_OPP_PROPOSAL:       opp->probability = 0.60f; break;
        case MENT_OPP_NEGOTIATION:    opp->probability = 0.80f; break;
        case MENT_OPP_CLOSED_WON:     opp->probability = 1.00f; opp->is_closed = true; opp->actual_close_date = time(NULL); break;
        case MENT_OPP_CLOSED_LOST:    opp->probability = 0.00f; opp->is_closed = true; opp->actual_close_date = time(NULL); break;
    }
    return 0;
}

float ment_crm_pipeline_value(const MENT_CRMInstance *crm) {
    float total = 0.0f;
    for (int i = 0; i < crm->num_opportunities; i++) {
        const MENT_Opportunity *opp = &crm->opportunities[i];
        if (!opp->is_closed)
            total += opp->amount * opp->probability;
    }
    return total;
}

int ment_crm_pipeline_stages(const MENT_CRMInstance *crm,
                              int *counts, MENT_OpportunityStage *stages,
                              int max_stages) {
    (void)stages;
    memset(counts, 0, (size_t)max_stages * sizeof(int));
    for (int i = 0; i < crm->num_opportunities; i++) {
        int s = (int)crm->opportunities[i].stage;
        if (s < max_stages) counts[s]++;
    }
    int n = 0;
    for (int s = 0; s < max_stages; s++)
        if (counts[s] > 0) n++;
    return n;
}

float ment_crm_win_rate(const MENT_CRMInstance *crm, int owner_id) {
    int won = 0, closed = 0;
    for (int i = 0; i < crm->num_opportunities; i++) {
        const MENT_Opportunity *opp = &crm->opportunities[i];
        if (owner_id >= 0 && opp->owner_user_id != owner_id) continue;
        if (opp->stage == MENT_OPP_CLOSED_WON) won++;
        if (opp->stage == MENT_OPP_CLOSED_WON ||
            opp->stage == MENT_OPP_CLOSED_LOST) closed++;
    }
    if (closed == 0) return 0.0f;
    return (float)won / (float)closed;
}

int ment_crm_rfm_score(const MENT_CRMInstance *crm, int account_id,
                        float *r, float *f, float *m) {
    time_t now = time(NULL);
    *r = 365.0f; /* days since last deal */
    *f = 0.0f;   /* number of deals */
    *m = 0.0f;   /* total value */

    for (int i = 0; i < crm->num_opportunities; i++) {
        const MENT_Opportunity *opp = &crm->opportunities[i];
        if (opp->account_id != account_id) continue;
        if (opp->stage == MENT_OPP_CLOSED_WON) {
            float days = (float)difftime(now, opp->actual_close_date) / 86400.0f;
            if (days < *r) *r = days;
            (*f)++;
            *m += opp->amount;
        }
    }
    return 0;
}

float ment_crm_forecast(const MENT_CRMInstance *crm,
                         time_t period_start, time_t period_end) {
    float total = 0.0f;
    for (int i = 0; i < crm->num_opportunities; i++) {
        const MENT_Opportunity *opp = &crm->opportunities[i];
        if (opp->is_closed) continue;
        if (opp->expected_close_date >= period_start &&
            opp->expected_close_date <= period_end)
            total += opp->amount * opp->probability;
    }
    return total;
}

int ment_crm_add_activity(MENT_CRMInstance *crm, int related_type,
                           int related_id, const char *subject,
                           time_t due_date) {
    if (crm->num_activities >= MENT_CRM_MAX_ACTIVITIES) return -1;
    int id = crm->next_id++;
    MENT_Activity *act = &crm->activities[crm->num_activities++];
    memset(act, 0, sizeof(MENT_Activity));
    act->id = id;
    act->related_type = related_type;
    act->related_id = related_id;
    act->due_date = due_date;
    act->is_completed = false;
    snprintf(act->subject, sizeof(act->subject), "%s", subject);
    return id;
}

int ment_crm_overdue_activities(const MENT_CRMInstance *crm,
                                 int *activity_ids, int max_results) {
    time_t now = time(NULL);
    int count = 0;
    for (int i = 0; i < crm->num_activities && count < max_results; i++) {
        if (!crm->activities[i].is_completed &&
            crm->activities[i].due_date < now &&
            crm->activities[i].due_date > 0)
            activity_ids[count++] = crm->activities[i].id;
    }
    return count;
}
