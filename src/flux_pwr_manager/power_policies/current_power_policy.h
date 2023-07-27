#ifndef FLUX_CURRENT_POWER_POLICY_H
#define FLUX_CURRENT_POWER_POLICY_H
#include "job_data.h"
double current_power_policy_get_job_powercap(job_data* job_data);
double current_power_policy_get_device_powercap(device_power_profile* device_data);
double current_power_policy_get_node_powercap(node_power_profile* node_data);
#endif
