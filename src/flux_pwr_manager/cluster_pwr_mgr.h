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

/**
 * @brief This function returns a pointer to the new cluster_mgr struct.
 *
 * @param h flux_handle to make RPC calls.
 * @param global_power_budget of the cluster
 * @param num_of_nodes num of nodes in the cluster
 * @return the new cluster_mgr_T pointer.
 */
cluster_mgr_t *cluster_mgr_new(flux_t *h, double global_power_budget,
                               uint64_t num_of_nodes);

/**
 * @brief This methods sets the global pwr_budget for the given cluster.
 *
 * @param cluster_mgr cluster_mgr_t pointer.
 * @param pwr the new powerlimit.
 * @return status.
 */
int cluster_mgr_set_global_pwr_budget(cluster_mgr_t *cluster_mgr, double pwr);

/**
 * @brief This methods add a new job to the job hash table.
 *
 * @param cluster_mgr new job will be added to this pointer hash table.
 * @param jobId The new job's JobId.
 * @param nodelist a double pointer to the list of hostname of the nodes in the
 * job.
 * @param num_of_nodes number of nodes in the job.
 * @param cwd current working directory of the job.
 * @param job_name job name.
 * @return status.
 */
int cluster_mgr_add_new_job(cluster_mgr_t *cluster_mgr, uint64_t jobId,
                            char **nodelist, int num_of_nodes, char *cwd,
                            char *job_name);

/**
 * @brief removes a job from the cluster.
 *
 * @param cluster_mgr the cluster from where the job is to be removed.
 * @param jobId the jobid.
 * @return status.
 */
int cluster_mgr_remove_job(cluster_mgr_t *cluster_mgr, uint64_t jobId);

/**
 * @brief destructor for cluster_mgr_t
 *
 * @param manager
 */
void cluster_mgr_destroy(cluster_mgr_t **manager);

/**
 * @brief A callback which we use to keep the hostname and flux broker rank for
 * all the nodes.
 *
 * @param rank the FLUX broker rank.
 * @param hostname hostname of that rank.
 */
void cluster_mgr_add_hostname(int rank, char *hostname);

/**
 * @brief Callback function for setting powerratio between CPU/GPU. The RPC
 * should be coming from the end user. Only applicable for IBM systems. For more
 * info read: https://variorum.readthedocs.io/en/latest/IBM.html
 *
 * @param h
 * @param mh
 * @param msg
 * @param args
 */
void cluster_mgr_set_power_ratio_cb(flux_t *h, flux_msg_handler_t *mh,
                                    const flux_msg_t *msg, void *args);

/**
 * @brief Callback function for setting global power budget. This RPC will
 * trigger a redistribution of powerlimit for all the jobs.
 *
 * @param h
 * @param mh
 * @param msg
 * @param args
 */
void cluster_mgr_set_global_powerlimit_cb(flux_t *h, flux_msg_handler_t *mh,
                                          const flux_msg_t *msg, void *args);
void cluster_mgr_collect_power_data(cluster_mgr_t *cluster_mgr);
void job_map_destroy(void **job_map);
#endif
