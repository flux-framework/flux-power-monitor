#ifndef FLUX_PWR_MANAGER_NODE_MANAGER_H
#define FLUX_PWR_MANAGER_NODE_MANAGER_H
#include "node_data.h"
#include <flux/core.h>
// Node manager lives at each node and its lifecycle is tied to the flux broker module.


void node_flux_powerlimit_rpc_cb(flux_t *h, flux_msg_handler_t *mh,
                              const flux_msg_t *msg, void *arg);
int node_manager_destructor();
int  node_manager_init(flux_t* h,uint32_t rank,uint32_t size,char* hostname,size_t buffer_size,size_t sampling_rate);
#endif
