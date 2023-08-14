#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "circular_buffer.h"
#include "job_data.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int create_node_power_profile(job_data *job) {
  job->node_power_profile_data =
      malloc(sizeof(node_power_profile *) * job->num_of_nodes);
  if (job->node_power_profile_data == NULL)
    return -1;

  for (int i = 0; i < job->num_of_nodes; i++) {
    job->node_power_profile_data[i] =
        node_power_profile_new(job->node_hostname_list[i], POWER_HISTORY_SIZE);
    if (job->node_power_profile_data[i] == NULL) {
      for (int j = 0; j < i; j++)
        node_power_profile_destroy(job->node_power_profile_data[j]);
      free(job->node_power_profile_data);
      return -1;
    }
  }
  return 0;
}
job_data *job_data_new(uint64_t jobId, char **node_hostname_list, int node_size,
                       uint64_t t_depend) {
  job_data *newJob = (job_data *)calloc(1, sizeof(job_data));
  if (newJob == NULL) {
    fprintf(stderr, "Failed to allocate memory for job_data.\n");
    return NULL;
  }

  newJob->jobId = jobId;
  newJob->t_depend = t_depend;
  newJob->power_history = circular_buffer_new(POWER_HISTORY_SIZE, free);
  if (newJob->power_history == NULL) {
    fprintf(stderr, "Failed to allocate memory for job_power_history.\n");
    free(newJob);
    return NULL;
  }

  newJob->node_hostname_list = (char **)malloc(node_size * sizeof(char *));
  if (newJob->node_hostname_list == NULL) {
    fprintf(stderr, "Failed to allocate memory for node_hostname_list.\n");
    circular_buffer_destroy(newJob->power_history);
    free(newJob);
    return NULL;
  }

  for (int i = 0; i < node_size; i++) {
    newJob->node_hostname_list[i] = strdup(node_hostname_list[i]);
    if (newJob->node_hostname_list[i] == NULL) {
      fprintf(stderr, "Failed to allocate memory for node_hostname_list[i].\n");
      for (int j = 0; j < i; j++)
        free(newJob->node_hostname_list[j]);
      free(newJob->node_hostname_list);
      circular_buffer_destroy(newJob->power_history);
      free(newJob);
      return NULL;
    }
  }

  newJob->num_of_nodes = node_size;
  if (create_node_power_profile(newJob) == -1) {
    fprintf(stderr, "Failed to create node power profile.\n");
    job_data_destroy(newJob);
    return NULL;
  }
  return newJob;
}
void job_data_destroy(job_data *job) {
  if (job->node_hostname_list != NULL) {
    for (int i = 0; i < job->num_of_nodes; i++) {
      free(job->node_hostname_list[i]);
      job->node_hostname_list[i] = NULL;
    }
    free(job->node_hostname_list);
    job->node_hostname_list = NULL;
  }

  if (job->node_power_profile_data != NULL) {
    for (int i = 0; i < job->num_of_nodes; i++) {
      node_power_profile_destroy(job->node_power_profile_data[i]);
      job->node_power_profile_data[i] = NULL;
    }
    free(job->node_power_profile_data);
    job->node_power_profile_data = NULL;
  }

  circular_buffer_destroy(job->power_history);
  job->power_history = NULL;

  free(job);
  job = NULL;
}
int job_power_update(job_data *job, power_data *data) {
  if (job == NULL || data == NULL)
    return -1;
  job->power_current = data->power_value;
  do_agg(job->power_history, data->power_value, job->power_agg);
  return 0;
}
int job_node_power_update(job_data *job, char *hostname, power_data **p_data,
                          int num_of_gpus, int num_of_sockets, bool mem,
                          int num_of_devices, power_data *node_p_data,
                          uint64_t timestamp) {

  int found = 0;
  for (int i = 0; i < job->num_of_nodes; i++) {
    if (strcmp(job->node_hostname_list[i], hostname) == 0) {
      if ((node_device_power_update(job->node_power_profile_data[i], p_data,
                                    num_of_devices)) < 0) {
        HANDLE_ERROR("Error adding power data to node device list");
      }

      if ((node_power_update(job->node_power_profile_data[i], node_p_data)) <
          0) {
        HANDLE_ERROR("Error adding power data to node history");
      }
      found = 1;
      break;
    }
  }
  if (!found)
    HANDLE_ERROR("Hostname not found");
  return 0;
cleanup:
  return -1;
}
