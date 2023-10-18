#ifndef FLUX_PWR_MANAGER_POWER_UTIL_H
#define FLUX_PWR_MANAGER_POWER_UTIL_H
#include "dynamic_job_map.h"
#include "job_data.h"
#include "power_policies/power_policy_manager.h"
void set_global_power_strategy(dynamic_job_map *dynamic_job_data,
                               power_strategy *current_power_strategy,
                               double global_power_budget);
void set_job_power_strategy(job_data *job_data,
                            power_strategy *current_power_strategy,
                            double job_power_budget);
void set_node_power_strategy(node_power_profile *node_data,
                             power_strategy *current_power_strategy,
                             double node_power_budget);
void update_node_powercap(node_power_profile *node_data);
void update_job_powercap(job_data *job_data);
#endif
