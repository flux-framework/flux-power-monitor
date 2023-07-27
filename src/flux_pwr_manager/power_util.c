#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "power_util.h"
void set_global_power_strategy(dynamic_job_map *job_map_data,
                               power_strategy *current_power_strategy,
                               double global_power_budget) {
  if (current_power_strategy == NULL)
    return;
  if (job_map_data == NULL)
    return;
  current_power_strategy->set_job_power_distribution(job_map_data,
                                                     global_power_budget);
}
void set_job_power_strategy(job_data *job_data,
                            power_strategy *current_power_strategy,
                            double job_power_budget) {
  if (current_power_strategy == NULL)
    return;
  if (job_data == NULL)
    return;
  current_power_strategy->set_node_power_distribution(job_data,
                                                      job_power_budget);
}
void set_node_power_strategy(node_power_profile *node_data,
                             power_strategy *current_power_strategy,
                             double node_power_budget) {
  if (current_power_strategy == NULL)
    return;
  if (node_data == NULL)
    return;
  current_power_strategy->set_device_power_distribution(node_data,
                                                        node_power_budget);
}
void update_node_powercap(node_power_profile *node_data) {
  if (node_data == NULL)
    return;
  double power = 0;
  for (int i = 0; i < node_data->total_num_of_devices; i++) {
    if (node_data->device_list[i] != NULL)
      power += node_data->device_list[i]->powercap;
  }
  node_data->powercap = power;
}
void update_job_powercap(job_data *job_data) {
  if (job_data == NULL)
    return;
  double power = 0;
  for (int i = 0; i < job_data->num_of_nodes; i++) {
    if (job_data->node_power_profile_data[i] != NULL)
      power += job_data->node_power_profile_data[i]->powercap;
  }
  job_data->powercap = power;
}
