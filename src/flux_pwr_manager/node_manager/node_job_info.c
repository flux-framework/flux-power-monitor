#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "node_job_info.h"

node_job_info *node_job_info_create(uint64_t jobId, char *job_cwd,
                                    node_device_info_t *device_data) {
  if (jobId == 0 || job_cwd == NULL || device_data == NULL)
    return NULL;
  node_job_info *job_info = calloc(0,sizeof(node_job_info));
  job_info->jobId = jobId;
  job_info->job_cwd = strdup(job_cwd);
  job_info->num_of_devices = device_data->num_of_gpus;
  for (int i = 0; i < device_data->num_of_gpus; i++) {
    job_info->deviceId[i] = device_data->device_id_gpus[i];
    job_info->device_type[i]=1;
    }
  return job_info;
}
void node_job_info_destroy(void **job) {
  node_job_info *job_info = (node_job_info *)*job;
  if (job_info != NULL) {
    free(job_info->job_cwd);
  }
  free(job_info);
  job_info = NULL;
}
