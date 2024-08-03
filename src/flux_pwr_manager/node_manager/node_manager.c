/* power-mgr.c */
#include <zlist.h>
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "fft_predictor.h"
#include "file_logger.h"
#include "flux_pwr_logging.h"
#include "job_hash.h"
#include "node_job_info.h"
#include "node_manager.h"
#include "node_power_info.h"
#include "node_util.h"
#include "power_monitor.h"
#include "power_policies/fft_based_power_policy.h"
#include "power_policies/policy_mgr.h"
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
#include <time.h>
#include <unistd.h>
#include <zhash.h>
#define LOG_LEN 100
#define HOSTNAME_SIZE 256
static uint32_t power_sampling_rate, power_buffer_size;
static char node_hostname[HOSTNAME_SIZE];
static char **hostname_list;
static flux_t *flux_handler;
uint32_t node_rank;
uint32_t cluster_size;
node_data *node_power_data = NULL;
bool fft_enable = true;
zhashx_t *current_jobs = NULL;
bool enable_dynamic_powercapping = false;
Logger *file_log = NULL;

void node_manager_get_relevant_power_data(node_job_info *job_data) {

  for (int i = 0; i < job_data->num_of_devices; i++) {
    if (job_data->power_policy_type[job_data->deviceId[i]] == FFT)
      job_data->external_power_data_reference[job_data->deviceId[i]] =
          fft_predictor_get_full_results(job_data->deviceId[i]);
  }
}
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
  node_power_data->node_power_time = buffer;
  node_power_data->hostname = strdup(hostname);
  if (node_power_data->hostname == NULL)
    return -1;
  node_rank = rank;
  cluster_size = size;
  flux_handler = h;
  power_sampling_rate = sampling_rate;
  char filename[256];
  snprintf(filename, 256, "%s.power.log", node_power_data->hostname);
  file_log = file_logger_new(filename, 256);
  if (file_log == NULL)
    log_error("File logger unable to init it");
  current_jobs = job_hash_create();
  zhashx_set_destructor(current_jobs, node_job_info_destroy);
  power_monitor_init(buffer_size);
  fft_predictor_init();

  return 0;
}
int node_manager_update_and_set_powercap(node_job_info *job, double powercap,
                                         int deviceId) {
  if (job == NULL)
    return -1;

  double *data = malloc(sizeof(double));
  *data = powercap;
  log_message("setting power cap for JobId %ld deviceI %d and powercap %f",
              job->jobId, deviceId, powercap);
  retro_queue_buffer_push(job->power_cap_data[deviceId], data);
  if (power_monitor_set_node_powercap(powercap, deviceId) < 0) {
    return -1;
  }
  if (fft_enable)
    fft_predictor_reset(deviceId);
  return 0;
}

int node_manager_cal_and_set_powercap() {
  if (enable_dynamic_powercapping) {

    log_message("set nodepowercap function");
    pwr_policy_t *data = malloc(sizeof(pwr_policy_t));
    node_job_info *job_data = zhashx_first(current_jobs);
    while (job_data != NULL) {

      for (int i = 0; i < job_data->num_of_devices; i++) {
        if (job_data->power_policy_type[job_data->deviceId[i]] == FFT) {
          log_message("Power Policy type is FFT");
          fft_pwr_policy_init(data);
        } else {
          continue;
        }
        double job_device_current_powercap = *(double *)zlist_last(
            job_data->power_cap_data[job_data->deviceId[i]]->list);
        log_message("job_device current power cap %f",
                    job_device_current_powercap);
        if (job_device_current_powercap == 0)
          continue;
        log_message("powerlimit of the job %f",
                    job_data->powerlimit[job_data->deviceId[i]]);
        char j_key[LOG_LEN];
        char j_data[LOG_LEN];
        snprintf(j_key, LOG_LEN, "nm_pl_gpu_%d", job_data->deviceId[i]);
        snprintf(j_data, LOG_LEN, "%f",
                 job_data->powerlimit[job_data->deviceId[i]]);
        file_logger_add_data_to_buffer(file_log, j_key, strlen(j_key), j_data,
                                       strlen(j_data));
        snprintf(j_key, LOG_LEN, "fft_time_gpu_%d", job_data->deviceId[i]);
        double period = -1.0f;
        if (retro_queue_buffer_get_current_size(
                job_data
                    ->external_power_data_reference[job_data->deviceId[i]]) !=
            0) {
          period = zlist_last(
              job_data->external_power_data_reference[job_data->deviceId[i]]);
        }
        snprintf(j_data, LOG_LEN, "%f", period);
        file_logger_add_data_to_buffer(file_log, j_key, strlen(j_key), j_data,
                                       strlen(j_data));
        double new_powecap = data->get_powercap(
            job_data->powerlimit[job_data->deviceId[i]],
            job_device_current_powercap,
            job_data->external_power_data_reference[job_data->deviceId[i]]);
        log_message("getting new powercap");
        snprintf(j_key, LOG_LEN, "new_pcap_%d", job_data->deviceId[i]);
        snprintf(j_data, LOG_LEN, "%f", new_powecap);
        file_logger_add_data_to_buffer(file_log, j_key, strlen(j_key), j_data,
                                       strlen(j_data));
        if (node_manager_update_and_set_powercap(job_data, new_powecap,

                                                 job_data->deviceId[i]) < 0)
          log_message("Powercap setting failed for jobId and deviceId",
                      job_data->jobId, job_data->deviceId[i]);
      }

      log_message("size of current jobs %ld", zhashx_size(current_jobs));
      job_data = zhashx_next(current_jobs);
      if (job_data == NULL)
        break;
    }
    free(data);
    return 0;
  } else {
    log_message("dynamic powercapping disabled");
  }
}

int node_manager_set_powerlimit(uint64_t jobId, double powerlimit,
                                int deviceId) {
  log_message("setting powercap for host %s device %d and power limit %f",
              node_hostname, deviceId, powerlimit);
  if (powerlimit <= 0)
    G return -1;
  node_job_info *job_data = zhashx_lookup(current_jobs, &jobId);
  if (job_data == NULL)
    return -1;

  job_data->powerlimit[deviceId] = powerlimit;

  if (node_manager_update_and_set_powercap(job_data, powerlimit, deviceId) < 0)
    return -1;
  return 0;
}
int node_manager_destructor() {
  file_logger_destroy(&file_log);
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
  void *key;
  node_job_info *value;

  node_job_info *data = (node_job_info *)zhashx_lookup(current_jobs, &jobId);
  if (data == NULL) {
    log_error("NM:JOB NOT FOUND");
    return -1;
  }

  char j_key[LOG_LEN] = "nm_end_jobid";
  char j_data[LOG_LEN];
  snprintf(j_data, LOG_LEN, "%lu", jobId);
  file_logger_add_data_to_buffer(file_log, j_key, strlen(j_key), j_data,
                                 strlen(j_data));
  power_monitor_end_job(jobId);
  if (fft_enable)
    fft_predictor_finish_job(jobId);
  zhashx_delete(current_jobs, &jobId);
  log_message("NM:Job Finished");
  return 0;
}

int node_manager_new_job(uint64_t jobId, char *job_cwd, char *job_name,
                         node_device_info_t *device_data) {

  node_job_info *data =
      node_job_info_create(jobId, job_cwd, device_data, job_name);
  if (data == NULL)
    return -1;
  // Dynamic memory important.
  uint64_t *key = malloc(sizeof(uint64_t));
  char j_key[LOG_LEN] = "nm_new_jobid";
  char j_data[LOG_LEN];
  snprintf(j_data, LOG_LEN, "%lu", jobId);
  file_logger_add_data_to_buffer(file_log, j_key, strlen(j_key), j_data,
                                 strlen(j_data));
  *key = jobId;
  if (zhashx_insert(current_jobs, key, (void *)data) < 0) {
    log_error("insert to current_jobs failed");
    return -1;
  }

  power_monitor_start_job(jobId);
  if (fft_enable) {
    fft_predictor_new_job(jobId);
    // Get the FFT result retro queue buffer.
    // The buffer lifetime is tied with the Job's,
    // so calling after fft_predictor_new_job
    node_manager_get_relevant_power_data(data);
  }
  log_message("NM: New job %ld", jobId);
  return 0;
}

void node_manager_set_pl_cb(flux_t *h, flux_msg_handler_t *mh,
                            const flux_msg_t *msg, void *args) {
  json_t *device_data_json;
  double *powerlimit_data;
  int powerlimit_size;
  uint64_t jobId;
  log_message("recv data");
  if (flux_request_unpack(msg, NULL, "{s:I s:O}", "jobId", &jobId, "data",
                          &device_data_json) < 0) {
    log_error("RPC_ERROR:Unable to decode set powerlimit RPC");
  }
  node_device_info_t *device_data = json_to_node_device_info(
      device_data_json, &powerlimit_data, &powerlimit_size);
  if (!device_data || jobId == 0) {
    log_error("Unable to get node_device data from json");
    return;
  }
  for (int k = 0; k < device_data->num_of_gpus; k++) {
    char j_key[LOG_LEN];
    char j_data[LOG_LEN];
    snprintf(j_key, LOG_LEN, "jobid_%lu_pl_cb_gpu_%d", jobId,
             device_data->device_id_gpus[k]);
    snprintf(j_data, LOG_LEN, "%f", powerlimit_data[k]);
    file_logger_add_data_to_buffer(file_log, j_key, strlen(j_key), j_data,
                                   strlen(j_data));
    node_manager_set_powerlimit(jobId, powerlimit_data[k],
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
  int powerlimit_size = 0;
  if (flux_request_unpack(msg, NULL, "{s:I s:s s:s s:O}", "jobid", &jobId,
                          "cwd", &job_cwd, "name", &job_name, "data",
                          &device_data_json) < 0) {
    errno = -1;
    log_error("RPC_ERROR:unpack error for node_manager newjob");
  }
  // The job sends device list and there respective power.
  // log_message("New job with power ra  tio %d", power_ratio);
  node_device_info_t *device_data = json_to_node_device_info(
      device_data_json, &powerlimit_data, &powerlimit_size);
  if (!device_data) {
    log_error("Unable to get node_device data from json");
    return;
  }
  node_manager_new_job(jobId, job_cwd, job_name, device_data);
  for (int i = 0; i < powerlimit_size; i++) {
    if ((node_manager_set_powerlimit(jobId, powerlimit_data[i],
                                     device_data->device_id_gpus[i]) < 0))
      log_error("ERROR in setting rank %d , device %d node power settings",
                node_rank, i);
  }
  free(device_data);
}

// void node_flux_powerlimit_rpc_cb(flux_t *h, flux_msg_handler_t *mh,
//                                  const flux_msg_t *msg, void *arg) {}

void get_fft_result() {
  FILE *file;
  char filename[100];
  snprintf(filename, 100, "file_%s_final.txt", node_power_data->hostname);
  file = fopen(filename, "a");
  zhashx_t *data = zhashx_first(current_jobs);
  while (data != NULL) {
    node_job_info *data_node = (node_job_info *)data;
    for (int i = 0; i < data_node->num_of_devices; i++) {
      double result = fft_predictor_get_result(data_node->deviceId[i]);
      if (result < 0) {
        log_error("Error: FFT Result not found for GPUId %d",
                  data_node->deviceId[0]);
        continue;
      }
      double result_second =
          fft_predictor_secondary_get_result(data_node->deviceId[i]);
      size_t size = fft_predictor_get_size(data_node->deviceId[i]);
      struct timespec tv;
      if (clock_gettime(CLOCK_REALTIME, &tv))
        log_error("error clock_gettime  %s", strerror(errno));
      if (fprintf(file, "timestamp:%ld, ", tv.tv_sec) < 0) {
        log_error("failure in writing the timestamp");
      }

      if (fprintf(file, "Job Name: %s, ", data_node->name) < 0) {
        log_error("failure in writing the job name");
      }

      if (fprintf(file, "Device: %d, ", data_node->deviceId[i]) < 0) {
        log_error("failure in writing the device ID");
      }

      if (fprintf(file, "Input Size : %ld, ", size) < 0) {
        log_error("failure in writing the device ID");
      }
      if (fprintf(file, "FFT RESULT: %f ", result) < 0) {
        log_error("failure in writing the FFT result");
      }

      if (fprintf(file, "FFT RESULT_SECONDARY: %f \n", result_second) < 0) {
        log_error("failure in writing the FFT result");
      }
      // log_message(
      //     "FFT_RESULT FOR DeviceId %d , %f, second time %f and size %ld",
      //     data_node->deviceId[i], result, result_second, size);
    }
    data = zhashx_next(current_jobs);
  }
  fclose(file);
}
void node_manager_disable_pm_cb(flux_t *h, flux_msg_handler_t *mh,
                                const flux_msg_t *msg, void *args) {
  bool flag;

  if (flux_request_unpack(msg, NULL, "{s:b}", "flag", &flag) < 0) {
    log_error("RPC_ERROR:Unable to decode set powerlimit RPC");
  }
  enable_dynamic_powercapping = flag;
}
void node_manager_manage_power() {
  //
  //   for (int i = 0; i < node_job_data->num_of_devices; i++) {
  //     retro_queue_buffer_t *results =
  //         fft_predictor_get_full_results(node_job_data->deviceId[i]);
  //     if (results == NULL) {
  //       continue;
  //     }
  //     if (results->current_size == 0)
  //       continue;
  // }
  // }
  if (node_manager_cal_and_set_powercap() < 0)
    log_message("setting powercap failed");
}
void node_manager_send_power_data() {}
void node_manager_print_fft_result(void) { get_fft_result(); }
