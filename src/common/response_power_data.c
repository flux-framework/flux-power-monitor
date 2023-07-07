#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "response_power_data.h"

const char *data_presence_string_literal[STATUS_COUNT] = {"FULL", "PARTIAL", "NONE"};
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
  power_data->start_time = UINT64_MAX;
  power_data->end_time = 0;
  power_data->data_presence = NONE;
  power_data->agg_cpu_power = 0;
  power_data->agg_gpu_power = 0;
  power_data->agg_mem_power = 0;
  power_data->agg_node_power = 0;
  power_data->number_of_samples = 0;
  return power_data;
}
const char *get_data_presence_string(DATA_PRESENCE_STATUS status) {
  if (status < 0 || status > STATUS_COUNT)
    status = 0;
  return data_presence_string_literal[status];
};

void response_power_data_destroy(response_power_data *data) {
  if (data == NULL)
    return;
  free((void *)data->hostname);
  free(data);
}
