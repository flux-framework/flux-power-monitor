#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "node_power_profile.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HANDLE_ERROR(msg)                                                      \
  do {                                                                         \
    fprintf(stderr, "%s\n", msg);                                              \
    goto cleanup;                                                              \
  } while (0)

node_power_profile *node_power_profile_new(char *hostname,
                                           size_t node_history_size) {
  node_power_profile *node = calloc(1, sizeof(node_power_profile));
  if (node == NULL)
    HANDLE_ERROR("Failed to allocate memory for node_power_profile.");

  node->hostname = strdup(hostname);
  if (node->hostname == NULL)
    HANDLE_ERROR("Failed to allocate memory for hostname.");
  node->history_size = node_history_size;
  node->power_history = circular_buffer_new(node->history_size, free);
  if (node->power_history == NULL)
    HANDLE_ERROR("Failed to create circular_buffer for node_power_history");
  node->node_current_power_policy = CURRENT_POWER;
  node->powercap_allowed = false;
  node->device_power_policy = CURRENT_POWER;
  return node;

cleanup:
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
  if (node->power_history != NULL)
    circular_buffer_destroy(node->power_history);
  if (node->device_list != NULL) {
    for (int i = 0; i < node->total_num_of_devices; i++) {
      if (node->device_list[i] != NULL)
        device_power_profile_destroy(node->device_list[i]);
    }
    free(node->device_list);
  }
  free(node);
}
static int init_device(device_capability *capability,
                       device_power_profile **device_list, int index,
                       size_t device_history_size) {
  device_power_profile *device_data =
      device_power_profile_new(capability->type, index, device_history_size);
  if (!device_data) {
    return -1;
  }
  device_data->powercap_allowed = capability->powercap_allowed;
  device_data->min_power = capability->min_power;
  device_data->max_power = capability->max_power;
  device_list[index] = device_data;
  return 0; // Success
}

int node_device_list_init(node_power_profile *node, node_capabilities *d_data,
                          size_t device_history_size) {
  if (!node || !d_data) {
    HANDLE_ERROR("NULL parameter provided to node_device_list_init");
    return -1;
  }

  node->num_of_socket = d_data->sockets.count;
  node->num_of_gpus = d_data->gpus.count;
  node->total_num_of_devices = d_data->sockets.count + d_data->cpus.count +
                               d_data->gpus.count + d_data->mem.count;

  node->device_list =
      malloc(sizeof(device_power_profile *) * node->total_num_of_devices);
  if (!node->device_list) {
    HANDLE_ERROR("Unable to allocate memory for node device_list");
    return -1;
  }

  int index = 0;

  // Initialize GPUs
  for (int i = 0; i < d_data->gpus.count; i++, index++) {
    if (init_device(&(d_data->gpus), node->device_list, index,
                    device_history_size) < 0) {
      return -1;
    }
  }

  // Initialize CPUs
  for (int i = 0; i < d_data->cpus.count; i++, index++) {
    if (init_device(&(d_data->cpus), node->device_list, index,
                    device_history_size) < 0) {
      return -1;
    }
  }

  // Initialize Memory
  if (d_data->mem.count > 0) {
    if (init_device(&(d_data->mem), node->device_list, index++,
                    device_history_size) < 0) {
      return -1;
    }
  }
  if (d_data->node.count > 0) {
    node->powercap_allowed = d_data->node.powercap_allowed;
    node->max_power = d_data->node.max_power;
    node->min_power = d_data->node.min_power;
  }

  return 0;
cleanup:
  if (node->device_list != NULL) {
    for (int j = 0; j < node->total_num_of_devices;
         j++) { // Free all the device_power_profile
      if (node->device_list[j] != NULL)
        device_power_profile_destroy(node->device_list[j]);
    }
    free(node->device_list);
    node->device_list = NULL; // Set to NULL to prevent double-free
  }
  return -1;
}

void node_current_power_policy_set(node_power_profile *node,
                                   POWER_POLICY_TYPE node_power_policy) {
  if (node == NULL)
    return;
  node->node_current_power_policy = node_power_policy;
}

int node_device_power_update(node_power_profile *node, power_data **data,
                             int num_of_devices) {
  if (node == NULL || num_of_devices == 0 || data == NULL)
    HANDLE_ERROR("NULL parameter provided to node_device_power_update");
  if (node->total_num_of_devices != num_of_devices)
    HANDLE_ERROR(
        "Mismatch between the number_of_devices and node->num_of_devices");
  for (int i = 0; i < num_of_devices; i++) {
    if (node->device_list[i] == NULL) {
      HANDLE_ERROR("Panic:Device Not found");
    }
    for (int j = 0; j < num_of_devices; j++) {
      if (node->device_list[i]->type == data[j]->device_id) {
        if (node->device_list[i]->device_id == data[j]->device_id) {
          if (device_power_profile_add_power_data_to_device_history(
                  node->device_list[i], data[j]) < 0) {
            fprintf(stderr, "Unable to add the power data to device");
          }
        }
      }
    }
  }
  return 0;
cleanup:
  return -1;
}
int node_power_update(node_power_profile *node, power_data *data) {
  if (data->type != NODE)
    HANDLE_ERROR("Wrong Data supplied");
  node->power_current = data->power_value;
  if (node->power_history == NULL)
    HANDLE_ERROR("node power history is NULL");
  // First finding the average and then only add the item to buffer as we want
  // to keep a running average of all the items in the circular_buffer
  double node_power_agg =
      do_agg(node->power_history, data->power_value, node->power_agg);
  if (node_power_agg != -1.0)
    node->power_agg = node_power_agg;
  else
    HANDLE_ERROR("Error when adding value to node history");
  return 0;
cleanup:
  return -1;
}
