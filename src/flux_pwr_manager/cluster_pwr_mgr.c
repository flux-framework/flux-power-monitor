#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "cluster_pwr_mgr.h"
#include "job_hash.h"
#define MAX_HOSTNAME_SIZE 256
flux_t *flux_handle;
char **cluster_node_hostname_list;
int nodes_in_cluster;
cluster_mgr_t *self = NULL;
cluster_mgr_t *cluster_mgr_new(flux_t *h, double global_power_budget,
                               uint64_t cluster_size) {
  if (global_power_budget == 0)
    return NULL;
  cluster_node_hostname_list = malloc(cluster_size * sizeof(char *));
  flux_handle = h;
  cluster_mgr_t *cluster_mgr = malloc(sizeof(cluster_mgr_t));
  if (!cluster_mgr)
    return NULL;
  cluster_mgr->job_hash_table = job_hash_create();
  if (!cluster_mgr->job_hash_table)
    return NULL;
  zhashx_set_destructor(cluster_mgr->job_hash_table, job_map_destory);
  cluster_mgr->global_power_budget = global_power_budget;
  cluster_mgr->num_of_nodes = cluster_size;
  nodes_in_cluster = cluster_size;
  self = cluster_mgr;
  return cluster_mgr;
}

void cluster_mgr_add_hostname(int rank, char *hostname) {
  cluster_node_hostname_list[rank] = strdup(hostname);
}

int cluster_mgr_add_new_job(cluster_mgr_t *cluster_mgr, uint64_t jobId,
                            char **nodelist, int nodes_in_job, char *cwd) {
  if (jobId == 0 || !nodelist || !cwd || !cluster_mgr || nodes_in_job == 0)
    return -1;
  char **node_hostname_list;

  job_map_t *map = malloc(sizeof(job_map_t));
  if (!map)
    return -1;
  map->jobId = jobId;
  int *ranks = malloc(sizeof(int) * nodes_in_job);
  // Quite inefficent but should be fine for newer systems
  for (int i = 0; i < nodes_in_job; i++) {
    for (int j = 0; j < nodes_in_cluster; j++) {
      if (strcmp(nodelist[i], cluster_node_hostname_list[j]) == 0) {
        ranks[i] = j;
        break;
      }
    }
  }
  // TODO: properly implement a new job

  map->job_pwr_manager =
      job_mgr_new(jobId, nodelist, nodes_in_job, cwd, NULL, 0, ranks);
  free(ranks);
  if (!map->job_pwr_manager)
    return -1;
  if (!cluster_mgr->job_hash_table)
    return -1;
  zhashx_insert(cluster_mgr->job_hash_table, (void *)jobId, (void *)map);

  return 0;
}

int cluster_mgr_remove_job(cluster_mgr_t *cluster_mgr, uint64_t jobId) {
  if (jobId == 0 || !cluster_mgr)
    return -1;
  if (!cluster_mgr->job_hash_table)
    return -1;
  zhashx_delete(cluster_mgr->job_hash_table, (void *)jobId);
  return 0;
}
void cluster_mgr_destroy(cluster_mgr_t **manager) {
  if (manager && *manager) {
    cluster_mgr_t *mgr = (cluster_mgr_t *)*manager;
    zhashx_purge(mgr->job_hash_table);
    zhashx_destroy(&mgr->job_hash_table);
    for (int i = 0; i < nodes_in_cluster; i++)
      free(cluster_node_hostname_list[i]);
    free(cluster_node_hostname_list);
    self = NULL;
    free(mgr);
    mgr = NULL;
  }
}
void job_map_destory(void **job_map) {
  if (job_map && *job_map) {
    job_map_t *proper_job_map = (job_map_t *)*job_map;
    job_mgr_destroy(&proper_job_map->job_pwr_manager);
    free(proper_job_map);
    proper_job_map = NULL;
  }
}

int cluster_mgr_set_global_pwr_budget(cluster_mgr_t *cluster_mgr, double pwr) {
  if (!cluster_mgr)
    return -1;
  cluster_mgr->global_power_budget = pwr;
  return 0;
}
