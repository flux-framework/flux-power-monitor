#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "constants.h"
#include "flux_pwr_logging.h"
#include "node_job_info.h"
node_job_info *node_job_info_create(uint64_t jobId, char *job_cwd,
                                    node_device_info_t *device_data,
                                    char *job_name) {
  if (jobId == 0 || job_cwd == NULL || device_data == NULL)
    return NULL;
  node_job_info *job_info = calloc(1, sizeof(node_job_info));
  job_info->jobId = jobId;
  job_info->job_cwd = strdup(job_cwd);
  job_info->name = strdup(job_name);
  job_info->num_of_devices = device_data->num_of_gpus;
  for (int i = 0; i < device_data->num_of_gpus; i++) {
    job_info->deviceId[i] = device_data->device_id_gpus[i];
    job_info->device_type[i] = 1;
    job_info->power_cap_data[job_info->deviceId[i]] =
        retro_queue_buffer_new(100, free);
  }
  for (int i = 0; i < device_data->num_of_gpus; i++) {
    job_info->external_power_data_reference[job_info->deviceId[i]] = NULL;
    job_info->power_policy_type[job_info->deviceId[i]] = FFT;
  }

  return job_info;
}
node_job_info *node_job_info_copy(node_job_info *data) {
  if (data == NULL)
    return NULL;
  node_job_info *new_data = malloc(sizeof(node_job_info));
  new_data->jobId = data->jobId;
  new_data->job_cwd = strdup(data->job_cwd);
  new_data->name = strdup(data->name);
  new_data->num_of_devices = data->num_of_devices;
  for (int i = 0; i < data->num_of_devices; i++) {
    new_data->deviceId[i] = data->deviceId[i];
    new_data->device_type[i] = 1;
  }
  return new_data;
}
void node_job_info_destroy(void **job) {
  node_job_info *job_info = (node_job_info *)*job;
  if (job_info != NULL) {
    free(job_info->job_cwd);
    free(job_info->name);
  }
  for (int i = 0; i < job_info->num_of_devices; i++) {
    retro_queue_buffer_destroy(job_info->power_cap_data[i]);
  }
  free(job_info);
  job_info = NULL;
}
