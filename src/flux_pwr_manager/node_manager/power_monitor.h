#ifndef FLUX_PWR_MANAGER_POWER_MONITOR_H
#define FLUX_PWR_MANAGER_POWER_MONITOR_H
#include "node_job_info.h"
#include <flux/core.h>
void flux_pwr_monitor_request_power_data_from_node(flux_t *h,
                                                   flux_msg_handler_t *mh,
                                                   const flux_msg_t *msg,
                                                   void *arg);
void power_monitor_init(size_t temp_buffer_size);
void power_monitor_destructor();
int power_monitor_set_node_power_ratio(int power);
int power_monitor_set_node_powercap(double powercap,int gpu_id);
void power_monitor_start_job(uint64_t jobId, char *job_cwd, char *job_name);
void power_monitor_end_job();
#endif
