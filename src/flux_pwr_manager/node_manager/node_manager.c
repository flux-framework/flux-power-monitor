/* power-mgr.c */
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "fft_predictor.h"
#include "flux_pwr_logging.h"
#include "node_manager.h"
#include "node_power_info.h"
#include "power_monitor.h"
#include "response_power_data.h"
#include "retro_queue_buffer.h"
#include "root_node_level_info.h"
#include "util.h"
#include "variorum.h"
#include <assert.h>
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
// Some strong assumptions:
//  1. Each node will be only a single job(No coscheduling)


int node_manager_init(flux_t *h, uint32_t rank, uint32_t size, char *hostname,
                      size_t buffer_size, size_t sampling_rate) {
  node_rank = rank;
  cluster_size = size;
  flux_handler = h;
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
  power_sampling_rate = sampling_rate;
  power_monitor_init(buffer_size);
  fft_predictor_init();
  return 0;
}

int node_manager_set_powerlimit(double powerlimit) {
  log_message("Got powerlimit %f", powerlimit);
  if (powerlimit <= 0)
    return -1;
  return power_monitor_set_node_powercap(powerlimit);
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
  return 0;
}

int node_manager_new_job(uint64_t jobId, char *job_cwd, char *job_name) {
  if (node_power_data->jobId == -1) {
    node_power_data->jobId = jobId;
    power_monitor_start_job(jobId, job_cwd, job_name);
    fft_predictor_new_job();
  } else if (node_power_data->jobId != -1) {
    // Got a new job when previous did not finish
    power_monitor_end_job();
    fft_predictor_finish_job();
  }
  log_message("NM: New job %ld", jobId);
  return 0;
}

int node_manager_finish_job(uint64_t jobId) {
  if (node_power_data->jobId != jobId)
    return -1;
  else if (node_power_data->jobId != -1) {
    node_power_data->jobId = -1;
    power_monitor_end_job();
    fft_predictor_finish_job();
  }
  log_message("NM:Job Finished");
  return 0;
}
// void node_flux_powerlimit_rpc_cb(flux_t *h, flux_msg_handler_t *mh,
//                                  const flux_msg_t *msg, void *arg) {}
