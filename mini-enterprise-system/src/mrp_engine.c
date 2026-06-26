#include "mrp_engine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ================================================================
 * MRP Engine Implementation
 *
 * L2: Dependent/independent demand, lead time, lot sizing, safety stock
 * L3: BOM tree (Gozinto graph), low-level coding
 * L5: MRP explosion, lot-sizing (EOQ, L4L, POQ, PPB), CRP
 * L6: Net requirements calculation, pegging, work order generation
 *
 * Reference: Orlicky (1975), Vollmann et al. (2005)
 * ================================================================ */

MENT_MRPInstance* ment_mrp_create(int horizon_weeks) {
    MENT_MRPInstance *mrp = (MENT_MRPInstance*)malloc(sizeof(MENT_MRPInstance));
    if (!mrp) return NULL;
    memset(mrp, 0, sizeof(MENT_MRPInstance));
    mrp->horizon_weeks = (horizon_weeks > 0 && horizon_weeks <= MENT_MRP_HORIZON_WEEKS) ?
                         horizon_weeks : 52;
    /* Allocate dynamic arrays */
    int max_pegging = MENT_MRP_MAX_ITEMS * mrp->horizon_weeks;
    mrp->demands = (MENT_Demand*)calloc((size_t)MENT_MRP_MAX_ITEMS * 52, sizeof(MENT_Demand));
    mrp->supplies = (MENT_Supply*)calloc((size_t)MENT_MRP_MAX_ITEMS * 52, sizeof(MENT_Supply));
    mrp->pegging = (MENT_MRPPegging*)calloc((size_t)max_pegging, sizeof(MENT_MRPPegging));
    if (!mrp->demands || !mrp->supplies || !mrp->pegging) {
        free(mrp->demands); free(mrp->supplies); free(mrp->pegging);
        free(mrp); return NULL;
    }
    return mrp;
}

void ment_mrp_destroy(MENT_MRPInstance *mrp) {
    if (!mrp) return;
    free(mrp->demands);
    free(mrp->supplies);
    free(mrp->pegging);
    free(mrp);
}

int ment_mrp_add_item(MENT_MRPInstance *mrp, const char *code,
                       const char *desc, MENT_ItemType type,
                       int lead_time, int safety_stock) {
    if (mrp->num_items >= MENT_MRP_MAX_ITEMS) return -1;
    int idx = mrp->num_items++;
    MENT_Item *item = &mrp->items[idx];
    memset(item, 0, sizeof(MENT_Item));
    item->id = idx + 1;
    item->type = type;
    item->lead_time_days = lead_time;
    item->safety_stock = safety_stock;
    item->lot_size_method = MENT_LOTSIZE_LOT_FOR_LOT;
    item->lot_size_qty = 1;
    item->is_purchased = (type == MENT_ITEM_RAW_MATERIAL);
    item->is_manufactured = (type == MENT_ITEM_FINISHED_GOOD || type == MENT_ITEM_SUBASSEMBLY);
    snprintf(item->item_code, sizeof(item->item_code), "%s", code);
    snprintf(item->description, sizeof(item->description), "%s", desc);
    snprintf(item->uom, sizeof(item->uom), "EA");
    return item->id;
}

int ment_mrp_add_bom(MENT_MRPInstance *mrp, int parent_id,
                      int component_id, float qty_per) {
    if (mrp->num_bom_lines >= MENT_MRP_MAX_BOM_LINES) return -1;
    int idx = mrp->num_bom_lines++;
    MENT_BOMLine *bom = &mrp->bom[idx];
    memset(bom, 0, sizeof(MENT_BOMLine));
    bom->id = idx + 1;
    bom->parent_item_id = parent_id;
    bom->component_item_id = component_id;
    bom->quantity_per = qty_per;
    bom->effective_to = -1; /* indefinite */
    return bom->id;
}

int ment_mrp_add_workcenter(MENT_MRPInstance *mrp, const char *code,
                              float capacity_per_day) {
    if (mrp->num_workcenters >= MENT_MRP_MAX_WORKCENTERS) return -1;
    int idx = mrp->num_workcenters++;
    MENT_WorkCenter *wc = &mrp->workcenters[idx];
    memset(wc, 0, sizeof(MENT_WorkCenter));
    wc->id = idx + 1;
    wc->capacity_per_day = capacity_per_day;
    wc->efficiency = 0.85f;
    wc->utilization = 0.80f;
    wc->cost_per_hour = 50.0f;
    snprintf(wc->code, sizeof(wc->code), "%s", code);
    return wc->id;
}

int ment_mrp_add_routing(MENT_MRPInstance *mrp, int item_id,
                          int seq, int wc_id, float run_hours) {
    if (mrp->num_routings >= MENT_MRP_MAX_ROUTINGS) return -1;
    int idx = mrp->num_routings++;
    MENT_RoutingStep *rt = &mrp->routings[idx];
    memset(rt, 0, sizeof(MENT_RoutingStep));
    rt->id = idx + 1;
    rt->item_id = item_id;
    rt->operation_seq = seq;
    rt->workcenter_id = wc_id;
    rt->run_hours_per_unit = run_hours;
    rt->setup_hours = 0.5f;
    return rt->id;
}

int ment_mrp_add_demand(MENT_MRPInstance *mrp, int item_id,
                         int period, int qty) {
    if (mrp->num_demands >= MENT_MRP_MAX_ITEMS * 52) return -1;
    int idx = mrp->num_demands++;
    MENT_Demand *d = &mrp->demands[idx];
    d->item_id = item_id;
    d->period = period;
    d->quantity = qty;
    snprintf(d->source, sizeof(d->source), "MPS");
    return 0;
}

int ment_mrp_add_supply(MENT_MRPInstance *mrp, int item_id,
                         int period, int qty, const char *type) {
    if (mrp->num_supplies >= MENT_MRP_MAX_ITEMS * 52) return -1;
    int idx = mrp->num_supplies++;
    MENT_Supply *s = &mrp->supplies[idx];
    s->item_id = item_id;
    s->period = period;
    s->quantity = qty;
    snprintf(s->type, sizeof(s->type), "%s", type ? type : "OnHand");
    return 0;
}

/* Low-Level Code Assignment (L5 pre-processing) */
void ment_mrp_assign_low_level_codes(MENT_MRPInstance *mrp) {
    /* Reset all to 0 */
    for (int i = 0; i < mrp->num_items; i++)
        mrp->items[i].low_level_code = 0;

    /* Iterate until stable */
    bool changed = true;
    int iter = 0;
    while (changed && iter < 200) {
        changed = false;
        iter++;
        for (int i = 0; i < mrp->num_bom_lines; i++) {
            /* Parent must have higher LLC than component */
            int p_idx = -1, c_idx = -1;
            for (int j = 0; j < mrp->num_items; j++) {
                if (mrp->items[j].id == mrp->bom[i].parent_item_id) p_idx = j;
                if (mrp->items[j].id == mrp->bom[i].component_item_id) c_idx = j;
            }
            if (p_idx < 0 || c_idx < 0) continue;
            int new_llc = mrp->items[p_idx].low_level_code + 1;
            if (new_llc > mrp->items[c_idx].low_level_code) {
                mrp->items[c_idx].low_level_code = new_llc;
                changed = true;
            }
        }
    }
}

/* MRP Explosion (L5: Core MRP Algorithm) */
int ment_mrp_explode(MENT_MRPInstance *mrp) {
    ment_mrp_assign_low_level_codes(mrp);

    /* Process items by low-level code (ascending) */
    /* Use static arrays for period data */
    int gross_req[52] = {0};
    int sched_rec[52] = {0};
    int proj_on_hand[52] = {0};
    int net_req[52] = {0};
    int planned_rec[52] = {0};
    int planned_rel[52] = {0};

    /* For each item, ordered by LLC */
    for (int llc = 0; llc < 100; llc++) {
        for (int i = 0; i < mrp->num_items; i++) {
            if (mrp->items[i].low_level_code != llc) continue;
            int item_id = mrp->items[i].id;
            int lt_weeks = (mrp->items[i].lead_time_days + 6) / 7;
            if (lt_weeks < 1) lt_weeks = 1;

            memset(gross_req, 0, sizeof(gross_req));
            memset(sched_rec, 0, sizeof(sched_rec));
            memset(proj_on_hand, 0, sizeof(proj_on_hand));
            memset(net_req, 0, sizeof(net_req));
            memset(planned_rec, 0, sizeof(planned_rec));
            memset(planned_rel, 0, sizeof(planned_rel));

            /* Collect gross requirements from demand */
            for (int d = 0; d < mrp->num_demands; d++) {
                if (mrp->demands[d].item_id == item_id &&
                    mrp->demands[d].period < mrp->horizon_weeks)
                    gross_req[mrp->demands[d].period] += mrp->demands[d].quantity;
            }

            /* Collect scheduled receipts from supply */
            for (int s = 0; s < mrp->num_supplies; s++) {
                if (mrp->supplies[s].item_id == item_id &&
                    mrp->supplies[s].period < mrp->horizon_weeks)
                    sched_rec[mrp->supplies[s].period] += mrp->supplies[s].quantity;
            }

            /* Initial projected on-hand */
            int on_hand = 0; /* would come from inventory */
            proj_on_hand[0] = on_hand;

            /* MRP logic period by period */
            for (int p = 0; p < mrp->horizon_weeks; p++) {
                int prev_poh = (p > 0) ? proj_on_hand[p-1] : on_hand;
                int available = prev_poh + sched_rec[p];
                net_req[p] = (gross_req[p] > available + mrp->items[i].safety_stock) ?
                             gross_req[p] - available : 0;

                if (net_req[p] > 0) {
                    /* Apply lot sizing */
                    int lot = mrp->items[i].lot_size_qty;
                    if (lot < 1) lot = 1;
                    planned_rec[p] = ((net_req[p] + lot - 1) / lot) * lot;

                    /* Offset by lead time for planned release */
                    int rel_period = p - lt_weeks;
                    if (rel_period >= 0 && rel_period < mrp->horizon_weeks) {
                        planned_rel[rel_period] += planned_rec[p];

                        /* Explode BOM: generate component requirements */
                        for (int b = 0; b < mrp->num_bom_lines; b++) {
                            if (mrp->bom[b].parent_item_id == item_id) {
                                int comp_id = mrp->bom[b].component_item_id;
                                int comp_qty = (int)(planned_rel[rel_period] *
                                                     mrp->bom[b].quantity_per);
                                /* Add as demand for component at release period */
                                if (mrp->num_demands < MENT_MRP_MAX_ITEMS * 52) {
                                    int didx = mrp->num_demands++;
                                    mrp->demands[didx].item_id = comp_id;
                                    mrp->demands[didx].period = rel_period;
                                    mrp->demands[didx].quantity = comp_qty;
                                    snprintf(mrp->demands[didx].source, 32,
                                             "MRP-Explode");
                                }
                            }
                        }
                    }
                }

                proj_on_hand[p] = available + planned_rec[p] - gross_req[p];
                if (proj_on_hand[p] < 0) proj_on_hand[p] = 0;
            }

            /* Store pegging data */
            for (int p = 0; p < mrp->horizon_weeks && mrp->num_pegging < MENT_MRP_MAX_ITEMS * MENT_MRP_HORIZON_WEEKS; p++) {
                int pidx = mrp->num_pegging++;
                mrp->pegging[pidx].item_id = item_id;
                mrp->pegging[pidx].period = p;
                mrp->pegging[pidx].gross_requirements = gross_req[p];
                mrp->pegging[pidx].scheduled_receipts = sched_rec[p];
                mrp->pegging[pidx].projected_on_hand = proj_on_hand[p];
                mrp->pegging[pidx].net_requirements = net_req[p];
                mrp->pegging[pidx].planned_order_receipts = planned_rec[p];
                mrp->pegging[pidx].planned_order_releases = planned_rel[p];
            }
        }
    }
    return 0;
}

int ment_mrp_get_pegging(const MENT_MRPInstance *mrp, int item_id,
                          MENT_MRPPegging *pegging, int max_periods) {
    int count = 0;
    for (int i = 0; i < mrp->num_pegging && count < max_periods; i++) {
        if (mrp->pegging[i].item_id == item_id) {
            memcpy(&pegging[count], &mrp->pegging[i], sizeof(MENT_MRPPegging));
            count++;
        }
    }
    return count;
}

int ment_mrp_generate_work_orders(MENT_MRPInstance *mrp) {
    int count = 0;
    for (int i = 0; i < mrp->num_pegging && mrp->num_work_orders < MENT_MRP_MAX_WORKORDERS; i++) {
        if (mrp->pegging[i].planned_order_releases > 0) {
            int widx = mrp->num_work_orders++;
            MENT_WorkOrder *wo = &mrp->work_orders[widx];
            memset(wo, 0, sizeof(MENT_WorkOrder));
            wo->id = widx + 1;
            wo->item_id = mrp->pegging[i].item_id;
            wo->quantity = mrp->pegging[i].planned_order_releases;
            wo->status = MENT_WO_PLANNED;
            wo->priority = 50;
            count++;
        }
    }
    return count;
}

int ment_mrp_lot_size(MENT_LotSizeMethod method, int *net_req, int periods,
                       float setup_cost, float holding_cost,
                       int *planned_orders, int lead_time) {
    (void)lead_time;
    memset(planned_orders, 0, (size_t)periods * sizeof(int));

    switch (method) {
        case MENT_LOTSIZE_LOT_FOR_LOT:
            for (int p = 0; p < periods; p++)
                planned_orders[p] = net_req[p];
            return 0;

        case MENT_LOTSIZE_FIXED: {
            /* Use a reasonable fixed lot size: first net req or 100 */
            int lot = 100;
            for (int p = 0; p < periods; p++) {
                if (net_req[p] > 0 && lot == 100) lot = net_req[p];
                planned_orders[p] = ((net_req[p] + lot - 1) / lot) * lot;
            }
            return 0;
        }

        case MENT_LOTSIZE_EOQ: {
            /* EOQ = sqrt(2 * D * S / H) */
            int annual_demand = 0;
            for (int p = 0; p < periods; p++) annual_demand += net_req[p];
            if (holding_cost <= 0.0f || setup_cost <= 0.0f) {
                for (int p = 0; p < periods; p++) planned_orders[p] = net_req[p];
                return 0;
            }
            int eoq = (int)sqrtf(2.0f * (float)annual_demand * setup_cost / holding_cost);
            if (eoq < 1) eoq = 1;
            for (int p = 0; p < periods; p++)
                planned_orders[p] = ((net_req[p] + eoq - 1) / eoq) * eoq;
            return 0;
        }

        case MENT_LOTSIZE_PERIOD_ORDER: {
            /* POQ: combine N periods based on EOQ logic */
            int annual_demand = 0;
            for (int p = 0; p < periods; p++) annual_demand += net_req[p];
            int avg_per_period = (periods > 0) ? annual_demand / periods : 1;
            if (avg_per_period < 1) avg_per_period = 1;
            float eoq_f = (holding_cost > 0 && setup_cost > 0) ?
                sqrtf(2.0f * (float)annual_demand * setup_cost / holding_cost) : 1.0f;
            int n_periods = (int)(eoq_f / (float)avg_per_period);
            if (n_periods < 1) n_periods = 1;
            int accum = 0;
            int counter = 0;
            for (int p = 0; p < periods; p++) {
                accum += net_req[p];
                counter++;
                if (counter >= n_periods || p == periods - 1) {
                    planned_orders[p] = accum;
                    accum = 0;
                    counter = 0;
                }
            }
            return 0;
        }

        case MENT_LOTSIZE_PART_PERIOD: {
            /* PPB: combine until holding cost >= setup cost */
            int start = 0;
            while (start < periods) {
                int accum_qty = 0;
                int accum_holding = 0;
                int end = start;
                for (int p = start; p < periods; p++) {
                    if (net_req[p] == 0 && accum_qty == 0) { start = p + 1; continue; }
                    accum_qty += net_req[p];
                    accum_holding += net_req[p] * (p - start);
                    if ((float)accum_holding * holding_cost >= setup_cost) {
                        end = p;
                        break;
                    }
                    end = p;
                }
                planned_orders[end] = accum_qty;
                start = end + 1;
            }
            return 0;
        }
        default:
            return -1;
    }
}

int ment_mrp_capacity_check(const MENT_MRPInstance *mrp,
                             int wc_id, float *load, float *capacity,
                             int max_periods) {
    memset(load, 0, (size_t)max_periods * sizeof(float));
    memset(capacity, 0, (size_t)max_periods * sizeof(float));

    /* Find work center */
    int wc_idx = -1;
    for (int i = 0; i < mrp->num_workcenters; i++)
        if (mrp->workcenters[i].id == wc_id) { wc_idx = i; break; }
    if (wc_idx < 0) return -1;

    float cap_per_period = mrp->workcenters[wc_idx].capacity_per_day * 5.0f; /* weekly */

    for (int p = 0; p < max_periods && p < mrp->horizon_weeks; p++) {
        float total_load = 0.0f;
        /* Sum hours from planned orders */
        for (int i = 0; i < mrp->num_pegging; i++) {
            if (mrp->pegging[i].period == p &&
                mrp->pegging[i].planned_order_releases > 0) {
                /* Find routing for this item at this work center */
                for (int r = 0; r < mrp->num_routings; r++) {
                    if (mrp->routings[r].item_id == mrp->pegging[i].item_id &&
                        mrp->routings[r].workcenter_id == wc_id) {
                        total_load += (float)mrp->pegging[i].planned_order_releases *
                                      mrp->routings[r].run_hours_per_unit +
                                      mrp->routings[r].setup_hours;
                    }
                }
            }
        }
        load[p] = total_load;
        capacity[p] = cap_per_period;
    }
    return max_periods;
}

float ment_mrp_inventory_value(const MENT_MRPInstance *mrp) {
    float total = 0.0f;
    for (int i = 0; i < mrp->num_items; i++) {
        /* On-hand quantity * unit cost */
        if (mrp->items[i].is_purchased || mrp->items[i].is_manufactured)
            total += 0.0f; /* Would need inventory tracking */
    }
    (void)total;
    return total;
}

float ment_mrp_wip_value(const MENT_MRPInstance *mrp) {
    float total = 0.0f;
    for (int i = 0; i < mrp->num_work_orders; i++) {
        if (mrp->work_orders[i].status == MENT_WO_RELEASED ||
            mrp->work_orders[i].status == MENT_WO_IN_PROGRESS) {
            /* Find item cost */
            for (int j = 0; j < mrp->num_items; j++) {
                if (mrp->items[j].id == mrp->work_orders[i].item_id) {
                    total += (float)mrp->work_orders[i].quantity *
                             mrp->items[j].unit_cost;
                    break;
                }
            }
        }
    }
    return total;
}
