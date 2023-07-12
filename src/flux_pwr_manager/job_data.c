#include "job_data.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>


int create_node_power_profile(job_data *job) {
  job->node_power_profile_data =
      malloc(sizeof(node_power_profile *) * job->num_of_nodes);
  if (job->node_power_profile_data == NULL)
    return -1;

  for (int i = 0; i < job->num_of_nodes; i++) {
    job->node_power_profile_data[i] = node_power_profile_new(
        job->node_hostname_list[i], PER_NODE_POWER_HISTORY);
    if (job->node_power_profile_data[i] == NULL) {
      for (int j = 0; j < i; j++)
        node_power_profile_destroy(job->node_power_profile_data[j]);
      free(job->node_power_profile_data);
      return -1;
    }
  }
  return 0;
}

job_data *job_data_new(char *jobId, char **node_hostname_list, int node_size) {
  job_data *newJob = (job_data *)malloc(sizeof(job_data));
  if (newJob == NULL) {
    HANDLE_ERROR("Failed to allocate memory for job_data.");
  }

  newJob->jobId = strdup(jobId);
  if (newJob->jobId == NULL) {
    HANDLE_ERROR("Failed to allocate memory for jobId.");
  }

  newJob->node_hostname_list = (char **)malloc(node_size * sizeof(char *));
  if (newJob->node_hostname_list == NULL) {
    HANDLE_ERROR("Failed to allocate memory for node_hostname_list.");
  }

  for (int i = 0; i < node_size; i++) {
    newJob->node_hostname_list[i] = strdup(node_hostname_list[i]);
    if (newJob->node_hostname_list[i] == NULL) {
      HANDLE_ERROR("Failed to allocate memory for node_hostname_list[i].");
    }
  }

  newJob->num_of_nodes = node_size;
  if (create_node_power_profile(newJob) == -1) {
    HANDLE_ERROR("Failed to create node power profile.");
  }
  return newJob;

cleanup:
  job_data_destroy(newJob);
  return NULL;
}

void job_data_destroy(job_data *job) {
  if (job->node_hostname_list != NULL) {
    for (int i = 0; i < job->num_of_nodes; i++) {
      free(job->node_hostname_list[i]);
    }
    free(job->node_hostname_list);
    job->node_hostname_list = NULL;
  }

  if (job->jobId != NULL) {
    free(job->jobId);
    job->jobId = NULL;
  }

  if (job->node_power_profile_data != NULL) {
    for (int i = 0; i < job->num_of_nodes; i++) {
      node_power_profile_destroy(job->node_power_profile_data[i]);
    }
    free(job->node_power_profile_data);
    job->node_power_profile_data = NULL;
  }

  free(job);
}
