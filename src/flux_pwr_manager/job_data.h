#ifndef FLUX_JOB_DATA_H
#define FLUX_JOB_DATA_H
#include "circular_buffer.h"
#include <stdint.h>

typedef struct {
  double mem_power;
  double node_power;
  double cpu_power;
  double gpu_power;
  uint64_t timestamp;
} power_data;


typedef struct {
  char *jobId;
  int num_of_nodes;
  char **node_hostname_list;
  power_data *job_current_power_data;
  power_data *job_agg_power_data;
  circular_buffer_t *per_node_power_data;
  power_data *per_node_powercap;

} job_data;

job_data *job_data_new(char *jobId, char **node_hostname_list, int node_size);
void job_data_destroy(job_data *job);
#endif
