#ifndef FLUX_POWER_CAPPING_UTIL_H
#define FLUX_POWER_CAPPING_UTIL_H
#include "power_data.h"
#include "job_data.h"
#include "dynamic_job_map.h"
#include "power_policy.h"
void get_job_level_powercap(dynamic_job_map *job_map,double global_power_budget, POWER_POLICY power_policy);
double get_power_cap(job_data* data);
#endif

