# Knowledge Graph — mini-agriculture-tech

## L1: Core Definitions (Complete)

| Concept | C Type | API |
|---------|--------|-----|
| GPS Coordinate | agri_gps_t | agri_make_gps(), agri_gps_distance_m() |
| Field Boundary | agri_field_t | agri_polygon_area_ha() |
| Soil Layer | agri_soil_layer_t | agri_soil_add_layer() |
| Soil Profile | agri_soil_profile_t | agri_soil_init(), agri_soil_awc_mm() |
| Crop Type | agri_crop_type_t (enum) | agri_crop_lookup() |
| Growth Stage | agri_growth_stage_t (enum) | (used in agri_season_step) |
| Crop Parameters | agri_crop_t | agri_crop_lookup() |
| Weather Day | agri_weather_day_t | agri_weather_add_day() |
| Weather Series | agri_weather_series_t | agri_weather_init() |
| Season Config | agri_season_config_t | agri_season_init() |
| Daily State | agri_daily_state_t | (internal to agri_season_step) |
| Season Context | agri_season_context_t | agri_season_simulate(), agri_predict_yield() |
| Irrigation Event | agri_irrigation_event_t | agri_irrigation_optimize() |
| Pest Model | agri_pest_model_t | agri_pest_init(), agri_pest_add_model() |
| Farm | agri_farm_t | agri_farm_init(), agri_farm_simulate_all() |
| USLE Factors | agri_usle_factors_t | agri_usle_soil_loss_ton_ha_yr() |
| Fertilizer Plan | agri_fertilizer_plan_t | agri_fertilizer_plan() |
| Soil Test | agri_soil_test_t | (input to agri_fertilizer_plan) |
| Rotation Plan | agri_rotation_plan_t | agri_rotation_optimize() |
| Carbon Farm | agri_carbon_farm_t | agri_carbon_farm_assess() |
| WGEN Params | agri_wgen_params_t | agri_wgen_generate() |
| Nitrogen Pool | agri_nitrogen_pool_t | agri_nitrogen_step() |
| Cost Breakdown | agri_cost_breakdown_t | agri_economic_analyze() |
| Economic Analysis | agri_economic_analysis_t | agri_economic_analyze() |
| Harvest Model | agri_harvest_model_t | agri_harvest_model() |
| Climate Scenario | agri_climate_scenario_t | agri_climate_impact_assess() |
| Insurance Policy | agri_insurance_policy_t | agri_insurance_quote() |
| Water Quality | agri_water_quality_t | agri_water_quality_estimate() |
| Tillage System | agri_tillage_system_t | agri_tillage_evaluate() |
| Yield Map | agri_yield_map_t | agri_yield_map_init(), agri_vrt_generate_prescription() |
| Grain Quality | agri_grain_quality_t | agri_grain_quality_assess() |
| Intercrop System | agri_intercrop_system_t | agri_intercrop_evaluate() |

## L2: Core Concepts (Complete)

| Concept | Implementation | Location |
|---------|---------------|----------|
| Water Balance | Soil water dynamics with ET, rain, irrigation | agri_season_step() |
| Crop Phenology | GDD-based growth stage progression | agri_season_step() |
| Biomass Accumulation | RUE (Radiation Use Efficiency) model | agri_season_step() |
| Nitrogen Cycle | Mineralization, nitrification, denitrification, leaching | agri_nitrogen_step() |
| Soil Physics | Rawls pedotransfer functions | agri_soil_add_layer() |
| Evapotranspiration | FAO-56 Penman-Monteith | agri_weather_et0_penman_monteith() |
| Crop Water Stress | FAO-56 water stress coefficient Ks | water_stress() (static) |
| Temperature Stress | Linear temperature response function | temp_stress() (static) |
| Soil Erosion | USLE/RUSLE framework | agri_usle_* family |
| Pest Epidemiology | Environmental suitability + degree-day model | agri_pest_predict() |
| Precipitation Generation | 1st-order Markov chain + gamma distribution | agri_wgen_generate() |
| Yield Response | Stewart water production function | agri_irrigation_deficit_irrigation() |

## L3: Engineering Structures (Complete)

| Structure | Description |
|-----------|-------------|
| Daily-step simulation engine | agri_season_step(): single-day crop growth with water/temp/N stress |
| Multi-field farm manager | agri_farm_t: aggregates fields, runs simulations, optimizes planting |
| MAD irrigation scheduler | agri_irrigation_optimize(): event-based scheduling with deficit tracking |
| Pest risk predictor | agri_pest_predict(): multi-model risk assessment framework |
| Nitrogen pool tracker | agri_nitrogen_step(): 6-process nitrogen transformation model |
| WGEN stochastic engine | agri_wgen_generate(): autoregressive weather generation |
| VRT yield mapper | agri_yield_map_t: grid-based spatial data with zone clustering |
| Tillage system evaluator | agri_tillage_evaluate(): 5-system comparison framework |

## L4: Standards/Theorems (Complete)

| Theorem/Standard | Formula | Verification |
|-----------------|---------|-------------|
| FAO-56 Penman-Monteith | ET0 = (0.408*delta*(Rn-G) + gamma*(900/(T+273))*u2*(es-ea)) / (delta + gamma*(1+0.34*u2)) | agri_weather_et0_penman_monteith() |
| USLE | A = R * K * LS * C * P | agri_usle_soil_loss_ton_ha_yr() |
| Stewart Water Production | 1 - Ya/Ym = Ky * (1 - ETa/ETm) | agri_irrigation_deficit_irrigation() |
| Land Equivalent Ratio | LER = Y_ab/Y_aa + Y_ba/Y_bb | agri_land_equivalent_ratio() |
| Rawls Pedotransfer | theta_FC = 0.2576 - 0.002*S + 0.0036*C + 0.0299*OM | agri_soil_add_layer() |
| Curve Number Runoff | Q = (P - 0.2S)^2 / (P + 0.8S) | agri_water_quality_estimate() |
| Stanford Mineralization | dN/dt = k * f(T) * f(W) * N_org (first-order) | agri_nitrogen_mineralization() |
| Q10 Temperature Correction | rate_T = rate_20 * 2^((T-20)/10) | Used in N cycle, residue decay |
| IPCC N2O Tier 1 | N2O-N = N_input * EF1 (EF1 = 1%) | agri_carbon_n2o_from_fertilizer() |
| IPCC SOC Tier 2 | delta_SOC = SOC_ref * (F_LU*F_MG*F_I - 1) / T | agri_tillage_carbon_impact() |
| Actuarial Premium | Premium = integral of indemnity * pdf(yield) | agri_insurance_actuarial_premium() |

## L5: Algorithms/Methods (Complete)

| Algorithm | Implementation | Complexity |
|-----------|---------------|------------|
| Haversine Distance | agri_gps_distance_m() | O(1) |
| Shoelace Polygon Area | agri_polygon_area_ha() | O(n) |
| Growing Degree Days | agri_weather_gdd_accum() | O(n) |
| RUE Biomass Model | agri_season_step() | O(1) per step |
| MAD Irrigation Scheduling | agri_irrigation_optimize() | O(n) |
| Greedy Crop-to-Field Allocation | agri_farm_optimize_planting() | O(F * C) |
| Greedy Rotation Sequencing | agri_rotation_optimize() | O(C^2) |
| Box-Muller Normal RNG | wgen_normal() | O(1) |
| Markov Chain Precipitation | agri_wgen_generate() | O(n) |
| First-Order Mineralization | agri_nitrogen_mineralization() | O(1) |
| Numerical Integration (insurance) | agri_insurance_actuarial_premium() | O(k) |
| Equal-Interval Zone Clustering | agri_yield_map_zones() | O(n) |
| Exponential Dry-Down | agri_harvest_dry_down() | O(1) |
| Exponential Storage Loss | agri_grain_storage_loss() | O(1) |

## L6: Canonical Problems (Complete)

| Problem | Example |
|---------|---------|
| Crop Yield Prediction | examples/example_farm_analysis.c |
| Irrigation Scheduling | examples/example_irrigation_scheduling.c |
| Farm-Level Optimization | examples/example_farm_analysis.c |
| Intercropping LER | examples/example_precision_vrt.c |
| Harvest Loss Modeling | examples/example_farm_analysis.c |

## L7: Applications (≥2) (Complete)

| Application | Coverage |
|-------------|----------|
| Precision Fertilizer (4R) | agri_fertilizer_plan(), agri_fertilizer_variable_rate() |
| Crop Insurance (RMA-style) | agri_insurance_quote(), agri_insurance_calculate_indemnity() |
| Economic B/E Analysis | agri_economic_analyze(), agri_economic_sensitivity() |
| Grain Quality Grades | agri_grain_quality_assess() |
| WGEN Climate Simulation | agri_wgen_generate(), agri_wgen_estimate_from_weather() |

## L8: Advanced Topics (≥1) (Complete)

| Topic | Coverage |
|-------|----------|
| Crop Rotation Optimization | agri_rotation_optimize() with greedy + local search |
| Climate Change Impact (AgMIP) | agri_climate_impact_assess(), agri_climate_co2_fertilization_effect() |
| Mycotoxin Risk (aflatoxin/DON) | agri_mycotoxin_aflatoxin_risk(), agri_mycotoxin_don_risk() |

## L9: Industry Frontiers (Complete)

| Topic | Coverage |
|-------|----------|
| Carbon Farming (IPCC AFOLU) | agri_carbon_farm_assess(), agri_carbon_sequestration_potential() |
| VRT Precision Agriculture | agri_yield_map_*, agri_vrt_generate_prescription() |
| No-Till SOC Modeling | agri_tillage_carbon_impact(), agri_tillage_evaluate() |
