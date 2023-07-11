#ifndef FLUX_JOB_DATA_H
#define FLUX_JOB_DATA_H
#include "flux/core.h"
#include "node_power_profile.h"
#include "power_data.h"
#define HANDLE_ERROR(msg)                                                      \
  do {                                                                         \
    fprintf(stderr, "%s\n", msg);                                              \
    goto cleanup;                                                              \
  } while (0)

#define PER_NODE_POWER_HISTORY 100
typedef struct {
  char *jobId;
  int num_of_nodes;
  char **node_hostname_list;
  power_data *job_agg_power_data;
  node_power_profile **node_power_profile_data;
} job_data;

job_data *job_data_new(char *jobId, char **node_hostname_list, int node_size);
void job_data_destroy(job_data *job);
#endif
