#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "constants.h"
#include "flux_pwr_logging.h"
#include "node_job_info.h"
#include "power_policies/policy_mgr.h"
#include "power_policies/power_policy.h"
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
        retro_queue_buffer_new(10, free);
  }
  for (int i = 0; i < device_data->num_of_gpus; i++) {
    job_info->external_power_data_reference[job_info->deviceId[i]] = NULL;
    job_info->power_policy_type[job_info->deviceId[i]] = FFT;
    pwr_policy_t *t = NULL;
    if (job_info->power_policy_type[job_info->deviceId[i]] == FFT) {
      t = pwr_policy_new(FFT);
      log_message("NEW t for device %d",job_info->deviceId[i]);
      if (t == NULL) {
        log_error("Unable to allocate memory for pwr_policy");
      }
    }

     log_message("Setting t");
    job_info->node_job_power_mgr[job_info->deviceId[i]] = t;
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
    // This is for power tracker to just have information about the job. It does
    // not require the information about powercap history; may need to change
    // later.
    new_data->node_job_power_mgr[new_data->deviceId[i]]=NULL;
    new_data->power_cap_data[new_data->deviceId[i]] = NULL;
  }
  return new_data;
}
void node_job_info_destroy(void **job) {
  node_job_info *job_info = (node_job_info *)*job;
  if (job_info == NULL)
    return;
  free(job_info->job_cwd);
  free(job_info->name);
  for (int i = 0; i < job_info->num_of_devices; i++) {
    if (job_info->node_job_power_mgr[i]!=NULL){
      pwr_policy_destroy(&(job_info->node_job_power_mgr[i]));
    }
    if (job_info->power_cap_data[job_info->deviceId[i]] != NULL)

      retro_queue_buffer_destroy(job_info->power_cap_data[i]);
  }
  free(job_info);
  job_info = NULL;
}

void node_job_info_reset_power_data(node_job_info *job_data,int deviceId,double powerlimit){
  if(job_data==NULL)
    return;
  log_message("resetting for Device Id %d",deviceId);
  if(job_data->node_job_power_mgr[deviceId]==NULL){
    log_error("Power Manager not initalized");
    return;
  }
  pwr_policy_t* mgr=job_data->node_job_power_mgr[deviceId];
  zlist_purge(mgr->powercap_history->list);
  double *a=malloc(sizeof(double));
  double *b=malloc(sizeof(double));
  *a=powerlimit;
  *b=powerlimit;
  retro_queue_buffer_push(mgr->powercap_history,a);
  retro_queue_buffer_push(mgr->powerlimit_history,a);
}
