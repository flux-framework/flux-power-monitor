#include <sys/ucontext.h>
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "current_power_policy.h"
#include "current_power_strategy.h"
#include <math.h>
void set_device_power_policy(node_power_profile *node_data) {
  if (node_data == NULL)
    return;
  for (int i = 0; i < node_data->total_num_of_devices; i++) {
    if (node_data->device_list[i] != NULL)
      node_data->device_list[i]->device_power_policy = CURRENT_POWER;
  }
}
void set_node_power_policy(job_data *job_data) {
  if (job_data == NULL)
    return;
  for (int i = 0; i < job_data->num_of_nodes; i++) {
    if (job_data->node_power_profile_data[i] != NULL)
      job_data->node_power_profile_data[i]->node_current_power_policy =
          CURRENT_POWER;
  }
}
void set_job_power_policy(dynamic_job_map *job_map_data) {
  if (job_map_data == NULL)
    return;
  for (int i = 0; i < job_map_data->size; i++) {
    job_map_data->entries[i].data->current_power_policy = CURRENT_POWER;
  }
}

double get_job_powercap(job_data *job_data) {
  if (job_data == NULL)
    return -1;
  return current_power_policy_get_job_powercap(job_data);
}
// Right now this strategy will only equally distrbiute the power to each job.
void set_job_power_distribution(dynamic_job_map *job_map,
                                double global_power_cap) {
  if (job_map == NULL)
    return;
  if (global_power_cap == 0)
    return;
  for (int i = 0; i < job_map->size; i++) {
    if (job_map->entries[i].data == NULL)
      return;
    job_map->entries[i].data->max_powercap = global_power_cap / job_map->size;
    // Setting the power cap to the maximum allowed when a job is initalized.
    if (job_map->entries[i].data->powercap == 0)
      job_map->entries[i].data->powercap =
          job_map->entries[i].data->max_powercap;
  }
}
// Right now this strategy will only equally distrbiute the power to each node.
void set_node_power_distribution(job_data *job_data, double job_power_cap) {
  if (job_data == NULL)
    return;
  if (job_power_cap == 0)
    return;
  for (int i = 0; i < job_data->num_of_nodes; i++) {
    if (job_data->node_power_profile_data[i] == NULL)
      return;
    job_data->node_power_profile_data[i]->max_powercap =

        job_power_cap / job_data->num_of_nodes;
    if (job_data->node_power_profile_data[i]->powercap == 0)
      job_data->node_power_profile_data[i]->powercap =
          job_data->node_power_profile_data[i]->max_powercap;
  }
}

// Right now this strategy will only equally distrbiute the power to each
// device.
void set_device_power_distribution(node_power_profile *node_data,
                                   double node_cap) {
  if (node_data == NULL)
    return;
  if (node_cap == 0)
    return;
  for (int i = 0; i < node_data->total_num_of_devices; i++) {
    if (node_data->device_list[i] == NULL)
      return;
    node_data->device_list[i]->max_powercap =
        node_cap / node_data->total_num_of_devices;
    if (node_data->device_list[i]->powercap == 0)
      node_data->device_list[i]->powercap =
          node_data->device_list[i]->max_powercap;
  }
}
double get_device_powercap(device_power_profile *device_data) {
  if (device_data == NULL)
    return -1;
  return current_power_policy_get_device_powercap(device_data);
}
double get_node_powercap(node_power_profile *node_data) {
  if (node_data == NULL)
    return -1;
  return current_power_policy_get_node_powercap(node_data);
}
void current_power_policy_init(power_strategy *policy) {
  if (policy == NULL)
    return;
  policy->set_device_power_policy = set_device_power_policy;
  policy->set_job_power_policy = set_job_power_policy;
  policy->set_node_power_policy = set_node_power_policy;
  // policy->get_node_powercap = get_node_powercap;
  // policy->get_job_powercap = get_job_powercap;
  policy->get_device_powercap = get_device_powercap;
  policy->set_device_power_distribution = set_device_power_distribution;
  policy->set_job_power_distribution = set_job_power_distribution;
  policy->set_node_power_distribution = set_node_power_distribution;
}
