#ifndef FLUX_PWR_MANAGER_NODE_DATA_H
#define FLUX_PWR_MANAGER_NODE_DATA_H
#include "power_policies/power_policy.h"
#include "system_config.h"
#include "retro_queue_buffer.h"
#define MAX_CSV_SIZE 512
typedef struct {
  double node_power;   // Actual node power
  double cpu_power[NUM_OF_CPUS]; // Assuming two sockets
  double gpu_power[NUM_OF_GPUS]; // Assuming at max 8 GPUs
  double mem_power[NUM_OF_MEMS];
  char csv_string[MAX_CSV_SIZE];
  int num_of_gpu;
  uint64_t timestamp;
} node_power;

typedef struct {
  retro_queue_buffer_t *node_power_time; // Stores the power data of the node in
                                         // the form of node_power
  char *hostname;                        // hostname
  double power_limit;
  POWER_POLICY_TYPE power_policy;
  bool file_enable;
  bool write_data_to_file;
} node_data;
extern node_data
    *node_power_data; // Making it extern, available to power monitor
extern uint32_t node_rank, cluster_size;
extern zhashx_t *current_jobs; // Holds the current job running and there info.
#endif
