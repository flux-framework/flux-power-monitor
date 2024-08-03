#ifndef FLUX_PWR_MANAGER_NODE_MANAGER_H
#define FLUX_PWR_MANAGER_NODE_MANAGER_H
#include "node_data.h"
#include <flux/core.h>
#include "parse_util.h"
// Node manager lives at each node and its lifecycle is tied to the flux broker
// module.
// Power wise Node manager should independently do power management for each
// node. Every Thirty seconds it should check whether it needs to adjust power
// or not.
//
int node_manager_destructor();
int node_manager_init(flux_t *h, uint32_t rank, uint32_t size, char *hostname,
                      size_t buffer_size, size_t sampling_rate);

void node_manager_set_pl_cb(flux_t *h, flux_msg_handler_t *mh,
                            const flux_msg_t *msg, void *args);
void node_manager_new_job_cb(flux_t *h, flux_msg_handler_t *mh,
                             const flux_msg_t *msg, void *args);
void node_manager_end_job_cb(flux_t *h, flux_msg_handler_t *mh,
                             const flux_msg_t *msg, void *args);


/**
 * @brief RPC callback for disabling dynamic power management for node manager.
 * By default disable dynamic power mangement
 */
void node_manager_disable_pm_cb(flux_t *h, flux_msg_handler_t *mh,
                             const flux_msg_t *msg, void *args);
/**
 * @brief This function is required as RPC calls in situation where there is one
 * broker and a self RPC call is made. This won't work. A work around we call
 * the method manually then.
 *
 *
 * @param jobId flux jobId 
 * @param job_cwd current working directory of the job
 * @param job_name name of the job
 * @param device_info job's device info for this node
 * @return
 */
int node_manager_new_job(uint64_t jobId, char *job_cwd, char *job_name,
                         node_device_info_t *device_info);
/**
 * @brief
 *
 * @param jobId
 * @return
 */
int node_manager_finish_job(uint64_t jobId);

/**
 * @brief This method set the powerlimit for the specified jobid.
 *
 * @param jobId is the jobId of the job.
 * @param powerlimit Is the powerlimit.
 * @param deviceId device whose power is capped.
 * @return
 */
int node_manager_set_powerlimit(uint64_t jobid,double powerlimit, int deviceId);
/**
 * @brief
 *
 * @param power_ratio
 * @return
 */
int node_manager_set_power_ratio(int power_ratio);

/**
 * @brief called by the main module on each node. Does power capping on devices
 * as required. Is a periodic method where, each job's device will have maximum
 * power and we try to minimize the power draw.
 *
 * @return
 */
void node_manager_manage_power();
void node_manager_print_fft_result(void);

#endif
