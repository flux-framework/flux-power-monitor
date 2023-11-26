#ifndef FLUX_PWR_MANAGER_NODE_MANAGER_H
#define FLUX_PWR_MANAGER_NODE_MANAGER_H
#include "node_data.h"
#include <flux/core.h>
// Node manager lives at each node and its lifecycle is tied to the flux broker
// module.

int node_manager_destructor();
int node_manager_init(flux_t *h, uint32_t rank, uint32_t size, char *hostname,
                      size_t buffer_size, size_t sampling_rate);

void node_manager_set_pl_cb(flux_t *h, flux_msg_handler_t *mh,
                            const flux_msg_t *msg, void *args);
void node_manager_new_job_cb(flux_t *h, flux_msg_handler_t *mh,
                             const flux_msg_t *msg, void *args);
void node_manager_end_job_cb(flux_t *h, flux_msg_handler_t *mh,
                             const flux_msg_t *msg, void *args);
void nm_test();
/**
 * @brief This function is required as RPC calls in situation where there is one
 * broker and a self RPC call is made. This won't work. A work around we call
 * the method manually then.
 *
 *
 * @param jobId
 * @param job_cwd
 * @param job_name
 * @return
 */
int node_manager_new_job(uint64_t jobId, char *job_cwd, char *job_name);
int node_manager_finish_job(uint64_t jobId) ;

int node_manager_set_powerlimit(double powerlimit) ;
#endif
