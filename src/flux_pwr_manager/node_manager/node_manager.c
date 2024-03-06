/* power-mgr.c */
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "fft_predictor.h"
#include "flux_pwr_logging.h"
#include "job_hash.h"
#include "node_job_info.h"
#include "node_manager.h"
#include "node_power_info.h"
#include "node_util.h"
#include "power_monitor.h"
#include "response_power_data.h"
#include "retro_queue_buffer.h"
#include "system_config.h"
#include "util.h"
#include "variorum.h"
#include <assert.h>
#include <czmq.h>
#include <flux/core.h>
#include <jansson.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#define HOSTNAME_SIZE 256
static uint32_t power_sampling_rate, power_buffer_size;
static char node_hostname[HOSTNAME_SIZE];
static char **hostname_list;
static flux_t *flux_handler;
uint32_t node_rank;
uint32_t cluster_size;
node_data *node_power_data = NULL;
bool fft_enable = true;

// Some strong assumptions:
//  1. Each node will be only a single job(No coscheduling)

int node_manager_init(flux_t *h, uint32_t rank, uint32_t size, char *hostname,
                      size_t buffer_size, size_t sampling_rate) {
  node_power_data = malloc(sizeof(node_data));
  if (node_power_data == NULL)
    return -1;
  power_buffer_size = buffer_size;
  retro_queue_buffer_t *buffer =
      retro_queue_buffer_new(power_buffer_size, free);
  if (buffer == NULL)
    return -1;
  node_power_data->jobId = -1;
  node_power_data->node_power_time = buffer;
  node_power_data->hostname = strdup(hostname);
  if (node_power_data->hostname == NULL)
    return -1;
  node_rank = rank;
  cluster_size = size;
  flux_handler = h;
  power_sampling_rate = sampling_rate;
  current_jobs = job_hash_create();
  zhashx_set_destructor(current_jobs, node_job_info_destroy);

  power_monitor_init(buffer_size);
  fft_predictor_init();

  return 0;
}

int node_manager_set_powerlimit(double powerlimit, int deviceId) {
  log_message("Got powerlimit %f", powerlimit);
  if (powerlimit <= 0)
    return -1;
  return power_monitor_set_node_powercap(powerlimit, deviceId);
  return -1;
}
int node_manager_destructor() {

  power_monitor_destructor();
  fft_predictor_destructor();
  if (node_power_data != NULL) {
    if (node_power_data->node_power_time != NULL)
      retro_queue_buffer_destroy(node_power_data->node_power_time);
  }
  if (node_power_data->hostname != NULL) {
    free(node_power_data->hostname);
  }
  free(node_power_data);
  if (current_jobs != NULL)
    zhashx_destroy(&current_jobs);
  return 0;
}

int node_manager_finish_job(uint64_t jobId) {
  if (node_power_data->jobId != jobId)
    return -1;
  else if (node_power_data->jobId != -1) {
    node_power_data->jobId = -1;
    power_monitor_end_job();
    if (fft_enable)
      fft_predictor_finish_job();
  }
  zhashx_delete(current_jobs, &jobId);
  log_message("NM:Job Finished");
  return 0;
}

int node_manager_new_job(uint64_t jobId, char *job_cwd, char *job_name,
                         node_device_info_t *device_data) {
  if (node_power_data->jobId == -1) {
    node_power_data->jobId = jobId;

    node_job_info *data = node_job_info_create(jobId, job_cwd, device_data);
    if (data != NULL)
      return -1;
    zhashx_insert(current_jobs, &jobId, (void *)data);
    power_monitor_start_job(jobId, job_cwd, job_name);
    if (fft_enable)
      fft_predictor_new_job();
  } else if (node_power_data->jobId != -1) {
    // Got a new job when previous did not finish
    power_monitor_end_job();
    if (fft_enable)
      fft_predictor_finish_job();
  }
  log_message("NM: New job %ld", jobId);
  return 0;
}

void node_manager_set_pl_cb(flux_t *h, flux_msg_handler_t *mh,
                            const flux_msg_t *msg, void *args) {
  json_t *device_data_json;
  double *powerlimit_data;
  int powerlimit_size;
  if (flux_request_unpack(msg, NULL, "{s:s}", "data", &device_data_json) < 0) {
    log_error("RPC_ERROR:Unable to decode set powerlimit RPC");
  }
  node_device_info_t *device_data = json_to_node_device_info(
      device_data_json, powerlimit_data, &powerlimit_size);
  if (!device_data) {
    log_error("Unable to get node_device data from json");
  }
  for (int k = 0; k < device_data->num_of_gpus; k++) {
    node_manager_set_powerlimit(powerlimit_data[k],
                                device_data->device_id_gpus[k]);
  }
}

void node_manager_end_job_cb(flux_t *h, flux_msg_handler_t *mh,
                             const flux_msg_t *msg, void *args) {
  uint64_t jobId;
  if (flux_request_unpack(msg, NULL, "{s:I}", "jobid", &jobId) < 0) {
    log_error("RPC_ERROR:Unable to decode end job RPC");
  }
  node_manager_finish_job(jobId);
}

int node_manager_set_power_ratio(int power_ratio) {
  if (power_ratio < 0 || power_ratio > 100)
    return -1;
  return power_monitor_set_node_power_ratio(power_ratio);
}

// void handle_job_device_info(flux_future_t *f, void *args) {
//
//   json_t *json_r;
//   uint64_t *jobid = (uint64_t *)args;
//   int version;
//   if (flux_rpc_get_unpack(f, "{s:o}", "R", &json_r) < 0) {
//     log_error("RPC_INFO:Unable to parse RPC data %s",
//               flux_future_error_string(f));
//     flux_future_destroy(f);
//     return;
//   }
//   node_job_info *job_info = (node_job_info *)zhashx_lookup(current_jobs,
//   jobid); if (!job_info)
//     log_error("Data not found: Job Not found when searching for device
//     info.");
//   update_device_info_from_json(json_r,job_info,node_rank);
//   flux_future_destroy(f);
// }
//
// // Get device info for each job in the node.
// int get_job_device_info(flux_t *h, uint64_t job_id) {
//   if (job_id == 0 || h == NULL)
//     return -1;
//   flux_future_t *f;
//   if (!(f = flux_rpc_pack(h, "job-info.lookup", FLUX_NODEID_ANY, 0,
//                           "{s:I s:s s:i}", "id", job_id, "key", "R", "flags",
//                           0))) {
//     log_error("ROC_ERROR:Unable to send RPC to get device info for the job
//     %s",
//               flux_future_error_string(f));
//     flux_future_destroy(f);
//   }
//
//   if (flux_future_then(f, -1., handle_job_device_info, (void *)job_id) < 0) {
//     log_message("RPC_INFO:Error in setting flux_future_then for RPC get "
//                 "job-info.loopkup");
//     flux_future_destroy(f); // Clean up the future object
//   }
//   return 0;
// }
//
void node_manager_new_job_cb(flux_t *h, flux_msg_handler_t *mh,
                             const flux_msg_t *msg, void *args) {
  uint64_t jobId;
  char *job_cwd;
  char *job_name;
  double powerlimit;
  json_t *device_info;
  int errno;
  errno = 0;
  json_t *device_data_json;
  double *powerlimit_data;
  int powerlimit_size;
  log_message("RPC GOT it");
  if (flux_request_unpack(msg, NULL, "{s:I s:s s:s s:f s:i}", "jobid", &jobId,
                          "cwd", &job_cwd, "name", &job_name, "data",
                          &device_data_json) < 0) {
    errno = -1;
    log_error("RPC_ERROR:unpack error for node_manager newjob");
  }
  // The job sends device list and there respective power.
  // log_message("New job with power ra  tio %d", power_ratio);
  node_device_info_t *device_data = json_to_node_device_info(
      device_data_json, powerlimit_data, &powerlimit_size);
  if (!device_data) {
    log_error("Unable to get node_device data from json");
  }
  node_manager_new_job(jobId, job_cwd, job_name, device_data);
  for (int i = 0; i < powerlimit_size; i++) {
    if ((node_manager_set_powerlimit(powerlimit, powerlimit_data[i]) < 0))
      log_error("ERROR in setting rank %d , device %d node power settings",
                node_rank, i);
  }
  free(device_data->hostname);
  free(device_data);
}

// void node_flux_powerlimit_rpc_cb(flux_t *h, flux_msg_handler_t *mh,
//                                  const flux_msg_t *msg, void *arg) {}
//
void node_manager_manage_power() {}
void node_manager_send_power_data() {}
