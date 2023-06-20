#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "response_power_data.h"

response_power_data *response_power_data_new(const char *hostname,
                                             uint64_t start_time,
                                             uint64_t end_time) {

  response_power_data *power_data;
  power_data = malloc(sizeof(response_power_data));
  if (power_data == NULL)
    return NULL;
  power_data->hostname = strdup(hostname);
  if (power_data->hostname == NULL) {
    free(power_data);
    return NULL;
  }
  power_data->start_time = start_time;
  power_data->end_time = end_time;
  power_data->full_data_present = true;
  power_data->agg_cpu_power = 0;
  power_data->agg_gpu_power = 0;
  power_data->agg_mem_power = 0;
  power_data->agg_node_power = 0;
  power_data->number_of_samples = 0;
  return power_data;
}
void response_power_data_destroy(response_power_data *data) {
  if (data == NULL)
    return;
  free((void *)data->hostname);
  free(data);
}
