#ifndef FLUX_PWR_MANAGER_NODE_JOB_INFO_H
#define FLUX_PWR_MANAGER_NODE_JOB_INFO_H
#include "node_data.h"
#include "parse_util.h"
#include "power_policies/power_policy.h"
#include "power_policies/policy_mgr.h"
#include "retro_queue_buffer.h"
#include <czmq.h>
typedef struct node_job_info {
  char *name;
  char *job_cwd;
  uint64_t jobId;
  int deviceId[12];
  int num_of_devices;
  int device_type[12]; // CPU:0, GPU:1
  double powerlimit[12];
  POWER_POLICY_TYPE power_policy_type[12];
  retro_queue_buffer_t
      *external_power_data_reference[12]; // External reference does not own
                                          // comes from some system. Do not free
  retro_queue_buffer_t *power_cap_data[12];
  pwr_policy_t *node_job_power_mgr[12]; // Right now putting power policy for each job
} node_job_info;
node_job_info *node_job_info_create(uint64_t jobId, char *job_cwd,
                                    node_device_info_t *device_data,
                                    char *job_name);
void node_job_info_destroy(void **node_job_info);
node_job_info *node_job_info_copy(node_job_info *data);
void node_job_info_reset_power_data(node_job_info *job_data,int deviceId,double powerlimit);
#endif
