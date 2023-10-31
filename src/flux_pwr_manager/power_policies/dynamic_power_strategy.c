#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "dynamic_power_policy.h"
#include "dynamic_power_strategy.h"
#include <math.h>
static void set_device_power_policy(node_power_profile *node_data) {
  if (node_data == NULL)
    return;
  for (int i = 0; i < node_data->total_num_of_devices; i++) {
    if (node_data->device_list[i] != NULL)
      node_data->device_list[i]->device_power_policy = DYNAMIC_POWER;
  }
}
static void set_node_power_policy(job_data *job_data) {
  if (job_data == NULL)
    return;
  for (int i = 0; i < job_data->num_of_nodes; i++) {
    if (job_data->node_power_profile_data[i] != NULL)
      job_data->node_power_profile_data[i]->node_current_power_policy =
          DYNAMIC_POWER;
  }
}
static void set_job_power_policy(dynamic_job_map *job_map_data) {
  if (job_map_data == NULL)
    return;
  for (int i = 0; i < job_map_data->size; i++) {
    job_map_data->entries[i].data->current_power_policy = DYNAMIC_POWER;
  }
}

static double get_job_powercap(job_data *job_data) {
  if (job_data == NULL)
    return -1;
  return dynamic_power_policy_get_job_powercap(job_data);
}
// Right now this strategy will only equally distrbiute the power to each job
// with each node in the job as a basis
static void set_job_power_distribution(dynamic_job_map *job_map,
                                double global_power_cap) {
  if (job_map == NULL)
    return;
  if (global_power_cap == 0)
    return;
  for (int i = 0; i < job_map->size; i++) {
    if (job_map->entries[i].data == NULL)
      return;
    int total_num_nodes = 0;
    for (int i = 0; i < job_map->size; i++) {
      job_data *j_data = job_map->entries[i].data;
      if (j_data == NULL)
        return;
      total_num_nodes += j_data->num_of_nodes;
    }

    for (int i = 0; i < job_map->size; i++)

      job_map->entries[i].data->powerlimit =
          global_power_cap *
          (int)(job_map->entries[i].data->num_of_nodes / total_num_nodes);

    // Setting the power cap to the maximum allowed when a job is initalized.
    if (job_map->entries[i].data->powercap == 0.0)
      job_map->entries[i].data->powercap =
          job_map->entries[i].data->powerlimit;
  }
}
// Right now this strategy will only equally distrbiute the power to each node
static void set_node_power_distribution(job_data *job_data, double job_power_cap) {
  if (job_data == NULL)
    return;
  if (job_power_cap == 0)
    return;
  for (int i = 0; i < job_data->num_of_nodes; i++) {
    if (job_data->node_power_profile_data[i] == NULL)
      return;
    job_data->node_power_profile_data[i]->powerlimit =

        job_power_cap / job_data->num_of_nodes;
    if (job_data->node_power_profile_data[i]->powercap == 0.0)
      job_data->node_power_profile_data[i]->powercap =
          job_data->node_power_profile_data[i]->powerlimit;
  }
}

// Right now this strategy will only equally distrbiute the power to each
// device.
static void set_device_power_distribution(node_power_profile *node_data,
                                   double node_cap) {
  if (node_data == NULL)
    return;
  if (node_cap == 0)
    return;
  int powercap_allowed_devices = 0;
  // Only distribute to devices where actual powercap is allowed.
  for (int i = 0; i < node_data->total_num_of_devices; i++) {
    if (node_data->device_list[i]->powercap_allowed)
      powercap_allowed_devices++;
  }
  // Right now when distrbiuting power inside a node, remove power of constant
  // elements.
  for (int i = 0; i < node_data->total_num_of_devices; i++) {
    if (!node_data->device_list[i]->powercap_allowed)
      node_cap -= node_data->device_list[i]->current_power;
  }
  for (int i = 0; i < node_data->total_num_of_devices; i++) {
    if (node_data->device_list[i] == NULL)
      return;
    if (node_data->device_list[i]->powercap_allowed) {
      node_data->device_list[i]->powerlimit =
          node_cap / powercap_allowed_devices;
    } else {
      node_data->device_list[i]->powerlimit = -1;
    }
    if (node_data->device_list[i]->powercap == 0.0)
      node_data->device_list[i]->powercap =
          node_data->device_list[i]->powerlimit;
  }
}
static double get_device_powercap(device_power_profile *device_data) {
  if (device_data == NULL)
    return -1;
  if (device_data->powerlimit == -1)
    return 0;
  return dynamic_power_policy_get_device_powercap(device_data);
}
 static double get_node_powercap(node_power_profile *node_data) {
  if (node_data == NULL)
    return -1;
  return dynamic_power_policy_get_node_powercap(node_data);
}
void dynamic_power_policy_init(power_strategy *policy) {
  if (policy == NULL)
    return;
  policy->set_device_power_policy = set_device_power_policy;
  policy->set_job_power_policy = set_job_power_policy;
  policy->set_node_power_policy = set_node_power_policy;
  policy->get_node_powercap = get_node_powercap;
  // policy->get_job_powercap = get_job_powercap;
  policy->get_device_powercap = get_device_powercap;
  policy->set_device_power_distribution = set_device_power_distribution;
  policy->set_job_power_distribution = set_job_power_distribution;
  policy->set_node_power_distribution = set_node_power_distribution;
}