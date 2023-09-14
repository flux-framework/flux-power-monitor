#ifndef FLUX_POWER_POLICY_MANAGER_H
#define FLUX_POWER_POLICY_MANAGER_H
#include "device_power_info.h"
#include "dynamic_job_map.h"
#include "job_data.h"
#include "node_power_info.h"
typedef struct power_strategy {
  double (*get_device_powercap)(device_power_profile *);
  // double (*get_job_powercap)(job_data *);
  double (*get_node_powercap)(node_power_profile *);
  void (*set_job_power_policy)(dynamic_job_map *);
  void (*set_device_power_policy)(node_power_profile *);
  void (*set_node_power_policy)(job_data *);
  void (*set_job_power_distribution)(dynamic_job_map *, double);
  void (*set_node_power_distribution)(job_data *, double);
  void (*set_device_power_distribution)(node_power_profile *, double);
} power_strategy;
#endif
