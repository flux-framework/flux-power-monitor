#ifndef FLUX_PWR_MANAGER_NODE_JOB_INFO_H
#define FLUX_PWR_MANAGER_NODE_JOB_INFO_H
#include <czmq.h>
#include "parse_util.h"
typedef struct node_job_info {
  uint64_t jobId;
  int deviceId[12];
  int num_of_devices;
  int device_type[12]; // CPU:0, GPU:1
  double powerlimit[12];
  char *job_cwd;
} node_job_info;
node_job_info* node_job_info_create(uint64_t jobId,char *job_cwd,node_device_info_t* device_data);
void node_job_info_destroy(void **node_job_info);
#endif
