#ifndef FLUX_NODE_POWER_PROFILE_H
#define FLUX_NODE_POWER_PROFILE_H
#include "circular_buffer.h"
#include "device_power_info.h"
#include "power_data.h"
#include "power_policy.h"
typedef struct {
  char *hostname;
  int num_of_gpus;
  int num_of_socket; // applies to its devices.
  double node_power_agg;
  POWER_POLICY
  node_current_power_policy; // This power Policy is applied by the JOB.
  int total_num_of_devices;
  double node_power_latest;
  size_t node_history_size;
  double node_power_cap;
  POWER_POLICY device_power_policy; // This is the power policy this node
  device_power_profile **device_list;
  circular_buffer_t *node_power_history;
} node_power_profile;
node_power_profile *node_power_profile_new(char *hostname,
                                           size_t node_history_size);
void node_current_power_policy_set(node_power_profile *node,
                                   POWER_POLICY node_power_policy);
int node_device_power_update(node_power_profile *node, power_data **data,
                             int num_of_devices);
int node_device_list_init(node_power_profile *node, int num_of_sockets,
                          int num_of_gpus, bool mem, power_data **data,
                          int data_size,size_t device_history_size);
int node_power_update(node_power_profile *node, power_data *node_power);
void node_power_profile_destroy(node_power_profile *node);

#endif
