#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "flux_pwr_logging.h"
#include "job_pwr_mgr.h"
#include "pwr_info.h"
#include "util.h"
int *node_indices;
int self_num_of_nodes;
job_mgr_t *job_mgr_new(uint64_t jobId, char **nodelist, int num_of_nodes,
                       char *cwd, pwr_policy_t *job_pwr_policy,
                       double powerlimit, int *node_indices_para) {
  if (!nodelist || num_of_nodes == 0 || !cwd || !job_pwr_policy)
    return NULL;
  job_mgr_t *job_mgr = malloc(sizeof(job_mgr_t));
  job_mgr->power_history = retro_queue_buffer_new(100, free);
  if (!job_mgr->power_history)
    return NULL;
  for (int i = 0; i < num_of_nodes; i++) {
    job_mgr->node_hostname_list[i] = strdup(nodelist[i]);
  }
  job_mgr->num_of_nodes = num_of_nodes;
  job_mgr->nodes_pwr_stats =
      malloc(sizeof(pwr_stats_t) * job_mgr->num_of_nodes);
  if (!job_mgr->nodes_pwr_stats)
    return NULL;
  self_num_of_nodes = job_mgr->num_of_nodes;
  node_indices = malloc(sizeof(int) * num_of_nodes);
  if (!node_indices)
    return NULL;

  for (int i = 0; i < num_of_nodes; i++)
    node_indices[i] = node_indices_para[i];

  job_mgr->jobId = jobId;
  job_mgr->powerlimit = powerlimit;
  job_mgr->job_pwr_policy = job_pwr_policy;
  // TODO SET initial powerlimit

  return job_mgr;
}

int update_node_powerlimit(job_mgr_t *job_mgr, flux_t *h, double *power_data) {
  for (int i = 0; i < self_num_of_nodes; i++) {
    if (flux_rpc_pack(h, "node_pwr_mgr.set-powerlimit", 0, node_indices[i],
                      "{s:f}", "pl", &power_data[i]) < 0) {
      log_error(
          "RPC_ERROR:Unable to send new powelimit to node index: %d and name",
          i, job_mgr->node_hostname_list[i]);
    }
  }
  return 0;
}
int job_mgr_update_powerlimit(flux_t *h, job_mgr_t *job_mgr,
                              double new_powerlimit) {
  if (!job_mgr || new_powerlimit == 0)
    return -1;
  double current_powerlimit = 0;
  job_mgr->powerlimit = new_powerlimit;
  for (int i = 0; i < job_mgr->num_of_nodes; i++)
    current_powerlimit += job_mgr->nodes_pwr_stats[i].powerlimit;
  double *power_data;
  job_mgr->job_pwr_policy->get_power_dist(
      job_mgr->num_of_nodes, job_mgr->nodes_pwr_stats, new_powerlimit,
      current_powerlimit, power_data);
  if (update_node_powerlimit(job_mgr, h, power_data) < 0) {
    log_error("Unable to update powerlimit");
    return -1;
  }

  return 0;
}

void job_mgr_destroy(job_mgr_t **job) {
  if (job && *job) {
    job_mgr_t *proper_job = (job_mgr_t *)*job;
    for (int i = 0; i < proper_job->num_of_nodes; i++)
      free(proper_job->node_hostname_list[i]);
    free(proper_job->node_hostname_list);
    retro_queue_buffer_destroy(proper_job->power_history);
    free(proper_job->nodes_pwr_stats);
    free(node_indices);
    proper_job->power_history = NULL;
    free(proper_job);
  }
}
