#ifndef FLUX_PWR_MANAGER_NODE_DATA_H
#define FLUX_PWR_MANAGER_NODE_DATA_H
#include "circular_buffer.h"
#include "power_policies/power_policy.h"
#define MAX_CSV_SIZE 512
typedef struct {
  double node_power;   // Actual node power
  double cpu_power[2]; // Assuming two sockets
  double gpu_power[8]; // Assuming at max 8 GPUs
  double mem_power[2];
  char csv_string[MAX_CSV_SIZE];
  int num_of_gpu;
} node_power;
typedef struct {
  circular_buffer_t *node_power_time; // Stores the power data of the node in
                                      // the form of node_power
  char *hostname;                     // hostname
  double power_limit;
  POWER_POLICY_TYPE power_policy;
  uint64_t jobId; // -1 in case of no job;
  bool file_enable;
  bool write_data_to_file;
} node_data;
extern node_data
    *node_power_data; // Making it extern, available to power monitor
extern uint32_t node_rank, cluster_size;
#endif
