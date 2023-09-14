#ifndef FLUX_NODE_POWER_PROFILE_H
#define FLUX_NODE_POWER_PROFILE_H
#include "circular_buffer.h"
#include "device_power_info.h"
#include "node_capabilities.h"
#include "power_data.h"
#include "power_policies/power_policy.h"
typedef struct {
  char *hostname;
  int num_of_gpus;
  int num_of_socket; // applies to its devices.
  int num_of_cpus;
  double power_agg;
  double max_power;
  double min_power;
  POWER_POLICY_TYPE
  node_current_power_policy; // This power Policy is applied by the JOB.
  int total_num_of_devices;
  double power_current;
  size_t history_size;
  double powercap;
  //IN Lassen systems only node level powercap allowed. To accomodate this using this flag.
  bool powercap_allowed;
  double powerlimit;
  POWER_POLICY_TYPE device_power_policy; // This is the power policy this node
  device_power_profile **device_list;
  circular_buffer_t *power_history;
} node_power_profile;

node_power_profile *node_power_profile_new(char *hostname,
                                           size_t node_history_size);
void node_current_power_policy_set(node_power_profile *node,
                                   POWER_POLICY_TYPE node_power_policy);
int node_device_power_update(node_power_profile *node, power_data **data,
                             int num_of_devices);
int node_device_list_init(node_power_profile *node, node_capabilities *d_data,
                          size_t device_history_size);
int node_power_update(node_power_profile *node, power_data *node_power);
void node_power_profile_destroy(node_power_profile *node);

#endif
