/* ================================================================
 *  Smart City Core Implementation (L1-L8)
 *  Citizen mgmt, service requests, workflow, priority queue,
 *  KPI dashboard, sentiment analysis, auto-triage, events.
 * ================================================================ */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "smartcity_core.h"

struct SC_CitySystem {
    char city_name[SMARTCITY_NAME_LEN];
    int district_count, next_request_id, next_event_id;
    SC_Citizen *citizens;
    int citizen_count, citizen_capacity;
    SC_ServiceRequest *requests;
    int request_count, request_capacity;
    SC_Department departments[SC_DEPT_COUNT];
    SC_CityEvent *events;
    int event_count, event_capacity;
    SC_WorkflowTransition transitions[SMARTCITY_MAX_WORKFLOW_STEPS];
    int transition_count;
    SC_PriorityQueue *dispatch_queue;
};

static void sc_strncpy_safe(char *dst, const char *src, int max_len) {
    if (!dst || !src) return;
    int i;
    for (i = 0; i < max_len - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

SC_CitySystem* sc_city_create(const char *city_name, int district_count) {
    SC_CitySystem *city = (SC_CitySystem*)calloc(1, sizeof(SC_CitySystem));
    if (!city) return NULL;
    sc_strncpy_safe(city->city_name, city_name, SMARTCITY_NAME_LEN);
    city->district_count = district_count;
    city->next_request_id = 1000;
    city->next_event_id = 1;
    city->citizen_capacity = 1024;
    city->citizens = (SC_Citizen*)calloc(city->citizen_capacity, sizeof(SC_Citizen));
    if (!city->citizens) { free(city); return NULL; }
    city->request_capacity = 2048;
    city->requests = (SC_ServiceRequest*)calloc(city->request_capacity, sizeof(SC_ServiceRequest));
    if (!city->requests) { free(city->citizens); free(city); return NULL; }
    city->event_capacity = 512;
    city->events = (SC_CityEvent*)calloc(city->event_capacity, sizeof(SC_CityEvent));
    if (!city->events) { free(city->requests); free(city->citizens); free(city); return NULL; }
    const char *dnames[] = {"Transportation","Public Works","Sanitation","Public Safety","Water Utility","Health","Education","Housing","Parks & Recreation","Planning & Zoning","Finance & Tax","Human Services","Information Technology","Energy","Emergency Management"};
    for (int i = 0; i < SC_DEPT_COUNT; i++) {
        city->departments[i].type = (SC_DepartmentType)i;
        sc_strncpy_safe(city->departments[i].dept_name, dnames[i], SMARTCITY_NAME_LEN);
        sprintf(city->departments[i].dept_code, "DEPT-%02d", i);
        city->departments[i].num_officers = 10 + (i * 5);
        city->departments[i].budget_allocated = 1000000.0f + (i * 250000.0f);
    }
    city->transition_count = sc_workflow_get_transitions(city->transitions, SMARTCITY_MAX_WORKFLOW_STEPS);
    city->dispatch_queue = sc_pq_create(1024);
    return city;
}

void sc_city_destroy(SC_CitySystem *city) {
    if (!city) return;
    if (city->dispatch_queue) sc_pq_destroy(city->dispatch_queue);
    free(city->citizens); free(city->requests); free(city->events);
    free(city);
}

/* ---- L2: Citizen Management ---- */
int sc_citizen_register(SC_CitySystem *city, const SC_Citizen *citizen) {
    if (!city || !citizen) return -1;
    if (city->citizen_count >= city->citizen_capacity) {
        int nc = city->citizen_capacity * 2;
        SC_Citizen *tmp = (SC_Citizen*)realloc(city->citizens, nc * sizeof(SC_Citizen));
        if (!tmp) return -1;
        city->citizens = tmp; city->citizen_capacity = nc;
    }
    int idx = city->citizen_count;
    memcpy(&city->citizens[idx], citizen, sizeof(SC_Citizen));
    city->citizens[idx].registered_at = time(NULL);
    city->citizens[idx].open_requests = 0;
    city->citizens[idx].total_requests = 0;
    city->citizen_count++;
    return idx;
}

SC_Citizen* sc_citizen_lookup(SC_CitySystem *city, const char *citizen_id) {
    if (!city || !citizen_id) return NULL;
    for (int i = 0; i < city->citizen_count; i++)
        if (strcmp(city->citizens[i].citizen_id, citizen_id) == 0)
            return &city->citizens[i];
    return NULL;
}

int sc_citizen_update(SC_CitySystem *city, const char *citizen_id, const SC_Citizen *updated) {
    SC_Citizen *c = sc_citizen_lookup(city, citizen_id);
    if (!c || !updated) return -1;
    time_t rt = c->registered_at; int oq = c->open_requests, tq = c->total_requests;
    memcpy(c, updated, sizeof(SC_Citizen));
    c->registered_at = rt; c->open_requests = oq; c->total_requests = tq;
    return 0;
}

int sc_citizen_count(const SC_CitySystem *city) { return city ? city->citizen_count : 0; }

int sc_citizen_list_by_district(const SC_CitySystem *city, int district_id, int *results, int max_results) {
    if (!city || !results) return 0;
    int count = 0;
    for (int i = 0; i < city->citizen_count && count < max_results; i++)
        if (city->citizens[i].district_id == district_id) results[count++] = i;
    return count;
}

/* ---- L2: Service Request Management ---- */
int64_t sc_request_submit(SC_CitySystem *city, SC_ServiceRequest *request) {
    if (!city || !request) return -1;
    if (city->request_count >= city->request_capacity) {
        int nc = city->request_capacity * 2;
        SC_ServiceRequest *tmp = (SC_ServiceRequest*)realloc(city->requests, nc * sizeof(SC_ServiceRequest));
        if (!tmp) return -1;
        city->requests = tmp; city->request_capacity = nc;
    }
    int idx = city->request_count;
    memcpy(&city->requests[idx], request, sizeof(SC_ServiceRequest));
    city->requests[idx].request_id = city->next_request_id++;
    city->requests[idx].status = SC_STATUS_SUBMITTED;
    city->requests[idx].created_at = time(NULL);
    city->requests[idx].assigned_department = -1;
    city->requests[idx].assigned_officer = -1;
    city->requests[idx].escalations = 0;
    time_t sla = request->is_emergency ? (4 * 3600) : (72 * 3600);
    city->requests[idx].sla_deadline = time(NULL) + sla;
    city->request_count++;
    SC_Citizen *cz = sc_citizen_lookup(city, request->citizen_id);
    if (cz) { cz->open_requests++; cz->total_requests++; }
    return city->requests[idx].request_id;
}

SC_ServiceRequest* sc_request_lookup(SC_CitySystem *city, int64_t request_id) {
    if (!city) return NULL;
    for (int i = 0; i < city->request_count; i++)
        if (city->requests[i].request_id == request_id) return &city->requests[i];
    return NULL;
}

int sc_request_update_status(SC_CitySystem *city, int64_t request_id, SC_RequestStatus new_status) {
    SC_ServiceRequest *req = sc_request_lookup(city, request_id);
    if (!req) return -1;
    if (!sc_workflow_is_valid_transition(req->status, new_status)) return -2;
    req->status = new_status;
    time_t now = time(NULL);
    switch (new_status) {
        case SC_STATUS_ACKNOWLEDGED: req->acknowledged_at = now; break;
        case SC_STATUS_ASSIGNED: req->assigned_at = now; break;
        case SC_STATUS_RESOLVED: req->resolved_at = now; break;
        case SC_STATUS_CLOSED: req->closed_at = now; break;
        default: break;
    }
    if (new_status == SC_STATUS_CLOSED || new_status == SC_STATUS_CANCELLED) {
        SC_Citizen *cz = sc_citizen_lookup(city, req->citizen_id);
        if (cz && cz->open_requests > 0) cz->open_requests--;
    }
    return 0;
}

int sc_request_assign(SC_CitySystem *city, int64_t request_id, int department, int officer) {
    SC_ServiceRequest *req = sc_request_lookup(city, request_id);
    if (!req) return -1;
    req->assigned_department = department;
    req->assigned_officer = officer;
    if (department >= 0 && department < SC_DEPT_COUNT)
        city->departments[department].active_requests++;
    return sc_request_update_status(city, request_id, SC_STATUS_ASSIGNED);
}

int sc_request_escalate(SC_CitySystem *city, int64_t request_id) {
    SC_ServiceRequest *req = sc_request_lookup(city, request_id);
    if (!req) return -1;
    req->escalations++;
    if (req->priority < SC_PRIORITY_CRITICAL) req->priority = (SC_Priority)(req->priority + 1);
    return sc_request_update_status(city, request_id, SC_STATUS_ESCALATED);
}

int sc_request_list_by_citizen(SC_CitySystem *city, const char *citizen_id, int64_t *results, int max_results) {
    if (!city || !citizen_id || !results) return 0;
    int count = 0;
    for (int i = 0; i < city->request_count && count < max_results; i++)
        if (strcmp(city->requests[i].citizen_id, citizen_id) == 0)
            results[count++] = city->requests[i].request_id;
    return count;
}

int sc_request_list_by_status(SC_CitySystem *city, SC_RequestStatus status, int64_t *results, int max_results) {
    if (!city || !results) return 0;
    int count = 0;
    for (int i = 0; i < city->request_count && count < max_results; i++)
        if (city->requests[i].status == status) results[count++] = city->requests[i].request_id;
    return count;
}

int sc_request_count_by_category(const SC_CitySystem *city, SC_ServiceCategory cat) {
    if (!city) return 0;
    int count = 0;
    for (int i = 0; i < city->request_count; i++)
        if (city->requests[i].category == cat) count++;
    return count;
}

/* ---- L2: Department Operations ---- */
int sc_department_register(SC_CitySystem *city, const SC_Department *dept) {
    if (!city || !dept) return -1;
    if (dept->type < 0 || dept->type >= SC_DEPT_COUNT) return -2;
    memcpy(&city->departments[dept->type], dept, sizeof(SC_Department));
    return 0;
}

SC_Department* sc_department_lookup(SC_CitySystem *city, SC_DepartmentType type) {
    if (!city || type < 0 || type >= SC_DEPT_COUNT) return NULL;
    return &city->departments[type];
}

float sc_department_workload_ratio(const SC_CitySystem *city, SC_DepartmentType type) {
    if (!city || type < 0 || type >= SC_DEPT_COUNT) return -1.0f;
    const SC_Department *d = &city->departments[type];
    if (d->num_officers <= 0) return -1.0f;
    return (float)d->active_requests / (float)d->num_officers;
}

/* ================================================================
 *  L3: Workflow Engine - State Machine with SLA tracking
 *
 *  Defines all valid transitions for the service request lifecycle.
 *  This is a finite state machine with 11 states and 14 defined edges.
 *  SLA (Service Level Agreement) hours track maximum allowed time
 *  per transition step, enabling performance monitoring.
 *
 *  Reference: Business Process Model and Notation (BPMN 2.0)
 * ================================================================ */

bool sc_workflow_is_valid_transition(SC_RequestStatus from, SC_RequestStatus to) {
    if (from == to) return true;
    if (from == SC_STATUS_CANCELLED || from == SC_STATUS_CLOSED)
        return to == SC_STATUS_REOPENED;
    if (to == SC_STATUS_CANCELLED && from != SC_STATUS_RESOLVED) return true;
    switch (from) {
        case SC_STATUS_SUBMITTED: return to == SC_STATUS_ACKNOWLEDGED;
        case SC_STATUS_ACKNOWLEDGED: return to == SC_STATUS_TRIAGED;
        case SC_STATUS_TRIAGED:
            return to == SC_STATUS_ASSIGNED || to == SC_STATUS_PENDING_INFO;
        case SC_STATUS_ASSIGNED: return to == SC_STATUS_IN_PROGRESS;
        case SC_STATUS_IN_PROGRESS:
            return to == SC_STATUS_RESOLVED || to == SC_STATUS_PENDING_INFO
                || to == SC_STATUS_ESCALATED;
        case SC_STATUS_PENDING_INFO:
            return to == SC_STATUS_IN_PROGRESS || to == SC_STATUS_ASSIGNED;
        case SC_STATUS_RESOLVED:
            return to == SC_STATUS_CLOSED || to == SC_STATUS_REOPENED;
        case SC_STATUS_REOPENED:
            return to == SC_STATUS_TRIAGED || to == SC_STATUS_IN_PROGRESS;
        case SC_STATUS_ESCALATED:
            return to == SC_STATUS_ASSIGNED || to == SC_STATUS_IN_PROGRESS;
        default: return false;
    }
}

int sc_workflow_get_sla_hours(SC_RequestStatus from, SC_RequestStatus to) {
    if (from == SC_STATUS_SUBMITTED    && to == SC_STATUS_ACKNOWLEDGED) return 4;
    if (from == SC_STATUS_ACKNOWLEDGED && to == SC_STATUS_TRIAGED)      return 8;
    if (from == SC_STATUS_TRIAGED      && to == SC_STATUS_ASSIGNED)     return 24;
    if (from == SC_STATUS_ASSIGNED     && to == SC_STATUS_IN_PROGRESS)  return 48;
    if (from == SC_STATUS_IN_PROGRESS  && to == SC_STATUS_RESOLVED)     return 120;
    if (from == SC_STATUS_RESOLVED     && to == SC_STATUS_CLOSED)       return 72;
    return -1;
}

int sc_workflow_get_transitions(SC_WorkflowTransition *transitions, int max) {
    if (!transitions) return 0;
    struct { SC_RequestStatus from; SC_RequestStatus to; } pairs[] = {
        {SC_STATUS_SUBMITTED,    SC_STATUS_ACKNOWLEDGED},
        {SC_STATUS_SUBMITTED,    SC_STATUS_CANCELLED},
        {SC_STATUS_ACKNOWLEDGED, SC_STATUS_TRIAGED},
        {SC_STATUS_ACKNOWLEDGED, SC_STATUS_CANCELLED},
        {SC_STATUS_TRIAGED,      SC_STATUS_ASSIGNED},
        {SC_STATUS_TRIAGED,      SC_STATUS_CANCELLED},
        {SC_STATUS_ASSIGNED,     SC_STATUS_IN_PROGRESS},
        {SC_STATUS_IN_PROGRESS,  SC_STATUS_RESOLVED},
        {SC_STATUS_IN_PROGRESS,  SC_STATUS_PENDING_INFO},
        {SC_STATUS_IN_PROGRESS,  SC_STATUS_ESCALATED},
        {SC_STATUS_RESOLVED,     SC_STATUS_CLOSED},
        {SC_STATUS_RESOLVED,     SC_STATUS_REOPENED},
        {SC_STATUS_CLOSED,       SC_STATUS_REOPENED},
        {SC_STATUS_REOPENED,     SC_STATUS_TRIAGED},
    };
    int n = (int)(sizeof(pairs) / sizeof(pairs[0]));
    if (n > max) n = max;
    for (int i = 0; i < n; i++) {
        transitions[i].from_status = pairs[i].from;
        transitions[i].to_status   = pairs[i].to;
        transitions[i].required_role = 1;
        transitions[i].max_time_hours =
            sc_workflow_get_sla_hours(pairs[i].from, pairs[i].to);
    }
    return n;
}

const char* sc_service_category_name(SC_ServiceCategory cat) {
    static const char *n[] = {"Road/Potholes","Street Lighting","Garbage","Noise","Water Leak","Parking","Public Safety","Building Permit","Business License","Marriage","Property Tax","Public Transport","Park Maint.","School Enrollment","Health Insp.","Animal Control","Graffiti","Traffic Signal","Flood Drainage","Emerg. Housing","Senior Services","Voter Reg."};
    return (cat >= 0 && cat < SC_SVC_COUNT) ? n[cat] : "Unknown";
}

const char* sc_status_name(SC_RequestStatus status) {
    static const char *n[] = {"Submitted","Acknowledged","Triaged","Assigned","In Progress","Pending Info","Resolved","Closed","Reopened","Escalated","Cancelled"};
    return (status >= 0 && status <= SC_STATUS_CANCELLED) ? n[status] : "Unknown";
}

/* ================================================================
 *  L3: Priority Queue - Binary Max-Heap
 *
 *  Used for dispatch ordering. Higher priority and earlier deadline
 *  items are dequeued first. O(log n) push and pop.
 *  This implements a standard binary heap data structure.
 * ================================================================ */

typedef struct {
    int64_t request_id;
    SC_Priority priority;
    time_t deadline;
} PQEntry;

struct SC_PriorityQueue {
    PQEntry *heap;
    int size;
    int capacity;
};

SC_PriorityQueue* sc_pq_create(int capacity) {
    SC_PriorityQueue *pq = (SC_PriorityQueue*)malloc(sizeof(SC_PriorityQueue));
    if (!pq) return NULL;
    pq->heap = (PQEntry*)calloc((size_t)capacity, sizeof(PQEntry));
    if (!pq->heap) { free(pq); return NULL; }
    pq->size = 0;
    pq->capacity = capacity;
    return pq;
}

void sc_pq_destroy(SC_PriorityQueue *pq) {
    if (!pq) return;
    free(pq->heap);
    free(pq);
}

static void pq_swap(PQEntry *a, PQEntry *b) {
    PQEntry t = *a; *a = *b; *b = t;
}

static int pq_compare(const PQEntry *a, const PQEntry *b) {
    if (a->priority != b->priority)
        return (a->priority > b->priority) ? 1 : -1;
    if (a->deadline != b->deadline)
        return (a->deadline < b->deadline) ? 1 : -1;
    return 0;
}

int sc_pq_push(SC_PriorityQueue *pq, int64_t request_id, SC_Priority prio, time_t deadline) {
    if (!pq || pq->size >= pq->capacity) return -1;
    int i = pq->size++;
    pq->heap[i].request_id = request_id;
    pq->heap[i].priority = prio;
    pq->heap[i].deadline = deadline;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (pq_compare(&pq->heap[i], &pq->heap[p]) <= 0) break;
        pq_swap(&pq->heap[i], &pq->heap[p]);
        i = p;
    }
    return 0;
}

int64_t sc_pq_pop(SC_PriorityQueue *pq) {
    if (!pq || pq->size == 0) return -1;
    int64_t result = pq->heap[0].request_id;
    pq->heap[0] = pq->heap[--pq->size];
    int i = 0;
    while (1) {
        int largest = i;
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        if (left < pq->size && pq_compare(&pq->heap[left], &pq->heap[largest]) > 0)
            largest = left;
        if (right < pq->size && pq_compare(&pq->heap[right], &pq->heap[largest]) > 0)
            largest = right;
        if (largest == i) break;
        pq_swap(&pq->heap[i], &pq->heap[largest]);
        i = largest;
    }
    return result;
}

int sc_pq_size(const SC_PriorityQueue *pq) { return pq ? pq->size : 0; }
bool sc_pq_is_empty(const SC_PriorityQueue *pq) { return pq ? (pq->size == 0) : true; }

/* ================================================================
 *  L4: KPI Dashboard (ISO 37120 aligned)
 *
 *  Computes city governance performance indicators:
 *  - Total active/overdue requests
 *  - Average resolution time
 *  - Budget utilization across departments
 *  - Average citizen satisfaction
 *  - Gini coefficient for income inequality
 *  - Service equity index across districts
 *
 *  Standard: ISO 37120:2018 - Sustainable cities and communities
 *  Indicators for city services and quality of life.
 * ================================================================ */

SC_DashboardKPI sc_dashboard_compute(const SC_CitySystem *city) {
    SC_DashboardKPI kpi;
    memset(&kpi, 0, sizeof(kpi));
    if (!city) return kpi;

    kpi.total_citizens = city->citizen_count;
    time_t now = time(NULL);

    for (int i = 0; i < city->request_count; i++) {
        const SC_ServiceRequest *r = &city->requests[i];
        if (r->status != SC_STATUS_CLOSED && r->status != SC_STATUS_CANCELLED) {
            kpi.active_requests++;
            if (r->sla_deadline < now) kpi.overdue_requests++;
        }
        if (r->status == SC_STATUS_RESOLVED || r->status == SC_STATUS_CLOSED) {
            double hours = difftime(r->resolved_at ? r->resolved_at : r->closed_at,
                                    r->created_at) / 3600.0;
            kpi.avg_resolution_time_hours += (float)hours;
        }
    }

    int resolved = city->request_count - kpi.active_requests;
    if (resolved > 0)
        kpi.avg_resolution_time_hours /= (float)resolved;

    /* Count today's resolutions */
    time_t day_ago = now - 86400;
    for (int i = 0; i < city->request_count; i++) {
        if (city->requests[i].resolved_at > day_ago)
            kpi.resolved_today++;
    }

    /* Average citizen satisfaction */
    float sat_sum = 0.0f;
    int sat_count = 0;
    for (int i = 0; i < city->citizen_count; i++) {
        if (city->citizens[i].satisfaction_score > 0.0f) {
            sat_sum += city->citizens[i].satisfaction_score;
            sat_count++;
        }
    }
    kpi.avg_satisfaction = sat_count > 0 ? sat_sum / (float)sat_count : 0.0f;

    /* Budget utilization */
    float total_budget = 0.0f, total_spent = 0.0f;
    for (int i = 0; i < SC_DEPT_COUNT; i++) {
        total_budget += city->departments[i].budget_allocated;
        total_spent  += city->departments[i].budget_spent;
    }
    kpi.budget_utilization_pct = total_budget > 0.0f
        ? (total_spent / total_budget) * 100.0f : 0.0f;

    /* Today's events count */
    for (int i = 0; i < city->event_count; i++) {
        if (city->events[i].start_time > day_ago &&
            city->events[i].start_time < now + 86400)
            kpi.total_events_today++;
    }

    /* Department SLA compliance */
    kpi.departments_under_sla = 0;
    kpi.departments_over_sla = 0;
    for (int i = 0; i < SC_DEPT_COUNT; i++) {
        float ratio = sc_department_workload_ratio(city, (SC_DepartmentType)i);
        if (ratio >= 0 && ratio <= 5.0f)
            kpi.departments_under_sla++;
        else
            kpi.departments_over_sla++;
    }

    return kpi;
}

float sc_city_gini_coefficient(const SC_CitySystem *city,
                                const float *income_data, int count) {
    (void)city;
    if (!income_data || count < 2) return 0.0f;

    /* Gini = (sum_i sum_j |x_i - x_j|) / (2 * n^2 * mean) */
    double sum_abs_diff = 0.0, sum = 0.0;
    for (int i = 0; i < count; i++) sum += income_data[i];
    if (sum <= 0.0) return 0.0f;
    double mean = sum / count;

    for (int i = 0; i < count; i++)
        for (int j = 0; j < count; j++)
            sum_abs_diff += fabs((double)income_data[i] - (double)income_data[j]);

    return (float)(sum_abs_diff / (2.0 * count * count * mean));
}

float sc_city_service_equity_index(const SC_CitySystem *city) {
    if (!city || city->district_count <= 0) return 1.0f;

    int *req_per_district = (int*)calloc((size_t)city->district_count, sizeof(int));
    if (!req_per_district) return -1.0f;

    for (int i = 0; i < city->request_count; i++) {
        int d = city->requests[i].district_id;
        if (d >= 0 && d < city->district_count)
            req_per_district[d]++;
    }

    double sum = 0.0, sumsq = 0.0;
    for (int i = 0; i < city->district_count; i++) {
        sum += req_per_district[i];
        sumsq += (double)req_per_district[i] * req_per_district[i];
    }
    free(req_per_district);

    double mean = sum / city->district_count;
    if (mean == 0.0) return 1.0f;
    /* Coefficient of Variation, then invert for equity index */
    double variance = sumsq / city->district_count - mean * mean;
    double cv = sqrt(variance) / mean;
    return (float)(1.0 / (1.0 + cv));
}

/* ================================================================
 *  L5: Sentiment Analysis - Keyword-based scoring
 *
 *  Algorithm: Simple bag-of-words sentiment classification using
 *  positive/negative keyword dictionaries. Returns a score 0.0-5.0
 *  where 5.0 is maximally positive and 0.0 is maximally negative.
 *  2.5 is neutral (no keywords found).
 *
 *  This is a simplified lexicon-based approach. Production systems
 *  would use NLP/transformer models (L8/L9).
 * ================================================================ */

float sc_sentiment_analyze(const char *feedback_text) {
    if (!feedback_text || !feedback_text[0]) return 2.5f;

    static const char *positive_words[] = {
        "good", "great", "excellent", "thank", "happy", "satisfied",
        "wonderful", "pleased", "quick", "efficient", "helpful",
        "professional", "resolved", "fixed", "clean", "safe",
        "beautiful", "improved", "appreciate", "fantastic"
    };
    static const char *negative_words[] = {
        "bad", "terrible", "awful", "slow", "rude", "unhappy",
        "disappointed", "broken", "dirty", "dangerous", "ugly",
        "waste", "neglect", "ignored", "unacceptable", "frustrated",
        "angry", "complaint", "failed", "worst"
    };

    int n_pos = (int)(sizeof(positive_words) / sizeof(positive_words[0]));
    int n_neg = (int)(sizeof(negative_words) / sizeof(negative_words[0]));

    /* Lowercase the input */
    char buf[1024];
    strncpy(buf, feedback_text, 1023);
    buf[1023] = '\0';
    for (char *p = buf; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);

    int pos_count = 0, neg_count = 0;
    for (int i = 0; i < n_pos; i++)
        if (strstr(buf, positive_words[i])) pos_count++;
    for (int i = 0; i < n_neg; i++)
        if (strstr(buf, negative_words[i])) neg_count++;

    int total = pos_count + neg_count;
    if (total == 0) return 2.5f;

    /* Score: 0.0 (all negative) to 5.0 (all positive) */
    return 5.0f * (float)pos_count / (float)total;
}

int sc_sentiment_summarize_by_district(const SC_CitySystem *city,
                                        int district_id) {
    if (!city) return 0;
    float sum = 0.0f;
    int count = 0;
    for (int i = 0; i < city->citizen_count; i++) {
        if (city->citizens[i].district_id == district_id &&
            city->citizens[i].satisfaction_score > 0.0f) {
            sum += city->citizens[i].satisfaction_score;
            count++;
        }
    }
    /* Return percentage (0-100) */
    return count > 0 ? (int)((sum / count) * 20.0f) : 50;
}

/* ================================================================
 *  L5: Department Workload Balancing
 *
 *  Algorithm: Heuristic redistribution of active requests from
 *  overloaded departments (workload ratio > 1.5x average) to
 *  underloaded departments (< 0.5x average). This is a greedy
 *  approximation; optimal workload balancing is NP-hard.
 *
 *  Returns number of requests moved.
 * ================================================================ */

int sc_balance_workload(SC_CitySystem *city) {
    if (!city) return -1;
    int moved = 0;
    float ratios[SC_DEPT_COUNT];
    float avg_ratio = 0.0f;

    for (int i = 0; i < SC_DEPT_COUNT; i++) {
        ratios[i] = sc_department_workload_ratio(city, (SC_DepartmentType)i);
        if (ratios[i] >= 0) avg_ratio += ratios[i];
    }
    avg_ratio /= SC_DEPT_COUNT;

    for (int i = 0; i < SC_DEPT_COUNT; i++) {
        if (ratios[i] > avg_ratio * 1.5f && ratios[i] > 0) {
            for (int j = 0; j < SC_DEPT_COUNT; j++) {
                if (ratios[j] >= 0 && ratios[j] < avg_ratio * 0.5f) {
                    city->departments[i].active_requests--;
                    city->departments[j].active_requests++;
                    moved++;
                    ratios[i] = sc_department_workload_ratio(city, (SC_DepartmentType)i);
                    ratios[j] = sc_department_workload_ratio(city, (SC_DepartmentType)j);
                    break;
                }
            }
        }
    }
    return moved;
}

/* ================================================================
 *  L5: Auto-Triage - Rule-based service request routing
 *
 *  Algorithm: Maps each of the 22 service categories to the most
 *  appropriate municipal department. Priority is automatically set
 *  to CRITICAL for emergencies and HIGH for public safety issues.
 *
 *  This is a deterministic rule engine. Future L8/L9 work could
 *  replace this with ML-based classification.
 * ================================================================ */

int sc_auto_triage(SC_CitySystem *city, int64_t request_id) {
    SC_ServiceRequest *req = sc_request_lookup(city, request_id);
    if (!req) return -1;

    switch (req->category) {
        case SC_SVC_ROAD_POTHOLES:
        case SC_SVC_STREET_LIGHTING:
        case SC_SVC_TRAFFIC_SIGNAL:
        case SC_SVC_PUBLIC_TRANSPORT:
            req->assigned_department = SC_DEPT_TRANSPORTATION; break;

        case SC_SVC_GARBAGE_COLLECTION:
        case SC_SVC_GRAFFITI_REMOVAL:
        case SC_SVC_PARK_MAINTENANCE:
            req->assigned_department = SC_DEPT_SANITATION; break;

        case SC_SVC_NOISE_COMPLAINT:
        case SC_SVC_PUBLIC_SAFETY:
        case SC_SVC_ANIMAL_CONTROL:
            req->assigned_department = SC_DEPT_PUBLIC_SAFETY; break;

        case SC_SVC_WATER_LEAK:
        case SC_SVC_FLOOD_DRAINAGE:
            req->assigned_department = SC_DEPT_WATER_UTILITY; break;

        case SC_SVC_BUILDING_PERMIT:
        case SC_SVC_PARKING_VIOLATION:
            req->assigned_department = SC_DEPT_PLANNING_ZONING; break;

        case SC_SVC_PROPERTY_TAX:
        case SC_SVC_BUSINESS_LICENSE:
            req->assigned_department = SC_DEPT_FINANCE_TAX; break;

        case SC_SVC_SCHOOL_ENROLLMENT:
            req->assigned_department = SC_DEPT_EDUCATION; break;

        case SC_SVC_HEALTH_INSPECTION:
            req->assigned_department = SC_DEPT_HEALTH; break;

        case SC_SVC_SENIOR_SERVICES:
        case SC_SVC_EMERGENCY_HOUSING:
            req->assigned_department = SC_DEPT_HUMAN_SERVICES; break;

        case SC_SVC_MARRIAGE_REGISTRATION:
        case SC_SVC_VOTER_REGISTRATION:
            req->assigned_department = SC_DEPT_INFORMATION_TECH; break;

        default:
            req->assigned_department = SC_DEPT_PUBLIC_WORKS; break;
    }

    /* Set priority based on urgency */
    if (req->is_emergency)
        req->priority = SC_PRIORITY_CRITICAL;
    else if (req->category == SC_SVC_PUBLIC_SAFETY ||
             req->category == SC_SVC_FLOOD_DRAINAGE)
        req->priority = SC_PRIORITY_HIGH;

    /* Acknowledge first, then triage (workflow: SUBMITTED->ACKNOWLEDGED->TRIAGED) */
    if (req->status == SC_STATUS_SUBMITTED)
        sc_request_update_status(city, request_id, SC_STATUS_ACKNOWLEDGED);
    return sc_request_update_status(city, request_id, SC_STATUS_TRIAGED);
}

/* ================================================================
 *  L6: City Events Management
 *
 *  Canonical Problem: Municipal event scheduling and management.
 *  Tracks public events, permits, attendance, and cancellations.
 *  Programs: parks & recreation, cultural festivals, public hearings.
 * ================================================================ */

int sc_event_schedule(SC_CitySystem *city, const SC_CityEvent *event) {
    if (!city || !event) return -1;
    if (city->event_count >= city->event_capacity) {
        int nc = city->event_capacity * 2;
        SC_CityEvent *tmp = (SC_CityEvent*)realloc(city->events,
                                                     nc * sizeof(SC_CityEvent));
        if (!tmp) return -1;
        city->events = tmp;
        city->event_capacity = nc;
    }
    int idx = city->event_count;
    memcpy(&city->events[idx], event, sizeof(SC_CityEvent));
    city->events[idx].event_id = city->next_event_id++;
    city->events[idx].is_cancelled = false;
    city->event_count++;
    return city->events[idx].event_id;
}

int sc_event_cancel(SC_CitySystem *city, int64_t event_id) {
    SC_CityEvent *ev = sc_event_lookup(city, event_id);
    if (!ev) return -1;
    ev->is_cancelled = true;
    return 0;
}

SC_CityEvent* sc_event_lookup(SC_CitySystem *city, int64_t event_id) {
    if (!city) return NULL;
    for (int i = 0; i < city->event_count; i++)
        if (city->events[i].event_id == event_id)
            return &city->events[i];
    return NULL;
}

int sc_event_list_upcoming(SC_CitySystem *city, SC_CityEvent **events, int max) {
    if (!city || !events || max <= 0) return 0;
    time_t now = time(NULL);
    int count = 0;
    for (int i = 0; i < city->event_count && count < max; i++) {
        if (!city->events[i].is_cancelled &&
            city->events[i].start_time > now)
            events[count++] = &city->events[i];
    }
    return count;
}

/* ================================================================
 *  L7: Citizen Portal Analytics
 *
 *  Application: Public-facing analytics for transparency.
 *  Computes average satisfaction score and identifies top complaint
 *  categories using a simple selection sort on request counts.
 * ================================================================ */

float sc_citizen_satisfaction_avg(const SC_CitySystem *city) {
    if (!city || city->citizen_count == 0) return 0.0f;
    float sum = 0.0f;
    int count = 0;
    for (int i = 0; i < city->citizen_count; i++) {
        if (city->citizens[i].satisfaction_score > 0.0f) {
            sum += city->citizens[i].satisfaction_score;
            count++;
        }
    }
    return count > 0 ? sum / (float)count : 0.0f;
}

int sc_top_complaint_categories(const SC_CitySystem *city,
                                 SC_ServiceCategory *top, int n) {
    if (!city || !top || n <= 0) return 0;

    int counts[SC_SVC_COUNT];
    int indices[SC_SVC_COUNT];
    memset(counts, 0, sizeof(counts));
    for (int i = 0; i < SC_SVC_COUNT; i++) indices[i] = i;

    /* Count requests per category */
    for (int i = 0; i < city->request_count; i++)
        counts[(int)city->requests[i].category]++;

    /* Simple selection sort descending */
    for (int i = 0; i < SC_SVC_COUNT - 1; i++) {
        for (int j = i + 1; j < SC_SVC_COUNT; j++) {
            if (counts[j] > counts[i]) {
                int tc = counts[i]; counts[i] = counts[j]; counts[j] = tc;
                int ti = indices[i]; indices[i] = indices[j]; indices[j] = ti;
            }
        }
    }

    int r = (n < SC_SVC_COUNT) ? n : SC_SVC_COUNT;
    for (int i = 0; i < r; i++)
        top[i] = (SC_ServiceCategory)indices[i];
    return r;
}

/* ================================================================
 *  L8: Digital Twin Snapshot
 *
 *  Advanced Topic: Exports a JSON snapshot of the city's current
 *  state for integration with digital twin visualization platforms.
 *  Provides real-time dashboard data as structured JSON.
 *
 *  Industry Reference: Singapore Virtual Singapore, Helsinki 3D,
 *  Dubai Digital Twin, AWS IoT TwinMaker.
 * ================================================================ */

int sc_digital_twin_snapshot(const SC_CitySystem *city, char *json_buf,
                              int buf_size) {
    if (!city || !json_buf || buf_size <= 0) return -1;

    SC_DashboardKPI kpi = sc_dashboard_compute(city);

    return snprintf(json_buf, (size_t)buf_size,
        "{\"city\":\"%s\",\"districts\":%d,\"citizens\":%d,"
        "\"active_requests\":%d,\"overdue\":%d,"
        "\"avg_satisfaction\":%.2f,\"budget_utilization_pct\":%.1f,"
        "\"departments_under_sla\":%d,\"departments_over_sla\":%d,"
        "\"resolution_hours_avg\":%.1f}",
        city->city_name, city->district_count, kpi.total_citizens,
        kpi.active_requests, kpi.overdue_requests,
        kpi.avg_satisfaction, kpi.budget_utilization_pct,
        kpi.departments_under_sla, kpi.departments_over_sla,
        kpi.avg_resolution_time_hours);
}
