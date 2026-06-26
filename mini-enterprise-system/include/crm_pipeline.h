#ifndef MINI_ENT_CRM_PIPELINE_H
#define MINI_ENT_CRM_PIPELINE_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* ================================================================
 *  CRM Pipeline — Customer Relationship Management
 *
 *  L1: Core types — Lead, Opportunity, Account, Contact, Activity
 *  L2: Core concepts — Sales funnel, pipeline stages, conversion
 *      rates, customer lifecycle, RFM analysis
 *  L3: Engineering structures — Pipeline state machine, territory
 *      hierarchy, activity log
 *  L4: Standards — GDPR (EU 2016/679) consent management,
 *      CAN-SPAM compliance
 *  L5: Algorithms — Lead scoring, opportunity forecasting,
 *      territory assignment, churn prediction
 *  L6: Canonical problems — Sales pipeline management
 *  L7: Applications — B2B CRM, e-commerce customer analytics
 *  L8: Advanced — AI lead scoring, predictive analytics
 *  L9: Industry — Salesforce, HubSpot, Microsoft Dynamics 365
 *
 *  References:
 *  - Buttle & Maklan, "Customer Relationship Management" (2019)
 *  - Kotler & Keller, "Marketing Management" (2016)
 * ================================================================ */

#define MENT_CRM_MAX_LEADS         32768
#define MENT_CRM_MAX_OPPORTUNITIES 16384
#define MENT_CRM_MAX_ACCOUNTS      8192
#define MENT_CRM_MAX_CONTACTS      32768
#define MENT_CRM_MAX_ACTIVITIES    131072

typedef enum {
    MENT_LEAD_NEW         = 0,
    MENT_LEAD_CONTACTED   = 1,
    MENT_LEAD_QUALIFIED   = 2,
    MENT_LEAD_CONVERTED   = 3,
    MENT_LEAD_DISQUALIFIED = 4
} MENT_LeadStatus;

typedef enum {
    MENT_OPP_PROSPECTING  = 0,
    MENT_OPP_QUALIFICATION = 1,
    MENT_OPP_NEEDS_ANALYSIS = 2,
    MENT_OPP_PROPOSAL     = 3,
    MENT_OPP_NEGOTIATION  = 4,
    MENT_OPP_CLOSED_WON   = 5,
    MENT_OPP_CLOSED_LOST  = 6
} MENT_OpportunityStage;

typedef struct {
    int      id;
    char     company_name[128];
    char     industry[64];
    char     website[128];
    char     phone[24];
    int      num_employees;
    float    annual_revenue;
    int      territory_id;
    int      owner_user_id;
    time_t   created_at;
    bool     is_active;
} MENT_CRMAccount;

typedef struct {
    int      id;
    int      account_id;
    char     first_name[32];
    char     last_name[32];
    char     title[64];
    char     email[64];
    char     phone[24];
    bool     is_primary;
    bool     gdpr_consent;
    time_t   consent_date;
} MENT_Contact;

typedef struct {
    int      id;
    char     company_name[128];
    char     contact_name[64];
    char     email[64];
    char     phone[24];
    char     source[32];        /* "Web", "Referral", "Event", "ColdCall" */
    MENT_LeadStatus status;
    int      score;             /* 0-100 */
    int      owner_user_id;
    int      converted_opp_id;  /* -1 if not converted */
    time_t   created_at;
    time_t   last_contacted;
} MENT_Lead;

typedef struct {
    int      id;
    char     name[128];
    int      account_id;
    int      primary_contact_id;
    float    amount;
    float    probability;       /* 0.0 - 1.0 */
    MENT_OpportunityStage stage;
    time_t   expected_close_date;
    time_t   actual_close_date;
    int      owner_user_id;
    bool     is_closed;
    char     competitor[64];
    char     next_step[128];
} MENT_Opportunity;

typedef struct {
    int      id;
    int      related_type;      /* 0=lead, 1=opp, 2=account */
    int      related_id;
    time_t   due_date;
    time_t   completed_date;
    char     subject[128];
    char     description[512];
    int      assigned_to;
    bool     is_completed;
} MENT_Activity;

typedef struct {
    MENT_Lead         leads[MENT_CRM_MAX_LEADS];
    int               num_leads;
    MENT_Opportunity  opportunities[MENT_CRM_MAX_OPPORTUNITIES];
    int               num_opportunities;
    MENT_CRMAccount      accounts[MENT_CRM_MAX_ACCOUNTS];
    int               num_accounts;
    MENT_Contact      contacts[MENT_CRM_MAX_CONTACTS];
    int               num_contacts;
    MENT_Activity     *activities;
    int               num_activities;
    int               next_id;
} MENT_CRMInstance;

/* ---- API ---- */

MENT_CRMInstance* ment_crm_create(void);
void ment_crm_destroy(MENT_CRMInstance *crm);

/** Account & Contact */
int  ment_crm_add_account(MENT_CRMInstance *crm, const char *name,
                           const char *industry);
int  ment_crm_add_contact(MENT_CRMInstance *crm, int account_id,
                           const char *first, const char *last,
                           const char *email, bool gdpr);

/** Lead Management */
int  ment_crm_add_lead(MENT_CRMInstance *crm, const char *company,
                        const char *contact, const char *email,
                        const char *source);

/**
 * Lead Scoring (L5):
 * Score based on: company size, industry match, email domain,
 * website visits, email engagement.
 * Score = w1*company_size + w2*industry_match + w3*engagement
 */
int  ment_crm_score_lead(MENT_CRMInstance *crm, int lead_id);

/** Convert qualified lead to opportunity+account+contact */
int  ment_crm_convert_lead(MENT_CRMInstance *crm, int lead_id);

/** Opportunity Management */
int  ment_crm_add_opportunity(MENT_CRMInstance *crm, const char *name,
                               int account_id, float amount,
                               time_t close_date);
int  ment_crm_update_stage(MENT_CRMInstance *crm, int opp_id,
                            MENT_OpportunityStage stage);

/**
 * Sales Pipeline Analysis
 * Compute weighted pipeline value: Σ amount * probability
 */
float ment_crm_pipeline_value(const MENT_CRMInstance *crm);

/** Pipeline stage distribution */
int  ment_crm_pipeline_stages(const MENT_CRMInstance *crm,
                               int *counts, MENT_OpportunityStage *stages,
                               int max_stages);

/** Win rate by owner */
float ment_crm_win_rate(const MENT_CRMInstance *crm, int owner_id);

/**
 * RFM Analysis (L5: Recency, Frequency, Monetary):
 * R = days since last deal
 * F = number of deals in period
 * M = total deal value
 * Score = percentile(R) + percentile(F) + percentile(M)
 */
int  ment_crm_rfm_score(const MENT_CRMInstance *crm, int account_id,
                         float *r, float *f, float *m);

/** Forecast pipeline for period */
float ment_crm_forecast(const MENT_CRMInstance *crm,
                         time_t period_start, time_t period_end);

/** Activity management */
int  ment_crm_add_activity(MENT_CRMInstance *crm, int related_type,
                            int related_id, const char *subject,
                            time_t due_date);

/** Get overdue activities */
int  ment_crm_overdue_activities(const MENT_CRMInstance *crm,
                                  int *activity_ids, int max_results);

#endif /* MINI_ENT_CRM_PIPELINE_H */
