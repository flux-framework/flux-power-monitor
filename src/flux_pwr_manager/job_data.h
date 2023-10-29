#ifndef FLUX_PWR_MANAGER_JOB_DATA_H
#define FLUX_PWR_MANAGER_JOB_DATA_H
#include "node_power_profile.h"
#include "power_data.h"
#include <unistd.h>
#include <flux/core.h>
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
  double power_agg;
  double power_current;
  double powercap;
  double powerlimit;
  uint64_t t_depend;
  double max_power;
  double min_power;
  POWER_POLICY_TYPE current_power_policy;
  uint64_t latest_entry_time_stamp;
  circular_buffer_t *power_history;
  node_power_profile **node_power_profile_data;

} job_data;

int job_power_update(job_data *job, power_data *data);

int job_node_power_update(job_data *job, char *hostname, power_data **p_data,
                          int num_of_gpus, int num_of_sockets, bool mem,
                          int num_of_devices, power_data *node_p_data,
                          uint64_t timestamp);
job_data *job_data_new(uint64_t jobId, char **node_hostname_list, int node_size,
                       uint64_t t_depend);
void job_data_destroy(job_data *job);
#endif
