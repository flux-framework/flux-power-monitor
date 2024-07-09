#ifndef FLUX_PWR_MANAGER_RESPONSE_POWER_DATA_H
#define FLUX_PWR_MANAGER_RESPONSE_POWER_DATA_H

#include <czmq.h>
#include <inttypes.h>
#include <jansson.h>

typedef enum { FULL, PARTIAL, NONE, STATUS_COUNT } DATA_PRESENCE_STATUS;
extern const char *data_presence_string_literal[STATUS_COUNT];
const char *get_data_presence_string(DATA_PRESENCE_STATUS status);
typedef struct {
  char *hostname;
  uint64_t start_time;
  uint64_t end_time;
  DATA_PRESENCE_STATUS data_presence;
  double agg_cpu_power[12];
  double agg_gpu_power[12];
  double agg_mem_power[12];
  int num_of_cpus;
  int num_of_gpus;
  int num_of_mem;
  double agg_node_power;
  size_t number_of_samples;
} response_power_data;
response_power_data *response_power_data_new(const char *hostname,
                                             uint64_t start_time,
                                             uint64_t end_time);

json_t *response_power_data_to_json(response_power_data *data);
void response_power_data_destroy(response_power_data *data);
#endif
