#include "node_power_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HANDLE_ERROR(msg)                                                      \
  do {                                                                         \
    fprintf(stderr, "%s\n", msg);                                              \
    goto cleanup;                                                              \
  } while (0)

void do_agg(power_data *old_agg, size_t buffer_size, power_data *new_data,
            power_data *new_agg) {
  if (buffer_size == 0) {
    new_agg->cpu_power = new_data->cpu_power;
    new_agg->mem_power = new_data->mem_power;
    new_agg->gpu_power = new_data->gpu_power;
    new_agg->node_power = new_data->node_power;
  } else {
    new_agg->cpu_power =
        ((old_agg->cpu_power * buffer_size) + new_data->cpu_power) /
        (buffer_size + 1);
    new_agg->mem_power =
        ((old_agg->mem_power * buffer_size) + new_data->mem_power) /
        (buffer_size + 1);
    new_agg->gpu_power =
        ((old_agg->gpu_power * buffer_size) + new_data->gpu_power) /
        (buffer_size + 1);
    new_agg->node_power =
        ((old_agg->node_power * buffer_size) + new_data->node_power) /
        (buffer_size + 1);
  }
  new_agg->timestamp = new_data->timestamp;
}

node_power_profile *node_power_profile_new(char *hostname,
                                           size_t node_history_size) {
  node_power_profile *node = malloc(sizeof(node_power_profile));
  if (node == NULL)
    HANDLE_ERROR("Failed to allocate memory for node_power_profile.");

  node->hostname = strdup(hostname);
  if (node->hostname == NULL)
    HANDLE_ERROR("Failed to allocate memory for hostname.");

  node->node_history_size = node_history_size;
  node->node_power_history = circular_buffer_new(node->node_history_size, free);
  if (node->node_power_history == NULL)
    HANDLE_ERROR("Failed to create circular_buffer.");

  node->node_power_latest = power_data_new();
  if (node->node_power_latest == NULL)
    HANDLE_ERROR("Failed to allocate memory for node_power_latest.");

  node->node_power_agg = power_data_new();
  if (node->node_power_agg == NULL)
    HANDLE_ERROR("Failed to allocate memory for node_power_agg.");

  node->node_current_power_policy = CURRENT_POWER;
  node->device_power_policy = CURRENT_POWER;
  return node;

cleanup:
  if (node->node_power_agg != NULL)
    free(node->node_power_agg);
  if (node->node_power_latest != NULL)
    free(node->node_power_latest);
  if (node->node_power_history != NULL)
    circular_buffer_destroy(node->node_power_history);
  if (node->hostname != NULL)
    free(node->hostname);
  if (node != NULL)
    free(node);
  return NULL;
}

void node_power_profile_destroy(node_power_profile *node) {
  if (node == NULL)
    return;
  if (node->hostname != NULL)
    free(node->hostname);
  if (node->node_power_agg != NULL)
    free(node->node_power_agg);
  if (node->node_power_latest != NULL)
    free(node->node_power_latest);
  if (node->node_power_history != NULL)
    circular_buffer_destroy(node->node_power_history);
  free(node);
}

void set_node_current_power_policy(node_power_profile *node,
                                   POWER_POLICY node_power_policy) {
  if (node == NULL)
    return;
  node->node_current_power_policy = node_power_policy;
}

void set_device_power_profile(node_power_profile *node,
                              POWER_POLICY node_power_policy) {
  if (node == NULL)
    return;
  node->device_power_policy = node_power_policy;
}

int add_power_data_to_node_history(node_power_profile *node, power_data *data) {
  if (node == NULL || data == NULL)
    return -1;

  power_data *data_copy = malloc(sizeof(power_data));
  if (data_copy == NULL) {
    fprintf(stderr, "Failed to allocate memory for power_data copy.\n");
    return -1;
  }
  memcpy(data_copy, data, sizeof(power_data));

  circular_buffer_push(node->node_power_history, data_copy);
  memcpy(node->node_power_latest, data, sizeof(power_data));

  do_agg(node->node_power_agg,
         circular_buffer_get_current_size(node->node_power_history), data,
         node->node_power_agg);

  return 0;
}
