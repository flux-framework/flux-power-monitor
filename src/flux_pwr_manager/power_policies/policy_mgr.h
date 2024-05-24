#ifndef FLUX_PWR_MANAGER_POWER_POLICY_MANAGER_H
#define FLUX_PWR_MANAGER_POWER_POLICY_MANAGER_H
#include "pwr_stats.h"
typedef struct power_strategy {
  double (*get_powercap)(double powerlimit, double old_powercap, void *data);
  double (*get_power_dist)(int num_of_elements, pwr_stats_t *stats,
                        double pwr_avail, double pwr_used, double *result);
} pwr_policy_t;
#endif
