#ifndef FLUX_NODE_POWER_PROFILE_H
#define FLUX_NODE_POWER_PROFILE_H
#include "circular_buffer.h"
#include "power_data.h"
#include "power_policy.h"
typedef struct {
  circular_buffer_t *node_power_history;
  size_t node_history_size;
  char *hostname;
  POWER_POLICY
  node_current_power_policy;        // This power Policy is applied by the JOB.
  POWER_POLICY device_power_policy; // This is the power policy this node
                                    // applies to its devices.
  power_data *node_power_latest;
  power_data *node_power_agg;
} node_power_profile;
node_power_profile *node_power_profile_new(char *hostname,
                                           size_t node_history_size);
void set_node_current_power_policy(node_power_profile *node,
                                   POWER_POLICY node_power_policy);
void set_device_power_profile(node_power_profile *node,
                              POWER_POLICY device_power_policy);
// This function add data to node_power_history and also updates the struct's
// node_power_latest and node_power_agg
int add_power_data_to_node_history(node_power_profile *node, power_data *data);
void node_power_profile_destroy(node_power_profile *node);
#endif
