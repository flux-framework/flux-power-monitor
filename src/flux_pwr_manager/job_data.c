#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>

#include "job_data.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

job_data *job_data_new(char *jobId, char **node_hostname_list, int node_size) {
  // Dynamically allocate a new job_data
  job_data *newJob = (job_data *)malloc(sizeof(job_data));
  if (newJob == NULL) {
    fprintf(stderr, "Failed to allocate memory for job_data.\n");
    return NULL;
  }

  // Copy the jobId
  newJob->jobId = strdup(jobId);
  if (newJob->jobId == NULL) {
    fprintf(stderr, "Failed to allocate memory for jobId.\n");
    free(newJob);
    return NULL;
  }

  // Copy the node_hostname_list
  newJob->node_hostname_list = (char **)malloc(node_size * sizeof(char *));
  if (newJob->node_hostname_list == NULL) {
    fprintf(stderr, "Failed to allocate memory for node_hostname_list.\n");
    free(newJob->jobId);
    free(newJob);
    return NULL;
  }
  for (int i = 0; i < node_size; i++) {
    newJob->node_hostname_list[i] = strdup(node_hostname_list[i]);
    if (newJob->node_hostname_list[i] == NULL) {
      fprintf(stderr, "Failed to allocate memory for node_hostname_list[%d].\n",
              i);
      // Free any previously allocated strings
      for (int j = 0; j < i; j++) {
        free(newJob->node_hostname_list[j]);
      }
      free(newJob->node_hostname_list);
      free(newJob->jobId);
      free(newJob);
      return NULL;
    }
  }

  // Initialize num_of_nodes
  // You'll need to replace this with your own code.
  newJob->num_of_nodes = node_size;

  return newJob;
}

void job_data_destroy(job_data *job) {
  // Free each string in the node_hostname_list array
  if (job->node_hostname_list != NULL) {
    char **p = job->node_hostname_list;
    for (int i = 0; i < job->num_of_nodes; i++) {
      free(p[i]);
    }
    // Free the array itself
    free(job->node_hostname_list);
    job->node_hostname_list = NULL;
  }

  // Free the jobId if it was dynamically allocated
  if (job->jobId != NULL) {
    free(job->jobId);
    job->jobId = NULL;
  }
}
