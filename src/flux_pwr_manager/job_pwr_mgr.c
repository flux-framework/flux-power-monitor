#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "flux_pwr_logging.h"
#include "job_pwr_mgr.h"
#include "node_manager/node_manager.h"
#include "power_policies/uniform_pwr_policy.h"
#include "pwr_info.h"
#include "util.h"

int update_node_powerlimit_rpc(job_mgr_t *job_mgr, flux_t *h,
                               double *power_data) {
  for (int i = 0; i < job_mgr->num_of_nodes; i++) {

    int local_rank = job_mgr->hostname_rank_mapping[i];
    if (local_rank == 0) {
      node_manager_set_powerlimit(power_data[i]);
    } else {
      if (flux_rpc_pack(h, "flux_pwr.nm-set_pl", local_rank,
                        FLUX_RPC_NORESPONSE, "{s:f}", "pl",
                        &power_data[i]) < 0) {
        log_error(
            "RPC_ERROR:Unable to send new powelimit to node index: %d and name",
            i, job_mgr->node_hostname_list[i]);
        return -1;
      }
    }
    job_mgr->nodes_pwr_stats[i].powerlimit = power_data[i];
  }
  return 0;
}

int send_new_job_rpc(flux_t *h, job_mgr_t *job_mgr) {
  // Get the per node power data;
  int errnos;
  errnos = 0;
  if (!h) {
    errnos = -1;
    log_error("NULL FLUX HANDLE");
  }
  double *power_data = calloc(job_mgr->num_of_nodes, sizeof(double));
  if (!power_data) {
    errnos = -1;
    goto cleanup;
  }

  job_mgr->job_pwr_policy->get_power_dist(
      job_mgr->num_of_nodes, job_mgr->nodes_pwr_stats, job_mgr->powerlimit,
      job_mgr->powerlimit, power_data);
  for (int i = 0; i < job_mgr->num_of_nodes; i++) {
    job_mgr->nodes_pwr_stats[i].powerlimit = power_data[i];
    int local_rank = job_mgr->hostname_rank_mapping[i];
    log_message("local rank %d", local_rank);
    if (local_rank == 0) {
      log_message("RANK 0 power ratio setting %d", job_mgr->power_ratio);
      node_manager_new_job(job_mgr->jobId, job_mgr->cwd, job_mgr->job_name);
      if ((node_manager_set_powerlimit(power_data[i]) < 0) ||
          (node_manager_set_power_ratio(job_mgr->power_ratio) < 0))
        log_error("ERROR in setting rank 0 node power settings");

    } else {
      log_message("checking  sending RPC to rank %d job_mgr "
                  "jobId %ld cwd %s name %s power %f",
                  local_rank, job_mgr->jobId, job_mgr->cwd, job_mgr->job_name,
                  power_data[i]);
      flux_future_t *f = flux_rpc_pack(
          h, "pwr_mgr.nm-new_job", local_rank, FLUX_RPC_NORESPONSE,
          "{s:I s:s s:s s:f s:i}", "jobid", job_mgr->jobId, "cwd", job_mgr->cwd,
          "name", job_mgr->job_name, "pl", power_data[i], "pr",
          job_mgr->power_ratio);
      if (!f) {
        log_error("RPC_ERROR:new job notification failure");
        errnos = -1;
        goto cleanup;
      }
    }
  }
cleanup:
  if (power_data)
    free(power_data);
  return errnos;
}

int send_end_job_rpc(flux_t *h, job_mgr_t *job_mgr) {
  for (int i = 0; i < job_mgr->num_of_nodes; i++) {

    int local_rank = job_mgr->hostname_rank_mapping[i];
    if (local_rank == 0) {
      node_manager_finish_job(job_mgr->jobId);
    } else {
      if (flux_rpc_pack(h, "pwr_mgr.nm-end_job",
                        job_mgr->hostname_rank_mapping[i], FLUX_RPC_NORESPONSE,
                        "{s:I}", "jobid", job_mgr->jobId) < 0) {
        log_error("RPC_ERROR:job end rpc failure");
        return -1;
      }
    }
  }
  return 0;
}
job_mgr_t *job_mgr_new(uint64_t jobId, char **nodelist, int num_of_nodes,
                       char *cwd, char *job_name, POWER_POLICY_TYPE pwr_policy,
                       double powerlimit, int power_ratio,
                       int *node_indices_para, flux_t *h) {
  bool error = false;
  if (!nodelist || num_of_nodes == 0 || !cwd)
    return NULL;
  job_mgr_t *job_mgr = calloc(1, sizeof(job_mgr_t));
  if (!job_mgr) {
    error = true;
    goto cleanup;
  }
  job_mgr->power_history = retro_queue_buffer_new(100, free);
  if (!job_mgr->power_history) {
    error = true;
    goto cleanup;
  }
  job_mgr->node_hostname_list = calloc(num_of_nodes, sizeof(char *));
  for (int i = 0; i < num_of_nodes; i++) {
    job_mgr->node_hostname_list[i] = strdup(nodelist[i]);
    if (!job_mgr->node_hostname_list[i]) {
      error = true;
      goto cleanup;
    }
  }
  job_mgr->num_of_nodes = num_of_nodes;
  job_mgr->nodes_pwr_stats = calloc(job_mgr->num_of_nodes, sizeof(pwr_stats_t));
  if (!job_mgr->nodes_pwr_stats) {
    error = true;
    goto cleanup;
  }
  job_mgr->hostname_rank_mapping = calloc(num_of_nodes, sizeof(int));
  if (!job_mgr->hostname_rank_mapping) {
    error = true;
    goto cleanup;
  }

  for (int i = 0; i < num_of_nodes; i++)
    job_mgr->hostname_rank_mapping[i] = node_indices_para[i];

  job_mgr->job_pwr_policy = malloc(sizeof(pwr_policy_t));
  if (!job_mgr->job_pwr_policy) {
    error = true;
    goto cleanup;
  }
  if (pwr_policy == UNIFORM) {
    uniform_pwr_policy_init(job_mgr->job_pwr_policy);
  }
  job_mgr->jobId = jobId;
  job_mgr->powerlimit = powerlimit;
  job_mgr->power_ratio = power_ratio;
  job_mgr->cwd = strdup(cwd);
  job_mgr->job_name = strdup(job_name);
  send_new_job_rpc(h, job_mgr);
cleanup:
  if (error && job_mgr) {
    if (job_mgr->power_history)
      retro_queue_buffer_destroy(job_mgr->power_history);

    if (job_mgr->node_hostname_list) {
      for (int i = 0; i < num_of_nodes; i++) {
        free(job_mgr->node_hostname_list[i]);
      }
      free(job_mgr->node_hostname_list);
    }

    free(job_mgr->hostname_rank_mapping);
    free(job_mgr->job_pwr_policy);
    free(job_mgr->cwd);
    free(job_mgr->job_name);
    free(job_mgr);
  }
  return job_mgr;
}

int job_mgr_update_powerlimit(job_mgr_t *job_mgr, flux_t *h,
                              double new_powerlimit) {

  int errnos;
  errnos = 0;
  if (!job_mgr || new_powerlimit == 0)
    errnos = -1;
  double current_powerlimit = 0;
  job_mgr->powerlimit = new_powerlimit;
  for (int i = 0; i < job_mgr->num_of_nodes; i++)
    current_powerlimit += job_mgr->nodes_pwr_stats[i].powerlimit;
  double *power_data = calloc(job_mgr->num_of_nodes, sizeof(double));
  if (!power_data) {
    errnos = -1;
    goto cleanup;
  }

  job_mgr->job_pwr_policy->get_power_dist(
      job_mgr->num_of_nodes, job_mgr->nodes_pwr_stats, new_powerlimit,
      current_powerlimit, power_data);
  if (update_node_powerlimit_rpc(job_mgr, h, power_data) < 0) {
    log_error("Unable to update powerlimit");
    errnos = -1;
    goto cleanup;
  }
cleanup:
  if (power_data)
    free(power_data);
  power_data = NULL;
  return errnos;
}

void job_mgr_destroy(flux_t *h, job_mgr_t **job) {
  if (!job || !*job)
    return;

  job_mgr_t *proper_job = *job;

  send_end_job_rpc(h, proper_job);

  if (proper_job->node_hostname_list) {
    for (int i = 0; i < proper_job->num_of_nodes; i++) {
      free(proper_job->node_hostname_list[i]);
    }
    free(proper_job->node_hostname_list);
  }

  retro_queue_buffer_destroy(proper_job->power_history);
  free(proper_job->nodes_pwr_stats);
  free(proper_job->hostname_rank_mapping);
  free(proper_job->job_name);
  free(proper_job->cwd);
  free(proper_job);
  *job = NULL; // Set the pointer to NULL to avoid dangling references.
}
