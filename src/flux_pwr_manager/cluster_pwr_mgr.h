#ifndef FLUX_PWR_MANAGER_CLUSTER_PWR_MANAGER_H
#define FLUX_PWR_MANAGER_CLUSTER_PWR_MANAGER_H
#include "job_pwr_mgr.h"
#include <czmq.h>
#include <flux/core.h>
typedef struct {
  uint64_t jobId;
  job_mgr_t *job_pwr_manager;
} job_map_t;

typedef struct {
  double global_power_budget;
  double current_power_usage;
  int num_of_jobs;
  zhashx_t *job_hash_table;
  uint64_t num_of_nodes;
} cluster_mgr_t;

cluster_mgr_t *cluster_mgr_new(flux_t *h, double global_power_budget,
                               uint64_t num_of_nodes);
int cluster_mgr_set_global_pwr_budget(cluster_mgr_t *cluster_mgr, double pwr);
int cluster_mgr_add_new_job(cluster_mgr_t *cluster_mgr, uint64_t jobId,
                            char **nodelist, int num_of_nodes, char *cwd);
int cluster_mgr_remove_job(cluster_mgr_t *cluster_mgr, uint64_t jobId);
void cluster_mgr_destroy(cluster_mgr_t **manager);
void cluster_mgr_add_hostname(int rank, char *hostname);
void job_map_destory(void **job_map);
#endif
