#ifndef FLUX_PWR_MANAGER_JOB_PWR_MANAGER_H
#define FLUX_PWR_MANAGER_JOB_PWR_MANAGER_H
#include "power_policies/policy_mgr.h"
#include "pwr_info.h"
#include "pwr_stats.h"
#include "retro_queue_buffer.h"
#include <flux/core.h>
#include <unistd.h>
#define MAX_NODE_PWR 3050
#define MIN_NODE_PWR 1000
// Responsible for dealing with all things releated to job power manager
typedef struct {

  uint64_t jobId;
  int num_of_nodes;
  char **node_hostname_list;
  double powerlimit;
  retro_queue_buffer_t *power_history;
  pwr_info pwr_data;
  double *_nodes_powerlimit;
  pwr_stats_t *nodes_pwr_stats;
  double excess_power;
  pwr_policy_t *job_pwr_policy;
} job_mgr_t;
job_mgr_t *job_mgr_new(uint64_t jobId, char **nodelist, int num_of_nodes,
                       char *cwd, pwr_policy_t *job_power_policy,
                       double powerlimit, int *node_index);
void job_mgr_destroy(job_mgr_t **job);
void job_mgr_node_power_get_cb(flux_t *h, flux_msg_handler_t *mh,
                               const flux_msg_t *msg, void *arg);

void all_receive_node_power_get();
int job_mgr_update_powerlimit(flux_t *h, job_mgr_t *job_mgr,
                              double new_powerlimit);
int job_mgr_set_nodes_powerlimit(job_mgr_t *job_mgr, flux_t *h);
// should be between 0-100
void broadcast_node_power_ratio(int power_ratio);

void manage_power_capping();

#endif
