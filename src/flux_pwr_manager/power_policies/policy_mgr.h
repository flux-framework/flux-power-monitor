#ifndef FLUX_PWR_MANAGER_POWER_POLICY_MANAGER_H
#define FLUX_PWR_MANAGER_POWER_POLICY_MANAGER_H
#include "pwr_stats.h"
#include "retro_queue_buffer.h"
#include "power_policy.h"
#include "file_logger.h"
typedef struct power_strategy pwr_policy_t;

typedef struct power_strategy {
  double (*get_powercap)(pwr_policy_t *mgr,double powerlimit, double old_powercap, void *data);
  double (*get_power_dist)(int num_of_elements, pwr_stats_t *stats,
                        double pwr_avail, double pwr_used, double *result);
  retro_queue_buffer_t* powercap_history;  
  retro_queue_buffer_t* powerlimit_history;  
  retro_queue_buffer_t* time_history;
  int deviceId;
  Logger *file_log; // Reference to node manager file logger. Don't delete, Do not own this
  bool converged;
} pwr_policy_t;
pwr_policy_t *pwr_policy_new(POWER_POLICY_TYPE policy_type, Logger *log,int deviceId);
void  pwr_policy_destroy(pwr_policy_t **pwr_mgr);
#endif
