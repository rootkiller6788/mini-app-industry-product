#ifndef MINI_SMF_SMART_MANUFACTURING_H
#define MINI_SMF_SMART_MANUFACTURING_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* ================================================================
 *  smart_manufacturing.h — Smart Manufacturing Core Definitions
 *
 *  L1: Core definitions — Factory, Machine, Tool, Material,
 *      Product, BOM, Route, Workstation
 *  L2: Core concepts — Lean Manufacturing, Just-In-Time (JIT),
 *      Toyota Production System (TPS), Kaizen, Muda/Mura/Muri
 *  L3: Engineering structures — Product routing chain, BOM tree,
 *      Work cell layout, Production line topology
 *  L4: Standards — ISA-95 (IEC 62264) automation pyramid,
 *      ISO 22400 KPIs for manufacturing operations
 *  L5: Algorithms — BOM traversal, routing optimization, cycle
 *      time calculation, takt time analysis
 *  L6: Canonical problems — Factory layout design, production
 *      line balancing, bottleneck identification
 *  L7: Applications — Smart factory digital twin, Industry 4.0
 *      reference architecture (RAMI 4.0)
 *  L8: Advanced — Cyber-Physical Systems (CPS), IIoT gateway,
 *      OPC-UA Unified Architecture
 *  L9: Industry — Siemens MindSphere, GE Predix, Bosch IoT Suite
 *
 *  References:
 *  - ISA-95 (IEC 62264): Enterprise-Control System Integration
 *  - ISO 22400-1/2: KPIs for Manufacturing Operations Management
 *  - Womack & Jones, "Lean Thinking" (1996)
 *  - Ohno, "Toyota Production System" (1988)
 *  - Plattform Industrie 4.0, "RAMI 4.0" (2015)
 * ================================================================ */

#define SMF_MAX_MACHINES       2048
#define SMF_MAX_WORKSTATIONS   512
#define SMF_MAX_TOOLS          4096
#define SMF_MAX_MATERIALS      8192
#define SMF_MAX_PRODUCTS       1024
#define SMF_MAX_BOM_ITEMS      128
#define SMF_MAX_ROUTE_STEPS    64
#define SMF_MAX_WORK_ORDERS    65536
#define SMF_NAME_LEN           128
#define SMF_CODE_LEN           32
#define SMF_DESC_LEN           256

/* ---- L1: Machine / Equipment Types (ISA-95 Level 2) ---- */

typedef enum {
    SMF_MACH_CNC              = 0,
    SMF_MACH_LATHE            = 1,
    SMF_MACH_MILLING          = 2,
    SMF_MACH_GRINDING         = 3,
    SMF_MACH_WELDING          = 4,
    SMF_MACH_ASSEMBLY         = 5,
    SMF_MACH_INSPECTION       = 6,
    SMF_MACH_PACKAGING        = 7,
    SMF_MACH_CONVEYOR         = 8,
    SMF_MACH_AGV              = 9,
    SMF_MACH_ROBOT_ARM       = 10,
    SMF_MACH_3D_PRINTER      = 11,
    SMF_MACH_INJECTION_MOLD  = 12,
    SMF_MACH_PRESS           = 13,
    SMF_MACH_HEAT_TREAT      = 14,
    SMF_MACH_PAINT_BOOTH     = 15
} SMF_MachineType;

typedef enum {
    SMF_MACH_IDLE          = 0,
    SMF_MACH_RUNNING       = 1,
    SMF_MACH_SETUP         = 2,
    SMF_MACH_BREAKDOWN     = 3,
    SMF_MACH_MAINTENANCE   = 4,
    SMF_MACH_STANDBY       = 5,
    SMF_MACH_OFFLINE       = 6
} SMF_MachineState;

typedef struct {
    char            code[SMF_CODE_LEN];
    char            name[SMF_NAME_LEN];
    SMF_MachineType type;
    SMF_MachineState state;
    int             workstation_idx;
    float           max_speed;
    float           nominal_power_kw;
    float           uptime_percent;
    float           mtbf_hours;
    float           mttr_hours;
    time_t          last_maintenance;
    int             maintenance_interval_hours;
    float           accumulated_runtime;
    int             total_cycles;
    int             good_parts;
    int             reject_parts;
    bool            is_critical;
    bool            requires_operator;
    float           setup_time_minutes;
    float           cycle_time_minutes;
    float           oee_availability;
    float           oee_performance;
    float           oee_quality;
    float           oee_overall;
} SMF_Machine;

typedef struct {
    char    code[SMF_CODE_LEN];
    char    name[SMF_NAME_LEN];
    char    type[32];
    int     machine_idx;
    float   max_life_hours;
    float   accumulated_hours;
    float   wear_percent;
    time_t  last_calibration;
    int     calibration_interval_hours;
    bool    is_active;
} SMF_Tool;

typedef enum {
    SMF_MAT_RAW           = 0,
    SMF_MAT_WIP           = 1,
    SMF_MAT_SEMI_FINISHED = 2,
    SMF_MAT_FINISHED      = 3,
    SMF_MAT_CONSUMABLE    = 4,
    SMF_MAT_PACKAGING     = 5
} SMF_MaterialType;

typedef struct {
    char            code[SMF_CODE_LEN];
    char            name[SMF_NAME_LEN];
    SMF_MaterialType type;
    char            unit[8];
    float           quantity_on_hand;
    float           quantity_reserved;
    float           quantity_on_order;
    float           reorder_point;
    float           safety_stock;
    float           economic_order_qty;
    float           unit_cost;
    float           lead_time_days;
    int             supplier_idx;
    bool            is_critical;
    char            location[32];
    time_t          last_received;
} SMF_Material;

typedef struct {
    char    code[SMF_CODE_LEN];
    char    name[SMF_NAME_LEN];
    char    revision[16];
    int     routing_idx;
    int     bom_idx;
    float   standard_cost;
    float   selling_price;
    float   weight_kg;
    char    unit_of_measure[8];
    int     min_lot_size;
    int     max_lot_size;
    bool    is_make_to_order;
    bool    is_engineer_to_order;
    int     lead_time_days;
    int     current_revision_no;
    time_t  revision_date;
    bool    is_active;
} SMF_Product;

typedef struct {
    int     material_idx;
    int     parent_product_idx;
    float   quantity_per;
    float   scrap_percent;
    int     level;
    int     sequence;
    bool    is_phantom;
    bool    is_critical_component;
    char    reference_designator[16];
    int     alt_material_idx;
} SMF_BOMItem;

typedef struct {
    char        code[SMF_CODE_LEN];
    char        description[SMF_DESC_LEN];
    SMF_BOMItem items[SMF_MAX_BOM_ITEMS];
    int         num_items;
    int         product_idx;
    int         revision;
    bool        is_approved;
    time_t      effective_date;
    time_t      obsolete_date;
} SMF_BillOfMaterials;

typedef enum {
    SMF_OP_MACHINING      = 0,
    SMF_OP_ASSEMBLY       = 1,
    SMF_OP_INSPECTION     = 2,
    SMF_OP_HEAT_TREAT     = 3,
    SMF_OP_SURFACE_FINISH = 4,
    SMF_OP_PACKAGING      = 5,
    SMF_OP_QUALITY_GATE   = 6,
    SMF_OP_TRANSPORT      = 7,
    SMF_OP_WAIT_QUEUE     = 8
} SMF_OperationType;

typedef struct {
    SMF_OperationType type;
    char             name[SMF_NAME_LEN];
    int              workstation_idx;
    int              machine_type_req;
    int              tool_idx;
    float            setup_time_min;
    float            run_time_per_unit_min;
    float            queue_time_avg_min;
    int              sequence_no;
    bool             is_bottleneck;
    bool             requires_inspection;
    int              inspection_plan_idx;
    int              next_step;
    int              prev_step;
    int              alt_step;
} SMF_RouteStep;

typedef struct {
    char          code[SMF_CODE_LEN];
    char          description[SMF_DESC_LEN];
    SMF_RouteStep steps[SMF_MAX_ROUTE_STEPS];
    int           num_steps;
    int           product_idx;
    int           revision;
    float         total_cycle_time_min;
    float         total_setup_time_min;
    int           bottleneck_step_idx;
    bool          is_approved;
    time_t        effective_date;
} SMF_Routing;

typedef struct {
    char    code[SMF_CODE_LEN];
    char    name[SMF_NAME_LEN];
    char    department[64];
    int     machine_indices[32];
    int     num_machines;
    int     operator_count;
    int     shift_count;
    float   capacity_per_hour;
    float   buffer_capacity;
    float   current_wip_count;
    float   uptime_target;
    bool    is_cellular;
    int     parent_line_idx;
} SMF_Workstation;

typedef struct {
    char    code[SMF_CODE_LEN];
    char    name[SMF_NAME_LEN];
    int     workstation_indices[32];
    int     num_workstations;
    float   line_capacity_per_hour;
    float   current_takt_time_min;
    float   target_takt_time_min;
    float   line_balance_efficiency;
    bool    is_u_shaped;
    int     bottleneck_ws_idx;
} SMF_ProductionLine;

typedef enum {
    SMF_WO_PLANNED      = 0,
    SMF_WO_RELEASED     = 1,
    SMF_WO_IN_PROGRESS  = 2,
    SMF_WO_COMPLETED    = 3,
    SMF_WO_CLOSED       = 4,
    SMF_WO_ON_HOLD      = 5,
    SMF_WO_CANCELLED    = 6
} SMF_WorkOrderState;

typedef struct {
    char              wo_number[SMF_CODE_LEN];
    int               product_idx;
    int               routing_idx;
    int               bom_idx;
    int               quantity_ordered;
    int               quantity_completed;
    int               quantity_scrapped;
    SMF_WorkOrderState state;
    time_t            planned_start;
    time_t            planned_end;
    time_t            actual_start;
    time_t            actual_end;
    int               current_step_idx;
    int               priority;
    int               customer_order_idx;
    float             estimated_cost;
    float             actual_cost;
    char              released_by[32];
    bool              is_outsourced;
    int               outsourced_vendor_idx;
} SMF_WorkOrder;

typedef struct {
    char              name[SMF_NAME_LEN];
    char              location[SMF_DESC_LEN];
    SMF_Machine       machines[SMF_MAX_MACHINES];
    int               num_machines;
    SMF_Workstation   workstations[SMF_MAX_WORKSTATIONS];
    int               num_workstations;
    SMF_ProductionLine lines[64];
    int               num_lines;
    SMF_Tool          tools[SMF_MAX_TOOLS];
    int               num_tools;
    SMF_Material      materials[SMF_MAX_MATERIALS];
    int               num_materials;
    SMF_Product       products[SMF_MAX_PRODUCTS];
    int               num_products;
    SMF_BillOfMaterials boms[256];
    int               num_boms;
    SMF_Routing       routings[256];
    int               num_routings;
    SMF_WorkOrder     *work_orders[SMF_MAX_WORK_ORDERS];
    int               num_work_orders;
    float             total_floor_area_sqm;
    int               total_employees;
    int               shifts_per_day;
    float             hours_per_shift;
    float             days_per_week;
    bool              is_operating;
    time_t            established_date;
} SMF_Factory;

/* API: Factory Lifecycle */
SMF_Factory* smf_factory_create(const char *name, const char *location);
void smf_factory_destroy(SMF_Factory *factory);
float smf_factory_capacity(const SMF_Factory *factory);

/* API: Machine Management */
int  smf_add_machine(SMF_Factory *factory, const char *code,
                     const char *name, SMF_MachineType type);
int  smf_machine_set_state(SMF_Factory *factory, int machine_idx,
                            SMF_MachineState new_state);
int  smf_machine_record_production(SMF_Factory *factory, int machine_idx,
                                    int good, int reject);
float smf_machine_mtbf(const SMF_Machine *machine);
float smf_machine_mttr(const SMF_Machine *machine);
int  smf_find_bottleneck(const SMF_Factory *factory, int *machine_indices,
                          int count);

/* API: Material Management */
int  smf_add_material(SMF_Factory *factory, const char *code,
                      const char *name, SMF_MaterialType type,
                      const char *unit, float unit_cost);
int  smf_material_receive(SMF_Factory *factory, int material_idx, float quantity);
int  smf_material_issue(SMF_Factory *factory, int material_idx, float quantity);
float smf_calc_eoq(float annual_demand, float order_cost,
                    float holding_cost_per_unit);
bool smf_material_needs_reorder(const SMF_Material *material);
float smf_inventory_turnover(const SMF_Factory *factory);

/* API: Product & BOM */
int  smf_add_product(SMF_Factory *factory, const char *code,
                     const char *name, const char *revision);
int  smf_add_bom(SMF_Factory *factory, const char *code, int product_idx);
int  smf_bom_add_item(SMF_Factory *factory, int bom_idx,
                      int material_idx, float quantity_per, int level);
int  smf_bom_explode(const SMF_Factory *factory, int product_idx,
                     int quantity, float *total_material_qty, int max_materials);
float smf_product_standard_cost(const SMF_Factory *factory, int product_idx);

/* API: Routing & Workstation */
int  smf_add_workstation(SMF_Factory *factory, const char *code,
                         const char *name, const char *department);
int  smf_add_routing(SMF_Factory *factory, const char *code, int product_idx);
int  smf_routing_add_step(SMF_Factory *factory, int routing_idx,
                          SMF_OperationType type, const char *name,
                          int workstation_idx, float setup_time,
                          float run_time, int sequence);
float smf_routing_cycle_time(const SMF_Routing *routing);
float smf_line_balance_efficiency(const SMF_ProductionLine *line,
                                   const SMF_Factory *factory);

/* Utility */
const char* smf_machine_type_name(SMF_MachineType type);
const char* smf_machine_state_name(SMF_MachineState state);
const char* smf_material_type_name(SMF_MaterialType type);
const char* smf_wo_state_name(SMF_WorkOrderState state);
const char* smf_op_type_name(SMF_OperationType type);

#endif