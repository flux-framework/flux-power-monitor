#ifndef RESPONSE_POWER_DATA_H
#define RESPONSE_POWER_DATA_H
#include <czmq.h>
#include <inttypes.h>
typedef struct {
   char *hostname;
  uint64_t start_time;
  uint64_t end_time;
  bool full_data_present;
  double agg_cpu_power;
  double agg_gpu_power;
  double agg_mem_power;
  double agg_node_power;
  int number_of_samples;
} response_power_data;
#endif
response_power_data *
response_power_data_new(const char *hostname, uint64_t start_time, uint64_t end_time);
void response_power_data_destroy(response_power_data *data);
