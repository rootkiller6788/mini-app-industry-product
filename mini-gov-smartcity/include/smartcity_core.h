#ifndef MINI_SMARTCITY_CORE_H
#define MINI_SMARTCITY_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* ================================================================
 *  Smart City Core -- Urban Governance Foundation
 *
 *  L1: Core definitions -- Citizen, ServiceRequest, Department, Event
 *  L2: Core concepts -- Citizen-centric governance, municipal admin hierarchy
 *  L3: Engineering structures -- Event-driven service bus, priority queue
 *       for request dispatch, department workflow state machine
 *  L4: Standards -- ISO 37120 (Sustainable cities indicators),
 *       UN SDG 11 (Sustainable Cities), GDPR data protection,
 *       Open311 API standard
 *  L5: Algorithms -- Service request routing with priority preemption,
 *       citizen sentiment analysis, department workload balancing,
 *       auto-triage rule engine
 *  L6: Canonical problems -- Citizen 311 portal, government workflow engine,
 *       municipal service delivery pipeline
 *  L7: Applications -- Smart city dashboard, public service analytics
 *  L8: Advanced -- Digital twin governance, AI-assisted policy simulation
 *  L9: Industry -- Sidewalk Labs, Singapore Smart Nation, Barcelona Smart City,
 *       Hangzhou City Brain, Dubai Smart City
 *
 *  References:
 *  - ISO 37120:2018 Indicators for city services and quality of life
 *  - UN SDG 11 -- Sustainable Cities and Communities
 *  - Open311 GeoReport v2 Specification
 * ================================================================ */

#define SMARTCITY_MAX_CITIZENS        65536
#define SMARTCITY_MAX_DEPARTMENTS     128
#define SMARTCITY_MAX_REQUESTS        131072
#define SMARTCITY_MAX_EVENTS          65536
#define SMARTCITY_MAX_WORKFLOW_STEPS  32
#define SMARTCITY_CITIZEN_ID_LEN      32
#define SMARTCITY_NAME_LEN            128
#define SMARTCITY_ADDR_LEN            256
#define SMARTCITY_DESC_LEN            512
#define SMARTCITY_DEPT_CODE_LEN       16
#define SMARTCITY_PHONE_LEN           24
#define SMARTCITY_EMAIL_LEN           128

/* ---- L1: Service Request Categories (Open311 aligned) ---- */
typedef enum {
    SC_SVC_ROAD_POTHOLES         = 0,
    SC_SVC_STREET_LIGHTING       = 1,
    SC_SVC_GARBAGE_COLLECTION    = 2,
    SC_SVC_NOISE_COMPLAINT       = 3,
    SC_SVC_WATER_LEAK            = 4,
    SC_SVC_PARKING_VIOLATION     = 5,
    SC_SVC_PUBLIC_SAFETY         = 6,
    SC_SVC_BUILDING_PERMIT       = 7,
    SC_SVC_BUSINESS_LICENSE      = 8,
    SC_SVC_MARRIAGE_REGISTRATION = 9,
    SC_SVC_PROPERTY_TAX          = 10,
    SC_SVC_PUBLIC_TRANSPORT      = 11,
    SC_SVC_PARK_MAINTENANCE      = 12,
    SC_SVC_SCHOOL_ENROLLMENT     = 13,
    SC_SVC_HEALTH_INSPECTION     = 14,
    SC_SVC_ANIMAL_CONTROL        = 15,
    SC_SVC_GRAFFITI_REMOVAL      = 16,
    SC_SVC_TRAFFIC_SIGNAL        = 17,
    SC_SVC_FLOOD_DRAINAGE        = 18,
    SC_SVC_EMERGENCY_HOUSING     = 19,
    SC_SVC_SENIOR_SERVICES       = 20,
    SC_SVC_VOTER_REGISTRATION    = 21,
    SC_SVC_COUNT                 = 22
} SC_ServiceCategory;

/* ---- L1: Request Status (Workflow State Machine) ---- */
typedef enum {
    SC_STATUS_SUBMITTED      = 0,
    SC_STATUS_ACKNOWLEDGED   = 1,
    SC_STATUS_TRIAGED        = 2,
    SC_STATUS_ASSIGNED       = 3,
    SC_STATUS_IN_PROGRESS    = 4,
    SC_STATUS_PENDING_INFO   = 5,
    SC_STATUS_RESOLVED       = 6,
    SC_STATUS_CLOSED         = 7,
    SC_STATUS_REOPENED       = 8,
    SC_STATUS_ESCALATED      = 9,
    SC_STATUS_CANCELLED      = 10
} SC_RequestStatus;

/* ---- L1: Department Type ---- */
typedef enum {
    SC_DEPT_TRANSPORTATION   = 0,
    SC_DEPT_PUBLIC_WORKS     = 1,
    SC_DEPT_SANITATION       = 2,
    SC_DEPT_PUBLIC_SAFETY    = 3,
    SC_DEPT_WATER_UTILITY    = 4,
    SC_DEPT_HEALTH           = 5,
    SC_DEPT_EDUCATION        = 6,
    SC_DEPT_HOUSING          = 7,
    SC_DEPT_PARKS_REC        = 8,
    SC_DEPT_PLANNING_ZONING  = 9,
    SC_DEPT_FINANCE_TAX      = 10,
    SC_DEPT_HUMAN_SERVICES   = 11,
    SC_DEPT_INFORMATION_TECH = 12,
    SC_DEPT_ENERGY           = 13,
    SC_DEPT_EMERGENCY_MGMT   = 14,
    SC_DEPT_COUNT            = 15
} SC_DepartmentType;

/* ---- L1: Priority Levels ---- */
typedef enum {
    SC_PRIORITY_LOW          = 0,
    SC_PRIORITY_MEDIUM       = 1,
    SC_PRIORITY_HIGH         = 2,
    SC_PRIORITY_CRITICAL     = 3
} SC_Priority;

/* ---- L1: Citizen Record ---- */
typedef struct {
    char     citizen_id[SMARTCITY_CITIZEN_ID_LEN];
    char     full_name[SMARTCITY_NAME_LEN];
    char     address[SMARTCITY_ADDR_LEN];
    char     phone[SMARTCITY_PHONE_LEN];
    char     email[SMARTCITY_EMAIL_LEN];
    time_t   date_of_birth;
    int      household_size;
    bool     is_registered_voter;
    bool     is_senior;
    bool     has_disability;
    bool     is_veteran;
    double   longitude;
    double   latitude;
    int      district_id;
    int      ward_number;
    time_t   registered_at;
    int      open_requests;
    int      total_requests;
    float    satisfaction_score;
} SC_Citizen;

/* ---- L1: Service Request ---- */
typedef struct {
    int64_t  request_id;
    char     citizen_id[SMARTCITY_CITIZEN_ID_LEN];
    SC_ServiceCategory category;
    SC_RequestStatus status;
    SC_Priority priority;
    int      assigned_department;
    int      assigned_officer;
    char     description[SMARTCITY_DESC_LEN];
    char     location_address[SMARTCITY_ADDR_LEN];
    double   longitude;
    double   latitude;
    int      district_id;
    int      ward_number;
    time_t   created_at;
    time_t   acknowledged_at;
    time_t   assigned_at;
    time_t   resolved_at;
    time_t   closed_at;
    time_t   sla_deadline;
    float    estimated_cost;
    float    actual_cost;
    int      escalations;
    bool     is_emergency;
    char     resolution_note[SMARTCITY_DESC_LEN];
} SC_ServiceRequest;

/* ---- L1: Department ---- */
typedef struct {
    char     dept_code[SMARTCITY_DEPT_CODE_LEN];
    char     dept_name[SMARTCITY_NAME_LEN];
    SC_DepartmentType type;
    int      director_citizen_idx;
    int      num_officers;
    int      active_requests;
    int      total_resolved;
    float    avg_resolution_hours;
    float    budget_allocated;
    float    budget_spent;
    float    satisfaction_rating;
} SC_Department;
/* ---- L1: City Event ---- */
typedef struct {
    int64_t  event_id;
    char     title[SMARTCITY_NAME_LEN];
    char     description[SMARTCITY_DESC_LEN];
    time_t   start_time;
    time_t   end_time;
    char     venue[SMARTCITY_ADDR_LEN];
    double   longitude;
    double   latitude;
    SC_DepartmentType organizer;
    int      expected_attendance;
    int      actual_attendance;
    bool     requires_permit;
    bool     is_public;
    bool     is_cancelled;
} SC_CityEvent;

/* ---- L1: Government Dashboard KPI ---- */
typedef struct {
    int      total_citizens;
    int      active_requests;
    int      overdue_requests;
    int      resolved_today;
    int      total_events_today;
    float    avg_satisfaction;
    float    avg_resolution_time_hours;
    float    budget_utilization_pct;
    int      departments_under_sla;
    int      departments_over_sla;
} SC_DashboardKPI;

/* ---- L1: Workflow Transition ---- */
typedef struct {
    SC_RequestStatus from_status;
    SC_RequestStatus to_status;
    int      required_role;
    int      max_time_hours;
} SC_WorkflowTransition;

/* ---- API: Smart City Core System ---- */

typedef struct SC_CitySystem SC_CitySystem;
SC_CitySystem* sc_city_create(const char *city_name, int district_count);
void           sc_city_destroy(SC_CitySystem *city);

/* Citizen Management (L2) */
int  sc_citizen_register(SC_CitySystem *city, const SC_Citizen *citizen);
SC_Citizen* sc_citizen_lookup(SC_CitySystem *city, const char *citizen_id);
int  sc_citizen_update(SC_CitySystem *city, const char *citizen_id, const SC_Citizen *updated);
int  sc_citizen_count(const SC_CitySystem *city);
int  sc_citizen_list_by_district(const SC_CitySystem *city, int district_id, int *results, int max_results);

/* Service Request Management (L2) */
int64_t sc_request_submit(SC_CitySystem *city, SC_ServiceRequest *request);
SC_ServiceRequest* sc_request_lookup(SC_CitySystem *city, int64_t request_id);
int  sc_request_update_status(SC_CitySystem *city, int64_t request_id, SC_RequestStatus new_status);
int  sc_request_assign(SC_CitySystem *city, int64_t request_id, int department, int officer);
int  sc_request_escalate(SC_CitySystem *city, int64_t request_id);
int  sc_request_list_by_citizen(SC_CitySystem *city, const char *citizen_id, int64_t *results, int max_results);
int  sc_request_list_by_status(SC_CitySystem *city, SC_RequestStatus status, int64_t *results, int max_results);
int  sc_request_count_by_category(const SC_CitySystem *city, SC_ServiceCategory cat);

/* Department Operations (L2) */
int  sc_department_register(SC_CitySystem *city, const SC_Department *dept);
SC_Department* sc_department_lookup(SC_CitySystem *city, SC_DepartmentType type);
float sc_department_workload_ratio(const SC_CitySystem *city, SC_DepartmentType type);

/* Workflow Engine (L3) */
bool sc_workflow_is_valid_transition(SC_RequestStatus from, SC_RequestStatus to);
int  sc_workflow_get_sla_hours(SC_RequestStatus from, SC_RequestStatus to);
int  sc_workflow_get_transitions(SC_WorkflowTransition *transitions, int max);
const char* sc_service_category_name(SC_ServiceCategory cat);
const char* sc_status_name(SC_RequestStatus status);

/* Priority Queue (L3) */
typedef struct SC_PriorityQueue SC_PriorityQueue;
SC_PriorityQueue* sc_pq_create(int capacity);
void   sc_pq_destroy(SC_PriorityQueue *pq);
int    sc_pq_push(SC_PriorityQueue *pq, int64_t request_id, SC_Priority prio, time_t deadline);
int64_t sc_pq_pop(SC_PriorityQueue *pq);
int    sc_pq_size(const SC_PriorityQueue *pq);
bool   sc_pq_is_empty(const SC_PriorityQueue *pq);

/* KPI Dashboard - ISO 37120 aligned (L4) */
SC_DashboardKPI sc_dashboard_compute(const SC_CitySystem *city);
float sc_city_gini_coefficient(const SC_CitySystem *city, const float *income_data, int count);
float sc_city_service_equity_index(const SC_CitySystem *city);

/* Sentiment Analysis (L5) */
float sc_sentiment_analyze(const char *feedback_text);
int   sc_sentiment_summarize_by_district(const SC_CitySystem *city, int district_id);

/* Workload Balancing (L5) */
int  sc_balance_workload(SC_CitySystem *city);

/* Auto-Triage (L5) */
int  sc_auto_triage(SC_CitySystem *city, int64_t request_id);

/* City Events (L6) */
int  sc_event_schedule(SC_CitySystem *city, const SC_CityEvent *event);
int  sc_event_cancel(SC_CitySystem *city, int64_t event_id);
SC_CityEvent* sc_event_lookup(SC_CitySystem *city, int64_t event_id);
int  sc_event_list_upcoming(SC_CitySystem *city, SC_CityEvent **events, int max);

/* Citizen Portal Analytics (L7) */
float sc_citizen_satisfaction_avg(const SC_CitySystem *city);
int   sc_top_complaint_categories(const SC_CitySystem *city, SC_ServiceCategory *top, int n);

/* Digital Twin Snapshot (L8) */
int  sc_digital_twin_snapshot(const SC_CitySystem *city, char *json_buf, int buf_size);

#endif /* MINI_SMARTCITY_CORE_H */
