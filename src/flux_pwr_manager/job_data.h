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

#define POWER_HISTORY_SIZE 100
typedef struct {
  uint64_t jobId;
  int num_of_nodes;
  char **node_hostname_list;
  double job_power_agg;
  double job_power_current;
  double job_power_cap;
  uint64_t latest_entry_time_stamp;
  circular_buffer_t *job_power_history;
  node_power_profile **node_power_profile_data;
} job_data;

int job_power_update(job_data *job, power_data *data);

int job_node_power_update(job_data *job, char *hostname, power_data **p_data,
                          int num_of_gpus, int num_of_sockets, bool mem,
                          int num_of_devices, power_data *node_p_data,uint64_t timestamp);
job_data *job_data_new(uint64_t jobId, char **node_hostname_list,
                       int node_size);
void job_data_destroy(job_data *job);
#endif
