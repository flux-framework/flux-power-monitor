#ifndef FLUX_PWR_MANAGER_POWER_MONITOR_H
#define FLUX_PWR_MANAGER_POWER_MONITOR_H
#include <flux/core.h>
void flux_pwr_monitor_request_power_data_from_node(flux_t *h,
                                                   flux_msg_handler_t *mh,
                                                   const flux_msg_t *msg,
                                                   void *arg);
void power_monitor_init();
void power_monitor_destructor();
void power_monitor_start_job(uint64_t jobId);
void power_monitor_end_job();
#endif
