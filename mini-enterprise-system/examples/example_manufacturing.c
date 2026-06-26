/* example_manufacturing.c - MRP II: BOM explosion, work orders, capacity planning */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mrp_engine.h"

int main(void) {
  printf("=== Manufacturing MRP II Demo ===\n\n");
  MENT_MRPInstance *mrp = ment_mrp_create(26);

  /* 1. Items */
  int fg = ment_mrp_add_item(mrp, "FG-100", "Finished Bicycle", MENT_ITEM_FINISHED_GOOD, 7, 50);
  int sa = ment_mrp_add_item(mrp, "SA-200", "Wheel Assembly", MENT_ITEM_SUBASSEMBLY, 3, 100);
  int rm = ment_mrp_add_item(mrp, "RM-300", "Aluminum Tube", MENT_ITEM_RAW_MATERIAL, 2, 500);
  printf("[1] Item Master: FG=%d, SA=%d, RM=%d\n", fg, sa, rm);

  /* 2. BOM: 1 FG needs 2 SAs, 1 SA needs 18 RMs */
  ment_mrp_add_bom(mrp, fg, sa, 2.0f);
  ment_mrp_add_bom(mrp, sa, rm, 18.0f);
  printf("[2] BOM: FG-100 -> 2x SA-200 -> 36x RM-300\n");

  /* 3. Low-Level Codes */
  ment_mrp_assign_low_level_codes(mrp);
  printf("[3] LLC: FG=%d, SA=%d, RM=%d\n",
         mrp->items[0].low_level_code,
         mrp->items[1].low_level_code,
         mrp->items[2].low_level_code);

  /* 4. Demand & Supply */
  ment_mrp_add_demand(mrp, fg, 5, 200);
  ment_mrp_add_supply(mrp, fg, 1, 30, "OnHand");
  ment_mrp_add_supply(mrp, sa, 1, 80, "OnHand");
  printf("[4] MPS: 200 units in week 5, On-hand: FG=30, SA=80\n");

  /* 5. Work Center */
  ment_mrp_add_workcenter(mrp, "ASSY", 40.0f);
  ment_mrp_add_routing(mrp, fg, 1, 1, 2.5f);
  printf("[5] Work Center: ASSY (40 hrs/day), Routing: FG=2.5 hrs/unit\n");

  /* 6. MRP Explosion */
  printf("[6] Running MRP explosion...\n");
  ment_mrp_explode(mrp);

  /* 7. Pegging */
  MENT_MRPPegging peg[26];
  int n = ment_mrp_get_pegging(mrp, fg, peg, 26);
  printf("[7] Pegging for FG-100 (%d periods):\n", n);
  printf("    Period | GrossReq | OnHand | NetReq | PlannedOrder\n");
  printf("    -------+----------+--------+--------+-------------\n");
  for (int i = 0; i < n && i < 12; i++) {
    if (peg[i].gross_requirements > 0 || peg[i].net_requirements > 0 ||
        peg[i].planned_order_releases > 0) {
      printf("    %6d | %8d | %6d | %6d | %12d\n",
             peg[i].period, peg[i].gross_requirements,
             peg[i].projected_on_hand, peg[i].net_requirements,
             peg[i].planned_order_releases);
    }
  }

  /* 8. Work Orders */
  int n_wo = ment_mrp_generate_work_orders(mrp);
  printf("[8] Generated %d planned work orders\n", n_wo);

  /* 9. Capacity Check */
  float load[26], cap[26];
  ment_mrp_capacity_check(mrp, 1, load, cap, 26);
  printf("[9] Capacity: Work Center ASSY\n");
  for (int i = 0; i < 12; i++) {
    if (load[i] > 0.01f) {
      printf("    Week %d: Load=%.1f hrs, Capacity=%.1f hrs, %s\n",
             i, load[i], cap[i],
             load[i] > cap[i] ? "OVERLOADED!" : "OK");
    }
  }

  printf("\n=== MRP Demo Complete ===\n");
  ment_mrp_destroy(mrp);
  return 0;
}
