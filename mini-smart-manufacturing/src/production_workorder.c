/* production_workorder.c - Work Order Management & Capacity Planning
 * L2: Shop floor control, MRP II work order lifecycle
 * L6: Work order dispatching, capacity requirements planning (CRP)
 */
#include "production_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

SMF_WorkOrder* smf_wo_create(SMF_Factory *factory, int product_idx,
                              int quantity, time_t planned_start,
                              time_t planned_end, int priority) {
    if (!factory) return NULL;
    if (factory->num_work_orders >= SMF_MAX_WORK_ORDERS) return NULL;
    if (product_idx < 0 || product_idx >= factory->num_products) return NULL;

    SMF_WorkOrder *wo = (SMF_WorkOrder*)calloc(1, sizeof(SMF_WorkOrder));
    if (!wo) return NULL;

    int wo_num = factory->num_work_orders + 1;
    snprintf(wo->wo_number, SMF_CODE_LEN, "WO-%06d", wo_num);
    wo->product_idx = product_idx;
    wo->routing_idx = factory->products[product_idx].routing_idx;
    wo->bom_idx = factory->products[product_idx].bom_idx;
    wo->quantity_ordered = quantity;
    wo->quantity_completed = 0;
    wo->quantity_scrapped = 0;
    wo->state = SMF_WO_PLANNED;
    wo->planned_start = planned_start;
    wo->planned_end = planned_end;
    wo->actual_start = 0;
    wo->actual_end = 0;
    wo->current_step_idx = 0;
    wo->priority = priority;
    wo->customer_order_idx = -1;
    wo->is_outsourced = false;
    wo->outsourced_vendor_idx = -1;
    wo->estimated_cost = smf_product_standard_cost(factory, product_idx)
                         * (float)quantity;
    wo->actual_cost = 0.0f;

    factory->work_orders[factory->num_work_orders++] = wo;
    return wo;
}

int smf_wo_release(SMF_Factory *factory, int wo_idx) {
    if (!factory || wo_idx < 0 || wo_idx >= factory->num_work_orders) return -1;
    SMF_WorkOrder *wo = factory->work_orders[wo_idx];
    if (wo->state != SMF_WO_PLANNED) return -2;
    wo->state = SMF_WO_RELEASED;
    wo->actual_start = time(NULL);

    /* Reserve materials from BOM */
    int bom_idx = wo->bom_idx;
    if (bom_idx >= 0 && bom_idx < factory->num_boms) {
        SMF_BillOfMaterials *bom = &factory->boms[bom_idx];
        for (int i = 0; i < bom->num_items; i++) {
            int mat_idx = bom->items[i].material_idx;
            if (mat_idx >= 0 && mat_idx < factory->num_materials) {
                float needed = bom->items[i].quantity_per
                               * (float)wo->quantity_ordered;
                factory->materials[mat_idx].quantity_reserved += needed;
            }
        }
    }
    return 0;
}

int smf_wo_complete_step(SMF_Factory *factory, int wo_idx,
                          int step_idx, int qty_good, int qty_scrap) {
    if (!factory || wo_idx < 0 || wo_idx >= factory->num_work_orders) return -1;
    SMF_WorkOrder *wo = factory->work_orders[wo_idx];
    if (wo->state != SMF_WO_RELEASED && wo->state != SMF_WO_IN_PROGRESS)
        return -2;
    wo->state = SMF_WO_IN_PROGRESS;
    wo->current_step_idx = step_idx + 1;
    wo->quantity_completed += qty_good;
    wo->quantity_scrapped += qty_scrap;
    /* Simplified cost accumulation */
    wo->actual_cost += (float)(qty_good + qty_scrap) * 1.0f;
    return 0;
}

int smf_wo_close(SMF_Factory *factory, int wo_idx) {
    if (!factory || wo_idx < 0 || wo_idx >= factory->num_work_orders) return -1;
    SMF_WorkOrder *wo = factory->work_orders[wo_idx];
    if (wo->state != SMF_WO_IN_PROGRESS && wo->state != SMF_WO_COMPLETED)
        return -2;
    wo->state = SMF_WO_CLOSED;
    wo->actual_end = time(NULL);
    return 0;
}

int smf_wo_find_next(const SMF_Factory *factory, int workstation_idx,
                      SMF_DispatchRule rule) {
    if (!factory) return -1;
    int best_idx = -1;
    float best_criterion = FLT_MAX;
    time_t now = time(NULL);

    for (int i = 0; i < factory->num_work_orders; i++) {
        SMF_WorkOrder *wo = factory->work_orders[i];
        if (!wo || wo->state != SMF_WO_RELEASED) continue;

        int routing_idx = wo->routing_idx;
        if (routing_idx < 0 || routing_idx >= factory->num_routings) continue;
        SMF_Routing *routing = (SMF_Routing*)&factory->routings[routing_idx];

        int step = wo->current_step_idx;
        if (step < 0 || step >= routing->num_steps) continue;
        if (routing->steps[step].workstation_idx != workstation_idx
            && routing->steps[step].workstation_idx >= 0) continue;

        float criterion;
        switch (rule) {
            case SMF_DISPATCH_EDD:
                criterion = (float)difftime(wo->planned_end, now);
                break;
            case SMF_DISPATCH_SPT:
                criterion = routing->steps[step].run_time_per_unit_min;
                break;
            default:
                criterion = (float)wo->priority;
                break;
        }

        if (criterion < best_criterion) {
            best_criterion = criterion;
            best_idx = i;
        }
    }
    return best_idx;
}

float smf_wo_progress(const SMF_WorkOrder *wo) {
    if (!wo || wo->quantity_ordered <= 0) return 0.0f;
    return (float)wo->quantity_completed
           / (float)wo->quantity_ordered * 100.0f;
}

/* ================================================================
 *  Capacity Requirements Planning (CRP)
 *  Calculates load vs capacity for each work center.
 * ================================================================ */

int smf_capacity_load_profile(const SMF_Factory *factory,
                               SMF_CapacityLoad *loads, int max_ws) {
    if (!factory || !loads) return 0;
    int count = factory->num_workstations < max_ws
                ? factory->num_workstations : max_ws;
    float avail = factory->shifts_per_day * factory->hours_per_shift
                  * factory->days_per_week;

    for (int i = 0; i < count; i++) {
        loads[i].workstation_idx = i;
        loads[i].available_hours_per_week = avail;
        loads[i].required_hours_per_week = 0.0f;
        loads[i].load_percent = 0.0f;
        loads[i].overload_hours = 0.0f;
        loads[i].underload_hours = 0.0f;

        /* Sum required hours from all active work orders */
        for (int j = 0; j < factory->num_work_orders; j++) {
            SMF_WorkOrder *wo = factory->work_orders[j];
            if (!wo || wo->state >= SMF_WO_COMPLETED) continue;
            int ridx = wo->routing_idx;
            if (ridx < 0) continue;
            SMF_Routing *r = (SMF_Routing*)&factory->routings[ridx];
            for (int s = 0; s < r->num_steps; s++) {
                if (r->steps[s].workstation_idx == i) {
                    float remaining = (float)(wo->quantity_ordered
                        - wo->quantity_completed);
                    loads[i].required_hours_per_week +=
                        r->steps[s].run_time_per_unit_min / 60.0f * remaining;
                }
            }
        }
    }

    for (int i = 0; i < count; i++) {
        if (loads[i].available_hours_per_week > 0.0f) {
            loads[i].load_percent = (loads[i].required_hours_per_week
                / loads[i].available_hours_per_week) * 100.0f;
        }
        float diff = loads[i].required_hours_per_week
                     - loads[i].available_hours_per_week;
        if (diff > 0.0f) loads[i].overload_hours = diff;
        else loads[i].underload_hours = -diff;
    }
    return count;
}

float smf_capacity_overload(const SMF_CapacityLoad *load) {
    if (!load) return 0.0f;
    return load->load_percent > 100.0f ? load->load_percent - 100.0f : 0.0f;
}

int smf_capacity_bottlenecks(const SMF_CapacityLoad *loads, int count,
                              int *bottleneck_indices, int max_results) {
    if (!loads || !bottleneck_indices) return 0;
    int result = 0;
    for (int i = 0; i < count && result < max_results; i++) {
        if (loads[i].load_percent > 100.0f) {
            bottleneck_indices[result++] = loads[i].workstation_idx;
        }
    }
    return result;
}

/* ================================================================
 *  Takt Time & Line Balancing
 *  Takt = Available Time / Demand (German: rhythm/beat)
 *  N_min = ceil(sum of task times / Cycle Time)
 *  LCR: Largest Candidate Rule for assembly line balancing
 * ================================================================ */

float smf_calc_takt_time(float available_time_min, float customer_demand) {
    if (customer_demand <= 0.0f) return 0.0f;
    return available_time_min / customer_demand;
}

int smf_calc_min_workstations(const float *task_times, int num_tasks,
                               float cycle_time) {
    if (!task_times || num_tasks <= 0 || cycle_time <= 0.0f) return 0;
    float total = 0.0f;
    for (int i = 0; i < num_tasks; i++) total += task_times[i];
    return (int)ceilf(total / cycle_time);
}

int smf_line_balance_lcr(const float *task_times,
                          const int *precedence[2], int num_tasks,
                          int num_prec_edges, float cycle_time,
                          int *station_assignments) {
    if (!task_times || !station_assignments || num_tasks <= 0) return 0;
    for (int i = 0; i < num_tasks; i++) station_assignments[i] = -1;

    int *task_order = (int*)malloc(num_tasks * sizeof(int));
    if (!task_order) return 0;
    for (int i = 0; i < num_tasks; i++) task_order[i] = i;

    /* Sort by descending task time (Largest Candidate Rule) */
    for (int i = 0; i < num_tasks - 1; i++) {
        for (int j = i + 1; j < num_tasks; j++) {
            if (task_times[task_order[i]] < task_times[task_order[j]]) {
                int tmp = task_order[i];
                task_order[i] = task_order[j];
                task_order[j] = tmp;
            }
        }
    }

    int num_stations = 0;
    float *station_load = (float*)calloc(num_tasks, sizeof(float));

    for (int ti = 0; ti < num_tasks; ti++) {
        int task = task_order[ti];
        float ttime = task_times[task];

        /* Check precedence constraints */
        bool prec_ok = true;
        if (precedence) {
            for (int e = 0; e < num_prec_edges; e++) {
                if (precedence[1][e] == task) {
                    if (station_assignments[precedence[0][e]] < 0) {
                        prec_ok = false;
                        break;
                    }
                }
            }
        }
        if (!prec_ok) continue;

        /* Assign to first station that fits */
        bool assigned = false;
        for (int s = 0; s <= num_stations; s++) {
            if (station_load[s] + ttime <= cycle_time + 0.001f) {
                station_assignments[task] = s;
                station_load[s] += ttime;
                assigned = true;
                break;
            }
        }

        if (!assigned) {
            num_stations++;
            station_assignments[task] = num_stations;
            station_load[num_stations] = ttime;
        }
    }

    free(station_load);
    free(task_order);
    return num_stations + 1;
}