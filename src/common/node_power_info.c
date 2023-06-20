#include "node_power_info.h"
#include <stdlib.h>
#include <string.h>

node_power_info *node_power_info_new(const char *hostname,
                                     const char *power_info,
                                     uint64_t timestamp) {
  node_power_info *power_data =
      (node_power_info *)malloc(sizeof(node_power_info));
  if (power_data == NULL)
    return NULL;
  power_data->hostname = strdup(hostname);
  if (power_data->hostname == NULL) {
    free(power_data);
    return NULL;
  }
  power_data->power_info = strdup(power_info);
  if (power_data->power_info == NULL) {
    free(power_data->hostname);
    free(power_data);
    return NULL;
  }
  power_data->timestamp = timestamp;
  return power_data;
}

void node_power_info_destroy(void *power_info) {
  if (power_info == NULL)
    return;
  node_power_info *power_data = (node_power_info *)power_info;
  if (power_data != NULL) {

    free(power_data->hostname);
    free(power_data->power_info);
  }
  free(power_data);
}
